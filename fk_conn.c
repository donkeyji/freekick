#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include <fk_macro.h>
#include <fk_log.h>
#include <fk_conn.h>
#include <fk_mem.h>
#include <fk_sock.h>
#include <fk_util.h>
#include <freekick.h>//it's OK to do so

static int fk_conn_read_cb(int fd, unsigned char type, void *ext);
static int fk_conn_write_cb(int fd, unsigned char type, void *ext);
static int fk_conn_timer_cb(int interval, unsigned char type, void *ext);
static void fk_conn_args_free(fk_conn *conn);
static int fk_conn_req_parse(fk_conn *conn);
static int fk_conn_data_recv(fk_conn *conn);
static int fk_conn_cmd_proc(fk_conn *conn);
static int fk_conn_rsp_send(fk_conn *conn);

/*response format*/
static char *rsp_status 	= "+%s\r\n";
static char *rsp_error 		= "-%s\r\n";
static char *rsp_int 		= ":%d\r\n";
static char *rsp_bulk		= "$%d\r\n";
static char *rsp_mbulk		= "*%d\r\n";
static char *rsp_content	= "%s\r\n" ;

fk_conn *fk_conn_create(int fd)
{
	fk_conn *conn;

	conn = (fk_conn *)fk_mem_alloc(sizeof(fk_conn));

	conn->fd = fd;
	conn->rbuf = fk_buf_create();
	conn->wbuf = fk_buf_create();
	conn->read_ev = fk_ev_ioev_create(fd, FK_EV_READ, conn, fk_conn_read_cb);
	fk_ev_ioev_add(conn->read_ev);
	conn->write_ev = fk_ev_ioev_create(fd, FK_EV_WRITE, conn, fk_conn_write_cb);
	conn->write_added = 0;
	conn->last_recv = time(NULL);//save the current time
	conn->timer = fk_ev_tmev_create(5000, FK_EV_CYCLE, conn, fk_conn_timer_cb);
	//fk_ev_tmev_add(conn->timer);

	conn->arg_vtr = fk_vtr_create();
	conn->len_vtr = fk_vtr_create();
	conn->arg_cnt = 0;
	conn->arg_idx = 0;
	conn->arg_idx_type = 0;//arg_len
	conn->parse_done = 0;

	conn->db_idx = 0;
	return conn;
}

void fk_conn_destroy(fk_conn *conn)
{
	//remove from freekick and unregister event from event manager
	fk_ev_ioev_remove(conn->read_ev);
	fk_ev_ioev_destroy(conn->read_ev);

	fk_buf_destroy(conn->rbuf);
	fk_buf_destroy(conn->wbuf);

	fk_conn_args_free(conn);//free arg_vtr first
	fk_vtr_destroy(conn->arg_vtr);//then free vector
	fk_vtr_destroy(conn->len_vtr);

	conn->last_recv = -1;
	fk_ev_tmev_remove(conn->timer);
	fk_ev_tmev_destroy(conn->timer);

	fk_sock_close(conn->fd);

	fk_mem_free(conn);//should be the last step
}

int fk_conn_data_recv(fk_conn *conn)
{
	char *free_buf;
	int free_len, recv_len;

#ifdef FK_DEBUG
	fk_log_debug("[before rbuf adjust]rbuf->low: %d, rbuf->high: %d, rbuf->len: %d\n", conn->rbuf->low, conn->rbuf->high, conn->rbuf->len);
#endif
	fk_buf_shrink(conn->rbuf);
	if (fk_buf_free_len(conn->rbuf) <= conn->rbuf->len / 4) {
		fk_buf_shift(conn->rbuf);
	}
	if (fk_buf_free_len(conn->rbuf) == 0) {
		fk_buf_stretch(conn->rbuf);
	}
#ifdef FK_DEBUG
	fk_log_debug("[after rbuf adjust]rbuf->low: %d, rbuf->high: %d, rbuf->len: %d\n", conn->rbuf->low, conn->rbuf->high, conn->rbuf->len);
#endif

	free_buf = fk_buf_free_start(conn->rbuf);
	free_len = fk_buf_free_len(conn->rbuf);
	if (free_len == 0) {
		fk_log_info("beyond max buf len\n");
		return -1;//need to close this connection
	}

	recv_len = fk_sock_recv(conn->fd, free_buf, free_len, 0);
#ifdef FK_DEBUG
	fk_log_debug("free len: %d, recv_len: %d\n", free_len, recv_len);
#endif
	conn->last_recv = time(NULL);

	if (recv_len == 0) {//conn disconnected
		fk_log_info("[conn socket closed] fd: %d\n", conn->fd);
		return -1;
	} else if (recv_len < 0) {
		if (errno != EAGAIN) {//nonblocking sock
			fk_log_error("[recv error] %s\n", strerror(errno));
			return -2;
		} else {
			return 0;
		}
	} else {//succeed
#ifdef FK_DEBUG
		fk_log_debug("[conn data] fd: %d, recv_len: %d, data: %s\n", conn->fd, recv_len, free_buf);
#endif
		fk_buf_high_inc(conn->rbuf, recv_len);
#ifdef FK_DEBUG
		fk_log_debug("[after recv]rbuf->low: %d, rbuf->high: %d\n", conn->rbuf->low, conn->rbuf->high);
#endif
		return 0;
	}
	return 0;
}

