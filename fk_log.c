#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include <fk_log.h>
#include <fk_mem.h>

#define FK_LOG_BUFF_SIZE 1024

#define fk_log_write(level)	{					\
	va_list ap;									\
	char log_buff[FK_LOG_BUFF_SIZE];			\
	if (level > logger.log_level) {				\
		return;									\
	}											\
	va_start(ap, fmt);							\
	vsprintf(log_buff, fmt, ap);				\
	va_end(ap);									\
	fk_log_fprint_str(level, log_buff);			\
}

static void fk_log_fprint_str(int level, char *data);

static fk_log_t logger = {
    FK_LOG_DEBUG,
    NULL/* could not be "stdout" here, because "stdout" is not a compile-time constant */
};

/*
 * if fk_log_init() is not invoked, fk_log_info/error/debug/warn can also be called,
 * when the log is redirected to stdout
 */
void fk_log_init(char *log_path, int log_level)
{
    FILE *fp;

    fp = fopen(log_path, "a+");
    if (fp == NULL) {
        printf("null file\n");
        exit(1);
    }
    logger.log_level = log_level;
    logger.log_file = fp;
}

void fk_log_error(char *fmt, ...)
{
    fk_log_write(FK_LOG_ERROR);
}

void fk_log_warn(char *fmt, ...)
{
    fk_log_write(FK_LOG_WARN);
}

void fk_log_info(char *fmt, ...)
{
    fk_log_write(FK_LOG_INFO);
}

void fk_log_debug(char *fmt, ...)
{
    fk_log_write(FK_LOG_DEBUG);
}

void fk_log_fprint_str(int level, char *data)
{
    time_t now;
    char *level_name;
    struct tm *tm_now;

    switch (level) {
    case FK_LOG_ERROR:
        level_name = "ERROR";
        break;
    case FK_LOG_WARN:
        level_name = "WARN";
        break;
    case FK_LOG_INFO:
        level_name = "INFO";
        break;
    case FK_LOG_DEBUG:
        level_name = "DEBUG";
        break;
    }
    now = time(NULL);
    tm_now = localtime(&now);
    if (tm_now == NULL) {
        return;
    }

    fprintf(logger.log_file == NULL ? stdout : logger.log_file,
            "[%d-%.2d-%.2d %.2d:%.2d:%.2d]<%s>%s",
            tm_now->tm_year + 1900,
            tm_now->tm_mon + 1,
            tm_now->tm_mday,
            tm_now->tm_hour,
            tm_now->tm_min,
            tm_now->tm_sec,
            level_name,
            data
           );
}
