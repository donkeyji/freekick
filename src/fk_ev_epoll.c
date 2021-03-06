/* c standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* unix headers */
#include <sys/epoll.h>

/* local headers */
#include <fk_ev.h>
#include <fk_str.h>
#include <fk_mem.h>
#include <fk_log.h>
#include <fk_util.h>

/*
 * here I use emask to track the existing ev associated to fd because of the
 * limitation of the interface of epoll, and I think it's better than using 2
 * arrays of fk_tmev_t pointer which are used in libevent^_^
 */

/*
 * Note that epoll could be used on socket/fifo/pipe..., but not regular files
 * and directories.
 * epoll() performs much better than select()/poll() when monitoring large
 * numbers of file descriptors for the reason that select()/poll() passes a
 * complete list of file descriptors to be monitored to the kernel, and then
 * the kernel return a list of file descriptors which are ready for
 * reading/writing, so the time required by select()/poll() grows with the
 * increase of number of file descriptors, rather than the number of I/O events.
 * By contrast, with epoll(), the time required just increase with the number of
 * I/O events.
 */

typedef struct {
    int                    efd;
    int                    max_evs;
    struct epoll_event     ev;        /* temporary variable */
    struct epoll_event    *evlist;
    uint8_t               *emask;     /* to track events associated to a fd */
} fk_epoll_t;

static void *fk_epoll_create(int max_files);
static int fk_epoll_add(void *ev_iompx, int fd, uint8_t type);
static int fk_epoll_remove(void *ev_iompx, int fd, uint8_t type);
static int fk_epoll_dispatch(void *ev_iompx, struct timeval *timeout);

/*
 * no need to expose this interface in fk_ev.h
 * just declare mpx_optr as "extern" here
 */
void fk_ev_activate_ioev(int fd, uint8_t type);

fk_mpxop_t mpx_optr = {
    "epoll",
    fk_epoll_create,
    fk_epoll_add,
    fk_epoll_remove,
    fk_epoll_dispatch
};

void *
fk_epoll_create(int max_files)
{
    int          efd;
    fk_epoll_t  *iompx;

    efd = epoll_create(1);
    if (efd < 0) {
        return NULL;
    }

    iompx = (fk_epoll_t *)fk_mem_alloc(sizeof(fk_epoll_t));

    iompx->max_evs = max_files;
    iompx->efd = efd;

    iompx->evlist = (struct epoll_event *)fk_mem_alloc(sizeof(struct epoll_event) * iompx->max_evs);
    iompx->emask = (uint8_t *)fk_mem_alloc(sizeof(uint8_t *) * iompx->max_evs);
    memset(iompx->emask, 0, sizeof(uint8_t *) * iompx->max_evs);

    return iompx;
}

int
fk_epoll_add(void *ev_iompx, int fd, uint8_t type)
{
    int          rt, op;
    uint8_t      otp;
    int32_t      oev, nev;
    fk_epoll_t  *iompx;

    iompx = (fk_epoll_t *)ev_iompx;

    otp = iompx->emask[fd];
    if (type & otp) {
        fk_log_error("try to add existing ev\n");
        return FK_EV_ERR;
    }

    oev = 0x0000;
    if (otp & FK_IOEV_READ) {
        oev |= EPOLLIN;
    }
    if (otp & FK_IOEV_WRITE) {
        oev |= EPOLLOUT;
    }

    nev = 0x0000;
    if (type & FK_IOEV_READ) {
        nev |= EPOLLIN;
    }
    if (type & FK_IOEV_WRITE) {
        nev |= EPOLLOUT;
    }

    if (oev == 0x0000) {
        op = EPOLL_CTL_ADD;
    } else {
        op = EPOLL_CTL_MOD;
    }
    iompx->ev.events = oev | nev;
    /*
     * the epoll_event.data.fd or epoll_event.data.ptr is the only mechanism for
     * finding out the number of the file descriptors when epoll_wait() returns
     * with success.
     * We can use epoll_event.data.ptr to store a pointer referring to an object
     * which stores more information associated with this fd
     */
    iompx->ev.data.fd = fd;

    rt = epoll_ctl(iompx->efd, op, fd, &(iompx->ev));
    if (rt < 0) {
        fk_log_error("epoll_ctl failed: %s\n", strerror(errno));
        return FK_EV_ERR;
    }

    /* if succeed, save emask */
    iompx->emask[fd] = otp | type;
    return FK_EV_OK;
}

int
fk_epoll_remove(void *ev_iompx, int fd, uint8_t type)
{
    int          rt, op;
    uint8_t      otp;
    int32_t      oev, nev;
    fk_epoll_t  *iompx;

    iompx = (fk_epoll_t *)ev_iompx;

    otp = iompx->emask[fd];
    if (!(type & otp)) {
        fk_log_error("try to remove a non-existing ev\n");
        return FK_EV_ERR;
    }

    oev = 0x0000;
    if (otp & FK_IOEV_READ) {
        oev |= EPOLLIN;
    }
    if (otp & FK_IOEV_WRITE) {
        oev |= EPOLLOUT;
    }

    nev = 0x0000;
    if (type & FK_IOEV_READ) {
        nev |= EPOLLIN;
    }
    if (type & FK_IOEV_WRITE) {
        nev |= EPOLLOUT;
    }

    if (oev == nev) {
        op = EPOLL_CTL_DEL;
    } else {
        op = EPOLL_CTL_MOD;
    }
    iompx->ev.events = oev & (~nev);
    iompx->ev.data.fd = fd;

    rt = epoll_ctl(iompx->efd, op, fd, &(iompx->ev));
    if (rt < 0) {
        fk_log_error("epoll_ctl failed: %s\n", strerror(errno));
        return FK_EV_ERR;
    }

    /* if succeed, remove from the emask */
    iompx->emask[fd] = otp & (~type); /* my clever!!!!!! */
    return FK_EV_OK;
}

int
fk_epoll_dispatch(void *ev_iompx, struct timeval *timeout)
{
    int          i, nfds, fd, ms_timeout;
    uint8_t      type;
    fk_epoll_t  *iompx;

    iompx = (fk_epoll_t *)ev_iompx;

    ms_timeout = -1;
    if (timeout != NULL) {
        ms_timeout = fk_util_tv2millis(timeout);
    }

    //fk_log_debug("epoll to wait\n");
    nfds = epoll_wait(iompx->efd, iompx->evlist, iompx->max_evs, ms_timeout);
    //fk_log_debug("epoll return\n");
    if (nfds < 0) {
        if (errno != EINTR) {
            return FK_EV_ERR;
        }
        return FK_EV_OK;
    }

    for (i = 0; i < nfds; i++) {
        fd = iompx->evlist[i].data.fd;
        type = 0x00;
        if (iompx->evlist[i].events & (EPOLLIN | EPOLLHUP | EPOLLERR)) {
            type |= FK_IOEV_READ;
        }
        if (iompx->evlist[i].events & (EPOLLOUT | EPOLLHUP | EPOLLERR)) {
            type |= FK_IOEV_WRITE;
        }
        fk_ev_activate_ioev(fd, type);
    }

    return FK_EV_OK;
}
