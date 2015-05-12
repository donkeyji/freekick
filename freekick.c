#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>

#include <fk_ev.h>
#include <fk_buf.h>
#include <fk_conn.h>
#include <fk_conf.h>
#include <fk_ev.h>
#include <fk_heap.h>
#include <fk_list.h>
#include <fk_log.h>
#include <fk_mem.h>
#include <fk_item.h>
#include <fk_sock.h>
#include <fk_str.h>
#include <fk_macro.h>
#include <fk_util.h>
#include <fk_dict.h>
#include <fk_cache.h>
#include <freekick.h>

typedef struct _fk_server {
	uint16_t port;
	fk_str *addr;
	int listen_fd;
	unsigned max_conn;/*max connections*/
	unsigned conn_cnt;/*connection count*/
	time_t start_time;
	int stop;
	fk_ioev *listen_ev;
	fk_tmev *svr_timer;
	fk_tmev *svr_timer2;
	fk_conn **conns_tab;
	fk_str *pid_path;
	unsigned dbcnt;
	fk_dict **db;
	fk_str *db_path;
	int save_done;
} fk_server;

/*----------------------------------------------------*/
static void fk_main_init(char *conf_path);
static void fk_main_finalize();
static void fk_main_loop();
static void fk_svr_init();
static int fk_svr_timer_cb(unsigned interval, char type, void *arg);
static int fk_svr_listen_cb(int listen_fd, char type, void *arg);
static void fk_svr_db_load(fk_str *db_path);
static void fk_svr_db_save();
static void fk_signal_reg();
static void fk_sigint(int sig);
static void fk_sigchld(int sig);
static void fk_setrlimit();
static void fk_daemonize();
static void fk_write_pid_file();
static void fk_proto_init();

static void fk_db_dict_val_free(void *elt);
static void fk_db_list_val_free(void *ptr);

/*----------------------------------------------------*/
#define FK_RSP_OK				"OK"
#define FK_RSP_TYPE_ERR			"Type Error"
#define FK_RSP_NIL				(-1)
/*----------------------------------------------------*/

/*all the proto handlers*/
static int fk_cmd_set(fk_conn *conn);
static int fk_cmd_setnx(fk_conn *conn);
static int fk_cmd_get(fk_conn *conn);
static int fk_cmd_del(fk_conn *conn);
static int fk_cmd_flushdb(fk_conn *conn);
static int fk_cmd_flushall(fk_conn *conn);
static int fk_cmd_mset(fk_conn *conn);
static int fk_cmd_mget(fk_conn *conn);
static int fk_cmd_hset(fk_conn *conn);
static int fk_cmd_exists(fk_conn *conn);
static int fk_cmd_hget(fk_conn *conn);
static int fk_cmd_lpush(fk_conn *conn);
static int fk_cmd_rpush(fk_conn *conn);
static int fk_cmd_lpop(fk_conn *conn);
static int fk_cmd_rpop(fk_conn *conn);
/*----------------------------------------------------*/
static int fk_cmd_generic_push(fk_conn *conn, int pos);
static int fk_cmd_generic_pop(fk_conn *conn, int pos);
/*----------------------------------------------------*/

/*global variable*/
static fk_server server;

static fk_elt_op db_dict_eop = {
	NULL,
	fk_str_destroy,
	NULL,
	fk_db_dict_val_free
};

/*for lpush/lpop*/
static fk_node_op db_list_op = {
	NULL,
	fk_db_list_val_free,
	NULL
};