int fk_conn_req_parse(fk_conn *conn)
{
	int rt;
	fk_buf *rbuf;
	uintptr_t len;
	char *start, *end;

	rbuf = conn->rbuf;

#ifdef FK_DEBUG
	fk_log_debug("[before parsing] low: %d, high: %d\n", rbuf->low, rbuf->high);
#endif

	if (conn->arg_cnt == 0) {
		if (fk_buf_valid_len(rbuf) > 0) {
			start = fk_buf_valid_start(rbuf);
			if (*start != '*') {
				fk_log_debug("wrong client data\n");
				return -1;
			}
			end = memchr(start + 1, '\n', fk_buf_valid_len(rbuf) - 1);
			if (end == NULL) {
				return 0;
			}
			if (*(end - 1) != '\r') {
				fk_log_debug("wrong client data\n");
				return -1;
			}
			rt = fk_util_positive_check(start + 1, end - 1 - 1);
			if (rt < 0) {
				fk_log_debug("wrong client data\n");
				return -1;
			}
			conn->arg_cnt = atoi(start + 1);
			if (conn->arg_cnt == 0) {
				fk_log_debug("wrong client data\n");
				return -1;
			}
#ifdef FK_DEBUG
			fk_log_debug("[cnt parsed]: %d\n", conn->arg_cnt);
#endif
			fk_vtr_adjust(conn->arg_vtr, conn->arg_cnt);
			fk_vtr_adjust(conn->len_vtr, conn->arg_cnt);

			fk_buf_low_inc(rbuf, end - start + 1);
		}
	}

	while (fk_buf_valid_len(rbuf) > 0) {
		if (conn->arg_idx_type == 0) {
			start = fk_buf_valid_start(rbuf);
			if (*start != '$') {
				fk_log_debug("wrong client data\n");
				return -1;
			}
			end = memchr(start + 1, '\n', fk_buf_valid_len(rbuf) - 1);
			if (end == NULL) {
				return 0;
			}
			if (*(end - 1) != '\r') {
				fk_log_debug("wrong client data\n");
				return -1;
			}
			rt = fk_util_positive_check(start + 1, end - 1 - 1);
			if (rt < 0) {
				fk_log_debug("wrong client data\n");
				return -1;
			}
			len = atoi(start + 1);//arg len
			if (len == 0) {
				fk_log_debug("wrong client data\n");
				return -1;
			}
			fk_conn_arg_len(conn, conn->arg_idx) = (void *)len;
			conn->arg_idx_type = 1;//need to parse arg
			fk_buf_low_inc(rbuf, end - start + 1);
		}

		if (conn->arg_idx_type == 1) {
			start = fk_buf_valid_start(rbuf);
			len = (uintptr_t)fk_conn_arg_len(conn, conn->arg_idx);
			fk_log_debug("arg_len: %lu\n", len);

			if (fk_buf_valid_len(rbuf) >= len + 2) {
				if (*(start + len) != '\r' ||
					*(start + len + 1) != '\n') 
				{
					fk_log_debug("wrong client data\n");
					return -1;
				}
				fk_conn_arg(conn, conn->arg_idx) = fk_str_create(start, len);
				conn->arg_idx += 1;
				conn->arg_idx_type = 0;
				fk_buf_low_inc(rbuf, len + 2);
			} else {//not received yet
				break;
			}

			if (conn->arg_cnt == conn->arg_idx) {//a total protocol has been parsed
				conn->parse_done = 1;
				break;
			}
		}
	}

#ifdef FK_DEBUG
	fk_log_debug("[after parsing] low: %d, high: %d\n", rbuf->low, rbuf->high);
#endif

	return 0;
}

