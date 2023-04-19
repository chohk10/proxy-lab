/*
 * proxy.c - A concurrent proxy server
 */
#include "csapp.h"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.0 Safari/605.1.15\r\n";

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

  printf("proxy server initiated\n");
  /* listen */
  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);     
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
    printf("connection with client closed\n\n");
  }

  return 0;
}

void doit(int connfd) {
  int is_static, clientfd;
  struct stat sbuf;
  char buf[MAXLINE], client_buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio, mid_rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, connfd);        // initiate buffer by reading fd in custom data type rio
  Rio_readlineb(&rio, buf, MAXLINE);  // read from rio into buffer  ==> the first line is the request line
  printf("Request headers:\n");       // start printing request header
  printf("%s", buf);                  // echo the request line
  sscanf(buf, "%s %s %s", method, uri, version);  // divide request line into method, uri, vesion
  read_requesthdrs(&rio);

  /* connect with end-server and send request*/
  printf("end-server connecting...\n");
  clientfd = Open_clientfd("chohk10.site", "8080");
  printf("end-server connected\n");
  Rio_readinitb(&mid_rio, clientfd);                        // initiate buffer for end-server connection
  sprintf(client_buf, "%s %s HTTP/1.0\r\n", method, uri);   // build request header
  sprintf(client_buf, "%sHost: chohk10.site\r\n", client_buf);
  sprintf(client_buf, "%sUser-Agent: %s\r\n", client_buf, user_agent_hdr);
  sprintf(client_buf, "%sConnection: close\r\n", client_buf);
  sprintf(client_buf, "%sProxy-Connection: close\r\n\r\n", client_buf);
  printf("request to end-server sending...\n");
  Rio_writen(clientfd, client_buf, strlen(client_buf));     // send request header to end-server
  printf("request to end-server sent\n");
  
  /* read and write response from end-server to client */
  Rio_readlineb(&mid_rio, client_buf, MAXLINE);             // read and write response line from end-server to client
  Rio_writen(connfd, client_buf, strlen(client_buf));
  printf("Response headers:\n");
  printf("%s", client_buf);
  while (strcmp(client_buf, "\r\n")) {                      // 이전에 read and write 한 헤더가 null이 아니라면 다음줄 read and write
    Rio_readlineb(&mid_rio, client_buf, MAXLINE);
    Rio_writen(connfd, client_buf, strlen(client_buf));
    printf("%s", client_buf);
  }
  
  /* read and write response body */
  size_t n;
  printf("response body sending...\n");
  while((n=Rio_readlineb(&mid_rio, client_buf, MAXLINE)) != 0) {
    Rio_writen(connfd, client_buf, n);
  }
  printf("response body sent\n");

  close(clientfd);
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
  
  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}