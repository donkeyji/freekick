#---------------------------------------------------
# CC && CFLAGS && LDFLAGS && LDLIBS
#---------------------------------------------------
CC := gcc

BASIC_CFLAGS	:= -I . -std=c99 -pedantic -Wall -O2
BASIC_LDFLAGS 	:=
BASIC_LDLIBS 	:= -llua -ldl -lm
# -ldl: needed when in linux, but not in mac
# -lm: needed when in linux, but not in mac

# debug mode is the default
ifeq ($(debug), yes)
DEBUG_CFLAGS	:= -D FK_DEBUG -g
DEBUG_LDFLAGS 	:=
DEBUG_LDLIBS 	:=
endif

# the default malloc/free is original libc malloc/free
ifeq ($(jemalloc), yes)
MALLOC_CFLAGS	:= -D FK_USE_JEMALLOC -D JEMALLOC_MANGLE
MALLOC_LDFLAGS 	:=
MALLOC_LDLIBS 	:= -ljemalloc
endif

# gprof
ifeq ($(gprof), yes)
GPROF_CFLAGS	:= -g -pg -lc_p
GPROF_LDFLAGS 	:= -pg
GPROF_LDLIBS 	:= -lc_p
endif

CFLAGS 	:= $(BASIC_CFLAGS) 	$(DEBUG_CFLAGS)  $(MALLOC_CFLAGS)  $(GPROF_CFLAGS)
LDFLAGS := $(BASIC_LDFLAGS) $(DEBUG_LDFLAGS) $(MALLOC_LDFLAGS) $(GPROF_LDFLAGS)
LDLIBS 	:= $(BASIC_LDLIBS) 	$(DEBUG_LDFLAGS) $(MALLOC_LDLIBS)  $(GPROF_LDLIBS)

#---------------------------------------------------
# sources && objects && Makefile.dep
#---------------------------------------------------
SVRBIN	:= freekick
SVRSRCS := fk_buf.c fk_conf.c fk_ev.c fk_list.c fk_log.c fk_mem.c fk_sock.c fk_str.c fk_util.c fk_heap.c fk_pool.c fk_item.c fk_dict.c fk_vtr.c fk_cache.c fk_skiplist.c fk_svr.c fk_svr_conn.c fk_svr_proto.c fk_svr_lua.c fk_svr_str.c fk_svr_hash.c fk_svr_list.c fk_svr_zset.c fk_svr_fkdb.c fk_svr_blog.c fk_svr_db.c freekick.c
SVROBJS := $(patsubst %.c, %.o, $(SVRSRCS))
DEPS 	:= Makefile.dep

.PHONY : all clean

all : $(SVRBIN)

$(SVRBIN) : $(SVROBJS) 
	@echo "[Linking...]"
	$(CC) -o $(SVRBIN) $(SVROBJS) $(LDFLAGS) $(LDLIBS)

-include $(DEPS)

# if Makefile.dep does not exist, this target will be executed
$(DEPS) :
	@echo "[Generating Makefile.dep...]"
	$(CC) -MM $(CFLAGS) $(SVRSRCS) > $(DEPS)

clean :
	@echo "[Removing all the objects...]"
	$(RM) $(SVROBJS) $(SVRBIN) $(DEPS)
