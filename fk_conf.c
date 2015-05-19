#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <sys/types.h>

#include <fk_conf.h>
#include <fk_macro.h>
#include <fk_mem.h>
#include <fk_str.h>

#define FK_CONF_MAX_FIELDS 	5
#define FK_CONF_MAX_LEN 	1024

typedef struct _fk_line {
	unsigned no;/* line number */
	unsigned cnt;/* the cnt field to parse */
	size_t len;
	char *buf;
	char err[FK_CONF_MAX_LEN];
	fk_str *fields[FK_CONF_MAX_FIELDS];
} fk_line;

typedef struct _fk_itv {/* instructive */
	char *name;
	unsigned field_cnt;
	int (*handler) (fk_line *line);
} fk_itv;

static int fk_conf_parse_file(char *conf_path);
static int fk_conf_line_read(fk_line *line, FILE *fp);
static int fk_conf_line_parse(fk_line *line);
static int fk_conf_line_proc(fk_line *line);
static void fk_conf_line_reset(fk_line *line);
static fk_itv *fk_conf_search(fk_str *name);

static int fk_conf_handle_port(fk_line *line);
static int fk_conf_handle_daemon(fk_line *line);
static int fk_conf_handle_dump(fk_line *line);
static int fk_conf_handle_pidpath(fk_line *line);
static int fk_conf_handle_logpath(fk_line *line);
static int fk_conf_handle_maxconn(fk_line *line);
static int fk_conf_handle_dbcnt(fk_line *line);
static int fk_conf_handle_addr(fk_line *line);
static int fk_conf_handle_loglevel(fk_line *line);
static int fk_conf_handle_dbpath(fk_line *line);
static int fk_conf_handle_timeout(fk_line *line);
static int fk_conf_handle_dir(fk_line *line);

static fk_itv itv_map[] = {
	{"port", 2, fk_conf_handle_port},
	{"daemon", 1, fk_conf_handle_daemon},
	{"pidpath", 2, fk_conf_handle_pidpath},
	{"logpath", 2, fk_conf_handle_logpath},
	{"dump", 1, fk_conf_handle_dump},
	{"dbpath", 2, fk_conf_handle_dbpath},
	{"dir", 2, fk_conf_handle_dir},
	{"loglevel", 2, fk_conf_handle_loglevel},
	{"maxconn", 2, fk_conf_handle_maxconn},
	{"dbcnt", 2, fk_conf_handle_dbcnt},
	{"timeout", 2, fk_conf_handle_timeout},
	{"addr", 2, fk_conf_handle_addr},
	{NULL, 0, NULL}
};

/* global variable, referencd by other modules */
fk_conf setting;

fk_line *fk_conf_line_create()
{
	fk_line *line = (fk_line *)fk_mem_alloc(sizeof(fk_line));
	line->no = 0;/* line number */
	line->cnt = 0;
	line->buf = NULL;
	line->len = 0;
	//bzero(line->buf, FK_CONF_MAX_LEN);
	bzero(line->err, FK_CONF_MAX_LEN);
	bzero(line->fields, sizeof(fk_str *) * FK_CONF_MAX_FIELDS);
	return line;
}

void fk_conf_line_destroy(fk_line *line)
{
	unsigned i;

	if (line->buf != NULL) {
		free(line->buf);/* could not use fk_mem_free */
	}
	for (i = 0; i < FK_CONF_MAX_FIELDS; i++) {
		if (line->fields[i] != NULL) {
			fk_str_destroy(line->fields[i]);
		}
	}
	fk_mem_free(line);
}

void fk_conf_init(char *conf_path)
{
	int rt;
	/* step 1: set default first */
	setting.port = FK_DEFAULT_PORT;
	setting.daemon = FK_DEFAULT_DAEMON;
	setting.max_conn = FK_DEFAULT_MAX_CONN;
	setting.log_level = FK_DEFAULT_LOG_LEVEL;
	setting.dbcnt = FK_DEFAULT_DB_CNT;
	setting.log_path = fk_str_create(FK_DEFAULT_LOG_PATH, sizeof(FK_DEFAULT_LOG_PATH) - 1);
	setting.pid_path = fk_str_create(FK_DEFAULT_PID_PATH, sizeof(FK_DEFAULT_PID_PATH) - 1);
	setting.dump = FK_DEFAULT_DUMP;
	setting.db_path = fk_str_create(FK_DEFAULT_DB_PATH, sizeof(FK_DEFAULT_DB_PATH) - 1);
	setting.addr = fk_str_create(FK_DEFAULT_SVR_ADDR, sizeof(FK_DEFAULT_SVR_ADDR) - 1);
	setting.timeout = FK_DEFAULT_CONN_TIMEOUT;
	setting.dir = fk_str_create(FK_DEFAULT_DIR, sizeof(FK_DEFAULT_DIR) - 1);

	/* setp 2: parse config file */
	if (conf_path != NULL) {
		rt = fk_conf_parse_file(conf_path);
		if (rt < 0) {
			printf("parsing config failed\n");
			exit(EXIT_FAILURE);
		}
	}
}

