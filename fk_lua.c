#include <stdlib.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <fk_buf.h>
#include <fk_mem.h>
#include <fk_vtr.h>
#include <fk_item.h>
#include <fk_lua.h>
#include <freekick.h>

static int fk_lua_pcall(lua_State *L);

static int fk_lua_status_parse(lua_State *L, fk_buf *buf);
static int fk_lua_error_parse(lua_State *L, fk_buf *buf);
static int fk_lua_integer_parse(lua_State *L, fk_buf *buf);
static int fk_lua_mbulk_parse(lua_State *L, fk_buf *buf);
static int fk_lua_bulk_parse(lua_State *L, fk_buf *buf);

lua_State *gL = NULL;

static const struct luaL_Reg fklib[] = {
	{"pcall", fk_lua_pcall},
	{NULL, NULL}
};

void fk_lua_init()
{
	gL = luaL_newstate();

	luaL_openlibs(gL);

	luaL_register(gL, "freekick", fklib);
}

int fk_lua_pcall(lua_State *L)
{
	int i;
	size_t len;
	char *start;
	fk_str *cmd;
	fk_buf *buf;
	fk_item *itm;
	fk_proto *pto;
	const char *arg;
	fk_conn *lua_conn;

	lua_conn = fk_conn_create(-1);/* invalid fd */

	/* get the argument count */
	lua_conn->arg_cnt = lua_gettop(L);

	/* allocate memory for arguments */
	fk_vtr_stretch(lua_conn->arg_vtr, (size_t)(lua_conn->arg_cnt));
	fk_vtr_stretch(lua_conn->len_vtr, (size_t)(lua_conn->arg_cnt));

	/* similar to the function: fk_conn_req_parse() */
	for (i = 0; i < lua_conn->arg_cnt; i++) {
		arg = luaL_checklstring(L, i + 1, &len);
		itm = fk_item_create(FK_ITEM_STR, fk_str_create((char *)arg, len));
		fk_conn_arg_set(lua_conn, lua_conn->arg_idx, itm);
		fk_conn_arglen_set(lua_conn, lua_conn->arg_idx, (void *)((size_t)len));
		fk_item_ref_inc(itm);
		lua_conn->arg_idx += 1;
	}
	lua_conn->parse_done = 1;

	/* the first arg */
	itm = (fk_item *)fk_conn_arg_get(lua_conn, 0);
	cmd = (fk_str *)fk_item_raw(itm);
	fk_str_2upper(cmd);
	pto = fk_proto_search(cmd);
	pto->handler(lua_conn);

	/* get return values, push them to lua */
	/* parse lua_conn->write_buf */
	buf = lua_conn->wbuf;
	start = fk_buf_payload_start(buf);
	switch (*start) {
	case '+':
		fk_lua_status_parse(L, buf);
		break;
	case '-':
		fk_lua_error_parse(L, buf);
		break;
	case ':':
		fk_lua_integer_parse(L, buf);
		break;
	case '*':
		fk_lua_mbulk_parse(L, buf);
		break;
	case '$':
		fk_lua_bulk_parse(L, buf);
		break;
	}

	/* destroy this fake connection */
	fk_conn_destroy(lua_conn);

	return 1;
}

int fk_lua_status_parse(lua_State *L, fk_buf *buf)
{
	char *s;

	s = fk_buf_payload_start(buf);
	lua_pushlstring(L, s + 1, fk_buf_payload_len(buf) - 1 - 2);
	return 1;
}

int fk_lua_error_parse(lua_State *L, fk_buf *buf)
{
	return 0;
}

int fk_lua_integer_parse(lua_State *L, fk_buf *buf)
{
	return 0;
}

int fk_lua_bulk_parse(lua_State *L, fk_buf *buf)
{
	int blen;
	char *s, *e;

	s = fk_buf_payload_start(buf);
	e = memchr(s, '\n', fk_buf_payload_len(buf));

	blen = atoi(s + 1);

	lua_pushlstring(L, e + 1, blen);

	return 1;
}

int fk_lua_mbulk_parse(lua_State *L, fk_buf *buf)
{
	return 0;
}

int fk_lua_keys_push(fk_conn *conn, int keyc)
{
	int i;
	fk_item *key;

	lua_newtable(gL);

	for (i = 0; i < keyc; i++) {
		key = fk_conn_arg_get(conn, 3 + i);	
		lua_pushstring(gL, fk_str_raw((fk_str *)fk_item_raw(key)));
		lua_rawseti(gL, -2, i + 1);
	}

	lua_setglobal(gL, "KEYS");

	return 0;
}

int fk_lua_argv_push(fk_conn *conn, int argc, int keyc)
{
	int i;
	fk_item *arg;

	lua_newtable(gL);

	for (i = 0; i < argc; i++) {
		arg = fk_conn_arg_get(conn, 3 + keyc + i);	
		lua_pushstring(gL, fk_str_raw((fk_str *)fk_item_raw(arg)));
		lua_rawseti(gL, -2, i + 1);
	}

	lua_setglobal(gL, "ARGV");

	return 0;
}

int fk_lua_script_run(fk_conn *conn, char *code)
{
	int rt;
	size_t len;
	const char *reply;

	rt = luaL_loadstring(gL, code) || lua_pcall(gL, 0, LUA_MULTRET, 0);
	if (rt < 0) {
	} else if (rt == 0) {
		reply = luaL_checklstring(gL, -1, &len);
		printf("reply: %s, len: %zu\n", reply, len);
		fk_conn_bulk_rsp_add(conn, len);
		fk_conn_content_rsp_add(conn, (char *)reply, len);
	}

	return 0;
}
