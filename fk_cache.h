#ifndef _FK_CACHE_H_
#define _FK_CACHE_H_

#include <fk_list.h>

fk_rawlist_def(fk_node, fk_free_nodes);

void fk_cache_init();

fk_node *fk_cache_free_node_get();
void fk_cache_free_node_put(fk_node *nd);

#endif