/*all proto to deal*/
static fk_proto protos[] = {
	{"SET", 	FK_PROTO_WRITE, 	3, 					fk_cmd_set	 	},
	{"SETNX", 	FK_PROTO_WRITE, 	3, 					fk_cmd_setnx	},
	{"MSET", 	FK_PROTO_WRITE, 	FK_PROTO_VARLEN, 	fk_cmd_mset	 	},
	{"MGET", 	FK_PROTO_READ, 		FK_PROTO_VARLEN, 	fk_cmd_mget	 	},
	{"GET", 	FK_PROTO_READ, 		2, 					fk_cmd_get	 	},
	{"DEL", 	FK_PROTO_WRITE, 	FK_PROTO_VARLEN, 	fk_cmd_del	 	},
	{"FLUSHDB",	FK_PROTO_WRITE, 	1, 					fk_cmd_flushdb	},
	{"EXISTS",	FK_PROTO_READ, 		2, 					fk_cmd_exists	},
	{"FLUSHALL",FK_PROTO_WRITE, 	1, 					fk_cmd_flushall	},
	{"HSET", 	FK_PROTO_WRITE, 	4, 					fk_cmd_hset	 	},
	{"HGET", 	FK_PROTO_READ, 		3, 					fk_cmd_hget	 	},
	{"LPUSH", 	FK_PROTO_WRITE, 	FK_PROTO_VARLEN, 	fk_cmd_lpush 	},
	{"RPUSH", 	FK_PROTO_WRITE, 	FK_PROTO_VARLEN, 	fk_cmd_rpush 	},
	{"LPOP", 	FK_PROTO_READ, 		2, 					fk_cmd_lpop	 	},
	{"RPOP", 	FK_PROTO_READ, 		2, 					fk_cmd_rpop	 	},
	{NULL, 		FK_PROTO_INVALID, 	0, 					NULL}
};

static fk_dict *pmap = NULL;

void fk_proto_init()
{
	int i;
	char *name;
	fk_str *key;
	void *value;

	pmap = fk_dict_create(NULL);
	if (pmap == NULL) {
		exit(EXIT_FAILURE);
	}

	for (i = 0; protos[i].type != FK_PROTO_INVALID; i++) {
		name = protos[i].name;
		value = protos + i;
		key = fk_str_create(name, strlen(name));
		fk_dict_add(pmap, key, value);
	}
#ifdef FK_DEBUG
	fk_dict_print(pmap);
#endif
}

fk_proto *fk_proto_search(fk_str *name)
{
	fk_proto *pto;

	pto = (fk_proto *)fk_dict_get(pmap, name);

	return pto;
}

int fk_cmd_set(fk_conn *conn)
{
	int rt;
	fk_str *key;
	fk_item *value;

	key = fk_conn_arg_get(conn, 1);
	value = fk_item_create(FK_ITEM_STR, fk_conn_arg_get(conn, 2));

	rt = fk_dict_replace(server.db[conn->db_idx], key, value);
	fk_conn_arg_consume(conn, 2);
	if (rt == 0) {
		fk_conn_arg_consume(conn, 1);
	}

	rt = fk_conn_status_rsp_add(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	if (rt < 0) {
		return -1;
	}

	return 0;
}

int fk_cmd_setnx(fk_conn *conn)
{
	int rt;
	fk_str *key;
	fk_item *value;

	key = fk_conn_arg_get(conn, 1);
	value = fk_dict_get(server.db[conn->db_idx], key);
	if (value != NULL) {
		rt = fk_conn_int_rsp_add(conn, 0);
		if (rt < 0) {
			return -1;
		}
		return 0;
	}

	value = fk_item_create(FK_ITEM_STR, fk_conn_arg_get(conn, 2));
	fk_dict_add(server.db[conn->db_idx], key, value);
	fk_conn_arg_consume(conn, 1);
	fk_conn_arg_consume(conn, 2);

	rt = fk_conn_int_rsp_add(conn, 1);
	if (rt < 0) {
		return -1;
	}
	return 0;
}

int fk_cmd_mset(fk_conn *conn)
{
	int rt, i;
	fk_str *key;
	fk_item *value;

	for (i = 1; i < conn->arg_cnt; i += 2) {
		key = fk_conn_arg_get(conn, i);
		value = fk_item_create(FK_ITEM_STR, fk_conn_arg_get(conn, i + 1));
		rt = fk_dict_replace(server.db[conn->db_idx], key, value);
		fk_conn_arg_consume(conn, i + 1);
		if (rt == 0) {
			fk_conn_arg_consume(conn, i);
		}
	}
	rt = fk_conn_status_rsp_add(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	if (rt < 0) {
		return -1;
	}
	return 0;
}

int fk_cmd_mget(fk_conn *conn)
{
	int rt, i;
	fk_item *itm;
	fk_str *key, *ss;

	rt = fk_conn_mbulk_rsp_add(conn, conn->arg_cnt - 1);
	if (rt < 0) {
		return -1;
	}
	for (i = 1; i < conn->arg_cnt; i++) {
		key = fk_conn_arg_get(conn, i);
		itm = fk_dict_get(server.db[conn->db_idx], key);
		if (itm == NULL) {
			rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
			if (rt < 0) {
				return -1;
			}
		} else {
			if (fk_item_type(itm) != FK_ITEM_STR) {
				rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
				if (rt < 0) {
					return -1;
				}
			} else {
				ss = (fk_str *)fk_item_raw(itm);
				rt = fk_conn_bulk_rsp_add(conn, fk_str_len(ss) - 1);
				if (rt < 0) {
					return -1;
				}
				rt = fk_conn_content_rsp_add(conn, fk_str_raw(ss), fk_str_len(ss) - 1);
				if (rt < 0) {
					return -1;
				}
			}
		}
	}
	return 0;
}

int fk_cmd_get(fk_conn *conn)
{
	int rt;
	fk_item *itm;
	fk_str *key, *value;

	key = fk_conn_arg_get(conn, 1);
	itm = fk_dict_get(server.db[conn->db_idx], key);
	if (itm == NULL) {
		rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
		if (rt < 0) {
			return -1;
		}
		return 0;
	} 

	if (fk_item_type(itm) != FK_ITEM_STR) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt < 0) {
			return -1;
		}
		return 0;
	} 

	value = (fk_str *)fk_item_raw(itm);
	rt = fk_conn_bulk_rsp_add(conn, fk_str_len(value) - 1);
	if (rt < 0) {
		return -1;
	}
	rt = fk_conn_content_rsp_add(conn, fk_str_raw(value), fk_str_len(value) - 1);
	if (rt < 0) {
		return -1;
	}
	return 0;
}

