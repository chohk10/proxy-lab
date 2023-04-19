/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE], val1[MAXLINE], val2[MAXLINE];
  int n1=0, n2=0;
  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL) {
    /* separate two args */
    p = strchr(buf, '&');
    *p = '\0';
    strcpy(arg1, buf);
    strcpy(arg2, p+1);

    /* extract vlaue of first argument */
    p = strchr(arg1, '=');
    *p = '\0';
    strcpy(val1, p+1);
    n1 = atoi(val1);  //ascii to int

    /* extract vlaue of second argument */
    p = strchr(arg2, '=');
    *p = '\0';
    strcpy(val2, p+1);
    n2 = atoi(val2);
  }

  /* Make the response body */
  // sprintf(content, "QUERY_STRING=%s", buf);  // ??? 불필요한 줄로 보임
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>",
          content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);
  
  /* Generate the HTTP response */
  printf("Connection: close\r\n");            // child에서 print 시 네트워크에 나감
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");  // end of response header
  printf("%s", content);                      // 위에서 담은거 (html소스) 출력 => html을 dynamic하게 생성
  fflush(stdout);                             // printf한건 버퍼에 담겨있다가 fflush를 했을 때 printf가 실제로 나가게 됨 

  exit(0);
}
/* $end adder */
