/* feature test macros */
#include <fk_ftm.h>

/* c standard headers */
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

/* unix headers */
#include <strings.h>
#include <sys/types.h>

/* local headers */
#include <fk_conf.h>
#include <fk_mem.h>
#include <fk_str.h>
#include <fk_log.h> /* FK_LOG_INFO & FK_LOG_ERR & FK_LOG_DEBUG & FK_LOG_WARN */

/* default setting */
#define FK_DEFAULT_MAX_CONN 		10240
#define FK_DEFAULT_LOG_PATH 		"/tmp/freekick.log"
#define FK_DEFAULT_LOG_LEVEL 		(FK_LOG_DEBUG)
#define FK_DEFAULT_PID_PATH 		"/tmp/freekick.pid"
#define FK_DEFAULT_DUMP				0
#define FK_DEFAULT_DB_PATH 			"/tmp/freekick.db"
#define FK_DEFAULT_PORT 			6379
#define FK_DEFAULT_DAEMON 			0
#define FK_DEFAULT_SVR_ADDR	 		"0.0.0.0"
#define FK_DEFAULT_DB_CNT 			1
#define FK_DEFAULT_CONN_TIMEOUT 	300 /* timeout of conn, unit: second */
#define FK_DEFAULT_DIR				"/tmp/"
#define FK_DEFAULT_MAX_WBUF			4096
#define FK_DEFAULT_BLOG_PATH		"/tmp/freekick.blog"
#define FK_DEFAULT_BLOG_ON		0

#define FK_CONF_MAX_FIELDS 	5
#define FK_CONF_MAX_LEN 	1024

typedef struct {
    uint32_t     no;                              /* line number */
    uint32_t     cnt;                             /* the cnt field to parse */
    size_t       len;
    char        *buf;
    char         err[FK_CONF_MAX_LEN];
    fk_str_t    *fields[FK_CONF_MAX_FIELDS];
} fk_cfline_t;

/* directive */
typedef struct {
    char      *name;
    uint32_t   field_cnt;
    int      (*handler) (fk_cfline_t *line);
} fk_dtv;

static fk_cfline_t *fk_cfline_create(void);
static void fk_cfline_destroy(fk_cfline_t *line);

static int fk_conf_parse_file(char *conf_path);
static int fk_conf_read_line(fk_cfline_t *line, FILE *fp);
static int fk_conf_parse_line(fk_cfline_t *line);
static int fk_conf_proc_line(fk_cfline_t *line);
static void fk_conf_reset_line(fk_cfline_t *line);
static fk_dtv *fk_conf_search(fk_str_t *name);

static int fk_conf_parse_port(fk_cfline_t *line);
static int fk_conf_parse_daemon(fk_cfline_t *line);
static int fk_conf_parse_dump(fk_cfline_t *line);
static int fk_conf_parse_pidpath(fk_cfline_t *line);
static int fk_conf_parse_logpath(fk_cfline_t *line);
static int fk_conf_parse_maxconn(fk_cfline_t *line);
static int fk_conf_parse_dbcnt(fk_cfline_t *line);
static int fk_conf_parse_addr(fk_cfline_t *line);
static int fk_conf_parse_loglevel(fk_cfline_t *line);
static int fk_conf_parse_dbfile(fk_cfline_t *line);
static int fk_conf_parse_timeout(fk_cfline_t *line);
static int fk_conf_parse_dir(fk_cfline_t *line);
static int fk_conf_parse_maxwbuf(fk_cfline_t *line);
static int fk_conf_parse_blogfile(fk_cfline_t *line);
static int fk_conf_parse_blogon(fk_cfline_t *line);

static fk_dtv dtv_map[] = {
    { "port", 2, fk_conf_parse_port },
    { "daemon", 1, fk_conf_parse_daemon },
    { "pidpath", 2, fk_conf_parse_pidpath },
    { "logpath", 2, fk_conf_parse_logpath },
    { "dump", 1, fk_conf_parse_dump },
    { "dbfile", 2, fk_conf_parse_dbfile },
    { "dir", 2, fk_conf_parse_dir },
    { "loglevel", 2, fk_conf_parse_loglevel },
    { "maxconn", 2, fk_conf_parse_maxconn },
    { "dbcnt", 2, fk_conf_parse_dbcnt },
    { "timeout", 2, fk_conf_parse_timeout },
    { "addr", 2, fk_conf_parse_addr },
    { "maxwbuf", 2, fk_conf_parse_maxwbuf },
    { "blogfile", 2, fk_conf_parse_blogfile },
    { "blogon", 1, fk_conf_parse_blogon },
    { NULL, 0, NULL }
};

