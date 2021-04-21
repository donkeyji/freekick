LUA_DIR=lua-5.1.5

LUA_PLAT := $(shell sh -c 'uname -s 2>/dev/null || echo Generic')

ifeq ($(LUA_PLAT), Generic)
	LUA_PLAT="generic"
else
ifeq ($(LUA_PLAT), Linux)
	LUA_PLAT="linux"
else
ifeq ($(LUA_PLAT), FreeBSD)
    LUA_PLAT="freebsd"
else
ifeq ($(LUA_PLAT), Darwin)
    LUA_PLAT="macosx"
endif
endif
endif
endif

all:
	(cd $(LUA_DIR) && $(MAKE) $(LUA_PLAT) )
	
clean:
	-cd $(LUA_DIR) && $(MAKE) clean
