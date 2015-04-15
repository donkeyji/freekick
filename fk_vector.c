#include <strings.h>

#include <fk_mem.h>
#include <fk_vector.h>


fk_vector *fk_vector_create()
{
	fk_vector *vtr;

	vtr = (fk_vector *)fk_mem_alloc(sizeof(fk_vector));
	if (vtr == NULL) {
		return NULL;
	}
	vtr->array = (fk_str **)fk_mem_alloc(sizeof(fk_str *) * FK_VECTOR_INIT_LEN);
	if (vtr->array == NULL) {
		fk_mem_free(vtr);
		return NULL;
	}
	vtr->len = FK_VECTOR_INIT_LEN;
	bzero(vtr->array, vtr->len * sizeof(fk_str *));
	return vtr;
}

void fk_vector_destroy(fk_vector *vtr)
{
	int i;
	fk_mem_free(vtr->array);
	fk_mem_free(vtr);
}
