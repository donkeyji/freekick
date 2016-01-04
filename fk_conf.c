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

typedef struct _fk_cfline {
	unsigned no;/* line number */
	unsigned cnt;/* the cnt field to parse */
	size_t len;
	char *buf;
	char err[FK_CONF_MAX_LEN];
	fk_str *fields[FK_CONF_MAX_FIELDS];
} fk_cfline;

typedef struct _fk_dtv {/* directive */
	char *name;
	unsigned field_cnt;
	int (*handler) (fk_cfline *line);
} fk_dtv;

static fk_cfline *fk_cfline_create();
static void fk_cfline_destroy(fk_cfline *line);

static int fk_conf_parse_file(char *conf_path);
static int fk_conf_read_line(fk_cfline *line, FILE *fp);
static int fk_conf_parse_line(fk_cfline *line);
static int fk_conf_proc_line(fk_cfline *line);
static void fk_conf_reset_line(fk_cfline *line);
static fk_dtv *fk_conf_search(fk_str *name);

static int fk_conf_parse_port(fk_cfline *line);
static int fk_conf_parse_daemon(fk_cfline *line);
static int fk_conf_parse_dump(fk_cfline *line);
static int fk_conf_parse_pidpath(fk_cfline *line);
static int fk_conf_parse_logpath(fk_cfline *line);
static int fk_conf_parse_maxconn(fk_cfline *line);
static int fk_conf_parse_dbcnt(fk_cfline *line);
static int fk_conf_parse_addr(fk_cfline *line);
static int fk_conf_parse_loglevel(fk_cfline *line);
static int fk_conf_parse_dbfile(fk_cfline *line);
static int fk_conf_parse_timeout(fk_cfline *line);
static int fk_conf_parse_dir(fk_cfline *line);

static fk_dtv dtv_map[] = {
	{"port", 2, fk_conf_parse_port},
	{"daemon", 1, fk_conf_parse_daemon},
	{"pidpath", 2, fk_conf_parse_pidpath},
	{"logpath", 2, fk_conf_parse_logpath},
	{"dump", 1, fk_conf_parse_dump},
	{"dbfile", 2, fk_conf_parse_dbfile},
	{"dir", 2, fk_conf_parse_dir},
	{"loglevel", 2, fk_conf_parse_loglevel},
	{"maxconn", 2, fk_conf_parse_maxconn},
	{"dbcnt", 2, fk_conf_parse_dbcnt},
	{"timeout", 2, fk_conf_parse_timeout},
	{"addr", 2, fk_conf_parse_addr},
	{NULL, 0, NULL}
};

/* global variable, referencd by other modules */
fk_conf setting;

fk_cfline *fk_cfline_create()
{
	fk_cfline *line = (fk_cfline *)fk_mem_alloc(sizeof(fk_cfline));
	line->no = 0;/* line number */
	line->cnt = 0;
	line->buf = NULL;
	line->len = 0;
	bzero(line->err, FK_CONF_MAX_LEN);
	bzero(line->fields, sizeof(fk_str *) * FK_CONF_MAX_FIELDS);
	return line;
}

void fk_cfline_destroy(fk_cfline *line)
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
	setting.db_file = fk_str_create(FK_DEFAULT_DB_PATH, sizeof(FK_DEFAULT_DB_PATH) - 1);
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
	long tail;
	fk_cfline *line;
	unsigned line_num;

	fp = fopen(conf_path, "r");
	if (fp == NULL) {
		printf("failed to open config file\n");
		return FK_CONF_ERR;
	}
	fseek(fp, 0, SEEK_END);
	tail = ftell(fp);
	rewind(fp);/* fseek(fp, 0, SEEK_SET); */
	line_num = 0;
	line = fk_cfline_create();

	/* do not reach the end of the file */
	while (ftell(fp) != tail) {
		line_num++;/* it begins from 1, not 0 */
		fk_conf_reset_line(line);
		line->no = line_num;
		rt = fk_conf_read_line(line, fp);
		if (rt < 0) {
			printf("%s", line->err);
			return FK_CONF_ERR;
		}
		rt = fk_conf_parse_line(line);
		if (rt < 0) {
			printf("%s", line->err);
			return FK_CONF_ERR;
		}
		rt = fk_conf_proc_line(line);
		if (rt < 0) {
			printf("%s", line->err);
			return FK_CONF_ERR;
		}
	}

	fk_cfline_destroy(line);

	fclose(fp);

	return FK_CONF_OK;
}

int fk_conf_read_line(fk_cfline *line, FILE *fp)
{
	int rt;

	rt = getline(&(line->buf), &(line->len), fp);
	/* no need to think about end-of-file here */
	if (rt < 0) {
		sprintf(line->err, "%s\n", strerror(errno));
		return FK_CONF_ERR;
	}
	return FK_CONF_OK;
}

