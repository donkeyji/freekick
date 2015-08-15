#include <stdlib.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <fk_log.h>
#include <fk_buf.h>
#include <fk_mem.h>
#include <fk_vtr.h>
#include <fk_item.h>
#include <fk_lua.h>
#include <fk_macro.h>
#include <fk_conn.h>
#include <freekick.h>

static int fk_lua_pcall(lua_State *L);

static int fk_lua_conn_cmd_proc(fk_conn *conn);

static int fk_lua_status_parse(lua_State *L, fk_buf *buf);
static int fk_lua_error_parse(lua_State *L, fk_buf *buf);
static int fk_lua_integer_parse(lua_State *L, fk_buf *buf);
static int fk_lua_mbulk_parse(lua_State *L, fk_buf *buf);
static int fk_lua_bulk_parse(lua_State *L, fk_buf *buf);

lua_State *gL = NULL;
fk_conn *lua_conn = NULL;

static const struct luaL_Reg fklib[] = {
	{"pcall", fk_lua_pcall},
	{NULL, NULL}
};

void fk_lua_init()
{
	gL = luaL_newstate();

	luaL_openlibs(gL);

	//luaL_register(gL, "freekick", fklib);
	luaL_register(gL, "redis", fklib);/* just for convenience when debuging by using "redis" */
}

int fk_lua_pcall(lua_State *L)
{
	int i, rt;
	size_t len;
	char *start;
	fk_buf *buf;
	fk_item *itm;
	const char *arg;

	/* 
	 * create a fake client used for executing a freekick command,
	 * of which fd is invalid, just ignore the error generated 
	 * when calling fk_ioev_add()
	 */

	/* get the argument count */
	lua_conn->arg_cnt = lua_gettop(L);

	/* allocate memory for arguments */
	fk_vtr_stretch(lua_conn->arg_vtr, (size_t)(lua_conn->arg_cnt));
	fk_vtr_stretch(lua_conn->len_vtr, (size_t)(lua_conn->arg_cnt));

	/* similar to the function: fk_conn_req_parse() */
	for (i = 0; i < lua_conn->arg_cnt; i++) {
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
		fk_conn_arg_set(lua_conn, lua_conn->arg_idx, itm);
		fk_conn_arglen_set(lua_conn, lua_conn->arg_idx, (void *)((size_t)len));
		fk_item_ref_inc(itm);
		lua_conn->arg_idx += 1;
	}
	lua_conn->parse_done = 1;

	fk_lua_conn_cmd_proc(lua_conn);

	/* 
	 * parse data in lua_conn->wbuf, which is the reply to the client.
	 * push them to lua level
	 */
	buf = lua_conn->wbuf;
	start = fk_buf_payload_start(buf);
	switch (*start) {
	case '+':
		rt = fk_lua_status_parse(L, buf);
		break;
	case '-':
		rt = fk_lua_error_parse(L, buf);
		break;
	case ':':
		rt = fk_lua_integer_parse(L, buf);
		break;
	case '*':
		rt = fk_lua_mbulk_parse(L, buf);
		break;
	case '$':
		rt = fk_lua_bulk_parse(L, buf);
		break;
	}


	return rt;/* number of return value */
}

int fk_lua_conn_cmd_proc(fk_conn *conn)
{
	int rt;
	fk_str *cmd;
	fk_item *itm;
	fk_proto *pto;

	itm = (fk_item *)fk_conn_arg_get(conn, 0);
	cmd = (fk_str *)fk_item_raw(itm);
	fk_str_2upper(cmd);
	pto = fk_proto_search(cmd);
	if (pto == NULL) {
		fk_conn_error_rsp_add(conn, "Invalid Protocol", strlen("Invalid Protocol"));
		return 0;
	}
	if (pto->arg_cnt != FK_PROTO_VARLEN && pto->arg_cnt != conn->arg_cnt) {
		fk_conn_error_rsp_add(conn, "Wrong Argument Number", strlen("Wrong Argument Number"));
		return 0;
	}
	rt = pto->handler(conn);
	if (rt == FK_ERR) {/* arg_vtr are not consumed, free all the arg_vtr */
		fk_conn_error_rsp_add(conn, "cmd handler failed", strlen("cmd handler failed"));
		return 0;
	}

	return 0;
}

int fk_lua_status_parse(lua_State *L, fk_buf *buf)
{
	char *s;

	lua_newtable(L);
	lua_pushstring(L, "ok");/* key */

	s = fk_buf_payload_start(buf);
	lua_pushlstring(L, s + 1, fk_buf_payload_len(buf) - 1 - 2);/* value */

	lua_rawset(L, -3);

	return 1;
}

