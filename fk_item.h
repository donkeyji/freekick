#ifndef _FK_ITEM_H_
#define _FK_ITEM_H_

#define FK_ITEM_NIL 	0
#define FK_ITEM_STR  	1
#define FK_ITEM_LIST 	2
#define FK_ITEM_DICT 	3
#define FK_ITEM_SKLIST	4

typedef struct {
	int type;
	unsigned ref;/* reference count */
	void *entity;/* point to the real data of the itm */
} fk_item;

void fk_item_init(void);
fk_item *fk_item_create(int type, void *entity);
void fk_item_dec_ref(fk_item *itm);
void fk_item_inc_ref(fk_item *itm);

#define fk_item_type(itm)	((itm)->type)
#define fk_item_ref(itm)	((itm)->ref)
#define fk_item_raw(itm)	((itm)->entity)

#endif
