/* unix headers */
#include <poll.h>

/*
 * in practice, the flags of real interest of poll() are:
 * read:       POLLIN, POLLRDHUP, POLLPRI
 * write:      POLLOUT
 * additional: POLLHUP, POLLERR
 */

typedef struct {
    struct pollfd    *evlist;
    int               last;
    int              *fd2idx;
} fk_poll_t;

static void *fk_poll_create(int max_files);
static int fk_poll_add(void *ev_iompx, int fd, uint8_t type);
static int fk_poll_remove(void *ev_iompx, int fd, uint8_t type);
static int fk_poll_dispatch(void *ev_iompx, struct timeval *timeout);

fk_mpxop_t poll_op = {
    "poll",
    fk_poll_create,
    fk_poll_add,
    fk_poll_remove,
    fk_poll_dispatch
};

void *
fk_poll_create(int max_files)
{
    int         i;
    fk_poll_t  *iompx;

    iompx = (fk_poll_t *)fk_mem_alloc(sizeof(fk_poll_t));
    iompx->evlist = (struct pollfd *)fk_mem_alloc(sizeof(struct pollfd) * max_files);
    memset(iompx->evlist, 0, sizeof(struct pollfd) * max_files);
    iompx->last = 0;
    iompx->fd2idx = (int *)fk_mem_alloc(sizeof(int) * max_files);
    for (i = 0; i < max_files; i++) {
        iompx->evlist[i].fd = -1;
        iompx->fd2idx[i] = -1;
    }

    return iompx;
}

int
fk_poll_add(void *ev_iompx, int fd, uint8_t type)
{
    int             idx;
    short           nev, oev;
    fk_poll_t      *iompx;
    struct pollfd  *pfd;

    iompx = (fk_poll_t *)ev_iompx;

    idx = iompx->fd2idx[fd];
    if (idx == -1) { /* never add this fd */
        idx =  iompx->last;
    }
    pfd = iompx->evlist + idx;
    oev = pfd->events;

    nev = 0x0000;
    if (type & FK_IOEV_READ) {
        nev |= POLLIN; /* equivalent to POLLRDNORM */
    }
    if (type & FK_IOEV_WRITE) {
        nev |= POLLOUT; /* equivalent to POLLWRNORM */
    }

    if (oev & nev) {
        fk_log_error("try to add an existing ev\n");
        return FK_EV_ERR;
    }

    pfd->events = oev | nev;
    pfd->fd = fd;
    iompx->fd2idx[fd] = idx;

    if (idx == iompx->last) { /* a new fd */
        iompx->last++;
        fk_log_debug("after add: last %d\n", iompx->last);
    }

    return FK_EV_OK;
}

int
fk_poll_remove(void *ev_iompx, int fd, uint8_t type)
{
    int             idx;
    short           nev, oev;
    fk_poll_t      *iompx;
    struct pollfd  *pfd, *tail;

    iompx = (fk_poll_t *)ev_iompx;

    idx = iompx->fd2idx[fd];
    pfd = iompx->evlist + idx;
    oev = pfd->events;

    nev = 0x000;
    if (type & FK_IOEV_READ) {
        nev |= POLLIN;
    }
    if (type & FK_IOEV_WRITE) {
        nev |= POLLOUT;
    }

    if (nev & (~oev)) {
        fk_log_error("try to remove a non-existing ev\n");
        return FK_EV_ERR;
    }

    pfd->events = oev & (~nev);

    if (pfd->events == 0x0000) { /* need to delete */
        tail = iompx->evlist + iompx->last - 1;
        memcpy(pfd, tail, sizeof(struct pollfd));
        tail->fd = -1;
        tail->events = 0x0000;
        tail->revents = 0x0000;
        iompx->last--;
    }

    return FK_EV_OK;
}

int
fk_poll_dispatch(void *ev_iompx, struct timeval *timeout)
{
    int             i, nfds, ms_timeout, fd, cnt;
    uint8_t         type;
    fk_poll_t      *iompx;
    struct pollfd  *pfd;

    ms_timeout = -1;
    if (timeout != NULL) {
        ms_timeout = fk_util_tv2millis(timeout);
    }

    iompx = (fk_poll_t *)ev_iompx;

    /*
     * Every time when poll() called, the array "evlist" need
     * to be coped from user to kernel space and back again.
     * This behavior greatly reduce the performance of poll()
     * when monitoring a large number of file descriptors
     */
    nfds = poll(iompx->evlist, iompx->last, ms_timeout);
    if (nfds < 0) {
        fk_log_debug("poll failed: %s\n", strerror(errno));
        return FK_EV_ERR;
    }
    if (nfds == 0) {
        return FK_EV_OK;
    }
    cnt = 0;
    for (i = 0; i < iompx->last; i++) {
        pfd = iompx->evlist + i;
        fd = pfd->fd;
        type = 0x00;
        /*
         * perform the similar check to epoll, ORing POLLHUB | POLLERR
         * POLLNVAL is not included, because POLLNVAL occurs only when
         * the corresponding fd is not open, but this could not happen
         * in this scenario, we ensure that every fd added to the poll
         * is valid. We can usually keep track of the closed fd.
         *
         * when POLLHUB or POLLERR occurs, we mark the corresponding fd
         * as readable and writable,  and then a subsequent call to
         * read()/write()/recv()/send() could tell us what exactly happened
         * to the fd by checking the return value of the corresponding call,
         * as well as the global "errno"
         *
         */
        if (pfd->revents & (POLLIN | POLLHUP | POLLERR/* | POLLNVAL */)) {
            type |= FK_IOEV_READ;
        }
        if (pfd->revents & (POLLOUT | POLLHUP | POLLERR/* | POLLNVAL */)) {
            type |= FK_IOEV_WRITE;
        }
        if (type == 0x00) {
            continue;
        }
        fk_ev_activate_ioev(fd, type);
        pfd->revents = 0x0000; /* reset revents */
        cnt++;
        if (cnt == nfds) {
            break;
        }
    }
    return FK_EV_OK;
}
