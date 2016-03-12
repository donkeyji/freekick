#include <stdlib.h>

#include <fk_mem.h>
#include <fk_item.h>
#include <fk_list.h>
#include <fk_str.h>
#include <fk_dict.h>

#define FK_FREE_OBJS_MAX 1024

fk_list_t *free_objs = NULL;

/*
static fk_item_t *fk_item_free_obj_get(void);
static void fk_item_free_obj_put(fk_item_t *itm);
*/

void
fk_item_init(void)
{
    /* create a null list for free objs */
    free_objs = fk_list_create(NULL);
}

/*
fk_item_t *
fk_item_free_obj_get(void)
{
    fk_item_t  *itm;
    fk_node_t  *nd;

    nd = fk_list_head_pop(free_objs);
    itm = nd->entity;
    if (itm == NULL) {
        itm = (fk_item_t *)fk_mem_alloc(sizeof(fk_item_t)); //really malloc memory here
        itm->type = FK_ITEM_NIL;
        itm->ref = 0;
        itm->entity = NULL;
    }
    return itm;
}

void
fk_item_free_obj_put(fk_item_t *itm)
{
    if (free_objs->len < FK_FREE_OBJS_MAX) {
        fk_list_insert(free_objs, itm);
    } else {
        fk_mem_free(itm);//really free memory
    }
}

void
fk_item_put_free(fk_item_t *itm)
{
    itm->type = FK_ITEM_NIL;
    itm->entity = NULL;
    itm->ref = 0;
    if (free_objs->len < FK_FREE_OBJS_MAX) {
        fk_list_insert(free_objs, itm);
    } else {//beyond the upper limit
        fk_mem_free(itm);//really free memory
    }
}
*/

fk_item_t *
fk_item_create(uint8_t type, void *entity)
{
    fk_item_t  *itm;

    itm = (fk_item_t *)fk_mem_alloc(sizeof(fk_item_t));
    itm->entity = entity;
    itm->ref = 0;/* I think the initial value of ref should be 0, not 1 */
    itm->type = type;

    return itm;
}

void
fk_item_dec_ref(fk_item_t *itm)
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
        itm->entity = NULL;
        itm->ref = 0;
        itm->type = FK_ITEM_NIL;
        fk_mem_free(itm);
    }
}

void
fk_item_inc_ref(fk_item_t *itm)
{
    itm->ref++;
}
