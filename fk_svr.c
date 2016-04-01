/* c standard library headers */
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

/* unix headers */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>

/* local headers */
#include <fk_env.h>
#include <fk_conf.h>
#include <fk_log.h>
#include <fk_mem.h>
#include <fk_sock.h>
#include <fk_cache.h>
#include <fk_item.h>
#include <fk_svr.h>

/*
 * signal flags
 * the timer checks these sigxxx_flag periodically to see whether the
 * corresponding signals have delivered. If delivered, do something and
 * restore the state of these flags to the original
 */
volatile sig_atomic_t sigint_flag = 0;
volatile sig_atomic_t sigterm_flag = 0;
volatile sig_atomic_t sigchld_flag = 0;

static int fk_svr_timer_cb(uint32_t interval, uint8_t type, void *arg);
#ifdef FK_DEBUG
static int fk_svr_timer_cb2(uint32_t interval, uint8_t type, void *arg);
#endif
static void fk_svr_listen_cb(int listen_fd, uint8_t type, void *arg);

static uint32_t fk_db_dict_key_hash(void *key);
static int fk_db_dict_key_cmp(void *k1, void *k2);
static void *fk_db_dict_key_copy(void *key);
static void fk_db_dict_key_free(void *key);
static void *fk_db_dict_val_copy(void *val);
static void fk_db_dict_val_free(void *elt);

static void *fk_db_list_val_copy(void *ptr);
static void fk_db_list_val_free(void *ptr);

static void *fk_db_skiplist_val_copy(void *ptr);
static void fk_db_skiplist_val_free(void *ptr);

/* global variable */
fk_svr_t server;

fk_elt_op_t db_dict_eop = {
    fk_db_dict_key_hash,
    fk_db_dict_key_cmp,
    fk_db_dict_key_copy,
    fk_db_dict_key_free,
    fk_db_dict_val_copy,
    fk_db_dict_val_free
};

/* for lpush/lpop */
fk_node_op_t db_list_op = {
    fk_db_list_val_copy,
    fk_db_list_val_free,
    NULL
};

fk_skipnode_op_t db_skiplist_op = {
    fk_db_skiplist_val_copy,
    fk_db_skiplist_val_free,
    NULL
};

uint32_t
fk_db_dict_key_hash(void *key)
{
    fk_str_t   *s;
    fk_item_t  *itm;

    itm = (fk_item_t *)key;
    s = (fk_str_t *)fk_item_raw(itm);
    return fk_str_hash(s);
}

int
fk_db_dict_key_cmp(void *k1, void *k2)
{
    fk_str_t   *s1, *s2;
    fk_item_t  *itm1, *itm2;

    itm1 = (fk_item_t *)k1;
    itm2 = (fk_item_t *)k2;
    s1 = (fk_str_t *)fk_item_raw(itm1);
    s2 = (fk_str_t *)fk_item_raw(itm2);

    return fk_str_cmp(s1, s2);
}

void *
fk_db_dict_key_copy(void *key)
{
    fk_item_t  *itm;

    itm = (fk_item_t *)key;

    fk_item_inc_ref(itm); /* increase the ref here */

    return key;
}

void
fk_db_dict_key_free(void *key)
{
    fk_item_t  *itm;

    itm = (fk_item_t *)key;

    fk_item_dec_ref(itm); /* decrease the ref here */
}

void *
fk_db_dict_val_copy(void *val)
{
    fk_item_t  *itm;

    itm = (fk_item_t *)val;

    fk_item_inc_ref(itm);

    return val;
}

void
fk_db_dict_val_free(void *val)
{
    fk_item_t  *itm;

    itm = (fk_item_t *)val;

    fk_item_dec_ref(itm);
}

void *
fk_db_list_val_copy(void *ptr)
{
    fk_item_t  *itm;

    itm = (fk_item_t *)ptr;

    fk_item_inc_ref(itm);

    return ptr;
}

/* for lpush/lpop */
void
fk_db_list_val_free(void *ptr)
{
    fk_item_t  *itm;

    itm = (fk_item_t *)ptr;

    fk_item_dec_ref(itm);
}

