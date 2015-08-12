#ifndef _FK_LUA_H_
#define _FK_LUA_H_

#include <fk_conn.h>

#define FK_LUA_PARA_KEYS		0
#define FK_LUA_PARA_ARGV		1

void fk_lua_init();

int fk_lua_paras_push(char **paras, int npara, int type);

int fk_lua_script_run(fk_conn *conn, char *code);

#endif
