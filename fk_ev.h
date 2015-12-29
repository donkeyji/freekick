#ifndef _FK_EV_H_
#define _FK_EV_H_

#include <sys/time.h>

#include <fk_list.h>
#include <fk_heap.h>

#define FK_EV_OK		0
#define FK_EV_ERR		-1

/* for file ev */
#define FK_IOEV_READ 	0x01
#define FK_IOEV_WRITE 	0x02

/* for timer ev */
#define FK_TMEV_CYCLE 	1
#define FK_TMEV_ONCE	2

typedef int (*fk_ioev_cb) (int, char, void *);
typedef int (*fk_tmev_cb) (unsigned, char, void *);

typedef struct _fk_ioev {
	int fd;
	char type;/* FK_IOEV_READ|FK_IOEV_WRITE */
	int active;/* whether in active list */
	void *arg;
	fk_ioev_cb iocb;

	struct _fk_ioev *prev;
	struct _fk_ioev *next;
} fk_ioev;

typedef struct _fk_tmev {
	FK_HEAP_LEAF_HEADER;/* for heap */

	int expired;/* whether in the expired list */
	char type;/* FK_TMEV_CYCLE|FK_TMEV_ONCE */
	unsigned interval;/* milliseconds */
	struct timeval when;/* save the trigger point time: now + timeout */
	void *arg;/* ext arg */
	fk_tmev_cb tmcb;

	struct _fk_tmev *prev;
	struct _fk_tmev *next;
} fk_tmev;

fk_rawlist_def(fk_ioev, fk_ioev_list);
fk_rawlist_def(fk_tmev, fk_tmev_list);

typedef struct _fk_mpxop {
	char *iompx_name;/* fk_str is not used here, because it's easier to use char* here */
	void *(*iompx_create)(unsigned max_files);
	int (*iompx_add)(void *iompx, int fd, char type);
	int (*iompx_remove)(void *iompx, int fd, char type);
	int (*iompx_dispatch)(void *iompx, struct timeval *timeout);
} fk_mpxop;

typedef struct _fk_evmgr {
	int stop;
	unsigned max_files;
	/* use min_heap to save timer ev */
	fk_heap *timer_heap;

	/* actived file ev */
	fk_ioev_list *act_ioev;
	fk_tmev_list *exp_tmev;/* when SINGLE expire, fk_mem_free() */

	/* use array to save file ev */
	/* conn + listen */
	fk_ioev **read_ev;/* max_files */
	fk_ioev **write_ev;/* max_files */

	/* implemention of epoll/kqueue/poll */
	void *iompx;
} fk_evmgr;

void fk_ev_init(unsigned max_files);
int fk_ev_dispatch();
void fk_ev_cycle();
void fk_ev_stop();
int fk_ev_add_ioev(fk_ioev *ioev);
int fk_ev_remove_ioev(fk_ioev *ioev);
int fk_ev_add_tmev(fk_tmev *tmev);
int fk_ev_remove_tmev(fk_tmev *tmev);

char *fk_ev_iompx_name();

fk_ioev *fk_ioev_create(int fd, char type, void *arg, fk_ioev_cb iocb);
void fk_ioev_destroy(fk_ioev *ioev);
fk_tmev *fk_tmev_create(unsigned timeout, char type, void *arg, fk_tmev_cb tmcb);
void fk_tmev_destroy(fk_tmev *tmev);

#endif
