#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <time.h>
#include <signal.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <fk_ev.h>
#include <fk_buf.h>
#include <fk_conn.h>
#include <fk_conf.h>
#include <fk_ev.h>
#include <fk_heap.h>
#include <fk_list.h>
#include <fk_log.h>
#include <fk_mem.h>
#include <fk_obj.h>
#include <fk_sock.h>
#include <fk_str.h>
#include <fk_macro.h>
#include <fk_util.h>
#include <fk_pool.h>
#include <fk_dict.h>
#include <freekick.h>

typedef struct _fk_server {
	int port;
	fk_str *addr;
	int listen_fd;
	int max_conn;//max connections
	int conn_cnt;//connection count
	time_t start;
	//int daemon;//1: run as daemon 0: not daemon
	fk_ioev *listen_ev;
	fk_tmev *svr_timer;
	fk_tmev *svr_timer2;
	fk_tmev *svr_timer3;
	//fk_tmev* timer;//do not own the timer obj any more
	fk_conn **all_conns;//MAX_CONN
	fk_str *pid_path;
	//db
	int dbcnt;
	fk_dict **db;
	fk_str *db_path;
	int save_done;
} fk_server;

static void fk_main_init(char *conf_path);
static void fk_main_end();
static void fk_setrlimit();
static void fk_daemon_run();
static void fk_write_pid_file();
static void fk_main_loop();
static void fk_svr_init();
static int fk_svr_timer_cb(int interval, unsigned char type, void *arg);
static int fk_svr_listen_cb(int listen_fd, unsigned char type, void *arg);
static void fk_svr_db_load(fk_str *db_path);
static void fk_svr_db_save();
static void fk_sigint(int sig);
static void fk_sigchld(int sig);
static void fk_dict_obj_free(void *elt);

//all the proto handlers
static int fk_on_set(fk_conn *conn);
static int fk_on_get(fk_conn *conn);
static int fk_on_hset(fk_conn *conn);
static int fk_on_hget(fk_conn *conn);
static int fk_on_zadd(fk_conn *conn);


//global variable
static fk_server server;

static fk_elt_op dbeop = {
	NULL,
	fk_str_destroy,
	NULL,
	fk_dict_obj_free
};

//all proto to deal
static fk_proto protos[] = {
	{"SET", FK_PROTO_WRITE, 3, fk_on_set},
	{"GET", FK_PROTO_READ, 2, fk_on_get},
	{"HSET", FK_PROTO_WRITE, 4, fk_on_hset},
	{"HGET", FK_PROTO_READ, 4, fk_on_hget},
	{"ZADD", FK_PROTO_WRITE, 4, fk_on_zadd},
	{NULL, FK_PROTO_INVALID, 0, NULL}
};

static fk_dict *pdct = NULL;

void fk_proto_init()
{
	int i;
	char *name;
	fk_str *key;
	void *value;

	pdct = fk_dict_create(NULL);
	if (pdct == NULL) {
		exit(EXIT_FAILURE);
	}

	for (i = 0; protos[i].type != FK_PROTO_INVALID; i++) {
		name = protos[i].name;
		value = protos + i;
		key = fk_str_create(name, strlen(name));
		fk_dict_add(pdct, key, value);
	}
}

fk_proto *fk_proto_search(fk_str *name)
{
	fk_proto *pto;

	pto = (fk_proto *)fk_dict_get(pdct, name);

	return pto;
}

