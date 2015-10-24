#include <stdlib.h>

#include <fk_mem.h>
#include <fk_sklist.h>

static fk_sknode *fk_sknode_create(int level, int score, void *data);
static void fk_sknode_destroy(fk_sknode *nd);

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

void fk_sklist_insert(fk_sklist *sl, void *val)
{
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
