#include <arpa/inet.h>
#include <math.h>   /* fmin */
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>    /* bzero */
#include <sys/socket.h>  /* for socket use */
#include <sys/stat.h>
#include <unistd.h> /* read, write */

#if defined(__APPLE__)
#  define COMMON_DIGEST_FOR_OPENSSL
#  include <CommonCrypto/CommonDigest.h>
#  define SHA1 CC_SHA1
#else
#  include <openssl/md5.h>
#endif

#define HASHLEN 32
#define NUMSERVERS 4
#define NUMPIECES (NUMSERVERS * 2)
#define BUFLEN 8192
#define FILELIMIT 20
#define DOWNLOADSDIR "downloads"

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

typedef struct
{
    char name[10];
    char IP[45];
    int port;
    struct sockaddr_in server;
    socklen_t serverLen;
    int sock;
} ServerInfo;

typedef struct FileInfo
{
    char *fileName;
    int *pieceLocs;
    int *pieceSizes;
    struct FileInfo *next;
} FileInfo;

bool parseConfigFile(char *dfConFileName, ServerInfo servers[NUMSERVERS], User *user)
{
    FILE* dfConFile;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    dfConFile = fopen(dfConFileName, "r");
    if (!dfConFile)
    {
        printf("Unable to open %s\n", dfConFileName);
        return false;
    }

    int i = 0;
    while (i < NUMSERVERS && (read = getline(&line, &len, dfConFile)) != -1)
    {
        sscanf(line, "Server %s %[^:]:%d\n", servers[i].name, servers[i].IP, &servers[i].port);
        i++;
    }

    if ((read = getline(&line, &len, dfConFile)) != -1)
    {
        sscanf(line, "Username: %s\n", user->name);
    }
    if ((read = getline(&line, &len, dfConFile)) != -1)
    {
        sscanf(line, "Password: %s\n", user->password);
    }

    fclose(dfConFile);
    if (line)
        free(line);
    
    return true;
}

bool connectToServer(ServerInfo *serverInfo)
{
    serverInfo->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverInfo->sock == -1)
    {
        printf("Failed to create server socket\n");
        return false;
    }

    serverInfo->server.sin_family = AF_INET;
    inet_aton(serverInfo->IP, &serverInfo->server.sin_addr);
    serverInfo->server.sin_port = htons(serverInfo->port);

    serverInfo->serverLen = sizeof(serverInfo->server);
    int conn = connect(serverInfo->sock, (struct sockaddr *) &serverInfo->server, serverInfo->serverLen);
    if (conn == -1)
    {
        //printf("Was unable to connect to %s\n", serverInfo->name);
        return false;
    }
    return true;
}

void printCommands() 
{
    printf("Please enter one of the following commands: \n");
    printf("  - %s\n", "list");
    printf("  - %s\n", "get [file_name]");
    printf("  - %s\n", "put [file_name]");
    printf("  - %s\n", "exit");
}

void getCommand(char *command, char *fileName)
{
    printCommands();
    bzero(command, strlen(command));
    bzero(fileName, strlen(fileName));

    char buf[100];
    fgets(buf, 100, stdin);
    sscanf(buf, "%s %s\n", command, fileName);
}

void serializeRequest(char *buf, User *user, char *command)
{
    snprintf(buf, 200, "%s %s %s \r\n\r\n", user->name, user->password, command);
}

bool isValidRequest(int serverSock, User *user, char *command, char *buffer)
{
    serializeRequest(buffer, user, command);
    write(serverSock, buffer, strlen(buffer));

    bzero(buffer, BUFLEN);
    int bytesReceived = recv(serverSock, buffer, BUFLEN, 0);
    //printf("received %d bytes from server %s\n", bytesReceived, serverInfo->name);

    int validUser = 0;
    sscanf(buffer, "%d \r\n\r\n", &validUser);
    bzero(buffer, BUFLEN);
    
    return validUser == 1;
}

void initializeFileInfo(FileInfo *fileInfo)
{
    fileInfo->pieceLocs = malloc( (NUMSERVERS * 2) * sizeof(*fileInfo->pieceLocs));
    fileInfo->pieceSizes = malloc( NUMSERVERS * sizeof(*fileInfo->pieceLocs));
    for (int i = 0; i < NUMSERVERS*2; i++)
    {
        fileInfo->pieceLocs[i] = -1;
    }
}

