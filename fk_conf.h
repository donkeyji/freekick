#ifndef _FK_CONF_H_
#define _FK_CONF_H_

#include <stdint.h>

#include <fk_str.h>

typedef struct _fk_conf {
	uint16_t port;
	fk_str *addr;
	int daemon;
	unsigned max_conn;
	int log_level;
	fk_str *log_path;
	fk_str *pid_path;
	fk_str *db_path;
	unsigned dbcnt;
} fk_conf;

extern fk_conf setting;

void fk_conf_init(char *conf_path);

#endif
