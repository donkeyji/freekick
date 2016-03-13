#ifndef _FK_SVR_H_
#define _FK_SVR_H_

/* standard c library headers */
#include <stdint.h>             /* for uint_16 */

/* system headers */
#include <sys/types.h>          /* for time_t */

/* local headers */
#include <fk_str.h>
#include <fk_dict.h>
#include <fk_skiplist.h>
#include <fk_ev.h>
#include <fk_buf.h>
#include <fk_vtr.h>

/* reply sent to the clients */
#define FK_RSP_OK				"OK"
#define FK_RSP_ERR				"ERROR"
#define FK_RSP_TYPE_ERR			"Type Error"
#define FK_RSP_ARGC_ERR			"Argument Number Error"
#define FK_RSP_NIL				(-1)

/* high water */
/*
 * 1. FK_BUF_HIGHWAT & FK_ARG_HIGHWAT & FK_BUF_INIT_LEN should be a power of 2
 * 2. FK_BUF_HIGHWAT should be bigger than FK_ARG_HIGHWAT
 * 3. FK_BUF_INIT_LEN should not be bigger than FK_BUF_HIGHWAT
 */
#define FK_BUF_HIGHWAT				(4 * 1024 * 1024)
#define FK_BUF_INIT_LEN				16
#define FK_ARG_HIGHWAT 				(1024 * 1024)
#define FK_ARG_CNT_HIGHWAT 			128

/* the max length of a argument count line, the max line: *128\r\n */
#define FK_ARG_CNT_LINE_HIGHWAT 	5

/*
 * the max length of a argument length line, the max line: $1048576\r\n
 * 1024 * 1024 = 1048576
 */
#define FK_ARG_LEN_LINE_HIGHWAT		9

#define FK_VTR_INIT_LEN				4

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
typedef struct {
    int           fd;
    uint16_t      type;           /* FK_CONN_REAL | FK_CONN_FAKE */
    fk_ioev_t    *read_ev;
    fk_ioev_t    *write_ev;
    int           write_added;

    fk_buf_t     *rbuf;
    fk_buf_t     *wbuf;
    time_t        last_recv;      /* time of last data receiving */
    fk_tmev_t    *timer;

    fk_vtr_t     *arg_vtr;
    int           arg_parsed;     /* parsed from the head of a protocol, original 0; */
    int           arg_cnt;        /* the number of the arguments which have been parsed, original 0 */
    int           cur_arglen;     /* the argument length of the arg_cnt..TH, original -1 */
    int           parse_done;     /* original 0 */

    int           db_idx;
} fk_conn_t;

typedef struct {
    uint32_t       arch;          /* 32 or 64 */
    int            listen_fd;
    int            conn_cnt;      /* connection count */
    time_t         start_time;
    time_t         last_save;
    uint64_t       timer_cnt;
    fk_ioev_t     *listen_ev;
    fk_tmev_t     *svr_timer;
    fk_tmev_t     *svr_timer2;
    fk_conn_t    **conns_tab;
    uint32_t       dbcnt;
    fk_dict_t    **db;
    pid_t          save_pid;      /* -1: the save child process ended */

    int            last_dbidx;
    int            blog_fd;
} fk_svr_t;

typedef int (*fk_handler_t) (fk_conn_t *conn);

typedef struct {
    char           *name;
    uint16_t        type;
    int             arg_cnt;
    fk_handler_t    handler;
} fk_proto_t;

/* interface of fk_conn_t */
#define FK_CONN_FAKE_FD		-1	/* a fake connection, of which fd is -1 */
fk_conn_t *fk_conn_create(int fd);
void fk_conn_destroy(fk_conn_t *conn);
void fk_conn_free_args(fk_conn_t *conn);
#define fk_conn_set_type(conn, t)		((conn)->type = (t))
#define fk_conn_get_type(conn)			((conn)->type)
int fk_conn_add_status_rsp(fk_conn_t *conn, char *stat, size_t stat_len);
int fk_conn_add_error_rsp(fk_conn_t *conn, char *error, size_t error_len);
int fk_conn_add_content_rsp(fk_conn_t *conn, char *content, size_t content_len);
int fk_conn_add_int_rsp(fk_conn_t *conn, int num);
int fk_conn_add_bulk_rsp(fk_conn_t *conn, int bulk_len);
int fk_conn_add_mbulk_rsp(fk_conn_t *conn, int bulk_cnt);

#define fk_conn_set_arg(conn, idx, a)	fk_vtr_set((conn->arg_vtr), (idx), (a))
#define fk_conn_get_arg(conn, idx)	fk_vtr_get((conn)->arg_vtr, (idx))

/* interface of fk_svr_t */
void fk_svr_init(void);
void fk_svr_exit(void);
void fk_svr_add_conn(int fd);
void fk_svr_remove_conn(fk_conn_t *conn);

void fk_svr_signal_exit_handler(int sig);
void fk_svr_signal_child_handler(int sig);

/* related to dump/restore */
void fk_fkdb_init(void);
void fk_fkdb_load(fk_str_t *db_path);
void fk_fkdb_bgsave(void);
int fk_fkdb_save(void);

/* related to replication */
int fk_svr_sync_with_master(void);

/* related to lua scripting */
void fk_lua_init(void);

/* related to binary log */
void fk_blog_init(void);
void fk_blog_load(fk_str_t *blog_path);
void fk_blog_append(int argc, fk_vtr_t *arg_vtr, fk_proto_t *pto);

/* related to protocol */
void fk_proto_init(void);
fk_proto_t *fk_proto_search(fk_str_t *name);

/*
 * all the protocol handlers which are splited
 * into different fk_svr_xxx.c files
 */
int fk_cmd_set(fk_conn_t *conn);
int fk_cmd_setnx(fk_conn_t *conn);
int fk_cmd_get(fk_conn_t *conn);
int fk_cmd_del(fk_conn_t *conn);
int fk_cmd_flushdb(fk_conn_t *conn);
int fk_cmd_flushall(fk_conn_t *conn);
int fk_cmd_mset(fk_conn_t *conn);
int fk_cmd_mget(fk_conn_t *conn);
int fk_cmd_hset(fk_conn_t *conn);
int fk_cmd_hget(fk_conn_t *conn);
int fk_cmd_exists(fk_conn_t *conn);
int fk_cmd_lpush(fk_conn_t *conn);
int fk_cmd_rpush(fk_conn_t *conn);
int fk_cmd_lpop(fk_conn_t *conn);
int fk_cmd_rpop(fk_conn_t *conn);
int fk_cmd_llen(fk_conn_t *conn);
int fk_cmd_save(fk_conn_t *conn);
int fk_cmd_select(fk_conn_t *conn);
int fk_cmd_eval(fk_conn_t *conn);
int fk_cmd_zadd(fk_conn_t *conn);

/* extern declerations of global variables */

extern fk_svr_t server;         /* this "server" is visited in different .c files */
extern fk_elt_op_t db_dict_eop;
extern fk_node_op_t db_list_op;
extern fk_skipnode_op_t db_skiplist_op;

#endif
