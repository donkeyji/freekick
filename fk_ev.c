#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <errno.h>

#include <fk_conf.h>
#include <fk_ev.h>
#include <fk_list.h>
#include <fk_mem.h>
#include <fk_util.h>
#include <fk_macro.h>
#include <fk_log.h>

static int fk_ev_ioev_activate(int fd, unsigned char type);
static fk_tmev *fk_ev_nearest_tmev_get();
static int fk_ev_pending_tmev_update();
static int fk_ev_active_ioev_proc();
static int fk_ev_expired_tmev_proc();
static int fk_ev_tmev_cmp(void *tmev1, void *tmev2);

#if defined(__linux__)
#include <fk_epoll.c>
	fk_mpxop *mpxop = &epoll_op;
#elif defined(__FreeBSD__) || defined(__APPLE__)
#include <fk_kqueue.c>
	fk_mpxop *mpxop = &kqueue_op;
#else
#include <fk_poll.c>
	fk_mpxop *mpxop = &poll_op;
#endif

static fk_evmgr evmgr;

static fk_leaf_op tmev_func = {
	fk_ev_tmev_cmp
};

void fk_ev_init()
{
	int max_fds;

	max_fds = setting.max_conn + 1 + FK_SAVED_FD;
	evmgr.timer_list = fk_list_create(NULL);
	evmgr.timer_heap = fk_heap_create(&tmev_func);
	//use macro to initialize this two member
	evmgr.exp_tmev = FK_EV_LIST_CREATE(fk_tmev_list, evmgr.exp_tmev);
	evmgr.act_ioev = FK_EV_LIST_CREATE(fk_ioev_list, evmgr.act_ioev);
	evmgr.read_ev = (fk_ioev **)fk_mem_alloc(sizeof(fk_ioev *) * max_fds);
	evmgr.write_ev = (fk_ioev **)fk_mem_alloc(sizeof(fk_ioev *) * max_fds);

	//io mode
	evmgr.iompx = mpxop->iompx_create(max_fds);

	if (evmgr.timer_list == NULL
		||evmgr.timer_heap == NULL
		||evmgr.exp_tmev == NULL
		||evmgr.act_ioev == NULL
		||evmgr.read_ev == NULL
		||evmgr.write_ev == NULL
		||evmgr.iompx == NULL)
	{
		fk_log_error("evmgr init failed\n");
		exit(EXIT_FAILURE);
	}
}

void fk_ev_end()
{
	//fk_list_destroy(evmgr.timer_list);
	//fk_list_destroy(evmgr.act_ioev);
}

int fk_ev_dispatch()
{
	fk_tmev *tmev;
	struct timeval now, timeout;
	struct timeval *pto;

	pto = NULL;//indefinitely wait

	tmev = fk_ev_nearest_tmev_get(evmgr);
	if (tmev != NULL) {
		gettimeofday(&now, NULL);
		FK_UTIL_TMVAL_SUB(&(tmev->expire), &now, &timeout);
		if ((FK_UTIL_TV2MS(&timeout)) < 0) {
			timeout.tv_sec = 0;
			timeout.tv_usec = 0;
		}
		pto = &timeout;
	}

	/* pto:
	 * (1)NULL: wait indefinitely
	 * (2){0,0}: return immediately
	 * (3)>{0,0}: wait for a period
	 * cannot be < {0, 0}
	 */
	mpxop->iompx_dispatch((&evmgr)->iompx, pto);

	//remove the nearest timer from timer_list,
	//insert it to the exp_tmev
	fk_ev_pending_tmev_update();

	fk_ev_active_ioev_proc();

	fk_ev_expired_tmev_proc();

	return 0;
}

int fk_ev_ioev_add(fk_ioev *ioev)
{
	int fd;
	unsigned char type;

	fd = ioev->fd;
	type = ioev->type;
	if (type & FK_EV_READ) {
		evmgr.read_ev[fd] = ioev;
	}
	if (type & FK_EV_WRITE) {
		evmgr.write_ev[fd] = ioev;
	}

	mpxop->iompx_add((&evmgr)->iompx, fd, type);
	return 0;
}

int fk_ev_ioev_remove(fk_ioev *ioev)
{
	int fd;
	unsigned char type;

	//must be in the read/write event array
	fd = ioev->fd;
	type = ioev->type;
	if (type & FK_EV_READ) {
		evmgr.read_ev[fd] = NULL;
	}
	if (type & FK_EV_WRITE) {
		evmgr.write_ev[fd] = NULL;
	}
	mpxop->iompx_remove((&evmgr)->iompx, fd, type);

	//maybe this ioev in active list
	if (ioev->prev != NULL || ioev->next != NULL || evmgr.act_ioev->head == ioev) {
		FK_EV_LIST_REMOVE(evmgr.act_ioev, ioev);
	}
	return 0;
}

fk_ioev *fk_ev_ioev_create(int fd, unsigned char type, void *arg, file_ev_cb iocb)
{
	fk_ioev *ioev;

	ioev = (fk_ioev *)fk_mem_alloc(sizeof(fk_ioev));
	ioev->fd = fd;
	ioev->type = type;
	ioev->arg = arg;
	ioev->iocb = iocb;

	ioev->prev = NULL;
	ioev->next = NULL;
	return ioev;
}

void fk_ev_ioev_destroy(fk_ioev *ioev)
{
	fk_mem_free(ioev);
}

/* no need to delete timer
 * so fk_ev_timer_del not supplied
 */
