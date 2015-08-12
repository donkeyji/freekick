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
#include <fk_lua.h>
#include <freekick.h>

typedef struct _fk_server {
	uint16_t port;
	fk_str *addr;
	int listen_fd;
	unsigned max_conn;/* max connections */
	unsigned conn_cnt;/* connection count */
	time_t start_time;
	int stop;
	fk_ioev *listen_ev;
	fk_tmev *svr_timer;
	fk_tmev *svr_timer2;
	fk_conn **conns_tab;
	fk_str *pid_path;
	unsigned dbcnt;
	fk_dict **db;
	fk_str *db_file;
	int save_done;
} fk_server;

typedef struct _fk_zline {
	char *line;
	size_t len;
} fk_zline;

/* ---------------------------------------------------- */
static void fk_main_init(char *conf_path);
static void fk_main_final();
static void fk_main_cycle();
static void fk_svr_init();
static int fk_svr_timer_cb(unsigned interval, char type, void *arg);
static int fk_svr_listen_cb(int listen_fd, char type, void *arg);

static void fk_svr_db_load(fk_str *db_file);
static int fk_svr_db_restore(FILE *fp, fk_zline *buf);
static int fk_svr_db_str_elt_restore(FILE *fp, fk_dict *db, fk_zline *buf);
static int fk_svr_db_list_elt_restore(FILE *fp, fk_dict *db, fk_zline *buf);
static int fk_svr_db_dict_elt_restore(FILE *fp, fk_dict *db, fk_zline *buf);

static void fk_svr_db_save_background();
static int fk_svr_db_save();
static int fk_svr_db_dump(FILE *fp, unsigned db_idx);
static int fk_svr_db_str_elt_dump(FILE *fp, fk_elt *elt);
static int fk_svr_db_list_elt_dump(FILE *fp, fk_elt *elt);
static int fk_svr_db_dict_elt_dump(FILE *fp, fk_elt *elt);

static int fk_svr_sync_with_master();

static void fk_signal_reg();
static void fk_sigint(int sig);
static void fk_sigchld(int sig);
static void fk_setrlimit();
static void fk_set_pwd();
static void fk_daemonize();
static void fk_write_pid_file();
static void fk_proto_init();

static uint32_t fk_db_dict_key_hash(void *key);
static int fk_db_dict_key_cmp(void *k1, void *k2);
static void *fk_db_dict_key_copy(void *key);
static void fk_db_dict_key_free(void *key);
static void *fk_db_dict_val_copy(void *val);
static void fk_db_dict_val_free(void *elt);

static void *fk_db_list_val_copy(void *ptr);
static void fk_db_list_val_free(void *ptr);

static fk_zline *fk_zline_create(size_t len);
static void fk_zline_adjust(fk_zline *buf, size_t len);
static void fk_zline_destroy(fk_zline *buf);
/* ---------------------------------------------------- */

/* all the proto handlers */
static int fk_cmd_set(fk_conn *conn);
static int fk_cmd_setnx(fk_conn *conn);
static int fk_cmd_get(fk_conn *conn);
static int fk_cmd_del(fk_conn *conn);
static int fk_cmd_flushdb(fk_conn *conn);
static int fk_cmd_flushall(fk_conn *conn);
static int fk_cmd_mset(fk_conn *conn);
static int fk_cmd_mget(fk_conn *conn);
static int fk_cmd_hset(fk_conn *conn);
static int fk_cmd_hget(fk_conn *conn);
static int fk_cmd_exists(fk_conn *conn);
static int fk_cmd_lpush(fk_conn *conn);
static int fk_cmd_rpush(fk_conn *conn);
static int fk_cmd_lpop(fk_conn *conn);
static int fk_cmd_rpop(fk_conn *conn);
static int fk_cmd_llen(fk_conn *conn);
static int fk_cmd_save(fk_conn *conn);
static int fk_cmd_select(fk_conn *conn);
static int fk_cmd_eval(fk_conn *conn);
/* ---------------------------------------------------- */
static int fk_cmd_generic_push(fk_conn *conn, int pos);
static int fk_cmd_generic_pop(fk_conn *conn, int pos);
/* ---------------------------------------------------- */

/* global variable */
static fk_server server;

static fk_elt_op db_dict_eop = {
	fk_db_dict_key_hash,
	fk_db_dict_key_cmp,
	fk_db_dict_key_copy,
	fk_db_dict_key_free,
	fk_db_dict_val_copy,
	fk_db_dict_val_free
};

/* for lpush/lpop */
static fk_node_op db_list_op = {
	fk_db_list_val_copy,
	fk_db_list_val_free,
	NULL
};

/* all proto to deal */
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
	{"LLEN",	FK_PROTO_READ,		2,					fk_cmd_llen		},
	{"SAVE",	FK_PROTO_READ,		1,					fk_cmd_save		},
	{"SELECT",	FK_PROTO_WRITE,		2,					fk_cmd_select	},
	{"EVAL",	FK_PROTO_SCRIPT,	FK_PROTO_VARLEN,	fk_cmd_eval		},
	{NULL, 		FK_PROTO_INVALID, 	0, 					NULL}
};

static fk_dict *pmap = NULL;

uint32_t fk_dict_pto_key_hash(void *key)
{
	fk_str *s;

	s = (fk_str *)key;
	return fk_str_hash(s);
}

