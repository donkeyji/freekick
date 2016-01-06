#ifndef _FK_SVR_H_
#define _FK_SVR_H_

/* standard c library headers */
#include <stdint.h>/* for uint_16 */

/* system headers */
#include <sys/types.h>/* for time_t */

/* local headers */
#include <fk_macro.h>
#include <fk_str.h>
#include <fk_dict.h>
#include <fk_skiplist.h>
#include <fk_ev.h>
#include <fk_buf.h>
#include <fk_vtr.h>

/* status code definition copied from nginx */
#define FK_SVR_OK			 0
#define FK_SVR_ERR			-1
#define FK_SVR_AGAIN    	-2
#define FK_SVR_BUSY     	-3
#define FK_SVR_DONE     	-4
#define FK_SVR_DECLINED   	-5
#define FK_SVR_ABORT      	-6

/* protocol types definitions */
#define FK_PROTO_INVALID 	0
#define FK_PROTO_READ 		1
#define FK_PROTO_WRITE 		2
#define FK_PROTO_SCRIPT 	3

#define FK_PROTO_VARLEN		0

#define FK_CONN_REAL	0
#define FK_CONN_FAKE	1
typedef struct _fk_conn {
	int fd;
	int type;/* FK_CONN_REAL | FK_CONN_FAKE */
	fk_ioev *read_ev;
	fk_ioev *write_ev;
	int write_added;

	fk_buf *rbuf;
	fk_buf *wbuf;
	time_t last_recv;/* time of last data receiving */
	fk_tmev *timer;

	fk_vtr *arg_vtr;
	fk_vtr *len_vtr;
	int arg_cnt;/* the number of arg_vtr of the current protocol, original 0; */
	int arg_idx;/* the arg_idx arg is being parsing, original 0 */
	int idx_flag;/* arg_len or arg */
	int parse_done;/* original 0 */

	int db_idx;
} fk_conn;

typedef struct _fk_svr {
	uint16_t port;
	fk_str *addr;
	int listen_fd;
	unsigned max_conn;/* max connections */
	unsigned conn_cnt;/* connection count */
	time_t start_time;
	time_t last_save;
	unsigned long long timer_cnt;
	fk_ioev *listen_ev;
	fk_tmev *svr_timer;
	fk_tmev *svr_timer2;
	fk_conn **conns_tab;
	fk_str *pid_path;
	unsigned dbcnt;
	fk_dict **db;
	fk_str *db_file;
	pid_t save_pid;/* -1: the save child process ended */
} fk_svr;

typedef struct _fk_proto {
	char *name;
	int type;
	int arg_cnt;
	int (*handler) (fk_conn *conn);
} fk_proto;

/* interface of fk_conn */
#define FK_CONN_FAKE_FD		-1	/* a fake connection, of which fd is -1 */
fk_conn *fk_conn_create(int fd);
void fk_conn_destroy(fk_conn *conn);
void fk_conn_free_args(fk_conn *conn);
#define fk_conn_set_type(conn, t)		((conn)->type = (t))
#define fk_conn_get_type(conn)			((conn)->type)
int fk_conn_add_status_rsp(fk_conn *conn, char *stat, size_t stat_len);
int fk_conn_add_error_rsp(fk_conn *conn, char *error, size_t error_len);
int fk_conn_add_content_rsp(fk_conn *conn, char *content, size_t content_len);
int fk_conn_add_int_rsp(fk_conn *conn, int num);
int fk_conn_add_bulk_rsp(fk_conn *conn, int bulk_len);
int fk_conn_add_mbulk_rsp(fk_conn *conn, int bulk_cnt);

#define fk_conn_set_arg(conn, idx, a)	fk_vtr_set((conn->arg_vtr), (idx), (a))
#define fk_conn_get_arg(conn, idx)	fk_vtr_get((conn)->arg_vtr, (idx))
#define fk_conn_set_arglen(conn, idx, l)  fk_vtr_set((conn)->len_vtr, (idx), (l))
#define fk_conn_get_arglen(conn, idx)	fk_vtr_get((conn->len_vtr), (idx))

/* interface of fk_svr */
void fk_svr_init();
void fk_svr_exit();
void fk_svr_add_conn(int fd);
void fk_svr_remove_conn(fk_conn *conn);

void fk_svr_signal_exit_handler(int sig);
void fk_svr_signal_child_handler(int sig);

/* related to dump/restore */
void fk_fkdb_load(fk_str *db_file);
void fk_fkdb_bgsave();
int fk_fkdb_save();

/* related to replication */
int fk_svr_sync_with_master();

/* related to lua scripting */
void fk_lua_init();

/* related to protocol */
void fk_proto_init();
fk_proto *fk_proto_search(fk_str *name);

/* 
 * all the protocol handlers which are splited 
 * into different fk_svr_xxx.c files
 */
int fk_cmd_set(fk_conn *conn);
int fk_cmd_setnx(fk_conn *conn);
int fk_cmd_get(fk_conn *conn);
int fk_cmd_del(fk_conn *conn);
int fk_cmd_flushdb(fk_conn *conn);
int fk_cmd_flushall(fk_conn *conn);
int fk_cmd_mset(fk_conn *conn);
int fk_cmd_mget(fk_conn *conn);
int fk_cmd_hset(fk_conn *conn);
int fk_cmd_hget(fk_conn *conn);
int fk_cmd_exists(fk_conn *conn);
int fk_cmd_lpush(fk_conn *conn);
int fk_cmd_rpush(fk_conn *conn);
int fk_cmd_lpop(fk_conn *conn);
int fk_cmd_rpop(fk_conn *conn);
int fk_cmd_llen(fk_conn *conn);
int fk_cmd_save(fk_conn *conn);
int fk_cmd_select(fk_conn *conn);
int fk_cmd_eval(fk_conn *conn);
int fk_cmd_zadd(fk_conn *conn);

/* extern declerations of global variables */

extern fk_svr server;/* this "server" is visited in different .c files */
extern fk_elt_op db_dict_eop;
extern fk_node_op db_list_op;
extern fk_skipnode_op db_skiplist_op;

#endif
