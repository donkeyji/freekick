/* unix headers */
#include <sys/select.h>

/*
 * kernel dosen't remember the events to be monitored, so we
 * do this by ourself by using the fields saved_rset/saved_wset
 * in fk_select_t, we can also add some extra fields to record
 * more information for {fd, event} pairs
 */

typedef struct {
    /* store all the events to be monitored */
    fd_set      saved_rset;
    fd_set      saved_wset;

    /* passed to the select() system call */
    fd_set      work_rset;
    fd_set      work_wset;

    /* how to track the current max fd efficiently??? */
    int         max_fd;
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
    fk_select_t *iompx;

    /* select can not monitor beyond FD_SETSIZE */
    if (max_files > FD_SETSIZE) {
        return NULL;
    }

    iompx = (fk_select_t *)fk_mem_alloc(sizeof(fk_select_t));

    FD_ZERO(&(iompx->saved_rset));
    FD_ZERO(&(iompx->saved_wset));

    /* actually, no need to zero these two work fd_set */
    FD_ZERO(&(iompx->work_rset));
    FD_ZERO(&(iompx->work_wset));

    iompx->max_fd = -1; /* an invalid fd as the initial value */

    /* initialized to 0 */
    iompx->fd_map = (uint8_t *)fk_mem_calloc(max_files, sizeof(uint8_t));

    return iompx;
}

int
fk_select_add(void *ev_iompx, int fd, uint8_t type)
{
    int r_added, w_added;
    fk_select_t *iompx;

    r_added = 0;
    w_added = 0;

    iompx = (fk_select_t *)ev_iompx;

    if (type & FK_IOEV_READ) {
        if (FD_ISSET(fd, &(iompx->saved_rset))) {
            fk_log_error("try to add existing ev\n");
            return FK_EV_ERR;
        }
        FD_SET(fd, &(iompx->saved_rset));
        r_added = 1;
    }

    if (type & FK_IOEV_WRITE) {
        if (FD_ISSET(fd, &(iompx->saved_wset))) {
            fk_log_error("try to add existing ev\n");
            return FK_EV_ERR;
        }
        FD_SET(fd, &(iompx->saved_wset));
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
    int r_rmed, w_rmed, i;
    fk_select_t *iompx;

    r_rmed = 0;
    w_rmed = 0;

    iompx = (fk_select_t *)ev_iompx;

    if (type & FK_IOEV_READ) {
        /* no this fd */
        if (!FD_ISSET(fd, &(iompx->saved_rset))) {
            fk_log_error("try to remove a non-existing ev\n");
            return FK_EV_ERR;
        }
        FD_CLR(fd, &(iompx->saved_rset));
        r_rmed = 1;
    }

    if (type & FK_IOEV_WRITE) {
        /* no this fd */
        if (!FD_ISSET(fd, &(iompx->saved_wset))) {
            fk_log_error("try to remove a non-existing ev\n");
            return FK_EV_ERR;
        }
        FD_CLR(fd, &(iompx->saved_wset));
        w_rmed = 1;
    }

    if (r_rmed == 1 && w_rmed == 1) {
        iompx->fd_map[fd] = 0;
    }

    /* how to update the iompx->max_fd efficiently??? */
    if (fd == iompx->max_fd) {
        if (iompx->fd_map[fd] == 0) {
            iompx->max_fd = -1;
            /* search the current max fd */
            for (i = fd; i >= 0; i--) {
                if (iompx->fd_map[i] == 1) {
                    iompx->max_fd = i;
                    break;
                }
            }
        }
    }

    return FK_EV_OK;
}

int
fk_select_dispatch(void *ev_iompx, struct timeval *timeout)
{
    int             fd, ready_total, ready_cnt;
    uint8_t         ev;
    fk_select_t    *iompx;
    struct timeval  pto;

    iompx = (fk_select_t *)ev_iompx;

    /* need to perform this copy every time when calling select, shit!!! */
    //memcpy(&(iompx->work_rset), &(iompx->saved_rset), sizeof(fd_set));
    //memcpy(&(iompx->work_wset), &(iompx->saved_wset), sizeof(fd_set));
    iompx->work_rset = iompx->saved_rset;
    iompx->work_wset = iompx->saved_wset;

    /*
     * On Linux, the pto will be modified by select()
     * ensure that the object that timeout point to will not
     * be modified by select()
     */
    pto.tv_sec = timeout->tv_sec;
    pto.tv_usec = timeout->tv_usec;

    /*
     * why dose this select() always return with "interrupted system call"????
     * because the child process perform saving exits periodically
     */
    ready_total = select(iompx->max_fd + 1, &(iompx->work_rset), &(iompx->work_wset), NULL, &pto);

    /* error occurs */
    if (ready_total < 0) {
        /*
         * no need to care about EINTR
         * in this scenario, SIGCHLD will appear periodically
         */
        if (errno != EINTR) {
            return FK_EV_ERR;
        }
        return FK_EV_OK;
    }
    
    /* timeout */
    if (ready_total == 0) {
        return FK_EV_OK;
    }

    /* some fd are ready */
    ready_cnt = 0;
    for (fd = 0; fd <= iompx->max_fd; fd++) {
        ev = 0x00;

        /* check read */
        if (FD_ISSET(fd, &(iompx->work_rset))) {
            ev |= FK_IOEV_READ;
            ready_cnt++;
        }

        /* check write */
        if (FD_ISSET(fd, &(iompx->work_wset))) {
            ev |= FK_IOEV_WRITE;
            ready_cnt++;
        }

        fk_ev_activate_ioev(fd, ev);

        /* no need to check any more */
        if (ready_cnt == ready_total) {
            break;
        }
    }

    return FK_EV_OK;
}
