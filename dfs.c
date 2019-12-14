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
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>      /* for read, write */

#define LISTENQ  1024  /* second argument to listen */
#define BUFLEN 8192
#define DFSCONFILE "dfs.conf"
char serverDir[20];

int open_listenfd(int port);
void *thread(void *vargp);

typedef struct
{
    char name[100];
    char password[100];
} User;

typedef struct
{
    char fileName[50];
    int pieceNum;
    int fileInd;
    int bytes;
} PieceInfo;

void validateDir(char *dirName)
{
    struct stat st = {0};

    if (stat(dirName, &st) == -1) {
        mkdir(dirName, 0700);
    }
}

void validateUserDir(char *userName, char *userDirName)
{
    strcpy(userDirName, serverDir);
    strcat(userDirName, "/");
    strcat(userDirName, userName);
    validateDir(userDirName);
}

void deserializeUser(char *buf, User *user)
{
    sscanf(buf, "%s %s \r\n\r\n", user->name, user->password);
    bzero(buf, BUFLEN);
}

bool isVerifiedUser(User *user)
{
    FILE* dfsConf;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    
    dfsConf = fopen(DFSCONFILE, "r");
    if (!dfsConf)
    {
        printf("Unable to open %s\n", DFSCONFILE);
        return false;
    }

    char username[100];
    char password[100];
    while ((read = getline(&line, &len, dfsConf)) != -1) 
    {
        sscanf(line, "%s %s\n", username, password);
        if (strcmp(username, user->name) == 0)
        {
            return strcmp(password, user->password) == 0;
        }

        memset(username, 0, sizeof(username));
        memset(password, 0, sizeof(password));
    }
    
    fclose(dfsConf);
    if (line)
        free(line);
    
    printf("User %s not found\n", user->name);
    return false;
}

void deserializePieceInfo(char *buf, PieceInfo *pieceInfo)
{
    sscanf(buf, "%s %d %d %d \r\n\r\n", pieceInfo->fileName, &pieceInfo->pieceNum, &pieceInfo->fileInd, &pieceInfo->bytes);
    bzero(buf, BUFLEN);
}

void printPieceInfo(PieceInfo *pieceInfo)
{
    printf("%s %d %d %d\n", pieceInfo->fileName, pieceInfo->pieceNum, pieceInfo->fileInd, pieceInfo->bytes);
}

bool getPieceFile(FILE *fp, char *userDir, PieceInfo *pieceInfo)
{
    char fileName[50];
    snprintf(fileName, 50, "%s/.%s.%d", userDir, pieceInfo->fileName, pieceInfo->pieceNum+1);
    fp = fopen(fileName, "wb+");
    return fp;
}

void handleRequest(int clientSock)
{
    char buffer[BUFLEN];
    
    User user;
    read(clientSock, buffer, BUFLEN);
    deserializeUser(buffer, &user);

    bool isUserValid = isVerifiedUser(&user);
    printf("%s is %d\n", user.name, isUserValid);

    snprintf(buffer, BUFLEN, "%d \r\n\r\n", isUserValid);
    send(clientSock, buffer, strlen(buffer), 0);
    
    char userDir[20];
    validateUserDir(user.name, userDir);

    PieceInfo pieceInfo;
    read(clientSock, buffer, BUFLEN);
    deserializePieceInfo(buffer, &pieceInfo);
    printPieceInfo(&pieceInfo);

    FILE *pieceFile = NULL;
    getPieceFile(pieceFile, userDir, &pieceInfo);
    
    // int bytesToBeRead = pieceInfo.bytes;
    // int bytesRead = 0;
    // while (bytesToBeRead > 0)
    // {
    //     bytesRead = read(clientSock, buffer, BUFSIZ);
    //     printf("%s", buffer);
    //     bytesToBeRead -= bytesRead;
    // }
    
    printf("done\n");
}

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

    strcpy(serverDir, argv[1]);
    validateDir(serverDir);

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