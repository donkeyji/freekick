#ifndef _FK_SkLIST_H_
#define _FK_SkLIST_H_

#include <sys/types.h>

#define FK_SKLIST_MAX_LEVEL		16

typedef struct _fk_skipnode {
	int score;/* the value to sort */
	void *data;/* hold the fk_item */
	struct _fk_skipnode *next[1];/* at least 1 element */
} fk_skipnode;

typedef struct _fk_skipnode_op {
	void *(*data_copy)(void *);
	void (*data_free)(void *);
	void (*data_cmp)(void *, void *);
} fk_skipnode_op;

typedef struct _fk_skiplist {
	fk_skipnode *head;
	int level;/* the max level of the nodes */
	size_t len;
	fk_skipnode_op *skop;
} fk_skiplist;

fk_skiplist *fk_skiplist_create(fk_skipnode_op *skop);
void fk_skiplist_destroy(fk_skiplist *sl);
void fk_skiplist_insert(fk_skiplist *sl, int score, void *data);
void fk_skiplist_remove(fk_skiplist *sl, int score);
fk_skipnode *fk_skiplist_search(fk_skiplist *sl, int score);

#define fk_skiplist_len(sl)	((sl)->len)

#define fk_skiplist_level(sl)	((sl)->level)

#endif
