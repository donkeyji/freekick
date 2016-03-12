#include <stdlib.h>

#include <fk_cache.h>
#include <fk_mem.h>

/* save free fk_node_t */
static fk_free_nodes_t *free_nodes = NULL;

void
fk_cache_init(void)
{
    free_nodes = fk_rawlist_create(fk_free_nodes_t);
    fk_rawlist_init(free_nodes);
}

fk_node_t *
fk_cache_get_free_node(void)
{
    fk_node_t  *nd;

    nd = NULL;
    if (fk_rawlist_len(free_nodes) > 0) {
        nd = fk_rawlist_head(free_nodes);
        fk_rawlist_remove_anyone(free_nodes, nd);
    }

    if (nd == NULL) {
        nd = fk_mem_alloc(sizeof(fk_node_t));
        nd->prev = NULL;
        nd->next = NULL;
        nd->data = NULL;
    }
    return nd;
}

void
fk_cache_put_free_node(fk_node_t *nd)
{
    nd->prev = NULL;
    nd->next = NULL;
    nd->data = NULL;
    fk_rawlist_insert_head(free_nodes, nd);
}
