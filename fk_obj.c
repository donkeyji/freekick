#include <stdlib.h>

#include <fk_mem.h>
#include <fk_obj.h>
#include <fk_list.h>
#include <fk_str.h>
#include <fk_dict2.h>

#define FK_FREE_OBJS_MAX 1024

fk_list *free_objs = NULL;

/*
static fk_obj *fk_obj_free_obj_get();
static void fk_obj_free_obj_put(fk_obj *obj);
*/

void fk_obj_init()
{
	//create a null list for free objs
	free_objs = fk_list_create(NULL);
}

/*
fk_obj *fk_obj_free_obj_get()
{
    fk_obj *obj;
    fk_node *nd;

    nd = fk_list_head_pop(free_objs);
    obj = nd->data;
    if (obj == NULL) {
        obj = (fk_obj *)fk_mem_alloc(sizeof(fk_obj)); //really malloc memory here
        obj->type = FK_OBJ_NIL;
        obj->ref = 0;
        obj->data = NULL;
    }
    return obj;
}

void fk_obj_free_obj_put(fk_obj *obj)
{
    if (free_objs->len < FK_FREE_OBJS_MAX) {
        fk_list_insert(free_objs, obj);
    } else {
        fk_mem_free(obj);//really free memory
    }
}

void fk_obj_put_free(fk_obj *obj)
{
    obj->type = FK_OBJ_NIL;
    obj->data = NULL;
    obj->ref = 0;
    if (free_objs->len < FK_FREE_OBJS_MAX) {
        fk_list_insert(free_objs, obj);
    } else {//beyond the upper limit
        fk_mem_free(obj);//really free memory
    }
}
*/

fk_obj *fk_obj_create(int type, void *data)
{
	fk_obj *obj;
	obj = (fk_obj *)fk_mem_alloc(sizeof(fk_obj));
	obj->data = data;
	obj->ref = 0;
	obj->type = type;

	return obj;
}

void fk_obj_destroy(fk_obj *obj)
{
	switch (obj->type) {
	case FK_OBJ_STR:
		fk_str_destroy(obj->data);
		break;
	case FK_OBJ_LIST:
		fk_list_destroy(obj->data);
		break;
	case FK_OBJ_DICT:
		fk_dict_destroy(obj->data);
		break;
	}
	obj->data = NULL;
	obj->ref = 0;
	obj->type = FK_OBJ_NIL;
	fk_mem_free(obj);

	return;
}
