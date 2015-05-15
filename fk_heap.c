#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <fk_heap.h>
#include <fk_mem.h>
#include <fk_log.h>

#define FK_HEAP_INIT_SIZE 64

static void fk_heap_stretch(fk_heap *hp);
static void fk_heap_init(fk_heap *hp);
static void fk_heap_clear(fk_heap *hp);

fk_heap *fk_heap_create(fk_leaf_op *lop)
{
	fk_heap *hp;

	assert(lop != NULL);

   	hp = (fk_heap *)fk_mem_alloc(sizeof(fk_heap));
	hp->lop = lop;

	fk_heap_init(hp);

	return hp;
}

void fk_heap_init(fk_heap *hp)
{
	hp->size = FK_HEAP_INIT_SIZE;
	hp->tree = (fk_leaf **)fk_mem_alloc(sizeof(fk_leaf *) * hp->size);
	bzero(hp->tree, sizeof(fk_leaf *) * hp->size);
	hp->last = 0;
}

void fk_heap_empty(fk_heap *hp)
{
	fk_heap_clear(hp);

	fk_heap_init(hp);
}

/*only remove all the existing leaves*/
void fk_heap_clear(fk_heap *hp)
{
	size_t i;

	for (i = 0; i <= hp->last; i++) {
		hp->tree[i] = NULL;
	}
	fk_mem_free(hp->tree);
}

void fk_heap_destroy(fk_heap *hp)
{
	fk_heap_clear(hp);
	fk_mem_free(hp);
}

void fk_heap_remove(fk_heap *hp, fk_leaf *leaf)
{
	int cmp;
	size_t i, j, idx;
	fk_leaf *parent, *tmp, *min, *left, *right;

	idx = leaf->idx;
	/*index 0 is not used*/
	if (idx < 1 || idx > hp->last) {/*not in this heap, necessary????*/
		return;
	}

	/*step 1: move [hp->last] to [idx]*/
	hp->tree[idx] = hp->tree[hp->last];
	hp->tree[idx]->idx = idx;
	/*step 2: clear [hp->last]*/
	hp->tree[hp->last] = NULL;
	/*step 3: decrease hp->last*/
	hp->last -= 1;
	/*step 4: mark the removed leaf as 0,
	 *if step 4 is before step 1, when idx == hp->last, it will cause error*/
	leaf->idx = 0;

	i = idx;
	while ((i << 1) <= hp->last) {/*donot reach the last leaf*/
		parent = hp->tree[i];
		/*to search the smallest in left, right*/
		left = hp->tree[i << 1];
		min = left;
		j = i << 1;
		if ((i << 1) + 1 <= hp->last) {/*if right child exists*/
			right = hp->tree[(i << 1) + 1];
			cmp = hp->lop->leaf_cmp(left, right);
			if (cmp > 0) {
				min = right;
				j = (i << 1) + 1;
			}
		}
		/*compare the parent with min(left, right)*/
		cmp = hp->lop->leaf_cmp(parent, min);
		if (cmp > 0) {
			tmp = hp->tree[i];
			hp->tree[i] = hp->tree[j];
			hp->tree[i]->idx = i;
			hp->tree[j] = tmp;
			hp->tree[j]->idx = j;
			i = j;
		} else {
			break;
		}
	}
}

fk_leaf *fk_heap_pop(fk_heap *hp)
{
	fk_leaf *root;

	if (hp->last < 1) {//null heap
		return NULL;
	}
	root = hp->tree[1];//the root to return

	fk_heap_remove(hp, root);

	return root;
}

void fk_heap_push(fk_heap *hp, fk_leaf *leaf)
{
	int cmp;
	size_t i;
	fk_leaf *tmp;

	if (hp->last == (hp->size - 1)) {/*heap is full*/
		fk_heap_stretch(hp);
	}
	hp->last += 1;/*from index 1 on, index 0 is not used*/
	hp->tree[hp->last] = leaf;
	hp->tree[hp->last]->idx = hp->last;

	i = hp->last;
	while (i >> 1 > 0) {/*donot reach the root*/
		cmp = hp->lop->leaf_cmp(hp->tree[i], hp->tree[i >> 1]);
		if (cmp < 0) {/*swap position*/
			tmp = hp->tree[i >> 1];
			hp->tree[i >> 1] = hp->tree[i];
			hp->tree[i >> 1]->idx = i >> 1;
			hp->tree[i] = tmp;
			hp->tree[i]->idx = i;
			i >>= 1;
		} else {
			break;
		}
	}
}

fk_leaf *fk_heap_root(fk_heap *hp)
{
	if (hp->last == 0) {
		return NULL;
	}
	return hp->tree[1];/*the first item as the root*/
}

void fk_heap_stretch(fk_heap *hp)
{
	size_t new_size;

	new_size = hp->size << 1;/*double size*/
	hp->tree = (fk_leaf **)fk_mem_realloc(hp->tree, sizeof(fk_leaf *) * new_size);
	/*initialize the new allocated memory*/
	bzero(hp->tree + hp->size, sizeof(fk_leaf *) * hp->size);
	hp->size = new_size;
}
