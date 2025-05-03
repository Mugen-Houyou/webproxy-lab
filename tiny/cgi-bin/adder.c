/*
 * adder_key_value.c - a minimal CGI program that adds two numbers together. This accepts "key=value" format.
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* Extract the two arguments from QUERY_STRING */
  if ((buf = getenv("QUERY_STRING")) != NULL) {
    char *v1 = strstr(buf, "num1="); // 예: "adder?num1=1&num2=2"
    char *v2 = strstr(buf, "num2=");

    if (v1) {
      v1 += strlen("num1=");
      p = strchr(v1, '&');
      if (p) *p = '\0';
      strcpy(arg1, v1);
    }

    if (v2) {
      v2 += strlen("num2=");
      strcpy(arg2, v2);
    }

    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }

  /* Make the response body */
  sprintf(content, "Welcome to add.com: ");
  sprintf(content + strlen(content), "the internet addition portal.<p>\r\n");
  sprintf(content + strlen(content), "The answer is: %d + %d = %d<p>\r\n", n1, n2, n1 + n2);
  sprintf(content + strlen(content), "Thanks for visiting!\r\n");

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %lu\r\n", strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);

  fflush(stdout);
  exit(0);
}
/* $end adder */
