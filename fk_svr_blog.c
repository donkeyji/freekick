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
    fp = fopen(fk_str_raw(setting.blog_file), "w");
    if (fp == NULL) {
        fk_log_error("open blog failed\n");
        exit(EXIT_FAILURE);
    }

    server.blog_fp = fp;
    server.blog_conn = fk_conn_create(FK_CONN_FAKE_FD);
    printf("=====%d\n", fileno(fp));
}

int
fk_blog_read_data(fk_conn_t *conn)
{
    int      fd;
    char    *free_buf;
    size_t   free_len;
    ssize_t  recv_len;

    fd = fileno(server.blog_fp);
    printf("===read fd: %d\n", fd);

    while (1) {
        if (fk_buf_payload_len(conn->rbuf) > (3 * (fk_buf_len(conn->rbuf) >> 2))) {
            fk_buf_shift(conn->rbuf);
        }
        if (fk_buf_free_len(conn->rbuf) == 0) {
            fk_buf_stretch(conn->rbuf);
        }
        /* reach the high water of read buffer */
        if (fk_buf_free_len(conn->rbuf) == 0) { /* could not receive data this time */
            return FK_SVR_ERR; /* go to the next step: parse request */
        }

        free_buf = fk_buf_free_start(conn->rbuf);
        free_len = fk_buf_free_len(conn->rbuf);

        printf("-----before fd: %d\n", fd);
        recv_len = read(fd, free_buf, free_len);
        printf("===recv_len: %ld\n", (long)recv_len);
        if (recv_len == 0) {
            fk_log_info("blog file read completed\n");
            return FK_SVR_OK;
        } else if (recv_len < 0) {
            if (errno != EAGAIN) {
                fk_log_error("[read error] %s\n", strerror(errno));
                return FK_SVR_ERR;
            } else {
                return FK_SVR_OK;
            }
        } else {
            fk_buf_high_inc(conn->rbuf, recv_len);
            if (recv_len < free_len) {
                return FK_SVR_OK;
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
    fk_conn_t  *conn;

    conn = server.blog_conn;
    /* read data from blog file, and write them to blog_conn->rbuf */
    rt = fk_blog_read_data(conn);

    while (fk_buf_payload_len(conn->rbuf) > 0) {
        rt = fk_conn_parse_req(conn);
        if (rt == FK_SVR_ERR) {
            fk_svr_remove_conn(conn);
            return;
        } else if (rt == FK_SVR_AGAIN) {
            break;
        }

        rt = fk_conn_proc_cmd(conn);
        if (rt == FK_SVR_ERR) {
            fk_svr_remove_conn(conn);
            return;
        }
    }
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

    fp = server.blog_fp;

    /* dump arguments number */
    fprintf(fp, "*%d\r\n", argc);

    /* dump all the arguments individually */
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
