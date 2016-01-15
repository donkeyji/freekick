
# --------------------------------------------------------
# use jemalloc
# --------------------------------------------------------
CC = gcc 
CFLAGS := -std=c99 -Wall -g -D JEMALLOC_MANGLE
CFLAGS += -I ../
LDFLAGS := 
LDFLAGS += -ljemalloc


TMODBIN := tmod
# --------------------------------------------------------
# "freekick.o"  "fk_svr.o" "fk_svr_xxx.o" should not be included here
# "tmod.o" should be included here
# --------------------------------------------------------
TMODOBJS = ../fk_buf.o ../fk_cache.o ../fk_conf.o ../fk_dict.o ../fk_ev.o ../fk_heap.o ../fk_item.o ../fk_list.o ../fk_log.o ../fk_mem.o ../fk_pool.o ../fk_sock.o ../fk_str.o ../fk_util.o ../fk_vtr.o ../fk_skiplist.o ./tmod.o

TMODDEPS = $(patsubst %.o,%.d,$(TMODOBJS)) 

.PHONY : all clean

all : $(TMODBIN)

$(TMODBIN) : $(TMODOBJS) 
	$(CC) $(LDFLAGS) -o $(TMODBIN) $(TMODOBJS)

$(TMODDEPS): %.d : %.c 
	@$(CC) -MM $(CFLAGS) $< -o $@

-include $(TMODDEPS)

clean :
	@$(RM) $(TMODOBJS) $(TMODDEPS) $(TMODBIN)
