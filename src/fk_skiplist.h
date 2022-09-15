#ifndef _FK_SkIPLIST_H_
#define _FK_SkIPLIST_H_

/* unix headers */
#include <sys/types.h>

#define FK_SKLIST_MAX_LEVEL     32

typedef struct fk_skipnode_s fk_skipnode_t;

typedef struct {
    uint32_t          span;
    fk_skipnode_t    *forward;
} fk_skiplist_level_t;

struct fk_skipnode_s {
    double                score;      /* the value to sort */
    void                 *data;       /* hold the fk_item_t */
    fk_skipnode_t        *backward;
    fk_skiplist_level_t   level[];    /* at least 1 element */
};

typedef struct {
    void    *(*data_copy)(void *);
    void     (*data_free)(void *);
    void     (*data_cmp)(void *, void *);
} fk_skipnode_op_t;

typedef struct {
    fk_skipnode_t       *head;
    fk_skipnode_t       *tail;
    int32_t              level;    /* the max level of the nodes */
    size_t               len;
    fk_skipnode_op_t    *skop;
} fk_skiplist_t;

fk_skiplist_t *fk_skiplist_create(fk_skipnode_op_t *skop);
void fk_skiplist_destroy(fk_skiplist_t *sl);
void fk_skiplist_insert(fk_skiplist_t *sl, int score, void *data);
void fk_skiplist_remove(fk_skiplist_t *sl, int score);
fk_skipnode_t *fk_skiplist_search(fk_skiplist_t *sl, int score);

#define fk_skiplist_len(sl)     ((sl)->len)

#define fk_skiplist_level(sl)   ((sl)->level)

#endif
