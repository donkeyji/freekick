#ifndef _FK_SOCK_H_
#define _FK_SOCK_H_

/* c standard library headers */
#include <stdint.h>

#define FK_SOCK_OK      0
#define FK_SOCK_ERR     -1

int fk_sock_create_tcp_listen(char *addr, uint16_t port);
int fk_sock_set_nonblocking(int sock);
int fk_sock_set_reuseaddr(int fd);
int fk_sock_set_keepalive(int fd);
int fk_sock_set_linger(int fd);

#endif
