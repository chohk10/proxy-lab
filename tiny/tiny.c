/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#include <stdbool.h>

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

  printf("Tiny server initiated\n\n");

  listenfd = Open_listenfd(argv[1]);    // start listening
  while (1) {                           // loop forever
    clientlen = sizeof(clientaddr);     
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd) {
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
  bool headFlag;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);            // initiate buffer by reading fd into custom data type rio
  Rio_readlineb(&rio, buf, MAXLINE);  // read from rio into buffer  ==> the first line is the request line
  printf("Request headers:\n");
  printf("%s", buf);                  // echo the request line
  sscanf(buf, "%s %s %s", method, uri, version);  // divide request line into method, uri, vesion
  
  /* Check methods */
  if (strstr(method, "GET")) {
    headFlag = false;
  } else if (strstr(method, "HEAD")) {
    headFlag = true;
  } else {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return; 
  }
  
  /* Read and ignore request headers */
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn’t find this file");
    return; 
  }

  if (is_static) {       /* Serve static content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn’t read the file");
      return; 
    }
    if (!headFlag) {
      serve_static(fd, filename, sbuf.st_size);
    } else {
      head_static(fd, filename, sbuf.st_size);
    }
  } else {               /* Serve dynamic content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn’t run the CGI program");
      return; 
    }
    if (!headFlag) {
      serve_dynamic(fd, filename, cgiargs);
    } else {
      head_dynamic(fd, filename, cgiargs);
    }    
  }
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

int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  if (!strstr(uri, "cgi-bin")) {  /* Static content */  // cgi-bin 이라는 string을 가지고 있지 않다면 static
    strcpy(cgiargs, "");    // cgi args는 없음
    strcpy(filename, ".");  // filename은 .으로 시작
    strcat(filename, uri);  // uri와 concatenate
    if (uri[strlen(uri)-1] == '/')    // /로 끝난다면
      strcat(filename, "home.html");  // concatenate html
    return 1; 
  }
  else {  /* Dynamic content */   // cgi-bin이라는 string을 가지고 있다면 해당 디렉토리에 있는 다이내믹 요소를 부른다는 뜻
    ptr = index(uri, '?');        // ?의 index를 찾음
    if (ptr) {                    // 인덱스가 있다면 (?가 존재한다면)
      strcpy(cgiargs, ptr+1);     // ? 뒤에 있는 내용들을 cgi args에 넣어줌
      *ptr = '\0';                // ?를 \0으로 대치해줌으로서 argument가 나오기 이전의 uri 부분을 분리해줌
    } else {                      // ?가 없다면 (argument가 없다면)
      strcpy(cgiargs, "");        // argument가 없는걸로 표시
    }
    strcpy(filename, ".");
    strcat(filename, uri);        // 앞서 분리한 uri 앞부분이 파일 디렉토리가 됨
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");                    // 버퍼에 첫줄 작성
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);     // 이제까지 작성된 내용을 %s 부분에 넣고 나머지 부분 합쳐주기
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));                       // 버퍼에 저장된 내용을 fd에 write 함으로써 네트워크 통신 
  printf("Response headers:\n");
  printf("%s", buf);                                      // 보낸 내용을 터미널에도 출력

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);                          // ??
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);   // 메모리가 할당되고, 데이터가 써지며, 메모리의 시작점을 return => srcp 는 할당된 메모리의 시작 포인터 
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);                               // srcp 지점에 저장된 데이터를 fd에 write
  Munmap(srcp, filesize);                                       // 보냈으니까 이제 삭제를 하는 것으로 보임 (unmap)
}

/* 
 * for returning header only when requested with HEAD method
 */
void head_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);
}

void get_filetype(char *filename, char *filetype) {
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = { NULL };
  
  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");        // 버퍼에 한줄씩 채우고 한줄씩 write 하는 방식
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  
  if (Fork() == 0) { /* Child process */      // fork 함수는 child process를 만들어준다
      /* Real server would set all CGI vars here */
      setenv("QUERY_STRING", cgiargs, 1);     // 새로운 환경 변수를 생성하고, 거기에 argument를 넣어줌 (이는 argument 사이에 &가 들어가있어서 리눅스 환경에서 argument로 넣어주기에는 오류가 발생할 수 있어서)
      Dup2(fd, STDOUT_FILENO);                /* Redirect stdout to client */   // child의 입장에서 print를 하면 바로 fd에 write가 되도록 함 --> duplicate를 하는데 stdout으로 만들어주니까 child 입장에서는 따로 write를 한다거나 fd를 받아서 조작하기보단 간단하게 print만 하면 되는 것
      Execve(filename, emptylist, environ);   /* Run CGI program */             // exec(ve) 등은 filename을 실행하는 것 --> 환경변수로 변수를 넘겨주는 방법을 채택 (argument로 넘겨주는 방법도 있음)
  }                 // emptylist <-- argument를 넣지 않는 상황!
  Wait(NULL); /* Parent waits for and reaps child */
}

/* 
 * for returning header only when requested with HEAD method
 */
void head_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = { NULL };
  
  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
}