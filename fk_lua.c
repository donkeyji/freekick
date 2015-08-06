#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <fk_lua.h>

static int fk_lua_pcall(lua_State *L);

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
	lua_pushinteger(L, 1);
	lua_pushinteger(L, 2);
	return 1;
}

void fk_lua_keys_reset()
{
	lua_newtable(gL);
	lua_setglobal(gL, "KEYS");
}

void fk_lua_argv_reset()
{
	lua_newtable(gL);
	lua_setglobal(gL, "ARGV");
}

int fk_lua_keys_push(char *key, int index)
{
	/* locate the array KEYS */
	lua_getglobal(gL, "KEYS");

	lua_pushstring(gL, key);
	lua_rawseti(gL, -2, index);

	return 0;
}

int fk_lua_argv_push(char *arg, int index)
{
	/* locate the array KEYS */
	lua_getglobal(gL, "ARGV");

	lua_pushstring(gL, arg);
	lua_rawseti(gL, -2, index);

	return 0;
}

int fk_lua_script_run(char *code)
{
	int rt;

	rt = luaL_loadstring(gL, code) || lua_pcall(gL, 0, LUA_MULTRET, 0);

	return 0;
}
