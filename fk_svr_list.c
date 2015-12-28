#include <fk_item.h>
#include <fk_svr.h>

static int fk_cmd_generic_push(fk_conn *conn, int pos);
static int fk_cmd_generic_pop(fk_conn *conn, int pos);

int fk_cmd_lpush(fk_conn *conn)
{
	return fk_cmd_generic_push(conn, 0);
}

int fk_cmd_rpush(fk_conn *conn)
{
	return fk_cmd_generic_push(conn, 1);
}

int fk_cmd_lpop(fk_conn *conn)
{
	return fk_cmd_generic_pop(conn, 0);
}

int fk_cmd_rpop(fk_conn *conn)
{
	return fk_cmd_generic_pop(conn, 1);
}

int fk_cmd_llen(fk_conn *conn)
{
	int rt, len;
	fk_list *lst;
	fk_item *key, *value;

	key = fk_conn_arg_get(conn, 1);
	value = fk_dict_get(server.db[conn->db_idx], key);
	if (value == NULL) {
		rt = fk_conn_add_int_rsp(conn, 0);
		if (rt == FK_SVR_ERR) {
			return FK_SVR_ERR;
		}
		return FK_SVR_OK;
	}
	if (fk_item_type(value) != FK_ITEM_LIST) {
		rt = fk_conn_add_error_rsp(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt == FK_SVR_ERR) {
			return FK_SVR_ERR;
		}
		return FK_SVR_OK;
	}

	lst = (fk_list *)fk_item_raw(value);
	len = fk_list_len(lst);
	rt = fk_conn_add_int_rsp(conn, len);
	if (rt == FK_SVR_ERR) {
		return FK_SVR_ERR;
	}
	return FK_SVR_OK;
}

int fk_cmd_generic_push(fk_conn *conn, int pos)
{
	int rt, i;
	fk_list *lst;
	fk_item *key, *lst_itm, *str_itm;

	key = fk_conn_arg_get(conn, 1);
	lst_itm = fk_dict_get(server.db[conn->db_idx], key);
	if (lst_itm == NULL) {
		lst = fk_list_create(&db_list_op);
		for (i = 2; i < conn->arg_cnt; i++) {
			str_itm = fk_conn_arg_get(conn, i);
			if (pos == 0) {/* lpush */
				fk_list_insert_head(lst, str_itm);
			} else {/* rpush */
				fk_list_insert_tail(lst, str_itm);
			}
		}
		lst_itm = fk_item_create(FK_ITEM_LIST, lst);/* do not increase ref for local var */
		fk_dict_add(server.db[conn->db_idx], key, lst_itm);
		rt = fk_conn_add_int_rsp(conn, conn->arg_cnt - 2);
		if (rt == FK_SVR_ERR) {
			return FK_SVR_ERR;
		}

		return FK_SVR_OK;
	}

	if (fk_item_type(lst_itm) != FK_ITEM_LIST) {
		rt = fk_conn_add_error_rsp(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt == FK_SVR_ERR) {
			return FK_SVR_ERR;
		}
		return FK_SVR_OK;
	}

	lst = fk_item_raw(lst_itm);
	for (i = 2; i < conn->arg_cnt; i++) {
		str_itm = fk_conn_arg_get(conn, i);
		if (pos == 0) {/* lpush */
			fk_list_insert_head(lst, str_itm);
		} else {/* rpush */
			fk_list_insert_tail(lst, str_itm);
		}
	}
	rt = fk_conn_add_int_rsp(conn, conn->arg_cnt - 2);
	if (rt == FK_SVR_ERR) {
		return FK_SVR_ERR;
	}
	return FK_SVR_OK;
}

int fk_cmd_generic_pop(fk_conn *conn, int pos)
{
	int rt;
	fk_str *ss;
	fk_list *lst;
	fk_node *nd_itm;
	fk_item *key, *lst_itm, *itm;

	key = fk_conn_arg_get(conn, 1);
	lst_itm = fk_dict_get(server.db[conn->db_idx], key);
	if (lst_itm == NULL) {
		rt = fk_conn_add_bulk_rsp(conn, FK_RSP_NIL);
		if (rt == FK_SVR_ERR) {
			return FK_SVR_ERR;
		}
		return FK_SVR_OK;
	}

	if (fk_item_type(lst_itm) != FK_ITEM_LIST) {
		rt = fk_conn_add_error_rsp(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
		if (rt == FK_SVR_ERR) {
			return FK_SVR_ERR;
		}
		return FK_SVR_OK;
	}

	lst = fk_item_raw(lst_itm);
	if (fk_list_len(lst) == 0) {
		rt = fk_conn_add_bulk_rsp(conn, FK_RSP_NIL);
		if (rt == FK_SVR_ERR) {
			return FK_SVR_ERR;
		}
		return FK_SVR_OK;
	}

	if (pos == 0) {
		nd_itm = fk_list_head(lst);
	} else {
		nd_itm = fk_list_tail(lst);
	}
	itm = (fk_item *)fk_node_raw(nd_itm);
	ss = (fk_str *)fk_item_raw(itm);
	rt = fk_conn_add_bulk_rsp(conn, (int)(fk_str_len(ss)));
	if (rt == FK_SVR_ERR) {
		return FK_SVR_ERR;
	}
	rt = fk_conn_add_content_rsp(conn, fk_str_raw(ss), fk_str_len(ss));
	if (rt == FK_SVR_ERR) {
		return FK_SVR_ERR;
	}
	fk_list_remove_any(lst, nd_itm);

	return FK_SVR_OK;
}
