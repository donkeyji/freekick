#--------------------------------------------------------------
# Usage:
# [g]make [os={generic | mac | linux | freebsd}]
#		  [debug={yes | no}]
#		  [optimize={yes | no}]
#		  [malloc={libc | jemalloc | tcmalloc}]
#		  [profile={gprof | gperf | none}]
#--------------------------------------------------------------

#--------------------------------------------------------------
# CC && CFLAGS && LDFLAGS && LDLIBS
#--------------------------------------------------------------
CC := gcc

# basic options
BASIC_CFLAGS  := -std=c99 -pedantic -Wall -I .
BASIC_LDFLAGS :=
BASIC_LDLIBS  := -llua

# default values for all the arguments accepted
os       := generic
debug    := no
optimize := no
malloc   := libc
profile  := none

# display all the arguments passed
$(info [Listing Arguments Used...])
$(info os:       $(os))
$(info debug:    $(debug))
$(info optimize: $(optimize))
$(info malloc:   $(malloc))
$(info profile:  $(profile))

# check conflicts of individual arguments
ifeq ($(profile), gprof)
ifeq ($(os), mac)
$(error gprof is not supported on mac)
endif # mac
endif # gprof

# check argument os
ifeq ($(os), generic)
OS_CFLAGS  :=
OS_LDFLAGS :=
OS_LDLIBS  := -ldl -lm
else
ifeq ($(os), freebsd)
OS_CFLAGS  := -I /usr/local/include
OS_LDFLAGS := -L /usr/local/lib
OS_LDLIBS  := -lm -lexecinfo
else
ifeq ($(os), linux)
OS_CFLAGS  :=
OS_LDFLAGS :=
OS_LDLIBS  := -ldl -lm -lrt
else
ifeq ($(os), mac)
OS_CFLAGS  :=
OS_LDFLAGS :=
OS_LDLIBS  :=
else
$(error $(os) not supported at present)
endif # mac
endif # linux
endif # freebsd
endif # generic

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

CFLAGS  := $(BASIC_CFLAGS)  $(OS_CFLAGS)  $(DEBUG_CFLAGS)  $(MALLOC_CFLAGS)  $(PROFILE_CFLAGS) $(OPTIMIZE_CFLAGS)

LDFLAGS := $(BASIC_LDFLAGS) $(OS_LDFLAGS) $(DEBUG_LDFLAGS) $(MALLOC_LDFLAGS) $(PROFILE_LDFLAGS) $(OPTIMIZE_LDFLAGS)

LDLIBS  := $(BASIC_LDLIBS)  $(OS_LDLIBS)  $(DEBUG_LDFLAGS) $(MALLOC_LDLIBS)  $(PROFILE_LDLIBS) $(OPTIMIZE_LIBS)

#--------------------------------------------------------------
# sources && objects && Makefile.dep
#--------------------------------------------------------------
SVRBIN  := freekick
SVRSRCS := fk_buf.c fk_conf.c fk_ev.c fk_list.c fk_log.c                        \
           fk_mem.c fk_sock.c fk_str.c fk_util.c fk_heap.c                      \
           fk_pool.c fk_item.c fk_dict.c fk_vtr.c fk_cache.c                    \
           fk_skiplist.c fk_svr.c fk_svr_conn.c fk_svr_proto.c fk_svr_lua.c     \
           fk_svr_str.c fk_svr_hash.c fk_svr_list.c fk_svr_zset.c fk_svr_fkdb.c \
           fk_svr_blog.c fk_svr_db.c freekick.c
SVROBJS := $(patsubst %.c, %.o, $(SVRSRCS))
DEPS    := Makefile.dep

.PHONY : all clean

all : $(SVRBIN)

$(SVRBIN) : $(SVROBJS) 
	@echo "[Linking...]"
	$(CC) -o $(SVRBIN) $(SVROBJS) $(LDFLAGS) $(LDLIBS)

-include $(DEPS)

# if Makefile.dep does not exist, this target will be executed
# this target is always the first part to be executed
$(DEPS) :
	@echo "[Generating Makefile.dep...]"
	$(CC) -MM $(CFLAGS) $(SVRSRCS) > $(DEPS)

clean :
	@echo "[Removing all the objects...]"
	$(RM) $(SVROBJS) $(SVRBIN) $(DEPS)