int fk_cmd_del(fk_conn *conn)
{
	fk_str *key;
	int deleted, rt, i;

	deleted = 0;

	for (i = 1; i < conn->arg_cnt; i++) {
		key = fk_conn_arg_get(conn, i);
		rt = fk_dict_remove(server.db[conn->db_idx], key);
		if (rt == 0) {
			deleted++;
		}
	}

	rt = fk_conn_int_rsp_add(conn, deleted);
	if (rt < 0) {
		return -1;
	}

	return 0;
}

int fk_cmd_flushdb(fk_conn *conn)
{
	int rt;

	fk_dict_empty(server.db[conn->db_idx]);
	rt = fk_conn_status_rsp_add(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	if (rt < 0) {
		return -1;
	}
	return 0;
}

int fk_cmd_flushall(fk_conn *conn)
{
	int rt;
	unsigned i;

	for (i = 0; i < server.dbcnt; i++) {
		fk_dict_empty(server.db[i]);
	}
	rt = fk_conn_status_rsp_add(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	if (rt < 0) {
		return -1;
	}

	return 0;
}

int fk_cmd_exists(fk_conn *conn)
{
	int rt, n;
	fk_str *key;
	fk_item *itm;

	key = fk_conn_arg_get(conn, 1);
	itm = fk_dict_get(server.db[conn->db_idx], key);
	n = 0;
	if (itm != NULL) {
		n = 1;
	}
	rt = fk_conn_int_rsp_add(conn, n);
	if (rt < 0) {
		return -1;
	}

	return 0;
}

int fk_cmd_hset(fk_conn *conn)
{
	int rt;
	fk_dict *dct;
	fk_str *hkey, *key;
	fk_item *hitm, *itm;

	hkey = fk_conn_arg_get(conn, 1);
	key = fk_conn_arg_get(conn, 2);
	hitm = fk_dict_get(server.db[conn->db_idx], hkey);
	if (hitm == NULL) {
		dct = fk_dict_create(&db_dict_eop);
		itm = fk_item_create(FK_ITEM_STR, fk_conn_arg_get(conn, 3));
		fk_dict_add(dct, key, itm);
		hitm = fk_item_create(FK_ITEM_DICT, dct);
		fk_dict_add(server.db[conn->db_idx], hkey, hitm);
		/*consume all the args, except args[0]*/
		fk_conn_arg_consume(conn, 1);
		fk_conn_arg_consume(conn, 2);
		fk_conn_arg_consume(conn, 3);
		rt = fk_conn_int_rsp_add(conn, 1);
		if (rt < 0) {
			return -1;
		}
		return 0;
	}

	if (fk_item_type(hitm) == FK_ITEM_DICT) {
		dct = (fk_dict *)fk_item_raw(hitm);
		itm = fk_item_create(FK_ITEM_STR, fk_conn_arg_get(conn, 3));
		fk_dict_add(dct, key, itm);
		fk_conn_arg_consume(conn, 2);
		fk_conn_arg_consume(conn, 3);
		rt = fk_conn_int_rsp_add(conn, 1);
		if (rt < 0) {
			return -1;
		}
		return 0;
	}

	rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
	if (rt < 0) {
		return -1;
	}
	return 0;
}

int fk_cmd_hget(fk_conn *conn)
{
	int rt;
	fk_dict *dct;
	fk_item *hitm, *itm;
	fk_str *hkey, *key, *value;

	hkey = fk_conn_arg_get(conn, 1);
	key = fk_conn_arg_get(conn, 2);
	hitm = fk_dict_get(server.db[conn->db_idx], hkey);
	if (hitm == NULL) {
		rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
		if (rt < 0) {
			return -1;
		}
		return 0;
	} 

	if (fk_item_type(hitm) != FK_ITEM_DICT) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt < 0) {
			return -1;
		}
		return 0;
	} 

	dct = (fk_dict *)fk_item_raw(hitm);
	itm = fk_dict_get(dct, key);
	if (itm == NULL) {
		rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
		if (rt < 0) {
			return -1;
		}
		return 0;
	} 

	if (itm->type != FK_ITEM_STR) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt < 0) {
			return -1;
		}
		return 0;
	}

	value = (fk_str *)fk_item_raw(itm);
	rt = fk_conn_bulk_rsp_add(conn, fk_str_len(value) - 1);
	if (rt < 0) {
		return -1;
	}
	rt = fk_conn_content_rsp_add(conn, fk_str_raw(value), fk_str_len(value) - 1);
	if (rt < 0) {
		return -1;
	}

	return 0;
}

