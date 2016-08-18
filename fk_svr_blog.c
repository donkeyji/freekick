/* c standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* unix headers */
#include <unistd.h>
#include <fcntl.h>

/* local headers */
#include <fk_conf.h>
#include <fk_mem.h>
#include <fk_item.h>
#include <fk_svr.h>
#include <fk_log.h>

static int fk_blog_read_data(fk_conn_t *conn);

void
fk_blog_init(void)
{
    FILE  *fp;

    if (setting.blog_on != 1) {
        return;
    }

    /* if blog file does not exist, create it */
    fp = fopen(fk_str_raw(setting.blog_file), "a+");
    if (fp == NULL) {
        fk_log_error("open blog failed\n");
        exit(EXIT_FAILURE);
    }

    /* update the global server */
    server.blog_fp = fp;
    server.blog_conn = fk_conn_create(FK_CONN_FAKE_FD);
}

int
fk_blog_read_data(fk_conn_t *conn)
{
    int      fd;
    char    *free_buf;
    size_t   free_len;
    ssize_t  rlen;

    /* translate file stream to file descriptor */
    fd = fileno(server.blog_fp);

    while (1) {
        if (fk_buf_payload_len(conn->rbuf) > (3 * (fk_buf_len(conn->rbuf) >> 2))) {
            fk_buf_shift(conn->rbuf);
        }
        if (fk_buf_free_len(conn->rbuf) == 0) {
            fk_buf_stretch(conn->rbuf);
        }
        /* rbuf is reaching the max, but probably data left unread in the blog */
        if (fk_buf_free_len(conn->rbuf) == 0) {
            return FK_SVR_OK;
        }

        free_buf = fk_buf_free_start(conn->rbuf);
        free_len = fk_buf_free_len(conn->rbuf);

        rlen = read(fd, free_buf, free_len);
        if (rlen == 0) { /* 0 indicates the end of the file */
            fk_log_info("blog read completed\n");
            return FK_SVR_DONE;
        } else if (rlen < 0) {
            return FK_SVR_ERR;
        } else {
            fk_buf_high_inc(conn->rbuf, rlen);
            if (rlen < free_len) {
                /* FK_SVR_DONE could not be used here */
                return FK_SVR_OK; /* indicates that all data has been read */
            } else {
                continue;
            }
        }
    }
}

/* probably using int as return value is better */
void
fk_blog_load(void)
{
    int         rt;
    FILE       *fp;
    fk_conn_t  *conn;

    conn = server.blog_conn;
    fp = server.blog_fp;

    /* after calling fopen(), the stream is positioned at the end of the file */
    fseek(fp, 0, SEEK_SET);

    while (1) {
        /* read data from blog file, and write them to blog_conn->rbuf */
        rt = fk_blog_read_data(conn);
        if (rt == FK_SVR_ERR) {
            return;
        } else if (rt == FK_SVR_DONE) {
            break;
        }

        while (fk_buf_payload_len(conn->rbuf) > 0) {
            rt = fk_conn_parse_req(conn);
            if (rt == FK_SVR_ERR) {
                return;
            } else if (rt == FK_SVR_AGAIN) {
                break;
            }

            rt = fk_conn_proc_cmd(conn);
            if (rt == FK_SVR_ERR) {
                return;
            }
        }

        fk_buf_shrink(conn->rbuf);
        fk_vtr_shrink(conn->arg_vtr);
    }
}

void
fk_blog_append(fk_conn_t *conn)
{
    int         i, argc;
    FILE       *fp;
    char       *arg;
    size_t      len;
    fk_str_t   *arg_str;
    fk_vtr_t   *argv;
    fk_item_t  *arg_itm;

    argc = conn->arg_cnt;
    argv = conn->arg_vtr;
    fp = server.blog_fp;

    /* dump arguments number */
    fprintf(fp, "*%d\r\n", argc);

    /* dump all the arguments individually */
    for (i = 0; i < argc; i++) {
        arg_itm = fk_vtr_get(argv, i);
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
