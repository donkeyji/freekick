#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>

#include <unistd.h>
#include <sys/socket.h>

#include <fk_macro.h>
#include <fk_log.h>
#include <fk_mem.h>
#include <fk_sock.h>
#include <fk_util.h>
#include <fk_conf.h>
#include <fk_item.h>
#include <fk_svr.h>/* it's OK to do so */

static void fk_conn_read_cb(int fd, char type, void *ext);
static void fk_conn_write_cb(int fd, char type, void *ext);
static int fk_conn_timer_cb(unsigned interval, char type, void *ext);
static int fk_conn_parse_req(fk_conn *conn);
static int fk_conn_recv_data(fk_conn *conn);
static int fk_conn_proc_cmd(fk_conn *conn);
static int fk_conn_send_rsp(fk_conn *conn);

/* response format */
static const char *rsp_status 	= "+%s\r\n";
static const char *rsp_error 	= "-%s\r\n";
static const char *rsp_content	= "%s\r\n" ;
static const char *rsp_int 		= ":%d\r\n";
static const char *rsp_bulk		= "$%d\r\n";
static const char *rsp_mbulk	= "*%d\r\n";

fk_conn *fk_conn_create(int fd)
{
	fk_conn *conn;

	conn = (fk_conn *)fk_mem_alloc(sizeof(fk_conn));

	conn->fd = fd;
	conn->rbuf = fk_buf_create(FK_BUF_HIGHWAT);
	conn->wbuf = fk_buf_create(setting.max_wbuf);

	/* every fields should be initialized */
	conn->read_ev = NULL;
	conn->write_ev = NULL;
	conn->timer = NULL;
	if (fd != FK_CONN_FAKE_FD) {
		conn->read_ev = fk_ioev_create(fd, FK_IOEV_READ, conn, fk_conn_read_cb);
		fk_ev_add_ioev(conn->read_ev);

		conn->write_ev = fk_ioev_create(fd, FK_IOEV_WRITE, conn, fk_conn_write_cb);

		conn->timer = fk_tmev_create(5000, FK_TMEV_CYCLE, conn, fk_conn_timer_cb);
		fk_ev_add_tmev(conn->timer);
	}
	conn->write_added = 0;

	conn->last_recv = time(NULL);/* save the current time */

	conn->arg_vtr = fk_vtr_create();
	conn->len_vtr = fk_vtr_create();
	conn->arg_cnt = 0;
	conn->arg_idx = 0;
	conn->idx_flag = 0;
	conn->parse_done = 0;

	conn->db_idx = 0;
	return conn;
}

void fk_conn_destroy(fk_conn *conn)
{
	/* remove from freekick and unregister event from event manager */
	if (conn->fd != FK_CONN_FAKE_FD) {
		fk_ev_remove_ioev(conn->read_ev);
		fk_ioev_destroy(conn->read_ev);

		if (conn->write_added == 1) {
			fk_ev_remove_ioev(conn->write_ev);
		}
		fk_ioev_destroy(conn->write_ev);

		fk_ev_remove_tmev(conn->timer);
		fk_tmev_destroy(conn->timer);
	}
	conn->read_ev = NULL;
	conn->write_ev = NULL;
	conn->timer = NULL;
	conn->write_added = 0;

	fk_buf_destroy(conn->rbuf);
	fk_buf_destroy(conn->wbuf);

	fk_conn_free_args(conn);/* free arg_vtr first */
	fk_vtr_destroy(conn->arg_vtr);/* then free vector */
	fk_vtr_destroy(conn->len_vtr);

	conn->last_recv = -1;

	if (conn->fd != FK_CONN_FAKE_FD) {
		close(conn->fd);
	}

	fk_mem_free(conn);/* should be the last step */
}

void fk_svr_add_conn(int fd)
{
	fk_conn *conn;

	conn = fk_conn_create(fd);
	fk_conn_set_type(conn, FK_CONN_REAL);

	/* added to the map */
	server.conns_tab[conn->fd] = conn;
	server.conn_cnt += 1;
}

