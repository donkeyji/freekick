SVRBIN := freekick
CLTBIN := clt_test

SRCDIRS := .

SRCEXTS := .c

CFLAGS := -I . -std=gnu99 -Wall -D FK_DEBUG
ifeq ($(gp),y)
CFLAGS += -g -pg -lc_p
endif

LDFLAGS := -ljemalloc
ifeq ($(gp),y)
LDFLAGS += -pg
endif

CC = gcc

#SVRSRCS = $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCEXTS)))) 
SVRSRCS = fk_buf.c fk_conf.c fk_ev.c fk_list.c fk_log.c fk_mem.c fk_sock.c fk_str.c fk_util.c fk_conn.c fk_heap.c fk_obj.c fk_pool.c fk_hash.c fk_vtr.c fk_cache.c freekick.c

CLTSRCS = clt_test.c

SVROBJS = $(foreach x,$(SRCEXTS), $(patsubst %$(x),%.o,$(filter %$(x),$(SVRSRCS)))) 
CLTOBJS = $(foreach x,$(SRCEXTS), $(patsubst %$(x),%.o,$(filter %$(x),$(CLTSRCS)))) 

#SVRDEPS = $(patsubst %.o,%.d,$(SVROBJS)) 
#CLTDEPS = $(patsubst %.o,%.d,$(CLTOBJS)) 
DEPS = Makefile.dep

.PHONY : all clean rebuild

all : $(SVRBIN) $(CLTBIN)

$(SVRBIN) : $(SVROBJS) 
	$(CC) -o $(SVRBIN) $(SVROBJS) $(LDFLAGS)

$(CLTBIN) : $(CLTOBJS) 
	$(CC) -o $(CLTBIN) $(CLTOBJS) $(LDFLAGS)

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
	@$(RM) $(SVROBJS) $(CLTOBJS) $(SVRBIN) $(CLTBIN) $(DEPS)