int fk_conf_parse_file(char *conf_path)
{
	int rt;
	FILE *fp;
	fk_line *line;
	unsigned line_num;

	fp = fopen(conf_path, "r");
	if (fp == NULL) {
		printf("failed to open config file\n");
		return -1;
	}
	line_num = 0;
	line = fk_conf_line_create();

	while (1) {
		line_num++;/* it begins from 1, not 0 */
		fk_conf_line_reset(line);
		line->no = line_num;
		rt = fk_conf_line_read(line, fp);
		if (rt > 0) {/* end of file */
			break;
		} else if (rt < 0) {
			printf("%s", line->err);
			return -1;
		}
		rt = fk_conf_line_parse(line);
		if (rt < 0) {
			printf("%s", line->err);
			return -1;
		}
		rt = fk_conf_line_proc(line);
		if (rt < 0) {
			printf("%s", line->err);
			return -1;
		}
	}

	fk_conf_line_destroy(line);

	fclose(fp);

	return 0;
}

/*
int fk_conf_line_read(fk_line *line, FILE *fp)
{
	char *p;

	p = fgets(line->buf, FK_CONF_MAX_LEN, fp);
	if (p == NULL) {
		if (feof(fp) != 1) {//error occur
			sprintf(line->err, "error occurs when read line %d\n", line->no);
			return -1;
		}
		return 1;//file end
	}

	return 0;
}
*/

int fk_conf_line_read(fk_line *line, FILE *fp)
{
	int rt;

	rt = getline(&(line->buf), &(line->len), fp);
	if (rt < 0) {
		if (feof(fp) != 1) {
			sprintf(line->err, "%s\n", strerror(errno));
			return -1;
		} else {
			return 1;
		}
	}
	return 0;
}

int fk_conf_line_parse(fk_line *line)
{
	char *buf;
	size_t i, start, end;

	i = 0;
	buf = line->buf;

	while (buf[i] != '\n') {
		if (line->cnt == FK_CONF_MAX_FIELDS) {
			sprintf(line->err, "the max fields of one line should not be more than %d, line: %d\n", FK_CONF_MAX_FIELDS, line->no);
			return -1;
		}
		while (buf[i] == ' ' || buf[i] == '\t') {/* fint the first no empty character */
			i++;
		}
		if (buf[i] == '#' || buf[i] == '\n') {/* this line end */
			break;
		}
		start = i;
		end = i;
		/* find a completed token */
		while (buf[end] != ' ' && buf[end] != '\t' && 
			   buf[end] != '\n' && buf[end] != '#') 
		{
			end++;
		}
		line->fields[line->cnt] = fk_str_create(buf + start, (size_t)(end - start));
		line->cnt += 1;/* a new token found */

		i = end;
	}

	return 0;
}

fk_itv *fk_conf_search(fk_str *name)
{
	unsigned i;

	for (i = 0; itv_map[i].name != NULL; i++) {
		if (strcasecmp(fk_str_raw(name), itv_map[i].name) == 0) {
			return &itv_map[i];
		}
	}
	return NULL;
}

int fk_conf_line_proc(fk_line *line)
{
	int rt;
	fk_str *cmd;
	fk_itv *itv;

	if (line->cnt == 0) {//the current line is a comment or empty line
		return 0;
	}

	cmd = line->fields[0];
	itv = fk_conf_search(cmd);
	if (itv == NULL) {
		sprintf(line->err, "no this cmd: %s, line: %d\n", fk_str_raw(cmd), line->no);
		return -1;
	}
	if (itv->field_cnt != line->cnt) {
		sprintf(line->err, "field cnt wrong. line: %d\n", line->no);
		return -1;
	}
	rt = itv->handler(line);
	if (rt < 0) {
		return -1;
	}

	return 0;
}

