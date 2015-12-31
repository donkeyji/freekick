#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <strings.h>
#include <string.h>

#include <sys/time.h>

#include <fk_def.h>
#include <fk_ev.h>
#include <fk_list.h>
#include <fk_mem.h>
#include <fk_util.h>
#include <fk_macro.h>
#include <fk_log.h>

/* finite state machine for ioev */
/* 3 kinds of states for fk_ioev */
#define FK_IOEV_INIT 			0/* never added to the evmgr */
#define FK_IOEV_UNACTIVATED 	1/* not in the activated list */
#define FK_IOEV_ACTIVATED 		2/* in the activated list */

/* finite state machine for tmev */
/* 4 kinds of states for fk_tmev */
#define FK_TMEV_INIT		0/* never added to the evmgr */
#define FK_TMEV_UNEXPIRED	1/* in the min heap, not in the expired list */
#define FK_TMEV_EXPIRED		2/* in the expired list, not in the min heap */
#define FK_TMEV_PENDING		3/* added to the evmgr, but not in the min heap, neither the expired list*/

static void fk_ev_activate_ioev(int fd, char type);
static fk_tmev *fk_ev_get_nearest_tmev();
static void fk_ev_update_pending_tmev();
static void fk_ev_proc_activated_ioev();
static void fk_ev_proc_expired_tmev();
static int fk_ev_tmev_cmp(fk_leaf *tmev1, fk_leaf *tmev2);

#if defined(FK_HAVE_EPOLL)
	#include <fk_ev_epoll.c>
	static fk_mpxop *mpxop = &epoll_op;
#elif defined(FK_HAVE_KQUEUE)
	#include <fk_ev_kqueue.c>
	static fk_mpxop *mpxop = &kqueue_op;
#else
	#include <fk_ev_poll.c>
	static fk_mpxop *mpxop = &poll_op;
#endif

static fk_evmgr evmgr;

static fk_leaf_op tmev_op = {
	fk_ev_tmev_cmp
};

void fk_ev_init(unsigned max_files)
{
	evmgr.stop = 0;/* never stop */
	evmgr.max_files = max_files; 
	evmgr.ioev_cnt = 0;
	evmgr.tmev_cnt = 0;

	evmgr.timer_heap = fk_heap_create(&tmev_op);

	/* use macro to initialize this two member */
	evmgr.read_ev = (fk_ioev **)fk_mem_alloc(sizeof(fk_ioev *) * (evmgr.max_files));
	evmgr.write_ev = (fk_ioev **)fk_mem_alloc(sizeof(fk_ioev *) * (evmgr.max_files));

	evmgr.exp_tmev = fk_rawlist_create(fk_tmev_list);
	fk_rawlist_init(evmgr.exp_tmev);
	evmgr.act_ioev = fk_rawlist_create(fk_ioev_list);
	fk_rawlist_init(evmgr.act_ioev);

	evmgr.iompx = mpxop->iompx_create(evmgr.max_files);
	if (evmgr.iompx == NULL) {
		fk_log_error("evmgr init failed\n");
		exit(EXIT_FAILURE);
	}
}

int fk_ev_dispatch()
{
	int rt;
	fk_tmev *tmev;
	struct timeval *pto, now, timeout;

	pto = NULL;/* indefinitely wait */

	tmev = fk_ev_get_nearest_tmev(evmgr);
	if (tmev != NULL) {
		gettimeofday(&now, NULL);
		fk_util_tmval_sub(&(tmev->when), &now, &timeout);
		if ((fk_util_tv2millis(&timeout)) < 0) {
			timeout.tv_sec = 0;
			timeout.tv_usec = 0;
		}
		pto = &timeout;
	}

	/* 
	 * pto:
	 * (1)NULL: wait indefinitely
	 * (2){0,0}: return immediately
	 * (3)>{0,0}: wait for a period
	 * cannot be < {0, 0} 
	 */
	rt = mpxop->iompx_dispatch((&evmgr)->iompx, pto);
	if (rt == FK_EV_ERR) {
		return FK_EV_ERR;
	}

	/* 
	 * remove the nearest timer from timer_heap, 
	 * insert it to the exp_tmev 
	 */
	fk_ev_update_pending_tmev();

	fk_ev_proc_activated_ioev();

	fk_ev_proc_expired_tmev();

	return FK_EV_OK;
}

void fk_ev_cycle(int *stop)
{
	while (evmgr.stop != 1) {
		fk_ev_dispatch();/* we donot care the retrun value of fk_ev_dispatch() */
	}
}

void fk_ev_stop()
{
	evmgr.stop = 1;
}

