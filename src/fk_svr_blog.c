/* c standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* unix headers */
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

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

    /*
     * actually this setting.blog_on has always been checked where
     * fk_blog_bgrewrite is invoked. for flexibility, the blog_on
     * should be checked outside this module where this interface
     * is called.
     */
    //if (setting.blog_on != 1) {
    //    return;
    //}

    /* when O_CREAT is specified, the 3rd argument mode is required */
    fd = open(fk_str_raw(setting.blog_file), O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd < 0) {
        fk_log_error("open blog failed\n");
        exit(EXIT_FAILURE);
    }

    /*
     * sufficient to accommodate an argument length plus a complete argument
     * e.g. $3\r\nSET\r\n
     * TODO: probably we can use the blog_conn.wbuf as the blog_buf
     */
    server.blog_buflen = FK_ARG_HIGHWAT + FK_ARG_LEN_LINE_HIGHWAT;
    server.blog_buf = (char *)fk_mem_alloc(server.blog_buflen);

    /* update the global server */
    server.blog_fd = fd;
    server.blog_conn = fk_conn_create(FK_CONN_FAKE_FD);
    /*
     * blog_conn->type is set to FK_CONN_FAKE in fk_conn_create
     */
    //fk_conn_set_type(server.blog_conn, FK_CONN_FAKE); /* cannot be omitted, very important */
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
        /* no need to distinguish EAGAIN */
        return FK_SVR_ERR;
    } else {
        fk_buf_high_inc(conn->rbuf, rlen);
        if (rlen < free_len) {
            return FK_SVR_DONE;
        } else {
            return FK_SVR_OK; /* indicates that all data has been read */
        }
    }
}

/* probably using int as return value is better */
void
fk_blog_load(void)
{
    int         rt, again;
    fk_conn_t  *conn;

    conn = server.blog_conn;

    again = 1;
    while (again == 1) {
        /*
         * read data from the blog file, and write them to blog_conn->rbuf
         * by contrast with fk_conn_recv_data(), fk_blog_read_data() has only 3
         * types of returned values, since reading data from a regular file
         * slightly differs from a socket.
         */
        rt = fk_blog_read_data(conn);
        if (rt == FK_SVR_ERR) {
            return;
        } else if (rt == FK_SVR_DONE) {
            again = 0;
        } else if (rt == FK_SVR_OK) {
            again = 1;
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
    int         i, argc, fd, plen, wlen;
    char       *arg, *blog_buf;
    size_t      len, blog_buflen;
    fk_str_t   *arg_str;
    fk_vtr_t   *argv;
    fk_item_t  *arg_itm;

    /*
     * actually this setting.blog_on has always been checked where
     * fk_blog_bgrewrite is invoked. for flexibility, the blog_on
     * should be checked outside this module where this interface
     * is called.
     */
    //if (setting.blog_on != 1) {
    //    return;
    //}

    argc = conn->arg_cnt;
    argv = conn->arg_vtr;
    fd = server.blog_fd;
    blog_buf = server.blog_buf;
    blog_buflen = server.blog_buflen;

    /* dump arguments number */
    plen = snprintf(blog_buf, blog_buflen, "*%d\r\n", argc);
    wlen = write(fd, blog_buf, (size_t)plen);

    /* dump all the arguments individually */
    for (i = 0; i < argc; i++) {
        arg_itm = fk_vtr_get(argv, i);
        arg_str = fk_item_raw(arg_itm);
        len = fk_str_len(arg_str);
        arg = fk_str_raw(arg_str);
        plen = snprintf(blog_buf, blog_buflen, "$%zu\r\n%s\r\n", len, arg);
        write(fd, blog_buf, (size_t)plen);
    }
}

int
fk_blog_rewrite(void)
{
    /* simulate doing some work by calling sleep() */
    sleep(6);
    return FK_SVR_OK;
}

void
fk_blog_bgrewrite(void)
{
    int    rt;
    pid_t  pid;

    /*
     * actually this setting.blog_on has always been checked where
     * fk_blog_bgrewrite is invoked. for flexibility, the blog_on
     * should be checked outside this module where this interface
     * is called.
     */
    //if (setting.blog_on != 1) {
    //    return;
    //}

    if (server.rewrite_pid != -1) {
        return;
    }

    fk_log_debug("to rewrite blog\n");

    pid = fork();
    switch (pid) {
    case 0:
        rt = fk_blog_rewrite();
        if (rt == FK_SVR_ERR) {
            fk_log_error("blog rewrite failed\n");
            _exit(EXIT_FAILURE);
        }
        fk_log_debug("rewrite child is exiting\n");
        _exit(EXIT_SUCCESS);

        break;
    case -1:
        /* no need to _exit() here, just return to the caller */
        fk_log_error("fork() for rewrite: %s\n", strerror(errno));
        break;
    default:
        server.rewrite_pid = pid; /* record this pid */
        fk_log_debug("new rewrite_pid: %ld\n", (long)pid);
        break;
    }
}
