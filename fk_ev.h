#ifndef _FK_EV_H_
#define _FK_EV_H_

#include <sys/time.h>
#include <fk_list.h>
#include <fk_heap.h>

//for file ev
#define FK_EV_READ 	0x01
#define FK_EV_WRITE 	0x02

//for timer ev
#define FK_EV_CYCLE 	0x01
#define FK_EV_DELAY 	0x02

typedef int (*file_ev_cb) (int, unsigned char, void *);
typedef int (*timer_ev_cb) (int, unsigned char, void *);

typedef struct _fk_ioev {
	unsigned char type;//FK_EV_READ|FK_EV_WRITE
	int fd;
	void *arg;
	file_ev_cb iocb;

	struct _fk_ioev *prev;
	struct _fk_ioev *next;
} fk_ioev;

typedef struct _fk_tmev {
	FK_HEAP_LEAF_HEADER;//for heap

	unsigned char type;//FK_EV_CYCLE|FK_EV_DELAY
	int interval;//milliseconds
	struct timeval expire;//save the trigger time: now + timeout
	void *arg;//ext arg
	timer_ev_cb tmcb;

	struct _fk_tmev *prev;
	struct _fk_tmev *next;
} fk_tmev;

#define def_list(type, name)	\
typedef struct {			\
	type *head;					\
	type *tail;					\
	int len;					\
} name

def_list(fk_ioev, fk_ioev_list);
def_list(fk_tmev, fk_tmev_list);

#define fk_ev_list_insert(lst, nd) {	\
    if (lst->head == NULL) {			\
        nd->next = NULL;				\
        nd->prev = NULL;				\
        lst->tail = nd;					\
    } else {							\
        nd->prev = lst->head->prev;		\
        nd->next = lst->head;			\
    }									\
    lst->head = nd;						\
    lst->len++;							\
}

#define fk_ev_list_remove(lst, nd) {		\
    if (lst->len > 0) {						\
		if (lst->len == 1) {				\
			lst->head = NULL;				\
			lst->tail = NULL;				\
		} else {							\
			if (nd == lst->head) {			\
				nd->next->prev = nd->prev;	\
				lst->head = nd->next;		\
			} else if (nd == lst->tail) {	\
				nd->prev->next = nd->next;	\
				lst->tail = nd->prev;		\
			} else {						\
				nd->next->prev = nd->prev;	\
				nd->prev->next = nd->next;	\
			}								\
		}									\
		lst->len--;							\
	}										\
}

#define fk_ev_list_create(type, lst)	\
	(type *)fk_mem_alloc(sizeof(type));\
	lst->head = NULL;					\
	lst->tail = NULL;					\
	lst->len = 0;						\
 
typedef struct _fk_mpxop {
	void *(*iompx_create)(int max_conn);
	int (*iompx_add)(void *iompx, int fd, unsigned char type);
	int (*iompx_remove)(void *iompx, int fd, unsigned char type);
	int (*iompx_dispatch)(void *iompx, struct timeval *timeout);
} fk_mpxop;

typedef struct _fk_evmgr {
	//use list to save timer ev
	fk_list *timer_list;//
	fk_heap *timer_heap;

	//actived file ev
	fk_ioev_list *act_ioev;
	fk_tmev_list *exp_tmev;//when SINGLE expire, fk_mem_free()

	//use array to save file ev
	//conn + listen
	fk_ioev **read_ev;//max_conn + 1
	fk_ioev **write_ev;//max_conn + 1

	//epoll/kqueue
	void *iompx;
} fk_evmgr;

void fk_ev_init();
void fk_ev_end();
int fk_ev_dispatch();
fk_ioev *fk_ev_ioev_create(int fd, unsigned char type, void *arg, file_ev_cb iocb);
void fk_ev_ioev_destroy(fk_ioev *ioev);
int fk_ev_ioev_add(fk_ioev *ioev);
int fk_ev_ioev_remove(fk_ioev *ioev);
fk_tmev *fk_ev_tmev_create(int timeout, unsigned char type, void *arg, timer_ev_cb tmcb);
void fk_ev_tmev_destroy(fk_tmev *tmev);
int fk_ev_tmev_add(fk_tmev *tmev);
int fk_ev_tmev_remove(fk_tmev *tmev);
int fk_ev_tmev_reg(int interval, unsigned char type, void *arg, timer_ev_cb tmcb);

#endif