int fk_ev_add_ioev(fk_ioev *ioev)
{
	char type;
	int fd, rt;

	/* unnecessary to do so? */
	if (ioev->activated != FK_IOEV_INIT) {
		return FK_EV_ERR;
	}

	fd = ioev->fd;
	type = ioev->type;

	rt = mpxop->iompx_add((&evmgr)->iompx, fd, type);
	if (rt == FK_EV_ERR) {
		return FK_EV_ERR;
	}

	if (type & FK_IOEV_READ) {
		evmgr.read_ev[fd] = ioev;
	}
	if (type & FK_IOEV_WRITE) {
		evmgr.write_ev[fd] = ioev;
	}
	ioev->activated = FK_IOEV_UNACTIVATED;/* necessary? */
	evmgr.ioev_cnt++;

	return FK_EV_OK;
}

int fk_ev_remove_ioev(fk_ioev *ioev)
{
	char type;
	int fd, rt;

	/* unnecessary ? */
	if (ioev->activated == FK_IOEV_INIT) {
		return FK_EV_ERR;
	}

	/* must be in the read/write event array */
	fd = ioev->fd;
	type = ioev->type;
	rt = mpxop->iompx_remove((&evmgr)->iompx, fd, type);
	if (rt == FK_EV_ERR) {
		return FK_EV_ERR;
	}

	/* maybe this ioev in activated list */
	if (ioev->activated == FK_IOEV_ACTIVATED) {
		fk_rawlist_remove_anyone(evmgr.act_ioev, ioev);
	}
	ioev->activated = FK_IOEV_INIT;/* go back to init state */

	if (type & FK_IOEV_READ) {
		evmgr.read_ev[fd] = NULL;
	}
	if (type & FK_IOEV_WRITE) {
		evmgr.write_ev[fd] = NULL;
	}
	evmgr.ioev_cnt--;

	return FK_EV_OK;
}

char *fk_ev_iompx_name()
{
	return mpxop->iompx_name;
}

fk_ioev *fk_ioev_create(int fd, char type, void *arg, fk_ioev_cb iocb)
{
	fk_ioev *ioev;

	ioev = (fk_ioev *)fk_mem_alloc(sizeof(fk_ioev));
	ioev->fd = fd;
	ioev->type = type;
	ioev->arg = arg;
	ioev->iocb = iocb;
	ioev->activated = FK_IOEV_INIT;

	ioev->prev = NULL;
	ioev->next = NULL;
	return ioev;
}

void fk_ioev_destroy(fk_ioev *ioev)
{
	fk_mem_free(ioev);
}

/* 
 * no need to delete timer
 * so fk_ev_timer_del not supplied 
 */
fk_tmev *fk_tmev_create(unsigned interval, char type, void *arg, fk_tmev_cb tmcb)
{
	fk_tmev *tmev;

	tmev = (fk_tmev *)fk_mem_alloc(sizeof(fk_tmev));
	tmev->type = type;
	tmev->expired = FK_TMEV_INIT;
	tmev->interval = interval;
	tmev->arg = arg;
	tmev->tmcb = tmcb;
	fk_util_cal_expire(&(tmev->when), interval);

	tmev->prev = NULL;
	tmev->next = NULL;
	tmev->idx = 0;/* index 0 was not used in min_heap */
	return tmev;
}

void fk_tmev_destroy(fk_tmev *tmev)
{
	/* just free memory, no other things to do */
	fk_mem_free(tmev);
}

int fk_ev_add_tmev(fk_tmev *tmev)
{
	fk_heap *tmhp;

	/* only when the tmev->expired == FK_TMEV_INIT is legal */
	if (tmev->expired != FK_TMEV_INIT) {
		return FK_EV_ERR;
	}
	tmhp = evmgr.timer_heap;
	fk_heap_push(tmhp, (fk_leaf *)tmev);
	tmev->expired = FK_TMEV_UNEXPIRED;/* in the min_heap, not in the expired list */
	evmgr.tmev_cnt++;

	return FK_EV_OK;
}

/*
 * when tmev->expired == FK_TMEV_PENDING, this interface also works
 */
int fk_ev_remove_tmev(fk_tmev *tmev)
{
	/* this means the tmev has never been added */
	if (tmev->expired == FK_TMEV_INIT) {
		return FK_EV_ERR;
	}

	/* in the min heap */
	if (tmev->expired == FK_TMEV_UNEXPIRED) {
		fk_heap_remove(evmgr.timer_heap, (fk_leaf *)tmev);
	}

	/* maybe this tmev in expired list */
	if (tmev->expired == FK_TMEV_EXPIRED) {
		fk_rawlist_remove_anyone(evmgr.exp_tmev, tmev);
	}
	tmev->expired = FK_TMEV_INIT;
	evmgr.tmev_cnt--;

	return FK_EV_OK;
}

