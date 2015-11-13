#ifndef _FK_ITEM_H_
#define _FK_ITEM_H_

#define FK_ITEM_NIL 	0
#define FK_ITEM_STR  	1
#define FK_ITEM_LIST 	2
#define FK_ITEM_DICT 	3
#define FK_ITEM_SKLIST	4

typedef struct _fk_item {
	int type;
	unsigned ref;/* reference count */
	void *entity;/* point to the real data of the itm */
} fk_item;

void fk_item_init();
fk_item *fk_item_create(int type, void *entity);
void fk_item_ref_dec(fk_item *itm);
void fk_item_ref_inc(fk_item *itm);
#ifdef FK_DEBUG
size_t fk_item_free_obj_cnt();
#endif

#define fk_item_type(itm)	((itm)->type)
#define fk_item_ref(itm)	((itm)->ref)
#define fk_item_raw(itm)	((itm)->entity)

#endif
