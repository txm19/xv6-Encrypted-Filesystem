
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int encrypt(int fd, uint8 key);

int main(int argc, char *argv[]) 
{

    uint8 key_encrypt = (uint8)atoi(argv[1]);

    //Fint file_encrypt = open("test_encrypt.txt", 2);
    //fprintf(file_encrypt, "This is a test file for encryption.");
    //close(file_encrypt);

    int file_encrypt = open("README", 2);

    // Step 3: Encrypt the file
    //printf("fd: %d\n", file_encrypt);
    if(encrypt(file_encrypt, key_encrypt) == -1)
        printf("encryption error\n");

    close(file_encrypt);

    return 0;
}