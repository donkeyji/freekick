/* local headers */
#include <fk_item.h>
#include <fk_svr.h>

/*
 * this fk_svr_str.c is just a part of the original freekick.c
 * I just split the freekick.c into defferent .c files,
 * so just #include <fk_svr.h>
 */
int
fk_cmd_set(fk_conn_t *conn)
{
    int         rt;
    fk_item_t  *key, *value;

    key = fk_conn_get_arg(conn, 1);
    value = fk_conn_get_arg(conn, 2);

    fk_dict_replace(server.db[conn->db_idx], key, value);

    rt = fk_conn_add_status_rsp(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
    if (rt == FK_SVR_ERR) {
        return FK_SVR_ERR;
    }

    return FK_SVR_OK;
}

int
fk_cmd_setnx(fk_conn_t *conn)
{
    int         rt;
    fk_item_t  *key, *value;

    key = fk_conn_get_arg(conn, 1);
    value = fk_dict_get(server.db[conn->db_idx], key);
    if (value != NULL) {
        rt = fk_conn_add_int_rsp(conn, 0);
        if (rt == FK_SVR_ERR) {
            return FK_SVR_ERR;
        }
        return FK_SVR_OK;
    }

    value = fk_conn_get_arg(conn, 2);
    fk_dict_add(server.db[conn->db_idx], key, value);

    rt = fk_conn_add_int_rsp(conn, 1);
    if (rt == FK_SVR_ERR) {
        return FK_SVR_ERR;
    }
    return FK_SVR_OK;
}

int
fk_cmd_mset(fk_conn_t *conn)
{
    int         rt, i;
    fk_item_t  *key, *value;

    if ((conn->arg_cnt - 1) % 2 != 0) {
        rt = fk_conn_add_error_rsp(conn, FK_RSP_ARGC_ERR, sizeof(FK_RSP_ARGC_ERR) - 1);
        if (rt == FK_SVR_ERR) {
            return FK_SVR_ERR;
        }
        return FK_SVR_OK;
    }

    for (i = 1; i < conn->arg_cnt; i += 2) {
        key = fk_conn_get_arg(conn, i);
        value = fk_conn_get_arg(conn, i + 1);
        fk_dict_replace(server.db[conn->db_idx], key, value);
    }
    rt = fk_conn_add_status_rsp(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
    if (rt == FK_SVR_ERR) {
        return FK_SVR_ERR;
    }
    return FK_SVR_OK;
}

int
fk_cmd_mget(fk_conn_t *conn)
{
    int         rt, i;
    fk_str_t   *ss;
    fk_item_t  *value, *key;

    rt = fk_conn_add_mbulk_rsp(conn, conn->arg_cnt - 1);
    if (rt == FK_SVR_ERR) {
        return FK_SVR_ERR;
    }
    for (i = 1; i < conn->arg_cnt; i++) {
        key = fk_conn_get_arg(conn, i);
        value = fk_dict_get(server.db[conn->db_idx], key);
        if (value == NULL) {
            rt = fk_conn_add_bulk_rsp(conn, FK_RSP_NIL);
            if (rt == FK_SVR_ERR) {
                return FK_SVR_ERR;
            }
        } else {
            if (fk_item_type(value) != FK_ITEM_STR) {
                rt = fk_conn_add_bulk_rsp(conn, FK_RSP_NIL);
                if (rt == FK_SVR_ERR) {
                    return FK_SVR_ERR;
                }
            } else {
                ss = (fk_str_t *)fk_item_raw(value);
                rt = fk_conn_add_bulk_rsp(conn, (int)(fk_str_len(ss)));
                if (rt == FK_SVR_ERR) {
                    return FK_SVR_ERR;
                }
                rt = fk_conn_add_content_rsp(conn, fk_str_raw(ss), fk_str_len(ss));
                if (rt == FK_SVR_ERR) {
                    return FK_SVR_ERR;
                }
            }
        }
    }
    return FK_SVR_OK;
}

int
fk_cmd_get(fk_conn_t *conn)
{
    int         rt;
    fk_str_t   *ss;
    fk_item_t  *value, *key;

    key = fk_conn_get_arg(conn, 1);
    value = fk_dict_get(server.db[conn->db_idx], key);
    if (value == NULL) {
        rt = fk_conn_add_bulk_rsp(conn, FK_RSP_NIL);
        if (rt == FK_SVR_ERR) {
            return FK_SVR_ERR;
        }
        return FK_SVR_OK;
    }

    if (fk_item_type(value) != FK_ITEM_STR) {
        rt = fk_conn_add_error_rsp(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
        if (rt == FK_SVR_ERR) {
            return FK_SVR_ERR;
        }
        return FK_SVR_OK;
    }

    ss = (fk_str_t *)fk_item_raw(value);
    rt = fk_conn_add_bulk_rsp(conn, (int)(fk_str_len(ss)));
    if (rt == FK_SVR_ERR) {
        return FK_SVR_ERR;
    }
    rt = fk_conn_add_content_rsp(conn, fk_str_raw(ss), fk_str_len(ss));
    if (rt == FK_SVR_ERR) {
        return FK_SVR_ERR;
    }
    return FK_SVR_OK;
}
