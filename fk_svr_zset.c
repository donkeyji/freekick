#include <stdlib.h>

#include <fk_item.h>
#include <freekick.h>

int fk_cmd_zadd(fk_conn *conn)
{
	fk_sklist *sl;
	int score, i, rt;
	fk_item *itm_key, *itm_score, *itm_str, *itm_sl;

	itm_key = fk_conn_arg_get(conn, 1);
	itm_sl = fk_dict_get(server.db[conn->db_idx], itm_key);
	if (itm_sl == NULL) {
		sl = fk_sklist_create(&db_sklist_op);
		itm_sl = fk_item_create(FK_ITEM_SKLIST, sl);
		fk_dict_add(server.db[conn->db_idx], itm_key, itm_sl);
	}
	sl = (fk_sklist *)fk_item_raw(itm_sl);

	for (i = 2; i < conn->arg_cnt; i += 2) {
		itm_score = fk_conn_arg_get(conn, i);
		itm_str = fk_conn_arg_get(conn, i + 1);

		score = atoi(fk_str_raw((fk_str *)fk_item_raw(itm_score)));
		//printf("score: %d\n", score);
		fk_sklist_insert(sl, score, itm_str);
	}
	rt = fk_conn_int_rsp_add(conn, (int)((conn->arg_cnt - 2) / 2));
	if (rt == FK_ERR) {
		return FK_ERR;
	}

	return FK_OK;
}