void *
fk_db_skiplist_val_copy(void *ptr)
{
    fk_item_t  *itm;

    itm = (fk_item_t *)ptr;

    fk_item_inc_ref(itm);

    return ptr;
}

void
fk_db_skiplist_val_free(void *ptr)
{
    fk_item_t  *itm;

    itm = (fk_item_t *)ptr;

    fk_item_dec_ref(itm);
}

void
fk_svr_listen_cb(int listen_fd, uint8_t type, void *arg)
{
    int  fd;

    while (1) {
        fd = accept(listen_fd, NULL, NULL);
        if (fd < 0) {
            /* interrupt by a signal */
            if (errno == EINTR) {
                fk_log_debug("accept interrupted by a signal\n");
                continue;
            }
            /* no more connection */
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fk_log_debug("no more connection available now\n");
                break;
            }
            /* any other errno to handle??? */
        }
        /* anything wrong by closing the fd directly? */
        if (server.conn_cnt == setting.max_conns) {
            fk_log_warn("beyond max connections\n");
            close(fd);
            continue;
        }

        /* no need to check the return code???? */
        fk_sock_set_nonblocking(fd);
        fk_sock_set_keepalive(fd);

        fk_svr_add_conn(fd);
#ifdef FK_DEBUG
        fk_log_debug("conn_cnt: %d, max_conns: %d\n", server.conn_cnt, setting.max_conns);
#endif
        /* why redis do like below? */
        //if (server.conn_cnt > server.max_conns) {
        //fk_log_info("beyond max connections\n");
        //fk_svr_remove_conn(fk_svr_conn_get(fd));
        //return;
        //}
        fk_log_info("new connection fd: %d\n", fd);
    }
    return;
}

int
fk_svr_timer_cb(uint32_t interval, uint8_t type, void *arg)
{
    int       st;
    pid_t     cpid;
    uint32_t  i;

    server.timer_cnt++;

    fk_log_info("[timer 1]conn cnt: %d, timer_cnt: %"PRIu64", last_save: %zu\n, iompx: %s\n", server.conn_cnt, server.timer_cnt, server.last_save, fk_ev_iompx_name());
    for (i = 0; i < server.dbcnt; i++) {
        fk_log_info("[timer 1]db %d size: %d, used: %d, limit: %d\n", i, server.db[i]->size, server.db[i]->used, server.db[i]->limit);
    }
#ifdef FK_DEBUG
    fk_ev_stat();
#endif

    /* deal with the exit of the child process */
    if (sigchld_flag == 1) {
        cpid = wait(&st);
        if (cpid < 0) {
            exit(EXIT_FAILURE);
        }
        if (st == 0) {
            server.save_pid = -1; /* the saving child process is terminated */
            server.last_save = time(NULL);
        }
        fk_log_debug("db saving done in background process\n");

        sigchld_flag = 0; /* restore the state of sigchld_flag */
    }

    /* deal with the shutdown signals */
    if (sigint_flag == 1 || sigterm_flag == 1) {
        /*
         * no need to restore the state of sigint_flag & sigterm_flag
         * because we're going to exit
         */
        fk_log_info("to exit by signal\n");
        fk_ev_stop(); /* will stop the event cycle int the next loop */
        return 0;
    }

    fk_fkdb_bgsave();

    fk_svr_sync_with_master();

    return 0;
}

#ifdef FK_DEBUG
int
fk_svr_timer_cb2(uint32_t interval, uint8_t type, void *arg)
{
    fk_tmev_t  *tmev;

    tmev = server.svr_timer2;

    fk_log_info("[timer 2]\n");

    /*
     * if we donot want to use this timer any more, remove it in this callback
     * because fk_ev donot remove any timer
     */
    fk_ev_remove_tmev(tmev);

    return -1; /* do not cycle once more */
}
#endif

