#include <stdlib.h>
#include <stdio.h>

#include <fk_list.h>
#include <fk_mem.h>

#define fk_node_create()		(fk_node *)fk_mem_alloc(sizeof(fk_node))

#define fk_node_destroy(nd)	fk_mem_free((nd))

#define fk_node_data_set(lst, nd, dt)	{			\
	if ((lst)->nop->data_copy != NULL) {			\
		(nd)->data = (lst)->nop->data_copy((dt));	\
	} else {										\
		(nd)->data = (dt);							\
	}												\
}

#define fk_node_data_free(lst, nd)	{				\
	if ((lst)->nop->data_free != NULL) {			\
		(lst)->nop->data_free((nd)->data);			\
	}												\
	(nd)->data = NULL;								\
}

#define fk_list_init	fk_rawlist_init
static void fk_list_clear(fk_list *lst);

static fk_node_op default_nop = {
	NULL,
	NULL,
	NULL
};

fk_list *fk_list_create(fk_node_op *nop)
{
	fk_list *lst;

	lst = (fk_list *)fk_mem_alloc(sizeof(fk_list));
	lst->nop = &default_nop;
	if (nop != NULL) {
		lst->nop = nop;
	}
	fk_list_init(lst);

	return lst;
}


void fk_list_insert_sorted_only(fk_list *lst, fk_node *nd)
{
	int pos;
	fk_node *low, *high;

	low = lst->head;
	high = lst->tail;

	if (lst->len == 0) {
		lst->head = nd;
		lst->tail = nd;
		nd->prev = NULL;
		nd->next = NULL;
		lst->len++;
		return;
	}

	pos = 0;/* low */
	//while (low != NULL && high != NULL) {
	//while (high != NULL) {
	while (low != NULL) {
		if (lst->nop->data_cmp(low->data, nd->data) >= 0) {
			pos = 0;/* use low as position */
			break;
		}
		if (lst->nop->data_cmp(high->data, nd->data) <= 0) {
			pos = 1;/* use high as position */
			break;
		}
		low = low->next;
		high = high->prev;
	}

	if (pos == 0) {/* use low: before low */
		if (low == lst->head) {
			lst->head = nd;
			nd->prev = NULL;
		} else {
			low->prev->next = nd;
			nd->prev = low->prev;
		}
		low->prev = nd;
		nd->next = low;
	} else {/* use high: behind high */
		if (high == lst->tail) {
			lst->tail = nd;
			nd->next = NULL;
		} else {
			high->next->prev = nd;
			nd->next = high->next;
		}
		nd->prev = high;
		high->next = nd;
	}
	lst->len++;

	return;
}

fk_list_iter *fk_list_iter_begin(fk_list *lst, int dir)
{
	fk_list_iter *iter;

   	iter = (fk_list_iter *)fk_mem_alloc(sizeof(fk_list_iter));
	iter->lst = lst;
	iter->dir = dir;
	iter->cur = NULL;
	iter->next = NULL;

	return iter;
}

fk_node *fk_list_iter_next(fk_list_iter *iter)
{
	if (iter->cur == NULL) {/* the first time to call this function */
		/* from head to tail */
		if (iter->dir == FK_LIST_ITER_H2T) {
			iter->cur = iter->lst->head;
		}
		/* from tail to head */
		if (iter->dir == FK_LIST_ITER_T2H) {
			iter->cur = iter->lst->tail;
		}
	} else {
		/* use cur as return value */
		iter->cur = iter->next;
		/* if visiting not completed */
	}

	if (iter->cur != NULL) {
		if (iter->dir == FK_LIST_ITER_H2T) {
			iter->next = iter->cur->next;
		}
		if (iter->dir == FK_LIST_ITER_T2H) {
			iter->next = iter->cur->prev;
		}
	}

	return iter->cur;
}

void fk_list_iter_end(fk_list_iter *iter)
{
	fk_mem_free(iter);/* just release the memory */
}

void fk_list_insert_head(fk_list *lst, void *val)
{
	fk_node *nd;

	nd = fk_node_create();

	fk_node_data_set(lst, nd, val);

	fk_list_insert_head_only(lst, nd);
}

void fk_list_insert_tail(fk_list *lst, void *val)
{
	fk_node *nd;

	nd = fk_node_create();

	fk_node_data_set(lst, nd, val);

	fk_list_insert_tail_only(lst, nd);
}

void fk_list_sorted_insert(fk_list *lst, void *val)
{
	fk_node *nd;

	nd = fk_node_create();

	fk_node_data_set(lst, nd, val);

	fk_list_insert_sorted_only(lst, nd);
}

void fk_list_remove_anyone(fk_list *lst, fk_node *nd)
{
	fk_list_remove_anyone_only(lst, nd);

	fk_node_data_free(lst, nd);
	fk_node_destroy(nd);
}

fk_node *fk_list_search(fk_list *lst, void *key)
{
	int cmp;
	fk_node *nd;

	nd = lst->head;
	while (nd != NULL) {
		if (lst->nop->data_cmp != NULL) {
			cmp = lst->nop->data_cmp(nd->data, key);
		} else {
			cmp = (uintptr_t)nd->data - (uintptr_t)key;/* compare the address */
		}
		if (cmp == 0) {
			return nd;
		}
		nd = nd->next;
	}
	return NULL;
}

void fk_list_empty(fk_list *lst)
{
	fk_list_clear(lst);

	fk_list_init(lst);
}

/* remove all nodes */
void fk_list_clear(fk_list *lst)
{
	fk_node *nd;

	nd = fk_list_head(lst);
	while (nd != NULL) {
		fk_list_remove_anyone(lst, nd);
		nd = fk_list_head(lst);
	}
}

void fk_list_destroy(fk_list *lst)
{
	fk_list_clear(lst);

	fk_mem_free(lst);
}
