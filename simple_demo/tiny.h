#ifndef _TINY_H_
#define _TINY_H_

#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFSIZE     512
#define PATHSIZE    128
#define MAXERRS     16

typedef struct {
    int parentfd;                       /* parent socket */
    int portno;                         /* port to listen on */
    struct sockaddr_in serveraddr;      /* server's addr */
} server_t;

typedef struct {
    int childfd;                        /* child socket */
    struct sockaddr_in clientaddr;      /* client addr */
    FILE *stream;                       /* stream version of childfd */
} client_t;

typedef struct {
    char buf[BUFSIZE];                  /* message buffer */
    char method[16];                    /* request method */
    char uri[PATHSIZE];                 /* request uri */
    char version[32];                   /* request method */
    char filename[PATHSIZE];            /* path derived from uri */
} request_t;


/* Create, initialize, bind, listen on server socket */
void server_connect(server_t* si, int port);

/* Accept client's connection and open fstream */
void client_connect(const server_t* si, client_t* ci);

/* Close client stream and socket */
void client_disconnect(client_t* ci);

/* Process client request */
void client_request_info(const client_t* ci, request_t* ri);

/* Serve static content to the client */
int serve_static_content(client_t* ci, request_t* ri, char* buf, int len);

/* Send error message to the client */
void cerror(FILE *s, char *cause, char *errno, char *shortmsg, char *longmsg);

#endif /* _TINY_H_ */