void
fk_svr_init(void)
{
    int       blog_loaded;
    uint32_t  i;

    /* protocol sub module */
    fk_proto_init();

    /*
     * copy setting from conf to server
     * but is it necessary???
     */
    server.dbcnt = setting.dbcnt;

    /* create global environment */
    server.arch = sizeof(uintptr_t) == 4 ? 32 : 64;
    server.save_pid = -1; /* -1 is a invalid pid */
    server.start_time = time(NULL);
    server.last_save = time(NULL);
    server.conn_cnt = 0;
    server.timer_cnt = 0;
    server.conns_tab = (fk_conn_t **)fk_mem_alloc(sizeof(fk_conn_t *) * fk_util_conns_to_files(setting.max_conns));
    server.listen_fd = fk_sock_create_tcp_listen(fk_str_raw(setting.addr), setting.port);
    if (server.listen_fd == FK_SOCK_ERR) {
        fk_log_error("server listen socket creating failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
#ifdef FK_DEBUG
    fk_log_debug("listen fd: %d\n", server.listen_fd);
#endif
    server.listen_ev = fk_ioev_create(server.listen_fd, FK_IOEV_READ, NULL, fk_svr_listen_cb);
    fk_ev_add_ioev(server.listen_ev);

    server.svr_timer = fk_tmev_create(3000, FK_TMEV_CYCLE, NULL, fk_svr_timer_cb);
    fk_ev_add_tmev(server.svr_timer);
#ifdef FK_DEBUG
    server.svr_timer2 = fk_tmev_create(4000, FK_TMEV_CYCLE, NULL, fk_svr_timer_cb2);
    fk_ev_add_tmev(server.svr_timer2);
#endif

    server.db = (fk_dict_t **)fk_mem_alloc(sizeof(fk_dict_t *) * server.dbcnt);
    for (i = 0; i < server.dbcnt; i++) {
        server.db[i] = fk_dict_create(&db_dict_eop);
    }

    /* lua sub module */
    fk_lua_init();

    blog_loaded = 0;
    /* blog file precedes db file */
    if (setting.blog_on == 1) {
        fk_blog_init();
        if (access(fk_str_raw(setting.blog_file), F_OK) == 0) {
            fk_blog_load(setting.blog_file);
            blog_loaded = 1;
        }
    }

    if (setting.dump == 1) {
        fk_fkdb_init();
        if (blog_loaded == 0) {
            if (access(fk_str_raw(setting.db_file), F_OK) == 0) {
                fk_fkdb_load(setting.db_file);
            }
        }
    }
}

void
fk_svr_exit(void)
{
    int  rt;

    if (setting.dump != 1) {
        return;
    }

    /* the child process is running at present */
    if (server.save_pid != -1) {
        rt = wait(NULL);
        if (rt < 0) {
            if (errno == ECHILD) {
                fk_log_info("no child process running now\n");
                return;
            }
            fk_log_error("call wait() error: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    /* save db once more */
    rt = fk_fkdb_save();
    if (rt == FK_SVR_ERR) {
        fk_log_error("db save failed\n");
        exit(EXIT_FAILURE);
    }
}


/*
 * only mark the sigxxx_flag flag, and return
 * only those async-signal-safe function can be called inside
 */
void
fk_svr_signal_exit_handler(int sig)
{
    /* just mark the flags */
    switch (sig) {
    case SIGINT:
        sigint_flag = 1;
        break;
    case SIGTERM:
        sigterm_flag = 1;
        break;
    default:
        break;
    }

    return;

    /*
     * fk_log_info() is not a async-signal-safe function
     * because it calls the stdio function fprintf() inside
     * so fk_log_info() should not be called in signal handler
     */
    //fk_log_info("to exit by signal: %d\n", sig);

    //fk_ev_stop(); /* stop the event cycle */
    //exit(EXIT_SUCCESS);
}

void
fk_svr_signal_child_handler(int sig)
{
    /* just mark the sigchld_flag and return */
    sigchld_flag = 1;

    return;
}

int
fk_svr_sync_with_master(void)
{
    return FK_SVR_OK;
}
