/* c standard library headers */
#include <stdlib.h>

/* 3rd headers */
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* local headers */
#include <fk_log.h>
#include <fk_mem.h>
#include <fk_buf.h>
#include <fk_vtr.h>
#include <fk_item.h>
#include <fk_svr.h>

#define FK_LUA_PARA_KEYS        0
#define FK_LUA_PARA_ARGV        1

static int fk_lua_pcall(lua_State *L);

static int fk_lua_conn_proc_cmd(fk_conn_t *conn);

static int fk_lua_push_paras(char **paras, int npara, int type);
static int fk_lua_run_script(fk_conn_t *conn, char *code);

static int fk_lua_parse_status(lua_State *L, fk_buf_t *buf);
static int fk_lua_parse_error(lua_State *L, fk_buf_t *buf);
static int fk_lua_parse_integer(lua_State *L, fk_buf_t *buf);
static int fk_lua_parse_mbulk(lua_State *L, fk_buf_t *buf);
static int fk_lua_parse_bulk(lua_State *L, fk_buf_t *buf);

static lua_State *gL = NULL;
static fk_conn_t *lua_conn = NULL;

static const struct luaL_Reg fklib[] = {
    {"pcall", fk_lua_pcall},
    {NULL, NULL}
};

void
fk_lua_init(void)
{
    gL = luaL_newstate();

    luaL_openlibs(gL);

    //luaL_register(gL, "freekick", fklib);
    luaL_register(gL, "redis", fklib); /* just for convenience when debuging by using "redis" */

    lua_conn = fk_conn_create(FK_CONN_FAKE_FD);
    fk_conn_set_type(lua_conn, FK_CONN_FAKE);
}

/* similar to fk_conn_read_cb */
int
fk_lua_pcall(lua_State *L)
{
    int          i, rt;
    char        *start;
    size_t       len;
    fk_buf_t    *buf;
    fk_item_t   *itm;
    const char  *arg;

    /* 1. similar to the function: fk_conn_req_parse() */
    lua_conn->arg_parsed = lua_gettop(L); /* get the argument count */

    /* allocate memory for arguments */
    fk_vtr_stretch(lua_conn->arg_vtr, (size_t)(lua_conn->arg_parsed));

    for (i = 0; i < lua_conn->arg_parsed; i++) {
        /* do not use luaL_checklstring() here, because if luaL_checklstring()
         * fails, this function will return to lua directly, so that error
         * couldn't be handled inside this function
         */
        if (!lua_isstring(L, i + 1)) {
            lua_pushstring(L, "the argument of pcall should be string");
            return 1;
        }
        arg = lua_tolstring(L, i + 1, &len);
        itm = fk_item_create(FK_ITEM_STR, fk_str_create((char *)arg, len));
        fk_conn_set_arg(lua_conn, lua_conn->arg_cnt, itm);
        fk_item_inc_ref(itm);
        lua_conn->arg_cnt += 1;
    }
    lua_conn->parse_done = 1;

    /* 2. process command */
    fk_lua_conn_proc_cmd(lua_conn);

    /* 3. parse the reply in lua_conn->wbuf */
    /*
     * parse data in lua_conn->wbuf, which is the reply to the client.
     * push them to lua level
     * to do: consume the lua_conn->wbuf ????
     */
    buf = lua_conn->wbuf;
    start = fk_buf_payload_start(buf);
    switch (*start) {
    case '+':
        rt = fk_lua_parse_status(L, buf);
        break;
    case '-':
        rt = fk_lua_parse_error(L, buf);
        break;
    case ':':
        rt = fk_lua_parse_integer(L, buf);
        break;
    case '*':
        rt = fk_lua_parse_mbulk(L, buf);
        break;
    case '$':
        rt = fk_lua_parse_bulk(L, buf);
        break;
    }

    return rt; /* number of return value */
}

int
fk_lua_conn_proc_cmd(fk_conn_t *conn)
{
    int          rt;
    fk_str_t    *cmd;
    fk_item_t   *itm;
    fk_proto_t  *pto;

    itm = (fk_item_t *)fk_conn_get_arg(conn, 0);
    cmd = (fk_str_t *)fk_item_raw(itm);
    fk_str_2upper(cmd);
    pto = fk_proto_search(cmd);
    if (pto == NULL) {
        fk_conn_free_args(conn);
        fk_conn_add_error_rsp(conn, "Invalid Protocol", strlen("Invalid Protocol"));
        return 0;
    }
    if (pto->arg_cnt < 0) {
        if (conn->arg_cnt < -pto->arg_cnt) {
            fk_log_error("wrong argument number\n");
            fk_conn_free_args(conn);
            fk_conn_add_error_rsp(conn, "Wrong Argument Number", strlen("Wrong Argument Number"));
            return FK_SVR_OK;
        }
    }
    if (pto->arg_cnt > 0 && pto->arg_cnt != conn->arg_cnt) {
        fk_conn_free_args(conn);
        fk_conn_add_error_rsp(conn, "Wrong Argument Number", strlen("Wrong Argument Number"));
        return 0;
    }
    rt = pto->handler(conn);
    if (rt == FK_SVR_ERR) { /* arg_vtr are not consumed, free all the arg_vtr */
        fk_conn_free_args(conn);
        fk_conn_add_error_rsp(conn, "cmd handler failed", strlen("cmd handler failed"));
        return 0;
    }
    fk_conn_free_args(conn);

    return 0;
}

