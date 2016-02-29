#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <strings.h>
#include <string.h>

#include <sys/time.h>

#include <fk_env.h>
#include <fk_ev.h>
#include <fk_list.h>
#include <fk_mem.h>
#include <fk_util.h>
#include <fk_log.h>

/* finite state is useful when add/remove ioev or tmev */
/* finite state machine for ioev */
/* 3 kinds of states for fk_ioev_t */
#define FK_IOEV_INIT 			0/* never added to the evmgr */
#define FK_IOEV_PENDING 		1/* not in the activated list */
#define FK_IOEV_ACTIVATED 		2/* in the activated list */

/* finite state machine for tmev */
/* 4 kinds of states for fk_tmev_t */
#define FK_TMEV_INIT		0/* never added to the evmgr */
#define FK_TMEV_PENDING		1/* in the min heap, not in the expired list */
#define FK_TMEV_EXPIRED		2/* in the expired list, not in the min heap */
#define FK_TMEV_OLD			3/* added to the evmgr, but not in the min heap, neither the expired list */

#define	fk_ioev_set_stat(ioev, stat)		(ioev)->activated = (stat)
#define fk_ioev_get_stat(ioev)				((ioev)->activated)
#define fk_tmev_set_stat(tmev, stat)		(tmev)->expired = (stat)
#define fk_tmev_get_stat(tmev)				((tmev)->expired)

static void fk_ev_activate_ioev(int fd, uint8_t type);
static fk_tmev_t *fk_ev_get_nearest_tmev(void);
static void fk_ev_update_pending_tmev(void);
static void fk_ev_proc_activated_ioev(void);
static void fk_ev_proc_expired_tmev(void);
static int fk_tmev_cmp(fk_leaf_t *tmev1, fk_leaf_t *tmev2);

#if defined(FK_HAVE_EPOLL)
#include <fk_ev_epoll.c>
static fk_mpxop_t *mpxop = &epoll_op;
#elif defined(FK_HAVE_KQUEUE)
#include <fk_ev_kqueue.c>
static fk_mpxop_t *mpxop = &kqueue_op;
#else
#include <fk_ev_poll.c>
static fk_mpxop_t *mpxop = &poll_op;
#endif

static fk_evmgr_t evmgr;

static fk_leaf_op_t tmev_op = {
    fk_tmev_cmp
};

void fk_ev_init(unsigned max_files)
{
    evmgr.stop = 0;/* never stop */
    evmgr.max_files = max_files;
    evmgr.ioev_cnt = 0;
    evmgr.tmev_cnt = 0;

    evmgr.timer_heap = fk_heap_create(&tmev_op);

    /* use macro to initialize this two member */
    evmgr.read_ev = (fk_ioev_t **)fk_mem_alloc(sizeof(fk_ioev_t *) * (evmgr.max_files));
    evmgr.write_ev = (fk_ioev_t **)fk_mem_alloc(sizeof(fk_ioev_t *) * (evmgr.max_files));

    evmgr.exp_tmev = fk_rawlist_create(fk_tmev_list_t);
    fk_rawlist_init(evmgr.exp_tmev);
    evmgr.act_ioev = fk_rawlist_create(fk_ioev_list_t);
    fk_rawlist_init(evmgr.act_ioev);
    evmgr.old_tmev = fk_rawlist_create(fk_tmev_list_t);
    fk_rawlist_init(evmgr.old_tmev);

    evmgr.iompx = mpxop->iompx_create(evmgr.max_files);
    if (evmgr.iompx == NULL) {
        fk_log_error("evmgr init failed\n");
        exit(EXIT_FAILURE);
    }
}

