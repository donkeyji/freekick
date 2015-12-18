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
#include <fk_conf.h>
#include <fk_log.h>
#include <fk_mem.h>
#include <fk_sock.h>
#include <fk_cache.h>
#include <fk_item.h>
#include <fk_svr.h>

/* ---------------------------------------------------- */
static int fk_svr_timer_cb(unsigned interval, char type, void *arg);
static int fk_svr_listen_cb(int listen_fd, char type, void *arg);

static uint32_t fk_db_dict_key_hash(void *key);
static int fk_db_dict_key_cmp(void *k1, void *k2);
static void *fk_db_dict_key_copy(void *key);
static void fk_db_dict_key_free(void *key);
static void *fk_db_dict_val_copy(void *val);
static void fk_db_dict_val_free(void *elt);

static void *fk_db_list_val_copy(void *ptr);
static void fk_db_list_val_free(void *ptr);

static void *fk_db_sklist_val_copy(void *ptr);
static void fk_db_sklist_val_free(void *ptr);

/* global variable */
fk_server server;

fk_elt_op db_dict_eop = {
	fk_db_dict_key_hash,
	fk_db_dict_key_cmp,
	fk_db_dict_key_copy,
	fk_db_dict_key_free,
	fk_db_dict_val_copy,
	fk_db_dict_val_free
};

/* for lpush/lpop */
fk_node_op db_list_op = {
	fk_db_list_val_copy,
	fk_db_list_val_free,
	NULL
};

fk_sknode_op db_sklist_op = {
	fk_db_sklist_val_copy,
	fk_db_sklist_val_free,
	NULL
};

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

void *fk_db_sklist_val_copy(void *ptr)
{
	fk_item *itm;

	itm = (fk_item *)ptr;

	fk_item_ref_inc(itm);

	return ptr;
}

void fk_db_sklist_val_free(void *ptr)
{
	fk_item *itm;

	itm = (fk_item *)ptr;

	fk_item_ref_dec(itm);
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

	server.timer_cnt++;

	fk_log_info("[timer 1]conn cnt: %u, timer_cnt: %llu, last_save: %zu\n, iompx: %s\n", server.conn_cnt, server.timer_cnt, server.last_save, fk_ev_iompx_name());
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

void fk_svr_init()
{
	unsigned i;

	/* sub module of fk_svr initialize */
	fk_proto_init();
	fk_lua_init();

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
	server.save_done = 1;/* no saving child process now */
	server.save_pid = -1;/* -1 is a invalid pid */
	server.start_time = time(NULL);
	server.last_save = time(NULL);
	server.stop = 0;
	server.conn_cnt = 0;
	server.timer_cnt = 0;
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
		server.save_pid = -1;/* the saving child process is terminated */
		server.last_save = time(NULL);
	}
	fk_log_debug("save db done\n");
}

int fk_svr_sync_with_master()
{
	return FK_OK;
}
