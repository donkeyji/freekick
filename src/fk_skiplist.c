/* c standard headers */
#include <stdlib.h>

/* local headers */
#include <fk_mem.h>
#include <fk_skiplist.h>

#define fk_skipnode_create(level)    fk_mem_alloc(sizeof(fk_skipnode_t) + (level) * sizeof(fk_skiplist_level_t))

#define fk_skipnode_destroy(nd) fk_mem_free(nd)

#define fk_skipnode_set_data(sl, nd, sc, dt)    {   \
    (nd)->score = (sc);                             \
    if ((sl)->skop->data_copy != NULL) {            \
        (nd)->data = (sl)->skop->data_copy((dt));   \
    } else {                                        \
        (nd)->data = (dt);                          \
    }                                               \
}

#define fk_skipnode_free_data(sl, nd)   {           \
    if ((sl)->skop->data_free != NULL) {            \
        (sl)->skop->data_free((nd)->data);          \
    }                                               \
    (nd)->data = NULL;                              \
}

static fk_skipnode_op_t default_skop = {
    NULL,
    NULL,
    NULL
};

static int32_t fk_skipnode_rand_level(void);

fk_skiplist_t *
fk_skiplist_create(fk_skipnode_op_t *skop)
{
    int             i;
    fk_skiplist_t  *sl;
    fk_skipnode_t  *nd;

    return sl;
}

void
fk_skiplist_destroy(fk_skiplist_t *sl)
{
    fk_skipnode_t  *p, *q;

}

/* data: hold the fk_item_t object */
void
fk_skiplist_insert(fk_skiplist_t *sl, int score, void *data)
{
    int32_t         i, nlv;
    fk_skipnode_t  *p, *q, *nd, *update[FK_SKLIST_MAX_LEVEL];

}

/* remove node by score */
void
fk_skiplist_remove(fk_skiplist_t *sl, int score, void *data)
{
}

fk_skipnode_t *
fk_skiplist_search(fk_skiplist_t *sl, int score)
{
    int32_t         i;
    fk_skipnode_t  *p, *q;


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
