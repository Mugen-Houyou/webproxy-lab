/**
 * proxy.c - A concurrent web proxy server based on select
 */
#include "csapp.h"

#define MAX_HEADERS 100
#define SHORT_CHARS 16

typedef struct { /* represents a pool of connected descriptors */ 
    int maxfd;        /* largest descriptor in read_set */   
    fd_set read_set;  /* set of all active descriptors */
    fd_set ready_set; /* subset of descriptors ready for reading  */
    int nready;       /* number of ready descriptors from select */   
    int maxi;         /* highwater index into client array */
    int clientfd[FD_SETSIZE];    /* set of active descriptors */
    rio_t clientrio[FD_SETSIZE]; /* set of active read buffers */
} pool; 

typedef struct {
  char method[SHORT_CHARS];
  char uri[MAXLINE];
  char version[SHORT_CHARS];
  char hostname[MAXLINE];
  char port[SHORT_CHARS];
  char path[MAXLINE];

  // 전체 헤더 줄들을 저장 (각 줄은 NULL문자로 끊음!)
  char headers[MAX_HEADERS][MAXLINE];
  int header_count;
} http_request_t;


/* 전역 함수 선언 */
// echo에서 가져온 파트
void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);

// tiny에서 가져온 파트
int parse_uri(const char* uri, char* hostname, char* port, char* path);


/* $begin proxyserversmain */
int total_bytes_received = 0; /* counts total bytes received by server */


// // 좀비 프로세스 처리
// void sigchld_handler(int sig) {
//   while (waitpid(-1, NULL, WNOHANG) > 0);
// }


int main(int argc, char **argv){
	char* port_p;
  int listenfd, connfd, port; 
  socklen_t clientlen = sizeof(struct sockaddr_in);
  struct sockaddr_in clientaddr;
  static pool pool; 

  // // 좀비 프로세스 처리
  // Signal(SIGCHLD, sigchld_handler);

  if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
  }
  port = atoi(argv[1]);
	port_p = argv[1];

  listenfd = Open_listenfd(port_p);
  init_pool(listenfd, &pool);

  while (1) {
    /* Wait for listening/connected descriptor(s) to become ready */
    pool.ready_set = pool.read_set;
    pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

    /* If listening descriptor ready, add new client to pool */
    if (FD_ISSET(listenfd, &pool.ready_set)) {
      connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
      add_client(connfd, &pool); 
    }
      
    /* 여기가 do the proxy request & response from each ready connected descriptor */ 
    check_clients(&pool);
  }
}
/* $end proxyserversmain */

/* $begin init_pool */
void init_pool(int listenfd, pool *p){
  /* Initially, there are no connected descriptors */
  int i;

  p->maxi = -1;
  for (i=0; i< FD_SETSIZE; i++)  
    p->clientfd[i] = -1;

  /* Initially, listenfd is only member of select read set */
  p->maxfd = listenfd; 
  FD_ZERO(&p->read_set);
  FD_SET(listenfd, &p->read_set); 
}
/* $end init_pool */

/* $begin add_client */
void add_client(int connfd, pool *p) {
  int i;

  p->nready--;
  for (i = 0; i < FD_SETSIZE; i++)  /* Find an available slot */
  if (p->clientfd[i] < 0) { 
    /* Add connected descriptor to the pool */
    p->clientfd[i] = connfd;
    Rio_readinitb(&p->clientrio[i], connfd);

    /* Add the descriptor to descriptor set */
    FD_SET(connfd, &p->read_set); 

    /* Update max descriptor and pool highwater mark */
    if (connfd > p->maxfd)
      p->maxfd = connfd;
    if (i > p->maxi) 
      p->maxi = i;  

    break;
  }

  if (i == FD_SETSIZE) /* Couldn't find an empty slot */
  app_error("add_client error: Too many clients");
}
/* $end add_client */

