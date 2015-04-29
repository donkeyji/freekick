#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <fk_heap.h>
#include <fk_mem.h>
#include <fk_log.h>

#define FK_HEAP_INIT_SIZE 65
#define FK_HEAP_INC_SIZE 64

static void fk_heap_stretch(fk_heap *hp);

fk_heap *fk_heap_create(fk_leaf_op *lop)
{
	fk_heap *hp;

	assert(lop != NULL);

   	hp = (fk_heap *)fk_mem_alloc(sizeof(fk_heap));
	hp->array = (fk_leaf **)fk_mem_alloc(sizeof(fk_leaf *) * FK_HEAP_INIT_SIZE);
	bzero(hp->array, sizeof(fk_leaf *) * FK_HEAP_INIT_SIZE);
	hp->last = 0;
	hp->max = FK_HEAP_INIT_SIZE;
	hp->lop = lop;
	return hp;
}

void fk_heap_destroy(fk_heap *hp)
{
}

void fk_heap_remove(fk_heap *hp, fk_leaf *leaf)
{
	int i, j, cmp, idx;
	fk_leaf *parent, *tmp, *min, *left, *right;

	idx = leaf->idx;
	if (idx < 1 || idx > hp->last) {//not in this heap, necessary????
		return;
	}

	//step 1: move [hp->last] to [idx]
	hp->array[idx] = hp->array[hp->last];
	hp->array[idx]->idx = idx;
	//step 2: clear [hp->last]
	hp->array[hp->last] = NULL;
	//step 3: decrease hp->last
	hp->last -= 1;
	//step 4: mark the removed leaf as -1,
	//if step 4 is before step 1, when idx == hp->last, it will cause error
	leaf->idx = -1;

	i = idx;
	while (2 * i <= hp->last) {//donot reach the last leaf
		parent = hp->array[i];
		//to search the smallest in left, right
		left = hp->array[2 * i];
		min = left;
		j = 2 * i;
		if (2 * i + 1 <= hp->last) {//if right child exists
			right = hp->array[2 * i + 1];
			cmp = hp->lop->leaf_cmp(left, right);
			if (cmp > 0) {
				min = right;
				j = 2 * i + 1;
			}
		}
		//compare the parent with min(left, right)
		cmp = hp->lop->leaf_cmp(parent, min);
		if (cmp > 0) {
			tmp = hp->array[i];
			hp->array[i] = hp->array[j];
			hp->array[i]->idx = i;
			hp->array[j] = tmp;
			hp->array[j]->idx = j;
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
	root = hp->array[1];//the root to return

	fk_heap_remove(hp, root);

	return root;
}

void fk_heap_push(fk_heap *hp, fk_leaf *leaf)
{
	fk_leaf *tmp;
	int i, cmp;

	if (hp->last == (hp->max - 1)) {//heap is full
		fk_heap_stretch(hp);
	}
	hp->last += 1;//from 1 on
	hp->array[hp->last] = leaf;
	hp->array[hp->last]->idx = hp->last;

	i = hp->last;
	while (i / 2 > 0) { //donot reach the root
		cmp = hp->lop->leaf_cmp(hp->array[i], hp->array[i / 2]);
		if (cmp < 0) {//swap position
			tmp = hp->array[i / 2];
			hp->array[i / 2] = hp->array[i];
			hp->array[i / 2]->idx = i / 2;
			hp->array[i] = tmp;
			hp->array[i]->idx = i;
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
	return hp->array[1];//the first item as the root
}

void fk_heap_stretch(fk_heap *hp)
{
	int inc_size, total_size;
	inc_size = sizeof(fk_leaf *) * FK_HEAP_INC_SIZE;
	total_size = inc_size + hp->max * sizeof(fk_leaf *);
	hp->array = (fk_leaf **)fk_mem_realloc(hp->array, total_size);
	hp->max += FK_HEAP_INC_SIZE;
	//to initialize the new alloc memory
	bzero(hp->array + hp->max, inc_size);
}
