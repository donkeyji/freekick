#ifndef _FK_OBJ_H_
#define _FK_OBJ_H_

#define FK_OBJ_NULL 0
#define FK_OBJ_STR  1
#define FK_OBJ_LIST 2
#define FK_OBJ_DICT 3

typedef struct _fk_obj {
	int type;
	int ref;//reference count
	void *data;//point to the real data of the obj
} fk_obj;

void fk_obj_init();
fk_obj *fk_obj_create(int type, void *data);
void fk_obj_destroy(fk_obj *obj);

#endif