static void process_ready_client(pool *p, int idx){
  int connfd = p->clientfd[idx];
  rio_t rio = p->clientrio[idx];
  char buf[MAXLINE], line[MAXLINE];
  http_request_t req;
  int n;

  /* 요청 라인 읽기 */
  if ((n = Rio_readlineb(&rio, buf, MAXLINE)) == 0) { // EOF면 연결 정리 
    Close(connfd);
    FD_CLR(connfd, &p->read_set);
    p->clientfd[idx] = -1;
    return;
  }

  sscanf(buf, "%s %s %s", req.method, req.uri, req.version);
  // CONNECT일 경우 터널링 (양방향 TCP 패스쓰루)
  if (!strcasecmp(req.method, "CONNECT")) {
    char *colon = strchr(req.uri, ':'); // host:port 파싱
    if (colon) { *colon = '\0';
      strcpy(req.hostname, req.uri);
      strcpy(req.port, colon + 1);
    } else {
      strcpy(req.hostname, req.uri); 
      strcpy(req.port, "443");
    }

    pid_t pid = Fork();
    if (pid == 0) {    
      tunnel_relay(connfd, req.hostname, req.port);
      Close(connfd); /* 자식도 클라이언트를 풀에서 제거 */
      exit(0); 
    }
    
    Close(connfd); /* 부모도 클라이언트를 풀에서 제거 */
    FD_CLR(connfd, &p->read_set);
    p->clientfd[idx] = -1;
    return;
  }

  // 일반 HTTP 요청 처리 
  if (!parse_uri(req.uri, req.hostname, req.port, req.path)) {
    clienterror(connfd, req.uri, "400", "Bad Request", "URI 파싱 실패");
    Close(connfd);
    FD_CLR(connfd, &p->read_set);
    p->clientfd[idx] = -1;
    return;
  }

  /* 나머지 헤더 수집 */
  req.header_count = 0;
  while (Rio_readlineb(&rio, line, MAXLINE) > 0) {
    if (!strcmp(line, "\r\n")) break;
    if (req.header_count < MAX_HEADERS)
      strcpy(req.headers[req.header_count++], line);
  }

  /* fork() 후 HTTP 릴레이 */
  pid_t pid = Fork();
  if (pid == 0) {  // 자식 
    handle_http_request(connfd, &req);
    Close(connfd);
    exit(0);
  }
  // 부모 정리
  Close(connfd);
  FD_CLR(connfd, &p->read_set);
  p->clientfd[idx] = -1;
}

void check_clients(pool *p){
  for (int i = 0; i <= p->maxi && p->nready > 0; i++) {
    int connfd = p->clientfd[i];
    if (connfd > 0 && FD_ISSET(connfd, &p->ready_set)) {
      p->nready--; // select()에서 빼먹지 않도록
      process_ready_client(p, i); 
    }
  }
}

void handle_http_request(int clientfd, http_request_t *req) {
  /**
    1. Open_clientfd(hostname, port)
    2. write(서버에 요청 라인 + 헤더)
    3. read(서버 응답)
    4. write(응답을 clientfd로 전달)
    5. Close(serverfd)
   */
  int serverfd;
  char buf[MAXLINE];
  rio_t server_rio;

  serverfd = Open_clientfd(req->hostname, req->port);
  if (serverfd < 0) {
    clienterror(clientfd, req->hostname, "502", "Bad Gateway", "Proxy couldn't connect to origin server");
    return;
  }

  Rio_readinitb(&server_rio, serverfd);

  // 요청 라인
  sprintf(buf, "%s %s HTTP/1.0\r\n", req->method, req->path);
  Rio_writen(serverfd, buf, strlen(buf));

  // 헤더 전송
  int has_host = 0;
  for (int i = 0; i < req->header_count; ++i) {
    if (strncasecmp(req->headers[i], "Host:", 5) == 0)
      has_host = 1;
    else if (strncasecmp(req->headers[i], "Connection:", 11) == 0)
      continue;
    else if (strncasecmp(req->headers[i], "Proxy-Connection:", 17) == 0)
      continue;
    else if (strncasecmp(req->headers[i], "User-Agent:", 11) == 0)
      continue;

    Rio_writen(serverfd, req->headers[i], strlen(req->headers[i]));
  }

  // 헤더 보완
  if (!has_host) {
    sprintf(buf, "Host: %s\r\n", req->hostname);
    Rio_writen(serverfd, buf, strlen(buf));
  }

  // 필수 헤더들 통일
  sprintf(buf, "Connection: close\r\n");
  Rio_writen(serverfd, buf, strlen(buf));
  sprintf(buf, "Proxy-Connection: close\r\n");
  Rio_writen(serverfd, buf, strlen(buf));
  sprintf(buf, "User-Agent: Mozilla/5.0 (compatible; TinyProxy/1.0)\r\n");
  Rio_writen(serverfd, buf, strlen(buf));

  // 요청 끝
  Rio_writen(serverfd, "\r\n", 2);

  // 응답 중계
  ssize_t n;
  while ((n = Rio_readn(serverfd, buf, MAXLINE)) > 0)
    Rio_writen(clientfd, buf, n);

  Close(serverfd);
}