int fk_cmd_generic_push(fk_conn *conn, int pos)
{
	int rt, i;
	fk_str *key;
	fk_list *lst;
	fk_item *lst_itm, *str_itm;

	key = fk_conn_arg_get(conn, 1);
	lst_itm = fk_dict_get(server.db[conn->db_idx], key);
	if (lst_itm == NULL) {
		lst = fk_list_create(&db_list_op);
		for (i = 2; i < conn->arg_cnt; i++) {
			str_itm = fk_item_create(FK_ITEM_STR, fk_conn_arg_get(conn, i));
			if (pos == 0) {/*lpush*/
				fk_list_head_insert(lst, str_itm);
			} else {/*rpush*/
				fk_list_tail_insert(lst, str_itm);
			}
			fk_conn_arg_consume(conn, i);
		}
		lst_itm = fk_item_create(FK_ITEM_LIST, lst);
		fk_dict_add(server.db[conn->db_idx], key, lst_itm);
		fk_conn_arg_consume(conn, 1);
		rt = fk_conn_int_rsp_add(conn, conn->arg_cnt - 2);
		if (rt < 0) {
			return -1;
		}

		return 0;
	}

	if (fk_item_type(lst_itm) != FK_ITEM_LIST) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt < 0) {
			return -1;
		}
		return 0;
	}

	lst = fk_item_raw(lst_itm);
	for (i = 2; i < conn->arg_cnt; i++) {
		str_itm = fk_item_create(FK_ITEM_STR, fk_conn_arg_get(conn, i));
		if (pos == 0) {/*lpush*/
			fk_list_head_insert(lst, str_itm);
		} else {/*rpush*/
			fk_list_tail_insert(lst, str_itm);
		}
		fk_conn_arg_consume(conn, i);
	}
	rt = fk_conn_int_rsp_add(conn, conn->arg_cnt - 2);
	if (rt < 0) {
		return -1;
	}
	return 0;
}

int fk_cmd_lpush(fk_conn *conn)
{
	return fk_cmd_generic_push(conn, 0);
}