int fk_dict_pto_key_cmp(void *k1, void *k2)
{
	fk_str *s1, *s2;

	s1 = (fk_str *)k1;
	s2 = (fk_str *)k2;

	return fk_str_cmp(s1, s2);
}

static fk_elt_op proto_dict_eop = {
	fk_dict_pto_key_hash,
	fk_dict_pto_key_cmp,
	NULL,
	NULL,
	NULL,
	NULL
};

void fk_proto_init()
{
	int i;
	char *name;
	fk_str *key;
	void *value;

	pmap = fk_dict_create(&proto_dict_eop);
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
	fk_elt *elt;
	fk_dict_iter *iter = fk_dict_iter_begin(pmap);
	while ((elt = fk_dict_iter_next(iter)) != NULL) {
		key = (fk_str *)fk_elt_key(elt);
		value = fk_elt_value(elt);
		fprintf(stdout, "proto key: %s\n", fk_str_raw(key));
	}
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
	fk_item *key, *value;

	key = fk_conn_arg_get(conn, 1);
	value = fk_conn_arg_get(conn, 2);

	fk_dict_replace(server.db[conn->db_idx], key, value);

	rt = fk_conn_status_rsp_add(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}

	return FK_OK;
}

int fk_cmd_setnx(fk_conn *conn)
{
	int rt;
	fk_item *key, *value;

	key = fk_conn_arg_get(conn, 1);
	value = fk_dict_get(server.db[conn->db_idx], key);
	if (value != NULL) {
		rt = fk_conn_int_rsp_add(conn, 0);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	}

	value = fk_conn_arg_get(conn, 2);
	fk_dict_add(server.db[conn->db_idx], key, value);

	rt = fk_conn_int_rsp_add(conn, 1);
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}
	return FK_OK;
}

int fk_cmd_mset(fk_conn *conn)
{
	int rt, i;
	fk_item *key, *value;

	for (i = 1; i < conn->arg_cnt; i += 2) {
		key = fk_conn_arg_get(conn, i);
		value = fk_conn_arg_get(conn, i + 1);
		fk_dict_replace(server.db[conn->db_idx], key, value);
	}
	rt = fk_conn_status_rsp_add(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}
	return FK_OK;
}

int fk_cmd_mget(fk_conn *conn)
{
	int rt, i;
	fk_str *ss;
	fk_item *value, *key;

	rt = fk_conn_mbulk_rsp_add(conn, conn->arg_cnt - 1);
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}
	for (i = 1; i < conn->arg_cnt; i++) {
		key = fk_conn_arg_get(conn, i);
		value = fk_dict_get(server.db[conn->db_idx], key);
		if (value == NULL) {
			rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
			if (rt == FK_CONN_ERR) {
				return FK_ERR;
			}
		} else {
			if (fk_item_type(value) != FK_ITEM_STR) {
				rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
				if (rt == FK_CONN_ERR) {
					return FK_ERR;
				}
			} else {
				ss = (fk_str *)fk_item_raw(value);
				rt = fk_conn_bulk_rsp_add(conn, (int)(fk_str_len(ss)));
				if (rt == FK_CONN_ERR) {
					return FK_ERR;
				}
				rt = fk_conn_content_rsp_add(conn, fk_str_raw(ss), fk_str_len(ss));
				if (rt == FK_CONN_ERR) {
					return FK_ERR;
				}
			}
		}
	}
	return FK_OK;
}

int fk_cmd_get(fk_conn *conn)
{
	int rt;
	fk_str *ss;
	fk_item *value, *key;

	key = fk_conn_arg_get(conn, 1);
	value = fk_dict_get(server.db[conn->db_idx], key);
	if (value == NULL) {
		rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	} 

	if (fk_item_type(value) != FK_ITEM_STR) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	} 

	ss = (fk_str *)fk_item_raw(value);
	rt = fk_conn_bulk_rsp_add(conn, (int)(fk_str_len(ss)));
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}
	rt = fk_conn_content_rsp_add(conn, fk_str_raw(ss), fk_str_len(ss));
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}
	return FK_OK;
}

int fk_cmd_del(fk_conn *conn)
{
	fk_item *key;
	int deleted, rt, i;

	deleted = 0;

	for (i = 1; i < conn->arg_cnt; i++) {
		key = fk_conn_arg_get(conn, i);
		rt = fk_dict_remove(server.db[conn->db_idx], key);
		if (rt == FK_DICT_OK) {
			deleted++;
		}
	}

	rt = fk_conn_int_rsp_add(conn, deleted);
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}

	return FK_OK;
}

int fk_cmd_flushdb(fk_conn *conn)
{
	int rt;

	fk_dict_empty(server.db[conn->db_idx]);
	rt = fk_conn_status_rsp_add(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}
	return FK_OK;
}

int fk_cmd_flushall(fk_conn *conn)
{
	int rt;
	unsigned i;

	for (i = 0; i < server.dbcnt; i++) {
		fk_dict_empty(server.db[i]);
	}
	rt = fk_conn_status_rsp_add(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}

	return FK_OK;
}

int fk_cmd_exists(fk_conn *conn)
{
	int rt, n;
	fk_item *key, *value;

	key = fk_conn_arg_get(conn, 1);
	value = fk_dict_get(server.db[conn->db_idx], key);
	n = 0;
	if (value != NULL) {
		n = 1;
	}
	rt = fk_conn_int_rsp_add(conn, n);
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}

	return FK_OK;
}