/**
 * parse_uri - 프록시 요청 파싱. 
 * 첫 줄 예시: GET http://www.example.com/asdf/index.html HTTP/1.0
 */
/* strstr(): strstr() 함수는 string1에서 string2의 첫 번째 표시를 찾습니다. 
      함수는 일치 프로세스에서 string2로 끝나는 NULL자(\0)를 무시합니다. 
*/
int parse_uri(const char* uri, char* hostname, char* port, char* path) {
  char* host_p;
  char* path_p;
  char* port_p;
  char hostport[SHORT_CHARS];

  // http:// 접두어 확인
  if (strncasecmp(uri, "http://", 7) != 0)
    return 0;

  host_p = uri + 7;  // "http://" 이후부터

  // path 위치 찾기
  path_p = strchr(host_p, '/');
  if (path_p != NULL) { // strchr() 결과가 NULL일 수 있으므로
    strcpy(path, path_p);   // "/foo/bar.html"
    
    size_t len = path_p - host_p; // hostname:port만 남기기 위해 임시로 NULL 종결

    if (len >= SHORT_CHARS) 
      return 0;  // 길이 초과 방지

    memcpy(hostport, host_p, len);
    hostport[len] = '\0';

  } else { // http://example.com 같은 형태
    strcpy(path, "/"); 
    strcpy(hostport, host_p); 
  }

  // 포트 파싱
  port_p = strchr(hostport, ':');
  if (port_p != NULL) {
    *(char*) port_p = '\0';  // hostname 분리
    strcpy(hostname, hostport);
    strcpy(port, port_p + 1);
  } else {
    strcpy(hostname, hostport);
    strcpy(port, "80");  // 기본 포트
  }

  return 1;
}


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

/**
 * 터널링: 클라이언트 - 프록시 - 오리진 서버 (양방향 TCP 패스쓰루) 
 * UDP는 나도 모르겠다.
 */
void tunnel_relay(int clientfd, const char *hostname, const char *port){
    int serverfd;
    int maxfd;
    fd_set readset;
    ssize_t n;
    char buf[MAXBUF];

    /* 1) 프록시 → 오리진 서버로 TCP 연결 */
    if ((serverfd = Open_clientfd(hostname, port)) < 0) {
      clienterror(clientfd, hostname, "502", "Bad Gateway", "Unable to connect to the origin server");
      return;
    }
    
    /* 여기서 200 OK 응답을 먼저 클라이언트에게 보내야 함 */
    const char *okmsg = "HTTP/1.0 200 Connection Established\r\n\r\n";
    Rio_writen(clientfd, okmsg, strlen(okmsg));
    
    
    /* 2) 양쪽 소켓을 select()로 감시하며 한쪽에서 읽어 다른 쪽으로 써 준다 */
    maxfd = (clientfd > serverfd ? clientfd : serverfd) + 1;

    while (1) {
      FD_ZERO(&readset);
      FD_SET(clientfd, &readset);   /* 클라이언트 → 서버 방향 감시 */
      FD_SET(serverfd, &readset);   /* 서버 → 클라이언트 방향 감시 */

      // int nready;
      // do {
      //     nready = Select(maxfd, &readset, NULL, NULL, NULL);
      // } while (nready < 0 && errno == EINTR);
      if (Select(maxfd, &readset, NULL, NULL, NULL) < 0)
        break;          /* select 에러면 터널 종료 */

      /* 2‑a) 클라이언트로부터 데이터 도착 */
      if (FD_ISSET(clientfd, &readset)) {
        n = read(clientfd, buf, sizeof(buf));
        if (n <= 0) 
          break;        /* EOF 또는 에러 → 터널 닫기 */
        Rio_writen(serverfd, buf, n);
      }

      /* 2‑b) 오리진 서버로부터 데이터 도착 */
      if (FD_ISSET(serverfd, &readset)) {
        n = read(serverfd, buf, sizeof(buf));
        if (n <= 0)
          break;        /* EOF 또는 에러 → 터널 닫기 */
        Rio_writen(clientfd, buf, n);
      }
    }

    Close(serverfd);
    // Close(clientfd);
}