void fk_conf_line_reset(fk_line *line)
{
	unsigned i;

	line->no = 0;
	line->cnt = 0;
	line->buf = NULL;
	line->len = 0;
	//bzero(line->buf, FK_CONF_MAX_LEN);
	for (i = 0; i < FK_CONF_MAX_FIELDS; i++) {
		if (line->fields[i] != NULL) {
			fk_str_destroy(line->fields[i]);
			line->fields[i] = NULL;
		}
	}
}

int fk_conf_handle_port(fk_line *line)
{
	int rt, port;

	rt = fk_str_is_positive(line->fields[1]);
	if (rt == 0) {//not a positive integer
		sprintf(line->err, "port is not a valid number. line: %d\n", line->no);
		return -1;
	}
	/* port of integer type can hold the 0 ~ 65535, if the
	 * line->fileds[1] beyond the range which an integer 
	 * could hold atoi() will return a minus value */
	port = atoi(fk_str_raw(line->fields[1]));
	if (port <= 0 || port > 65535) {
		sprintf(line->err, "port is not a valid number. line: %d\n", line->no);
		return -1;
	}
	setting.port = (uint16_t)port;
	return 0;
}

int fk_conf_handle_daemon(fk_line *line)
{
	setting.daemon = 1;
	return 0;
}

int fk_conf_handle_dump(fk_line *line)
{
	setting.dump = 1;
	return 0;
}

int fk_conf_handle_pidpath(fk_line *line)
{
	setting.pid_path = fk_str_clone(line->fields[1]);
	return 0;
}

int fk_conf_handle_logpath(fk_line *line)
{
	setting.log_path = fk_str_clone(line->fields[1]);
	return 0;
}

int fk_conf_handle_dbpath(fk_line *line)
{
	setting.db_path = fk_str_clone(line->fields[1]);
	return 0;
}

int fk_conf_handle_maxconn(fk_line *line)
{
	int rt, max_conn;

	rt = fk_str_is_positive(line->fields[1]);
	if (rt == 0) {/* not a positive integer */
		sprintf(line->err, "maxconn is not a valid number. line: %d\n", line->no);
		return -1;
	}
	max_conn = atoi(fk_str_raw(line->fields[1]));
	//if (max_conn == 0) {
		//sprintf(line->err, "maxconn is not a valid number. line: %d\n", line->no);
		//return -1;
	//}
	setting.max_conn = (unsigned)max_conn;
	return 0;
}

int fk_conf_handle_dbcnt(fk_line *line)
{
	int rt, dbcnt;

	rt = fk_str_is_positive(line->fields[1]);
	if (rt == 0) {/* not a positive integer */
		sprintf(line->err, "dbcnt is not a valid number. line: %d\n", line->no);
		return -1;
	}
	dbcnt = atoi(fk_str_raw(line->fields[1]));
	//if (dbcnt == 0) {
		//sprintf(line->err, "dbcnt is not a valid number. line: %d\n", line->no);
		//return -1;
	//}
	setting.dbcnt = (unsigned)dbcnt;
	return 0;
}

int fk_conf_handle_addr(fk_line *line)
{
	setting.addr = fk_str_clone(line->fields[1]);
	return 0;
}

int fk_conf_handle_loglevel(fk_line *line)
{
	char *level;

	level = fk_str_raw(line->fields[1]);
	if (strcasecmp(level, "debug") == 0) {
		setting.log_level = FK_LOG_DEBUG;
	} else if (strcasecmp(level, "info") == 0) {
		setting.log_level = FK_LOG_INFO;
	} else if (strcasecmp(level, "warnning") == 0) {
		setting.log_level = FK_LOG_WARN;
	} else if (strcasecmp(level, "error") == 0) {
		setting.log_level = FK_LOG_ERROR;
	} else {
		sprintf(line->err, "no this log level: %s, line: %d", level, line->no);
		return -1;
	}
	return 0;
}

int fk_conf_handle_timeout(fk_line *line)
{
	int rt, timeout;

	rt = fk_str_is_positive(line->fields[1]);
	if (rt == 0) {/* not a positive integer */
		sprintf(line->err, "dbcnt is not a valid number. line: %d\n", line->no);
		return -1;
	}
	timeout = atoi(fk_str_raw(line->fields[1]));
	setting.timeout = (time_t)timeout;

	return 0;
}

int fk_conf_handle_dir(fk_line *line)
{
	setting.dir = fk_str_clone(line->fields[1]);
	return 0;
}