int fk_on_set(fk_conn *conn)
{
	int i, len;
	char *reply;
	fk_str *key;
	fk_obj *value;

#ifdef FK_DEBUG
	fk_log_debug("[fk_on_set]parsed arg_cnt: %d\n", conn->arg_cnt);
	for (i = 0; i < conn->arg_cnt; i++) {
		fk_log_debug("[fk_on_set]idx: %d, len: %d, arg: %s\n", i, conn->args_len[i], conn->args[i]->data);
	}
#endif

	key = conn->args[1];
	value = fk_obj_create(FK_OBJ_STR, conn->args[2]);
	fk_dict_add(server.db[conn->db_idx], key, value);

	reply = "+OK\r\n";
	len = strlen(reply);
	if (FK_BUF_FREE_LEN(conn->wbuf) < len
		&& FK_BUF_TOTAL_LEN(conn->wbuf) < FK_BUF_HIGHWAT) 
	{
		conn->wbuf = fk_buf_stretch(conn->wbuf);
	}
	memcpy(FK_BUF_FREE_START(conn->wbuf), reply, len);
	FK_BUF_HIGH_INC(conn->wbuf, len);

	return 0;
}

int fk_on_get(fk_conn *conn)
{
	int pto_len, slen, sslen;
	fk_obj *obj;
	fk_str *key, *value;

	key = conn->args[1];
	obj = fk_dict_get(server.db[conn->db_idx], key);
	if (obj == NULL) {
		pto_len = 5;
		if (FK_BUF_FREE_LEN(conn->wbuf) < pto_len
			&& FK_BUF_TOTAL_LEN(conn->wbuf) < FK_BUF_HIGHWAT) 
		{
			conn->wbuf = fk_buf_stretch(conn->wbuf);
		}
		sprintf(FK_BUF_FREE_START(conn->wbuf), "%s", "$-1\r\n");
	} else {
		value = (fk_str *)obj->data;
		slen = value->len - 1;
		FK_UTIL_INT_LEN(slen, sslen);
		pto_len = value->len - 1 + 5 + sslen;
		if (FK_BUF_FREE_LEN(conn->wbuf) < pto_len
			&& FK_BUF_TOTAL_LEN(conn->wbuf) < FK_BUF_HIGHWAT) 
		{
			conn->wbuf = fk_buf_stretch(conn->wbuf);
		}
		sprintf(FK_BUF_FREE_START(conn->wbuf), "$%d\r\n%s\r\n", value->len - 1, value->data);
	}
	FK_BUF_HIGH_INC(conn->wbuf, pto_len);

	return 0;
}

int fk_on_hset(fk_conn *conn)
{
	char *reply;
	fk_dict *dct;
	fk_str *hkey, *key;
	fk_obj *hobj, *obj;

	hkey = conn->args[1];
	key = conn->args[2];
	hobj = fk_dict_get(server.db[conn->db_idx], hkey);
	if (hobj == NULL) {
		dct = fk_dict_create(&dbeop);
		obj = fk_obj_create(FK_OBJ_STR, conn->args[3]);
		fk_dict_add(dct, key, obj);
		hobj = fk_obj_create(FK_OBJ_DICT, dct);
		fk_dict_add(server.db[conn->db_idx], hkey, hobj);
	} else {
		dct = (fk_dict *)(hobj->data);
		obj = fk_obj_create(FK_OBJ_STR, conn->args[3]);
		fk_dict_add(dct, key, obj);
	}
	reply = "+OK\r\n";
	memcpy(conn->wbuf->data + conn->wbuf->low, reply, 5);
	FK_BUF_HIGH_INC(conn->wbuf, 5);
	return 0;
}

int fk_on_hget(fk_conn *conn)
{
	return 0;
}

int fk_on_zadd(fk_conn *conn)
{
	return 0;
}

void fk_dict_obj_free(void *val)
{
	fk_obj *obj;
	obj = (fk_obj *)val;
	fk_obj_destroy(obj);
}

//----------------------------
// server interface
//----------------------------
void fk_svr_conn_add(int fd)
{
	fk_conn *conn;

	conn = fk_conn_create(fd);
	server.all_conns[conn->fd] = conn;
	server.conn_cnt += 1;
}