int
fk_lua_parse_status(lua_State *L, fk_buf_t *buf)
{
    char  *s;

    lua_newtable(L);
    lua_pushstring(L, "ok"); /* key */

    s = fk_buf_payload_start(buf);
    lua_pushlstring(L, s + 1, fk_buf_payload_len(buf) - 1 - 2); /* value */

    lua_rawset(L, -3);

    fk_buf_low_inc(buf, fk_buf_payload_len(buf));

    return 1;
}

int
fk_lua_parse_error(lua_State *L, fk_buf_t *buf)
{
    char  *s;

    lua_newtable(L);
    lua_pushstring(L, "err"); /* key */

    s = fk_buf_payload_start(buf);
    lua_pushlstring(L, s + 1, fk_buf_payload_len(buf) - 1 - 2); /* value */

    lua_rawset(L, -3);

    fk_buf_low_inc(buf, fk_buf_payload_len(buf));
    return 1;
}

int
fk_lua_parse_integer(lua_State *L, fk_buf_t *buf)
{
    int    n;
    char  *s;

    s = fk_buf_payload_start(buf);
    n = atoi(s + 1);

    lua_pushnumber(L, (lua_Number)n); /* int ---> lua_Number */

    return 1;
}

int
fk_lua_parse_bulk(lua_State *L, fk_buf_t *buf)
{
    int    blen;
    char  *s, *e;

    s = fk_buf_payload_start(buf);
    e = memchr(s, '\n', fk_buf_payload_len(buf));

    blen = atoi(s + 1);
    if (blen == -1) {
        //lua_pushnil(L); /* nil */
        lua_pushboolean(L, 0);
    } else {
        lua_pushlstring(L, e + 1, blen); /* common string */
    }

    return 1;
}

int
fk_lua_parse_mbulk(lua_State *L, fk_buf_t *buf)
{
    int    mbulk, i, blen, n;
    char  *s, *e;

    lua_newtable(L);

    s = fk_buf_payload_start(buf);
    e = memchr(s, '\n', fk_buf_payload_len(buf));
    mbulk = atoi(s + 1);

    for (i = 0; i < mbulk; i++) {
        s = e + 1;
        switch (*s) {
        case '$':
            e = memchr(s + 1, '\n', fk_buf_payload_len(buf));
            blen = atoi(s + 1);
            if (blen == -1) {
                lua_pushboolean(L, 0);
            } else {
                lua_pushlstring(L, e + 1, blen);
            }
            lua_rawseti(L, -2, i + 1);
            e += (blen + 2);
            break;
        case ':':
            e = memchr(s + 1, '\n', fk_buf_payload_len(buf));
            n = atoi(s + 1);
            lua_pushnumber(L, (lua_Number)n);
            lua_rawseti(L, -2, i + 1);
            break;
        }
    }

    return 1;
}

int
fk_lua_push_paras(char **paras, int npara, int type)
{
    int    i;
    char  *name;

    lua_newtable(gL);

    for (i = 0; i < npara; i++) {
        lua_pushstring(gL, paras[i]);
        lua_rawseti(gL, -2, i + 1);
    }

    switch (type) {
    case FK_LUA_PARA_KEYS:
        name = "KEYS";
        break;
    case FK_LUA_PARA_ARGV:
        name = "ARGV";
        break;
    }
    lua_setglobal(gL, name);

    return 0;
}

