#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>    // for sockaddr_in
#include <sys/types.h>    // for socket
#include <sys/socket.h>    // for socket
#include <stdio.h>        // for printf
#include <stdlib.h>        // for exit
#include <string.h>        // for bzero
#include <strings.h>

#define HELLO_WORLD_SERVER_PORT    6666
#define BUFFER_SIZE 8
#define FILE_NAME_MAX_SIZE 512

int main(int argc, char **argv)
{
	int fd;
	struct sockaddr_in addr;
	char file_name[FILE_NAME_MAX_SIZE + 1];
	char buff[BUFFER_SIZE] = {
		'a', 'b', 'c', 'd',
		'e', 'f', 'g', 'h'
	};

	/*
	if (argc != 2) {
		printf("Usage: ./%s ServerIPAddress\n",argv[0]);
		exit(1);
	}
	*/

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(8210);
	fd = socket(AF_INET, SOCK_STREAM, 0);
	printf("0000000000000000000\n");
	if ( fd < 0) {
		printf("Create Socket Failed!\n");
		exit(1);
	}

	printf("111111111111111111\n");
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		printf("Can Not Connect To %s!\n", argv[1]);
		exit(1);
	}

	printf("2222222222222222\n");
	bzero(file_name, FILE_NAME_MAX_SIZE + 1);
	printf("Please Input File Name On Server:\t");
	printf("33333333333333333\n");
	scanf("%s", file_name);

	printf("4444444444444444444444\n");
	send(fd, buff, BUFFER_SIZE, 0);
	printf("555555555555555555555\n");

	close(fd);
	printf("6666666666666666666\n");
	return 0;
}