int fk_cmd_rpush(fk_conn *conn)
{
	return fk_cmd_generic_push(conn, 1);
}

int fk_cmd_generic_pop(fk_conn *conn, int pos)
{
	int rt;
	fk_node *nd_itm;
	fk_list *lst;
	fk_str *key, *ss;
	fk_item *lst_itm, *itm;

	key = fk_conn_arg_get(conn, 1);
	lst_itm = fk_dict_get(server.db[conn->db_idx], key);
	if (lst_itm == NULL) {
		rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
		if (rt < 0) {
			return -1;
		}
		return 0;
	}

	if (fk_item_type(lst_itm) != FK_ITEM_LIST) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt < 0) {
			return -1;
		}
		return 0;
	}

	lst = fk_item_raw(lst_itm);
	if (fk_list_len(lst) == 0) {
		rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
		if (rt < 0) {
			return -1;
		}
		return 0;
	}

	if (pos == 0) {
		nd_itm = fk_list_head(lst);
	} else {
		nd_itm = fk_list_tail(lst);
	}
	itm = (fk_item *)fk_node_raw(nd_itm);
	ss = fk_item_raw(itm);
	rt = fk_conn_bulk_rsp_add(conn, fk_str_len(ss) - 1);
	if (rt < 0) {
		return -1;
	}
	rt = fk_conn_content_rsp_add(conn, fk_str_raw(ss), fk_str_len(ss) - 1);
	if (rt < 0) {
		return -1;
	}
	fk_list_any_remove(lst, nd_itm);

	return 0;
}

int fk_cmd_lpop(fk_conn *conn)
{
	return fk_cmd_generic_pop(conn, 0);
}

int fk_cmd_rpop(fk_conn *conn)
{
	return fk_cmd_generic_pop(conn, 1);
}

/*--------------------------------------------*/
void fk_db_dict_val_free(void *val)
{
	fk_item *itm;
	itm = (fk_item *)val;
	fk_item_destroy(itm);
}

/*for lpush/lpop*/
void fk_db_list_val_free(void *ptr)
{
	fk_item *itm;

	itm = (fk_item *)ptr;

	fk_item_destroy(itm);
}

/*server interface*/
void fk_svr_conn_add(int fd)
{
	fk_conn *conn;

	conn = fk_conn_create(fd);
	server.conns_tab[conn->fd] = conn;
	server.conn_cnt += 1;
}

/*call when conn disconnect*/
void fk_svr_conn_remove(fk_conn *conn)
{
	server.conns_tab[conn->fd] = NULL;
	server.conn_cnt -= 1;
	fk_conn_destroy(conn);
}

fk_conn *fk_svr_conn_get(int fd)
{
	return server.conns_tab[fd];
}

int fk_svr_listen_cb(int listen_fd, char type, void *arg)
{
	int fd;

	fd = fk_sock_accept(listen_fd);
	/*anything wrong by closing the fd directly?*/
	if (server.conn_cnt == server.max_conn) {
		fk_log_warn("beyond max connections\n");
		close(fd);
		return 0;
	}
	fk_svr_conn_add(fd);
#ifdef FK_DEBUG
	fk_log_debug("conn_cnt: %d, max_conn: %d\n", server.conn_cnt, server.max_conn);
#endif
	/*why redis do like below?*/
	//if (server.conn_cnt > server.max_conn) {
		//fk_log_info("beyond max connections\n");
		//fk_svr_conn_remove(fk_svr_conn_get(fd));
		//return 0;
	//}
	fk_log_info("new connection fd: %d\n", fd);
	return 0;
}

int fk_svr_timer_cb(unsigned interval, char type, void *arg)
{
	fk_log_info("[timer 1]conn cnt: %d\n", server.conn_cnt);
	fk_log_info("[timer 1]dbdict size: %d, used: %d, limit: %d\n", server.db[0]->size, server.db[0]->used, server.db[0]->limit);

	fk_svr_db_save();
	return 0;
}

#ifdef FK_DEBUG
int fk_svr_timer_cb2(unsigned interval, char type, void *arg)
{
	fk_tmev *tmev;

	tmev = server.svr_timer2;

	fk_log_info("[timer 2]\n");
	fk_ev_tmev_remove(tmev);

	return -1;/*do not cycle once more*/
}
#endif

