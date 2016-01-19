#include <stdlib.h>

#include <fk_item.h>
#include <fk_svr.h>

int fk_cmd_zadd(fk_conn *conn)
{
	fk_skiplist *sl;
	int score, i, rt;
	fk_item *itm_key, *itm_score, *itm_str, *itm_sl;

	itm_key = fk_conn_get_arg(conn, 1);
	itm_sl = fk_dict_get(server.db[conn->db_idx], itm_key);
	if (itm_sl == NULL) {
		sl = fk_skiplist_create(&db_skiplist_op);
		itm_sl = fk_item_create(FK_ITEM_SKLIST, sl);
		fk_dict_add(server.db[conn->db_idx], itm_key, itm_sl);
	}
	sl = (fk_skiplist *)fk_item_raw(itm_sl);

	if ((conn->arg_cnt - 2) % 2 != 0) {
		rt = fk_conn_add_error_rsp(conn, FK_RSP_ARGC_ERR, sizeof(FK_RSP_ARGC_ERR) - 1);
		if (rt == FK_SVR_ERR) {
			return FK_SVR_ERR;
		}
		return FK_SVR_OK;
	}

	for (i = 2; i < conn->arg_cnt; i += 2) {
		itm_score = fk_conn_get_arg(conn, i);
		itm_str = fk_conn_get_arg(conn, i + 1);

		score = atoi(fk_str_raw((fk_str *)fk_item_raw(itm_score)));
		//printf("score: %d\n", score);
		fk_skiplist_insert(sl, score, itm_str);
	}
	rt = fk_conn_add_int_rsp(conn, (int)((conn->arg_cnt - 2) / 2));
	if (rt == FK_SVR_ERR) {
		return FK_SVR_ERR;
	}

	return FK_SVR_OK;
}
