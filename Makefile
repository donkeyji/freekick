SVRBIN := freekick

SRCDIRS := .

SRCEXTS := .c

INCLUDE := -I .
BASIC_CFLAGS := -std=gnu99 -Wall
BASIC_LDFLAGS := -llua -ldl -lm

# debug mode is the default
ifeq ($(release),y)
EXTRA_CFLAGS := -O2
else
EXTRA_CFLAGS := -D FK_DEBUG -g
endif

# the default malloc/free is original libc malloc/free
ifeq ($(jemalloc),y)
MALLOC_CFLAGS := -D USE_JEMALLOC -D JEMALLOC_MANGLE
MALLOC_LDFLAGS := -ljemalloc
endif

# gprof
ifeq ($(gprof),y)
GPROF_CFLAGS := -g -pg -lc_p
GPROF_LDFLAGS := -pg -lc_p
endif

CFLAGS := $(INCLUDE) $(BASIC_CFLAGS) $(EXTRA_CFLAGS) $(MALLOC_CFLAGS) $(GPROF_CFLAGS)

# ------------------------------------------------------------
# -ldl: needed when in linux, but not in mac
# -lm: needed when in linux, but not in mac
# ------------------------------------------------------------
LDFLAGS := $(BASIC_LDFLAGS) $(MALLOC_LDFLAGS) $(GPROF_LDFLAGS)


CC = gcc

#SVRSRCS = $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCEXTS)))) 
SVRSRCS = fk_buf.c fk_conf.c fk_ev.c fk_list.c fk_log.c fk_mem.c fk_sock.c fk_str.c fk_util.c fk_heap.c fk_pool.c fk_item.c fk_dict.c fk_vtr.c fk_cache.c fk_skiplist.c fk_svr.c fk_svr_conn.c fk_svr_proto.c fk_svr_lua.c fk_svr_str.c fk_svr_hash.c fk_svr_list.c fk_svr_zset.c fk_svr_fkdb.c fk_svr_db.c freekick.c

SVROBJS = $(foreach x,$(SRCEXTS), $(patsubst %$(x),%.o,$(filter %$(x),$(SVRSRCS)))) 

#SVRDEPS = $(patsubst %.o,%.d,$(SVROBJS)) 
DEPS = Makefile.dep

.PHONY : all clean rebuild

all : $(SVRBIN)

$(SVRBIN) : $(SVROBJS) 
	$(CC) -o $(SVRBIN) $(SVROBJS) $(LDFLAGS)

#-include $(SVRDEPS) $(CLTDEPS)
-include $(DEPS)

$(DEPS):
	@$(CC) -MM $(CFLAGS) $(SVRSRCS) > $(DEPS)

#$(CLTDEPS): %.d : %.c 
	#@$(CC) -MM $(CFLAGS) $< -o $@

#$(SVRDEPS): %.d : %.c 
	#@$(CC) -MM $(CFLAGS) $< -o $@
#----------------------------------------------
# $(CFLAGS) is used automatically, no it can be omitted
# the line below work well as the line above
#----------------------------------------------
	#@$(CC) -MM $< -o $@
#----------------------------------------------
#-MD is not right here, -MM is enough
#----------------------------------------------
	#@$(CC) -MM -MD $(CFLAGS) $< -o $@

#----------------------------------------------
# the line below is unnecessary
#----------------------------------------------
#%.o : %.c 
	#$(CC) -c $(CFLAGS) $< -o $@

#----------------------------------------------
# include all the *.d file
# let make know how to compile each *.c to *.o
# $(CFLAGS) is used here automatically
# e.g.
# a.o : a.c a.h b.h
# 	$(CC) $(CFLAGS) -c $< -o $@
#----------------------------------------------

rebuild: clean all

clean:
	@$(RM) $(SVROBJS) $(SVRBIN) $(DEPS)
