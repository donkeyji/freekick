#include <sys/event.h>

/*
 * no need to keep tracking the existing ev associated to fd,
 * which should be done in epoll. If an existing/non-existing
 * ev is added/removed, just return FK_EV_ERR to the caller
 */

typedef struct {
    int kfd;
    int max_evs;
    struct kevent kev;
    struct kevent *evlist;
    //uint8_t *emask;/* no need for kqueue */
} fk_kqueue_t;

static void *fk_kqueue_create(int max_files);
static int fk_kqueue_add(void *ev_iompx, int fd, uint8_t type);
static int fk_kqueue_remove(void *ev_iompx, int fd, uint8_t type);
static int fk_kqueue_dispatch(void *ev_iompx, struct timeval *timeout);

fk_mpxop_t kqueue_op = {
    "kqueue",
    fk_kqueue_create,
    fk_kqueue_add,
    fk_kqueue_remove,
    fk_kqueue_dispatch
};

void *fk_kqueue_create(int max_files)
{
    int kfd;
    fk_kqueue_t *iompx;

    kfd = kqueue();
    if (kfd < 0) {
        return NULL;
    }

    iompx = (fk_kqueue_t *)fk_mem_alloc(sizeof(fk_kqueue_t));
    iompx->max_evs = max_files << 1;/* read config from global setting */
    iompx->kfd = kfd;
    iompx->evlist = (struct kevent *)fk_mem_alloc(sizeof(struct kevent) * iompx->max_evs);
    //iompx->emask = (uint8_t *)fk_mem_alloc(sizeof(uint8_t *) * max_files);
    //memset(iompx->emask, 0, sizeof(uint8_t *) * max_files);

    return iompx;
}

int fk_kqueue_add(void *ev_iompx, int fd, uint8_t type)
{
    int rt;
    fk_kqueue_t *iompx;
    //uint8_t otyp;

    iompx = (fk_kqueue_t *)(ev_iompx);

    //otyp = iompx->emask[fd];
    //if (otyp & type) {
    //return FK_EV_ERR;
    //}

    if (type & FK_IOEV_READ) {
        EV_SET(&(iompx->kev), fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL );
        rt = kevent(iompx->kfd, &(iompx->kev), 1, NULL, 0, NULL);
        if (rt < 0) {
            fk_log_error("kevent add read failed: %s\n", strerror(errno));
            return FK_EV_ERR;
        }
        //iompx->emask[fd] |= FK_IOEV_READ;
    }

    if (type & FK_IOEV_WRITE) {/* both read & write */
        EV_SET(&(iompx->kev), fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL );
        rt = kevent(iompx->kfd, &(iompx->kev), 1, NULL, 0, NULL);
        if (rt < 0) {
            fk_log_error("kevent add write failed: %s\n", strerror(errno));
            return FK_EV_ERR;
        }
        //iompx->emask[fd] |= FK_IOEV_WRITE;
    }

    return FK_EV_OK;
}

int fk_kqueue_remove(void *ev_iompx, int fd, uint8_t type)
{
    int rt;
    fk_kqueue_t *iompx;
    //uint8_t otyp;

    iompx = (fk_kqueue_t *)ev_iompx;

    //otyp = iompx->emask[fd];

    //if (type & (~otyp)) {
    //return FK_EV_ERR;
    //}

    if (type & FK_IOEV_READ) {
        EV_SET(&(iompx->kev), fd, EVFILT_READ, EV_DELETE, 0, 0, NULL );
        rt = kevent(iompx->kfd, &(iompx->kev), 1, NULL, 0, NULL);
        if (rt < 0) {
            fk_log_error("kevent delete read failed: %s\n", strerror(errno));
            return FK_EV_ERR;
        }
        //iompx->emask[fd] &= (~FK_IOEV_READ);
    }
    if (type & FK_IOEV_WRITE) {/* both read & write */
        EV_SET(&(iompx->kev), fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL );
        rt = kevent(iompx->kfd, &(iompx->kev), 1, NULL, 0, NULL);
        if (rt < 0) {
            fk_log_error("kevent delete write failed: %s\n", strerror(errno));
            return FK_EV_ERR;
        }
        //iompx->emask[fd] &= (~FK_IOEV_WRITE);
    }

    return FK_EV_OK;
}

int fk_kqueue_dispatch(void *ev_iompx, struct timeval *timeout)
{
    int i, nfds;
    uint8_t type;
    uintptr_t fd;
    //intptr_t data;
    //uint16_t flags;
    fk_kqueue_t *iompx;
    struct timespec *pt;
    struct timespec kev_timeout;

    iompx = (fk_kqueue_t *)ev_iompx;

    pt = NULL;
    if (timeout != NULL) {
        fk_util_tv2ts(timeout, &kev_timeout);
        pt = &kev_timeout;
    }

    //fk_log_debug("kevent to wait\n");
    nfds = kevent(iompx->kfd, NULL, 0, iompx->evlist, iompx->max_evs, pt);
    if (nfds < 0) {
        if (errno != EINTR) {
            return FK_EV_ERR;
        }
        return FK_EV_OK;
    }

    //fk_log_debug("kevent return\n");
    for (i = 0; i < nfds; i++) {
        fd = iompx->evlist[i].ident;
        /* unnecessary to check error here, because I call kevent to add fd before,
         * and the timeout && struct kevent is valid, unlike libevent. libevent do
         * not call kevent to add fd before, but only call kevent one time when
         * polling */

        /*
        flags = iompx->evlist[i].flags;
        data = iompx->evlist[i].data;
        if (flags == EV_ERROR) {
        	if (data == EBADF || data == EINVAL || data == ENOENT) {
        		continue;
        	}
        	return FK_EV_ERR;
        }
        */

        if (iompx->evlist[i].filter == EVFILT_READ) {
            //fk_log_debug("EVFILT_READ\n");
            type = FK_IOEV_READ;
        }
        if (iompx->evlist[i].filter == EVFILT_WRITE) {
            //fk_log_debug("EVFILT_WRITE\n");
            type = FK_IOEV_WRITE;
        }
        //fk_log_debug("i: %d	fd: %lu	type: %d\n", i, iompx->evlist[i].ident, iompx->evlist[i].filter);
        fk_ev_activate_ioev(fd, type);
    }

    return FK_EV_OK;
}
