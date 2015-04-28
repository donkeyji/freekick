#ifndef _FK_SOCK_H
#define _FK_SOCK_H

#include <stdint.h>

int fk_sock_create_listen(char *addr, uint16_t port);
int fk_sock_accept(int listen_fd);

#endif