/* global variable, referred to by other modules */
fk_conf_t setting;

fk_cfline_t *
fk_cfline_create(void)
{
    fk_cfline_t  *line;

    line = (fk_cfline_t *)fk_mem_alloc(sizeof(fk_cfline_t));
    line->no = 0; /* line number */
    line->cnt = 0;
    line->buf = NULL;
    line->len = 0;
    memset(line->err, 0, FK_CONF_MAX_LEN);
    memset(line->fields, 0, sizeof(fk_str_t *) * FK_CONF_MAX_FIELDS);
    return line;
}

void
fk_cfline_destroy(fk_cfline_t *line)
{
    unsigned  i;

    if (line->buf != NULL) {
        free(line->buf); /* could not use fk_mem_free */
    }
    for (i = 0; i < FK_CONF_MAX_FIELDS; i++) {
        if (line->fields[i] != NULL) {
            fk_str_destroy(line->fields[i]);
        }
    }
    fk_mem_free(line);
}

void
fk_conf_init(char *conf_path)
{
    int  rt;

    /* step 1: set default first */
    setting.port = FK_DEFAULT_PORT;
    setting.daemon = FK_DEFAULT_DAEMON;
    setting.max_conns = FK_DEFAULT_MAX_CONN;
    setting.log_level = FK_DEFAULT_LOG_LEVEL;
    setting.dbcnt = FK_DEFAULT_DB_CNT;
    setting.log_path = fk_str_create(FK_DEFAULT_LOG_PATH, sizeof(FK_DEFAULT_LOG_PATH) - 1);
    setting.pid_path = fk_str_create(FK_DEFAULT_PID_PATH, sizeof(FK_DEFAULT_PID_PATH) - 1);
    setting.dump = FK_DEFAULT_DUMP;
    setting.db_file = fk_str_create(FK_DEFAULT_DB_PATH, sizeof(FK_DEFAULT_DB_PATH) - 1);
    setting.addr = fk_str_create(FK_DEFAULT_SVR_ADDR, sizeof(FK_DEFAULT_SVR_ADDR) - 1);
    setting.timeout = FK_DEFAULT_CONN_TIMEOUT;
    setting.dir = fk_str_create(FK_DEFAULT_DIR, sizeof(FK_DEFAULT_DIR) - 1);
    setting.max_wbuf = FK_DEFAULT_MAX_WBUF;
    setting.blog_file = fk_str_create(FK_DEFAULT_BLOG_PATH, sizeof(FK_DEFAULT_DB_PATH) - 1);
    setting.blog_on = FK_DEFAULT_BLOG_ON;

    /* setp 2: parse config file */
    if (conf_path != NULL) {
        rt = fk_conf_parse_file(conf_path);
        if (rt < 0) {
            printf("parsing config failed\n");
            exit(EXIT_FAILURE);
        }
    }
}

