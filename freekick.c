/* feature test macros */
#include <fk_ftm.h>

/* c standard library headers */
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>

/* system headers */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>

/* local headers */
#include <fk_env.h>
#include <fk_conf.h>
#include <fk_log.h>
#include <fk_cache.h>
#include <fk_ev.h>
#include <fk_svr.h>
#include <freekick.h>

static void fk_daemonize(void);
static void fk_set_pwd(void);
static void fk_signal_register(void);
static void fk_setrlimit(void);
static void fk_set_seed(void);
static void fk_create_pidfile(void);

/* signal handlers */
static void fk_signal_exit_handler(int sig);
static void fk_signal_child_handler(int sig);

static void fk_main_init(char *conf_path);
static void fk_main_cycle(void);
static void fk_main_exit(void);

/*
static void
fk_daemon_run_old(void)
{
    if (setting.daemon == 1) {
        if (daemon(1, 0) < 0) {
            exit(EXIT_FAILURE);
        }
    }
}
*/

void
fk_daemonize(void)
{
    int  fd;

    if (setting.daemon == 0) {
        return;
    }

    switch (fork()) {
    case -1:
        fk_log_error("fork: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    case 0: /* child continue */
        break;
    default: /* parent exit */
        fk_log_info("parent exit, to run as a daemon\n");
        exit(EXIT_SUCCESS);
    }

    if (setsid() == -1) { /* create a new session */
        fk_log_error("setsid: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    umask(0);

    fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
        fk_log_error("open /dev/null: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (dup2(fd, STDIN_FILENO) == -1) { /* close STDIN */
        fk_log_error("dup2: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (dup2(fd, STDOUT_FILENO) == -1) { /* close STDOUT */
        fk_log_error("dup2: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (dup2(fd, STDERR_FILENO) == -1) { /* close STDERR */
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

void
fk_create_pidfile(void)
{
    FILE  *pid_file;
    pid_t  pid;

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

void
fk_setrlimit(void)
{
    int            rt;
    pid_t          euid;
    rlim_t         max_files;
    struct rlimit  lmt;

    max_files = (rlim_t)fk_util_conns_to_files(setting.max_conns);
    rt = getrlimit(RLIMIT_NOFILE, &lmt);
    if (rt < 0) {
        fk_log_error("getrlimit: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    /*
     * the type "rlim_t" has different size in linux from mac,
     * so just convert "rlim_t" to "unsigned long long"
     */
    fk_log_info("original file number limit: rlim_cur = %"PRIuMAX", rlim_max = %"PRIuMAX"\n", (uintmax_t)(lmt.rlim_cur), (uintmax_t)(lmt.rlim_max));

    /*
     * convert built-in type "int" to "rlim_t" type
     * make sure this conversion is right
     */
    if (max_files > lmt.rlim_max) {
        euid = geteuid();
        if (euid == 0) { /* root */
#ifdef FK_DEBUG
            fk_log_info("running as root\n");
#endif
            lmt.rlim_max = max_files;
            lmt.rlim_cur = max_files;
            rt = setrlimit(RLIMIT_NOFILE, &lmt);
            if (rt < 0) {
                fk_log_error("setrlimit: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            fk_log_info("new file number limit: rlim_cur = %llu, rlim_max = %llu\n", (unsigned long long)lmt.rlim_cur, (unsigned long long)lmt.rlim_max);
        } else { /* non-root */
#ifdef FK_DEBUG
            fk_log_info("running as non-root\n");
#endif
            max_files = lmt.rlim_max; /* open as many as possible files */
            lmt.rlim_cur = lmt.rlim_max;
            rt = setrlimit(RLIMIT_NOFILE, &lmt);
            if (rt < 0) {
                fk_log_error("setrlimit: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* set current limit to the original setting */
            setting.max_conns = (int)fk_util_files_to_conns(max_files);
            fk_log_info("change the setting.max_conns according the current file number limit: %u\n", setting.max_conns);
        }
    }
}

void
fk_set_seed(void)
{
    srand(time(NULL));
}

void
fk_set_pwd(void)
{
    int  rt;

    rt = chdir(fk_str_raw(setting.dir));
    if (rt < 0) {
        fk_log_error("chdir failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    fk_log_info("change working diretory to %s\n", fk_str_raw(setting.dir));
}

void
fk_signal_exit_handler(int sig)
{
    /*
     * maybe some other process in other modules, like:
     * fk_a_signal_exit_handler(sig);
     * fk_b_signal_exit_handler(sig);
     */
    fk_svr_signal_exit_handler(sig);
}

void
fk_signal_child_handler(int sig)
{
    /*
     * maybe some other process in other modules, like:
     * fk_a_signal_child_handler(sig);
     * fk_b_signal_child_handler(sig);
     */
    fk_svr_signal_child_handler(sig);
}

void
fk_signal_register(void)
{
    int               rt;
    struct sigaction  sa;

    sa.sa_handler = fk_signal_exit_handler;
    sa.sa_flags = 0;
    rt = sigemptyset(&sa.sa_mask);

    /* use the same handler for different signals */
    rt = sigaction(SIGINT, &sa, NULL);
    rt = sigaction(SIGTERM, &sa, NULL);
    rt = sigaction(SIGKILL, &sa, NULL);
    rt = sigaction(SIGQUIT, &sa, NULL);

    sa.sa_handler = fk_signal_child_handler;
    sa.sa_flags = 0;
    rt = sigemptyset(&sa.sa_mask);
    if (rt < 0) {
        exit(EXIT_FAILURE);
    }
    rt = sigaction(SIGCHLD, &sa, NULL);
    if (rt < 0) {
        exit(EXIT_FAILURE);
    }
}

void
fk_main_init(char *conf_path)
{
    /* the first to init */
    fk_conf_init(conf_path);

    /* it must be done before fk_log_init(), but why??? */
    fk_daemonize();

    fk_set_pwd();

    fk_setrlimit();

    /* where is the best place to call fk_set_seed()?? */
    fk_set_seed();

    /*
     * the second to init, so that all the
     * ther module can call fk_log_xxx()
     */
    fk_log_init(fk_str_raw(setting.log_path), setting.log_level);

    fk_create_pidfile();

    fk_signal_register();

    fk_cache_init();

    fk_ev_init(fk_util_conns_to_files(setting.max_conns));

    fk_svr_init();
}

void
fk_main_cycle(void)
{
    fk_ev_cycle();
}

void
fk_main_exit(void)
{
    /* maybe some other fk_xxx_exit() to call here */
    fk_svr_exit();
    /* no need to call exit(), because this is the last function called */
    //exit(EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
    char  *conf_path;

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

    fk_main_exit();

    return 0;
}
