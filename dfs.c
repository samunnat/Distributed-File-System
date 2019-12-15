#include <arpa/inet.h>
#include <dirent.h>
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
#include <sys/stat.h>
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

void parseRequest(char *buf, User *user, char *command)
{
    sscanf(buf, "%s %s %s \r\n\r\n", user->name, user->password, command);
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

void getPieceFileName(char *userDir, PieceInfo *pieceInfo ,char *pieceFileName)
{
    snprintf(pieceFileName, 50, "%s/.%s.%d", userDir, pieceInfo->fileName, pieceInfo->pieceNum+1);
}

bool handlePut(int clientSock, User *user, char *userDir, char *buffer)
{
    printf("trying to handle put\n");

    PieceInfo pieceInfo;
    read(clientSock, buffer, BUFLEN);
    deserializePieceInfo(buffer, &pieceInfo);
    printPieceInfo(&pieceInfo);

    FILE *pieceFile;
    char pieceFileName[50];
    getPieceFileName(userDir, &pieceInfo, pieceFileName);
    pieceFile = fopen(pieceFileName, "wb+");

    if (!pieceFile)
    {
        return false;
    }

    // char *piece = calloc(pieceInfo.bytes, 0);
    // int received_bytes = read(clientSock, piece, pieceInfo.bytes);
    // fwrite(piece, 1, received_bytes, pieceFile);
    // free(piece)
    send(clientSock, "1", 1, 0);

    int received_bytes = 0;
    while ((received_bytes = read(clientSock, buffer, BUFLEN)) > 0) 
    {
        printf("received %d bytes\n", received_bytes);
        fwrite(buffer, 1, received_bytes, pieceFile);
        
        bzero(buffer, BUFLEN);
    }

    fclose(pieceFile);
    return true;
}

bool handleGet(int clientSock, User *user, char *userDir, char *buffer)
{
    printf("trying to handle get\n");
    return true;
}

void getFolderContents(char *userDir, char *buffer)
{
    
}

int getFileSize(char *fileName)
{
    struct stat st;
    stat(fileName, &st);
    return st.st_size;
}

bool handleList(int clientSock, User *user, char *userDirName, char *buffer)
{
    printf("trying to handle list\n");

    struct dirent *de;
    DIR *userDir = opendir(userDirName);
    de = readdir(userDir);  // .
    de = readdir(userDir);  // ..
    
    char fullFileName[50];
    char fileName[50];
    char extensionWithPieceNum[10];
    char extension[10];
    while((de = readdir(userDir)) != NULL)
    {
        char *fNameAfterDot = de->d_name + 1;
        sscanf(fNameAfterDot, "%[^.]%s", fileName, extensionWithPieceNum);
        //printf("epn: %s\n", extensionWithPieceNum);

        int pieceNum;
        sscanf(extensionWithPieceNum+1, "%[^.].%d", extension, &pieceNum);
        
        snprintf(fileName + strlen(fileName), 50, ".%s", extension);

        snprintf(fullFileName, 50, "%s/%s", userDirName, de->d_name);
        
        int fileSize = getFileSize(fullFileName);

        //printf("%s %d %d \n", fileName, pieceNum, fileSize);
        snprintf(buffer + strlen(buffer), BUFLEN, "%s %d %d \n", fileName, pieceNum, fileSize);

        bzero(fullFileName, 50);
        bzero(fileName, 50);
        bzero(extensionWithPieceNum, 10);
        bzero(extension, 10);
    }
    closedir(userDir);
    
    if (strlen(buffer) == 0)
    {
        strcat(buffer, "Empty");
    }

    send(clientSock, buffer, strlen(buffer), 0);
    printf("%s\n", buffer);
    return true;
}

void handleRequest(int clientSock)
{
    char buffer[BUFLEN];
    
    User user;
    char command[10];

    read(clientSock, buffer, BUFLEN);
    parseRequest(buffer, &user, command);

    bool isUserValid = isVerifiedUser(&user);
    printf("%s is %d\n", user.name, isUserValid);
    if (!isUserValid)
    {
        return;
    }

    char userDir[20];
    validateUserDir(user.name, userDir);

    snprintf(buffer, BUFLEN, "%d \r\n\r\n", isUserValid);
    send(clientSock, buffer, strlen(buffer), 0);
    bzero(buffer, BUFLEN);

    if (strcmp(command, "put") == 0)
    {
        handlePut(clientSock, &user, userDir, buffer);
    }
    else if(strcmp(command, "get") == 0)
    {
        handleGet(clientSock, &user, userDir, buffer);
    }
    else if(strcmp(command, "list") == 0)
    {
        handleList(clientSock, &user, userDir, buffer);
    }

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
    int listenfd, optVal;
    struct sockaddr_in proxyaddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;
    
    struct timeval t;    
    t.tv_sec = 1;
    t.tv_usec = 0;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(
                listenfd, 
                SOL_SOCKET, 
                SO_REUSEADDR,
                (const void *)&optVal, 
                sizeof(int)
            ) < 0)
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