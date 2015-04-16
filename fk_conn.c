#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include <fk_macro.h>
#include <fk_log.h>
#include <fk_conn.h>
#include <fk_mem.h>
#include <fk_sock.h>
#include <freekick.h>//it's OK to do so

static int fk_conn_read_cb(int fd, unsigned char type, void *ext);
static int fk_conn_write_cb(int fd, unsigned char type, void *ext);
static int fk_conn_timer_cb(int interval, unsigned char type, void *ext);
static void fk_conn_args_free(fk_conn *conn);
static int fk_conn_req_parse(fk_conn *conn);
static int fk_conn_data_recv(fk_conn *conn);
static int fk_conn_cmd_proc(fk_conn *conn);
static int fk_conn_rsp_send(fk_conn *conn);

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

	/*
	for (i = 0; i < FK_ARG_MAX; i++) {
		conn->args_len[i] = 0;
		conn->args[i] = NULL;
	}
	*/
	conn->args = fk_vtr_create();
	conn->arg_cnt = 0;
	conn->arg_idx = 0;
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

	fk_conn_args_free(conn);//free args first
	fk_vtr_destroy(conn->args);//then free vector
	//for (i = 0; i < FK_ARG_MAX; i++) {
		//if (conn->args[i] != NULL) {
			//fk_str_destroy(conn->args[i]);
		//}
	//}

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
	FK_BUF_SHRINK(conn->rbuf);
	if (FK_BUF_FREE_LEN(conn->rbuf) <= conn->rbuf->len / 4) {
		FK_BUF_SHIFT(conn->rbuf);
	}
	if (FK_BUF_FREE_LEN(conn->rbuf) == 0) {
		FK_BUF_STRETCH(conn->rbuf);
	}
#ifdef FK_DEBUG
	fk_log_debug("[after rbuf adjust]rbuf->low: %d, rbuf->high: %d, rbuf->len: %d\n", conn->rbuf->low, conn->rbuf->high, conn->rbuf->len);
#endif

	free_buf = FK_BUF_FREE_START(conn->rbuf);
	free_len = FK_BUF_FREE_LEN(conn->rbuf);
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
		FK_BUF_HIGH_INC(conn->rbuf, recv_len);
#ifdef FK_DEBUG
		fk_log_debug("[after recv]rbuf->low: %d, rbuf->high: %d\n", conn->rbuf->low, conn->rbuf->high);
#endif
		return 0;
	}
	return 0;
}

