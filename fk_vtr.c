#include <stdlib.h>
#include <strings.h>

#include <fk_mem.h>
#include <fk_vtr.h>


fk_vtr_t *fk_vtr_create(size_t init_len)
{
	fk_vtr_t *vtr;

	vtr = (fk_vtr_t *)fk_mem_alloc(sizeof(fk_vtr_t)); 
	vtr->init_len = init_len;
	vtr->len = init_len;
	vtr->array = fk_mem_alloc(sizeof(void *) * vtr->init_len);
	bzero(vtr->array, sizeof(void *) * vtr->init_len);
	return vtr;
}

void fk_vtr_destroy(fk_vtr_t *vtr)
{
	fk_mem_free(vtr->array);
	fk_mem_free(vtr);
}