void fk_conn_args_free(fk_conn *conn)
{
	int i;

	for (i = 0; i < conn->arg_cnt; i++) {
		if (fk_conn_arg(conn, i) != NULL) {
			fk_str_destroy((fk_str *)fk_conn_arg(conn, i));
			fk_conn_arg(conn, i) = NULL;
		}
		fk_conn_arg_len(conn, i) = (void *)0;
	}
	conn->arg_cnt = 0;
	conn->parse_done = 0;
	conn->arg_idx = 0;
	conn->arg_idx_type = 0;
}

int fk_conn_cmd_proc(fk_conn *conn)
{
	int rt;
	fk_proto *pto;

	if (conn->parse_done == 0) {
#ifdef FK_DEBUG
		fk_log_debug("parse not completed yet\n");
#endif
		return 0;
	}
	fk_str_2upper((fk_str *)fk_conn_arg(conn, 0));
	pto = fk_proto_search((fk_str *)fk_conn_arg(conn, 0));
	if (pto == NULL) {
		fk_log_error("invalid protocol: %s\n", fk_str_raw((fk_str *)fk_conn_arg(conn, 0)));
		fk_conn_args_free(conn);
		fk_conn_rsp_add_error(conn, "Invalid Protocol", strlen("Invalid Protocol"));
		return 0;
	}
	if (pto->arg_cnt != FK_PROTO_VARLEN && pto->arg_cnt != conn->arg_cnt) {
		fk_log_error("wrong argument number\n");
		fk_conn_args_free(conn);
		fk_conn_rsp_add_error(conn, "Wrong Argument Number", strlen("Wrong Argument Number"));
		return 0;
	}
	rt = pto->handler(conn);
	if (rt < 0) {//arg_vtr are not consumed, free all the arg_vtr
		fk_conn_args_free(conn);
		return -1;
	}
	fk_conn_args_free(conn);
	return 0;
}

int fk_conn_timer_cb(int interval, unsigned char type, void *ext)
{
	time_t now;
	fk_conn *conn;

	FK_UNUSE(type);
	FK_UNUSE(interval);

	conn = (fk_conn *)ext;

	now = time(NULL);

	if (now - conn->last_recv > FK_CONN_TIMEOUT) {
		fk_svr_conn_remove(conn);
		return -1;//tell evmgr not to add this timer again
	}
	return 0;
}

// callback for conn read event
int fk_conn_read_cb(int fd, unsigned char type, void *ext)
{
	int rt;
	fk_conn *conn;

	FK_UNUSE(fd);
	FK_UNUSE(type);

	conn = (fk_conn *)ext;

	rt = fk_conn_data_recv(conn);
	if (rt == -1) {//conn closed
		fk_svr_conn_remove(conn);
		return 0;
	} else if (rt == -2) {//how to handle read error?????
		return 0;
	}

	/*
	 * maybe more than one complete protocol were received
	 * parse all the complete protocol received yet
	 */
	while (fk_buf_valid_len(conn->rbuf) > 0) {
		rt = fk_conn_req_parse(conn);
		if (rt < 0) {//error when parsing
			fk_log_error("fatal error occured when parsing protocol\n");
			fk_svr_conn_remove(conn);
			return 0;
		}
		if (conn->parse_done != 1) {
			break;
		}

		rt = fk_conn_cmd_proc(conn);
		if (rt < 0) {
			fk_log_error("fatal error occured when processing cmd\n");
			fk_svr_conn_remove(conn);
			return 0;
		}
	}

	rt = fk_conn_rsp_send(conn);
	if (rt < 0) {
		fk_log_error("fatal error occurs when sending response\n");
		fk_svr_conn_remove(conn);
		return 0;
	}

	return 0;
}

