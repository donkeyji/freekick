#ifndef _FK_EV_H_
#define _FK_EV_H_

#include <stdint.h>

#include <sys/time.h>

#include <fk_list.h>
#include <fk_heap.h>

#define FK_EV_OK		0
#define FK_EV_ERR		-1

/* for file ev */
#define FK_IOEV_READ 	0x01
#define FK_IOEV_WRITE 	0x02

/* for timer ev */
#define FK_TMEV_CYCLE 	1
#define FK_TMEV_ONCE	2

typedef void (*fk_ioev_cb) (int, uint8_t, void *);
typedef int (*fk_tmev_cb) (uint32_t, uint8_t, void *);

typedef struct fk_ioev_s fk_ioev_t;
struct fk_ioev_s {
    int fd;
    uint8_t type;/* FK_IOEV_READ|FK_IOEV_WRITE */
    int activated;/* whether in activated list */
    void *arg;
    fk_ioev_cb iocb;

    fk_ioev_t *prev;
    fk_ioev_t *next;
};

typedef struct fk_tmev_s fk_tmev_t;
struct fk_tmev_s {
    FK_HEAP_LEAF_HEADER;/* for heap */

    int expired;/* whether in the expired list */
    uint8_t type;/* FK_TMEV_CYCLE|FK_TMEV_ONCE */
    uint32_t interval;/* milliseconds */
    struct timeval when;/* save the trigger point time: now + timeout */
    void *arg;/* ext arg */
    fk_tmev_cb tmcb;

    fk_tmev_t *prev;
    fk_tmev_t *next;
};

fk_rawlist_def(fk_ioev_t, fk_ioev_list_t);
fk_rawlist_def(fk_tmev_t, fk_tmev_list_t);

typedef struct {
    char *iompx_name;/* fk_str_t is not used here, because it's easier to use char* here */
    void *(*iompx_create)(unsigned max_files);
    int (*iompx_add)(void *iompx, int fd, uint8_t type);
    int (*iompx_remove)(void *iompx, int fd, uint8_t type);
    int (*iompx_dispatch)(void *iompx, struct timeval *timeout);
} fk_mpxop_t;

typedef struct {
    int stop;
    unsigned max_files;
    size_t ioev_cnt;
    size_t tmev_cnt;
    /* use min_heap to save timer ev */
    fk_heap_t *timer_heap;

    /* actived file ev */
    fk_ioev_list_t *act_ioev;
    fk_tmev_list_t *exp_tmev;/* when SINGLE expire, fk_mem_free() */
    fk_tmev_list_t *old_tmev;

    /* use array to save file ev */
    /* conn + listen */
    fk_ioev_t **read_ev;/* max_files */
    fk_ioev_t **write_ev;/* max_files */

    /* implemention of epoll/kqueue/poll */
    void *iompx;
} fk_evmgr_t;

void fk_ev_init(unsigned max_files);
int fk_ev_dispatch(void);
void fk_ev_cycle(void);
void fk_ev_stop(void);
int fk_ev_add_ioev(fk_ioev_t *ioev);
int fk_ev_remove_ioev(fk_ioev_t *ioev);
int fk_ev_add_tmev(fk_tmev_t *tmev);
int fk_ev_remove_tmev(fk_tmev_t *tmev);
#ifdef FK_DEBUG
void fk_ev_stat(void);
#endif
char *fk_ev_iompx_name(void);

fk_ioev_t *fk_ioev_create(int fd, uint8_t type, void *arg, fk_ioev_cb iocb);
void fk_ioev_destroy(fk_ioev_t *ioev);
fk_tmev_t *fk_tmev_create(uint32_t timeout, uint8_t type, void *arg, fk_tmev_cb tmcb);
void fk_tmev_destroy(fk_tmev_t *tmev);

#endif
