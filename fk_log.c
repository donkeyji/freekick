/* c standard headers */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* local headers */
#include <fk_log.h>
#include <fk_mem.h>

#define FK_LOG_BUFF_SIZE 1024

#define fk_log_loggable(level)  do {            \
    /* beyond the current capacity of log */    \
    if ((level) > logger.log_level) {                \
        return;                                 \
    }                                           \
} while (0)

static void fk_log_output(int level, char *data);
static void fk_log_exec(int level, char *fmt, va_list ap);

static fk_log_t logger = {
    FK_LOG_DEBUG,
    NULL /* could not be "stdout" here, because "stdout" isn't a compile-time constant */
};

/*
 * fk_log_info/error/debug/warn can also be called without calling to
 * fk_log_init(), in which case the log is redirected to stdout
 */
void
fk_log_init(char *log_path, int log_level)
{
    FILE  *fp;

    fp = fopen(log_path, "a+");
    if (fp == NULL) {
        printf("null file\n");
        exit(1);
    }
    logger.log_level = log_level;
    logger.log_file = fp;
}

void
fk_log_error(char *fmt, ...)
{
    va_list ap;

    fk_log_loggable(FK_LOG_ERROR);

    va_start(ap, fmt);
    fk_log_exec(FK_LOG_ERROR, fmt, ap);
    va_end(ap);
}

void
fk_log_warn(char *fmt, ...)
{
    va_list ap;

    fk_log_loggable(FK_LOG_WARN);

    va_start(ap, fmt);
    fk_log_exec(FK_LOG_WARN, fmt, ap);
    va_end(ap);
}

void
fk_log_info(char *fmt, ...)
{
    va_list ap;

    fk_log_loggable(FK_LOG_INFO);

    va_start(ap, fmt);
    fk_log_exec(FK_LOG_INFO, fmt, ap);
    va_end(ap);
}

void
fk_log_debugx(char *fmt, ...)
{
    va_list ap;

    fk_log_loggable(FK_LOG_DEBUG);

    va_start(ap, fmt);
    fk_log_exec(FK_LOG_DEBUG, fmt, ap);
    va_end(ap);
}

void
fk_log_output(int level, char *data)
{
    char       *level_name;
    FILE       *fp;
    time_t      now;
    struct tm  *tm_now;

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
    /* obtain the number of seconds since the Epoch (UTC) */
    now = time(NULL);
    /*
     * convert a time_t value into a broken-down time corresponding to the
     * system's local time. The returned value is statically allocated, which
     * may be overwrote by another call to localtime()/gmtime()/ctime()/
     * asctime().
     */
    tm_now = localtime(&now);
    if (tm_now == NULL) {
        return;
    }

    fp = logger.log_file == NULL ? stdout : logger.log_file;
    fprintf(fp,
            "[%d-%.2d-%.2d %.2d:%.2d:%.2d]<%s>%s",
            tm_now->tm_year + 1900, /* year since 1900 */
            tm_now->tm_mon + 1,     /* month in the range of 0 to 11 */
            tm_now->tm_mday,        /* day of the month (1-31) */
            tm_now->tm_hour,        /* hours (0-23) */
            tm_now->tm_min,         /* minutes (0-59) */
            tm_now->tm_sec,         /* seconds (0-60) */
            level_name,
            data
    );

    /*
     * no buffering is preferable for logging
     * with log file, it's desirable to flush all the contents written to a disk
     * file ASAP, in case of a crash of the server when running
     */
    fflush(fp);
}

void
fk_log_exec(int level, char *fmt, va_list ap)
{
    char     log_buff[FK_LOG_BUFF_SIZE];

    /*
     * vsnprintf() is more safer than vsprintf(), since vsprintf() can probably
     * cause memory overflow
     */
    //vsprintf(log_buff, fmt, ap);
    vsnprintf(log_buff, FK_LOG_BUFF_SIZE, fmt, ap);

    fk_log_output(level, log_buff);
}