void fk_ev_update_pending_tmev()
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
			fk_heap_pop(evmgr.timer_heap);/* pop root from the heap */
			fk_rawlist_insert_head(evmgr.exp_tmev, tmev);/* add to the exp list */
			tmev->expired = FK_TMEV_EXPIRED;
			root = fk_heap_root(evmgr.timer_heap);/* get new root */
		} else {/* break directly */
			break;
		}
	}
}

void fk_ev_proc_expired_tmev()
{
	int rt;
	char type;
	void *arg;
	fk_tmev *tmev;
	fk_tmev_cb tmcb;
	unsigned interval;

	tmev = fk_rawlist_head(evmgr.exp_tmev);
	while (tmev != NULL) {
		arg = tmev->arg;
		tmcb = tmev->tmcb;
		type = tmev->type;
		interval = tmev->interval;

		/* step 1: remove the expired tmev from the expired list first!!!! */
		fk_rawlist_remove_anyone(evmgr.exp_tmev, tmev);
		tmev->expired = FK_TMEV_PENDING;/* not init, not in the min heap, neither the expired list */
		/* step 2: call the callback of the expired tmev */
		rt = tmcb(interval, type, arg);/* maybe fk_ev_remove_tmev() is called in tmcb*/
		/* 
		 * step 3:
		 * fk_ev_remove_tmev() should not be called here, even if the return value
		 * of tmcb < 0
		 * if we do not want to use this timer anymore, we should manually call
		 * fk_ev_remove_tmev() in outside modules
		 */
		if (rt == 0) {
			if (type == FK_TMEV_CYCLE) {
				fk_util_cal_expire(&(tmev->when), interval);
				fk_heap_push(evmgr.timer_heap, (fk_leaf *)tmev);
				tmev->expired = FK_TMEV_UNEXPIRED;/* add to min heap again */
			}
		}
		tmev = fk_rawlist_head(evmgr.exp_tmev);
	}
}

void fk_ev_proc_activated_ioev()
{
	int fd;
	void *arg;
	char type;
	fk_ioev *ioev;
	fk_ioev_cb iocb;

	ioev = fk_rawlist_head(evmgr.act_ioev);
	while (ioev != NULL) {
		fd = ioev->fd;
		arg = ioev->arg;
		iocb = ioev->iocb;
		type = ioev->type;

		/* step 1: remove the activated ioev from the activated list first!!!! */
		fk_rawlist_remove_anyone(evmgr.act_ioev, ioev);
		ioev->activated = FK_IOEV_UNACTIVATED;/* not in the activated list now */
		/* step 2: call the callback of the activated ioev */
		iocb(fd, type, arg);/* maybe fk_ev_remove_ioev() is called in iocb */
		/* 
		 * we donot care about the return value of iocb, all error should be handled
		 * by iocb itself, but not here.
		 * we do nothing here, we donot call fk_ev_remove_ioev() here
		 * fk_ev_remove_ioev() should be called in outside modules
		 */
		ioev = fk_rawlist_head(evmgr.act_ioev);
	}
}

fk_tmev *fk_ev_get_nearest_tmev()
{
	void *root;
	fk_tmev *tmev;

	root = fk_heap_root(evmgr.timer_heap);
	tmev = (fk_tmev *)root;
	return tmev;
}

void fk_ev_activate_ioev(int fd, char type)
{
	fk_ioev *rioev, *wioev;

	/* 
	 * maybe rioev and wioev point to the same ioev object
	 * use ioev->activated to avoid inerting the ioev object 
	 * to the activated list twice 
	 */
	if (type & FK_IOEV_READ) {
		rioev = evmgr.read_ev[fd];
		if (rioev != NULL) {/* when EPOLLERR/EPOLLHUP occurs, maybe there is no rioev/wioev, so check non-null */
			if (rioev->activated == FK_IOEV_UNACTIVATED) {
				fk_rawlist_insert_head(evmgr.act_ioev, rioev);/* add to the exp list */
				rioev->activated = FK_IOEV_ACTIVATED;
			}
		}
	}
	if (type & FK_IOEV_WRITE) {
		wioev = evmgr.write_ev[fd];
		if (wioev != NULL) {
			if (wioev->activated == FK_IOEV_UNACTIVATED) {
				fk_rawlist_insert_head(evmgr.act_ioev, wioev);/* add to the exp list */
				wioev->activated = FK_IOEV_ACTIVATED;
			}
		}
	}
}

int fk_ev_tmev_cmp(fk_leaf *tmev1, fk_leaf *tmev2)
{
	fk_tmev *t1, *t2;

	t1 = (fk_tmev *)tmev1;
	t2 = (fk_tmev *)tmev2;
	return fk_util_tmval_cmp(&(t1->when), &(t2->when));
}

#ifdef FK_DEBUG
void fk_ev_stat()
{
	fprintf(stdout, "ioev_cnt: %llu, tmev_cnt: %llu\n", evmgr.ioev_cnt, evmgr.tmev_cnt);
}
#endif
