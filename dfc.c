#include <arpa/inet.h>
#include <math.h>   /* fmin */
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>    /* bzero */
#include <sys/socket.h>  /* for socket use */
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
    char fileName[100];
    int topPieceNum;
    int topPieceSize;
    int bottomPieceNum;
    int bottomPieceSize;
} FileInfo;

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

void printCommands() {
    printf("Please enter one of the following commands: \n");
    printf("  - %s\n", "list");
    printf("  - %s\n", "get [file_name]");
    printf("  - %s\n", "put [file_name]");
}

long int getFileSize(FILE* file)
{
    fseek(file, 0L, SEEK_END);
    long int fileSize = ftell(file);
    fseek(file, 0L, SEEK_SET);
    return fileSize;
}

int getIntFromMD5Hash(char hashStr[HASHLEN])
{
    int v1, v2, v3, v4;
    sscanf( &hashStr[0], "%4x", &v1 );
    sscanf( &hashStr[8], "%4x", &v2 );
    sscanf( &hashStr[16], "%4x", &v3 );
    sscanf( &hashStr[24], "%4x", &v4 );

    int hashInt = v1 ^ v2 ^ v3 ^ v4;
    return hashInt;
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
    printf("%s\n", hashStr);
    
    return getIntFromMD5Hash(hashStr);
}

int main(int argc, char **argv)
{
    if (argc < 2) 
    {
        fprintf(stderr, "usage: %s <dfc.conf>\n", argv[0]);
        exit(0);
    }
    
    ServerInfo testServer;
    strcpy(testServer.name, "DFS1");
    strcpy(testServer.IP, "127.0.0.1");
    testServer.port = 10001;

    if (!connectToServer(&testServer))
    {
        printf("poo\n");
    }

    char serverBuffer[100];
    strcpy(serverBuffer, "testing yoooo\n");
    send(testServer.sock, serverBuffer, sizeof(serverBuffer), 0);

    close(testServer.sock);
    
    printf("%d\n", md5HashSum);
    return 0;
}
