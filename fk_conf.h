#ifndef _FK_CONF_H_
#define _FK_CONF_H_

#include <stdint.h>

#include <fk_str.h>

#define FK_CONF_OK		0
#define FK_CONF_ERR		-1

typedef struct _fk_conf {
	uint16_t port;
	fk_str *addr;
	int daemon;
	unsigned max_conn;
	time_t timeout;
	int log_level;
	fk_str *log_path;
	fk_str *pid_path;
	int dump;
	fk_str *db_file;
	fk_str *dir;
	unsigned dbcnt;
	size_t max_wbuf;
	int blog_on;
	fk_str *blog_path;
} fk_conf;

extern fk_conf setting;

void fk_conf_init(char *conf_path);

#endif