int
fk_lua_run_script(fk_conn_t *conn, char *code)
{
    int          i, rt, top1, top2, nret, type, stype, idx;
    size_t       len, slen, olen;
    const char  *p, *sp, *err;

    top1 = lua_gettop(gL);

    rt = luaL_loadstring(gL, code);
    if (rt != 0) { /* error occurs when load a string */
        fk_log_info("load string failed\n");
        err = lua_tolstring(gL, -1, &slen);
        fk_conn_add_bulk_rsp(conn, slen);
        fk_conn_add_content_rsp(conn, (char *)err, slen);
        lua_pop(gL, 1); /* must pop this error message */
        return 0;
    }

    lua_conn->db_idx = conn->db_idx; /* keep the same db_idx */

    rt = lua_pcall(gL, 0, LUA_MULTRET, 0);
    if (rt != 0) { /* error occurs when run script */
        fk_log_info("run string failed\n");
        err = lua_tolstring(gL, -1, &slen);
        fk_conn_add_bulk_rsp(conn, slen);
        fk_conn_add_content_rsp(conn, (char *)err, slen);
        lua_pop(gL, 1); /* must pop this error message */
        return 0;
    }

    top2 = lua_gettop(gL);
    nret = top2 - top1; /* get the number of return value */

    if (nret == 0) { /* no return value at all */
        fk_conn_add_bulk_rsp(conn, FK_RSP_NIL);
        lua_settop(gL, top1);
        return 0;
    }

    idx = -1 - (nret - 1); /* only get the first return value */
    type = lua_type(gL, idx);
    switch (type) {
    case LUA_TNUMBER:
        fk_conn_add_int_rsp(conn, (int)lua_tointeger(gL, idx));
        break;
    case LUA_TSTRING:
        p = lua_tolstring(gL, idx, &len);
        fk_conn_add_bulk_rsp(conn, (int)len);
        fk_conn_add_content_rsp(conn, (char *)p, len);
        break;
    case LUA_TNIL:
        fk_conn_add_bulk_rsp(conn, FK_RSP_NIL);
        break;
    case LUA_TBOOLEAN:
        break;
    case LUA_TTABLE:
        olen = lua_objlen(gL, idx);
        fk_conn_add_mbulk_rsp(conn, olen);
        for (i = 1; i <= olen; i++) {
            lua_rawgeti(gL, idx, i);
            stype = lua_type(gL, -1);
            switch (stype) {
            case LUA_TSTRING:
                sp = lua_tolstring(gL, -1, &slen);
                fk_conn_add_bulk_rsp(conn, (int)slen);
                fk_conn_add_content_rsp(conn, (char *)sp, slen);
                break;
            case LUA_TNUMBER:
                fk_conn_add_int_rsp(conn, lua_tointeger(gL, -1));
                break;
            }
            lua_pop(gL, 1);
        }
        break;
    }

    lua_settop(gL, top1); /* keep the top of the stack */
    lua_gc(gL, LUA_GCSTEP, 1);

    return 0;
}

/* relate to lua protocol */
int
fk_cmd_eval(fk_conn_t *conn)
{
    int         nkey, nargv, rt, i;
    char       *code, **keys, **argv;
    fk_str_t   *str_nkey, *str_code;
    fk_item_t  *itm_code, *itm_nkey, *itm_arg;

    itm_code = fk_conn_get_arg(conn, 1);
    itm_nkey = fk_conn_get_arg(conn, 2);

    str_nkey = (fk_str_t *)fk_item_raw(itm_nkey);
    if (fk_str_is_nonminus(str_nkey) == 0) {
        rt = fk_conn_add_error_rsp(conn, FK_RSP_TYPE_ERR, sizeof(FK_RSP_TYPE_ERR) - 1);
        if (rt == FK_SVR_ERR) {
            return FK_SVR_ERR;
        }
        return FK_SVR_OK;
    }

    nkey = atoi(fk_str_raw(str_nkey));

    /* check the number of the arguments */
    if (3 + nkey > conn->arg_cnt) {
        rt = fk_conn_add_error_rsp(conn, FK_RSP_ARGC_ERR, sizeof(FK_RSP_ARGC_ERR) - 1);
        if (rt == FK_SVR_ERR) {
            return FK_SVR_ERR;
        }
        return FK_SVR_OK;
    }

    keys = (char **)fk_mem_calloc(nkey, sizeof(char *));
    for (i = 0; i < nkey; i++) {
        itm_arg = fk_conn_get_arg(conn, 3 + i);
        keys[i] = fk_str_raw((fk_str_t *)fk_item_raw(itm_arg));
    }
    fk_lua_push_paras(keys, nkey, FK_LUA_PARA_KEYS);

    nargv = conn->arg_cnt - 1 - 2 - nkey;
    argv = (char **)fk_mem_calloc(nargv, sizeof(char *));
    for (i = 0; i < nargv; i++) {
        itm_arg = fk_conn_get_arg(conn, 3 + nkey + i);
        argv[i] = fk_str_raw((fk_str_t *)fk_item_raw(itm_arg));
    }
    fk_lua_push_paras(argv, nargv, FK_LUA_PARA_ARGV);

    /* just release the array, but not the every item in it */
    fk_mem_free(keys);
    fk_mem_free(argv);

    str_code = (fk_str_t *)fk_item_raw(itm_code);
    code = fk_str_raw(str_code);
    fk_lua_run_script(conn, code);

    return FK_SVR_OK;
}