void fk_svr_remove_conn(fk_conn *conn)
{
	/* removed from the map */
	server.conns_tab[conn->fd] = NULL;
	server.conn_cnt -= 1;

	fk_conn_destroy(conn);
}

int fk_conn_recv_data(fk_conn *conn)
{
	char *free_buf;
	size_t free_len;
	ssize_t recv_len;

	/* 
	 * a complete line was not received, but the read buffer has reached
	 * its upper limit, so just close this connection 
	 */
	if (fk_buf_reach_highwat(conn->rbuf) &&
		fk_buf_free_len(conn->rbuf) == 0) 
	{
		fk_log_info("beyond max buffer length\n");
		return FK_SVR_ERR;/* need to close this connection */
	}

	while (1) {
#ifdef FK_DEBUG
		fk_log_debug("[before rbuf adjust]rbuf->low: %lu, rbuf->high: %lu, rbuf->len: %lu\n", fk_buf_low(conn->rbuf), fk_buf_high(conn->rbuf), fk_buf_len(conn->rbuf));
#endif
		if (fk_buf_free_len(conn->rbuf) < fk_buf_len(conn->rbuf) / 4) {
			fk_buf_shift(conn->rbuf);
		}
		if (fk_buf_free_len(conn->rbuf) == 0) {
			fk_buf_stretch(conn->rbuf);
		}
		if (fk_buf_free_len(conn->rbuf) == 0) {/* could not receive data this time */
			break;
		}
#ifdef FK_DEBUG
		fk_log_debug("[after rbuf adjust]rbuf->low: %lu, rbuf->high: %lu, rbuf->len: %lu\n", fk_buf_low(conn->rbuf), fk_buf_high(conn->rbuf), fk_buf_len(conn->rbuf));
#endif

		free_buf = fk_buf_free_start(conn->rbuf);
		free_len = fk_buf_free_len(conn->rbuf);

		recv_len = recv(conn->fd, free_buf, free_len, 0);
		if (recv_len == 0) {/* conn disconnected */
			fk_log_info("[conn socket closed] fd: %d\n", conn->fd);
			return FK_SVR_ERR;
		} else if (recv_len < 0) {
			if (errno != EAGAIN) {
				fk_log_error("[recv error] %s\n", strerror(errno));
				return FK_SVR_ERR;
			} else {/* no data left in the read buffer of the socket */
				break;
			}
		} else {/* succeed */
#ifdef FK_DEBUG
			fk_log_debug("[conn data] fd: %d, recv_len: %d, data: %s\n", conn->fd, recv_len, free_buf);
#endif
			conn->last_recv = time(NULL);
			fk_buf_high_inc(conn->rbuf, recv_len);
#ifdef FK_DEBUG
			fk_log_debug("[after recv]rbuf->low: %lu, rbuf->high: %lu\n", fk_buf_low(conn->rbuf), fk_buf_high(conn->rbuf));
#endif
			if (recv_len < free_len) {/* no extra data left */
				break;
			} else {/* maybe there is still data in socket buffer */
				continue;/* rbuf is full now */
			}
		}
	}

	return FK_SVR_OK;
}

