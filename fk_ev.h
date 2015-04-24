#ifndef _FK_EV_H_
#define _FK_EV_H_

#include <sys/time.h>
#include <fk_list.h>
#include <fk_heap.h>

/*for file ev*/
#define FK_IOEV_READ 		0x01
#define FK_IOEV_WRITE 	0x02

/*for timer ev*/
#define FK_TMEV_CYCLE 	1
#define FK_TMEV_ONCE		2

typedef int (*fk_ioev_cb) (int, char, void *);
typedef int (*fk_tmev_cb) (int, char, void *);

typedef struct _fk_ioev {
	int fd;
	char type;/*FK_IOEV_READ|FK_IOEV_WRITE*/
	int active;/*whether in active list*/
	void *arg;
	fk_ioev_cb iocb;

	struct _fk_ioev *prev;
	struct _fk_ioev *next;
} fk_ioev;

typedef struct _fk_tmev {
	FK_HEAP_LEAF_HEADER;/*for heap*/

	int expired;/*whether in the expired list*/
	char type;/*FK_TMEV_CYCLE|FK_TMEV_ONCE*/
	int interval;/*milliseconds*/
	struct timeval when;/*save the trigger point time: now + timeout*/
	void *arg;/*ext arg*/
	fk_tmev_cb tmcb;

	struct _fk_tmev *prev;
	struct _fk_tmev *next;
} fk_tmev;

fk_rawlist_def(fk_ioev, fk_ioev_list);
fk_rawlist_def(fk_tmev, fk_tmev_list);

typedef struct _fk_mpxop {
	void *(*iompx_create)(int max_conn);
	int (*iompx_add)(void *iompx, int fd, char type);
	int (*iompx_remove)(void *iompx, int fd, char type);
	int (*iompx_dispatch)(void *iompx, struct timeval *timeout);
} fk_mpxop;

typedef struct _fk_evmgr {
	/*use list to save timer ev*/
	fk_list *timer_list;
	fk_heap *timer_heap;

	/*actived file ev*/
	fk_ioev_list *act_ioev;
	fk_tmev_list *exp_tmev;/*when SINGLE expire, fk_mem_free()*/

	/*use array to save file ev*/
	/*conn + listen*/
	fk_ioev **read_ev;/*max_conn + 1*/
	fk_ioev **write_ev;/*max_conn + 1*/

	/*epoll/kqueue*/
	void *iompx;
} fk_evmgr;

void fk_ev_init();
int fk_ev_dispatch();
int fk_ev_ioev_add(fk_ioev *ioev);
int fk_ev_ioev_remove(fk_ioev *ioev);
int fk_ev_tmev_add(fk_tmev *tmev);
int fk_ev_tmev_remove(fk_tmev *tmev);

fk_ioev *fk_ioev_create(int fd, char type, void *arg, fk_ioev_cb iocb);
void fk_ioev_destroy(fk_ioev *ioev);
fk_tmev *fk_tmev_create(int timeout, char type, void *arg, fk_tmev_cb tmcb);
void fk_tmev_destroy(fk_tmev *tmev);

#endif
