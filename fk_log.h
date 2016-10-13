#ifndef _FK_LOG_H_
#define _FK_LOG_H_

/* c standard headers */
#include <stdio.h>

/* local headers */
#include <fk_str.h>

/* log level */
#define FK_LOG_ERROR    0
#define FK_LOG_WARN     1
#define FK_LOG_INFO     2
#define FK_LOG_DEBUG    3

typedef struct {
    int      log_level;
    FILE    *log_file;
} fk_log_t;

void fk_log_init(char *log_path, int log_level);
void fk_log_error(char *fmt, ...);
void fk_log_warn(char *fmt, ...);
void fk_log_info(char *fmt, ...);
void _fk_log_debug(char *fmt, ...);

/*
 * ##__VA_ARGS__ is an gcc extention for c99 variadic macro, which is used to
 * avoid the trailing comma when the ... argument is empty, but in this case gcc
 * will generate warning messages which could be suppressed by employing the gcc
 * option -Wno-variadic-macros which is not the default option
 * e.g.
 * fk_log_debug("hello\n");
 * the above line will generate a warning as follows:
 * warning: ISO C99 requires rest arguments to be used [enabled by default]
 */
#ifdef FK_DEBUG
#define fk_log_debug(fmt, ...) _fk_log_debug(fmt, ##__VA_ARGS__)
#else
#define fk_log_debug(fmt, ...)
#endif

#endif
