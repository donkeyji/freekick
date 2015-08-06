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
}

void fk_lua_argv_reset()
{
}

int fk_lua_keys_push(char *key)
{
	return 0;
}

int fk_lua_argv_push(char *arg)
{
	return 0;
}
