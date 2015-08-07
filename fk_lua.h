#ifndef _FK_LUA_H_
#define _FK_LUA_H_

#include <fk_conn.h>

void fk_lua_init();

int fk_lua_keys_push(fk_conn *conn, int keyc);
int fk_lua_argv_push(fk_conn *conn, int argc, int keyc);

int fk_lua_script_run(char *code);

#endif
