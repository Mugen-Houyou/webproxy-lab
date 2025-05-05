/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 * 
 * Mugen-Houyou initial commit
 */
#include "csapp.h"

// splice()기반 serve_static_splice()을 위함
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef SPLICE_F_MORE
#define SPLICE_F_MORE 4
#endif


void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);             // line:netp:doit:readrequest
  sscanf(buf, "%s %s %s", method, uri, version); // line:netp:doit:parserequest
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) { // line:netp:doit:beginrequesterr
    clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
    return;
  } // line:netp:doit:endrequesterr
  read_requesthdrs(&rio); // line:netp:doit:readrequesthdrs

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs); // line:netp:doit:staticcheck
  if (stat(filename, &sbuf) < 0) { // line:netp:doit:beginnotfound
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  } // line:netp:doit:endnotfound

  if (is_static) { /* Serve static content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // line:netp:doit:readable
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't read the file");
      return;
    }
    if (strcasecmp(method, "HEAD")==0) 
      serve_static_head(fd, filename, sbuf.st_size);
    else
      serve_static(fd, filename, sbuf.st_size); // line:netp:doit:servestatic
  } else { /* Serve dynamic content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { // line:netp:doit:executable
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't run the CGI program");
      return;
    }
    if (strcasecmp(method, "HEAD")==0) 
      serve_dynamic_head(fd, filename, sbuf.st_size);
    else
      serve_dynamic(fd, filename, sbuf.st_size); // line:netp:doit:servestatic
  }
}

void read_requesthdrs(rio_t* rp){
  char buf[MAXLINE];

  Rio_readlineb(rp,buf,MAXLINE);

  /**
   * GET /index.html HTTP/1.0\r\n
      Host: localhost:8000\r\n
      User-Agent: Mozilla/5.0\r\n
      Accept: text/html\r\n
      \r\n
      위와 같은 경우가 있을 때, 마지막 "\r\n"을 만날 때까지 while를 돈다.
   */
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }

  return;
}

/**
 * parse_uri - 만약 정적 콘텐츠면 1, 동적 콘텐츠면 0.
 */
int parse_uri(char* uri, char* filename, char* cgiargs){
  char* ptr;

  // 정적 콘텐츠
  if(!strstr(uri, "cgi-bin")){
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if(uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    return 1;

  // 동적 콘텐츠
  } else {
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    } else {
      strcpy(cgiargs, "");
    }
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

/**
 * server_static - 파일을 가상 메모리에 매핑 및 송신
 */
void serve_static(int fd, char* filename, int filesize){
  int srcfd;
  char* srcp, filetype[MAXLINE], buf[MAXBUF];

  // 리스폰스 헤더를 준비
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  
  // 클라이언트에게 리스폰스 헤더를 송신
  Rio_writen(fd, buf, strlen(buf));

  // 헤더 출력 (디버깅)
  printf("Response headers:\n");
  printf("%s", buf);

  // 리스폰스 보디를 준비
  srcfd = Open(filename, O_RDONLY, 0); // 해당 파일을 읽기 전용으로 `open()`.
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 가상메모리에 매핑 ("파일의 이 구간은 이 가상 주소와 대응된다")
  Close(srcfd);
  
  // 리스폰스 보디를 송신
  Rio_writen(fd, srcp, filesize); //  매핑된 주소들에 실제 접근 시 page fault가 나면서 디스크를 거침

  // 송신 후 메모리 매핑 해제
  Munmap(srcp, filesize);
}

/**
 * server_static - 파일을 가상 메모리에 매핑 및 송신
 */
void serve_static_head(int fd, char* filename, int filesize){
  int srcfd;
  char* srcp, filetype[MAXLINE], buf[MAXBUF];

  // 리스폰스 헤더를 준비
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  
  // 클라이언트에게 리스폰스 헤더를 송신
  Rio_writen(fd, buf, strlen(buf));

  // 헤더 출력 (디버깅)
  printf("Response headers:\n");
  printf("%s", buf);

  // // 리스폰스 보디를 준비
  // srcfd = Open(filename, O_RDONLY, 0); // 해당 파일을 읽기 전용으로 `open()`.
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 가상메모리에 매핑 ("파일의 이 구간은 이 가상 주소와 대응된다")
  // Close(srcfd);
  
  // // 리스폰스 보디를 송신
  // Rio_writen(fd, srcp, filesize); //  매핑된 주소들에 실제 접근 시 page fault가 나면서 디스크를 거침

  // // 송신 후 메모리 매핑 해제
  // Munmap(srcp, filesize);
}

/**
 * serve_dynamic - CGI 프로그램 실행 및 결과 송신
 *   - 여기서 char* filename가 실행할 CGI 프로그램의 경로. 예: /cgi-bin/adder?num1=1&num2=2
 *   - char* cgiargs는 URI에서 파싱된 쿼리 문자열. 예: num1=3&num2=5
 */
void serve_dynamic(int fd, char* filename, char* cgiargs){
  char buf[MAXLINE];
  char* emptylist[] = {NULL};

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  printf("cgiargs: %s\n", cgiargs);

  // 내가 지금 child면?
  if(Fork() == 0){
    // 실제로 서버들은 여기서 CGI 변수들을 지정함.
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd,STDOUT_FILENO); // stdout를 클라이언트에게 리다이렉트.
    Execve(filename, emptylist, environ);
  }

  // 내가 지금 부모면?
  Wait(NULL); // 기다렸다가 자식 프로세스를 종료.
}

void serve_dynamic_head(int fd, char* filename, char* cgiargs){
  char buf[MAXLINE];
  char* emptylist[] = {NULL};

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  printf("cgiargs: %s\n", cgiargs);

  // // 내가 지금 child면?
  // if(Fork() == 0){
  //   // 실제로 서버들은 여기서 CGI 변수들을 지정함.
  //   setenv("QUERY_STRING", cgiargs, 1);
  //   Dup2(fd,STDOUT_FILENO); // stdout를 클라이언트에게 리다이렉트.
  //   Execve(filename, emptylist, environ);
  // }

  // // 내가 지금 부모면?
  // Wait(NULL); // 기다렸다가 자식 프로세스를 종료.
}

/**
 * serve_static_revised - 소형 버퍼에 담아 보내는 방식.
 */
void serve_static_revised(int fd, char* filename, int filesize) {
  int srcfd;
  char filetype[MAXLINE], buf[MAXBUF];
  char readbuf[8192];  // 8KB씩 버퍼에 읽어 소켓에 전송
  ssize_t n;

  // 리스폰스 헤더를 준비
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf + strlen(buf), "Server: Tiny Web Server\r\n");
  sprintf(buf + strlen(buf), "Connection: close\r\n");
  sprintf(buf + strlen(buf), "Content-length: %d\r\n", filesize);
  sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n", filetype);

  // 클라이언트에게 리스폰스 헤더를 송신
  Rio_writen(fd, buf, strlen(buf));

  // printf("Response headers:\n%s", buf);

  // 리스폰스 보디를 준비, 파일 열기
  srcfd = Open(filename, O_RDONLY, 0);

  // read + write 루프로 본문 전송
  while ((n = read(srcfd, readbuf, sizeof(readbuf))) > 0) 
    Rio_writen(fd, readbuf, n);
  
  // 끝났으니 파일 닫기
  Close(srcfd);
}

