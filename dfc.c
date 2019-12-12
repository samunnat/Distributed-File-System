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
    char fileName[100];
    int topPieceNum;
    int topPieceSize;
    int bottomPieceNum;
    int bottomPieceSize;
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
            printf("poo for server %d\n", i+1);
            continue;
        }
        connsMade++;

        char serverBuffer[100];
        strcpy(serverBuffer, "testing yoooo\n");
        send(servers[0].sock, serverBuffer, sizeof(serverBuffer), 0);

        close(servers[0].sock);
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

bool list(ServerInfo servers[NUMSERVERS], User *user, char *fileName)
{
    return false;
}

bool put(ServerInfo servers[NUMSERVERS], User *user, char *fileName)
{
    return false;
}

bool get(ServerInfo servers[NUMSERVERS], User *user, char *fileName)
{
    return false;
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
        printf("Unable to parse %s file", argv[1]);
        return -1;
    }
    
    if (connectToServers(servers) != NUMSERVERS)
    {
        printf("poo\n");
    }

    char command[10];
    char fileName[50];
    while (1)
    {
        getCommand(command, fileName);

        if (strcmp(command, "exit") == 0)
        {
            return 0;
        }
        else if (strcmp(command, "list") == 0)
        {
            if (!list(servers, &user, fileName))
            {
                printf("'list' operation failed\n");
            }
        }
        else
        {
            if (strlen(fileName) == 0)
            {
                printf("Please enter a filename\n");
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
        }
        printf("\n");
    }

    return 0;
}