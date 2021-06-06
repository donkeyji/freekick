LUA_DIR=lua-5.1.5
JEMALLOC_DIR=jemalloc-5.2.1

os := $(shell sh -c 'uname -s 2>/dev/null')

ifeq ($(os), Linux)
    LUA_PLAT="linux"
else
ifeq ($(os), FreeBSD)
    LUA_PLAT="freebsd"
else
ifeq ($(os), Darwin)
    LUA_PLAT="macosx"
else
ifeq ($(findstring CYGWIN, $(os)), CYGWIN)
    LUA_PLAT="generic"
else
    $(error $(os) is not supported at present)
endif # CYGWIN
endif # Darwin
endif # FreeBSD
endif # Linux


.PHONY: all lua jemalloc clean distclean install

all: lua jemalloc

lua:
	(cd $(LUA_DIR) && $(MAKE) $(LUA_PLAT) )

jemalloc:
	(cd $(JEMALLOC_DIR) && (test -f ./Makefile || (chmod +x ./autogen.sh && ./autogen.sh)) && $(MAKE))
    
clean:
	cd $(LUA_DIR) && $(MAKE) $@
	cd $(JEMALLOC_DIR) && $(MAKE) $@

distclean:
	cd $(LUA_DIR) && $(MAKE) clean
	cd $(JEMALLOC_DIR) && $(MAKE) $@

install:
	cd $(LUA_DIR) && $(MAKE) $@
	cd $(JEMALLOC_DIR) && $(MAKE) $@
