#include <stdlib.h>

#include <fk_mem.h>
#include <fk_sklist.h>

static fk_sknode *fk_sknode_create(int level, int score, void *data);
static void fk_sknode_destroy(fk_sknode *nd);
static int fk_sknode_rand_level();

fk_sklist *fk_sklist_create()
{
	int i;
	fk_sknode *nd;
	fk_sklist *sl;

	sl = fk_mem_alloc(sizeof(fk_sklist));
	sl->level = 0;
	sl->len = 0;

	/* head is a empty node which donot hold a score nor a fk_item */
	nd = fk_sknode_create(FK_SKLIST_MAX_LEVEL, 0, NULL);
	for (i = 0; i < FK_SKLIST_MAX_LEVEL; i++) {
		nd->next[i] = NULL;
	}
	sl->head = nd;

	return sl;
}

void fk_sklist_destroy(fk_sklist *sl)
{
	fk_sknode *p, *q;

	p = sl->head;
	/* from the lowest list */
	while (p != NULL) {
		q = p->next[0];/* save the next node */
		fk_sknode_destroy(p);/* free the current node */
		p = q;/* go to the next node */
	}

	fk_mem_free(sl);
}

void fk_sklist_insert(fk_sklist *sl, int score, void *data)
{
	int i, nlv;
	fk_sknode *p, *q, *nd, *update[FK_SKLIST_MAX_LEVEL];

	p = sl->head;
	q = NULL;

	for (i = FK_SKLIST_MAX_LEVEL - 1; i >= 0; i--) {
		q = p->next[i];/* the first node in this level */
		/* if the score exist already, insert this new node after the older one */
		while (q != NULL && q->score <= score) {
			p = q;
			q = p->next[i];
		}
		/* 
		 * at this time, these condition below are satisfied:
		 * (1) q == NULL || q->score >= score 
		 * (2) q == p->next[i]
		 */
		update[i] = p;
	}

	/* generate a random level for this new node */
	nlv = fk_sknode_rand_level();

	/* 
	 * maybe nlv is greater than sl->level
	 * so we need to generate more lists 
	 */
	if (nlv > sl->level) {
		for (i = sl->level; i < nlv; i++) {
			update[i] = sl->head;/* use the head as the previous node */
		}
		sl->level = nlv;/* set this nlv as the level of skiplist */
	}

	nd = fk_sknode_create(nlv, score, data);

	/* insert in nlv levels */
	for (i = 0; i < nlv; i++) {
		nd->next[i] = update[i]->next[i];
		update[i]->next[i] = nd;
	}

	sl->len++;
}

/* remove node by score */
void fk_sklist_remove(fk_sklist *sl, int score)
{
	int i;
	fk_sknode *p, *q, *nd, *update[FK_SKLIST_MAX_LEVEL];

	p = sl->head;
	q = NULL;

	for (i = FK_SKLIST_MAX_LEVEL - 1; i >= 0; i--) {
		q = p->next[i];
		while (q != NULL && q->score < score) {
			p = q;
			q = p->next[i];
		}
		/* 
		 * at this time, these condition below are satisfied:
		 * (1) q == NULL || q->score >= score 
		 * (2) q == p->next[i]
		 */
		update[i] = p;
	}

	/* even not found in the lowest level list */
	if (q == NULL || (q != NULL && q->score != score)) {
		return;
	}

	nd = q;
	for (i = sl->level - 1; i >= 0; i--) {
		if (update[i]->next[i] == nd) {/* need to remove */
			update[i]->next[i] = nd->next[i];
			/* 
			 * if the node removed is the current toppest, 
			 * just decrease the level of the skiplist
			 */
			if (sl->head->next[i] == NULL) {
				sl->level--;
			}
		}
	}

	fk_sknode_destroy(nd);

	sl->len--;
}

fk_sknode *fk_sklist_search(fk_sklist *sl, int score)
{
	int i;
	fk_sknode *p, *q;

	p = sl->head;
	q = NULL;

	for (i = sl->level - 1; i >=0; i--) {
		q = p->next[i];
		while (q != NULL && q->score < score) {
			p = q;
			q = p->next[i];
		}
		/* if found, return directly */
		if (q->score == score) {
			return q;
		}
	}

	return q;
}

fk_sknode *fk_sknode_create(int level, int score, void *data)
{
	fk_sknode *nd;

	nd = fk_mem_alloc(sizeof(fk_sknode) + (level - 1) * sizeof(fk_sknode *));
	nd->score = score;
	nd->data = data;

	return nd;
}

void fk_sknode_destroy(fk_sknode *nd)
{
	fk_mem_free(nd);
}

int fk_sknode_rand_level()
{
	int level;

	level = 1;

	while (level % 2 != 0) {
		level++;
	}
	level = level > FK_SKLIST_MAX_LEVEL ? FK_SKLIST_MAX_LEVEL : level;

	return level;
}
