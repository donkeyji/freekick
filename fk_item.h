#ifndef _FK_OBJ_H_
#define _FK_OBJ_H_

#define FK_OBJ_NIL 0
#define FK_OBJ_STR  1
#define FK_OBJ_LIST 2
#define FK_OBJ_DICT 3

typedef struct _fk_item {
	int type;
	int ref;//reference count
	void *data;//point to the real data of the obj
} fk_item;

void fk_item_init();
fk_item *fk_item_create(int type, void *data);
void fk_item_ref_dec(fk_item *obj);
void fk_item_ref_inc(fk_item *obj);
void fk_item_destroy(fk_item *obj);

#endif
