#include <arpa/inet.h>
#include <math.h>   /* fmin */
#include <netdb.h>
#include <netinet/in.h>
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

typedef struct
{
    char name[100];
    char password[100];
} User;

typedef struct
{
    char name[10];
    char IP[45];
    int port;
    struct sockaddr_in server;
    socklen_t serverLen;
    int sock;
} ServerInfo;

typedef struct
{
    char fileName[50];
    int pieceNum;
    int fileInd;
    int bytes;
} PieceInfo;

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
        printf("Was unable to connect to %s\n", serverInfo->name);
        return false;
    }
    return true;
}

int connectToServers(ServerInfo servers[NUMSERVERS])
{
    int connsMade = 0;
    for (int i = 0; i < NUMSERVERS; i++)
    {
        if (!connectToServer(&servers[i]))
        {
            continue;
        }
        connsMade++;
    }
    return connsMade;
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

void serializeUser(char *buf, User *user)
{
    snprintf(buf, 200, "%s %s \r\n\r\n", user->name, user->password);
}

bool isValidUser(ServerInfo *serverInfo, User *user, char *buffer)
{
    serializeUser(buffer, user);
    send(serverInfo->sock, buffer, strlen(buffer), 0);

    bzero(buffer, BUFLEN);

    int bytesReceived = recvfrom(serverInfo->sock, buffer, BUFLEN, 0, (struct sockaddr *) &serverInfo->server, &serverInfo->serverLen);
    //printf("received %d bytes from server %s\n", bytesReceived, serverInfo->name);

    int validUser = 0;
    sscanf(buffer, "%d \r\n\r\n", &validUser);
    bzero(buffer, BUFLEN);
    
    return validUser == 1;
}

bool list(ServerInfo servers[NUMSERVERS], User *user, char *fileName)
{
    return false;
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

bool sendPiece(FILE *fp, int serverSock, PieceInfo *pieceInfo, char *buffer)
{
    if (!sendPieceInfo(serverSock, pieceInfo, buffer))
    {
        return false;
    }

    // fseek(fp, pieceInfo->fileInd, SEEK_SET);

    // int bytesRead = 0;
    // int bytesToBeRead = pieceInfo->bytes;

    // int readBufferSize = fmin(bytesToBeRead, BUFLEN);
    // while ( bytesToBeRead > 0 && (bytesRead = fread(buffer, 1, readBufferSize, fp)) > 0)
    // {
    //     write(serverSock, buffer, bytesRead);
    //     bytesToBeRead -= bytesRead;
    //     readBufferSize = fmin(bytesToBeRead, BUFLEN);
    //     bzero(buffer, BUFLEN);
    // }
    
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

    for (int i = 0; i < totalPieces; i++)
    {
        printPieceInfo(&pieces[i]);
    }

    for (int i = 0; i < NUMSERVERS; i++)
    {
        if (!isValidUser(&servers[i], user, buffer))
        {
            printf("Invalid user credentials to server %d\n", i);
            continue;
        }
        
        PieceInfo firstPiece = pieces[i * 2];
        if (!sendPiece(fp, servers[i].sock, &firstPiece, buffer))
        {
            printf("failed to send piece %d of %s to server %d\n", i*2, fileName, i);
        }

        // PieceInfo secondPiece = pieces[(i*2]+1)];
        // if (!sendPiece(fp, servers[i].sock, &firstPiece, buffer))
        // {
        //     printf("failed to send piece %d of %s to server %d\n", i, fileName, currentServer);
        // }
    }

    fclose(fp);
    return false;
}

bool get(ServerInfo servers[NUMSERVERS], User *user, char *fileName)
{
    return false;
}

void closeServerSockets(ServerInfo servers[NUMSERVERS])
{
    for (int i = 0; i < NUMSERVERS; i++)
    {
        close(servers[i].sock);
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

    if (!parseConfigFile(argv[1], servers, &user))
    {
        printf("Unable to parse %s file\n", argv[1]);
        return -1;
    }
    
    int serversAvailable;
    char command[10];
    char fileName[50];
    while (1)
    {
        serversAvailable = connectToServers(servers);
        printf("Connected to %d/%d servers\n", serversAvailable, NUMSERVERS);
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
        
        closeServerSockets(servers);
        if (strcmp(command, "exit") == 0)
        {
            return 0;
        }
        printf("\n");
    }

    return 0;
}