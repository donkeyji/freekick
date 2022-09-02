/* feature test macros */
#include <fk_ftm.h>

/* c standard headers */
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>

/* unix headers */
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
//static void fk_signal_child_handler(int sig);

static void fk_main_init(char *conf_path);
static void fk_main_cycle(void);
static void fk_main_exit(void);

/*
 * just because this daemon() library function is obsolete
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

    /*
     * after making the call to fork(), this child will not be the group leader,
     * thus the following call to setsid() will succeed
     */
    switch (fork()) {
    case -1:
        fk_log_error("fork: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    case 0: /* child continue */
        break;
    default: /* parent exit */
        fk_log_info("parent exit, to run as a daemon\n");
        _exit(EXIT_SUCCESS); /* do not use exit(EXIT_SUCCESS) */
    }

    /*
     * after calling setsid(), a new session will be created as the following:
     * 1. this process will become the leader of the new session
     * 2. this process will be the leader of the new process group in this new
     *    session
     * 3. this process has no controlling terminal any more
     */
    if (setsid() == -1) { /* create a new session */
        fk_log_error("setsid: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /*
     * after this secondary fork(), the parent exits, and the child continues.
     * The child is not the session leader, and thus, according to the System V
     * conventions, this child can never reacquire a controlling terminal.
     * On BSDs, this secondary fork() has no effect, but does no harm.
     */
    switch (fork()) {
    case -1:
        fk_log_error("fork: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
        break;
    case 0:
        break;
    default:
        _exit(EXIT_SUCCESS);
        break;
    }

    /* clear file mode creation mask */
    umask(0);

    /*
     * we do not close any file descriptors here, in fact, we did not open any
     * file before calling fk_daemonize(). The configuration file was opened for
     * reading, but later closed.
     * we just redirect 0,1,2 to /dev/null
     */

    /*
     * file descriptor 0,1,2 should be redirected to /dev/null
     * 1. later calls to library functions which may perform I/O on 0,1,2
     * 2. prevent the possibility that 0,1,2 will be occupied by other files
     */
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

    /*
     * we have redirected 0,1,2 to fd, and fd will never be used
     * so it is preferable to close this fd
     */
    if (fd > STDERR_FILENO) {
        if (close(fd) == -1) {
            fk_log_error("close: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
}

/*
 * TODO:
 * We can place a exclusive file lock on the pidfile in order to permit only one
 * instance to run at one time. Every time the program try to write pid to the
 * pidfile, it needs to obtain the exclusive file lock first.
 */
void
fk_create_pidfile(void)
{
    FILE  *pid_file;
    pid_t  pid;

    if (setting.daemon == 0) {
        return;
    }

    /*
     * By default, fully buffered is employed for disk files
     */
    pid_file = fopen(fk_str_raw(setting.pid_path), "w+");
    pid = getpid();
    fk_log_debug("pid: %d, pid file: %s\n", pid, fk_str_raw(setting.pid_path));
    fprintf(pid_file, "%d\n", pid);

    /*
     * although fclose() will implicitly call fflush() to flush all buffered
     * data in stdio buffer space to kernel buffer cache, but it is not
     * guaranteed that all data has been written to disk physically, so we need
     * to provide this guarantee manually by calling fflush() and fsync() before
     * calling fclose().
     * fflush() and fsync() will, respectively, flush stdio buffered data to
     * kernel buffer cache and flush kernel buffered data to disk.
     * rather than fdatasync(), we choose fsync() for a more restrictive
     * integrity reason, which ensures synchronized I/O file integrity, while
     * fdatasync() only provide synchronized I/O data integrity.
     */
    fflush(pid_file); /* flush the buffered data in stdio buffer */
    fsync(fileno(pid_file)); /* physically flush data to disk */
    fclose(pid_file);
}

void
fk_setrlimit(void)
{
    int            rt;
    uid_t          euid;
    rlim_t         max_files;
    struct rlimit  lmt;

    max_files = (rlim_t)fk_util_conns_to_files(setting.max_conns);
    rt = getrlimit(RLIMIT_NOFILE, &lmt);
    if (rt < 0) {
        fk_log_error("getrlimit: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    /*
     * the "rlim_t" type has a different size on linux from mac, so just convert
     * "rlim_t" to "unsigned long long" for portable code
     */
    fk_log_info("original file number limit: rlim_cur = %"PRIuMAX", rlim_max = %"PRIuMAX"\n", (uintmax_t)(lmt.rlim_cur), (uintmax_t)(lmt.rlim_max));

    /*
     * convert built-in type "int" to "rlim_t" type
     * make sure this conversion is right
     */
    if (max_files > lmt.rlim_max) {
        euid = geteuid();
        if (euid == 0) { /* root */
            fk_log_debug("running as root\n");
            lmt.rlim_max = max_files;
            lmt.rlim_cur = max_files;
            rt = setrlimit(RLIMIT_NOFILE, &lmt);
            if (rt < 0) {
                fk_log_error("setrlimit: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            fk_log_info("new file number limit: rlim_cur = %llu, rlim_max = %llu\n", (unsigned long long)lmt.rlim_cur, (unsigned long long)lmt.rlim_max);
        } else { /* non-root */
            fk_log_debug("running as non-root\n");
            max_files = lmt.rlim_max; /* open as many files as possible */
            lmt.rlim_cur = lmt.rlim_max;
            rt = setrlimit(RLIMIT_NOFILE, &lmt);
            if (rt < 0) {
                fk_log_error("setrlimit: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            /* set the current limit to the original setting */
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

    /*
     * each process has two directory-related attributes:
     * 1. root directory <==> chroot(), privileged required
     * 2. current working directory <==> chdir(), nonprivileged required
     * root directory determines the point from which absolute pathnames are
     * interpreted;
     * current working directory determines the point from which relative
     * pathnames are interpreted.
     * Both of these two directory of a new process are inherited from the
     * parent process.
     * the argument pathname of chdir() identifies the new current working
     * directory, specified via a relative or an absolute pathname
     * note that the current working directory would not be changed after a
     * chroot() call, so this result can be used to break out of the chroot
     * jail for a unpriviledge/privileged program.
     */
    rt = chdir(fk_str_raw(setting.dir));
    if (rt < 0) {
        fk_log_error("chdir failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    fk_log_info("change working diretory to %s\n", fk_str_raw(setting.dir));
}

/* should be reentrant */
void
fk_signal_exit_handler(int sig)
{
    /*
     * maybe some other processes in other modules, just like:
     * fk_a_signal_exit_handler(sig);
     * fk_b_signal_exit_handler(sig);
     */
    fk_svr_signal_exit_handler(sig);
}

//void
//fk_signal_child_handler(int sig)
//{
    /*
     * maybe some other process in other modules, just like:
     * fk_a_signal_child_handler(sig);
     * fk_b_signal_child_handler(sig);
     */
    //fk_svr_signal_child_handler(sig);
//}

void
fk_signal_register(void)
{
    int               rt;
    struct sigaction  sa;

    sa.sa_handler = fk_signal_exit_handler;
    sa.sa_flags = 0 ; /* SA_RESTART is not necessary here */
    /* no need to check the returned value when using sigxxxset */
    sigfillset(&sa.sa_mask); /* block all the signals when this handler is invoked */
    //sigemptyset(&sa.sa_mask);
    //sigaddset(&sa.sa_mask, SIGINT);
    //sigaddset(&sa.sa_mask, SIGTERM);
    //sigaddset(&sa.sa_mask, SIGKILL); /* attempt of blocking SIGKILL is silently ignored */

    /*
     * two ways of terminating this process gracefully
     * 1. SIGINT : ctl - c
     * 2. SIGTERM: kill / killall
     * among the two signals, SIGTERM should be the first and the standard
     * choice to terminate a process gracefully
     * SIGKILL is just a last resort for killing a runaway process which do
     * not respond to SIGTERM, so we do not catch SIGKILL here
     */
    rt = sigaction(SIGINT, &sa, NULL);
    if (rt < 0) {
        fk_log_error("sigaction for SIGINT failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    rt = sigaction(SIGTERM, &sa, NULL);
    if (rt < 0) {
        fk_log_error("sigaction for SIGTERM failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    //rt = sigaction(SIGQUIT, &sa, NULL); /* keep the default to get the core dump file */
    //rt = sigaction(SIGKILL, &sa, NULL); /* changing the disposition of SIGKILL is not allowed */

    //sa.sa_handler = fk_signal_child_handler;
    //sa.sa_flags = SA_RESTART;
    //sigfillset(&sa.sa_mask); /* block all the signals when this handler is invoked */
    //rt = sigaction(SIGCHLD, &sa, NULL);
    //if (rt < 0) {
        //fk_log_error("sigaction for SIGCHLD failed: %s\n", strerror(errno));
        //exit(EXIT_FAILURE);
    //}
}

void
fk_main_init(char *conf_path)
{
    /* where is the best place to call fk_set_seed()?? */
    fk_set_seed();

    /* the first module to init */
    fk_conf_init(conf_path);

    /* must be called before fk_log_init(), but why??? */
    fk_daemonize();

    fk_set_pwd();

    fk_setrlimit();

    /*
     * the second to init, so that all the ther module can call fk_log_xxx()
     */
    fk_log_init(fk_str_raw(setting.log_path), setting.log_level);

    fk_create_pidfile();

    fk_signal_register();

    fk_cache_init();

    fk_item_init();

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
