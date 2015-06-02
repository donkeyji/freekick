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
	fk_str *db_path;
	int save_done;
} fk_server;

/* ---------------------------------------------------- */
static void fk_main_init(char *conf_path);
static void fk_main_final();
static void fk_main_cycle();
static void fk_svr_init();
static int fk_svr_timer_cb(unsigned interval, char type, void *arg);
static int fk_svr_listen_cb(int listen_fd, char type, void *arg);

static void fk_svr_db_load(fk_str *db_path);
static int fk_svr_db_restore(FILE *dbf, char **buf);
static int fk_svr_db_str_elt_restore(FILE *dbf, fk_dict *db, char **buf);
static int fk_svr_db_list_elt_restore(FILE *dbf, fk_dict *db, char **buf);
static int fk_svr_db_dict_elt_restore(FILE *dbf, fk_dict *db, char **buf);

static void fk_svr_db_save();
static int fk_svr_db_save_exec();
static int fk_svr_db_dump(FILE *dbf, int db_idx);
static int fk_svr_db_elt_dump(FILE *dbf, fk_elt *elt);
static int fk_svr_db_str_elt_dump(FILE *dbf, fk_elt *elt);
static int fk_svr_db_list_elt_dump(FILE *dbf, fk_elt *elt);
static int fk_svr_db_dict_elt_dump(FILE *dbf, fk_elt *elt);

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

/* ---------------------------------------------------- */
#define FK_RSP_OK				"OK"
#define FK_RSP_TYPE_ERR			"Type Error"
#define FK_RSP_NIL				(-1)
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
		key = (fk_str *)(elt->key);
		value = elt->value;
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
	if (rt < 0) {
		return -1;
	}

	return 0;
}

int fk_cmd_setnx(fk_conn *conn)
{
	int rt;
	fk_item *key, *value;

	key = fk_conn_arg_get(conn, 1);
	value = fk_dict_get(server.db[conn->db_idx], key);
	if (value != NULL) {
		rt = fk_conn_int_rsp_add(conn, 0);
		if (rt < 0) {
			return -1;
		}
		return 0;
	}

	value = fk_conn_arg_get(conn, 2);
	fk_dict_add(server.db[conn->db_idx], key, value);

	rt = fk_conn_int_rsp_add(conn, 1);
	if (rt < 0) {
		return -1;
	}
	return 0;
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
	if (rt < 0) {
		return -1;
	}
	return 0;
}

