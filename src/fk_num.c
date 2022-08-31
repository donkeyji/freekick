/* c standard headers */
#include <stdlib.h>

/* local headers */
#include <fk_num.h>
#include <fk_mem.h>

fk_num_t *
fk_num_create(long long v)
{
    fk_num_t *n;
    n = fk_mem_alloc(sizeof(fk_num_t));
    n->val = v;
    return n;
}

void
fk_num_destroy(fk_num_t *n)
{
    fk_mem_free(n);
    n = NULL;
}
