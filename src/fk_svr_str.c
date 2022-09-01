/* local headers */
#include <fk_item.h>
#include <fk_svr.h>
#include <fk_num.h>
#include <fk_util.h>

/*
 * this fk_svr_str.c is just a part of the original freekick.c
 * I just split the freekick.c into separate .c files,
 * so just #include <fk_svr.h>
 * technically, all functions with the form of fk_cmd_xxx which
 * are meant to handle cmds are methods of fk_svr_t, for these
 * functions maintain/update the state of the global veriable
 * server.
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
fk_cmd_setex(fk_conn_t *conn)
{
    int         rt;
    long long   exp_secs, exp_millis;
    fk_num_t   *exp_num;
    fk_str_t   *exp_str;
    fk_item_t  *key, *value, *exp, *exp_item;
    struct timeval  now;

    key = fk_conn_get_arg(conn, 1);
    exp = fk_conn_get_arg(conn, 2);
    value = fk_conn_get_arg(conn, 3);

    /* convert string to long long integer */
    exp_str = (fk_str_t *)fk_item_raw(exp);
    exp_secs = (long long)atol(fk_str_raw(exp_str));

    /* generate the fk_num_t object */
    fk_util_get_time(&now);
    exp_millis = exp_secs * 1000 + fk_util_tv2millis(&now);
    exp_num = fk_num_create(exp_millis);

    /* generate a FK_ITEM_NUM object, no need to call fk_item_inc_ref() here */
    exp_item = fk_item_create(FK_ITEM_NUM, exp_num);

    /* save key and value to db */
    fk_dict_replace(server.db[conn->db_idx], key, value);

    /* save key and exp_item to expdb */
    fk_dict_replace(server.expdb[conn->db_idx], key, exp_item);

    rt = fk_conn_add_status_rsp(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
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
fk_handle_expired_key(fk_dict_t *db, fk_dict_t *expdb, fk_item_t *key)
{
    int             expired;
    fk_item_t      *exp;
    long long       exp_millis, now_millis;
    struct timeval  now;

    expired = 0;

    /* to check whether the key has expired */
    exp = fk_dict_get(expdb, key);
    if (exp != NULL) {
        exp_millis = (long long)fk_num_raw((fk_num_t *)fk_item_raw(exp));
        fk_util_get_time(&now);
        now_millis = fk_util_tv2millis(&now);
        if (exp_millis < now_millis) {
            expired = 1;
        }
    }
    /* if expired, remove the key */
    if (expired == 1) {
        /*
         * this key is now pointing to the item in expdb, if it's
         * removed from expdb first, then the memory for key may
         * be destroyed. for safety, remove key from db first.
         */
        fk_dict_remove(db, key);
        fk_dict_remove(expdb, key);
    }
    return expired;
}

int
fk_cmd_mget(fk_conn_t *conn)
{
    int         rt, i, expired;
    fk_str_t   *ss;
    fk_item_t  *value, *key;

    rt = fk_conn_add_mbulk_rsp(conn, conn->arg_cnt - 1);
    if (rt == FK_SVR_ERR) {
        return FK_SVR_ERR;
    }
    for (i = 1; i < conn->arg_cnt; i++) {
        key = fk_conn_get_arg(conn, i);

        expired = fk_handle_expired_key(server.db[conn->db_idx], server.expdb[conn->db_idx], key);
        value = NULL;
        if (expired == 0) {
            value = fk_dict_get(server.db[conn->db_idx], key);
        }

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
    int         rt, expired;
    fk_str_t   *ss;
    fk_item_t  *value, *key;

    key = fk_conn_get_arg(conn, 1);

    expired = fk_handle_expired_key(server.db[conn->db_idx], server.expdb[conn->db_idx], key);
    value = NULL;
    if (expired == 0) {
        value = fk_dict_get(server.db[conn->db_idx], key);
    }

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
