#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <strings.h>

#include <fk_conf.h>
#include <fk_ev.h>
#include <fk_list.h>
#include <fk_mem.h>
#include <fk_util.h>
#include <fk_macro.h>
#include <fk_log.h>

static int fk_ev_ioev_activate(int fd, char type);
static fk_tmev *fk_ev_nearest_tmev_get();
static int fk_ev_pending_tmev_update();
static int fk_ev_active_ioev_proc();
static int fk_ev_expired_tmev_proc();
static int fk_ev_tmev_cmp(fk_leaf *tmev1, fk_leaf *tmev2);

#if defined(__linux__)
	#include <fk_epoll.c>
	static fk_mpxop *mpxop = &epoll_op;
#elif defined(__FreeBSD__) || defined(__APPLE__)
	#include <fk_kqueue.c>
	static fk_mpxop *mpxop = &kqueue_op;
#else
	#include <fk_poll.c>
	static fk_mpxop *mpxop = &poll_op;
#endif

static fk_evmgr evmgr;

static fk_leaf_op tmev_op = {
	fk_ev_tmev_cmp
};

void fk_ev_init()
{
	int max_files;

	max_files = FK_MAXCONN_2_MAXFILES(setting.max_conn); 

	evmgr.timer_list = fk_list_create(NULL);
	evmgr.timer_heap = fk_heap_create(&tmev_op);

	//use macro to initialize this two member
	evmgr.read_ev = (fk_ioev **)fk_mem_alloc(sizeof(fk_ioev *) * max_files);
	evmgr.write_ev = (fk_ioev **)fk_mem_alloc(sizeof(fk_ioev *) * max_files);

	evmgr.exp_tmev = fk_rawlist_create(fk_tmev_list);
	fk_rawlist_init(evmgr.exp_tmev);
	evmgr.act_ioev = fk_rawlist_create(fk_ioev_list);
	fk_rawlist_init(evmgr.act_ioev);
	//io mode
	evmgr.iompx = mpxop->iompx_create(max_files);

	if (evmgr.timer_list == NULL ||
		evmgr.timer_heap == NULL ||
		evmgr.exp_tmev == NULL 	 ||
		evmgr.act_ioev == NULL 	 ||
		evmgr.read_ev == NULL 	 ||
		evmgr.write_ev == NULL 	 ||
		evmgr.iompx == NULL)
	{
		fk_log_error("evmgr init failed\n");
		exit(EXIT_FAILURE);
	}
}

