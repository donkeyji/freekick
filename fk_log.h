#ifndef _FK_LOG_H
#define _FK_LOG_H

#include <stdio.h>

#include <fk_str.h>

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
