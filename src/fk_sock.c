/* c standard headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* unix headers */
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h> /* must include this */
#include <arpa/inet.h>

/* local headers */
#include <fk_sock.h>


int
fk_sock_create_tcp_listen(char *addr, uint16_t port)
{
    int                 rt, listen_sock;
    /* struct sockaddr_in has the same size as struct sockaddr */
    struct sockaddr_in  saddr;

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) {
        return FK_SOCK_ERR;
    }

    if (fk_sock_set_nonblocking(listen_sock) == FK_SOCK_ERR) {
        return FK_SOCK_ERR;
    }

    rt = fk_sock_set_reuseaddr(listen_sock);
    if (rt == FK_SOCK_ERR) {
        return FK_SOCK_ERR;
    }

    memset(&saddr, 0, sizeof(struct sockaddr_in));
    saddr.sin_port = htons(port); /* uint16, defined in netinet/in.h */
    saddr.sin_family = AF_INET; /* uint8, defined in sys/socket.h */
    //saddr.sin_addr.s_addr = htons(INADDR_ANY); /* uint32, defined in netinet/in.h */
    /*
     * inet_addr() only works for IPv4
     * but inet_pton() works for both IPv4 and IPv6
     * inet_aton should not be used for being obsolete
     */
    saddr.sin_addr.s_addr = inet_addr(addr); /* uint32, defined in netinet/in.h */

    rt = bind(listen_sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr));
    if (rt < 0) {
        return FK_SOCK_ERR;
    }

    rt = listen(listen_sock, 5);
    if (rt < 0) {
        return FK_SOCK_ERR;
    }

    return listen_sock;
}

int
fk_sock_set_nonblocking(int fd)
{
    int  rt;

    /*
     * On linux, this flag can not be inherited by the new fd returned by
     * accept(), since this flag could be altered by fcntl() F_SETFL operation,
     * and these flags includes O_NONBLOCK, O_ASYNC.
     */
    rt = fcntl(fd, F_SETFL, O_NONBLOCK);
    if (rt < 0) {
        return FK_SOCK_ERR;
    }

    return FK_SOCK_OK;
}

int
fk_sock_set_keepalive(int fd)
{
    int  opt, rt;

    opt = 1;
    rt = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&opt, sizeof(opt));
    if (rt < 0) {
        return FK_SOCK_ERR;
    }
    return FK_SOCK_OK;
}

int
fk_sock_set_reuseaddr(int fd)
{
    int  opt, rt;

    opt = 1;
    rt = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(opt));
    if (rt < 0) {
        return FK_SOCK_ERR;
    }
    return FK_SOCK_OK;
}

int
fk_sock_set_linger(int fd)
{
    int            rt;
    struct linger  ling = {0, 0};

    rt = setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));
    if (rt < 0) {
        return FK_SOCK_ERR;
    }
    return FK_SOCK_OK;
}
