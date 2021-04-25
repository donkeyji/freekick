LUA_DIR=lua-5.1.5
JEMALLOC_DIR=jemalloc-5.2.1

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


.PHONY: lua jemalloc clean distclean install

lua:
	(cd $(LUA_DIR) && $(MAKE) $(LUA_PLAT) )

jemalloc:
	(cd $(JEMALLOC_DIR) && (test -f ./Makefile || ./configure) && $(MAKE))
    
clean:
	cd $(LUA_DIR) && $(MAKE) $@
	cd $(JEMALLOC_DIR) && $(MAKE) $@

distclean:
	cd $(LUA_DIR) && $(MAKE) $@
	cd $(JEMALLOC_DIR) && $(MAKE) $@

install:
	cd $(LUA_DIR) && $(MAKE) $@
	cd $(JEMALLOC_DIR) && $(MAKE) $@
