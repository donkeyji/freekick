#--------------------------------------------------------------
# Usage:
# [g]make [debug={yes | no}]
#         [optimize={yes | no}]
#         [malloc={libc | jemalloc | tcmalloc}]
#         [profile={gprof | gperf | none}]
# [g]make clean
#--------------------------------------------------------------

# compiler and linker specification
CC := gcc
LD := gcc

TOP_DIR = ..
3RD_DIR = $(TOP_DIR)/3rd

# basic options
BASIC_CFLAGS  := -std=c99 -pedantic -Wall -I .
BASIC_LDFLAGS :=
BASIC_LDLIBS  :=

# default values for all the arguments accepted
os       := $(shell sh -c 'uname -s 2>/dev/null || echo Generic')
debug    := no
optimize := no
malloc   := libc
profile  := none

# display all the arguments passed
$(info [Listing arguments used...])
$(info os:       $(os))
$(info debug:    $(debug))
$(info optimize: $(optimize))
$(info malloc:   $(malloc))
$(info profile:  $(profile))

# check conflicts of individual arguments
ifeq ($(profile), gprof)
ifeq ($(os), Darwin)
$(error gprof is not supported on Darwin)
endif # Darwin
endif # gprof

# check argument os
ifeq ($(os), Generic)
OS_CFLAGS  :=
OS_LDFLAGS :=
OS_LDLIBS  := -ldl -lm
MPLX_MOD   := fk_ev_poll.c
#MAKE       := make
else
ifeq ($(os), FreeBSD)
OS_CFLAGS  := -I /usr/local/include
OS_LDFLAGS := -L /usr/local/lib
OS_LDLIBS  := -lm -lexecinfo
MPLX_MOD   := fk_ev_kqueue.c
#MAKE       := gmake
else
ifeq ($(os), Linux)
OS_CFLAGS  :=
OS_LDFLAGS :=
OS_LDLIBS  := -ldl -lm -lrt
MPLX_MOD   := fk_ev_epoll.c
#MAKE       := make
else
ifeq ($(os), Darwin)
OS_CFLAGS  :=
OS_LDFLAGS :=
OS_LDLIBS  :=
MPLX_MOD   := fk_ev_kqueue.c
#MAKE       := make
else
$(error $(os) not supported at present)
endif # Darwin
endif # Linux
endif # FreeBSD
endif # Generic

# check argument debug
ifeq ($(debug), yes)
DEBUG_CFLAGS  := -D FK_DEBUG -g
DEBUG_LDFLAGS := -rdynamic
DEBUG_LDLIBS  :=
else
ifeq ($(debug), no)
DEBUG_CFLAGS  := -DNDEBUG
DEBUG_LDFLAGS :=
DEBUG_LDLIBS  :=
else
$(error illegal value for debug)
endif # no
endif # yes

# check argument optimize
ifeq ($(optimize), yes)
OPTIMIZE_CFLAGS  := -O2
OPTIMIZE_LDFLAGS :=
OPTIMIZE_LIBS    :=
else
ifeq ($(optimize), no)
OPTIMIZE_CFLAGS  := -O0
OPTIMIZE_LDFLAGS :=
OPTIMIZE_LIBS    :=
else
$(error illegal value for optimize)
endif # no
endif # yes


# check argument malloc
ifeq ($(malloc), jemalloc)
MALLOC_CFLAGS  := -D FK_USE_JEMALLOC -D JEMALLOC_MANGLE
MALLOC_LDFLAGS :=
MALLOC_LDLIBS  := -ljemalloc
else
ifeq ($(malloc), tcmalloc)
MALLOC_CFLAGS  :=
MALLOC_LDFLAGS :=
MALLOC_LDLIBS  := -ltcmalloc
else
ifeq ($(malloc), libc)
MALLOC_CFLAGS  :=
MALLOC_LDFLAGS :=
MALLOC_LDLIBS  :=
else
$(error illegal value for malloc)
endif # libc
endif # tcmalloc
endif # jemalloc