//call when conn disconnect
void fk_svr_conn_remove(fk_conn *conn)
{
	server.all_conns[conn->fd] = NULL;
	server.conn_cnt -= 1;
#ifdef FK_DEBUG
	fk_log_debug("before free conn: %d\n", conn->fd);
#endif
	fk_conn_destroy(conn);
#ifdef FK_DEBUG
	fk_log_debug("after free conn\n");
#endif
}

int fk_svr_listen_cb(int listen_fd, unsigned char type, void *arg)
{
	int fd;

	fd = fk_sock_accept(listen_fd);
	if (server.conn_cnt == server.max_conn) {
		fk_sock_close(fd);
		return 0;
	}
	fk_svr_conn_add(fd);
	fk_log_info("new conn fd: %d\n", fd);
	return 0;
}

int fk_svr_timer_cb(int interval, unsigned char type, void *arg)
{
	fk_log_info("[timer callback 1]conn cnt: %d\n", server.conn_cnt);
	fk_log_info("[timer callback 1]dbdict size: %d, used: %d, limit: %d\n", server.db[0]->size, server.db[0]->used, server.db[0]->limit);
	//fk_log_info("[timer callback 1]protos len: %d\n", pdct->used);
	//fk_log_info("[memory] alloc size: %lu\n", fk_mem_allocated());
	//fk_log_info("[memory] alloc times: %lu\n", fk_mem_alloc_times());
	//fk_log_info("[memory] free times: %lu\n", fk_mem_free_times());
	fk_svr_db_save();
	return 0;
}

int fk_svr_timer_cb2(int interval, unsigned char type, void *arg)
{
	fk_tmev *tmev;

	tmev = server.svr_timer2;

	fk_log_info("[timer callback 2]\n");
	fk_ev_tmev_remove(tmev);

	return -1;//do not cycle once more
}

int fk_svr_timer_cb3(int interval, unsigned char type, void *arg)
{
	fk_tmev *tmev;

	tmev = (fk_tmev *)arg;
	fk_log_info("[timer callback 3] idx: %d\n", tmev->idx);
	return 0;
}

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

