TMODBIN := tmod

CFLAGS := -std=c99 -Wall -g
CFLAGS += -I ../

LDFLAGS := 
LDFLAGS +=

CC = gcc 

TMODOBJS = ../fk_buf.o ../fk_conf.o ../fk_ev.o ../fk_list.o ../fk_log.o ../fk_mem.o  ../fk_sock.o ../fk_str.o ../fk_util.o ../fk_heap.o ../fk_obj.o ../fk_pool.o ../fk_dict.o ./tmod.o

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
