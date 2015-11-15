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
#include <freekick.h>

/* ---------------------------------------------------- */
static void fk_main_init(char *conf_path);
static void fk_main_final();
static void fk_main_cycle();

static void fk_svr_init();
static int fk_svr_timer_cb(unsigned interval, char type, void *arg);
static int fk_svr_listen_cb(int listen_fd, char type, void *arg);

static void fk_daemonize();
static void fk_set_pwd();
static void fk_signal_reg();
static void fk_sigint(int sig);
static void fk_sigchld(int sig);
static void fk_setrlimit();
static void fk_set_seed();
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
	{"ZADD",	FK_PROTO_WRITE,		FK_PROTO_VARLEN,	fk_cmd_zadd		},
	{NULL, 		FK_PROTO_INVALID, 	0, 					NULL			}
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

	server.timer_cnt++;

	fk_log_info("[timer 1]conn cnt: %u, timer_cnt: %llu, last_save: %zu\n", server.conn_cnt, server.timer_cnt, server.last_save);
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
	/* the type "rlim_t" has different size in linux from mac,
	 * so just convert "rlim_t" to "unsigned long long"
	 */
	fprintf(stdout, "original file number limit: rlim_cur = %llu, rlim_max = %llu\n", (unsigned long long)lmt.rlim_cur, (unsigned long long)lmt.rlim_max);

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
			fprintf(stdout, "new file number limit: rlim_cur = %llu, rlim_max = %llu\n", (unsigned long long)lmt.rlim_cur, (unsigned long long)lmt.rlim_max);
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

void fk_set_seed()
{
	srand(time(NULL));
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
		server.save_pid = -1;/* the saving child process is terminated */
		server.last_save = time(NULL);
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

	/* where is the best place to call fk_set_seed()?? */
	fk_set_seed();

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
