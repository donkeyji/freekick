#include <poll.h>

typedef struct {
	struct pollfd *evlist;
	int last;
	int *fd2idx;
} fk_poll;

static void *fk_poll_create(unsigned max_files);
static int fk_poll_add(void *ev_iompx, int fd, char type);
static int fk_poll_remove(void *ev_iompx, int fd, char type);
static int fk_poll_dispatch(void *ev_iompx, struct timeval *timeout);

fk_mpxop poll_op = {
	"poll",
	fk_poll_create,
	fk_poll_add,
	fk_poll_remove,
	fk_poll_dispatch
};

void *fk_poll_create(unsigned max_files)
{
	int i;
	fk_poll *iompx;

	iompx = (fk_poll *)fk_mem_alloc(sizeof(fk_poll));
	iompx->evlist = (struct pollfd *)fk_mem_alloc(sizeof(struct pollfd) * max_files);
	bzero(iompx->evlist, sizeof(struct pollfd) * max_files);
	iompx->last = 0;
	iompx->fd2idx = (int *)fk_mem_alloc(sizeof(int) * max_files);
	for (i = 0; i < max_files; i++) {
		iompx->evlist[i].fd = -1;
		iompx->fd2idx[i] = -1;
	}

	return iompx;
}

int fk_poll_add(void *ev_iompx, int fd, char type)
{
	int idx;
	short nev, oev;
	fk_poll *iompx;
	struct pollfd *pfd;

	iompx = (fk_poll *)ev_iompx;

	idx = iompx->fd2idx[fd];
	if (idx == -1) {/* never add this fd */
		idx =  iompx->last;
	}
	pfd = iompx->evlist + idx;
	oev = pfd->events;

	nev = 0x0000;
	if (type & FK_IOEV_READ) {
		nev |= POLLIN;
	}
	if (type & FK_IOEV_WRITE) {
		nev |= POLLOUT;
	}

	if (oev & nev) {
		fk_log_error("try to add an existing ev\n");
		return FK_EV_ERR;
	}

	pfd->events = oev | nev;
	pfd->fd = fd;
	iompx->fd2idx[fd] = idx;

	if (idx == iompx->last) {/* a new fd */
		iompx->last++;
		fk_log_debug("after add: last %d\n", iompx->last);
	}

	return FK_EV_OK;
}

int fk_poll_remove(void *ev_iompx, int fd, char type)
{
	int idx;
	short nev, oev;
	fk_poll *iompx;
	struct pollfd *pfd, *tail;

	iompx = (fk_poll *)ev_iompx;

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

	if (pfd->events == 0x0000) {/* need to delete */
		tail = iompx->evlist + iompx->last -1;
		memcpy(pfd, tail, sizeof(struct pollfd));
		tail->fd = -1;
		tail->events = 0x0000;
		tail->revents = 0x0000;
		iompx->last--;
	}

	return FK_EV_OK;
}

int fk_poll_dispatch(void *ev_iompx, struct timeval *timeout)
{
	char type;
	fk_poll *iompx;
	struct pollfd *pfd;
	int i, nfds, ms_timeout, fd, cnt;

	ms_timeout = -1;
	if (timeout != NULL) {
		ms_timeout = fk_util_tv2millis(timeout);
	}

	iompx = (fk_poll *)ev_iompx;

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
		if (pfd->revents & POLLIN) {
			type |= FK_IOEV_READ;
		}
		if (pfd->revents & POLLOUT) {
			type |= FK_IOEV_WRITE;
		}
		if (type == 0x00) {
			continue;
		}
		fk_ev_activate_ioev(fd, type);
		pfd->revents = 0x0000;/* reset revents */
		cnt++;
		if (cnt == nfds) {
			break;
		}
	}
	return FK_EV_OK;
}