int fk_cmd_mget(fk_conn *conn)
{
	int rt, i;
	fk_str *ss;
	fk_item *value, *key;

	rt = fk_conn_mbulk_rsp_add(conn, conn->arg_cnt - 1);
	if (rt < 0) {
		return -1;
	}
	for (i = 1; i < conn->arg_cnt; i++) {
		key = fk_conn_arg_get(conn, i);
		value = fk_dict_get(server.db[conn->db_idx], key);
		if (value == NULL) {
			rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
			if (rt < 0) {
				return -1;
			}
		} else {
			if (fk_item_type(value) != FK_ITEM_STR) {
				rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
				if (rt < 0) {
					return -1;
				}
			} else {
				ss = (fk_str *)fk_item_raw(value);
				rt = fk_conn_bulk_rsp_add(conn, (int)(fk_str_len(ss) - 1));
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
	fk_str *ss;
	fk_item *value, *key;

	key = fk_conn_arg_get(conn, 1);
	value = fk_dict_get(server.db[conn->db_idx], key);
	if (value == NULL) {
		rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
		if (rt < 0) {
			return -1;
		}
		return 0;
	} 

	if (fk_item_type(value) != FK_ITEM_STR) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt < 0) {
			return -1;
		}
		return 0;
	} 

	ss = (fk_str *)fk_item_raw(value);
	rt = fk_conn_bulk_rsp_add(conn, (int)(fk_str_len(ss) - 1));
	if (rt < 0) {
		return -1;
	}
	rt = fk_conn_content_rsp_add(conn, fk_str_raw(ss), fk_str_len(ss) - 1);
	if (rt < 0) {
		return -1;
	}
	return 0;
}

int fk_cmd_del(fk_conn *conn)
{
	fk_item *key;
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
	fk_item *key, *value;

	key = fk_conn_arg_get(conn, 1);
	value = fk_dict_get(server.db[conn->db_idx], key);
	n = 0;
	if (value != NULL) {
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
		if (rt < 0) {
			return -1;
		}
		return 0;
	}

	if (fk_item_type(hitm) == FK_ITEM_DICT) {
		dct = (fk_dict *)fk_item_raw(hitm);
		itm = fk_conn_arg_get(conn, 3);
		fk_dict_add(dct, key, itm);
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
	rt = fk_conn_bulk_rsp_add(conn, (int)(fk_str_len(value) - 1));
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
		str_itm = fk_conn_arg_get(conn, i);
		if (pos == 0) {/* lpush */
			fk_list_head_insert(lst, str_itm);
		} else {/* rpush */
			fk_list_tail_insert(lst, str_itm);
		}
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
	fk_str *ss;
	fk_list *lst;
	fk_node *nd_itm;
	fk_item *key, *lst_itm, *itm;

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
	ss = (fk_str *)fk_item_raw(itm);
	rt = fk_conn_bulk_rsp_add(conn, (int)(fk_str_len(ss) - 1));
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

int fk_cmd_llen(fk_conn *conn)
{
	int rt, len;
	fk_list *lst;
	fk_item *key, *value;

	key = fk_conn_arg_get(conn, 1);
	value = fk_dict_get(server.db[conn->db_idx], key);
	if (value == NULL) {
		rt = fk_conn_int_rsp_add(conn, 0);
		if (rt < 0) {
			return -1;
		}
		return 0;
	}
	if (fk_item_type(value) != FK_ITEM_LIST) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt < 0) {
			return -1;
		}
		return 0;
	}

	lst = (fk_list *)fk_item_raw(value);
	len = fk_list_len(lst);
	rt = fk_conn_int_rsp_add(conn, len);
	if (rt < 0) {
		return -1;
	}
	return 0;
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
	fk_log_info("[timer 1]conn cnt: %u\n", server.conn_cnt);
	fk_log_info("[timer 1]dbdict size: %d, used: %d, limit: %d\n", server.db[0]->size, server.db[0]->used, server.db[0]->limit);

	if (setting.dump != 1) {
		return 0;
	}
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

	/* copy setting from conf to server */
	server.port = setting.port;
	server.max_conn = setting.max_conn;
	server.dbcnt = setting.dbcnt;
	server.db_path = fk_str_clone(setting.db_path);
	server.save_done = 1;
	server.addr = fk_str_clone(setting.addr);

	/* create global environment */
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
	/* load db from file */
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
		fprintf(stderr, "getrlimit: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	fprintf(stdout, "original file number limit: rlim_cur = %llu, rlim_max = %llu\n", lmt.rlim_cur, lmt.rlim_max);

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
			fprintf(stdout, "new file number limit: rlim_cur = %llu, rlim_max = %llu\n", lmt.rlim_cur, lmt.rlim_max);
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

int fk_svr_db_save_exec()
{
	int i;
	FILE *fp;

	fp = fopen(fk_str_raw(server.db_path), "w+");

	for (i = 0; i < server.dbcnt; i++) {
		fk_svr_db_dump(fp, i);
	}
	fclose(fp);
	return 0;
}

int fk_svr_db_dump(FILE *dbf, int db_idx)
{
	fk_elt *elt;
	fk_dict *dct;
	fk_dict_iter *iter;

	dct = server.db[db_idx];
	if (fk_dict_len(dct) == 0) {
		return 0;
	}

	/* only write index of db */
	fprintf(dbf, "%d\r\n", db_idx);

	/* dump the len of the dict */
	fprintf(dbf, "%zu\r\n", fk_dict_len(dct));

	/* dict body */
	iter = fk_dict_iter_begin(dct);
	elt = fk_dict_iter_next(iter);
	while (elt != NULL) {
		fk_svr_db_elt_dump(dbf, elt);
		elt = fk_dict_iter_next(iter);
	}

	return 0;
}

int fk_svr_db_elt_dump(FILE *dbf, fk_elt *elt)
{
	int type;

	type = fk_item_type(((fk_item *)elt->value));
	switch (type) {
	case FK_ITEM_STR:
		fk_svr_db_str_elt_dump(dbf, elt);
		break;
	case FK_ITEM_LIST:
		fk_svr_db_list_elt_dump(dbf, elt);
		break;
	case FK_ITEM_DICT:
		fk_svr_db_dict_elt_dump(dbf, elt);
		break;
	}
	return 0;
}

int fk_svr_db_str_elt_dump(FILE *dbf, fk_elt *elt)
{
	fk_str *key, *value;
	fk_item *kitm, *vitm;

	kitm = (fk_item *)(elt->key);
	vitm = (fk_item *)(elt->value);

	key = (fk_str *)(fk_item_raw(kitm));
	value = (fk_str *)(fk_item_raw(vitm));

	/* type dump */
	fprintf(dbf, "%u\r\n", fk_item_type(vitm));

	/* key dump */
	fprintf(dbf, "%zu\r\n", fk_str_len(key) - 1);
	fprintf(dbf, "%s\r\n", fk_str_raw(key));

	/* value dump */
	fprintf(dbf, "%zu\r\n", fk_str_len(value) - 1);
	fprintf(dbf, "%s\r\n", fk_str_raw(value));

	return 0;
}

int fk_svr_db_list_elt_dump(FILE *dbf, fk_elt *elt)
{
	fk_node *nd;
	fk_list *lst;
	fk_str *key, *vs;
	fk_list_iter *iter;
	fk_item *kitm, *vitm, *nitm;

	kitm = (fk_item *)(elt->key);
	vitm = (fk_item *)(elt->value);

	key = (fk_str *)(fk_item_raw(kitm));
	lst = (fk_list *)(fk_item_raw(vitm));

	/* type dump */
	fprintf(dbf, "%u\r\n", fk_item_type(vitm));

	/* key dump */
	fprintf(dbf, "%zu\r\n", fk_str_len(key) - 1);
	fprintf(dbf, "%s\r\n", fk_str_raw(key));

	/* size dump */
	fprintf(dbf, "%zu\r\n", fk_list_len(lst));

	/*value dump */
	iter = fk_list_iter_begin(lst, FK_LIST_ITER_H2T);
	while ((nd = fk_list_iter_next(iter)) != NULL) {
		nitm = (fk_item *)(fk_node_raw(nd));
		vs = (fk_str *)(fk_item_raw(nitm));
		fprintf(dbf, "%zu\r\n", fk_str_len(vs) - 1);
		fprintf(dbf, "%s\r\n", fk_str_raw(vs));
	}

	return 0;
}

int fk_svr_db_dict_elt_dump(FILE *dbf, fk_elt *elt)
{
	fk_elt *selt;
	fk_dict *dct;
	fk_dict_iter *iter;
	fk_str *key, *skey, *svs;
	fk_item *kitm, *vitm, *skitm, *svitm;

	kitm = (fk_item *)(elt->key);
	vitm = (fk_item *)(elt->value);

	key = (fk_str *)(fk_item_raw(kitm));
	dct = (fk_dict *)(fk_item_raw(vitm));

	/* type dump */
	fprintf(dbf, "%u\r\n", fk_item_type(vitm));

	/* key dump */
	fprintf(dbf, "%zu\r\n", fk_str_len(key) - 1);
	fprintf(dbf, "%s\r\n", fk_str_raw(key));

	/* size dump */
	fprintf(dbf, "%zu\r\n", fk_dict_len(dct));

	iter = fk_dict_iter_begin(dct);
	while ((selt = fk_dict_iter_next(iter)) != NULL) {
		skitm = (fk_item *)(selt->key);
		svitm = (fk_item *)(selt->value);

		skey = (fk_str *)(fk_item_raw(skitm));
		svs = (fk_str *)(fk_item_raw(svitm));

		fprintf(dbf, "%zu\r\n", fk_str_len(skey) - 1);
		fprintf(dbf, "%s\r\n", fk_str_raw(skey));

		fprintf(dbf, "%zu\r\n", fk_str_len(svs) - 1);
		fprintf(dbf, "%s\r\n", fk_str_raw(svs));
	}

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
		fk_svr_db_save_exec();
		exit(EXIT_SUCCESS);
	}
}

void fk_svr_db_load(fk_str *db_path)
{
	FILE *fp; 
	char *buf;
	int rt, tail;

	buf = NULL;/* must be initialized to NULL */
	fp = fopen(fk_str_raw(db_path), "r");
	if (fp == NULL) {/* db not exist */
		return;
	}
	fseek(fp, 0, SEEK_END);/* move to the tail */
	tail = ftell(fp);/* record the position of the tail */
	fseek(fp, 0, SEEK_SET);/* rewind to the head */

	while (ftell(fp) != tail) {
		rt = fk_svr_db_restore(fp, &buf);
		if (rt < 0) {
			fk_log_error("load db body failed\n");
			exit(EXIT_FAILURE);
		}
	}
	free(buf);/* must free this block */
	fclose(fp);

	return;
}

int fk_svr_db_restore(FILE *dbf, char **buf)
{
	int idx, rt;
	fk_dict *db;
	size_t cnt, i;
	unsigned type;

	/* restore the index */
	rt = fscanf(dbf, "%d\r\n", &idx);
	if (rt < 0) {
		return -1;
	}
	db = server.db[idx];
#ifdef FK_DEBUG
	fk_log_debug("===load db idx: %d\n", idx);
#endif

	/* restore len of dictionary */
	rt = fscanf(dbf, "%zu\r\n", &cnt);
	if (rt < 0) {
		return -1;
	}
#ifdef FK_DEBUG
	fk_log_debug("===load db size: %zu\n", cnt);
#endif

	/* load all the elements */
	for (i = 0; i < cnt; i++) {
		rt = fscanf(dbf, "%u\r\n", &type);
		if (rt < 0) {
			return -1;
		}

		switch (type) {
		case FK_ITEM_STR:
			rt = fk_svr_db_str_elt_restore(dbf, db, buf);
			break;
		case FK_ITEM_LIST:
			rt = fk_svr_db_list_elt_restore(dbf, db, buf);
			break;
		case FK_ITEM_DICT:
			rt = fk_svr_db_dict_elt_restore(dbf, db, buf);
			break;
		}
		if (rt < 0) {
			return -1;
		}
	}
	return 0;
}

int fk_svr_db_str_elt_restore(FILE *dbf, fk_dict *db, char **buf)
{
	int rt;
	fk_str *key, *value;
	fk_item *kitm, *vitm;
	size_t klen, vlen, llen;

	rt = fscanf(dbf, "%zu\r\n", &klen);
	if (rt < 0) {
		return -1;
	}
	rt = getline(buf, &llen, dbf);
	if (rt < 0) {
		return -1;
	}
	key = fk_str_create(*buf, klen);
	kitm = fk_item_create(FK_ITEM_STR, key);

	rt = fscanf(dbf, "%zu\r\n", &vlen);
	if (rt < 0) {
		return -1;
	}
	rt = getline(buf, &llen, dbf);
	if (rt < 0) {
		return -1;
	}
	value = fk_str_create(*buf, vlen);
	vitm = fk_item_create(FK_ITEM_STR, value);

	fk_dict_add(db, kitm, vitm);

	return 0;
}

int fk_svr_db_list_elt_restore(FILE *dbf, fk_dict *db, char **buf)
{
	int rt;
	fk_list *lst;
	fk_str *key, *nds;
	fk_item *kitm, *vitm, *nitm;
	size_t klen, llen, i, nlen, slen;

	rt = fscanf(dbf, "%zu\r\n", &klen);
	if (rt < 0) {
		return -1;
	}
	rt = getline(buf, &slen, dbf);
	if (rt < 0) {
		return -1;
	}
	key = fk_str_create(*buf, klen);
	kitm = fk_item_create(FK_ITEM_STR, key);

	rt = fscanf(dbf, "%zu\r\n", &llen);
	if (rt < 0) {
		return -1;
	}

	lst = fk_list_create(&db_list_op);
	for (i = 0; i < llen; i++) {
		rt = fscanf(dbf, "%zu\r\n", &nlen);
		if (rt < 0) {
			return -1;
		}
		rt = getline(buf, &slen, dbf);
		if (rt < 0) {
			return -1;
		}
		nds = fk_str_create(*buf, nlen);
		nitm = fk_item_create(FK_ITEM_STR, nds);
		fk_list_tail_insert(lst, nitm);
	}
	vitm = fk_item_create(FK_ITEM_LIST, lst);
	fk_dict_add(db, kitm, vitm);

	return 0;
}

int fk_svr_db_dict_elt_restore(FILE *dbf, fk_dict *db, char **buf)
{
	int rt;
	fk_dict *sdct;
	fk_str *key, *skey, *svalue;
	fk_item *kitm, *skitm, *vitm, *svitm;
	size_t klen, sklen, svlen, dlen, slen, i;

	rt = fscanf(dbf, "%zu\r\n", &klen);
	if (rt < 0) {
		return -1;
	}
	rt = getline(buf, &slen, dbf);
	if (rt < 0) {
		return -1;
	}
	key = fk_str_create(*buf, klen);
	kitm = fk_item_create(FK_ITEM_STR, key);

	rt = fscanf(dbf, "%zu\r\n", &dlen);
	if (rt < 0) {
		return -1;
	}

	sdct = fk_dict_create(&db_dict_eop);
	for (i = 0; i < dlen; i++) {
		rt = fscanf(dbf, "%zu\r\n", &sklen);
		if (rt < 0) {
			return -1;
		}
		rt = getline(buf, &slen, dbf);
		if (rt < 0) {
			return -1;
		}
		skey = fk_str_create(*buf, sklen);
		skitm = fk_item_create(FK_ITEM_STR, skey);

		rt = fscanf(dbf, "%zu\r\n", &svlen);
		if (rt < 0) {
			return -1;
		}
		rt = getline(buf, &slen, dbf);
		if (rt < 0) {
			return -1;
		}
		svalue = fk_str_create(*buf, svlen);
		svitm = fk_item_create(FK_ITEM_STR, svalue);

		fk_dict_add(sdct, skitm, svitm);
	}
	vitm = fk_item_create(FK_ITEM_DICT, sdct);
	fk_dict_add(db, kitm, vitm);

	return 0;
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
	/* to do: free resource */
	while (server.save_done == 0) {
		sleep(1);
	}
	/* save db once more */
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

	fk_main_cycle();

	fk_main_final();

	return 0;
}
