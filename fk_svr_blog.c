/* c standard headers */
#include <stdio.h>
#include <stdlib.h>

/* unix headers */
#include <unistd.h>

/* local headers */
#include <fk_conf.h>
#include <fk_item.h>
#include <fk_svr.h>
#include <fk_log.h>


void
fk_blog_init(void)
{
    FILE  *fp;

    if (setting.blog_on != 1) {
        return;
    }

    fp = fopen(fk_str_raw(setting.blog_file), "w");
    if (fp == NULL) {
        fk_log_error("open blog failed\n");
        exit(EXIT_FAILURE);
    }

    server.blog_file = fp;
}

void
fk_blog_load(fk_str_t *blog_path)
{
}

void
fk_blog_append(int argc, fk_vtr_t *arg_vtr, fk_proto_t *pto)
{
    int         i;
    FILE       *fp;
    char       *arg;
    size_t      len;
    fk_str_t   *arg_str;
    fk_item_t  *arg_itm;

    /* no need to dump read protocol */
    if (pto->type == FK_PROTO_READ) {
        return;
    }

    fp = server.blog_file;

    /* dump arguments number */
    fprintf(fp, "*%d\r\n", argc);

    /* dump all the argument individually */
    for (i = 0; i < argc; i++) {
        arg_itm = fk_vtr_get(arg_vtr, i);
        arg_str = fk_item_raw(arg_itm);
        len = fk_str_len(arg_str);
        arg = fk_str_raw(arg_str);
        fprintf(fp, "$%zu\r\n", len);
        fprintf(fp, "%s\r\n", arg);
    }

    /*
     * flush data from user space buffer to kernel buffer after writing one
     * complete protocol
     */
    fflush(fp);
}