int fk_conn_req_parse(fk_conn *conn)
{
	char *p;
	fk_buf *rbuf;
	int len, start, end;

	rbuf = conn->rbuf;

#ifdef FK_DEBUG
	fk_log_debug("[before parsing] low: %d, high: %d\n", rbuf->low, rbuf->high);
#endif

	if (conn->arg_cnt == 0) {
		if (FK_BUF_VALID_LEN(rbuf) > 0) {
			start = rbuf->low;
			if (rbuf->data[start] != '*') {
				fk_log_debug("wrong client data\n");
				return -1;
			}
			p = memchr(FK_BUF_VALID_START(rbuf), '\n', FK_BUF_VALID_LEN(rbuf));
			if (p == NULL) {
				return 0;
			}
			end = p - rbuf->data;
			if (rbuf->data[end - 1] != '\r') {
				fk_log_debug("wrong client data\n");
				return -1;
			}
			conn->arg_cnt = atoi(rbuf->data + start + 1);
			if (conn->arg_cnt == 0) {
				fk_log_debug("wrong client data\n");
				return -1;
			}
#ifdef FK_DEBUG
			fk_log_debug("[cnt parsed]: %d\n", conn->arg_cnt);
#endif
			//check whether need to stretch
			//FK_VTR_STRETCH(conn->args, conn->arg_cnt);
			//check whether need to shrink
			//FK_VTR_SHRINK(conn->args, conn->arg_cnt);
			FK_VTR_ADJUST(conn->args, conn->arg_cnt);

			FK_BUF_LOW_INC(rbuf, end - start + 1);
		}
	}

	//while (rbuf->low < rbuf->high) {
	while (FK_BUF_VALID_LEN(rbuf) > 0) {
		start = rbuf->low;
		if (rbuf->data[start] != '$') {
			fk_log_debug("wrong client data\n");
			return -1;
		}
		//end = fk_buf_locate_char(rbuf, start + 1, '\n');
		p = memchr(FK_BUF_VALID_START(rbuf), '\n', FK_BUF_VALID_LEN(rbuf));
		if (p == NULL) {
			return 0;
		}
		end = p - rbuf->data;
		//if (end == -1) {//not found
			//return 0;//not received yet
		//}
		if (rbuf->data[end - 1] != '\r') {
			fk_log_debug("wrong client data\n");
			return -1;
		}
		len = atoi(rbuf->data + start + 1);//arg len
		if (len == 0) {
			fk_log_debug("wrong client data\n");
			return -1;
		}
		if (rbuf->high > end + len + 2) {//arg data available
			//conn->args[conn->arg_idx] = fk_str_create(rbuf->data + end + 1, len);
			//conn->args_len[conn->arg_idx] = len;
			FK_CONN_ARG(conn, conn->arg_idx) = fk_str_create(rbuf->data + end + 1, len);
			conn->arg_idx += 1;
			FK_BUF_LOW_INC(rbuf, end + len + 2 - start + 1);
		} else {//not received yet
			break;
		}

		if (conn->arg_cnt == conn->arg_idx) {//a total protocol has been parsed
			conn->parse_done = 1;
			break;
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
		if (FK_CONN_ARG(conn, i) != NULL) {
			fk_str_destroy(FK_CONN_ARG(conn, i));
			FK_CONN_ARG(conn, i) = NULL;
			//conn->args_len[i] = 0;
		}
	}
	conn->arg_cnt = 0;
	conn->parse_done = 0;
	conn->arg_idx = 0;
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
	fk_str_2upper(FK_CONN_ARG(conn, 0));
	pto = fk_proto_search(FK_CONN_ARG(conn, 0));
	if (pto == NULL) {
		fk_log_error("invalid protocol: %s\n", FK_STR_RAW(FK_CONN_ARG(conn, 0)));
		fk_conn_args_free(conn);
		return -1;
	}
	if (pto->arg_cnt != FK_PROTO_VARLEN && pto->arg_cnt != conn->arg_cnt) {
		fk_log_error("wrong argument number\n");
		fk_conn_args_free(conn);
		return -1;
	}
#ifdef FK_DEBUG
	fk_log_debug("[before adjust wbuf] low: %d, high: %d\n", conn->wbuf->low, conn->wbuf->high);
#endif
	if (FK_BUF_FREE_LEN(conn->wbuf) <= conn->wbuf->len / 4) {
		FK_BUF_SHIFT(conn->wbuf);
	}
	if (FK_BUF_FREE_LEN(conn->wbuf) == 0) {
		FK_BUF_STRETCH(conn->wbuf);
	}
	if (FK_BUF_FREE_LEN(conn->wbuf) == 0) {
		fk_log_info("wbuf beyond max len\n");
		return -1;//need to close this connection
	}
#ifdef FK_DEBUG
	fk_log_debug("[after adjust wbuf] low: %d, high: %d\n", conn->wbuf->low, conn->wbuf->high);
#endif
	rt = pto->handler(conn);
	if (rt < 0) {//args are not consumed, free all the args
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

	rt = fk_conn_req_parse(conn);
	if (rt < 0) {//error when parsing
		fk_log_error("error when parsing\n");
		fk_svr_conn_remove(conn);
		return 0;
	}

	rt = fk_conn_cmd_proc(conn);
	if (rt < 0) {
		fk_log_error("error when cmd proc\n");
		fk_svr_conn_remove(conn);
		return 0;
	}

	rt = fk_conn_rsp_send(conn);
	if (rt < 0) {
		fk_log_error("error when rsp send\n");
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
	buf = FK_BUF_VALID_START(conn->wbuf);
	len = FK_BUF_VALID_LEN(conn->wbuf);

#ifdef FK_DEBUG
	fk_log_debug("write callback]wbuf  buf: %s, valid len: %d, low: %d, high: %d\n", buf, len, conn->wbuf->low, conn->wbuf->high);
#endif
	rt = fk_sock_send(fd, buf, len, 0);
	if (rt < 0) {
		fk_log_error("send error\n");
		return -1;
	}

	FK_BUF_LOW_INC(conn->wbuf, rt);

	FK_BUF_SHRINK(conn->wbuf);

	//if all the data in wbuf is sent, remove the write ioev
	//but donot destroy the write ioev
	if (FK_BUF_VALID_LEN(conn->wbuf) == 0) {
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
	//fk_log_debug("[wbuf data]: %s\n", FK_BUF_VALID_START(wbuf));
#endif
	//if any data in write buf and never add write ioev yet
	if (FK_BUF_VALID_LEN(wbuf) > 0 && conn->write_added == 0) {
		fk_ev_ioev_add(conn->write_ev);
		conn->write_added = 1;
	}
	return 0;
}