int fk_lua_error_parse(lua_State *L, fk_buf *buf)
{
	char *s;

	lua_newtable(L);
	lua_pushstring(L, "err");/* key */

	s = fk_buf_payload_start(buf);
	lua_pushlstring(L, s + 1, fk_buf_payload_len(buf) - 1 - 2);/* value */

	lua_rawset(L, -3);

	return 1;
}

int fk_lua_integer_parse(lua_State *L, fk_buf *buf)
{
	int n;
	char *s;

	s = fk_buf_payload_start(buf);
	n = atoi(s + 1);

	lua_pushnumber(L, (lua_Number)n);/* int ---> lua_Number */

	return 1;
}

int fk_lua_bulk_parse(lua_State *L, fk_buf *buf)
{
	int blen;
	char *s, *e;

	s = fk_buf_payload_start(buf);
	e = memchr(s, '\n', fk_buf_payload_len(buf));

	blen = atoi(s + 1);
	if (blen == -1) {
		//lua_pushnil(L);/* nil */
		lua_pushboolean(L, 0);
	} else {
		lua_pushlstring(L, e + 1, blen);/* common string */
	}

	return 1;
}

int fk_lua_mbulk_parse(lua_State *L, fk_buf *buf)
{
	int mbulk, i, blen, n;
	char *s, *e;

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

int fk_lua_paras_push(char **paras, int npara, int type)
{
	int i;
	char *name;

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

int fk_lua_script_run(fk_conn *conn, char *code)
{
	size_t len, slen, olen;
	const char *p, *sp, *err;
	int i, rt, top1, top2, nret, type, stype, idx;

	top1 = lua_gettop(gL);

	rt = luaL_loadstring(gL, code);
	if (rt != 0) {/* error occurs when load a string */
		fk_log_info("load string failed\n");
		err = lua_tolstring(gL, -1, &slen);
		fk_conn_bulk_rsp_add(conn, slen);
		fk_conn_content_rsp_add(conn, (char *)err, slen);
		lua_pop(gL, 1);/* must pop this error message */
		return 0;
	}

	lua_conn = fk_conn_create(-1);
	lua_conn->db_idx = conn->db_idx;/* keep the same db_idx */
   	rt = lua_pcall(gL, 0, LUA_MULTRET, 0);
	if (rt != 0) {/* error occurs when run script */
		fk_log_info("run string failed\n");
		err = lua_tolstring(gL, -1, &slen);
		fk_conn_bulk_rsp_add(conn, slen);
		fk_conn_content_rsp_add(conn, (char *)err, slen);
		lua_pop(gL, 1);/* must pop this error message */
		fk_conn_destroy(lua_conn);/* destroy this fake client */
		return 0;
	}
	fk_conn_destroy(lua_conn);/* destroy this fake client */

	top2 = lua_gettop(gL);
	nret = top2 - top1;/* get the number of return value */

	if (nret == 0) {/* no return value at all */
		fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
		return 0;
	}

	idx = -1 - (nret - 1);/* only get the first return value */
	type = lua_type(gL, idx);
	switch (type) {
	case LUA_TNUMBER:
		fk_conn_int_rsp_add(conn, (int)lua_tointeger(gL, idx));
		break;
	case LUA_TSTRING:
		p = lua_tolstring(gL, idx, &len);
		fk_conn_bulk_rsp_add(conn, (int)len);
		fk_conn_content_rsp_add(conn, (char *)p, len);
		break;
	case LUA_TNIL:
		fk_conn_bulk_rsp_add(conn, FK_RSP_NIL);
		break;
	case LUA_TBOOLEAN:
		break;
	case LUA_TTABLE:
		olen = lua_objlen(gL, idx);
		fk_conn_mbulk_rsp_add(conn, olen);
		for (i = 1; i <= olen; i++) {
			lua_rawgeti(gL, idx, i);
			stype = lua_type(gL, -1);
			switch (stype) {
			case LUA_TSTRING:
				sp = lua_tolstring(gL, -1, &slen);
				fk_conn_bulk_rsp_add(conn, (int)slen);
				fk_conn_content_rsp_add(conn, (char *)sp, slen);
				break;
			case LUA_TNUMBER:
				fk_conn_int_rsp_add(conn, lua_tointeger(gL, -1));
				break;
			}
			lua_pop(gL, 1);
		}
		break;
	}

	lua_pop(gL, nret);/* pop all the return values */
	lua_gc(gL, LUA_GCSTEP, 1);

	return 0;
}
