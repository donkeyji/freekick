#include <stdlib.h>

#include <fk_mem.h>
#include <fk_sklist.h>

fk_sklist *fk_sklist_create()
{
	fk_sklist *sl;

	sl = fk_mem_alloc(sizeof(fk_sklist));
	sl->level = 0;
	sl->len = 0;
	sl->head = NULL;

	return sl;
}

void fk_sklist_destroy(fk_sklist *sl)
{
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
