#ifndef _FK_SOCK_H
#define _FK_SOCK_H

#include <stdint.h>

int fk_sock_create_listen(char *addr, uint16_t port);
int fk_sock_accept(int listen_fd);
int fk_sock_set_nonblock(int sock);
int fk_sock_set_reuse_addr(int fd);
int fk_sock_keep_alive(int fd);
int fk_sock_set_linger(int fd);

#endif
