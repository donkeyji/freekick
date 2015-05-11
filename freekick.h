#ifndef _FREEKICK_H
#define _FREEKICK_H

#include <fk_ev.h>
#include <fk_str.h>
#include <fk_conn.h>

#define FK_PROTO_INVALID 	0
#define FK_PROTO_READ 		1
#define FK_PROTO_WRITE 		2

#define FK_PROTO_VARLEN		0

typedef struct _fk_proto {
	char *name;
	int type;
	unsigned arg_cnt;
	int (*handler) (fk_conn *conn);
} fk_proto;

void fk_svr_conn_add(int fd);
void fk_svr_conn_remove(fk_conn *conn);
fk_conn *fk_svr_conn_get(int fd);
fk_proto *fk_proto_search(fk_str *name);

#endif
