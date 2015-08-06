#ifndef _FK_LUA_H_
#define _FK_LUA_H_

void fk_lua_init();
void fk_lua_keys_reset();
void fk_lua_argv_reset();

int fk_lua_keys_push(char *key, int index);
int fk_lua_argv_push(char *arg, int index);

int fk_lua_script_run(char *code);
#endif