int fk_conn_write_cb(int fd, unsigned char type, void *ext)
{
	int rt, len;
	char *buf;
	fk_conn *conn;

	FK_UNUSE(type);

	conn = (fk_conn *)ext;
	buf = fk_buf_valid_start(conn->wbuf);
	len = fk_buf_valid_len(conn->wbuf);

#ifdef FK_DEBUG
	fk_log_debug("write callback]wbuf  buf: %s, valid len: %d, low: %d, high: %d\n", buf, len, conn->wbuf->low, conn->wbuf->high);
#endif
	rt = fk_sock_send(fd, buf, len, 0);
	if (rt < 0) {
		fk_log_error("send error\n");
		return -1;
	}

	fk_buf_low_inc(conn->wbuf, rt);

	fk_buf_shrink(conn->wbuf);

	//if all the data in wbuf is sent, remove the write ioev
	//but donot destroy the write ioev
	if (fk_buf_valid_len(conn->wbuf) == 0) {
		fk_ev_ioev_remove(conn->write_ev);
		conn->write_added = 0;
	}
	return 0;
}

int fk_conn_rsp_send(fk_conn *conn)
{
	fk_buf *wbuf;

	wbuf = conn->wbuf;
#ifdef FK_DEBUG
	//fk_log_debug("[wbuf data]: %s\n", fk_buf_valid_start(wbuf));
#endif
	//if any data in write buf and never add write ioev yet
	if (fk_buf_valid_len(wbuf) > 0 && conn->write_added == 0) {
		fk_ev_ioev_add(conn->write_ev);
		conn->write_added = 1;
	}
	return 0;
}

#define FK_CONN_WBUF_ADJUST(buf, len)	{		\
	if (fk_buf_free_len((buf)) < (len)) {		\
		fk_buf_shift((buf));					\
	}											\
	if (fk_buf_free_len((buf)) < (len)) {		\
		fk_buf_stretch((buf));					\
	}											\
	if (fk_buf_free_len((buf)) < (len)) {		\
		return -1;								\
	}											\
}

int fk_conn_rsp_add_int(fk_conn *conn, int num)
{
	int len;
	int tmp;

	tmp = abs(num);
	FK_UTIL_INT_LEN(tmp, len);
	len += 3;
	if (num < 0) {
		len += 1;//minus
	}

	FK_CONN_WBUF_ADJUST(conn->wbuf, len);

	sprintf(fk_buf_free_start(conn->wbuf), rsp_int, num);
	fk_buf_high_inc(conn->wbuf, len);

	return 0;
}

int fk_conn_rsp_add_status(fk_conn *conn, char *stat, int stat_len)
{
	int len;

	len = stat_len + 3;

	FK_CONN_WBUF_ADJUST(conn->wbuf, len);

	sprintf(fk_buf_free_start(conn->wbuf), rsp_status, stat);
	fk_buf_high_inc(conn->wbuf, len);

	return 0;
}

int fk_conn_rsp_add_error(fk_conn *conn, char *error, int error_len)
{
	int len;

	len = error_len + 3;

	FK_CONN_WBUF_ADJUST(conn->wbuf, len);

	sprintf(fk_buf_free_start(conn->wbuf), rsp_error, error);
	fk_buf_high_inc(conn->wbuf, len);

	return 0;
}

int fk_conn_rsp_add_bulk(fk_conn *conn, int bulk_len)
{
	int len;
	int tmp;

	tmp = abs(bulk_len);
	FK_UTIL_INT_LEN(tmp, len);
	len += 3;
	if (bulk_len < 0) {
		len += 1;//minus
	}

	FK_CONN_WBUF_ADJUST(conn->wbuf, len);

	sprintf(fk_buf_free_start(conn->wbuf), rsp_bulk, bulk_len);
	fk_buf_high_inc(conn->wbuf, len);

	return 0;
}

int fk_conn_rsp_add_mbulk(fk_conn *conn, int bulk_cnt)
{
	int len;
	int tmp;

	tmp = abs(bulk_cnt);
	FK_UTIL_INT_LEN(tmp, len);
	len += 3;
	if (bulk_cnt < 0) {
		len += 1;
	}

	FK_CONN_WBUF_ADJUST(conn->wbuf, len);

	sprintf(fk_buf_free_start(conn->wbuf), rsp_mbulk, bulk_cnt);
	fk_buf_high_inc(conn->wbuf, len);

	return 0;
}

int fk_conn_rsp_add_content(fk_conn *conn, char *content, int content_len)
{
	int len;

	len = content_len + 2;

	FK_CONN_WBUF_ADJUST(conn->wbuf, len);

	sprintf(fk_buf_free_start(conn->wbuf), rsp_content, content);
	fk_buf_high_inc(conn->wbuf, len);

	return 0;
}
