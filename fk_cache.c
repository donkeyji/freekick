#include <stdlib.h>

#include <fk_cache.h>
#include <fk_mem.h>

/* save free fk_node */
static fk_free_nodes *free_nodes = NULL;

void fk_cache_init()
{
	free_nodes = fk_rawlist_create(fk_free_nodes);
	fk_rawlist_init(free_nodes);
}

fk_node *fk_cache_free_node_get()
{
	fk_node *nd;

	nd = NULL;
	if (fk_rawlist_len(free_nodes) > 0) {
		nd = fk_rawlist_head(free_nodes);
		fk_rawlist_any_remove(free_nodes, nd);
	}

	if (nd == NULL) {
		nd = fk_mem_alloc(sizeof(fk_node));
		nd->prev = NULL;
		nd->next = NULL;
		nd->data = NULL;
	}
	return nd;
}

void fk_cache_free_node_put(fk_node *nd)
{
	nd->prev = NULL;
	nd->next = NULL;
	nd->data = NULL;
	fk_rawlist_head_insert(free_nodes, nd);
}
