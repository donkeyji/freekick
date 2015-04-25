#ifndef _FK_LIST_H_
#define _FK_LIST_H_

#define FK_LIST_ITER_H2T 1
#define FK_LIST_ITER_T2H 0

#define fk_rawlist_def(type, name)	\
typedef struct {					\
	type *head;						\
	type *tail;						\
	int len;						\
} name

#define fk_rawlist_create(type)		(type *)fk_mem_alloc(sizeof(type))

#define fk_rawlist_head(lst)	((lst)->head)

#define fk_rawlist_tail(lst)	((lst)->tail)

#define fk_rawList_len(lst)		((lst)->len)

#define fk_rawlist_init(lst)	{		\
	(lst)->head = NULL;					\
	(lst)->tail = NULL;					\
	(lst)->len = 0;						\
}

/*insert to the head*/
#define fk_rawlist_head_insert(lst, nd) {	\
    if (lst->len == 0) {					\
        nd->next = NULL;					\
        nd->prev = NULL;					\
        lst->tail = nd;						\
    } else {								\
        nd->prev = lst->head->prev;			\
        nd->next = lst->head;				\
    }										\
    lst->head = nd;							\
    lst->len++;								\
}

/*insert to the tail*/
#define fk_rawlist_tail_insert(lst, nd) {	\
    if (lst->len == 0) {					\
        nd->next = NULL;					\
        nd->prev = NULL;					\
        lst->head = nd;						\
    } else {								\
		nd->prev = lst->tail;				\
		nd->next = NULL;					\
		lst->tail->next = nd;				\
    }										\
    lst->tail = nd;							\
    lst->len++;								\
}

/*remove specified node*/
#define fk_rawlist_any_remove(lst, nd) {	\
    if (lst->len > 0) {						\
		if (lst->len == 1) {				\
			lst->head = NULL;				\
			lst->tail = NULL;				\
		} else {							\
			if (nd == lst->head) {			\
				nd->next->prev = nd->prev;	\
				lst->head = nd->next;		\
			} else if (nd == lst->tail) {	\
				nd->prev->next = nd->next;	\
				lst->tail = nd->prev;		\
			} else {						\
				nd->next->prev = nd->prev;	\
				nd->prev->next = nd->next;	\
			}								\
		}									\
		lst->len--;							\
	}										\
}

typedef struct _fk_node {
	struct _fk_node *prev;
	struct _fk_node *next;
	void *data;
} fk_node;

typedef struct _fk_iter {
	fk_node *cur;
	fk_node *end;
	int dir;/*0, 1 */
} fk_iter;

typedef struct _fk_node_op {
	/* method specified */
	void *(*data_copy)(void *);
	void (*data_free)(void *);
	int (*data_cmp)(void *, void *);
} fk_node_op;

typedef struct _fk_list {
	fk_node *head;
	fk_node *tail;
	int len;
	
	fk_iter iter;

	fk_node_op *nop;
} fk_list;

void fk_list_init();
fk_list *fk_list_create(fk_node_op *nop);
void fk_list_destroy(fk_list *lst);

void fk_list_head_insert(fk_list *lst, void *val);
void fk_list_tail_insert(fk_list *lst, void *val);
void fk_list_sorted_insert(fk_list *lst, void *val);
void fk_list_any_remove(fk_list *lst, fk_node *nd);

#define fk_list_head_insert_only	fk_rawlist_head_insert
#define fk_list_tail_insert_only	fk_rawlist_tail_insert
#define fk_list_any_remove_only		fk_rawlist_any_remove
void fk_list_sorted_insert_only(fk_list *lst, fk_node *nd);

fk_node *fk_list_head_pop(fk_list *lst);
fk_node *fk_list_search(fk_list *lst, void *key);

fk_node *fk_list_iter_next(fk_list *lst);
fk_node *fk_list_iter_begin(fk_list *lst, int dir);
int fk_list_iter_end(fk_list *lst);

#endif