/**
 * serve_static_splice - splice()를 사용하여 파일을 클라이언트로 전송 (zero-copy)
 */
void serve_static_splice(int fd, char* filename, int filesize) {
  int srcfd;
  int pipefd[2];
  char filetype[MAXLINE], buf[MAXBUF];
  ssize_t n;

  // 1. Content-Type 및 응답 헤더 구성
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf + strlen(buf), "Server: Tiny Web Server\r\n");
  sprintf(buf + strlen(buf), "Connection: close\r\n");
  sprintf(buf + strlen(buf), "Content-length: %d\r\n", filesize);
  sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n", filetype);

  // 2. 응답 헤더 전송
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n%s", buf);

  // 3. 정적 파일 열기
  srcfd = Open(filename, O_RDONLY, 0);

  // 4. 파이프 생성
  if (pipe(pipefd) < 0) 
    unix_error("pipe error");

  // 5. splice() 루프: file → pipe → socket
  off_t offset = 0;
  while (offset < filesize) {
    // 최대 전송량 제한 (이 예제에서는 64KB씩 보냄)
    size_t chunk = (filesize - offset > 65536) ? 65536 : (filesize - offset);

    // file → pipe (zero-copy)
    n = splice(srcfd, NULL, pipefd[1], NULL, chunk, SPLICE_F_MORE);
    if (n <= 0) break;

    // pipe → socket (zero-copy)
    splice(pipefd[0], NULL, fd, NULL, n, SPLICE_F_MORE);

    offset += n;
  }

  // 6. 정리
  Close(srcfd);
  Close(pipefd[0]);
  Close(pipefd[1]);
}

void get_filetype(char* filename, char* filetype){
  if(strstr(filename,".html"))
    strcpy(filetype, "text/html");
  else if(strstr(filename,".htm"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".jpeg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mpeg")) // 과제 11.7
    strcpy(filetype, "video/mpeg");
  else if (strstr(filename, ".mpg"))
    strcpy(filetype, "video/mpeg");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".css"))
    strcpy(filetype, "text/css");
  else if (strstr(filename, ".js"))
    strcpy(filetype, "text/javascript");
  else
    strcpy(filetype, "text/plain");
  return;
}

/**
 * clienterror - Tiny Web Server의 에러 응답 처리
 *    각 인자 설명:
 *        - fd: 클라이언트 소켓 파일 디스크립터
 *        - cause: 문제 원인 (예: 요청한 파일 이름)
 *        - errnum: HTTP 상태 코드 문자열 (예: "404", "501")
 *        - shortmsg: 한 줄 요약 (예: "Not found", "Not Implemented")
 *        - longmsg: 장문 설명
 */
void clienterror(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg ){
  char buf[MAXLINE], body[MAXBUF];
  
  // HTTP response body를 만듦
  /* 이때, printf vs. fprintf vs. sprintf?
    sprintf는 파일이나 화면이 아니라 변수(버퍼)에 문자열을 출력한다 (담는다).
  */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf)); // 클라이언트의 소켓에 전송. 클라이언트는 여기서부터 실제 HTML 콘텐츠를 렌더링하게 됨.
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}
