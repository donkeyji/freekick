#ifndef _FK_LOG_H
#define _FK_LOG_H

#include <stdio.h>

#include <fk_str.h>

typedef struct _fk_log {
	int log_level;
	FILE *log_file;
} fk_log;

void fk_log_init();
void fk_log_error(const char *fmt, ...);
void fk_log_warn(const char *fmt, ...);
void fk_log_info(const char *fmt, ...);
void fk_log_debug(const char *fmt, ...);

#endif
