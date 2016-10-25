#--------------------------------------------------------------
# Usage:
# [g]make os=xxx debug=xxx jemalloc=xxx gprof=xxx optimize=xxx gperf=xxx
#
# os       = generic/mac/linux/freebsd
# debug    = yes/no
# jemalloc = yes/no
# gprof    = yes/no    # GNU profile
# gperf    = yes/no    # google perftool
# optimize = yes/no
#--------------------------------------------------------------

#--------------------------------------------------------------
# CC && CFLAGS && LDFLAGS && LDLIBS
#--------------------------------------------------------------
CC := gcc

BASIC_CFLAGS  := -std=c99 -pedantic -Wall -I .
BASIC_LDFLAGS :=
BASIC_LDLIBS  := -llua

# default value for arguments
os       := generic
debug    := no
optimize := no
jemalloc := no
tcmalloc := no
gprof    := no
gperf	 := no

$(info [Listing Arguments Used...])
$(info os:       $(os))
$(info debug:    $(debug))
$(info optimize: $(optimize))
$(info jemalloc: $(jemalloc))
$(info tcmalloc: $(tcmalloc))
$(info gprof:    $(gprof))
$(info gperf:    $(gperf))

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

# only one profile tool could be employed
ifeq ($(gperf), yes)
ifeq ($(gprof), yes)
$(error gperf and gprof cannot be specified as yes at the same time)
endif
endif

# only one implementation of malloc could be employed
ifeq ($(jemalloc), yes)
ifeq ($(tcmalloc), yes)
$(error jemalloc and tcmalloc cannot be specified as yes at the same time)
endif
endif

# gprof is not supported on mac
ifeq ($(gprof), yes)
ifeq ($(os), mac)
$(error gprof is not supported on mac)
endif
endif

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

ifeq ($(jemalloc), yes)
MALLOC_CFLAGS  := -D FK_USE_JEMALLOC -D JEMALLOC_MANGLE
MALLOC_LDFLAGS :=
MALLOC_LDLIBS  := -ljemalloc
else
ifeq ($(jemalloc), no)
MALLOC_CFLAGS  :=
MALLOC_LDFLAGS :=
MALLOC_LDLIBS  :=
else
$(error illegal value for jemalloc)
endif # no
endif # yes

ifeq ($(tcmalloc), yes)
MALLOC_CFLAGS  :=
MALLOC_LDFLAGS :=
MALLOC_LDLIBS  := -ltcmalloc
else
ifeq ($(tcmalloc), no)
MALLOC_CFLAGS  :=
MALLOC_LDFLAGS :=
MALLOC_LDLIBS  :=
else
$(error illegal value for tcmalloc)
endif # no
endif # yes

ifeq ($(gprof), yes)
GPROF_CFLAGS  := -pg
GPROF_LDFLAGS := -pg
GPROF_LDLIBS  :=
else
ifeq ($(gprof), no)
GPROF_CFLAGS  :=
GPROF_LDFLAGS :=
GPROF_LDLIBS  :=
else
$(error illegal value for gprof)
endif # no
endif # yes

ifeq ($(gperf), yes)
GPERF_CFLAGS  :=
GPERF_LDFLAGS :=
GPERF_LDLIBS  := -lprofiler
else
ifeq ($(gperf), no)
GPERF_CFLAGS  :=
GPERF_LDFLAGS :=
GPERF_LDLIBS  :=
else
$(error illegal value for gperf)
endif # no
endif # yes

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

CFLAGS  := $(BASIC_CFLAGS)  $(OS_CFLAGS)  $(DEBUG_CFLAGS)  $(MALLOC_CFLAGS)  $(GPROF_CFLAGS) $(OPTIMIZE_CFLAGS) $(GPERF_CFLAGS)

LDFLAGS := $(BASIC_LDFLAGS) $(OS_LDFLAGS) $(DEBUG_LDFLAGS) $(MALLOC_LDFLAGS) $(GPROF_LDFLAGS) $(OPTIMIZE_LDFLAGS) $(GPERF_LDFLAGS)

LDLIBS  := $(BASIC_LDLIBS)  $(OS_LDLIBS)  $(DEBUG_LDFLAGS) $(MALLOC_LDLIBS)  $(GPROF_LDLIBS) $(OPTIMIZE_LIBS) $(GPERF_LDLIBS)

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
