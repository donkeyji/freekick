#ifndef _FK_CLIENT_H_
#define _FK_CLIENT_H_

#include <sys/types.h>

#include <fk_macro.h>
#include <fk_ev.h>
#include <fk_buf.h>
#include <fk_str.h>

typedef struct _fk_conn {
	int fd;
	fk_ioev *read_ev;
	fk_ioev *write_ev;
	int write_added;

	fk_buf *rbuf;
	fk_buf *wbuf;
	time_t last_recv;//time of last data receiving
	fk_tmev *timer;

	fk_str *args[FK_ARG_MAX];
	int		args_len[FK_ARG_MAX];
	int		arg_cnt;//the number of args of the current protocol, original 0;
	int		arg_idx;//the arg_idx arg is being parsing, original 0;
	int 	parse_done;//original 0;

	int db_idx;
} fk_conn;

fk_conn *fk_conn_create(int fd);
void fk_conn_destroy(fk_conn *conn);

#endif