int fk_conn_parse_req(fk_conn *conn)
{
	int rt, argl;
	fk_buf *rbuf;
	fk_item *itm;
	size_t arg_len;
	char *start, *end;

	rbuf = conn->rbuf;

#ifdef FK_DEBUG
	fk_log_debug("[before parsing] low: %lu, high: %lu\n", fk_buf_low(rbuf), fk_buf_high(rbuf));
#endif

	if (conn->arg_cnt == 0) {
		if (fk_buf_payload_len(rbuf) > 0) {
			start = fk_buf_payload_start(rbuf);
			if (*start != '*') {
				fk_log_debug("wrong client data\n");
				return FK_SVR_ERR;
			}
			end = memchr(start + 1, '\n', fk_buf_payload_len(rbuf) - 1);
			if (end == NULL) {
				return FK_SVR_AGAIN;
			}
			if (*(end - 1) != '\r') {
				fk_log_debug("wrong client data\n");
				return FK_SVR_ERR;
			}
			if (end - 2 < start + 1) {
				fk_log_debug("wrong client data\n");
				return FK_SVR_ERR;
			}
			rt = fk_util_is_positive_seq(start + 1, (size_t)(end - 2 - start));
			if (rt < 0) {
				fk_log_debug("wrong client data\n");
				return FK_SVR_ERR;
			}
			/* 
			 * a variable of integer type can hold the argument 
			 * count, because the FK_ARG_CNT_HIGHWAT == 128, 
			 * and INT_MAX > FK_ARG_CNT_HIGHWAT 
			 */
			conn->arg_cnt = atoi(start + 1);
			if (conn->arg_cnt <= 0 || conn->arg_cnt > FK_ARG_CNT_HIGHWAT) {
				fk_log_debug("invalid argument count\n");
				return FK_SVR_ERR;
			}
#ifdef FK_DEBUG
			fk_log_debug("[arg_cnt parsed]: %d\n", conn->arg_cnt);
			fk_log_debug("before arg_vtr stretch: len: %lu\n", fk_vtr_len(conn->arg_vtr));
			fk_log_debug("before len_vtr stretch: len: %lu\n", fk_vtr_len(conn->len_vtr));
#endif
			fk_vtr_stretch(conn->arg_vtr, (size_t)(conn->arg_cnt));
			fk_vtr_stretch(conn->len_vtr, (size_t)(conn->arg_cnt));
#ifdef FK_DEBUG
			fk_log_debug("after arg_vtr stretch: len: %lu\n", fk_vtr_len(conn->arg_vtr));
			fk_log_debug("after len_vtr stretch: len: %lu\n", fk_vtr_len(conn->len_vtr));
#endif

			fk_buf_low_inc(rbuf, (size_t)(end - start + 1));
		}
	}

	while (fk_buf_payload_len(rbuf) > 0) {
		if (conn->idx_flag == 0) {
			start = fk_buf_payload_start(rbuf);
			if (*start != '$') {
				fk_log_debug("wrong client data\n");
				return FK_SVR_ERR;
			}
			end = memchr(start + 1, '\n', fk_buf_payload_len(rbuf) - 1);
			if (end == NULL) {
				return FK_SVR_AGAIN;
			}
			if (*(end - 1) != '\r') {
				fk_log_debug("wrong client data\n");
				return FK_SVR_ERR;
			}
			if (end - 2 < start + 1) {
				fk_log_debug("wrong client data\n");
				return FK_SVR_ERR;
			}
			rt = fk_util_is_nonminus_seq(start + 1, (size_t)(end - 2 - start));
			if (rt < 0) {
				fk_log_debug("wrong client data\n");
				return FK_SVR_ERR;
			}
			/* 
			 * argl of integer type can hold the argument length,
			 * because the FK_ARG_HIGHWAT == (64 * 1024 - 2), and
			 * INT_MAX > FK_ARG_HIGHWAT 
			 */
			argl = atoi(start + 1);/* argument length */
			if (argl < 0 || argl > FK_ARG_HIGHWAT) {
				fk_log_debug("invalid argument length\n");
				return FK_SVR_ERR;
			}
			fk_conn_set_arglen(conn, conn->arg_idx, (void *)((size_t)argl));
			conn->idx_flag = 1;/* need to parse arg */
			fk_buf_low_inc(rbuf, (size_t)(end - start + 1));
		}

		if (conn->idx_flag == 1) {
			start = fk_buf_payload_start(rbuf);
			arg_len = (size_t)fk_conn_get_arglen(conn, conn->arg_idx);
#ifdef FK_DEBUG
			fk_log_debug("saved arg_len: %lu\n", arg_len);
#endif
			if (fk_buf_payload_len(rbuf) >= arg_len + 2) {
				if (*(start + arg_len) != '\r' ||
					*(start + arg_len + 1) != '\n') 
				{
					fk_log_debug("wrong client data\n");
					return FK_SVR_ERR;
				}
				itm = fk_item_create(FK_ITEM_STR, fk_str_create(start, arg_len));
				fk_conn_set_arg(conn, conn->arg_idx, itm);
				fk_item_inc_ref(itm);/* ref: from 0 to 1 */
				conn->arg_idx += 1;
				conn->idx_flag = 0;
				fk_buf_low_inc(rbuf, arg_len + 2);
			} else {/* not received yet */
				return FK_SVR_AGAIN;
			}

			if (conn->arg_cnt == conn->arg_idx) {/* a total protocol has been parsed */
				conn->parse_done = 1;
				return FK_SVR_OK;
			}
		}
	}

#ifdef FK_DEBUG
	fk_log_debug("[after parsing] low: %lu, high: %lu\n", fk_buf_low(rbuf), fk_buf_high(rbuf));
#endif

	return FK_SVR_AGAIN;
}

