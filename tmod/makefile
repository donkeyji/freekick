TMODBIN := tmod

# --------------------------------------------------------
# use jemalloc
# --------------------------------------------------------
CFLAGS := -std=c99 -Wall -g -D JEMALLOC_MANGLE
CFLAGS += -I ../

LDFLAGS := 
LDFLAGS += -ljemalloc

CC = gcc 

# --------------------------------------------------------
# "fk_conn.o" and "freekick.o" should not be included here
# "tmod.o" should be included here
# --------------------------------------------------------
TMODOBJS = ../fk_buf.o ../fk_cache.o ../fk_conf.o ../fk_dict.o ../fk_ev.o ../fk_heap.o ../fk_item.o ../fk_list.o ../fk_log.o ../fk_mem.o ../fk_pool.o ../fk_sock.o ../fk_str.o ../fk_util.o ../fk_vtr.o ./tmod.o

TMODDEPS = $(patsubst %.o,%.d,$(TMODOBJS)) 

.PHONY : all clean

all : $(TMODBIN)

$(TMODDEPS): %.d : %.c 
	@$(CC) -MM $(CFLAGS) $< -o $@

$(TMODBIN) : $(TMODOBJS) 
	$(CC) -o $(TMODBIN) $(TMODOBJS) $(LDFLAGS)

-include $(TMODDEPS)

clean :
	@$(RM) $(TMODOBJS) $(TMODDEPS) $(TMODBIN)
