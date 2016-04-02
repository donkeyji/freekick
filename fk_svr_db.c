/* c standard library headers */
#include <stdlib.h>

/* local headers */
#include <fk_item.h>
#include <fk_svr.h>

/*
 * some protocol related to db operation
 */
int
fk_cmd_del(fk_conn_t *conn)
{
    int         deleted, rt, i;
    fk_item_t  *key;

    deleted = 0;

    for (i = 1; i < conn->arg_cnt; i++) {
        key = fk_conn_get_arg(conn, i);
        rt = fk_dict_remove(server.db[conn->db_idx], key);
        if (rt == FK_DICT_OK) {
            deleted++;
        }
    }

    rt = fk_conn_add_int_rsp(conn, deleted);
    if (rt == FK_SVR_ERR) {
        return FK_SVR_ERR;
    }

    return FK_SVR_OK;
}

int
fk_cmd_flushdb(fk_conn_t *conn)
{
    int  rt;

    fk_dict_empty(server.db[conn->db_idx]);
    rt = fk_conn_add_status_rsp(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
    if (rt == FK_SVR_ERR) {
        return FK_SVR_ERR;
    }
    return FK_SVR_OK;
}

int
fk_cmd_flushall(fk_conn_t *conn)
{
    int       rt;
    unsigned  i;

    for (i = 0; i < server.dbcnt; i++) {
        fk_dict_empty(server.db[i]);
    }
    rt = fk_conn_add_status_rsp(conn, FK_RSP_OK, sizeof(FK_RSP_OK) - 1);
    if (rt == FK_SVR_ERR) {
        return FK_SVR_ERR;
    }

    return FK_SVR_OK;
}

int
fk_cmd_exists(fk_conn_t *conn)
{
    int         rt, n;
    fk_item_t  *key, *value;

    key = fk_conn_get_arg(conn, 1);
    value = fk_dict_get(server.db[conn->db_idx], key);
    n = 0;
    if (value != NULL) {
        n = 1;
    }
    rt = fk_conn_add_int_rsp(conn, n);
    if (rt == FK_SVR_ERR) {
        return FK_SVR_ERR;
    }

    return FK_SVR_OK;
}

int
fk_cmd_save(fk_conn_t *conn)
{
    int  rt, err;

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

int
fk_cmd_select(fk_conn_t *conn)
{
    int         db_idx, rt;
    fk_str_t   *s;
    fk_item_t  *itm;

    itm = fk_conn_get_arg(conn, 1);
    s = (fk_str_t *)fk_item_raw(itm);
    db_idx = atoi(fk_str_raw(s));
    if (db_idx < 0 || db_idx >= server.dbcnt) {
        rt = fk_conn_add_error_rsp(conn, FK_RSP_ERR, sizeof(FK_RSP_ERR) - 1);
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
