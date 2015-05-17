#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>/*must include this*/
#include <arpa/inet.h>

#include <fk_sock.h>


int fk_sock_create_listen(char *addr, uint16_t port)
{
	int rt, opt, listen_sock;
	/* struct sockaddr_in has the same size as struct sockaddr */
	struct sockaddr_in saddr;

	opt = 1;
	listen_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_sock == -1) {
		return -1;
	}

	if (fk_sock_set_nonblock(listen_sock) == -1) {
		return -1;
	}

	rt = fk_sock_set_reuse_addr(listen_sock);
	if (rt < 0) {
		return -1;
	}

	bzero(&saddr, sizeof(struct sockaddr_in));
	saddr.sin_port = htons(port);/*uint16, defined in netinet/in.h*/
	saddr.sin_family = AF_INET;/*uint8, defined in sys/socket.h*/
	//saddr.sin_addr.s_addr = htons(INADDR_ANY);//uint32, defined in netinet/in.h
	saddr.sin_addr.s_addr = inet_addr(addr);/*uint32, defined in netinet/in.h*/

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
	int fd, rt;

	fd = accept(listen_fd, NULL, NULL);
	if (fd < 0) {
		return -1;
	}

	rt = fk_sock_set_nonblock(fd);
	if (rt < 0) {
		return -1;
	}
	rt = fk_sock_keep_alive(fd);
	if (rt < 0) {
		return -1;
	}

	return fd;
}

int fk_sock_set_nonblock(int fd)
{
	int rt;
	rt = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (rt < 0) {
		return -1;
	}

	return 0;
}

int fk_sock_keep_alive(int fd)
{
	int opt, rt;
	
	opt = 1;
	rt = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&opt, sizeof(opt));
	if (rt < 0) {
		return -1;
	}
	return 0;
}

int fk_sock_set_reuse_addr(int fd)
{
	int opt, rt;
	
	opt = 1;
	rt = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(opt));
	if (rt < 0) {
		return -1;
	}
	return 0;
}

int fk_sock_set_linger(int fd)
{
	int rt;
    struct linger ling = {0, 0};

	rt = setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));
	if (rt < 0) {
		return -1;
	}
	return 0;
}
