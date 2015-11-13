#include <stdlib.h>

#include <fk_mem.h>
#include <fk_item.h>
#include <fk_list.h>
#include <fk_str.h>
#include <fk_dict.h>

#define FK_FREE_OBJS_MAX 1024

static fk_list *free_items = NULL;

static fk_item *fk_item_free_obj_get();
static void fk_item_free_obj_put(fk_item *itm);

void fk_item_init()
{
	/* create a null list for free objs */
	free_items = fk_list_create(NULL);
}

fk_item *fk_item_create(int type, void *entity)
{
	fk_item *itm;
	//itm = (fk_item *)fk_mem_alloc(sizeof(fk_item));
	itm = fk_item_free_obj_get();
	itm->entity = entity;
	itm->ref = 0;/* I think the initial value of ref should be 0, not 1 */
	itm->type = type;

	return itm;
}

void fk_item_ref_dec(fk_item *itm)
{
	if (itm->ref > 0) {
		itm->ref--;
	}
	if (itm->ref == 0) {
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
		fk_item_free_obj_put(itm);
		//itm->entity = NULL;
		//itm->ref = 0;
		//itm->type = FK_ITEM_NIL;
		//fk_mem_free(itm);
	}
}

void fk_item_ref_inc(fk_item *itm)
{
	itm->ref++;
}

fk_item *fk_item_free_obj_get()
{
    fk_node *nd;
    fk_item *itm;

	if (fk_list_len(free_items) > 0) {
		nd = fk_list_head(free_items);
		itm = (fk_item *)(nd->data);
		fk_list_any_remove(free_items, nd);
	} else {
        itm = (fk_item *)fk_mem_alloc(sizeof(fk_item)); //really malloc memory here
	}
	return itm;
}

void fk_item_free_obj_put(fk_item *itm)
{
	if (fk_list_len(free_items) < FK_FREE_OBJS_MAX) {
		fk_list_head_insert(free_items, itm);
	} else {
		fk_mem_free(itm);
	}
}

#ifdef FK_DEBUG
size_t fk_item_free_obj_cnt()
{
	size_t len;

	len = fk_list_len(free_items);

	return len;
}
#endif
