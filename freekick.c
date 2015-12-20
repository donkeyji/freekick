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
#include <freekick.h>

/* ---------------------------------------------------- */
static void fk_main_init(char *conf_path);
static void fk_main_final();
static void fk_main_cycle();

static void fk_daemonize();
static void fk_set_pwd();
static void fk_signal_reg();
static void fk_setrlimit();
static void fk_set_seed();
static void fk_write_pid_file();


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
	/* the type "rlim_t" has different size in linux from mac,
	 * so just convert "rlim_t" to "unsigned long long"
	 */
	fk_log_info("original file number limit: rlim_cur = %llu, rlim_max = %llu\n", (unsigned long long)lmt.rlim_cur, (unsigned long long)lmt.rlim_max);

	if (max_files > lmt.rlim_max) {
		euid = geteuid();
		if (euid == 0) {/* root */
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
		} else {/* non-root */
#ifdef FK_DEBUG
			fk_log_info("running as non-root\n");
#endif
			max_files = lmt.rlim_max;/* open as many as possible files */
			lmt.rlim_cur = lmt.rlim_max;
			rt = setrlimit(RLIMIT_NOFILE, &lmt);
			if (rt < 0) {
				fk_log_error("setrlimit: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			/* set current limit to the original setting */
			setting.max_conn = fk_util_files_to_conns(max_files);
			fk_log_info("change the setting.max_conn according the current file number limit: %u\n", setting.max_conn);
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

void fk_signal_reg()
{
	int rt;
	struct sigaction sa;

	sa.sa_handler = fk_svr_sigint;
	sa.sa_flags = 0;
	rt = sigemptyset(&sa.sa_mask);
	if (rt < 0) {
	}
	rt = sigaction(SIGINT, &sa, 0);
	if (rt < 0) {
	}

	sa.sa_handler = fk_svr_sigchld;
	sa.sa_flags = 0;
	rt = sigemptyset(&sa.sa_mask);
	if (rt < 0) {
	}
	rt = sigaction(SIGCHLD, &sa, 0);
	if (rt < 0) {
	}
}

void fk_main_init(char *conf_path)
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

	fk_write_pid_file();

	fk_signal_reg();

	fk_cache_init();

	fk_ev_init(fk_util_conns_to_files(setting.max_conn));

	fk_svr_init();
}

void fk_main_cycle()
{
	fk_ev_cycle();
}

void fk_main_final()
{
	int rt;

	if (setting.dump != 1) {
		return;
	}

	/* maybe child process is running */
	rt = wait(NULL);
	if (rt < 0) {
		if (errno == ECHILD) {
			fprintf(stdout, "no child process now\n");
			return;
		}
		fprintf(stderr, "%s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* save db once more */
	rt = fk_fkdb_save();
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

	return 0;
}