void fk_conn_free_args(fk_conn *conn)
{
	int i;
	fk_item *arg_itm;

	for (i = 0; i < conn->arg_cnt; i++) {
		arg_itm = fk_conn_get_arg(conn, i);
		/* maybe arg_itm == NULL at this time */
		if (arg_itm != NULL) {
			fk_item_dec_ref(arg_itm);/* just decrease the ref */
			fk_conn_set_arg(conn, i, NULL);/* do not ref to this item */
		}
		fk_conn_set_arglen(conn, i, (void *)0);
	}
	conn->arg_cnt = 0;
	conn->parse_done = 0;
	conn->arg_idx = 0;
	conn->idx_flag = 0;
}

int fk_conn_proc_cmd(fk_conn *conn)
{
	int rt;
	fk_str *cmd;
	fk_item *itm;
	fk_proto *pto;

	if (conn->parse_done == 0) {
#ifdef FK_DEBUG
		fk_log_debug("parse not completed yet\n");
#endif
		return FK_SVR_OK;
	}
	itm = (fk_item *)fk_conn_get_arg(conn, 0);
	cmd = (fk_str *)fk_item_raw(itm);
	fk_str_2upper(cmd);
	pto = fk_proto_search(cmd);
	if (pto == NULL) {
		fk_log_error("invalid protocol: %s\n", fk_str_raw((fk_str *)fk_conn_get_arg(conn, 0)));
		fk_conn_free_args(conn);
		rt = fk_conn_add_error_rsp(conn, "Invalid Protocol", strlen("Invalid Protocol"));
		if (rt == FK_SVR_ERR) {
			return FK_SVR_ERR;
		}
		return FK_SVR_OK;
	}

	/* when the argument number is variable */
	if (pto->arg_cnt < 0) {
		if (conn->arg_cnt < -pto->arg_cnt) {
			fk_log_error("wrong argument number\n");
			fk_conn_free_args(conn);
			rt = fk_conn_add_error_rsp(conn, "Wrong Argument Number", strlen("Wrong Argument Number"));
			if (rt == FK_SVR_ERR) {
				return FK_SVR_ERR;
			}
			return FK_SVR_OK;
		}
	}

	/* when the argument nuber is statle */
	if (pto->arg_cnt >= 0 && pto->arg_cnt != conn->arg_cnt) {
		fk_log_error("wrong argument number\n");
		fk_conn_free_args(conn);
		fk_conn_add_error_rsp(conn, "Wrong Argument Number", strlen("Wrong Argument Number"));
		if (rt == FK_SVR_ERR) {
			return FK_SVR_ERR;
		}
		return FK_SVR_OK;
	}
	rt = pto->handler(conn);
	if (rt == FK_SVR_ERR) {/* arg_vtr are not consumed, free all the arg_vtr */
		fk_conn_free_args(conn);
		return FK_SVR_ERR;
	}
	fk_conn_free_args(conn);
	return FK_SVR_OK;
}

