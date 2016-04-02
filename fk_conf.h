#ifndef _FK_CONF_H_
#define _FK_CONF_H_

/* c standard library headers */
#include <stdint.h>

/* unix headers */
#include <sys/types.h>

/* local headers */
#include <fk_str.h>

#define FK_CONF_OK      0
#define FK_CONF_ERR     -1

typedef struct {
    uint16_t     port;
    fk_str_t    *addr;
    int          daemon;
    int          max_conns;
    time_t       timeout;
    int          log_level;
    fk_str_t    *log_path;
    fk_str_t    *pid_path;
    int          dump;
    fk_str_t    *db_file;
    fk_str_t    *dir;
    uint32_t     dbcnt;
    size_t       max_wbuf;
    int          blog_on;
    fk_str_t    *blog_file;
} fk_conf_t;

extern fk_conf_t setting;

void fk_conf_init(char *conf_path);

#endif
