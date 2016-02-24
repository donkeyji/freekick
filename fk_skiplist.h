#ifndef _FK_SkIPLIST_H_
#define _FK_SkIPLIST_H_

#include <sys/types.h>

#define FK_SKLIST_MAX_LEVEL		16

typedef struct fk_skipnode_s fk_skipnode_t;
struct fk_skipnode_s {
	int score;/* the value to sort */
	void *data;/* hold the fk_item_t */
	fk_skipnode_t *next[1];/* at least 1 element */
};

typedef struct {
	void *(*data_copy)(void *);
	void (*data_free)(void *);
	void (*data_cmp)(void *, void *);
} fk_skipnode_op;

typedef struct {
	fk_skipnode_t *head;
	int level;/* the max level of the nodes */
	uint64_t len;
	fk_skipnode_op *skop;
} fk_skiplist;

fk_skiplist *fk_skiplist_create(fk_skipnode_op *skop);
void fk_skiplist_destroy(fk_skiplist *sl);
void fk_skiplist_insert(fk_skiplist *sl, int score, void *data);
void fk_skiplist_remove(fk_skiplist *sl, int score);
fk_skipnode_t *fk_skiplist_search(fk_skiplist *sl, int score);

#define fk_skiplist_len(sl)	((sl)->len)

#define fk_skiplist_level(sl)	((sl)->level)

#endif