int fk_conn_timer_cb(unsigned interval, char type, void *ext)
{
	time_t now;
	fk_conn *conn;

	fk_util_unuse(type);
	fk_util_unuse(interval);

	conn = (fk_conn *)ext;

	now = time(NULL);

	if (now - conn->last_recv > setting.timeout) {
		fk_log_debug("connection timeout\n");
		fk_svr_remove_conn(conn);
		return -1;/* tell evmgr not to add this timer again */
	}
	return 0;
}

/* 
 * callback for conn read event
 * evmgr do not care the return value of this callback
 * so this callback itself should handle all error occurs 
 */
void fk_conn_read_cb(int fd, char type, void *ext)
{
	int rt;
	fk_conn *conn;

	fk_util_unuse(fd);
	fk_util_unuse(type);

	conn = (fk_conn *)ext;

	rt = fk_conn_recv_data(conn);
	if (rt == FK_SVR_ERR) {/* conn closed */
		fk_svr_remove_conn(conn);
		return;
	}

	/* 
	 * maybe more than one complete protocol were received
	 * parse all the complete protocol received yet 
	 */
	while (fk_buf_payload_len(conn->rbuf) > 0) {
		rt = fk_conn_parse_req(conn);
		if (rt == FK_SVR_ERR) {/* error when parsing */
			fk_log_error("fatal error occured when parsing protocol\n");
			fk_svr_remove_conn(conn);
			return;
		} else if (rt == FK_SVR_AGAIN) {/* parsing not completed */
			break;
		}

		rt = fk_conn_proc_cmd(conn);
		if (rt == FK_SVR_ERR) {
			fk_log_error("fatal error occured when processing cmd\n");
			fk_svr_remove_conn(conn);
			return;
		}
	}

	rt = fk_conn_send_rsp(conn);
	if (rt == FK_SVR_ERR) {
		fk_log_error("fatal error occurs when sending response\n");
		fk_svr_remove_conn(conn);
		return;
	}

	return;
}

void fk_conn_write_cb(int fd, char type, void *ext)
{
	char *pbuf;
	size_t plen;
	fk_conn *conn;
	ssize_t sent_len;

	fk_util_unuse(type);

	conn = (fk_conn *)ext;

#ifdef FK_DEBUG
	fk_log_debug("data to sent: paylen: %d, data: %s\n", fk_buf_payload_len(conn->wbuf), fk_buf_payload_start(conn->wbuf));
#endif

	while (fk_buf_payload_len(conn->wbuf) > 0) {
		plen = fk_buf_payload_len(conn->wbuf);
		pbuf = fk_buf_payload_start(conn->wbuf);
#ifdef FK_DEBUG
		fk_log_debug("[before send]low: %lu, high: %lu\n", fk_buf_low(conn->wbuf), fk_buf_high(conn->wbuf));
#endif

		sent_len = send(fd, pbuf, plen, 0);
		if (sent_len < 0) {
			if (errno != EAGAIN) {
				fk_log_error("send error: %s\n", strerror(errno));
				fk_svr_remove_conn(conn);/* close the connection directly */
				return;
			} else {/* no free space in this write buffer of the socket */
				break;
			}
		}
		fk_buf_low_inc(conn->wbuf, (size_t)(sent_len));
#ifdef FK_DEBUG
		fk_log_debug("[after send]low: %lu, high: %lu\n", fk_buf_low(conn->wbuf), fk_buf_high(conn->wbuf));
#endif
	}

	fk_buf_shrink(conn->wbuf);

	/* 
	 * if all the data in wbuf is sent, remove the write ioev
	 * but donot destroy the write ioev 
	 */
	if (fk_buf_payload_len(conn->wbuf) == 0) {
		fk_ev_remove_ioev(conn->write_ev);
		conn->write_added = 0;
	}

	return;
}

