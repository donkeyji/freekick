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

	nd = fk_sknode_create(FK_SKLIST_MAX_LEVEL, 0, NULL);
	for (i = 0; i < FK_SKLIST_MAX_LEVEL; i++) {
		nd->next[i] = NULL;
	}
	sl->head = nd;

	return sl;
}

void fk_sklist_destroy(fk_sklist *sl)
{
	fk_sknode *cur, *nxt;

	cur = sl->head;
	while (cur != NULL) {
		nxt = cur->next[0];
		fk_sknode_destroy(cur);
		cur = nxt;
	}
	fk_mem_free(sl);
}

void fk_sklist_insert(fk_sklist *sl, int score, void *val)
{
	int i, level;
	fk_sknode *cur, *nxt, *nd, *update[FK_SKLIST_MAX_LEVEL];

	cur = sl->head;
	for (i = FK_SKLIST_MAX_LEVEL - 1; i >= 0; i--) {
		if (cur->next[i] != NULL) {
			nxt = cur->next[i]->next;
			while (nxt->score > score) {
				nxt = nxt->next;
			}
		}
		update[i] = nxt;/* insert this one before nxt */
	}

	level = fk_sknode_rand_level();
	nd = fk_sknode_create(level, score, val);
}

void fk_sklist_remove(fk_sklist *sl, fk_sknode *nd)
{
}

fk_sknode *fk_sklist_search(fk_sklist *sl, void *val)
{
	fk_sknode *nd;

	nd = NULL;

	return nd;
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
