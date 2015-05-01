#include <stdlib.h>

#include <fk_mem.h>
#include <fk_item.h>
#include <fk_list.h>
#include <fk_str.h>
#include <fk_dict.h>

#define FK_FREE_OBJS_MAX 1024

fk_list *free_objs = NULL;

/*
static fk_item *fk_item_free_obj_get();
static void fk_item_free_obj_put(fk_item *itm);
*/

void fk_item_init()
{
	//create a null list for free objs
	free_objs = fk_list_create(NULL);
}

/*
fk_item *fk_item_free_obj_get()
{
    fk_item *itm;
    fk_node *nd;

    nd = fk_list_head_pop(free_objs);
    itm = nd->entity;
    if (itm == NULL) {
        itm = (fk_item *)fk_mem_alloc(sizeof(fk_item)); //really malloc memory here
        itm->type = FK_OBJ_NIL;
        itm->ref = 0;
        itm->entity = NULL;
    }
    return itm;
}

void fk_item_free_obj_put(fk_item *itm)
{
    if (free_objs->len < FK_FREE_OBJS_MAX) {
        fk_list_insert(free_objs, itm);
    } else {
        fk_mem_free(itm);//really free memory
    }
}

void fk_item_put_free(fk_item *itm)
{
    itm->type = FK_OBJ_NIL;
    itm->entity = NULL;
    itm->ref = 0;
    if (free_objs->len < FK_FREE_OBJS_MAX) {
        fk_list_insert(free_objs, itm);
    } else {//beyond the upper limit
        fk_mem_free(itm);//really free memory
    }
}
*/

fk_item *fk_item_create(int type, void *entity)
{
	fk_item *itm;
	itm = (fk_item *)fk_mem_alloc(sizeof(fk_item));
	itm->entity = entity;
	itm->ref = 0;
	itm->type = type;

	return itm;
}

void fk_item_ref_dec(fk_item *itm)
{
	itm->ref--;
	if (itm->ref == 0) {
		fk_item_destroy(itm);
	}
}

void fk_item_ref_inc(fk_item *itm)
{
	itm->ref++;
}

void fk_item_destroy(fk_item *itm)
{
	switch (itm->type) {
	case FK_ITEM_STR:
		fk_str_destroy(itm->entity);
		break;
	case FK_ITEM_LIST:
		fk_list_destroy(itm->entity);
		break;
	case FK_ITEM_DICT:
		fk_dict_destroy(itm->entity);
		break;
	}
	itm->entity = NULL;
	itm->ref = 0;
	itm->type = FK_OBJ_NIL;
	fk_mem_free(itm);

	return;
}
