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
    int  fd;

    if (setting.blog_on != 1) {
        return;
    }

    /* if blog file does not exist, create it */
    fd = open(fk_str_raw(setting.blog_file), O_CREAT | O_RDWR);
    if (fd < 0) {
        fk_log_error("open blog failed\n");
        exit(EXIT_FAILURE);
    }

    /* update the global server */
    server.blog_fd = fd;
    server.blog_conn = fk_conn_create(FK_CONN_FAKE_FD);
    fk_conn_set_type(server.blog_conn, FK_CONN_FAKE); /* cannot be omitted, very important */
}

int
fk_blog_read_data(fk_conn_t *conn)
{
    int      fd;
    char    *free_buf;
    size_t   free_len;
    ssize_t  rlen;

    fd = server.blog_fd;

    if (fk_buf_payload_len(conn->rbuf) > (3 * (fk_buf_len(conn->rbuf) >> 2))) {
        fk_buf_shift(conn->rbuf);
    }
    if (fk_buf_free_len(conn->rbuf) == 0) {
        fk_buf_stretch(conn->rbuf);
    }
    /* rbuf is reaching the max, but probably data left unread in the blog */
    if (fk_buf_free_len(conn->rbuf) == 0) {
        return FK_SVR_ERR;
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
        return FK_SVR_OK; /* indicates that all data has been read */
    }
}

/* probably using int as return value is better */
void
fk_blog_load(void)
{
    int         rt;
    fk_conn_t  *conn;

    conn = server.blog_conn;

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
    int         i, argc, fd;
    char       *arg;
    size_t      len;
    fk_str_t   *arg_str;
    fk_vtr_t   *argv;
    fk_item_t  *arg_itm;

    argc = conn->arg_cnt;
    argv = conn->arg_vtr;
    fd = server.blog_fd;

    /* dump arguments number */
    dprintf(fd, "*%d\r\n", argc);

    /* dump all the arguments individually */
    for (i = 0; i < argc; i++) {
        arg_itm = fk_vtr_get(argv, i);
        arg_str = fk_item_raw(arg_itm);
        len = fk_str_len(arg_str);
        arg = fk_str_raw(arg_str);
        dprintf(fd, "$%zu\r\n", len);
        dprintf(fd, "%s\r\n", arg);
    }
}