bool loadFileInfo(char *line, int serverInd, struct FileInfo** head)
{
    char fileName[50];
    int pieceNum = -1;
    int pieceSize = -1;
    sscanf(line, "%s %d %d", fileName, &pieceNum, &pieceSize);
    //printf("%s %d %d\n", fileName, pieceNum, pieceSize);

    FileInfo *crawl = *head;
    while ( crawl != NULL && strcmp(crawl->fileName, fileName) != 0)
    {
        crawl = crawl->next;
    }
    
    if (crawl == NULL)
    {
        crawl = (FileInfo*) malloc(sizeof(FileInfo));

        crawl->next = (*head);

        (*head) = crawl;
    }
    
    if (crawl->fileName == NULL)
    {
        initializeFileInfo(crawl);
        crawl->fileName = malloc(strlen(fileName)+1);
        strcpy(crawl->fileName, fileName);
    }

    int pieceInd = (pieceNum-1)*2;
    if (crawl->pieceLocs[pieceInd] == -1)
    {
        crawl->pieceLocs[pieceInd] = serverInd;
    }
    else
    {
        crawl->pieceLocs[pieceInd+1] = serverInd;
    }
    crawl->pieceSizes[pieceNum-1] = pieceSize;
    return true;
}

bool parseFileInfoList(char *buffer, int serverInd, struct FileInfo** head)
{
    char * line = strtok(strdup(buffer), "\n");
    while(line) {
        //printf("%s\n", line);
        loadFileInfo(line, serverInd, head);
        line  = strtok(NULL, "\n");
    }
    return true;
}

// bool getFilePieceList(ServerInfo servers[NUMSERVERS], User *user, char *fileName, PieceNode pieceList[NUMSERVERS])
bool getFileInfoList(ServerInfo servers[NUMSERVERS], User *user, char *fileName, FileInfo** head)
{
    char buffer[BUFLEN];
    
    for (int i = 0; i < NUMSERVERS; i++)
    {
        if (!connectToServer(&servers[i]))
        {
            continue;
        }
        if (!isValidRequest(servers[i].sock, user, "list", buffer))
        {
            printf("Invalid user credentials to %s\n", servers[i].name);
            close(servers[i].sock);
            return false;
        }

        recv(servers[i].sock, buffer, BUFLEN, 0);
        
        if (strcmp(buffer, "Empty") == 0 || strlen(buffer) == 0)
        {
            printf("server %s got nothing\n", servers[i].name);
        }
        else
        {
            parseFileInfoList(buffer, i, head);
        }
        bzero(buffer, BUFLEN);
        //printf("%s done\n", servers[i].name);
        close(servers[i].sock);
    }
    return true;
}

bool allPiecesAvailable(FileInfo* fi)
{
    bool pieces[NUMSERVERS];
    for (int i = 0; i < NUMSERVERS; i++)
    {
        pieces[i] = fi->pieceLocs[i*2] != -1 || fi->pieceLocs[(i*2)+1] != -1;
    }
    for (int i = 0; i < NUMSERVERS; i++)
    {
        if (!pieces[i])
        {
            //printf("piece %d missing for %s\n", i, fi->fileName);
            return false;
        }
    }
    return true;
}

void printFileInfoList(struct FileInfo** head)
{
    FileInfo* cursor = *head;
    while (cursor != NULL)
    {
        printf("- %s", cursor->fileName);
        bool isFileComplete = allPiecesAvailable(cursor);
        if (!isFileComplete)
        {
            printf(" [incomplete]");
        }
        printf("\n");
        cursor = cursor->next;
    }
}

void freeFileInfoList(FileInfo* head)
{
    FileInfo *tmp;
    while (head != NULL)
    {
        tmp = head;
        head = head->next;
        if (tmp->fileName != NULL)
        {
            free(tmp->fileName);
        }
        free(tmp->pieceLocs);
        free(tmp->pieceSizes);
        free(tmp);
    }
}

bool list(ServerInfo servers[NUMSERVERS], User *user, char *fileName)
{
    struct FileInfo *head = NULL;

    getFileInfoList(servers, user, fileName, &head);

    printFileInfoList(&head);
    
    freeFileInfoList(head);
    return true;
}

