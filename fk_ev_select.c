/* unix headers */
#include <sys/select.h>

/*
 * kernel dosen't remember the events to be monitored, so we
 * do this by ourself by using the fields rset/wset/eset in
 * fk_select_t
 */

typedef struct {
    /* store all the events to be monitored */
    fd_set      rset;
    fd_set      wset;
    fd_set      eset;

    /* passed to the select() system call */
    fd_set      run_rset;
    fd_set      run_wset;
    fd_set      run_eset;

    /* the current max fd */
    int         max_fd; /* how to track the current fd ??? */
    uint8_t    *fd_map; /* record all the fds added */
} fk_select_t;

static void *fk_select_create(int max_files);
static int fk_select_add(void *ev_iompx, int fd, uint8_t type);
static int fk_select_remove(void *ev_iompx, int fd, uint8_t type);
static int fk_select_dispatch(void *ev_iompx, struct timeval *timeout);

fk_mpxop_t select_op = {
    "select",
    fk_select_create,
    fk_select_add,
    fk_select_remove,
    fk_select_dispatch
};

void *
fk_select_create(int max_files)
{
    void  *iompx;

    /* select can not monitor beyond 1024 fds */
    if (max_files > FD_SETSIZE) {
        return FK_EV_ERR;
    }

    iompx = (fk_select_t *)fk_mem_alloc(sizeof(fk_select_t));

    FD_ZERO(&(iompx->rset));
    FD_ZERO(&(iompx->wset));
    FD_ZERO(&(iompx->eset));

    FD_ZERO(&(iompx->run_rset));
    FD_ZERO(&(iompx->run_wset));
    FD_ZERO(&(iompx->run_eset));

    iompx->max_fd = -1; /* an invalid fd as the initial value */
    iompx->fd_map = (int *)fk_mem_calloc(max_files, sizeof(uint8_t));

    return iompx;
}

int
fk_select_add(void *ev_iompx, int fd, uint8_t type)
{
    int r_added, w_added;
    fk_select_t *iompx;

    radded = 0;
    wadded = 0;

    iompx = (fk_select_t *)ev_iompx;

    if (type & FK_EV_READ) {
        if (FD_ISSET(fd, &(iompx->rset))) {
            return FK_EV_ERR;
        }
        FD_SET(fd, &(iompx->rset));
        r_added = 1;
    }

    if (type & FK_EV_WRITE) {
        if (FD_ISSET(fd, &(iompx->wset))) {
            return FK_EV_ERR;
        }
        FD_SET(fd, &(iompx->wset));
        w_added = 1;
    }

    if (r_added == 1 || w_added == 1) {
        if (fd > iompx->max_fd) {
            iompx->max_fd = fd;
        }
        iompx->fd_map[fd] = 1; /* mark it */
    }

    return FK_EV_OK;
}

int
fk_select_remove(void *ev_iompx, int fd, uint8_t type)
{
    int r_rmed, w_rmed;
    fk_select_t *iompx;

    r_rmed = 0;
    w_rmed = 0;

    iompx = (fk_select_t *)ev_iompx;

    if (type & FK_EV_READ) {
        /* no this fd */
        if (!FD_ISSET(fd, &(iompx->rset))) {
            return FK_EV_ERR;
        }
        FD_CLR(fd, &(iompx->rset));
        r_rmed = 1;
    }

    if (type & FK_EV_WRITE) {
        /* no this fd */
        if (!FD_ISSET(fd, &(iompx->wset))) {
            return FK_EV_ERR;
        }
        FD_CLR(fd, &(iompx->wset));
        w_rmed = 1;
    }

    if (r_rmed == 1 && w_rmed == 1) {
        iompx->fd_map[fd] = 0;
    }

    if (fd == iompx->max_fd) {
        if (iompx->fd_map[fd] == 0) {
        }
    }

    return FK_EV_OK;
}

int
fk_select_dispatch(void *ev_iompx, struct timeval *timeout)
{
    int           nfds, fd, cnt;
    uint8_t       ev;
    fk_select_t  *iompx;

    iompx = (fk_select_t *)ev_iompx;

    /* need to do it every time when calling select, shit!!! */
    FD_COPY(&(iompx->run_rset), &(iompx->rset));
    FD_COPY(&(iompx->run_wset), &(iompx->wset));
    FD_COPY(&(iompx->run_eset), &(iompx->eset));
    nfds = select(iompx->max + 1, &(iompx->rset), &(iompx->wset), &(iompx->eset), timeout);

    if (nfds < 0) {
        return FK_EV_ERR;
    }
    
    /* timeout */
    if (nfds == 0) {
        return FK_EV_OK;
    }

    cnt = 0;
    for (fd = 0; fd <= iompx->max_fd; fd++) {
        ev = 0x00;

        if (FD_ISSET(fd, &(iompx->rset))) {
            ev |= FK_EV_READ;
            cnt++;
        }

        if (FD_ISSET(fd, &(iompx->wset))) {
            ev |= FK_EV_WRITE;
            cnt++;
        }

        fk_ev_activate_ioev(fd, ev);

        /* no need to check any more */
        if (cnt == nfds) {
            break;
        }
    }

    return FK_EV_OK;
}
