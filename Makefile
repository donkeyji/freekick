SVRBIN := freekick

SRCDIRS := .

SRCEXTS := .c

INC := -I .
BASICOPT := -std=gnu99 -Wall -D JEMALLOC_MANGLE
ifeq ($(release),y)
CFLAGS := $(BASICOPT) $(INC) -O2
else
CFLAGS := $(BASICOPT) $(INC) -D FK_DEBUG
endif

ifeq ($(gp),y)
CFLAGS += -g -pg -lc_p
endif

# ------------------------------------------------------------
# -ldl: needed when in linux, but not in mac
# -lm: needed when in linux, but not in mac
# ------------------------------------------------------------
LDFLAGS := -ljemalloc -llua -ldl -lm
ifeq ($(gp),y)
LDFLAGS += -pg
endif

CC = gcc

#SVRSRCS = $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCEXTS)))) 
SVRSRCS = fk_buf.c fk_conf.c fk_ev.c fk_list.c fk_log.c fk_mem.c fk_sock.c fk_str.c fk_util.c fk_conn.c fk_heap.c fk_item.c fk_dict.c fk_vtr.c fk_cache.c fk_lua.c fk_sklist.c fk_cmd_str.c fk_cmd_hash.c fk_cmd_list.c fk_cmd_zset.c fk_dump.c fk_db.c freekick.c

SVROBJS = $(foreach x,$(SRCEXTS), $(patsubst %$(x),%.o,$(filter %$(x),$(SVRSRCS)))) 

#SVRDEPS = $(patsubst %.o,%.d,$(SVROBJS)) 
DEPS = Makefile.dep

.PHONY : all clean rebuild

all : $(SVRBIN)

$(SVRBIN) : $(SVROBJS) 
	$(CC) -o $(SVRBIN) $(SVROBJS) $(LDFLAGS)

#-include $(SVRDEPS) $(CLTDEPS)
-include $(DEPS)

$(DEPS): $(SVRSRCS) $(CLTSRCS)
	@$(CC) -MM $(CFLAGS) $^ > $(DEPS)

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

clean :
	@$(RM) $(SVROBJS) $(SVRBIN) $(DEPS)
