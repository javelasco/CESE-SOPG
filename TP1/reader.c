#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

#define FIFO_NAME "myFIFO"
#define BUFFER_SIZE 512

int main(void)
{
    FILE *myFile;
	uint8_t inputBuffer[BUFFER_SIZE];
    const uint8_t data[] = "DATA";
    const uint8_t signals[] = "SIGN";
	int32_t bytesRead, returnCode, fd;
    
    /* Create named FIFO. -1 means already exists so no action if already exists. */
    if( (returnCode = mknod(FIFO_NAME, S_IFIFO | 0666, 0) < -1) ) {
        printf("Error creating named FIFO: %d\n", returnCode);
        exit(1);
    }
    
    /* Open named FIFO. Blocks until other process opens it. */
	printf("Waiting for writers...\n");
	if ( (fd = open(FIFO_NAME, O_RDONLY) ) < 0 ) {
        printf("Error opening named FIFO file: %d\n", fd);
        exit(1);
    }
    
    /* Open syscalls returned without error -> other process attached to named FIFO. */
	printf("Got a writer\n");

    /* Loop until read syscall returns a value <= 0. */
	do {
        /* read data into local buffer */
		if ( (bytesRead = read(fd, inputBuffer, BUFFER_SIZE)) == -1 ) {
			perror("Read");
        } else {
            
            if( !strncmp(inputBuffer, data, 4) ) {
                /* Open file logs.txt. */
                if( (myFile = fopen("logs.txt", "a")) == NULL ) {
                    printf("Error open file logs.txt.\n");
                }
            } else if(!strncmp(inputBuffer, signals, 4)) {
                /* Open file signals.txt. */
                if( (myFile = fopen("signals.txt", "a")) == NULL ) {
                    printf("Error open file signals.txt.\n");
                }
            } 
            
			inputBuffer[bytesRead] = '\0';
            fputs(inputBuffer, myFile);
			printf("Reader: read %d bytes.\n%s", bytesRead, inputBuffer);
            fclose(myFile);
		}
	} while (bytesRead > 0);

	return 0;
}