int fk_cmd_hset(fk_conn *conn)
{
	int rt;
	fk_dict *dct;
	fk_item *hkey, *key;
	fk_item *hitm, *itm;

	hkey = fk_conn_arg_get(conn, 1);
	key = fk_conn_arg_get(conn, 2);
	hitm = fk_dict_get(server.db[conn->db_idx], hkey);
	if (hitm == NULL) {
		dct = fk_dict_create(&db_dict_eop);
		itm = fk_conn_arg_get(conn, 3);
		fk_dict_add(dct, key, itm);
		hitm = fk_item_create(FK_ITEM_DICT, dct);/* do not increase ref for local var */
		fk_dict_add(server.db[conn->db_idx], hkey, hitm);
		rt = fk_conn_int_rsp_add(conn, 1);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	}

	if (fk_item_type(hitm) == FK_ITEM_DICT) {
		dct = (fk_dict *)fk_item_raw(hitm);
		itm = fk_conn_arg_get(conn, 3);
		fk_dict_add(dct, key, itm);
		rt = fk_conn_int_rsp_add(conn, 1);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	}

	rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}
	return FK_OK;
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
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	} 

	if (fk_item_type(hitm) != FK_ITEM_DICT) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	} 

	dct = (fk_dict *)fk_item_raw(hitm);
	itm = fk_dict_get(dct, key);
	if (itm == NULL) {
		rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	} 

	if (itm->type != FK_ITEM_STR) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	}

	value = (fk_str *)fk_item_raw(itm);
	rt = fk_conn_bulk_rsp_add(conn, (int)(fk_str_len(value)));
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}
	rt = fk_conn_content_rsp_add(conn, fk_str_raw(value), fk_str_len(value));
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}

	return FK_OK;
}

int fk_cmd_generic_push(fk_conn *conn, int pos)
{
	int rt, i;
	fk_list *lst;
	fk_item *key, *lst_itm, *str_itm;

	key = fk_conn_arg_get(conn, 1);
	lst_itm = fk_dict_get(server.db[conn->db_idx], key);
	if (lst_itm == NULL) {
		lst = fk_list_create(&db_list_op);
		for (i = 2; i < conn->arg_cnt; i++) {
			str_itm = fk_conn_arg_get(conn, i);
			if (pos == 0) {/* lpush */
				fk_list_head_insert(lst, str_itm);
			} else {/* rpush */
				fk_list_tail_insert(lst, str_itm);
			}
		}
		lst_itm = fk_item_create(FK_ITEM_LIST, lst);/* do not increase ref for local var */
		fk_dict_add(server.db[conn->db_idx], key, lst_itm);
		rt = fk_conn_int_rsp_add(conn, conn->arg_cnt - 2);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}

		return FK_OK;
	}

	if (fk_item_type(lst_itm) != FK_ITEM_LIST) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	}

	lst = fk_item_raw(lst_itm);
	for (i = 2; i < conn->arg_cnt; i++) {
		str_itm = fk_conn_arg_get(conn, i);
		if (pos == 0) {/* lpush */
			fk_list_head_insert(lst, str_itm);
		} else {/* rpush */
			fk_list_tail_insert(lst, str_itm);
		}
	}
	rt = fk_conn_int_rsp_add(conn, conn->arg_cnt - 2);
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}
	return FK_OK;
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
	fk_str *ss;
	fk_list *lst;
	fk_node *nd_itm;
	fk_item *key, *lst_itm, *itm;

	key = fk_conn_arg_get(conn, 1);
	lst_itm = fk_dict_get(server.db[conn->db_idx], key);
	if (lst_itm == NULL) {
		rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	}

	if (fk_item_type(lst_itm) != FK_ITEM_LIST) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	}

	lst = fk_item_raw(lst_itm);
	if (fk_list_len(lst) == 0) {
		rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	}

	if (pos == 0) {
		nd_itm = fk_list_head(lst);
	} else {
		nd_itm = fk_list_tail(lst);
	}
	itm = (fk_item *)fk_node_raw(nd_itm);
	ss = (fk_str *)fk_item_raw(itm);
	rt = fk_conn_bulk_rsp_add(conn, (int)(fk_str_len(ss)));
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}
	rt = fk_conn_content_rsp_add(conn, fk_str_raw(ss), fk_str_len(ss));
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}
	fk_list_any_remove(lst, nd_itm);

	return FK_OK;
}

int fk_cmd_lpop(fk_conn *conn)
{
	return fk_cmd_generic_pop(conn, 0);
}

int fk_cmd_rpop(fk_conn *conn)
{
	return fk_cmd_generic_pop(conn, 1);
}

int fk_cmd_llen(fk_conn *conn)
{
	int rt, len;
	fk_list *lst;
	fk_item *key, *value;

	key = fk_conn_arg_get(conn, 1);
	value = fk_dict_get(server.db[conn->db_idx], key);
	if (value == NULL) {
		rt = fk_conn_int_rsp_add(conn, 0);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	}
	if (fk_item_type(value) != FK_ITEM_LIST) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	}

	lst = (fk_list *)fk_item_raw(value);
	len = fk_list_len(lst);
	rt = fk_conn_int_rsp_add(conn, len);
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}
	return FK_OK;
}