fk_tmev *fk_ev_tmev_create(int interval, unsigned char type, void *arg, timer_ev_cb tmcb)
{
	fk_tmev *tmev;

	tmev = (fk_tmev *)fk_mem_alloc(sizeof(fk_tmev));
	tmev->type = type;
	tmev->interval = interval;
	tmev->arg = arg;
	tmev->tmcb = tmcb;
	fk_util_cal_expire(&(tmev->expire), interval);

	tmev->prev = NULL;
	tmev->next = NULL;
	tmev->idx = -1;
	return tmev;
}

void fk_ev_tmev_destroy(fk_tmev *tmev)
{
	//just free memory, no other things to do
	fk_mem_free(tmev);
}

int fk_ev_tmev_add(fk_tmev *tmev)
{
	fk_heap *theap;

	theap = evmgr.timer_heap;
	fk_heap_push(theap, (fk_leaf *)tmev);
	return 0;
}

int fk_ev_tmev_remove(fk_tmev *tmev)
{
	if (tmev->idx != -1) {//maybe in the min heap
		fk_heap_remove(evmgr.timer_heap, (fk_leaf *)tmev);
	}

	//maybe this tmev in expired list
	if (tmev->prev != NULL || tmev->next != NULL || evmgr.exp_tmev->head == tmev) {
		FK_EV_LIST_REMOVE(evmgr.exp_tmev, tmev);
	}

	return 0;
}

//---------------------------
//do not return fk_tmev obj
//instead, evmgr own this tmev,
//and create/destroy this tmev itself
//---------------------------
int fk_ev_tmev_reg(int interval, unsigned char type, void *arg, timer_ev_cb tmcb)
{
	fk_tmev *tmev;

	tmev = (fk_tmev *)fk_mem_alloc(sizeof(fk_tmev));
	tmev->type = type;
	tmev->interval = interval;
	tmev->arg = arg;
	tmev->tmcb = tmcb;
	fk_util_cal_expire(&(tmev->expire), interval);

	fk_ev_tmev_add(tmev);
	return 0;
}

int fk_ev_pending_tmev_update()
{
	int cmp;
	fk_leaf *root;
	fk_tmev *tmev;
	struct timeval now;

	gettimeofday(&now, NULL);

	root = fk_heap_root(evmgr.timer_heap);
	while (root != NULL) {
		tmev = (fk_tmev *)root;
		cmp = FK_UTIL_TMVAL_CMP(&now, &(tmev->expire));
		if (cmp >= 0) {
			fk_heap_pop(evmgr.timer_heap);//pop root from the heap
			FK_EV_LIST_INSERT(evmgr.exp_tmev, tmev);//add to the exp list
			root = fk_heap_root(evmgr.timer_heap);//get new root
		} else {//break directly
			break;
		}
	}
	return 0;
}

int fk_ev_expired_tmev_proc()
{
	void *arg;
	int interval, rt;
	fk_tmev *tmev, *cur;
	timer_ev_cb tmcb;
	unsigned char type;

	tmev = evmgr.exp_tmev->head;
	while (tmev != NULL) {
		arg = tmev->arg;
		tmcb = tmev->tmcb;
		type = tmev->type;
		interval = tmev->interval;

		cur = tmev;//save current position
		tmev = tmev->next;//go to the next position
		//step 1: remove the expired tmev from the expired list first!!!!
		FK_EV_LIST_REMOVE(evmgr.exp_tmev, cur);//remove current from the expired list
		//step 2: call the callback of the expired tmev
		rt = tmcb(interval, type, arg);
		//step 3: how to handle the return value of the callback???
		if (rt == 0) {
			if (type == FK_EV_CYCLE) {
				fk_util_cal_expire(&(cur->expire), interval);//calculate the expire time again
				fk_heap_push(evmgr.timer_heap, (fk_leaf *)cur);//push into the heap once more
			}
		}
	}

	return 0;
}

int fk_ev_active_ioev_proc()
{
	int fd, rt;
	void *arg;
	file_ev_cb iocb;
	unsigned char type;
	fk_ioev *ioev, *cur;

	ioev = evmgr.act_ioev->head;
	while (ioev != NULL) {
		fd = ioev->fd;
		arg = ioev->arg;
		iocb = ioev->iocb;
		type = ioev->type;

		cur = ioev;//save the current position
		ioev = ioev->next;//go to the next positon

		//step 1: remove the active ioev from the active list first!!!!
		FK_EV_LIST_REMOVE(evmgr.act_ioev, cur);
		//step 2: call the callback of the active ioev
		rt = iocb(fd, type, arg);
		//step 3: how to process the return value of the callback????
		if (rt < 0) {
		}
	}

	return 0;
}

fk_tmev *fk_ev_nearest_tmev_get()
{
	void *root;
	fk_tmev *tmev;

	root = fk_heap_root(evmgr.timer_heap);
	tmev = (fk_tmev *)root;
	return tmev;
}

int fk_ev_ioev_activate(int fd, unsigned char type)
{
	fk_ioev *ioev;

	if (type & FK_EV_READ) {
		ioev = evmgr.read_ev[fd];
	}
	if (type & FK_EV_WRITE) {
		ioev = evmgr.write_ev[fd];
	}
	FK_EV_LIST_INSERT(evmgr.act_ioev, ioev);
	return 0;
}

int fk_ev_tmev_cmp(void *tmev1, void *tmev2)
{
	fk_tmev *t1, *t2;

	t1 = (fk_tmev *)tmev1;
	t2 = (fk_tmev *)tmev2;
	return FK_UTIL_TMVAL_CMP(&(t1->expire), &(t2->expire));
}
