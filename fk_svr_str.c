#include <freekick.h>

/* 
 * this fk_svr_str.c is just a part of the original freekick.c
 * I just split the freekick.c into defferent .c files, 
 * so just #include <freekick.h>
 */
int fk_cmd_set(fk_conn *conn)
{
	int rt;
	fk_item *key, *value;

	key = fk_conn_arg_get(conn, 1);
	value = fk_conn_arg_get(conn, 2);

	fk_dict_replace(server.db[conn->db_idx], key, value);

	rt = fk_conn_status_rsp_add(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	if (rt == FK_ERR) {
		return FK_ERR;
	}

	return FK_OK;
}

int fk_cmd_setnx(fk_conn *conn)
{
	int rt;
	fk_item *key, *value;

	key = fk_conn_arg_get(conn, 1);
	value = fk_dict_get(server.db[conn->db_idx], key);
	if (value != NULL) {
		rt = fk_conn_int_rsp_add(conn, 0);
		if (rt == FK_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	}

	value = fk_conn_arg_get(conn, 2);
	fk_dict_add(server.db[conn->db_idx], key, value);

	rt = fk_conn_int_rsp_add(conn, 1);
	if (rt == FK_ERR) {
		return FK_ERR;
	}
	return FK_OK;
}

int fk_cmd_mset(fk_conn *conn)
{
	int rt, i;
	fk_item *key, *value;

	for (i = 1; i < conn->arg_cnt; i += 2) {
		key = fk_conn_arg_get(conn, i);
		value = fk_conn_arg_get(conn, i + 1);
		fk_dict_replace(server.db[conn->db_idx], key, value);
	}
	rt = fk_conn_status_rsp_add(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	if (rt == FK_ERR) {
		return FK_ERR;
	}
	return FK_OK;
}

int fk_cmd_mget(fk_conn *conn)
{
	int rt, i;
	fk_str *ss;
	fk_item *value, *key;

	rt = fk_conn_mbulk_rsp_add(conn, conn->arg_cnt - 1);
	if (rt == FK_ERR) {
		return FK_ERR;
	}
	for (i = 1; i < conn->arg_cnt; i++) {
		key = fk_conn_arg_get(conn, i);
		value = fk_dict_get(server.db[conn->db_idx], key);
		if (value == NULL) {
			rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
			if (rt == FK_ERR) {
				return FK_ERR;
			}
		} else {
			if (fk_item_type(value) != FK_ITEM_STR) {
				rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
				if (rt == FK_ERR) {
					return FK_ERR;
				}
			} else {
				ss = (fk_str *)fk_item_raw(value);
				rt = fk_conn_bulk_rsp_add(conn, (int)(fk_str_len(ss)));
				if (rt == FK_ERR) {
					return FK_ERR;
				}
				rt = fk_conn_content_rsp_add(conn, fk_str_raw(ss), fk_str_len(ss));
				if (rt == FK_ERR) {
					return FK_ERR;
				}
			}
		}
	}
	return FK_OK;
}

int fk_cmd_get(fk_conn *conn)
{
	int rt;
	fk_str *ss;
	fk_item *value, *key;

	key = fk_conn_arg_get(conn, 1);
	value = fk_dict_get(server.db[conn->db_idx], key);
	if (value == NULL) {
		rt = fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
		if (rt == FK_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	} 

	if (fk_item_type(value) != FK_ITEM_STR) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt == FK_ERR) {
			return FK_ERR;
		}
		return FK_OK;
	} 

	ss = (fk_str *)fk_item_raw(value);
	rt = fk_conn_bulk_rsp_add(conn, (int)(fk_str_len(ss)));
	if (rt == FK_ERR) {
		return FK_ERR;
	}
	rt = fk_conn_content_rsp_add(conn, fk_str_raw(ss), fk_str_len(ss));
	if (rt == FK_ERR) {
		return FK_ERR;
	}
	return FK_OK;
}
