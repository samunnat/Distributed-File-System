#include <arpa/inet.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <sys/socket.h>  /* for socket use */
#include <time.h>
#include <unistd.h>      /* for read, write */

#define LISTENQ  1024  /* second argument to listen */
#define BUFSIZE 8192

int open_listenfd(int port);
void *thread(void *vargp);

int main(int argc, char **argv)
{
    int listenfd, *connfdp, port;
    socklen_t clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid; 

    if (argc < 3) 
    {
        fprintf(stderr, "usage: %s <DFSN> <port>\n", argv[0]);
        exit(0);
    }

    port = atoi(argv[2]);

    listenfd = open_listenfd(port);
    while (1)
    {
        connfdp = malloc(sizeof(int));
        *connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
        pthread_create(&tid, NULL, thread, connfdp);
    }

    printf("in server\n");
    return 0;
}

void handleRequest(int clientSock)
{
    char buffer[BUFSIZE];
    ssize_t n;

    n = read(clientSock, buffer, BUFSIZE);
    printf("%s\n", buffer);
}

/* thread routine */
void * thread(void * vargp) 
{  
    int clientSock = *((int *)vargp);
    pthread_detach(pthread_self()); 
    free(vargp);
    handleRequest(clientSock);
    close(clientSock);
    return NULL;
}

/* 
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure 
 */
int open_listenfd(int port) 
{
    int listenfd, optval=1;
    struct sockaddr_in proxyaddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &proxyaddr, sizeof(proxyaddr));
    proxyaddr.sin_family = AF_INET; 
    proxyaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    proxyaddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (struct sockaddr*)&proxyaddr, sizeof(proxyaddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
} /* end open_listenfd */