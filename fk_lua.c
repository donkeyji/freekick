#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <fk_mem.h>
#include <fk_vtr.h>
#include <fk_item.h>
#include <fk_lua.h>
#include <freekick.h>

static int fk_lua_pcall(lua_State *L);

lua_State *gL = NULL;
//fk_conn *lua_conn = NULL;

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
	fk_str *cmd;
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

	/* 
	 * similar to the fk_conn_req_parse()
	 */
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

	/* get return values */

	fk_conn_destroy(lua_conn);

	return 1;
}

int fk_lua_keys_push(fk_conn *conn, int keyc)
{
	int i;
	fk_item *key;

	lua_newtable(gL);

	for (i = 0; i < keyc; i++) {
		key = fk_conn_arg_get(conn, 3 + i);	
		printf("key %d: %s\n", i, fk_str_raw((fk_str *)fk_item_raw(key)));
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
		printf("arg %d: %s\n", i, fk_str_raw((fk_str *)fk_item_raw(arg)));
		lua_pushstring(gL, fk_str_raw((fk_str *)fk_item_raw(arg)));
		lua_rawseti(gL, -2, i + 1);
	}

	lua_setglobal(gL, "ARGV");

	return 0;
}

int fk_lua_script_run(char *code)
{
	int rt;

	printf("code: %s\n", code);

	rt = luaL_loadstring(gL, code) || lua_pcall(gL, 0, LUA_MULTRET, 0);

	return 0;
}