int fk_conf_parse_line(fk_cfline *line)
{
	char *buf;
	size_t i, start, end;

	i = 0;
	buf = line->buf;

	while (buf[i] != '\n') {
		if (line->cnt == FK_CONF_MAX_FIELDS) {
			sprintf(line->err, "the max fields of one line should not be more than %d, line: %d\n", FK_CONF_MAX_FIELDS, line->no);
			return FK_CONF_ERR;
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

	return FK_CONF_OK;
}

fk_dtv *fk_conf_search(fk_str *name)
{
	unsigned i;

	for (i = 0; dtv_map[i].name != NULL; i++) {
		if (strcasecmp(fk_str_raw(name), dtv_map[i].name) == 0) {
			return &dtv_map[i];
		}
	}
	return NULL;
}

int fk_conf_proc_line(fk_cfline *line)
{
	int rt;
	fk_str *cmd;
	fk_dtv *dtv;

	if (line->cnt == 0) {/* the current line is a comment or empty line */
		return FK_CONF_OK;
	}

	cmd = line->fields[0];
	dtv = fk_conf_search(cmd);
	if (dtv == NULL) {
		sprintf(line->err, "no this cmd: %s, line: %d\n", fk_str_raw(cmd), line->no);
		return FK_CONF_ERR;
	}
	if (dtv->field_cnt != line->cnt) {
		sprintf(line->err, "field cnt wrong. line: %d\n", line->no);
		return FK_CONF_ERR;
	}
	rt = dtv->handler(line);
	if (rt < 0) {
		return FK_CONF_ERR;
	}

	return FK_CONF_OK;
}

void fk_conf_reset_line(fk_cfline *line)
{
	unsigned i;

	line->no = 0;
	line->cnt = 0;
	line->buf = NULL;
	line->len = 0;
	for (i = 0; i < FK_CONF_MAX_FIELDS; i++) {
		if (line->fields[i] != NULL) {
			fk_str_destroy(line->fields[i]);
			line->fields[i] = NULL;
		}
	}
}

int fk_conf_parse_port(fk_cfline *line)
{
	int rt, port;

	rt = fk_str_is_positive(line->fields[1]);
	if (rt == 0) {/* not a positive integer */
		sprintf(line->err, "port is not a valid number. line: %d\n", line->no);
		return FK_CONF_ERR;
	}
	/* 
	 * port of integer type can hold the 0 ~ 65535, if the
	 * line->fileds[1] beyond the range which an integer 
	 * could hold atoi() will return a minus value 
	 */
	port = atoi(fk_str_raw(line->fields[1]));
	if (port <= 0 || port > 65535) {
		sprintf(line->err, "port is not a valid number. line: %d\n", line->no);
		return FK_CONF_ERR;
	}
	setting.port = (uint16_t)port;
	return FK_CONF_OK;
}

int fk_conf_parse_daemon(fk_cfline *line)
{
	setting.daemon = 1;
	return FK_CONF_OK;
}

int fk_conf_parse_dump(fk_cfline *line)
{
	setting.dump = 1;
	return FK_CONF_OK;
}

int fk_conf_parse_pidpath(fk_cfline *line)
{
	setting.pid_path = fk_str_clone(line->fields[1]);
	return FK_CONF_OK;
}

int fk_conf_parse_logpath(fk_cfline *line)
{
	setting.log_path = fk_str_clone(line->fields[1]);
	return FK_CONF_OK;
}

int fk_conf_parse_dbfile(fk_cfline *line)
{
	setting.db_file = fk_str_clone(line->fields[1]);
	return FK_CONF_OK;
}

int fk_conf_parse_maxconn(fk_cfline *line)
{
	int rt, max_conn;

	rt = fk_str_is_positive(line->fields[1]);
	if (rt == 0) {/* not a positive integer */
		sprintf(line->err, "maxconn is not a valid number. line: %d\n", line->no);
		return FK_CONF_ERR;
	}
	max_conn = atoi(fk_str_raw(line->fields[1]));
	setting.max_conn = (unsigned)max_conn;
	return FK_CONF_OK;
}

int fk_conf_parse_dbcnt(fk_cfline *line)
{
	int rt, dbcnt;

	rt = fk_str_is_positive(line->fields[1]);
	if (rt == 0) {/* not a positive integer */
		sprintf(line->err, "dbcnt is not a valid number. line: %d\n", line->no);
		return FK_CONF_ERR;
	}
	dbcnt = atoi(fk_str_raw(line->fields[1]));
	setting.dbcnt = (unsigned)dbcnt;
	return FK_CONF_OK;
}

int fk_conf_parse_addr(fk_cfline *line)
{
	setting.addr = fk_str_clone(line->fields[1]);
	return FK_CONF_OK;
}

int fk_conf_parse_loglevel(fk_cfline *line)
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
		return FK_CONF_ERR;
	}
	return FK_CONF_OK;
}

int fk_conf_parse_timeout(fk_cfline *line)
{
	int rt, timeout;

	rt = fk_str_is_positive(line->fields[1]);
	if (rt == 0) {/* not a positive integer */
		sprintf(line->err, "dbcnt is not a valid number. line: %d\n", line->no);
		return FK_CONF_ERR;
	}
	timeout = atoi(fk_str_raw(line->fields[1]));
	setting.timeout = (time_t)timeout;

	return FK_CONF_OK;
}

int fk_conf_parse_dir(fk_cfline *line)
{
	setting.dir = fk_str_clone(line->fields[1]);
	return FK_CONF_OK;
}
