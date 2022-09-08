SRCDIR   := ../src
os := $(shell sh -c 'uname -s 2>/dev/null')

# check argument os
ifeq ($(os), FreeBSD)
OS_LDLIBS  := -lm -lexecinfo
MPLX_MOD   := fk_ev_kqueue.c
#MAKE       := gmake
else
ifeq ($(os), Linux)
OS_LDLIBS  := -ldl -lm -lrt
MPLX_MOD   := fk_ev_epoll.c
#MAKE       := make
else
ifeq ($(os), Darwin)
OS_LDLIBS  :=
MPLX_MOD   := fk_ev_kqueue.c
#MAKE       := make
else
ifeq ($(findstring CYGWIN, $(os)), CYGWIN)
OS_LDLIBS  :=
MPLX_MOD   := fk_ev_poll.c
#MAKE       := make
else
$(error $(os) is not supported at present)
endif # CYGWIN
endif # Darwin
endif # Linux
endif # FreeBSD

# --------------------------------------------------------
# use jemalloc
# --------------------------------------------------------
CC 		:= gcc 
CFLAGS	:= -std=c99 -Wall -g -D JEMALLOC_MANGLE
CFLAGS 	+= -I $(SRCDIR)/
LDFLAGS := 
LDFLAGS += $(OS_LDLIBS)


TMODBIN := t
# --------------------------------------------------------
# "freekick.o"  "fk_svr.o" "fk_svr_xxx.o" should not be included here
# "t.o" should be included here
# --------------------------------------------------------
TMODSRCS := $(SRCDIR)/fk_buf.c $(SRCDIR)/fk_cache.c $(SRCDIR)/fk_conf.c $(SRCDIR)/fk_dict.c $(SRCDIR)/fk_ev.c $(SRCDIR)/fk_heap.c $(SRCDIR)/fk_item.c $(SRCDIR)/fk_list.c $(SRCDIR)/fk_log.c $(SRCDIR)/fk_mem.c $(SRCDIR)/fk_pool.c $(SRCDIR)/fk_sock.c $(SRCDIR)/fk_str.c $(SRCDIR)/fk_util.c $(SRCDIR)/fk_vtr.c $(SRCDIR)/fk_num.c $(SRCDIR)/fk_skiplist.c ./t.c
TMODSRCS += $(SRCDIR)/$(MPLX_MOD)
TMODOBJS := $(patsubst %.c, %.o, $(TMODSRCS))
TMODDEPS := makefile.dep


.PHONY : all clean dep

all : $(TMODBIN)

$(TMODBIN) : $(TMODOBJS) 
	$(CC) $(LDFLAGS) -o $(TMODBIN) $(TMODOBJS)

-include $(TMODDEPS)

dep:
	@$(CC) -MM $(CFLAGS) $(TMODSRCS) > $(TMODDEPS)

clean :
	@$(RM) $(TMODOBJS) $(TMODDEPS) $(TMODBIN)
