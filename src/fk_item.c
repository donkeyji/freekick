/* c standard headers */
#include <stdlib.h>

/* local headers */
#include <fk_mem.h>
#include <fk_item.h>
#include <fk_list.h>
#include <fk_str.h>
#include <fk_dict.h>
#include <fk_num.h>
#include <fk_log.h>

#define FK_FREE_ITEM_MAX 1024

fk_rawlist_def(fk_item_t, fk_item_list_t);

/* manage all items in use */
fk_item_list_t *itm_used = NULL;

/* a free item list as cache */
fk_item_list_t *itm_free = NULL;


void
fk_item_init(void)
{
    itm_used = fk_rawlist_create(fk_item_list_t);
    fk_rawlist_init(itm_used);

    itm_free = fk_rawlist_create(fk_item_list_t);
    fk_rawlist_init(itm_free);
}

fk_item_t *
fk_item_create(uint8_t type, void *entity)
{
    fk_item_t  *itm;

    /* frist try to get a free one from itm_free list */
    if (fk_rawlist_len(itm_free) > 0) {
        itm = fk_rawlist_head(itm_free);
        fk_rawlist_remove_anyone(itm_free, itm);
    } else {
        itm = (fk_item_t *)fk_mem_alloc(sizeof(fk_item_t));
    }

    itm->entity = entity;
    /*
     * the initial ref should be 0 rather than 1
     * coz it's external reference count
     */
    itm->ref = 0;
    itm->type = type;

    /* save the itm_used */
    itm->prev = NULL;
    itm->next = NULL;

    /* insert the new item to the itm_used */
    fk_rawlist_insert_head(itm_used, itm);

    return itm;
}

/* decrease external reference count */
void
fk_item_dec_ref(fk_item_t *itm)
{
    if (itm->ref > 0) {
        itm->ref--;
    }

    if (itm->ref > 0) {
        return;
    }

    switch (itm->type) {
    case FK_ITEM_STR:
        fk_str_destroy((fk_str_t *)(itm->entity));
        break;
    case FK_ITEM_LIST:
        fk_list_destroy((fk_list_t *)(itm->entity));
        break;
    case FK_ITEM_DICT:
        fk_dict_destroy((fk_dict_t *)(itm->entity));
        break;
    case FK_ITEM_NUM:
        fk_num_destroy((fk_num_t *)(itm->entity));
        break;
    }

    /* reset to item to NIL */
    itm->entity = NULL;
    itm->ref = 0;
    itm->type = FK_ITEM_NIL;

    /* remove from itm_used */
    fk_rawlist_remove_anyone(itm_used, itm);

    if (fk_rawlist_len(itm_free) == FK_FREE_ITEM_MAX) {
        fk_mem_free(itm);
    } else {
        fk_rawlist_insert_tail(itm_free, itm);
    }
}

/* increase external reference count */
void
fk_item_inc_ref(fk_item_t *itm)
{
    itm->ref++;
}

/* garbage collection */
void
fk_item_gc(void)
{
    fk_log_info("item_free: %zu, item_used: %zu, total: %zu\n",
            fk_rawlist_len(itm_free),
            fk_rawlist_len(itm_used),
            fk_rawlist_len(itm_free) + fk_rawlist_len(itm_used)
    );
}
