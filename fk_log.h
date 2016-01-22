#ifndef _FK_LOG_H_
#define _FK_LOG_H_

#include <stdio.h>

#include <fk_str.h>

/* log level */
#define FK_LOG_ERROR 	0
#define FK_LOG_WARN 	1
#define FK_LOG_INFO 	2
#define FK_LOG_DEBUG 	3

typedef struct _fk_log {
	int log_level;
	FILE *log_file;
} fk_log;

void fk_log_init(char *log_path, int log_level);
void fk_log_error(char *fmt, ...);
void fk_log_warn(char *fmt, ...);
void fk_log_info(char *fmt, ...);
void fk_log_debug(char *fmt, ...);

#endif
