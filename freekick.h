#ifndef _FREEKICK_H_
#define _FREEKICK_H_

/* standard c library headers */
#include <stdint.h>/* for uint_16 */

/* system headers */
#include <sys/types.h>/* for time_t */

/* local headers */
#include <fk_macro.h>
#include <fk_str.h>
#include <fk_dict.h>
#include <fk_sklist.h>
#include <fk_ev.h>
#include <fk_buf.h>
#include <fk_vtr.h>

/* status code definition copied from nginx */
#define FK_OK			 0
#define FK_ERR			-1
#define FK_AGAIN    	-2
#define FK_BUSY     	-3
#define FK_DONE     	-4
#define FK_DECLINED   	-5
#define FK_ABORT      	-6

/* protocol types definitions */
#define FK_PROTO_INVALID 	0
#define FK_PROTO_READ 		1
#define FK_PROTO_WRITE 		2
#define FK_PROTO_SCRIPT 	3

#define FK_PROTO_VARLEN		0

typedef struct _fk_conn {
	int fd;
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

typedef struct _fk_server {
	uint16_t port;
	fk_str *addr;
	int listen_fd;
	unsigned max_conn;/* max connections */
	unsigned conn_cnt;/* connection count */
	time_t start_time;
	time_t last_save;
	int stop;
	unsigned long long timer_cnt;
	fk_ioev *listen_ev;
	fk_tmev *svr_timer;
	fk_tmev *svr_timer2;
	fk_conn **conns_tab;
	fk_str *pid_path;
	unsigned dbcnt;
	fk_dict **db;
	fk_str *db_file;
	int save_done;/* maybe it's a redundancy */
	pid_t save_pid;/* -1: the save child process ended */
} fk_server;

typedef struct _fk_proto {
	char *name;
	int type;
	unsigned arg_cnt;
	int (*handler) (fk_conn *conn);
} fk_proto;

/* ---------------------------------------------------- */
/* related to dump */
void fk_svr_db_load(fk_str *db_file);
void fk_svr_db_save_background();
int fk_svr_db_save();

/* related to replication */
int fk_svr_sync_with_master();

/* related to lua scripting */
void fk_lua_init();


/* related to protocol */
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

extern fk_server server;/* this "server" is visited in different .c files */

extern fk_elt_op db_dict_eop;
extern fk_node_op db_list_op;
extern fk_sknode_op db_sklist_op;

/* interface for fk_conn */
fk_conn *fk_conn_create(int fd);
void fk_conn_destroy(fk_conn *conn);

#define fk_conn_arg_set(conn, idx, a)	fk_vtr_set((conn->arg_vtr), (idx), (a))

#define fk_conn_arg_get(conn, idx)	fk_vtr_get((conn)->arg_vtr, (idx))

#define fk_conn_arglen_set(conn, idx, l)  fk_vtr_set((conn)->len_vtr, (idx), (l))

#define fk_conn_arglen_get(conn, idx)	fk_vtr_get((conn->len_vtr), (idx))

int fk_conn_status_rsp_add(fk_conn *conn, char *stat, size_t stat_len);
int fk_conn_error_rsp_add(fk_conn *conn, char *error, size_t error_len);
int fk_conn_content_rsp_add(fk_conn *conn, char *content, size_t content_len);
int fk_conn_int_rsp_add(fk_conn *conn, int num);
int fk_conn_bulk_rsp_add(fk_conn *conn, int bulk_len);
int fk_conn_mbulk_rsp_add(fk_conn *conn, int bulk_cnt);

#endif
