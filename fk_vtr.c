#include <stdlib.h>
#include <strings.h>

#include <fk_mem.h>
#include <fk_vtr.h>


fk_vtr *fk_vtr_create()
{
	fk_vtr *vtr;

	vtr = (fk_vtr *)fk_mem_alloc(sizeof(fk_vtr));
	vtr->array = (fk_str **)fk_mem_alloc(sizeof(fk_str *) * FK_VTR_INIT_LEN);
	vtr->len = FK_VTR_INIT_LEN;
	bzero(vtr->array, vtr->len * sizeof(fk_str *));
	return vtr;
}

void fk_vtr_destroy(fk_vtr *vtr)
{
	fk_mem_free(vtr->array);
	fk_mem_free(vtr);
}
