#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>//must include this
#include <arpa/inet.h>

#include <fk_sock.h>

static int fk_sock_set_nonblock(int sock);

int fk_sock_create_listen(char *addr, uint16_t port)
{
	int rt, opt, listen_sock;
	struct sockaddr_in saddr;
    struct linger ling = {0, 0};

	opt = 1;
	listen_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_sock == -1) {
		return -1;
	}

	if (fk_sock_set_nonblock(listen_sock) == -1) {
		return -1;
	}

	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(opt));
	setsockopt(listen_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&opt, sizeof(opt));
	setsockopt(listen_sock, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));

	bzero(&saddr, sizeof(struct sockaddr_in));
	saddr.sin_port = htons(port);//uint16, defined in netinet/in.h
	saddr.sin_family = AF_INET;//uint8, defined in sys/socket.h
	//saddr.sin_addr.s_addr = htons(INADDR_ANY);//uint32, defined in netinet/in.h
	saddr.sin_addr.s_addr = inet_addr(addr);//uint32, defined in netinet/in.h

	rt = bind(listen_sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr));
	if (rt < 0) {
		return -1;
	}

	rt = listen(listen_sock, 5);
	if (rt < 0) {
		return -1;
	}

	return listen_sock;
}

int fk_sock_accept(int listen_fd)
{
	int fd;
	fd = accept(listen_fd, NULL, NULL);
	if (fd < 0) {
		return -1;
	}
	fk_sock_set_nonblock(fd);
	return fd;
}

int fk_sock_set_nonblock(int sock)
{
	int rt;
	rt = fcntl(sock, F_SETFL, O_NONBLOCK);
	if (rt < 0) {
		return -1;
	}

	return 0;
}
