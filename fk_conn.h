#ifndef _FK_CLIENT_H_
#define _FK_CLIENT_H_

#include <sys/types.h>

#include <fk_macro.h>
#include <fk_ev.h>
#include <fk_buf.h>
#include <fk_str.h>
#include <fk_vtr.h>

typedef struct _fk_conn {
	int fd;
	fk_ioev *read_ev;
	fk_ioev *write_ev;
	int write_added;

	fk_buf *rbuf;
	fk_buf *wbuf;
	time_t last_recv;//time of last data receiving
	fk_tmev *timer;

	fk_vtr *arg_vtr;
	fk_vtr *len_vtr;
	int		arg_cnt;//the number of arg_vtr of the current protocol, original 0;
	int		arg_idx;//the arg_idx arg is being parsing, original 0;
	int 	arg_idx_type;//arg_len or arg
	int 	parse_done;//original 0;

	int db_idx;
} fk_conn;

fk_conn *fk_conn_create(int fd);
void fk_conn_destroy(fk_conn *conn);

#define fk_conn_arg(conn, idx)	fk_vtr_get((conn)->arg_vtr, (idx))

#define fk_conn_arg_consume(conn, idx)	fk_conn_arg((conn), (idx)) = NULL

#define fk_conn_arg_len(conn, idx)  fk_vtr_get((conn)->len_vtr, (idx))

int fk_conn_rsp_add_status(fk_conn *conn, char *stat, int stat_len);
int fk_conn_rsp_add_error(fk_conn *conn, char *error, int error_len);
int fk_conn_rsp_add_content(fk_conn *conn, char *content, int content_len);
int fk_conn_rsp_add_int(fk_conn *conn, int num);
int fk_conn_rsp_add_bulk(fk_conn *conn, int bulk_len);
int fk_conn_rsp_add_mbulk(fk_conn *conn, int bulk_cnt);

#endif
