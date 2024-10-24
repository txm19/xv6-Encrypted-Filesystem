#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int decrypt(int fd, uint8 key);

int main(int argc, char *argv[]) 
{

    uint8 key_decrypt = (uint8)atoi(argv[1]);

    // Open the encrypted file for decryption
    int file_decrypt = open("README", 2);

    // Decrypt the file
    //printf("fd: %d\n", file_encrypt);
    if (decrypt(file_decrypt, key_decrypt) == -1)
        printf("decryption error\n");

    close(file_decrypt);

    return 0;
}