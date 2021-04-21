#ifndef _FK_HEAP_H_
#define _FK_HEAP_H_

#define FK_HEAP_LEAF_HDR        size_t    idx

typedef struct {
    FK_HEAP_LEAF_HDR;
} fk_leaf_t;


typedef struct {
    int    (*leaf_cmp)(fk_leaf_t *, fk_leaf_t *);
} fk_leaf_op_t;

/* do not copy memory from outside, just save a pointer to the field of tree */
typedef struct {
    size_t           size;    /* the total length of the tree */
    size_t           last;    /* current the last item index */
    fk_leaf_t      **tree;    /* can save any type of obj */
    fk_leaf_op_t    *lop;
} fk_heap_t;

fk_heap_t *fk_heap_create(fk_leaf_op_t *func);
fk_leaf_t *fk_heap_pop(fk_heap_t *hp);
void fk_heap_push(fk_heap_t *hp, fk_leaf_t *data);
fk_leaf_t *fk_heap_root(fk_heap_t *hp);
void fk_heap_remove(fk_heap_t *hp, fk_leaf_t *leaf);
void fk_heap_empty(fk_heap_t *hp);

#endif
