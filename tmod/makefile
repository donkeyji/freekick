TMODBIN := tmod

SRCDIRS := .

SRCEXTS := .c

CFLAGS := -std=c99 -Wall -g
CFLAGS += -I ../

LDFLAGS := 
LDFLAGS +=

CC = gcc 

TMODSRCS = ../fk_buf.c ../fk_conf.c ../fk_ev.c ../fk_list.c ../fk_log.c ../fk_mem.c ../fk_sig.c ../fk_sock.c ../fk_str.c ../fk_util.c ../fk_heap.c ../fk_obj.c ../fk_pool.c ../fk_dict.c ./tmod.c

TMODOBJS = $(foreach x,$(SRCEXTS), $(patsubst %$(x),%.o,$(filter %$(x),$(TMODSRCS)))) 
TMODDEPS = $(patsubst %.o,%.d,$(TMODOBJS)) 

.PHONY : all clean cleanall rebuild

all : $(TMODBIN)

$(TMODDEPS): %.d : %.c 
	@$(CC) -MM $(CFLAGS) $< -o $@

$(TMODBIN) : $(TMODOBJS) 
	$(CC) -o $(TMODBIN) $(TMODOBJS) $(LDFLAGS)

-include $(TMODDEPS)

rebuild: clean all

clean :
	@$(RM) *.o *.d

cleanall: clean
	@$(RM) $(TMODBIN)
