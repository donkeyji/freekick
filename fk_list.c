#include <stdlib.h>
#include <stdio.h>

#include <fk_list.h>
#include <fk_mem.h>

static fk_node *fk_list_free_node_get();
static void fk_list_free_node_put(fk_node *nd);

fk_node_op default_func = {
	NULL,
	NULL,
	NULL
};

//save free node
static fk_list *free_nodes = NULL;

fk_list *fk_list_create(fk_node_op *func)
{
	fk_list *lst;

	lst = (fk_list *)fk_mem_alloc(sizeof(fk_list));
	if (lst == NULL) {}
	lst->head = NULL;
	lst->tail = NULL;
	lst->len = 0;
	if (func == NULL) {
		lst->method = &default_func;
	} else {
		lst->method = func;
	}

	return lst;
}

void fk_list_init()
{
	free_nodes = fk_list_create(NULL);
}

void fk_list_free_display()
{
	printf("free_nodes len: %d\n", free_nodes->len);
}

//do not change node->data
void fk_list_insert_only(fk_list *lst, fk_node *nd)
{
	if (lst->head == NULL) {//empty list
		nd->next = NULL;
		nd->prev = NULL;
		lst->tail = nd;//also be the tail of this list
	} else {
		nd->prev = lst->head->prev;
		nd->next = lst->head;
	}
	lst->head = nd;//new head of this list
	lst->len++;
}

void fk_list_remove_only(fk_list *lst, fk_node *nd)
{
	//put the node away from the chain
	if (lst->len == 0) {
		return;
	} else if (lst->len == 1) {
		lst->head = NULL;
		lst->tail = NULL;
	} else {//len >= 2
		if (nd == lst->head) {
			nd->next->prev = nd->prev;
			lst->head = nd->next;
		} else if (nd == lst->tail) {
			nd->prev->next = nd->next;
			lst->tail = nd->prev;
		} else { //len  >= 3
			nd->next->prev = nd->prev;
			nd->prev->next = nd->next;
		}
	}
	lst->len--;
}

fk_node *fk_list_free_node_get()
{
	fk_node *nd;

	nd = NULL;
	if (free_nodes->len > 0) {
		nd = free_nodes->head;
		fk_list_remove_only(free_nodes, nd);
	}

	if (nd == NULL) {
		nd = fk_mem_alloc(sizeof(fk_node));
		nd->prev = NULL;
		nd->next = NULL;
		nd->data = NULL;
	}
	return nd;
}

void fk_list_free_node_put(fk_node *nd)
{
	nd->prev = NULL;
	nd->next = NULL;
	nd->data = NULL;
	fk_list_insert_only(free_nodes, nd);
}

//----------------------------------------------
fk_node *fk_list_iter_begin(fk_list *lst, int dir)
{
	lst->iter.dir = dir;
	if (dir == FK_ITER_BACKWARD) {
		lst->iter.cur = lst->tail;
	}
	if (dir == FK_ITER_FORWARD) {
		lst->iter.cur = lst->head;
	}
	return lst->iter.cur;
}

fk_node *fk_list_iter_next(fk_list *lst)
{
	if (lst->iter.dir == FK_ITER_BACKWARD) {
		lst->iter.cur = lst->iter.cur->prev;
	}
	if (lst->iter.dir == FK_ITER_FORWARD) {
		lst->iter.cur = lst->iter.cur->next;
	}
	return lst->iter.cur;
}

void fk_list_insert(fk_list *lst, void *val)
{
	fk_node *nd;

	nd = fk_list_free_node_get();

	//initialize the node->data
	if (lst->method->data_copy != NULL) {
		nd->data = lst->method->data_copy(val);//do copy val memory
	} else {
		nd->data = val;//do not copy val memory
	}

	//only insert this node to the list, only the prev/next field of the node changes
	fk_list_insert_only(lst, nd);
}

void fk_list_remove(fk_list *lst, fk_node *nd)
{
	fk_list_remove_only(lst, nd);

	//whether to free the node->data
	if (lst->method->data_free != NULL) {
		lst->method->data_free(nd->data);//free node->data
	}
	fk_list_free_node_put(nd);
}

//remove the head from the list, and return the head
fk_node *fk_list_head_pop(fk_list *lst)
{
	fk_node *nd;

	nd = NULL;
	if (lst->len > 0) {
		nd = lst->head;
		fk_list_remove(lst, nd);
	}
	return nd;
}

fk_node *fk_list_search(fk_list *lst, void *key)
{
	int cmp;
	fk_node *nd;

	nd = lst->head;
	while (nd != NULL) {
		if (lst->method->data_cmp != NULL) {
			cmp = lst->method->data_cmp(nd->data, key);
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
		fk_list_remove(lst, nd);
	}
}