int fk_ev_dispatch(void)
{
    int rt;
    fk_tmev_t *tmev;
    struct timeval *pto, now, timeout;

    pto = NULL;/* indefinitely wait */

    tmev = fk_ev_get_nearest_tmev();
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

void fk_ev_cycle(void)
{
    while (evmgr.stop != 1) {
        fk_ev_dispatch();/* we donot care the retrun value of fk_ev_dispatch() */
    }
}

void fk_ev_stop(void)
{
    evmgr.stop = 1;
}

int fk_ev_add_ioev(fk_ioev_t *ioev)
{
    int fd, rt;
    uint8_t type;

    /* unnecessary to do so? */
    if (fk_ioev_get_stat(ioev) != FK_IOEV_INIT) {
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
    fk_ioev_set_stat(ioev, FK_IOEV_PENDING);/* necessary? */
    evmgr.ioev_cnt++;

    return FK_EV_OK;
}

int fk_ev_remove_ioev(fk_ioev_t *ioev)
{
    int fd, rt;
    uint8_t type;

    /* unnecessary ? */
    if (fk_ioev_get_stat(ioev) == FK_IOEV_INIT) {
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
    if (fk_ioev_get_stat(ioev) == FK_IOEV_ACTIVATED) {
        fk_rawlist_remove_anyone(evmgr.act_ioev, ioev);
    }
    fk_ioev_set_stat(ioev, FK_IOEV_INIT);/* go back to init state */

    if (type & FK_IOEV_READ) {
        evmgr.read_ev[fd] = NULL;
    }
    if (type & FK_IOEV_WRITE) {
        evmgr.write_ev[fd] = NULL;
    }
    evmgr.ioev_cnt--;

    return FK_EV_OK;
}

char *fk_ev_iompx_name(void)
{
    return mpxop->iompx_name;
}

fk_ioev_t *fk_ioev_create(int fd, uint8_t type, void *arg, fk_ioev_cb iocb)
{
    fk_ioev_t *ioev;

    ioev = (fk_ioev_t *)fk_mem_alloc(sizeof(fk_ioev_t));
    ioev->fd = fd;
    ioev->type = type;
    ioev->arg = arg;
    ioev->iocb = iocb;
    fk_ioev_set_stat(ioev, FK_IOEV_INIT);

    ioev->prev = NULL;
    ioev->next = NULL;
    return ioev;
}

void fk_ioev_destroy(fk_ioev_t *ioev)
{
    fk_mem_free(ioev);
}

fk_tmev_t *fk_tmev_create(uint32_t interval, uint8_t type, void *arg, fk_tmev_cb tmcb)
{
    fk_tmev_t *tmev;

    tmev = (fk_tmev_t *)fk_mem_alloc(sizeof(fk_tmev_t));
    tmev->type = type;
    fk_tmev_set_stat(tmev, FK_TMEV_INIT);
    tmev->interval = interval;
    tmev->arg = arg;
    tmev->tmcb = tmcb;
    fk_util_cal_expire(&(tmev->when), interval);

    tmev->prev = NULL;
    tmev->next = NULL;
    tmev->idx = 0;/* index 0 was not used in min_heap */
    return tmev;
}

void fk_tmev_destroy(fk_tmev_t *tmev)
{
    /* just free memory, no other things to do */
    fk_mem_free(tmev);
}

int fk_ev_add_tmev(fk_tmev_t *tmev)
{
    fk_heap_t *tmhp;

    /* only when the tmev->expired == FK_TMEV_INIT is legal */
    if (fk_tmev_get_stat(tmev) != FK_TMEV_INIT) {
        return FK_EV_ERR;
    }
    tmhp = evmgr.timer_heap;
    fk_heap_push(tmhp, (fk_leaf_t *)tmev);
    fk_tmev_set_stat(tmev, FK_TMEV_PENDING);/* in the min_heap, not in the expired list */
    evmgr.tmev_cnt++;

    return FK_EV_OK;
}

/*
 * when tmev->expired == FK_TMEV_OLD, this interface also works
 */
int fk_ev_remove_tmev(fk_tmev_t *tmev)
{
    /* this means the tmev has never been added */
    if (fk_tmev_get_stat(tmev) == FK_TMEV_INIT) {
        return FK_EV_ERR;
    }

    /* in the min heap */
    if (fk_tmev_get_stat(tmev) == FK_TMEV_PENDING) {
        fk_heap_remove(evmgr.timer_heap, (fk_leaf_t *)tmev);
    }

    /* maybe this tmev in expired list */
    if (fk_tmev_get_stat(tmev) == FK_TMEV_EXPIRED) {
        fk_rawlist_remove_anyone(evmgr.exp_tmev, tmev);
    }

    if (fk_tmev_get_stat(tmev) == FK_TMEV_OLD) {
        fk_rawlist_remove_anyone(evmgr.old_tmev, tmev);
    }
    fk_tmev_set_stat(tmev, FK_TMEV_INIT);
    evmgr.tmev_cnt--;

    return FK_EV_OK;
}

void fk_ev_update_pending_tmev(void)
{
    int cmp;
    fk_leaf_t *root;
    fk_tmev_t *tmev;
    struct timeval now;

    gettimeofday(&now, NULL);

    root = fk_heap_root(evmgr.timer_heap);
    while (root != NULL) {
        tmev = (fk_tmev_t *)root;
        cmp = fk_util_tmval_cmp(&now, &(tmev->when));
        if (cmp >= 0) {
            fk_heap_pop(evmgr.timer_heap);/* pop root from the heap */
            fk_rawlist_insert_head(evmgr.exp_tmev, tmev);/* add to the exp list */
            fk_tmev_set_stat(tmev, FK_TMEV_EXPIRED);
            root = fk_heap_root(evmgr.timer_heap);/* get new root */
        } else {/* break directly */
            break;
        }
    }
}

void fk_ev_proc_expired_tmev(void)
{
    int rt;
    void *arg;
    uint8_t type;
    fk_tmev_t *tmev;
    fk_tmev_cb tmcb;
    uint32_t interval;

    tmev = fk_rawlist_head(evmgr.exp_tmev);
    while (tmev != NULL) {
        arg = tmev->arg;
        tmcb = tmev->tmcb;
        type = tmev->type;
        interval = tmev->interval;

        /* step 1: remove the expired tmev from the expired list first!!!! */
        fk_rawlist_remove_anyone(evmgr.exp_tmev, tmev);
        fk_rawlist_insert_head(evmgr.old_tmev, tmev);/* move to the old list */
        fk_tmev_set_stat(tmev, FK_TMEV_OLD);/* not init, not in the min heap, neither the expired list */
        /* step 2: call the callback of the expired tmev */
        rt = tmcb(interval, type, arg);/* maybe fk_ev_remove_tmev() is called in tmcb */
        /*
         * step 3:
         * fk_ev_remove_tmev() should not be called here, even if the return value
         * of tmcb < 0
         * if we do not want to use this timer anymore, we should manually call
         * fk_ev_remove_tmev() in outside modules
         */
        if (rt == 0) {
            if (type == FK_TMEV_CYCLE) {
                fk_rawlist_remove_anyone(evmgr.old_tmev, tmev);
                fk_util_cal_expire(&(tmev->when), interval);
                fk_heap_push(evmgr.timer_heap, (fk_leaf_t *)tmev);
                fk_tmev_set_stat(tmev, FK_TMEV_PENDING);/* add to min heap again */
            }
        }
        tmev = fk_rawlist_head(evmgr.exp_tmev);
    }
}

void fk_ev_proc_activated_ioev(void)
{
    int fd;
    void *arg;
    uint8_t type;
    fk_ioev_t *ioev;
    fk_ioev_cb iocb;

    ioev = fk_rawlist_head(evmgr.act_ioev);
    while (ioev != NULL) {
        fd = ioev->fd;
        arg = ioev->arg;
        iocb = ioev->iocb;
        type = ioev->type;

        /* step 1: remove the activated ioev from the activated list first!!!! */
        fk_rawlist_remove_anyone(evmgr.act_ioev, ioev);
        fk_ioev_set_stat(ioev, FK_IOEV_PENDING);/* not in the activated list now */
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

fk_tmev_t *fk_ev_get_nearest_tmev(void)
{
    void *root;
    fk_tmev_t *tmev;

    root = fk_heap_root(evmgr.timer_heap);
    tmev = (fk_tmev_t *)root;
    return tmev;
}

void fk_ev_activate_ioev(int fd, uint8_t type)
{
    fk_ioev_t *rioev, *wioev;

    /*
     * maybe rioev and wioev point to the same ioev object
     * use ioev->activated to avoid inerting the ioev object
     * to the activated list twice
     */
    if (type & FK_IOEV_READ) {
        rioev = evmgr.read_ev[fd];
        if (rioev != NULL) {/* when EPOLLERR/EPOLLHUP occurs, maybe there is no rioev/wioev, so check non-null */
            if (fk_ioev_get_stat(rioev) == FK_IOEV_PENDING) {
                fk_rawlist_insert_head(evmgr.act_ioev, rioev);/* add to the exp list */
                fk_ioev_set_stat(rioev, FK_IOEV_ACTIVATED);
            }
        }
    }
    if (type & FK_IOEV_WRITE) {
        wioev = evmgr.write_ev[fd];
        if (wioev != NULL) {
            if (fk_ioev_get_stat(wioev) == FK_IOEV_PENDING) {
                fk_rawlist_insert_head(evmgr.act_ioev, wioev);/* add to the exp list */
                fk_ioev_set_stat(wioev, FK_IOEV_ACTIVATED);
            }
        }
    }
}

int fk_tmev_cmp(fk_leaf_t *tmev1, fk_leaf_t *tmev2)
{
    fk_tmev_t *t1, *t2;

    t1 = (fk_tmev_t *)tmev1;
    t2 = (fk_tmev_t *)tmev2;
    return fk_util_tmval_cmp(&(t1->when), &(t2->when));
}

#ifdef FK_DEBUG
void fk_ev_stat(void)
{
    fprintf(stdout, "[event statistics]ioev_cnt: %zu, tmev_cnt: %zu\n", evmgr.ioev_cnt, evmgr.tmev_cnt);
}
#endif