long int getFileSize(FILE* file)
{
    fseek(file, 0L, SEEK_END);
    long int fileSize = ftell(file);
    fseek(file, 0L, SEEK_SET);
    return fileSize;
}

int getMD5HashInt(FILE* fp)
{
    CC_MD5_CTX context;
    CC_MD5_Init(&context);
    char hashStr[HASHLEN];
    unsigned char digest[CC_MD5_DIGEST_LENGTH];

    int bufSize = 1024;
    char buffer[bufSize];
    int bytesRead, bytesToBeRead;

    bytesToBeRead = getFileSize(fp);
    fseek(fp, 0, SEEK_SET);

    while ( (bytesRead = fread(buffer, 1, fmin(bufSize, bytesToBeRead), fp)) > 0)
    {
        CC_MD5_Update(&context, buffer, bytesRead);
        bytesToBeRead -= bytesRead;
        bzero(buffer, bufSize);
    }
    CC_MD5_Final(digest, &context);
    
    for(int i = 0; i < CC_MD5_DIGEST_LENGTH; ++i)
    {
        sprintf(&hashStr[i*2], "%02x", (unsigned int)digest[i]);
    }
    
    int hashInt = digest[MD5_DIGEST_LENGTH - 1] % 4;
    printf("hashStr: %s\n", hashStr);
    printf("hashInt: %d\n", hashInt);

    return hashInt;
}

void assignPieces(FILE *fp, PieceInfo pieces[NUMPIECES])
{
    int fileHashInt = getMD5HashInt(fp);
    for (int i = 0; i < NUMSERVERS; i++)
    {
        pieces[i*2].pieceNum = ((((NUMSERVERS - fileHashInt) % NUMSERVERS) + i) % NUMSERVERS);
        pieces[(i*2)+1].pieceNum = (pieces[i*2].pieceNum + 1) % NUMSERVERS;
    }
}

void getPieceInds(int fileSize, PieceInfo pieces[NUMPIECES])
{
    int pieceLen = fileSize / NUMSERVERS;
    for (int i=0; i < NUMPIECES; i++)
    {
        pieces[i].fileInd = pieces[i].pieceNum * pieceLen;
        pieces[i].bytes = pieces[i].pieceNum != (NUMPIECES - 1) ? pieceLen : fileSize - (pieceLen * (NUMPIECES-1));
    }
}

void serializePieceInfo(PieceInfo *pieceInfo, char *buf)
{
    snprintf(buf, BUFLEN, "%s %d %d %d \r\n\r\n", pieceInfo->fileName, pieceInfo->pieceNum, pieceInfo->fileInd, pieceInfo->bytes);
}

void printPieceInfo(PieceInfo *pieceInfo)
{
    printf("%s %d %d %d\n", pieceInfo->fileName, pieceInfo->pieceNum, pieceInfo->fileInd, pieceInfo->bytes);
}

bool sendPieceInfo(int serverSock, PieceInfo *pieceInfo, char *buf)
{
    serializePieceInfo(pieceInfo, buf);
    bool allBytesSent = write(serverSock, buf, strlen(buf)) == strlen(buf);
    bzero(buf, BUFLEN);
    return allBytesSent;
}

void XOR(char *string, char *key)
{
    int keyLen = strlen(key);
    for (int i = 0; i < strlen(string); i++)
    {
        string[i] = string[i] ^ key[i % keyLen];
    }
}

bool sendPiece(FILE *fp, ServerInfo *server, User *user, PieceInfo *pieceInfo, char *buffer)
{
    if (!connectToServer(server))
    {
        printf("unable to connect to %s\n", server->name);
        return false;
    }
    if (!isValidRequest(server->sock, user, "put", buffer))
    {
        printf("Invalid user credentials to %s\n", server->name);
        return false;
    }
    if (!sendPieceInfo(server->sock, pieceInfo, buffer))
    {
        return false;
    }
    fseek(fp, pieceInfo->fileInd, SEEK_SET);

    int bytesRead = 0;
    int bytesToBeRead = pieceInfo->bytes;
    int readBufferSize = fmin(bytesToBeRead, BUFLEN);
    
    if (recv(server->sock, buffer, 1, 0) != 1)
    {
        printf("no confirmation received\n");
        close(server->sock);
        return false;
    }

    while ( bytesToBeRead > 0 && (bytesRead = fread(buffer, 1, readBufferSize, fp)) > 0)
    {
        int sentBytes = write(server->sock, buffer, bytesRead);
        //printf("sent %d bytes for piece %d\n", sentBytes, pieceInfo->pieceNum);
        bytesToBeRead -= bytesRead;
        
        readBufferSize = fmin(bytesToBeRead, BUFLEN);
        bzero(buffer, BUFLEN);
    }
    // char *piece = calloc(pieceInfo->bytes, 0);
    // int bytesRead = fread(piece, 1, pieceInfo->bytes, fp);
    // write(serverSock, piece, bytesRead);
    // free(piece);
    close(server->sock);
    return true;
}

