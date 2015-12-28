#include <stdlib.h>

#include <fk_item.h>
#include <fk_svr.h>

/*
 * some protocol related to db operation
 */
int fk_cmd_del(fk_conn *conn)
{
	fk_item *key;
	int deleted, rt, i;

	deleted = 0;

	for (i = 1; i < conn->arg_cnt; i++) {
		key = fk_conn_arg_get(conn, i);
		rt = fk_dict_remove(server.db[conn->db_idx], key);
		if (rt == FK_DICT_OK) {
			deleted++;
		}
	}

	rt = fk_conn_int_rsp_add(conn, deleted);
	if (rt == FK_SVR_ERR) {
		return FK_SVR_ERR;
	}

	return FK_SVR_OK;
}

int fk_cmd_flushdb(fk_conn *conn)
{
	int rt;

	fk_dict_empty(server.db[conn->db_idx]);
	rt = fk_conn_add_status_rsp(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	if (rt == FK_SVR_ERR) {
		return FK_SVR_ERR;
	}
	return FK_SVR_OK;
}

int fk_cmd_flushall(fk_conn *conn)
{
	int rt;
	unsigned i;

	for (i = 0; i < server.dbcnt; i++) {
		fk_dict_empty(server.db[i]);
	}
	rt = fk_conn_add_status_rsp(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	if (rt == FK_SVR_ERR) {
		return FK_SVR_ERR;
	}

	return FK_SVR_OK;
}

int fk_cmd_exists(fk_conn *conn)
{
	int rt, n;
	fk_item *key, *value;

	key = fk_conn_arg_get(conn, 1);
	value = fk_dict_get(server.db[conn->db_idx], key);
	n = 0;
	if (value != NULL) {
		n = 1;
	}
	rt = fk_conn_int_rsp_add(conn, n);
	if (rt == FK_SVR_ERR) {
		return FK_SVR_ERR;
	}

	return FK_SVR_OK;
}

int fk_cmd_save(fk_conn *conn)
{
	int rt, err;

	err = 1;

	/* saving child process ended */
	if (server.save_pid == -1) {
		rt = fk_fkdb_save();
		if (rt == FK_SVR_OK) {
			err = 0;
		}
	}

	if (err == 1) {
		rt = fk_conn_add_status_rsp(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	} else {
		rt = fk_conn_add_status_rsp(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	}

	if (rt == FK_SVR_ERR) {
		return FK_SVR_ERR;
	}
	return FK_SVR_OK;
}

int fk_cmd_select(fk_conn *conn)
{
	fk_str *s;
	fk_item *itm;
	int db_idx, rt;

	itm = fk_conn_arg_get(conn, 1);
	s = (fk_str *)fk_item_raw(itm);
	db_idx = atoi(fk_str_raw(s));
	if (db_idx < 0 || db_idx >= server.dbcnt) {
		rt = fk_conn_error_rsp_add(conn, FK_RSP_ERR, sizeof(FK_RSP_ERR) - 1);
		if (rt == FK_SVR_ERR) {
			return FK_SVR_ERR;
		}
		return FK_SVR_OK;
	}

	conn->db_idx = db_idx;
	rt = fk_conn_add_status_rsp(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
	if (rt == FK_SVR_ERR) {
		return FK_SVR_ERR;
	}
	return FK_SVR_OK;
}
