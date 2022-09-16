/* c standard headers */
#include <stdlib.h>

/* local headers */
#include <fk_mem.h>
#include <fk_skiplist.h>

#define fk_slnode_create(level)    fk_mem_alloc(sizeof(fk_slnode_t) + (level) * sizeof(fk_skiplist_level_t))

#define fk_slnode_destroy(nd) fk_mem_free(nd)

#define fk_slnode_set_data(sl, nd, sc, dt)    {     \
    (nd)->score = (sc);                             \
    if ((sl)->skop->data_copy != NULL) {            \
        (nd)->data = (sl)->skop->data_copy((dt));   \
    } else {                                        \
        (nd)->data = (dt);                          \
    }                                               \
}

#define fk_slnode_free_data(sl, nd)   {             \
    if ((sl)->skop->data_free != NULL) {            \
        (sl)->skop->data_free((nd)->data);          \
    }                                               \
    (nd)->data = NULL;                              \
}

static fk_slnode_op_t default_skop = {
    NULL,
    NULL,
    NULL
};

static int32_t fk_skipnode_rand_level(void);

fk_skiplist_t *
fk_skiplist_create(fk_slnode_op_t *skop)
{
    int             i;
    fk_skiplist_t  *sl;
    fk_slnode_t  *nd;

    sl = (fk_skiplist_t *)fk_mem_alloc(sizeof(fk_skiplist_t));
    sl->head = NULL;
    sl->tail = NULL;
    sl->level = 1;
    sl->skop = skop;

    nd = (fk_slnode_t *)fk_slnode_create(FK_SKLIST_MAX_LEVEL);
    fk_slnode_set_data(sl, nd, 0, NULL);
    for (i = 0; i < FK_SKLIST_MAX_LEVEL; i++) {
        nd->level[i].forward = NULL;
        nd->level[i].span = 0;
    }

    return sl;
}

void
fk_skiplist_destroy(fk_skiplist_t *sl)
{
}

/* data: hold the fk_item_t object */
void
fk_skiplist_insert(fk_skiplist_t *sl, int score, void *data)
{
}

/* remove node by score */
void
fk_skiplist_remove(fk_skiplist_t *sl, int score, void *data)
{
}

fk_slnode_t *
fk_skiplist_search(fk_skiplist_t *sl, int score)
{
    fk_slnode_t  *q;

    q = NULL;

    return q;
}

int32_t
fk_skipnode_rand_level(void)
{
    int32_t  level;

    level = 1;

    while (rand() % 2 != 0) {
        level++;
    }
    level = level > FK_SKLIST_MAX_LEVEL ? FK_SKLIST_MAX_LEVEL : level;

    return level;
}