int fk_cmd_save(fk_conn *conn)
{
	int rt, err;

	err = 1;

	if (server.save_done == 1) {
		rt = fk_svr_db_save();
		if (rt == FK_OK) {
			err = 0;
		}
	}

	if (err == 1) {
		rt = fk_conn_status_rsp_add(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	} else {
		rt = fk_conn_status_rsp_add(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	}

	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}
	return FK_OK;
}

int fk_cmd_select(fk_conn *conn)
{
	fk_str *s;
	fk_item *itm;
	int db_idx, rt;

	itm = fk_conn_arg_get(conn, 1);
	s = (fk_str *)fk_item_raw(itm);
	db_idx = atoi(fk_str_raw(s));
	if (db_idx < 0 || db_idx >= server.dbcnt) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_ERR, sizeof(FK_RSP_ERR) - 1);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	}

	conn->db_idx = db_idx;
	rt = fk_conn_status_rsp_add(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	if (rt == FK_CONN_ERR) {
		return FK_ERR;
	}
	return FK_OK;
}

int fk_cmd_eval(fk_conn *conn)
{
	int nkey, nargv, rt, i;
	char *code, **keys, **argv;
	fk_str *str_nkey, *str_code;
	fk_item *itm_code, *itm_nkey, *itm_arg;

	itm_code = fk_conn_arg_get(conn, 1);
	itm_nkey = fk_conn_arg_get(conn, 2);

	str_nkey = (fk_str *)fk_item_raw(itm_nkey);
	if (fk_str_is_nonminus(str_nkey) == 0) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt == FK_CONN_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	}

	nkey = atoi(fk_str_raw(str_nkey));
	keys = (char **)calloc(nkey, sizeof(char *));
	for (i = 0; i < nkey; i++) {
		itm_arg = fk_conn_arg_get(conn, 3 + i);
		keys[i] = fk_str_raw((fk_str *)fk_item_raw(itm_arg));
	}
	fk_lua_paras_push(keys, nkey, FK_LUA_PARA_KEYS);

	nargv = conn->arg_cnt - 1 - 2 - nkey;
	argv = (char **)calloc(nargv, sizeof(char *));
	for (i = 0; i < nargv; i++) {
		itm_arg = fk_conn_arg_get(conn, 3 + nkey + i);
		argv[i] = fk_str_raw((fk_str *)fk_item_raw(itm_arg));
	}
	fk_lua_paras_push(argv, nargv, FK_LUA_PARA_ARGV);

	/* just release the array, but not the every item in it */
	fk_mem_free(keys);
	fk_mem_free(argv);

	str_code = (fk_str *)fk_item_raw(itm_code);
	code = fk_str_raw(str_code);
	fk_lua_script_run(conn, code);

	return FK_OK;
}

/* -------------------------------------------- */
uint32_t fk_db_dict_key_hash(void *key)
{
	fk_str *s;
	fk_item *itm;

	itm = (fk_item *)key;
	s = (fk_str *)fk_item_raw(itm);
	return fk_str_hash(s);
}

int fk_db_dict_key_cmp(void *k1, void *k2)
{
	fk_str *s1, *s2;
	fk_item *itm1, *itm2;

	itm1 = (fk_item *)k1;
	itm2 = (fk_item *)k2;
	s1 = (fk_str *)fk_item_raw(itm1);
	s2 = (fk_str *)fk_item_raw(itm2);

	return fk_str_cmp(s1, s2);
}

void *fk_db_dict_key_copy(void *key)
{
	fk_item *itm;

	itm = (fk_item *)key;

	fk_item_ref_inc(itm);/* increase the ref here */

	return key;
}

void fk_db_dict_key_free(void *key)
{
	fk_item *itm;

	itm = (fk_item *)key;

	fk_item_ref_dec(itm);/* decrease the ref here */
}

void *fk_db_dict_val_copy(void *val)
{
	fk_item *itm;

	itm = (fk_item *)val;

	fk_item_ref_inc(itm);

	return val;
}

void fk_db_dict_val_free(void *val)
{
	fk_item *itm;
	itm = (fk_item *)val;

	fk_item_ref_dec(itm);
}

void *fk_db_list_val_copy(void *ptr)
{
	fk_item *itm;

	itm = (fk_item *)ptr;

	fk_item_ref_inc(itm);

	return ptr;
}

/* for lpush/lpop */
void fk_db_list_val_free(void *ptr)
{
	fk_item *itm;

	itm = (fk_item *)ptr;

	fk_item_ref_dec(itm);
}

/* server interface */
void fk_svr_conn_add(int fd)
{
	fk_conn *conn;

	conn = fk_conn_create(fd);
	server.conns_tab[conn->fd] = conn;
	server.conn_cnt += 1;
}

/* call when conn disconnect */
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
	if (fd == FK_SOCK_ERR) {
		fk_log_error("accept fd failed\n");
		return 0;
	}

	/* anything wrong by closing the fd directly? */
	if (server.conn_cnt == server.max_conn) {
		fk_log_warn("beyond max connections\n");
		close(fd);
		return 0;
	}
	fk_svr_conn_add(fd);
#ifdef FK_DEBUG
	fk_log_debug("conn_cnt: %u, max_conn: %u\n", server.conn_cnt, server.max_conn);
#endif
	/* why redis do like below? */
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
	unsigned i;
	fk_log_info("[timer 1]conn cnt: %u\n", server.conn_cnt);
	for (i = 0; i < server.dbcnt; i++) {
		fk_log_info("[timer 1]db %d size: %d, used: %d, limit: %d\n", i, server.db[i]->size, server.db[i]->used, server.db[i]->limit);
	}

	fk_svr_db_save_background();

	fk_svr_sync_with_master();

	return 0;
}

