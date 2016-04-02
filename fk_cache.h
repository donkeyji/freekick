#ifndef _FK_CACHE_H_
#define _FK_CACHE_H_

/* local headers */
#include <fk_list.h>

fk_rawlist_def(fk_node_t, fk_free_nodes_t);

void fk_cache_init(void);

fk_node_t *fk_cache_get_free_node(void);
void fk_cache_put_free_node(fk_node_t *nd);

#endif
