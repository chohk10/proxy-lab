/*
 * proxy.c - A concurrent proxy server
 */
#include "csapp.h"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* forward declaration of functions */
void *thread(void *vargp);
void doit(int connfd);
void build_requesthdrs(char *request_header, char *hostname, char *path, int port, rio_t *client_rio);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/* static constants for request headers */
static const char *requestline_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.0 Safari/605.1.15\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_conn_hdr = "Proxy-Connection: close\r\n";

static const char *host_hdr_key = "Host";
static const char *user_agent_hdr_key = "User-Agent";
static const char *conn_hdr_key = "Connection";
static const char *prox_conn_hdr_key = "Proxy-Connection";

static const char *hostname = "chohk10.site";
static const int endsrv_port = 8080;

/*
 * main - main thread for listening to incoming connection requests
 */
int main(int argc, char **argv) {
  int listenfd, *connfdp;                 // listenfd 는 하나만 사용하기 때문에 int variable 이어도 괜찮다. 하지만 connfd의 경우 변수에 저장해서 thread 에 넘겨주는 경우, 다음 connection에서 해당 파일이 override될 수 있기 때문에 매번 새로 dynamic하게 할당되는 메모리 공간에 들어간 connfd를 dereference 해서 사용하는 것이 안전하다.
  char hostname[MAXLINE], port[MAXLINE];  // for checking host and port of the request that proxy receives
  struct sockaddr_storage clientaddr;
  socklen_t clientlen;
  pthread_t tid;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  printf("Proxy server initiated\n\n");

  /* listen */
  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfdp = Malloc(sizeof(int));
    *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n\n", hostname, port);
    Pthread_create(&tid, NULL, thread, connfdp);
  }

  return 0;
}

/*
 * thread - working thread for dealing with each client request
 */
void *thread(void *vargp) {
  int connfd = *((int *)vargp);
  Pthread_detach(Pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  printf("Connection with client closed\n\n");
}

/*
 * doit - actions of each working thread
 *      - reading client requests
 *      - sending connection requests to the end server
 *      - forwarding the response from end server to client
 */
void doit(int connfd) {
  int clientfd;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char request_header[MAXLINE], port[MAXLINE];
  rio_t client_rio, server_rio;

  /* Read request line and headers */
  Rio_readinitb(&client_rio, connfd);             // initiate buffer by reading fd into custom data type rio
  Rio_readlineb(&client_rio, buf, MAXLINE);       // read the first line from rio into buffer
  printf("Incoming Request headers:\n");                   // start printing the received request header
  printf("%s", buf);                              // echo the request line
  sscanf(buf, "%s %s %s", method, uri, version);  // divide request line into method, uri, vesion

  /* Check methods */
  if(strcasecmp(method,"GET")){
    clienterror(connfd, method, "501", "Not implemented", "Proxy does not implement this method");
    return;
  }

  /********** get port for test purpose **********/
  char hostname[MAXLINE], path[MAXLINE];
  int int_port;
  /* Parse the uri to get hostname, file path, port for test purpose */
  parse_uri(uri, hostname, path, &int_port);

  /* build the request header to send to the end server */
  build_requesthdrs(request_header, hostname, path, int_port, &client_rio);

  /* convert int to string */
  sprintf(port, "%d", int_port);
  /********** get port for test purpose **********/

  /********** for serving proxy server to browser **********/
  // /* build the request header to send to the end server */
  // build_requesthdrs(request_header, hostname, uri, endsrv_port, &client_rio);

  // /* convert int to string */
  // sprintf(port, "%d", endsrv_port);
  /********** for serving proxy server to browser **********/

  /* connect with the end server */
  printf("Connecting with end server\n");
  clientfd = Open_clientfd(hostname, port);
  if (clientfd < 0) {
    printf("Connection with end server failed\n");
    return;
  }
  printf("Connection established with end server\n\n");

  /* initiate buffer for end-server connection and send request header */
  Rio_readinitb(&server_rio, clientfd);
  Rio_writen(clientfd, request_header, strlen(request_header));
  printf("Outgoing Request headers:\n");
  printf("%s", request_header);
  
  /* read and write response from end-server to client */
  Rio_readlineb(&server_rio, buf, MAXLINE);     // read and write response line from end-server to client
  Rio_writen(connfd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);
  while (strcmp(buf, "\r\n")) {                 // 이전에 read and write 한 헤더가 null이 아니라면 다음줄 read and write
    Rio_readlineb(&server_rio, buf, MAXLINE);
    Rio_writen(connfd, buf, strlen(buf));
    printf("%s", buf);
  }
  
  /* read and write response body */
  size_t n;
  while((n=Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
    Rio_writen(connfd, buf, n);
  }
  printf("Response body sent to client\n");

  close(clientfd);
}

/*
 * build_requesthdrs - read from client request and build custom request header to send to the end server
 */
void build_requesthdrs(char *request_header, char *hostname, char *path, int port, rio_t *client_rio) {
  char buf[MAXLINE];
  char host_hdr[MAXLINE], other_hdr[MAXLINE];

  /* request line */
  sprintf(request_header, requestline_hdr_format, path);

  /* retrieve other headers from client rio */
  while (Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
    printf("%s", buf);  // print every line of request header from client

    if (!strcmp(buf, "\r\n")) {   // EOF
      break;
    }
    
    if (!strncasecmp(buf, host_hdr_key, strlen(host_hdr_key))) {   // Host header
      strcpy(host_hdr, buf);
      continue;
    }
    
    if (strncasecmp(buf, conn_hdr_key, strlen(conn_hdr_key)) 
        && strncasecmp(buf, prox_conn_hdr_key, strlen(prox_conn_hdr_key)) 
        && strncasecmp(buf, user_agent_hdr_key, strlen(user_agent_hdr_key))) {
      strcat(other_hdr, buf);
    }
  }

  /* fill out host header */
  if (!strlen(host_hdr)) {
    sprintf(host_hdr, host_hdr_format, hostname);
  }
  
  /* build request header */
  sprintf(request_header, "%s%s%s%s%s%s\r\n", 
      request_header, host_hdr, user_agent_hdr, conn_hdr, prox_conn_hdr, other_hdr);
}

/* 
 * clienterror - writing error message to client
 */
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

/* Parse the uri to get hostname, file path, port for test purpose */
void parse_uri(char *uri, char *hostname, char *path, int *port) {
    *port = 80;
    char* pos = strstr(uri,"//");

    pos = pos!=NULL? pos+2:uri;

    char*pos2 = strstr(pos,":");
    if(pos2!=NULL)
    {
        *pos2 = '\0';
        sscanf(pos,"%s",hostname);
        sscanf(pos2+1,"%d%s",port,path);
    }
    else
    {
        pos2 = strstr(pos,"/");
        if(pos2!=NULL)
        {
            *pos2 = '\0';
            sscanf(pos,"%s",hostname);
            *pos2 = '/';
            sscanf(pos2,"%s",path);
        }
        else
        {
            sscanf(pos,"%s",hostname);
        }
    }
    return;
}