#ifdef FK_DEBUG
int fk_svr_timer_cb2(unsigned interval, char type, void *arg)
{
	fk_tmev *tmev;

	tmev = server.svr_timer2;

	fk_log_info("[timer 2]\n");
	fk_ev_tmev_remove(tmev);

	return -1;/* do not cycle once more */
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
		fk_log_error("fork: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	case 0:/* child continue */
		break;
	default:/* parent exit */
		fk_log_info("parent exit, to run as a daemon\n");
		exit(EXIT_SUCCESS);
	}

	if (setsid() == -1) {/* create a new session */
		fk_log_error("setsid: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	umask(0);

	fd = open("/dev/null", O_RDWR);
	if (fd == -1) {
		fk_log_error("open /dev/null: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (dup2(fd, STDIN_FILENO) == -1) {/* close STDIN */
		fk_log_error("dup2: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (dup2(fd, STDOUT_FILENO) == -1) {/* close STDOUT */
		fk_log_error("dup2: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (dup2(fd, STDERR_FILENO) == -1) {/* close STDERR */
		fk_log_error("dup2: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (fd > STDERR_FILENO) {
		if (close(fd) == -1) {
			fk_log_error("close: %s\n", strerror(errno));
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

	/* 
	 * copy setting from conf to server 
	 * but is it necessary???
	 */
	server.port = setting.port;
	server.max_conn = setting.max_conn;
	server.dbcnt = setting.dbcnt;
	server.db_file = fk_str_clone(setting.db_file);
	server.addr = fk_str_clone(setting.addr);

	/* create global environment */
	server.save_done = 1;
	server.start_time = time(NULL);
	server.stop = 0;
	server.conn_cnt = 0;
	server.conns_tab = (fk_conn **)fk_mem_alloc(sizeof(fk_conn *) * fk_util_conns_to_files(server.max_conn));
	server.listen_fd = fk_sock_create_listen(fk_str_raw(server.addr), server.port);
	if (server.listen_fd == FK_SOCK_ERR) {
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
	/* load db from file */
	fk_svr_db_load(server.db_file);
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
		fprintf(stderr, "getrlimit: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
#if defined(__linux__)
	fprintf(stdout, "original file number limit: rlim_cur = %zu, rlim_max = %zu\n", lmt.rlim_cur, lmt.rlim_max);
#elif defined(__APPLE__) || defined(__BSD__)
	fprintf(stdout, "original file number limit: rlim_cur = %llu, rlim_max = %llu\n", lmt.rlim_cur, lmt.rlim_max);
#endif

	if (max_files > lmt.rlim_max) {
		euid = geteuid();
		if (euid == 0) {/* root */
#ifdef FK_DEBUG
			fprintf(stdout, "running as root\n");
#endif
			lmt.rlim_max = max_files;
			lmt.rlim_cur = max_files;
			rt = setrlimit(RLIMIT_NOFILE, &lmt);
			if (rt < 0) {
				fprintf(stderr, "setrlimit: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
#if defined(__linux__)
			fprintf(stdout, "new file number limit: rlim_cur = %zu, rlim_max = %zu\n", lmt.rlim_cur, lmt.rlim_max);
#elif defined(__APPLE__) || defined(__BSD__)
			fprintf(stdout, "new file number limit: rlim_cur = %llu, rlim_max = %llu\n", lmt.rlim_cur, lmt.rlim_max);
#endif
		} else {/* non-root */
#ifdef FK_DEBUG
			fprintf(stdout, "running as non-root\n");
#endif
			max_files = lmt.rlim_max;/* open as many as possible files */
			lmt.rlim_cur = lmt.rlim_max;
			rt = setrlimit(RLIMIT_NOFILE, &lmt);
			if (rt < 0) {
				fprintf(stderr, "setrlimit: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			/* set current limit to the original setting */
			setting.max_conn = fk_util_files_to_conns(max_files);
			fprintf(stdout, "change the setting.max_conn according the current file number limit: %u\n", setting.max_conn);
		}
	}
}

void fk_set_pwd()
{
	int rt;

	rt = chdir(fk_str_raw(setting.dir));
	if (rt < 0) {
		fk_log_error("chdir failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	fk_log_info("change working diretory to %s\n", fk_str_raw(setting.dir));
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

fk_zline *fk_zline_create(size_t len)
{
	fk_zline *buf; 
	buf = fk_mem_alloc(sizeof(fk_zline));
	buf->len = len;
	buf->line = (char *)fk_mem_alloc(buf->len);

	return buf;
}

void fk_zline_adjust(fk_zline *buf, size_t len)
{
	if (buf->len < len) {
		buf->len = fk_util_min_power(len);
		buf->line = (char *)fk_mem_realloc(buf->line, buf->len);
	}
}

void fk_zline_destroy(fk_zline *buf)
{
	fk_mem_free(buf->line);
	fk_mem_free(buf);
}

int fk_svr_db_save()
{
	int rt;
	FILE *fp;
	unsigned i;
	char *temp_db;

	temp_db = "freekick-temp.db";

	/* step 1: write to a temporary file */
	fp = fopen(temp_db, "w+");
	if (fp == NULL) {
		return FK_ERR;
	}

	for (i = 0; i < server.dbcnt; i++) {
		rt = fk_svr_db_dump(fp, i);
		if (rt == FK_ERR) {
			fclose(fp);
			remove(temp_db);/* remove this temporary db file */
			return FK_ERR;
		}
	}
	fclose(fp);/* close before rename */

	/* step 2: rename temporary file to server.db_file */
	rt = rename(temp_db, fk_str_raw(server.db_file));
	if (rt < 0) {
		remove(temp_db);/* remove this temporary db file */
		return FK_ERR;
	}

	return FK_OK;
}

/*
 * to do: 
 * error handling
 */
int fk_svr_db_dump(FILE *fp, unsigned db_idx)
{
	fk_elt *elt;
	int type, rt;
	fk_dict *dct;
	size_t len, wz;
	fk_dict_iter *iter;

	dct = server.db[db_idx];
	if (fk_dict_len(dct) == 0) {
		return FK_OK;
	}

	/* I do not think it's necessary to call htonl() */
	wz = fwrite(&db_idx, sizeof(db_idx), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}

	/* dump the len of the dict */
	len = fk_dict_len(dct);
	wz = fwrite(&len, sizeof(len), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}

	/* dict body */
	iter = fk_dict_iter_begin(dct);
	while ((elt = fk_dict_iter_next(iter)) != NULL) {
		type = fk_item_type(((fk_item *)fk_elt_value(elt)));
		wz = fwrite(&type, sizeof(type), 1, fp);
		if (wz == 0) {
			fk_dict_iter_end(iter);/* need to release iterator */
			return FK_ERR;
		}
		switch (type) {
		case FK_ITEM_STR:
			rt = fk_svr_db_str_elt_dump(fp, elt);
			break;
		case FK_ITEM_LIST:
			rt = fk_svr_db_list_elt_dump(fp, elt);
			break;
		case FK_ITEM_DICT:
			rt = fk_svr_db_dict_elt_dump(fp, elt);
			break;
		}
		if (rt == FK_ERR) {
			fk_dict_iter_end(iter);/* need to release iterator */
			return FK_ERR;
		}
	}
	fk_dict_iter_end(iter);/* must release this iterator of fk_dict */

	return FK_OK;
}

int fk_svr_db_str_elt_dump(FILE *fp, fk_elt *elt)
{
	size_t len, wz;
	fk_str *key, *value;
	fk_item *kitm, *vitm;

	kitm = (fk_item *)fk_elt_key(elt);
	vitm = (fk_item *)fk_elt_value(elt);

	key = (fk_str *)(fk_item_raw(kitm));
	value = (fk_str *)(fk_item_raw(vitm));

	/* key dump */
	len = fk_str_len(key);
	wz = fwrite(&len, sizeof(len), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}
	wz = fwrite(fk_str_raw(key), fk_str_len(key), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}

	/* value dump */
	len = fk_str_len(value);
	wz = fwrite(&len, sizeof(len), 1, fp);
	/* 
	 * when len == 0 ==> wz == 0
	 * when len > 0  ==> wz > 0
	 */
	if (wz == 0) {
		return FK_ERR;
	}
	wz = fwrite(fk_str_raw(value), fk_str_len(value), 1, fp);
	if (len > 0 && wz == 0) {
		return FK_ERR;
	}

	return FK_OK;
}

int fk_svr_db_list_elt_dump(FILE *fp, fk_elt *elt)
{
	fk_node *nd;
	fk_list *lst;
	size_t len, wz;
	fk_str *key, *vs;
	fk_list_iter *iter;
	fk_item *kitm, *vitm, *nitm;

	kitm = (fk_item *)fk_elt_key(elt);
	vitm = (fk_item *)fk_elt_value(elt);

	key = (fk_str *)(fk_item_raw(kitm));
	lst = (fk_list *)(fk_item_raw(vitm));

	/* key dump */
	len = fk_str_len(key);
	wz = fwrite(&len, sizeof(len), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}
	wz = fwrite(fk_str_raw(key), fk_str_len(key), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}

	/* size dump */
	len = fk_list_len(lst);
	wz = fwrite(&len, sizeof(len), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}

	/* value dump */
	iter = fk_list_iter_begin(lst, FK_LIST_ITER_H2T);
	while ((nd = fk_list_iter_next(iter)) != NULL) {
		nitm = (fk_item *)(fk_node_raw(nd));
		vs = (fk_str *)(fk_item_raw(nitm));
		len = fk_str_len(vs);
		wz = fwrite(&len, sizeof(len), 1, fp);
		if (wz == 0) {
			fk_list_iter_end(iter);
			return FK_ERR;
		}

		wz = fwrite(fk_str_raw(vs), fk_str_len(vs), 1, fp);
		if (len > 0 && wz == 0) {
			fk_list_iter_end(iter);
			return FK_ERR;
		}
	}
	fk_list_iter_end(iter);/* must release this iterator of fk_list */

	return FK_OK;
}

int fk_svr_db_dict_elt_dump(FILE *fp, fk_elt *elt)
{
	fk_elt *selt;
	fk_dict *dct;
	size_t len, wz;
	fk_dict_iter *iter;
	fk_str *key, *skey, *svs;
	fk_item *kitm, *vitm, *skitm, *svitm;

	kitm = (fk_item *)fk_elt_key(elt);
	vitm = (fk_item *)fk_elt_value(elt);

	key = (fk_str *)(fk_item_raw(kitm));
	dct = (fk_dict *)(fk_item_raw(vitm));

	/* key dump */
	len = fk_str_len(key);
	wz = fwrite(&len, sizeof(len), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}
	wz = fwrite(fk_str_raw(key), fk_str_len(key), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}

	/* size dump */
	len = fk_dict_len(dct);
	wz = fwrite(&len, sizeof(len), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}

	iter = fk_dict_iter_begin(dct);
	while ((selt = fk_dict_iter_next(iter)) != NULL) {
		skitm = (fk_item *)fk_elt_key(selt);
		svitm = (fk_item *)fk_elt_value(selt);

		skey = (fk_str *)(fk_item_raw(skitm));
		svs = (fk_str *)(fk_item_raw(svitm));

		len = fk_str_len(skey);
		wz = fwrite(&len, sizeof(len), 1, fp);
		if (wz == 0) {
			fk_dict_iter_end(iter);
			return FK_ERR;
		}
		wz = fwrite(fk_str_raw(skey), fk_str_len(skey), 1, fp);
		if (wz == 0) {
			fk_dict_iter_end(iter);
			return FK_ERR;
		}

		len = fk_str_len(svs);
		wz = fwrite(&len, sizeof(len), 1, fp);
		if (wz == 0) {
			fk_dict_iter_end(iter);
			return FK_ERR;
		}
		wz = fwrite(fk_str_raw(svs), fk_str_len(svs), 1, fp);
		if (len > 0 && wz == 0) {
			fk_dict_iter_end(iter);
			return FK_ERR;
		}
	}
	fk_dict_iter_end(iter);/* must release this iterator of fk_dict */

	return FK_OK;
}

void fk_svr_db_save_background()
{
	int rt;

	if (setting.dump != 1) {
		return;
	}

	if (server.save_done != 1) {
		return;
	}

	fk_log_debug("to save db\n");
	rt = fork();
	if (rt < 0) {/* should not exit here, just return */
		fk_log_error("fork: %s\n", strerror(errno));
		return;
	} else if (rt > 0) {/* mark the save_done */
		server.save_done = 0;
		return;
	} else {
		/* execute only in child process */
		if (fk_svr_db_save() == FK_ERR) {
			fk_log_error("db save failed\n");
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
	}
}

void fk_svr_db_load(fk_str *db_file)
{
	int rt;
	FILE *fp; 
	long tail;
	fk_zline *buf;

	if (setting.dump != 1) {
		return;
	}

	buf = fk_zline_create(4096);
	fp = fopen(fk_str_raw(db_file), "r");
	if (fp == NULL) {/* db not exist */
		return;
	}
	fseek(fp, 0, SEEK_END);/* move to the tail */
	tail = ftell(fp);/* record the position of the tail */
	fseek(fp, 0, SEEK_SET);/* rewind to the head */

	while (ftell(fp) != tail) {
		rt = fk_svr_db_restore(fp, buf);
		if (rt == FK_ERR) {
			fk_log_error("load db body failed\n");
			exit(EXIT_FAILURE);
		}
	}
	fk_zline_destroy(buf);/* must free this block */
	fclose(fp);

	return;
}

/* 
 * return value: -1
 * 1. error occurs when reading 
 * 2. reaching to the end of file 
 */
int fk_svr_db_restore(FILE *fp, fk_zline *buf)
{
	int rt;
	fk_dict *db;
	size_t cnt, i, rz;
	unsigned type, idx;

	/* 
	 * to do:
	 * check the range of "idx"
	 */
	/* restore the index */
	rz = fread(&idx, sizeof(idx), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	if (idx >= server.dbcnt) {
		return FK_ERR;
	}
	db = server.db[idx];

	/* restore len of dictionary */
	rz = fread(&cnt, sizeof(cnt), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}

	/* load all the elements */
	for (i = 0; i < cnt; i++) {
		rz = fread(&type, sizeof(type), 1, fp);
		if (rz == 0) {
			return FK_ERR;
		}

		switch (type) {
		case FK_ITEM_STR:
			rt = fk_svr_db_str_elt_restore(fp, db, buf);
			break;
		case FK_ITEM_LIST:
			rt = fk_svr_db_list_elt_restore(fp, db, buf);
			break;
		case FK_ITEM_DICT:
			rt = fk_svr_db_dict_elt_restore(fp, db, buf);
			break;
		default:
			rt = -1;
			break;
		}
		if (rt == FK_ERR) {
			return FK_ERR;
		}
	}
	return FK_OK;
}

/* 
 * return value: -1
 * 1. error occurs when reading 
 * 2. reaching to the end of file 
 */
int fk_svr_db_str_elt_restore(FILE *fp, fk_dict *db, fk_zline *buf)
{
	fk_str *key, *value;
	fk_item *kitm, *vitm;
	size_t klen, vlen, rz;

	rz = fread(&klen, sizeof(klen), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	if (klen > FK_STR_HIGHWAT) {
		return FK_ERR;
	}

	fk_zline_adjust(buf, klen);
	rz = fread(buf->line, klen, 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	key = fk_str_create(buf->line, klen);
	kitm = fk_item_create(FK_ITEM_STR, key);

	rz = fread(&vlen, sizeof(vlen), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	if (vlen > FK_STR_HIGHWAT) {
		return FK_ERR;
	}
	fk_zline_adjust(buf, vlen);
	rz = fread(buf->line, vlen, 1, fp);
	if (vlen > 0 && rz == 0) {
		return FK_ERR;
	}

	value = fk_str_create(buf->line, vlen);
	vitm = fk_item_create(FK_ITEM_STR, value);

	fk_dict_add(db, kitm, vitm);

	return FK_OK;
}

/* 
 * return value: -1
 * 1. error occurs when reading 
 * 2. reaching to the end of file 
 */
int fk_svr_db_list_elt_restore(FILE *fp, fk_dict *db, fk_zline *buf)
{
	fk_list *lst;
	fk_str *key, *nds;
	fk_item *kitm, *vitm, *nitm;
	size_t klen, llen, i, nlen, rz;

	rz = fread(&klen, sizeof(klen), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	if (klen > FK_STR_HIGHWAT) {
		return FK_ERR;
	}
	fk_zline_adjust(buf, klen);
	rz = fread(buf->line, klen, 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	key = fk_str_create(buf->line, klen);
	kitm = fk_item_create(FK_ITEM_STR, key);

	rz = fread(&llen, sizeof(llen), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}

	lst = fk_list_create(&db_list_op);
	for (i = 0; i < llen; i++) {
		rz = fread(&nlen, sizeof(nlen), 1, fp);
		if (rz == 0) {
			return FK_ERR;
		}
		if (nlen > FK_STR_HIGHWAT) {
			return FK_ERR;
		}
		fk_zline_adjust(buf, nlen);
		rz = fread(buf->line, nlen, 1, fp);
		if (nlen > 0 && rz == 0) {
			return FK_ERR;
		}
		nds = fk_str_create(buf->line, nlen);
		nitm = fk_item_create(FK_ITEM_STR, nds);
		fk_list_tail_insert(lst, nitm);
	}
	vitm = fk_item_create(FK_ITEM_LIST, lst);
	fk_dict_add(db, kitm, vitm);

	return FK_OK;
}

/* 
 * return value: -1
 * 1. error occurs when reading 
 * 2. reaching to the end of file 
 */
int fk_svr_db_dict_elt_restore(FILE *fp, fk_dict *db, fk_zline *buf)
{
	fk_dict *sdct;
	fk_str *key, *skey, *svalue;
	fk_item *kitm, *skitm, *vitm, *svitm;
	size_t klen, sklen, svlen, dlen, i, rz;

	rz = fread(&klen, sizeof(klen), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	if (klen > FK_STR_HIGHWAT) {
		return FK_ERR;
	}
	fk_zline_adjust(buf, klen);
	rz = fread(buf->line, klen, 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	key = fk_str_create(buf->line, klen);
	kitm = fk_item_create(FK_ITEM_STR, key);

	rz = fread(&dlen, sizeof(dlen), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}

	sdct = fk_dict_create(&db_dict_eop);
	for (i = 0; i < dlen; i++) {
		rz = fread(&sklen, sizeof(sklen), 1, fp);
		if (rz == 0) {
			return FK_ERR;
		}
		if (sklen > FK_STR_HIGHWAT) {
			return FK_ERR;
		}
		fk_zline_adjust(buf, sklen);
		rz = fread(buf->line, sklen, 1, fp);
		if (rz == 0) {
			return FK_ERR;
		}
		skey = fk_str_create(buf->line, sklen);
		skitm = fk_item_create(FK_ITEM_STR, skey);

		rz = fread(&svlen, sizeof(svlen), 1, fp);
		if (rz == 0) {
			return FK_ERR;
		}
		if (svlen > FK_STR_HIGHWAT) {
			return FK_ERR;
		}
		fk_zline_adjust(buf, svlen);
		rz = fread(buf->line, svlen, 1, fp);
		if (svlen > 0 && rz == 0) {
			return FK_ERR;
		}
		svalue = fk_str_create(buf->line, svlen);
		svitm = fk_item_create(FK_ITEM_STR, svalue);

		fk_dict_add(sdct, skitm, svitm);
	}
	vitm = fk_item_create(FK_ITEM_DICT, sdct);
	fk_dict_add(db, kitm, vitm);

	return FK_OK;
}

int fk_svr_sync_with_master()
{
	return FK_OK;
}

void fk_main_init(char *conf_path)
{
	/* the first to init */
	fk_conf_init(conf_path);

	/* it must be done before fk_log_init() */
	fk_daemonize();

	fk_set_pwd();

	fk_setrlimit();

	/* 
	 * the second to init, so that all the 
	 * ther module can call fk_log_xxx() 
	 */
	fk_log_init();

	fk_write_pid_file();

	fk_signal_reg();

	fk_cache_init();

	fk_proto_init();

	fk_ev_init();

	fk_lua_init();

	fk_svr_init();
}

void fk_main_cycle()
{
	/* use server.stop to indicate whether to continue cycling */
	fk_ev_cycle(&(server.stop));

	//while (!server.stop) {
		//fk_ev_dispatch();
	//}
}

void fk_main_final()
{
	int rt;
	/* to do: free resource */
	while (server.save_done == 0) {
		sleep(1);
	}
	if (setting.dump != 1) {
		return;
	}
	/* save db once more */
	rt = fk_svr_db_save();
	if (rt == FK_ERR) {
		fk_log_error("db save failed\n");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	char *conf_path;

	if (argc > 2) {
		fprintf(stderr, "illegal argument!!!\n");
		fprintf(stderr, "usage:\n");
		fprintf(stderr, "\tfreekick [/path/freekick.conf]\n");
		exit(EXIT_FAILURE);
	}

	conf_path = NULL;
	if (argc == 2) {
		conf_path = argv[1];
	}
	if (conf_path == NULL) {
		fprintf(stdout, "no config file specified, using the default setting\n");
	}

	fk_main_init(conf_path);

	fk_main_cycle();

	fk_main_final();

	return FK_OK;
}