int fk_ev_dispatch()
{
	fk_tmev *tmev;
	struct timeval now, timeout;
	struct timeval *pto;

	pto = NULL;/*indefinitely wait*/

	tmev = fk_ev_nearest_tmev_get(evmgr);
	if (tmev != NULL) {
		gettimeofday(&now, NULL);
		fk_util_tmval_sub(&(tmev->when), &now, &timeout);
		if ((fk_util_tv2millis(&timeout)) < 0) {
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

	/*
	 * remove the nearest timer from timer_list, 
	 * insert it to the exp_tmev
	 */
	fk_ev_pending_tmev_update();

	fk_ev_active_ioev_proc();

	fk_ev_expired_tmev_proc();

	return 0;
}

int fk_ev_ioev_add(fk_ioev *ioev)
{
	int fd, rt;
	char type;

	fd = ioev->fd;
	type = ioev->type;

	rt = mpxop->iompx_add((&evmgr)->iompx, fd, type);
	if (rt < 0) {
		return -1;
	}

	if (type & FK_IOEV_READ) {
		evmgr.read_ev[fd] = ioev;
	}
	if (type & FK_IOEV_WRITE) {
		evmgr.write_ev[fd] = ioev;
	}
	return 0;
}

int fk_ev_ioev_remove(fk_ioev *ioev)
{
	int fd, rt;
	char type;

	//must be in the read/write event array
	fd = ioev->fd;
	type = ioev->type;
	rt = mpxop->iompx_remove((&evmgr)->iompx, fd, type);
	if (rt < 0) {
		return -1;
	}

	//maybe this ioev in active list
	if (ioev->active == 1) {
		fk_rawlist_remove_any(evmgr.act_ioev, ioev);
		ioev->active = 0;
	}

	if (type & FK_IOEV_READ) {
		evmgr.read_ev[fd] = NULL;
	}
	if (type & FK_IOEV_WRITE) {
		evmgr.write_ev[fd] = NULL;
	}

	return 0;
}

fk_ioev *fk_ioev_create(int fd, char type, void *arg, fk_ioev_cb iocb)
{
	fk_ioev *ioev;

	ioev = (fk_ioev *)fk_mem_alloc(sizeof(fk_ioev));
	ioev->fd = fd;
	ioev->type = type;
	ioev->arg = arg;
	ioev->iocb = iocb;
	ioev->active = 0;

	ioev->prev = NULL;
	ioev->next = NULL;
	return ioev;
}

void fk_ioev_destroy(fk_ioev *ioev)
{
	fk_mem_free(ioev);
}

/* no need to delete timer
 * so fk_ev_timer_del not supplied
 */
fk_tmev *fk_tmev_create(int interval, char type, void *arg, fk_tmev_cb tmcb)
{
	fk_tmev *tmev;

	tmev = (fk_tmev *)fk_mem_alloc(sizeof(fk_tmev));
	tmev->type = type;
	tmev->expired = 0;
	tmev->interval = interval;
	tmev->arg = arg;
	tmev->tmcb = tmcb;
	fk_util_cal_expire(&(tmev->when), interval);

	tmev->prev = NULL;
	tmev->next = NULL;
	tmev->idx = -1;
	return tmev;
}

void fk_tmev_destroy(fk_tmev *tmev)
{
	//just free memory, no other things to do
	fk_mem_free(tmev);
}

int fk_ev_tmev_add(fk_tmev *tmev)
{
	fk_heap *tmhp;

	tmhp = evmgr.timer_heap;
	fk_heap_push(tmhp, (fk_leaf *)tmev);
	return 0;
}

int fk_ev_tmev_remove(fk_tmev *tmev)
{
	//in the min heap
	if (tmev->expired == 0) {
		fk_heap_remove(evmgr.timer_heap, (fk_leaf *)tmev);
	}

	//maybe this tmev in expired list
	if (tmev->expired == 1) {
		fk_rawlist_remove_any(evmgr.exp_tmev, tmev);
	}

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
		cmp = fk_util_tmval_cmp(&now, &(tmev->when));
		if (cmp >= 0) {
			fk_heap_pop(evmgr.timer_heap);//pop root from the heap
			fk_rawlist_insert_head(evmgr.exp_tmev, tmev);//add to the exp list
			tmev->expired = 1;
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
	fk_tmev_cb tmcb;
	char type;

	//tmev = evmgr.exp_tmev->head;
	tmev = fk_rawlist_head(evmgr.exp_tmev);
	while (tmev != NULL) {
		arg = tmev->arg;
		tmcb = tmev->tmcb;
		type = tmev->type;
		interval = tmev->interval;

		cur = tmev;//save current position
		tmev = tmev->next;//go to the next position
		//step 1: remove the expired tmev from the expired list first!!!!
		fk_rawlist_remove_any(evmgr.exp_tmev, cur);//remove current from the expired list
		cur->expired = 0;
		//step 2: call the callback of the expired tmev
		rt = tmcb(interval, type, arg);
		//step 3: how to handle the return value of the callback???
		if (rt == 0) {
			if (type == FK_TMEV_CYCLE) {
				fk_util_cal_expire(&(cur->when), interval);//calculate the trigger point time again
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
	fk_ioev_cb iocb;
	char type;
	fk_ioev *ioev, *cur;

	//ioev = evmgr.act_ioev->head;
	ioev = fk_rawlist_head(evmgr.act_ioev);
	while (ioev != NULL) {
		fd = ioev->fd;
		arg = ioev->arg;
		iocb = ioev->iocb;
		type = ioev->type;

		cur = ioev;//save the current position
		ioev = ioev->next;//go to the next positon

		//step 1: remove the active ioev from the active list first!!!!
		fk_rawlist_remove_any(evmgr.act_ioev, cur);
		cur->active = 0;//mark unactive
		//step 2: call the callback of the active ioev
		rt = iocb(fd, type, arg);
		//step 3: how to process the return value of the callback????
		if (rt < 0) {
			fk_log_error("error occurs when callback. fd: %d, type: %d\n", fd, type);
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

int fk_ev_ioev_activate(int fd, char type)
{
	fk_ioev *rioev, *wioev;

	/*
	 * maybe rioev and wioev point to the same ioev object
	 * use ioev->active to avoid inerting the ioev object to the active list twice
	 * */
	if (type & FK_IOEV_READ) {
		rioev = evmgr.read_ev[fd];
		if (rioev != NULL) {//when EPOLLERR/EPOLLHUP occurs, maybe there is no rioev/wioev, so check non-null
			if (rioev->active == 0) {
				fk_rawlist_insert_head(evmgr.act_ioev, rioev);//add to the exp list
				rioev->active = 1;
			}
		}
	}
	if (type & FK_IOEV_WRITE) {
		wioev = evmgr.write_ev[fd];
		if (wioev != NULL) {
			if (wioev->active == 0) {
				fk_rawlist_insert_head(evmgr.act_ioev, wioev);//add to the exp list
				wioev->active = 1;
			}
		}
	}

	return 0;
}

int fk_ev_tmev_cmp(fk_leaf *tmev1, fk_leaf *tmev2)
{
	fk_tmev *t1, *t2;

	t1 = (fk_tmev *)tmev1;
	t2 = (fk_tmev *)tmev2;
	return fk_util_tmval_cmp(&(t1->when), &(t2->when));
}
