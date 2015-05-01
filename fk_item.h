#ifndef _FK_OBJ_H_
#define _FK_OBJ_H_

#define FK_OBJ_NIL 0
#define FK_ITEM_STR  1
#define FK_ITEM_LIST 2
#define FK_ITEM_DICT 3

typedef struct _fk_item {
	int type;
	int ref;//reference count
	void *obj;//point to the real data of the itm
} fk_item;

void fk_item_init();
fk_item *fk_item_create(int type, void *obj);
void fk_item_ref_dec(fk_item *itm);
void fk_item_ref_inc(fk_item *itm);
void fk_item_destroy(fk_item *itm);

#define fk_item_type(itm)	((itm)->type)
#define fk_item_ref(itm)	((itm)->ref)

#endif