int
fk_conf_parse_file(char *conf_path)
{
    int           rt;
    FILE         *fp;
    long          tail;
    uint32_t      line_num;
    fk_cfline_t  *line;

    fp = fopen(conf_path, "r");
    if (fp == NULL) {
        printf("failed to open config file\n");
        return FK_CONF_ERR;
    }
    fseek(fp, 0, SEEK_END);
    tail = ftell(fp);
    rewind(fp); /* fseek(fp, 0, SEEK_SET); */
    line_num = 0;
    line = fk_cfline_create();

    /* do not encounter the end of the file */
    while (ftell(fp) != tail) {
        line_num++; /* it begins from 1, not 0 */
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

int
fk_conf_read_line(fk_cfline_t *line, FILE *fp)
{
    int  rt;

    rt = getline(&(line->buf), &(line->len), fp);
    /* no need to think about end-of-file here */
    if (rt < 0) {
        /*
         * in order to avoid memory overflow, it is preferable to employ
         * snprintf() instead of sprintf(), with respect to the 'size' argument
         * determining the maximum length of the resulting formatted string
         */
        snprintf(line->err, FK_CONF_MAX_LEN, "%s\n", strerror(errno));
        return FK_CONF_ERR;
    }
    return FK_CONF_OK;
}

int
fk_conf_parse_line(fk_cfline_t *line)
{
    char   *buf;
    size_t  i, start, end;

    i = 0;
    buf = line->buf;

    while (buf[i] != '\n') {
        if (line->cnt == FK_CONF_MAX_FIELDS) {
            snprintf(line->err, FK_CONF_MAX_LEN, "the max fields of one line should not be more than %d, line: %"PRIu32"\n", FK_CONF_MAX_FIELDS, line->no);
            return FK_CONF_ERR;
        }
        while (buf[i] == ' ' || buf[i] == '\t') { /* fint the first no empty character */
            i++;
        }
        if (buf[i] == '#' || buf[i] == '\n') { /* this line end */
            break;
        }
        start = i;
        end = i;
        /* find a completed token */
        while (buf[end] != ' ' && buf[end] != '\t' &&
               buf[end] != '\n' && buf[end] != '#') {
            end++;
        }
        line->fields[line->cnt] = fk_str_create(buf + start, (size_t)(end - start));
        line->cnt += 1; /* a new token found */

        i = end;
    }

    return FK_CONF_OK;
}

fk_dtv *
fk_conf_search(fk_str_t *name)
{
    unsigned  i;

    /* no need to use dictionary */
    for (i = 0; dtv_map[i].name != NULL; i++) {
        if (strcasecmp(fk_str_raw(name), dtv_map[i].name) == 0) {
            return &dtv_map[i];
        }
    }
    return NULL;
}

int
fk_conf_proc_line(fk_cfline_t *line)
{
    int        rt;
    fk_dtv    *dtv;
    fk_str_t  *cmd;

    if (line->cnt == 0) { /* the current line is a comment or empty line */
        return FK_CONF_OK;
    }

    cmd = line->fields[0];
    dtv = fk_conf_search(cmd);
    if (dtv == NULL) {
        snprintf(line->err, FK_CONF_MAX_LEN, "no this cmd: %s, line: %"PRIu32"\n", fk_str_raw(cmd), line->no);
        return FK_CONF_ERR;
    }
    if (dtv->field_cnt != line->cnt) {
        snprintf(line->err, FK_CONF_MAX_LEN, "field cnt wrong. line: %"PRIu32"\n", line->no);
        return FK_CONF_ERR;
    }
    rt = dtv->handler(line);
    if (rt < 0) {
        return FK_CONF_ERR;
    }

    return FK_CONF_OK;
}

void
fk_conf_reset_line(fk_cfline_t *line)
{
    unsigned  i;

    line->no = 0;
    line->cnt = 0;
    if (line->buf != NULL) {
        free(line->buf); /* do not use fk_mem_free() here */
    }
    line->buf = NULL;
    line->len = 0;
    for (i = 0; i < FK_CONF_MAX_FIELDS; i++) {
        if (line->fields[i] != NULL) {
            fk_str_destroy(line->fields[i]);
            line->fields[i] = NULL;
        }
    }
}

int
fk_conf_parse_port(fk_cfline_t *line)
{
    int  rt, port;

    rt = fk_str_is_positive(line->fields[1]);
    if (rt == 0) { /* not a positive integer */
        snprintf(line->err, FK_CONF_MAX_LEN, "port is not a valid number. line: %"PRIu32"\n", line->no);
        return FK_CONF_ERR;
    }
    /*
     * a port of integer type can hold a range of 0 ~ 65535, if the
     * line->fileds[1] is beyond the range which an integer type could
     * hold atoi() will return a minus value
     */
    port = atoi(fk_str_raw(line->fields[1]));
    if (port <= 0 || port > 65535) {
        snprintf(line->err, FK_CONF_MAX_LEN, "port is not a valid number. line: %"PRIu32"\n", line->no);
        return FK_CONF_ERR;
    }
    setting.port = (uint16_t)port;
    return FK_CONF_OK;
}

int
fk_conf_parse_daemon(fk_cfline_t *line)
{
    setting.daemon = 1;
    return FK_CONF_OK;
}

int
fk_conf_parse_dump(fk_cfline_t *line)
{
    setting.dump = 1;
    return FK_CONF_OK;
}

int
fk_conf_parse_pidpath(fk_cfline_t *line)
{
    fk_str_destroy(setting.pid_path);
    setting.pid_path = fk_str_clone(line->fields[1]);
    return FK_CONF_OK;
}

int
fk_conf_parse_logpath(fk_cfline_t *line)
{
    fk_str_destroy(setting.log_path);
    setting.log_path = fk_str_clone(line->fields[1]);
    return FK_CONF_OK;
}

int
fk_conf_parse_dbfile(fk_cfline_t *line)
{
    fk_str_destroy(setting.db_file);
    setting.db_file = fk_str_clone(line->fields[1]);
    return FK_CONF_OK;
}

int
fk_conf_parse_maxconn(fk_cfline_t *line)
{
    int  rt;

    rt = fk_str_is_positive(line->fields[1]);
    if (rt == 0) { /* not a positive integer */
        snprintf(line->err, FK_CONF_MAX_LEN, "maxconn is not a valid number. line: %"PRIu32"\n", line->no);
        return FK_CONF_ERR;
    }
    setting.max_conns = atoi(fk_str_raw(line->fields[1]));
    return FK_CONF_OK;
}

int
fk_conf_parse_dbcnt(fk_cfline_t *line)
{
    int  rt, dbcnt;

    rt = fk_str_is_positive(line->fields[1]);
    if (rt == 0) { /* not a positive integer */
        snprintf(line->err, FK_CONF_MAX_LEN, "dbcnt is not a valid number. line: %"PRIu32"\n", line->no);
        return FK_CONF_ERR;
    }
    dbcnt = atoi(fk_str_raw(line->fields[1]));
    setting.dbcnt = (uint32_t)dbcnt;
    return FK_CONF_OK;
}

int
fk_conf_parse_addr(fk_cfline_t *line)
{
    fk_str_destroy(setting.addr);
    setting.addr = fk_str_clone(line->fields[1]);
    return FK_CONF_OK;
}

int
fk_conf_parse_loglevel(fk_cfline_t *line)
{
    char  *level;

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
        snprintf(line->err, FK_CONF_MAX_LEN, "no this log level: %s, line: %"PRIu32"", level, line->no);
        return FK_CONF_ERR;
    }
    return FK_CONF_OK;
}

int
fk_conf_parse_timeout(fk_cfline_t *line)
{
    int  rt, timeout;

    rt = fk_str_is_positive(line->fields[1]);
    if (rt == 0) { /* not a positive integer */
        snprintf(line->err, FK_CONF_MAX_LEN, "dbcnt is not a valid number. line: %"PRIu32"\n", line->no);
        return FK_CONF_ERR;
    }
    timeout = atoi(fk_str_raw(line->fields[1]));
    setting.timeout = (time_t)timeout;

    return FK_CONF_OK;
}

int
fk_conf_parse_dir(fk_cfline_t *line)
{
    fk_str_destroy(setting.dir); /* release the default value */
    setting.dir = fk_str_clone(line->fields[1]);
    return FK_CONF_OK;
}

int
fk_conf_parse_maxwbuf(fk_cfline_t *line)
{
    int  rt, maxwbuf;

    rt = fk_str_is_positive(line->fields[1]);
    if (rt == 0) { /* not a positive integer */
        snprintf(line->err, FK_CONF_MAX_LEN, "maxwbuf is not a valid number. line: %"PRIu32"\n", line->no);
        return FK_CONF_ERR;
    }
    maxwbuf = atoi(fk_str_raw(line->fields[1]));
    setting.max_wbuf = (size_t)maxwbuf;
    return FK_CONF_OK;
}

int
fk_conf_parse_blogfile(fk_cfline_t *line)
{
    fk_str_destroy(setting.blog_file);
    setting.blog_file = fk_str_clone(line->fields[1]);
    return FK_CONF_OK;
}

int
fk_conf_parse_blogon(fk_cfline_t *line)
{
    setting.blog_on = 1;
    return FK_CONF_OK;
}
