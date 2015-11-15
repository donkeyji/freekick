#ifndef _FREEKICK_H
#define _FREEKICK_H

/* c standard library headers */
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/* system headers */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>

/* local headers */
#include <fk_def.h>
#include <fk_ev.h>
#include <fk_buf.h>
#include <fk_conn.h>
#include <fk_conf.h>
#include <fk_ev.h>
#include <fk_heap.h>
#include <fk_list.h>
#include <fk_sklist.h>
#include <fk_log.h>
#include <fk_mem.h>
#include <fk_item.h>
#include <fk_sock.h>
#include <fk_str.h>
#include <fk_macro.h>
#include <fk_util.h>
#include <fk_dict.h>
#include <fk_cache.h>
#include <fk_lua.h>

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

typedef struct _fk_zline {
	char *line;
	size_t len;
} fk_zline;

/* ---------------------------------------------------- */

void fk_svr_db_load(fk_str *db_file);

void fk_svr_db_save_background();
int fk_svr_db_save();

int fk_svr_sync_with_master();



/* ---------------------------------------------------- */

/* all the proto handlers */
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
/* ---------------------------------------------------- */

/* extern declerations */
extern fk_server server;
extern fk_elt_op db_dict_eop;
extern fk_node_op db_list_op;
extern fk_sknode_op db_sklist_op;

#endif