void fk_daemon_run()
{
	int  fd;

	if (setting.daemon == 0) {
		return;
	}

	switch (fork()) {
	case -1:
		fk_log_error("fork: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	case 0://child continue
		break;
	default:
		fk_log_info("parent exit, to run as a daemon\n");
		exit(EXIT_SUCCESS);//parent exit
	}

	if (setsid() == -1) {//create a new session
		fk_log_error("setsid: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	umask(0);

	fd = open("/dev/null", O_RDWR);
	if (fd == -1) {
		fk_log_error("open /dev/null: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (dup2(fd, STDIN_FILENO) == -1) {//close STDIN
		fk_log_error("dup2: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (dup2(fd, STDOUT_FILENO) == -1) {//close STDOUT
		fk_log_error("dup2: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (fd > STDERR_FILENO) {//why???
		if (close(fd) == -1) {
			fk_log_error("close: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
}

void fk_write_pid_file()
{
	FILE *pid_file;
	pid_t pid;

	pid_file = fopen(FK_STR_RAW(setting.pid_path), "w+");
	pid = getpid();
	fk_log_info("pid: %d\n", pid);
	fk_log_info("pid path: %s\n", FK_STR_RAW(setting.pid_path));
	fprintf(pid_file, "%d\n", pid);
	fclose(pid_file);
}

void fk_svr_init()
{
	int i;

	// copy setting from conf to server
	server.port = setting.port;
	server.max_conn = setting.max_conn;
	server.dbcnt = setting.dbcnt;
	server.db_path = fk_str_clone(setting.db_path);
	server.save_done = 1;
	server.addr = fk_str_clone(setting.addr);

	//create global environment
	server.start = time(NULL);
	server.conn_cnt = 0;
	server.all_conns = (fk_conn **)fk_mem_alloc(sizeof(fk_conn *) * (server.max_conn + FK_SAVED_FD));
	server.listen_fd = fk_sock_create_listen(FK_STR_RAW(server.addr), server.port);
	fk_log_debug("listen fd: %d\n", server.listen_fd);
	server.listen_ev = fk_ev_ioev_create(server.listen_fd, FK_EV_READ, NULL, fk_svr_listen_cb);
	fk_ev_ioev_add(server.listen_ev);

	server.svr_timer = fk_ev_tmev_create(3000, FK_EV_CYCLE, NULL, fk_svr_timer_cb);
	fk_ev_tmev_add(server.svr_timer);
	//server.svr_timer2 = fk_ev_tmev_create(4000, FK_EV_CYCLE, NULL, fk_svr_timer_cb2);
	//fk_ev_tmev_add(server.svr_timer2);

	server.db = (fk_dict **)fk_mem_alloc(sizeof(fk_dict *) * server.dbcnt);
	if (server.all_conns == NULL
		||server.listen_fd < 0
		||server.listen_ev == NULL
		||server.db == NULL)
	{
		fk_log_error("[server init failed\n]");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < server.dbcnt; i++) {
		server.db[i] = fk_dict_create(&dbeop);
		if (server.db[i] == NULL) {
			fk_log_error("[db create failed\n]");
			exit(EXIT_FAILURE);
		}
	}
	//load db from file
	fk_svr_db_load(server.db_path);
}

void fk_setrlimit()
{
	pid_t euid;
	struct rlimit lmt;
	int rt, max_files;

	max_files = setting.max_conn + FK_SAVED_FD;//
	rt = getrlimit(RLIMIT_NOFILE, &lmt);
	if (rt < 0) {
		fk_log_error("getrlimit: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	fk_log_info("original file number limit: rlim_cur = %llu, rlim_max = %llu\n", lmt.rlim_cur, lmt.rlim_max);

	if (max_files > lmt.rlim_max) {
		euid = geteuid();
		if (euid == 0) {//root
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
		} else {//non-root
#ifdef FK_DEBUG
			fk_log_debug("running as non-root\n");
#endif
			max_files = lmt.rlim_max;//open as many as possible files
			lmt.rlim_cur = lmt.rlim_max;
			rt = setrlimit(RLIMIT_NOFILE, &lmt);
			if (rt < 0) {
				fk_log_error("setrlimit: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			//set current limit to the original setting
			setting.max_conn = max_files - FK_SAVED_FD;
			fk_log_error("change the setting.max_conn according the current file number limit: %d\n", setting.max_conn);
		}
	}
}

void fk_sigint(int sig)
{
	fk_log_info("to exit by sigint\n");
	exit(EXIT_SUCCESS);
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

void fk_svr_db_save()
{
	int rt;

	if (server.save_done != 1) {
		return;
	}

	fk_log_info("to save db\n");
	rt = fork();
	if (rt < 0) {
		exit(EXIT_FAILURE);
	} else if (rt > 0) {
		server.save_done = 0;
		return;
	} else {
		sleep(6);
		exit(EXIT_SUCCESS);
	}
}

void fk_svr_db_load(fk_str *db_path)
{
	return;
}

void fk_main_init(char *conf_path)
{
	//the first to init
	fk_conf_init(conf_path);

	//the second to init, so that all the ther module can call fk_log_xxx()
	fk_log_init();

	fk_setrlimit();

	fk_daemon_run();

	fk_write_pid_file();

	fk_signal_reg();

	fk_list_init();

	fk_proto_init();

	fk_pool_init();

	fk_ev_init();

	fk_svr_init();
}

void fk_main_loop()
{
	for (;;) {
		fk_ev_dispatch();
	}
}

void fk_main_end()
{
	//to do: free resource
}

int main(int argc, char **argv)
{
	char *conf_path;

	if (argc != 2) {
		printf("no config specified, use default setting\n");
		conf_path = NULL;
	}
	conf_path = argv[1];

	fk_main_init(conf_path);

	fk_main_loop();

	fk_main_end();

	return 0;
}
