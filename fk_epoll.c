#include <sys/epoll.h>

typedef struct _fk_epoll {
	int efd;
	int max_evs;
	struct epoll_event ev;//temporary variable
	struct epoll_event *evlist;
	unsigned char *emask;
} fk_epoll;

static void *fk_epoll_create(int max_fds);
static int fk_epoll_add(void *ev_iompx, int fd, unsigned char type);
static int fk_epoll_remove(void *ev_iompx, int fd, unsigned char type);
static int fk_epoll_dispatch(void *ev_iompx, struct timeval *timeout);

fk_mpxop epoll_op = {
	fk_epoll_create,
	fk_epoll_add,
	fk_epoll_remove,
	fk_epoll_dispatch
};

void *fk_epoll_create(int max_fds)
{
	int efd;
	fk_epoll *iompx;

	efd = epoll_create(1);
	if (efd < 0) {
		exit(0);
	}

	iompx = (fk_epoll *)fk_mem_alloc(sizeof(fk_epoll));

	iompx->max_evs = max_fds;
	iompx->efd = efd;

	iompx->evlist = (struct epoll_event *)fk_mem_alloc(sizeof(struct epoll_event) * iompx->max_evs);
	iompx->emask = (unsigned char *)fk_mem_alloc(sizeof(unsigned char *) * iompx->max_evs);
	bzero(iompx->emask, sizeof(unsigned char *) * iompx->max_evs);

	return iompx;
}

int fk_epoll_add(void *ev_iompx, int fd, unsigned char type)
{
	int rt, op;
	fk_epoll *iompx;
	int32_t omk, nmk;
	unsigned char otyp;

	iompx = (fk_epoll *)ev_iompx;

	otyp = iompx->emask[fd];
	if (type & otyp) {
		fk_log_error("try to add existing ev\n");
		return -1;
	}

	omk = 0x0000;
	if (otyp & FK_EV_READ) {
		omk |= EPOLLIN;
	}
	if (otyp & FK_EV_WRITE) {
		omk |= EPOLLOUT;
	}

	nmk = 0x0000;
	if (type & FK_EV_READ) {
		nmk |= EPOLLIN;
	}
	if (type & FK_EV_WRITE) {
		nmk |= EPOLLOUT;
	}

	if (omk == 0x0000) {
		op = EPOLL_CTL_ADD;
		iompx->ev.events = nmk;
	} else {
		op = EPOLL_CTL_MOD;
		iompx->ev.events = omk | nmk;
	}
	iompx->ev.data.fd = fd;

	rt = epoll_ctl(iompx->efd, op, fd, &(iompx->ev));
	if (rt < 0) {
		fk_log_error("epoll_ctl failed: %s\n", strerror(errno));
		return -1;
	}

	//save emask
	if (omk == 0x0000) {
		iompx->emask[fd] = type;
	} else {
		iompx->emask[fd] = otyp | type;
	}
	return 0;
}

int fk_epoll_remove(void *ev_iompx, int fd, unsigned char type)
{
	int rt, op;
	fk_epoll *iompx;
	int32_t omk, nmk;
	unsigned char otyp;

	FK_UNUSE(type);

	iompx = (fk_epoll *)ev_iompx;

	otyp = iompx->emask[fd];
	if (!(type & otyp)) {
		fk_log_error("try to remove a non-existing ev\n");
		return -1;
	}

	omk = 0x0000;
	if (otyp & FK_EV_READ) {
		omk |= EPOLLIN;
	}
	if (otyp & FK_EV_WRITE) {
		omk |= EPOLLOUT;
	}

	nmk = 0x0000;
	if (type & FK_EV_READ) {
		nmk |= EPOLLIN;
	}
	if (type & FK_EV_WRITE) {
		nmk |= EPOLLOUT;
	}

	if (omk == nmk) {
		op = EPOLL_CTL_DEL;
		iompx->ev.events = nmk;
	} else {
		op = EPOLL_CTL_MOD;
		iompx->ev.events = omk & (~nmk);
	}
	iompx->ev.data.fd = fd;

	rt = epoll_ctl(iompx->efd, op, fd, &(iompx->ev));
	if (rt < 0) {
		fk_log_error("epoll_ctl failed: %s\n", strerror(errno));
		return -1;
	}

	if (omk == nmk) {
		iompx->emask[fd] = 0x00;
	} else {
		iompx->emask[fd] = otyp & (~type);
	}
	return 0;
}

int fk_epoll_dispatch(void *ev_iompx, struct timeval *timeout)
{
	fk_epoll *iompx;
	unsigned char type;
	int i, nfds, fd, ms_timeout;

	iompx = (fk_epoll *)ev_iompx;

	ms_timeout = -1;
	if (timeout != NULL) {
		ms_timeout = FK_UTIL_TV2MS(timeout);
	}

	//fk_log_debug("epoll to wait\n");
	nfds = epoll_wait(iompx->efd, iompx->evlist, iompx->max_evs, ms_timeout);
	//fk_log_debug("epoll return\n");
	if (nfds < 0) { }

	for (i = 0; i < nfds; i++) {
		fd = iompx->evlist[i].data.fd;
		type = 0x00;
		//printf("i: %d	fd: %d	type:%u\n", i, fd, iompx->evlist[i].events);
		if (iompx->evlist[i].events & EPOLLIN) {
			type |= FK_EV_READ;
		}
		if (iompx->evlist[i].events & EPOLLOUT) {
			type |= FK_EV_WRITE;
		}
		fk_ev_ioev_activate(fd, type);
	}

	return 0;
}