/*
static void fk_daemon_run_old()
{
    if (setting.daemon == 1) {
        if (daemon(1, 0) < 0) {
			exit(EXIT_FAILURE);
        }
    }
}
*/

void fk_daemonize()
{
	int  fd;

	if (setting.daemon == 0) {
		return;
	}

	switch (fork()) {
	case -1:
		fprintf(stderr, "fork: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	case 0:/*child continue*/
		break;
	default:/*parent exit*/
		fprintf(stdout, "parent exit, to run as a daemon\n");
		exit(EXIT_SUCCESS);
	}

	if (setsid() == -1) {/*create a new session*/
		fprintf(stderr, "setsid: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	umask(0);

	fd = open("/dev/null", O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "open /dev/null: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (dup2(fd, STDIN_FILENO) == -1) {/*close STDIN*/
		fprintf(stderr, "dup2: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (dup2(fd, STDOUT_FILENO) == -1) {/*close STDOUT*/
		fprintf(stderr, "dup2: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (dup2(fd, STDERR_FILENO) == -1) {/*close STDERR*/
		fprintf(stderr, "dup2: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (fd > STDERR_FILENO) {
		if (close(fd) == -1) {
			fprintf(stderr, "close: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
}

void fk_write_pid_file()
{
	pid_t pid;
	FILE *pid_file;

	if (setting.daemon == 0) {
		return;
	}

	pid_file = fopen(fk_str_raw(setting.pid_path), "w+");
	pid = getpid();
#ifdef FK_DEBUG
	fk_log_debug("pid: %d, pid file: %s\n", pid, fk_str_raw(setting.pid_path));
#endif
	fprintf(pid_file, "%d\n", pid);
	fclose(pid_file);
}

void fk_svr_init()
{
	unsigned i;

	/*copy setting from conf to server*/
	server.port = setting.port;
	server.max_conn = setting.max_conn;
	server.dbcnt = setting.dbcnt;
	server.db_path = fk_str_clone(setting.db_path);
	server.save_done = 1;
	server.addr = fk_str_clone(setting.addr);

	/*create global environment*/
	server.start_time = time(NULL);
	server.stop = 0;
	server.conn_cnt = 0;
	server.conns_tab = (fk_conn **)fk_mem_alloc(sizeof(fk_conn *) * fk_util_conns_to_files(setting.max_conn));
	server.listen_fd = fk_sock_create_listen(fk_str_raw(server.addr), server.port);
	if (server.listen_fd < 0) {
		fk_log_error("server listen socket creating failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
#ifdef FK_DEBUG
	fk_log_debug("listen fd: %d\n", server.listen_fd);
#endif
	server.listen_ev = fk_ioev_create(server.listen_fd, FK_IOEV_READ, NULL, fk_svr_listen_cb);
	fk_ev_ioev_add(server.listen_ev);

	server.svr_timer = fk_tmev_create(3000, FK_TMEV_CYCLE, NULL, fk_svr_timer_cb);
	fk_ev_tmev_add(server.svr_timer);
#ifdef FK_DEBUG
	server.svr_timer2 = fk_tmev_create(4000, FK_TMEV_CYCLE, NULL, fk_svr_timer_cb2);
	fk_ev_tmev_add(server.svr_timer2);
#endif

	server.db = (fk_dict **)fk_mem_alloc(sizeof(fk_dict *) * server.dbcnt);
	for (i = 0; i < server.dbcnt; i++) {
		server.db[i] = fk_dict_create(&db_dict_eop);
	}
	/*load db from file*/
	fk_svr_db_load(server.db_path);
}

void fk_setrlimit()
{
	int rt;
	pid_t euid;
	struct rlimit lmt;
	unsigned max_files;

	max_files = fk_util_conns_to_files(setting.max_conn);
	rt = getrlimit(RLIMIT_NOFILE, &lmt);
	if (rt < 0) {
		fk_log_error("getrlimit: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	fk_log_info("original file number limit: rlim_cur = %llu, rlim_max = %llu\n", lmt.rlim_cur, lmt.rlim_max);

	if (max_files > lmt.rlim_max) {
		euid = geteuid();
		if (euid == 0) {/*root*/
#ifdef FK_DEBUG
			fk_log_debug("running as root\n");
#endif
			lmt.rlim_max = max_files;
			lmt.rlim_cur = max_files;
			rt = setrlimit(RLIMIT_NOFILE, &lmt);
			if (rt < 0) {
				fk_log_error("setrlimit: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			fk_log_info("new file number limit: rlim_cur = %llu, rlim_max = %llu\n", lmt.rlim_cur, lmt.rlim_max);
		} else {/*non-root*/
#ifdef FK_DEBUG
			fk_log_debug("running as non-root\n");
#endif
			max_files = lmt.rlim_max;/*open as many as possible files*/
			lmt.rlim_cur = lmt.rlim_max;
			rt = setrlimit(RLIMIT_NOFILE, &lmt);
			if (rt < 0) {
				fk_log_error("setrlimit: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			/*set current limit to the original setting*/
			setting.max_conn = fk_util_files_to_conns(max_files);
			fk_log_error("change the setting.max_conn according the current file number limit: %d\n", setting.max_conn);
		}
	}
}

void fk_sigint(int sig)
{
	fk_log_info("to exit by sigint\n");
	server.stop = 1;
}

void fk_sigchld(int sig)
{
	int st;
	pid_t pid;

	pid = wait(&st);
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	if (st == 0) {
		server.save_done = 1;
	}
	fk_log_debug("save db done\n");
}

void fk_signal_reg()
{
	int rt;
	struct sigaction sa;

	sa.sa_handler = fk_sigint;
	sa.sa_flags = 0;
	rt = sigemptyset(&sa.sa_mask);
	if (rt < 0) {
	}
	rt = sigaction(SIGINT, &sa, 0);
	if (rt < 0) {
	}

	sa.sa_handler = fk_sigchld;
	sa.sa_flags = 0;
	rt = sigemptyset(&sa.sa_mask);
	if (rt < 0) {
	}
	rt = sigaction(SIGCHLD, &sa, 0);
	if (rt < 0) {
	}
}

int fk_svr_db_save_exec()
{
	return 0;
}

void fk_svr_db_save()
{
	int rt;

	if (server.save_done != 1) {
		return;
	}

	fk_log_debug("to save db\n");
	rt = fork();
	if (rt < 0) {
		fk_log_error("fork: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	} else if (rt > 0) {
		server.save_done = 0;
		return;
	} else {
		sleep(6);
		rt = fk_svr_db_save_exec();
		exit(EXIT_SUCCESS);
	}
}

void fk_svr_db_load(fk_str *db_path)
{
	return;
}

void fk_main_init(char *conf_path)
{
	/*the first to init*/
	fk_conf_init(conf_path);

	/*could not use fk_log_xxx in fk_daemonize()*/
	fk_daemonize();

	/* the second to init, so that all the 
	 * ther module can call fk_log_xxx() */
	fk_log_init();

	fk_setrlimit();

	fk_write_pid_file();

	fk_signal_reg();

	fk_cache_init();

	fk_proto_init();

	fk_ev_init();

	fk_svr_init();
}

void fk_main_loop()
{
	while (!server.stop) {
		fk_ev_dispatch();
	}
}

void fk_main_finalize()
{
	/*to do: free resource*/
	while (server.save_done == 0) {
		sleep(1);
	}
	/*save db once more*/
	fk_svr_db_save_exec();
}

int main(int argc, char **argv)
{
	char *conf_path;

	if (argc > 2) {
		printf("illegal argument!!!\n");
		printf("usage:\n");
		printf("1: no config specifed\n");
		printf("\tfreekick\n");
		printf("2: specify a config file\n");
		printf("\tfreekick /path/freekick.conf\n");
		exit(EXIT_FAILURE);
	}

	conf_path = NULL;
	if (argc == 2) {
		conf_path = argv[1];
	}
	if (conf_path == NULL) {
		printf("no config file specified, using the default setting\n");
	}

	fk_main_init(conf_path);

	fk_main_loop();

	fk_main_finalize();

	return 0;
}
