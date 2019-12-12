#include <math.h>   /* fmin */
#include <stdio.h>
#include <stdlib.h> /* strtol */
#include <strings.h>    /* bzero */
#include <unistd.h> /* read, write */

#if defined(__APPLE__)
#  define COMMON_DIGEST_FOR_OPENSSL
#  include <CommonCrypto/CommonDigest.h>
#  define SHA1 CC_SHA1
#else
#  include <openssl/md5.h>
#endif

#define HASHLEN 32

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
    FILE* fp = fopen(argv[1], "rb");
    int md5HashSum = getMD5HashInt(fp);
    fclose(fp);
    
    printf("%d\n", md5HashSum);
    return 0;
}
