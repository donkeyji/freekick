#ifndef _FK_HEAP_H_
#define _FK_HEAP_H_

#define FK_HEAP_LEAF_HEADER		int idx

typedef struct _fk_leaf {
	FK_HEAP_LEAF_HEADER;
} fk_leaf;

typedef struct _fk_leaf_op {
	int (*leaf_cmp)(fk_leaf *, fk_leaf *);
} fk_leaf_op;

//do not copy memory from outside, just save a pointer to the field of array
typedef struct _fk_heap {
	int max;//the total length of the array
	int last;//current the last item index
	fk_leaf **array;//can save any type of obj
	fk_leaf_op *lop;
} fk_heap;

fk_heap *fk_heap_create(fk_leaf_op *func);
fk_leaf *fk_heap_pop(fk_heap *hp);
void fk_heap_push(fk_heap *hp, fk_leaf *data);
fk_leaf *fk_heap_root(fk_heap *hp);
void fk_heap_remove(fk_heap *hp, fk_leaf *leaf);

#endif
