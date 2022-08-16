#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "SerialManager.h"
#include <signal.h>

/* Threads functions */
void* ThreadSerial(void* arg);
void* ThreadSocket(void* arg);
void sigint_handler(int sign);
void sigterm_handler(int sign);
void block_sign(void);
void unblock_sign(void);
void end_process(int status);


/* Internal functions */
int set_serveraddr(struct sockaddr_in * serveraddr);

/* Constants and variables */
#define BUFFER_SIZE 10
struct sockaddr_in serveraddr;
struct sockaddr_in clientaddr;
pthread_t threadSerial, threadSocket;
pthread_mutex_t myMutex = PTHREAD_MUTEX_INITIALIZER;
int fd, new_fd;
int sign_flag=0, accept_flag=0, error_flag=0;

/* Main code */
int main(void) {
	int ret;
	struct sigaction sa1;
	struct sigaction sa2;

	sa1.sa_handler = sigint_handler;
	sa1.sa_flags = 0;
	sigemptyset(&sa1.sa_mask);
	if(sigaction(SIGINT, &sa1, NULL) == -1) {
		perror("Sigaction");
		exit(1);
	}

	sa2.sa_handler = sigterm_handler;
	sa2.sa_flags = 0;
	sigemptyset(&sa2.sa_mask);
	if(sigaction(SIGTERM, &sa2, NULL) == -1) {
		perror("Sigaction");
		exit(1);
	}

	printf("Serial service starting...\r\n");
	ret = pthread_create(&threadSerial, NULL, ThreadSerial, NULL);

	if(ret != 0) {
		perror("pthreadSerial_create error");
		return -1;
	}
	printf("Serial thread created successfully.\r\n");

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd < 0) {
		perror("Error socket");
        exit(EXIT_FAILURE);
	}

	if (set_serveraddr(&serveraddr) < 0) {
		perror("Error serveraddr");
        exit(EXIT_FAILURE);
	}

	printf("Socket service starting...\r\n");
	ret = pthread_create(&threadSocket, NULL, ThreadSocket, NULL);

	if(ret != 0) {
		perror("pthreadSocket_create error");
		return -1;
	}

	printf("Socket thread created successfully.\r\n");


	while(!sign_flag && !error_flag) {
		usleep(100000);
	}

	end_process(EXIT_SUCCESS);
	return 0;
}


/* Thread 1 :: Serial*/
void* ThreadSerial(void* arg) {
	char buffer[BUFFER_SIZE];
	int bytes_read = 0;

	block_sign();

	if( serial_open(1, 115200) != 0) {
		perror("Failed to open serial port");
		error_flag = 1;
	}

	while(1) {
		bytes_read = serial_receive(buffer, BUFFER_SIZE);
			
		if(bytes_read == -1) {
			usleep(100000);
		} else if(bytes_read == 0) {
			if(serial_open(1, 115200) != 0) {
				perror("Failed to open serial port");
				error_flag = 1;
			}
			usleep(100000);
		} else {
			pthread_mutex_lock(&myMutex);
			if(accept_flag) {
				pthread_mutex_unlock(&myMutex);
				int n = write(new_fd, buffer, strlen(buffer));
				printf("****** Serial Thread ******\n\r");
				printf("Read: %d bytes.\n\r", bytes_read);
				printf("Data: %s", buffer);
			} else {
				printf("Socket closed!\n\r");
				error_flag = 1;
			}
		}
	}
}

/* Thread 2 :: Socket*/
void* ThreadSocket(void* arg) { 
	char buffer[BUFFER_SIZE];
	int n = 0;
	socklen_t addr_len;

	block_sign();

	/* Port opening */
	int bindError = 1;
	while(bindError) {
		if( bind(fd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1 ) {
			perror("listener bind");
			error_flag = 1;
		} else {
			bindError = 0;
		}
	}

	/* Socket in mode listening */
	if(listen(fd, 2) == -1) {
		perror("Error listening");
		error_flag = 1;
	}

	while(1) {
		pthread_mutex_lock(&myMutex);
		while (!accept_flag) {
			pthread_mutex_unlock(&myMutex);
			addr_len = sizeof(struct sockaddr_in);
			if( (new_fd = accept(fd, (struct sockaddr*)&clientaddr, &addr_len)) == -1) {
				perror("Error accept");
			} else {
				char ipClient[32];
				inet_ntop(AF_INET, &(clientaddr.sin_addr), ipClient, sizeof(ipClient));
				printf("Server: conection from %s\n\r", ipClient);
				pthread_mutex_lock(&myMutex);
				accept_flag = 1;
				pthread_mutex_unlock(&myMutex);
			}
		}

		/* Messages reading */
		pthread_mutex_lock(&myMutex);
		while (accept_flag) {
			pthread_mutex_unlock(&myMutex);
			if( (n = read(new_fd, buffer, BUFFER_SIZE)) <= 0 ) {
				printf("Error reading message. Socket closed!\n\r");
				close(new_fd);
				pthread_mutex_lock(&myMutex);
				accept_flag = 0;
				pthread_mutex_unlock(&myMutex);
			} else {
				printf("****** Socket Thread ******\n\r");
				printf("Read: %d bytes.\n\r", n);
				printf("Data: %s\n\r", buffer);
				serial_send(buffer, strlen(buffer));
			}
		}
	}
}


/* Internal functions */
int set_serveraddr(struct sockaddr_in * serveraddr) {
	bzero( (char*)serveraddr, sizeof(serveraddr) );
	serveraddr->sin_family = AF_INET;
	serveraddr->sin_port = htons(10000);
	//serveraddr->sin_addr.s_addr = inet_addr("127.0.0.1");
	if( inet_pton(AF_INET, "127.0.0.1", &(serveraddr->sin_addr)) <= 0 ) {
		return -1;
	} else {
		return 0;
	}
}

void sigint_handler(int sign) {
	printf("****** Signint handler ******\n\r");
	sign_flag = 1;
}

void sigterm_handler(int sign) {
	printf("****** Signterm handler ******\n\r");
	sign_flag = 1;
}

void block_sign(void) {
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &set, NULL);
}

void unblock_sign(void) {
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}

void end_process(int status) {
	close(new_fd);
	close(fd);
	pthread_cancel(threadSerial);
	pthread_join(threadSerial, NULL);
	pthread_cancel(threadSocket);
	pthread_join(threadSocket, NULL);
	exit(status);
}