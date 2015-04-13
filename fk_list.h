#ifndef _FK_LIST_H_
#define _FK_LIST_H_

#define FK_ITER_FORWARD 1
#define FK_ITER_BACKWARD 0

#define FK_NODE_DATA(nd) (nd->data)

typedef struct _fk_node {
	struct _fk_node *prev;
	struct _fk_node *next;
	void *data;
} fk_node;

typedef struct _fk_iter {
	fk_node *cur;
	fk_node *end;
	int dir;//0, 1
} fk_iter;

typedef struct _fk_node_op {
	// method specified
	void (*data_free)(void *);
	void *(*data_copy)(void *);
	int (*data_cmp)(void *, void *);
} fk_node_op;

typedef struct _fk_list {
	fk_node *head;
	fk_node *tail;
	int len;
	//
	fk_iter iter;

	fk_node_op *nop;
} fk_list;

fk_list *fk_list_create(fk_node_op *func);
void fk_list_destroy(fk_list *lst);
void fk_list_insert(fk_list *lst, void *val);
fk_node *fk_list_iter_next(fk_list *lst);
fk_node *fk_list_iter_begin(fk_list *lst, int dir);
int fk_list_iter_end(fk_list *lst);
void fk_list_remove(fk_list *lst, fk_node *nd);
void fk_list_init();
void fk_list_free_display();
fk_node *fk_list_head_pop(fk_list *lst);
fk_node *fk_list_search(fk_list *lst, void *key);
void fk_list_insert_only(fk_list *lst, fk_node *nd);
void fk_list_remove_only(fk_list *lst, fk_node *nd);

#endif