# check argument profile
ifeq ($(profile), gprof)
PROFILE_CFLAGS  := -pg
PROFILE_LDFLAGS := -pg
PROFILE_LDLIBS  :=
else
ifeq ($(profile), gperf)
PROFILE_CFLAGS  :=
PROFILE_LDFLAGS :=
PROFILE_LDLIBS  := -lprofiler
else
ifeq ($(profile), none)
PROFILE_CFLAGS  :=
PROFILE_LDFLAGS :=
PROFILE_LDLIBS  :=
else
$(error illegal value for profile)
endif # none
endif # gperf
endif # gprof

3RD_CFLAGS    := -I $(3RD_DIR)/lua-5.1.5/src
3RD_LDFLAGS   :=
3RD_LDLIBS    := -L $(3RD_DIR)/lua-5.1.5/src -llua

# final values for CFLAGS && LDFLAGS && LDLIBS
CFLAGS  := $(BASIC_CFLAGS)  $(OS_CFLAGS)  $(DEBUG_CFLAGS)  $(MALLOC_CFLAGS)  $(PROFILE_CFLAGS)  $(OPTIMIZE_CFLAGS) $(3RD_CFLAGS)

LDFLAGS := $(BASIC_LDFLAGS) $(OS_LDFLAGS) $(DEBUG_LDFLAGS) $(MALLOC_LDFLAGS) $(PROFILE_LDFLAGS) $(OPTIMIZE_LDFLAGS) $(3RD_LDFLAGS)

LDLIBS  := $(BASIC_LDLIBS)  $(OS_LDLIBS)  $(DEBUG_LDFLAGS) $(MALLOC_LDLIBS)  $(PROFILE_LDLIBS)  $(OPTIMIZE_LIBS) $(3RD_LDLIBS)

#--------------------------------------------------------------
# sources && objects && Makefile.dep
#--------------------------------------------------------------
SVRBIN  := freekick
SVRSRCS := fk_buf.c fk_conf.c fk_ev.c fk_list.c fk_log.c                        \
           fk_mem.c fk_sock.c fk_str.c fk_util.c fk_heap.c                      \
           fk_pool.c fk_item.c fk_dict.c fk_vtr.c fk_cache.c                    \
           fk_skiplist.c fk_svr.c fk_svr_conn.c fk_svr_proto.c fk_svr_lua.c     \
           fk_svr_str.c fk_svr_hash.c fk_svr_list.c fk_svr_zset.c fk_svr_kdb.c  \
           fk_svr_blog.c fk_svr_db.c freekick.c
SVRSRCS += $(MPLX_MOD)
SVROBJS := $(patsubst %.c, %.o, $(SVRSRCS))
DEPS    := Makefile.dep

.PHONY : all clean distclean dep

all : 3rd $(SVRBIN)

3rd :
	(cd $(3RD_DIR) && $(MAKE))

$(SVRBIN) : $(SVROBJS) 
	@echo "[Linking...]"
	$(LD) -o $(SVRBIN) $(SVROBJS) $(LDFLAGS) $(LDLIBS)

# this statement guarantees that the target $(DEPS) will always be executed,
# even when typing "make clean" to clear all the garbage generated yet
-include $(DEPS)

# if Makefile.dep does not exist, this target will be executed
# this target is always the first one to be executed
dep :
	@echo "[Generating Makefile.dep...]"
	$(CC) -MM $(CFLAGS) $(SVRSRCS) > $(DEPS)

clean :
	@echo "[Removing all the objects...]"
	$(RM) $(SVROBJS) $(SVRBIN)

distclean :
	@echo "[Removing all the objects, dependencies included...]"
	$(RM) $(SVROBJS) $(SVRBIN) && (cd $(3RD_DIR) && $(MAKE) clean)