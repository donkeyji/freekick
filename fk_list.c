#include <stdlib.h>
#include <stdio.h>

#include <fk_list.h>
#include <fk_mem.h>

#define fk_node_create()		(fk_node *)fk_mem_alloc(sizeof(fk_node))

#define fk_node_destroy(nd)	fk_mem_free((nd))

#define fk_node_data_set(lst, nd, dt)	{		\
	if ((lst)->nop->data_copy != NULL) {			\
		(nd)->data = (lst)->nop->data_copy((dt));	\
	} else {										\
		(nd)->data = (dt);							\
	}												\
}

#define fk_node_data_free(lst, nd)	{		\
	if ((lst)->nop->data_free != NULL) {			\
		(lst)->nop->data_free((nd)->data);			\
	}												\
	(nd)->data = NULL;								\
}

static fk_node_op default_nop = {
	NULL,
	NULL,
	NULL
};

fk_list *fk_list_create(fk_node_op *nop)
{
	fk_list *lst;

	lst = (fk_list *)fk_mem_alloc(sizeof(fk_list));
	lst->head = NULL;
	lst->tail = NULL;
	lst->len = 0;
	if (nop == NULL) {
		lst->nop = &default_nop;
	} else {
		lst->nop = nop;
	}

	return lst;
}


void fk_list_sorted_insert_only(fk_list *lst, fk_node *nd)
{
	int pos;
	fk_node *low, *high;

	low = lst->head;
	high = lst->tail;

	if (low == NULL && high == NULL) {//empty list
		lst->head = nd;
		lst->tail = nd;
		nd->prev = NULL;
		nd->next = NULL;
		lst->len++;
		return;
	}

	pos = 0;//low
	//while (low != NULL && high != NULL) {
	//while (high != NULL) {
	while (low != NULL) {
		if (lst->nop->data_cmp(low->data, nd->data) >= 0) {
			pos = 0;//use low as position
			break;
		}
		if (lst->nop->data_cmp(high->data, nd->data) <= 0) {
			pos = 1;//use high as position
			break;
		}
		low = low->next;
		high = high->prev;
	}

	if (pos == 0) {//use low: before low
		if (low == lst->head) {
			lst->head = nd;
			nd->prev = NULL;
		} else {
			low->prev->next = nd;
			nd->prev = low->prev;
		}
		low->prev = nd;
		nd->next = low;
	} else {//use high: behind high
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

fk_node *fk_list_iter_begin(fk_list *lst, int dir)
{
	lst->iter.dir = dir;
	if (dir == FK_LIST_ITER_T2H) {
		lst->iter.cur = lst->tail;
	}
	if (dir == FK_LIST_ITER_H2T) {
		lst->iter.cur = lst->head;
	}
	lst->iter.end = NULL;
	return lst->iter.cur;
}

fk_node *fk_list_iter_next(fk_list *lst)
{
	if (lst->iter.dir == FK_LIST_ITER_T2H) {
		lst->iter.cur = lst->iter.cur->prev;
	}
	if (lst->iter.dir == FK_LIST_ITER_H2T) {
		lst->iter.cur = lst->iter.cur->next;
	}
	return lst->iter.cur;
}

int fk_list_iter_end(fk_list *lst)
{
	if (lst->iter.cur == lst->iter.end) {
		return 1;
	}
	return 0;
}

void fk_list_head_insert(fk_list *lst, void *val)
{
	fk_node *nd;

	nd = fk_node_create();

	fk_node_data_set(lst, nd, val);

	fk_list_head_insert_only(lst, nd);
}

void fk_list_tail_insert(fk_list *lst, void *val)
{
	fk_node *nd;

	nd = fk_node_create();

	fk_node_data_set(lst, nd, val);

	fk_list_tail_insert_only(lst, nd);
}

void fk_list_sorted_insert(fk_list *lst, void *val)
{
	fk_node *nd;

	nd = fk_node_create();

	fk_node_data_set(lst, nd, val);

	fk_list_sorted_insert_only(lst, nd);
}

void fk_list_any_remove(fk_list *lst, fk_node *nd)
{
	fk_list_any_remove_only(lst, nd);

	fk_node_data_free(lst, nd);
	fk_node_destroy(nd);
}

/*remove the head from the list, and return the head*/
fk_node *fk_list_head_pop(fk_list *lst)
{
	fk_node *nd;

	nd = NULL;
	if (lst->len > 0) {
		nd = lst->head;
		fk_list_any_remove(lst, nd);
	}
	return nd;
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
			cmp = nd->data - key;//compare the address
		}
		if (cmp == 0) {
			return nd;
		}
		nd = nd->next;
	}
	return NULL;
}

void fk_list_destroy(fk_list *lst)
{
	fk_node *nd;

	nd = lst->head;
	while (nd != NULL) {
		fk_list_any_remove(lst, nd);
	}
	fk_mem_free(lst);
}
