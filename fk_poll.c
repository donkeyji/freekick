#include <poll.h>

typedef struct _fk_poll {
	struct pollfd *evlist;
	int last;
	int *pidx;
} fk_poll;

static void *fk_poll_create(int max_conn);
static int fk_poll_add(void *ev_iompx, int fd, unsigned char type);
static int fk_poll_remove(void *ev_iompx, int fd, unsigned char type);
static int fk_poll_dispatch(void *ev_iompx, struct timeval *timeout);

fk_mpxop poll_op = {
	fk_poll_create,
	fk_poll_add,
	fk_poll_remove,
	fk_poll_dispatch
};

void *fk_poll_create(int max_conn)
{
	fk_poll *iompx;

	iompx = (fk_poll *)fk_mem_alloc(sizeof(fk_poll));
	iompx->evlist = (struct pollfd *)fk_mem_alloc(sizeof(struct pollfd) * (max_conn + 1 + FK_SAVED_FD));
	bzero(iompx->evlist, sizeof(struct pollfd) * (max_conn + 1 + FK_SAVED_FD));
	iompx->last = 0;
	iompx->pidx = (int *)fk_mem_alloc(sizeof(int) * (max_conn + 1 + FK_SAVED_FD));
	memset(iompx->pidx, -1, sizeof(int));//initialize to -1

	return iompx;
}

int fk_poll_add(void *ev_iompx, int fd, unsigned char type)
{
	int idx;
	short ev;
	fk_poll *iompx;
	struct pollfd *pfd;

	iompx = (fk_poll *)ev_iompx;

	idx = iompx->pidx[fd];
	if (idx == -1) {//never add this fd
		idx =  iompx->last;
	}
	pfd = iompx->evlist + idx;

	ev = 0x0000;
	if (type & FK_EV_READ) {
		ev |= POLLIN;
	}
	if (type & FK_EV_WRITE) {
		ev |= POLLOUT;
	}
	pfd->events |= ev;
	pfd->fd = fd;
	iompx->pidx[fd] = idx;
	if (idx == iompx->last) {//a new fd
		iompx->last++;
		fk_log_debug("after add: last %d\n", iompx->last);
	}

	return 0;
}

int fk_poll_remove(void *ev_iompx, int fd, unsigned char type)
{
	int idx;
	short ev;
	fk_poll *iompx;
	struct pollfd *pfd;

	iompx = (fk_poll *)ev_iompx;

	idx = iompx->pidx[fd];
	if (idx == -1) {//never add this fd
		fk_log_warn("try to remove a non-existing ev\n");
		return -1;
	}
	pfd = iompx->evlist + idx;

	ev = 0x000;
	if (type & FK_EV_READ) {
		ev |= POLLIN;
	}
	if (type & FK_EV_WRITE) {
		ev |= POLLOUT;
	}
	pfd->events &= (~ev);
	if (pfd->events == 0x000) {//need to delete
		pfd->fd = 0;
		pfd->revents = 0;
		memcpy(pfd, iompx->evlist + iompx->last - 1, sizeof(struct pollfd));
		bzero(iompx->evlist + iompx->last - 1, sizeof(struct pollfd));
		iompx->last--;
	}

	return 0;
}

int fk_poll_dispatch(void *ev_iompx, struct timeval *timeout)
{
	fk_poll *iompx;
	struct pollfd *pfd;
	unsigned char type;
	int i, nfds, ms_timeout, fd;

	ms_timeout = -1;
	if (timeout != NULL) {
		ms_timeout = FK_UTIL_TV2MS(timeout);
	}
	fk_log_debug("timeout: %d\n", ms_timeout);

	iompx = (fk_poll *)ev_iompx;

	fk_log_debug("to poll\n");
	fk_log_debug("---last: %d\n", iompx->last);
	nfds = poll(iompx->evlist, iompx->last, ms_timeout);
	if (nfds < 0) {
		fk_log_debug("poll return: %s\n", strerror(errno));
		return -1;
	}
	for (i = 0; i < nfds; i++) {
		pfd = iompx->evlist + i;
		fd = pfd->fd;
		type = 0x00;
		if (pfd->revents & POLLIN) {
			type |= FK_EV_READ;
		}
		if (pfd->revents & POLLOUT) {
			type |= FK_EV_WRITE;
		}
		fk_ev_ioev_activate(fd, type);
	}
	return 0;
}
