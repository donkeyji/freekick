#ifndef _FK_DICT_H_
#define _FK_DICT_H_

#include <fk_obj.h>
#include <fk_str.h>
#include <fk_list.h>

typedef struct _fk_elt_op {
	fk_str *(*key_copy)(fk_str *key);//0 or 1
	void (*key_free)(fk_str *key);
	void *(*val_copy)(void *val);
	void (*val_free)(void *val);
} fk_elt_op;

typedef struct _fk_elt {
	fk_str *key;
	void *value;
} fk_elt;

typedef struct _fk_dict {
	int size;
	int used;
	int limit;
	fk_node_op *lf;
	fk_list **buckets;
	fk_elt_op *eop;
} fk_dict;

fk_dict *fk_dict_create();
void fk_dict_destroy(fk_dict *dct);
void fk_dict_add(fk_dict *dct, fk_str *key, void *value);
int fk_dict_remove(fk_dict *dct, fk_str *key);
void fk_dict_print(fk_dict *dct);
void *fk_dict_get(fk_dict *dct, fk_str *key);

#endif
