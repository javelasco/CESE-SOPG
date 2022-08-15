#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>

#define FIFO_NAME   "myFIFO"
#define BUFFER_SIZE 512

volatile int32_t fd;

void sigusr1_handler(int sig) {
    write(fd, "SIGN: 1\n", 8);
}

void sigusr2_handler(int sig) {
    write(fd, "SIGN: 2\n", 8);
}

int main(void) {
    uint8_t outputBuffer[BUFFER_SIZE];
    const uint8_t data[] = "DATA: ";
    const uint8_t sign[] = "SIGN: ";
    int32_t returnCode, bytesWrote;
    struct sigaction sa1;
    struct sigaction sa2;

    /* Create named FIFO. -1 means already exists so no action if already exists. */
    if( (returnCode = mknod(FIFO_NAME, S_IFIFO | 0666, 0) < -1) ) {
        printf("Error creating named FIFO: %d\n", returnCode);
        exit(1);
    }

    /* Open named FIFO. Blocks until other process opens it. */
    printf("Waiting for readers...\n");
    if( (fd = open(FIFO_NAME, O_WRONLY)) < 0 ) {
        printf("Error opening named FIFO file: %d\n", fd);
        exit(1);
    }
    
    /* Open syscalls returned without error -> other process attached to named FIFO. */
    printf("Got a reader --type some stuff\n");

    sa1.sa_handler = sigusr1_handler;
    sa1.sa_flags = 0;
    sigemptyset(&sa1.sa_mask);

    if( sigaction(SIGUSR1, &sa1, NULL) == -1 ) {
        perror("Error sigaction USR1.");
        exit(1);
    }

    sa2.sa_handler = sigusr2_handler;
    sa2.sa_flags = 0;
    sigemptyset(&sa2.sa_mask);

    if( sigaction(SIGUSR2, &sa2, NULL) == -1 ) {
        perror("Error sigaction USR2.");
        exit(1);
    }

    /* Loop forever. */
    while(1) {
        if( fgets(outputBuffer, BUFFER_SIZE, stdin) == NULL ) {
            perror("Error fgets.");
        } else {
            uint8_t *p = malloc( (strlen(outputBuffer) + 1) );
            if(p == NULL) {
                perror("Dont memory available.");
                exit(1);
            }
            
            strcat(strcpy(p, data), outputBuffer);

            if( (bytesWrote = write(fd, p, strlen(p))) == -1 ) {
                perror("Error Write.");
            } else {
                printf("Write: wrote %d bytes\n", bytesWrote);
            }
            free(p);
        }
    }

    return 0;
}