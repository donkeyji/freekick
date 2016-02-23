#ifndef _FK_DICT_H_
#define _FK_DICT_H_

#include <stdint.h>

#include <fk_list.h>
#include <fk_str.h>

#define FK_DICT_OK		0
#define FK_DICT_ERR		-1

typedef struct {
	uint32_t (*key_hash)(void *key);/* gen hash */
	int (*key_cmp)(void *k1, void *k2);/* key compare */
	void *(*key_copy)(void *key);
	void (*key_free)(void *key);
	void *(*val_copy)(void *val);
	void (*val_free)(void *val);
} fk_elt_op;

typedef struct fk_elt_s fk_elt;
struct fk_elt_s {
	void *key;
	void *value;
	fk_elt *next;
	fk_elt *prev;
};

fk_rawlist_def(fk_elt, fk_elt_list);

typedef struct {
	size_t size;
	size_t size_mask;
	size_t used;
	size_t limit;
	fk_elt_list **buckets;
	fk_elt_op *eop;
} fk_dict;

typedef struct {
	fk_dict *dct;
	fk_elt *cur, *next;
	long long idx;/* should be long than size_t type */
} fk_dict_iter;

fk_dict *fk_dict_create(fk_elt_op *eop);
void fk_dict_destroy(fk_dict *dct);
int fk_dict_add(fk_dict *dct, void *key, void *value);
int fk_dict_replace(fk_dict *dct, void *key, void *value);
int fk_dict_remove(fk_dict *dct, void *key);
void *fk_dict_get(fk_dict *dct, void *key);
void fk_dict_empty(fk_dict *dct);

#ifdef FK_DEBUG
void fk_dict_print(fk_dict *dct);
#endif

#define fk_dict_len(dct)	((dct)->used)
#define fk_elt_key(elt)		((elt)->key)
#define fk_elt_value(elt)	((elt)->value)

fk_dict_iter *fk_dict_iter_begin(fk_dict *dct);
fk_elt *fk_dict_iter_next(fk_dict_iter *iter);
void fk_dict_iter_end(fk_dict_iter *iter);

#endif