int fk_conn_send_rsp(fk_conn *conn)
{
	fk_buf *wbuf;

	/* maybe it's not so good to shrink vtr/buf here */
	fk_buf_shrink(conn->rbuf);
	fk_vtr_shrink(conn->arg_vtr);
	fk_vtr_shrink(conn->len_vtr);

	wbuf = conn->wbuf;
	/* if any data in write buf and never add write ioev yet */
	if (fk_buf_payload_len(wbuf) > 0 && conn->write_added == 0) {
		fk_ev_add_ioev(conn->write_ev);
		conn->write_added = 1;
	}
	return FK_SVR_OK;
}

int fk_conn_add_status_rsp(fk_conn *conn, char *stat, size_t stat_len)
{
	size_t len;

	len = stat_len + 3;

	fk_buf_adjust(conn->wbuf, len);
	if (fk_buf_free_len(conn->wbuf) < len) {
		return FK_SVR_ERR;
	}

	sprintf(fk_buf_free_start(conn->wbuf), rsp_status, stat);
	fk_buf_high_inc(conn->wbuf, len);

	return FK_SVR_OK;
}

int fk_conn_add_error_rsp(fk_conn *conn, char *error, size_t error_len)
{
	size_t len;

	len = error_len + 3;

	fk_buf_adjust(conn->wbuf, len);
	if (fk_buf_free_len(conn->wbuf) < len) {
		return FK_SVR_ERR;
	}

	sprintf(fk_buf_free_start(conn->wbuf), rsp_error, error);
	fk_buf_high_inc(conn->wbuf, len);

	return FK_SVR_OK;
}

int fk_conn_add_content_rsp(fk_conn *conn, char *content, size_t content_len)
{
	size_t len;

	len = content_len + 2;

	fk_buf_adjust(conn->wbuf, len);
	if (fk_buf_free_len(conn->wbuf) < len) {
		return FK_SVR_ERR;
	}

	sprintf(fk_buf_free_start(conn->wbuf), rsp_content, content);
	fk_buf_high_inc(conn->wbuf, len);

	return FK_SVR_OK;
}

int fk_conn_add_int_rsp(fk_conn *conn, int num)
{
	size_t len;

	len = fk_util_decimal_digit(num);
	len += 3;

	fk_buf_adjust(conn->wbuf, len);
	if (fk_buf_free_len(conn->wbuf) < len) {
		return FK_SVR_ERR;
	}

	sprintf(fk_buf_free_start(conn->wbuf), rsp_int, num);
	fk_buf_high_inc(conn->wbuf, len);

	return FK_SVR_OK;
}

int fk_conn_add_bulk_rsp(fk_conn *conn, int bulk_len)
{
	size_t len;

	len = fk_util_decimal_digit(bulk_len);
	len += 3;

	fk_buf_adjust(conn->wbuf, len);
	if (fk_buf_free_len(conn->wbuf) < len) {
		return FK_SVR_ERR;
	}

	sprintf(fk_buf_free_start(conn->wbuf), rsp_bulk, bulk_len);
	fk_buf_high_inc(conn->wbuf, len);

	return FK_SVR_OK;
}

int fk_conn_add_mbulk_rsp(fk_conn *conn, int bulk_cnt)
{
	size_t len;

	len = fk_util_decimal_digit(bulk_cnt);
	len += 3;

	fk_buf_adjust(conn->wbuf, len);
	if (fk_buf_free_len(conn->wbuf) < len) {
		return FK_SVR_ERR;
	}

	sprintf(fk_buf_free_start(conn->wbuf), rsp_mbulk, bulk_cnt);
	fk_buf_high_inc(conn->wbuf, len);

	return FK_SVR_OK;
}