bool put(ServerInfo servers[NUMSERVERS], User *user, char *fileName)
{
    FILE* fp = fopen(fileName, "rb");
    if (!fp)
    {
        printf("Couldn't open '%s'\n", fileName);
        return false;
    }

    char buffer[BUFLEN];

    int totalPieces = NUMSERVERS * 2;
    PieceInfo pieces[totalPieces];
    for (int i = 0; i < totalPieces; i++)
    {
        strcpy(pieces[i].fileName, fileName);
    }

    assignPieces(fp, pieces);
    int fileSize = getFileSize(fp);
    getPieceInds(fileSize, pieces);

    // for (int i = 0; i < totalPieces; i++)
    // {
    //     printPieceInfo(&pieces[i]);
    // }

    for (int i = 0; i < NUMSERVERS; i++)
    {   
        PieceInfo firstPiece = pieces[i * 2];
        if (!sendPiece(fp, &servers[i], user, &firstPiece, buffer))
        {
            printf("failed to send piece %d of %s to server %d\n", i*2, fileName, i);
        }

        PieceInfo secondPiece = pieces[(i*2)+1];
        if (!sendPiece(fp, &servers[i], user, &secondPiece, buffer))
        {
            printf("failed to send piece %d of %s to server %d\n", (i*2)+1, fileName, i);
        }
    }

    fclose(fp);
    return true;
}

FileInfo* getFileInfo(char *fileName, struct FileInfo** head)
{
    FileInfo* cursor = *head;
    while (cursor != NULL)
    {
        if (strcmp(cursor->fileName, fileName) == 0)
        {
            return cursor;
        }
        cursor = cursor->next;
    }
    return cursor;
}

bool get(ServerInfo servers[NUMSERVERS], User *user, char *fileName)
{
    struct FileInfo *head = NULL;
    getFileInfoList(servers, user, fileName, &head);

    FileInfo *fileInfo = getFileInfo(fileName, &head);

    if (fileInfo == NULL)
    {
        printf("Didn't find %s\n", fileName);
    }
    printf("%s\n", fileInfo->fileName);
    for (int i = 0; i < NUMSERVERS*2; i++)
    {
        printf("%d %d %d\n", i, fileInfo->pieceLocs[i], fileInfo->pieceSizes[i/2]);
    }

    int totalFileSize = 0;
    return true;
}

void validateDir(char *dirName)
{
    struct stat st = {0};

    if (stat(dirName, &st) == -1) {
        mkdir(dirName, 0700);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) 
    {
        fprintf(stderr, "usage: %s <dfc.conf>\n", argv[0]);
        exit(0);
    }

    ServerInfo servers[NUMSERVERS];
    User user;

    validateDir(DOWNLOADSDIR);

    if (!parseConfigFile(argv[1], servers, &user))
    {
        printf("Unable to parse %s file\n", argv[1]);
        return -1;
    }
    
    char command[10];
    char fileName[50];
    while (1)
    {
        getCommand(command, fileName);
        
        if (strcmp(command, "list") == 0)
        {
            if (!list(servers, &user, fileName))
            {
                printf("'list' failed\n");
            }
        }
        else if (strcmp(command, "put") == 0)
        {
            if (!put(servers, &user, fileName))
            {
                printf("'put %s' failed\n", fileName);
            }
        }
        else if (strcmp(command, "get") == 0)
        {
            if (!get(servers, &user, fileName))
            {
                printf("'get %s' failed\n", fileName);
            }
        }
        if (strcmp(command, "exit") == 0)
        {
            return 0;
        }
        printf("\n");
    }

    return 0;
}