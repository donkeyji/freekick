#include <stdlib.h>

#include <fk_mem.h>
#include <fk_skiplist.h>

#define fk_skipnode_create(level)       fk_mem_alloc(sizeof(fk_skipnode_t) + ((level) - 1) * sizeof(fk_skipnode_t *))

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

    sl = fk_mem_alloc(sizeof(fk_skiplist_t));
    sl->level = 0;
    sl->len = 0;
    sl->skop = &default_skop;
    if (skop != NULL) {
        sl->skop = skop;
    }

    nd = fk_skipnode_create(FK_SKLIST_MAX_LEVEL);
    /*
     * head is a empty node which donot hold a score nor a fk_item
     * the head node should be processed specially
     * could not use fk_skipnode_set_data(sl, nd, 0, NULL);
     */
    nd->score = 0;
    nd->data = NULL;
    for (i = 0; i < FK_SKLIST_MAX_LEVEL; i++) {
        nd->next[i] = NULL;
    }
    sl->head = nd;

    return sl;
}

void
fk_skiplist_destroy(fk_skiplist_t *sl)
{
    fk_skipnode_t  *p, *q;

    p = sl->head;

    /* the head node should be processed specially */
    q = p->next[0];
    fk_skipnode_destroy(p); /* free the head node */
    p = q;

    /* from the lowest list */
    while (p != NULL) {
        q = p->next[0]; /* save the next node */
        fk_skipnode_free_data(sl, p);
        fk_skipnode_destroy(p); /* free the current node */
        p = q; /* go to the next node */
    }

    fk_mem_free(sl);
}

/* data: hold the fk_item_t object */
void
fk_skiplist_insert(fk_skiplist_t *sl, int score, void *data)
{
    int32_t         i, nlv;
    fk_skipnode_t  *p, *q, *nd, *update[FK_SKLIST_MAX_LEVEL];

    p = sl->head;
    q = NULL;

    for (i = FK_SKLIST_MAX_LEVEL - 1; i >= 0; i--) {
        q = p->next[i]; /* the first node in this level */
        /* if the score exist already, insert this new node after the older one */
        while (q != NULL && q->score <= score) {
            p = q;
            q = p->next[i];
        }
        /*
         * at this time, these condition below are satisfied:
         * (1) q == NULL || q->score >= score
         * (2) q == p->next[i]
         */
        update[i] = p;
    }

    /* generate a random level for this new node */
    nlv = fk_skipnode_rand_level();

    /*
     * maybe nlv is greater than sl->level
     * so we need to generate more lists
     */
    if (nlv > sl->level) {
        for (i = sl->level; i < nlv; i++) {
            update[i] = sl->head; /* use the head as the previous node */
        }
        sl->level = nlv; /* set this nlv as the level of skiplist */
    }

    nd = fk_skipnode_create(nlv);
    fk_skipnode_set_data(sl, nd, score, data);

    /* insert in nlv levels */
    for (i = 0; i < nlv; i++) {
        nd->next[i] = update[i]->next[i];
        update[i]->next[i] = nd;
    }

    sl->len++;
}

/* remove node by score */
void
fk_skiplist_remove(fk_skiplist_t *sl, int score)
{
    int32_t         i;
    fk_skipnode_t  *p, *q, *nd, *update[FK_SKLIST_MAX_LEVEL];

    p = sl->head;
    q = NULL;

    for (i = FK_SKLIST_MAX_LEVEL - 1; i >= 0; i--) {
        q = p->next[i];
        while (q != NULL && q->score < score) {
            p = q;
            q = p->next[i];
        }
        /*
         * at this time, these condition below are satisfied:
         * (1) q == NULL || q->score >= score
         * (2) q == p->next[i]
         */
        update[i] = p;
    }

    /* even not found in the lowest level list */
    if (q == NULL || (q != NULL && q->score != score)) {
        return;
    }

    nd = q;
    for (i = sl->level - 1; i >= 0; i--) {
        if (update[i]->next[i] == nd) { /* need to remove */
            update[i]->next[i] = nd->next[i];
            /*
             * if the node removed is the current toppest,
             * just decrease the level of the skiplist
             */
            if (sl->head->next[i] == NULL) {
                sl->level--;
            }
        }
    }

    fk_skipnode_free_data(sl, nd);
    fk_skipnode_destroy(nd);

    sl->len--;
}

fk_skipnode_t *
fk_skiplist_search(fk_skiplist_t *sl, int score)
{
    int32_t         i;
    fk_skipnode_t  *p, *q;

    p = sl->head;
    q = NULL;

    for (i = sl->level - 1; i >= 0; i--) {
        q = p->next[i];
        while (q != NULL && q->score < score) {
            p = q;
            q = p->next[i];
        }
        /* if found, return directly */
        if (q->score == score) {
            return q;
        }
    }

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
