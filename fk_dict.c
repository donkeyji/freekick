#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <stdint.h>

#include <fk_dict.h>
#include <fk_mem.h>

#define FK_DICT_INIT_SIZE 4

#define fk_elt_create()	(fk_elt *)fk_mem_alloc(sizeof(fk_elt))
 
#define fk_elt_destroy(elt)	fk_mem_free(elt)

#define fk_elt_key_set(dct, elt, k)	{		\
	if ((dct)->eop->key_copy != NULL) {				\
		(elt)->key = (dct)->eop->key_copy((k));		\
	} else {										\
		(elt)->key = (k);							\
	}												\
}

#define fk_elt_value_set(dct, elt, v)	{		\
	if ((dct)->eop->val_copy != NULL) {				\
		(elt)->value = (dct)->eop->val_copy((v));	\
	} else {										\
		(elt)->value = (v);							\
	}												\
}

#define fk_elt_key_unset(dct, elt)	{			\
	if ((dct)->eop->key_free != NULL) {				\
		(dct)->eop->key_free((elt)->key);			\
	}												\
	(elt)->key = NULL;								\
}

#define fk_elt_value_unset(dct, elt)	{		\
	if ((dct)->eop->val_free != NULL) {				\
		(dct)->eop->val_free((elt)->value);			\
	}												\
	(elt)->value = NULL;							\
}

static uint32_t fk_dict_hash(fk_str *key);
static int fk_dict_stretch(fk_dict *dct);
static fk_node *fk_dict_search(fk_dict *dct, fk_str *key, int *bidx);

static fk_elt_op default_eop = {
	NULL,
	NULL,
	NULL,
	NULL
};

fk_dict *fk_dict_create(fk_elt_op *eop)
{
	fk_dict *dct;
	dct = (fk_dict *)fk_mem_alloc(sizeof(fk_dict));
	if (eop == NULL) {
		eop = &default_eop;
	}
	dct->eop = eop;
	dct->size = FK_DICT_INIT_SIZE;
	dct->limit = dct->size / 2;//when up to 50%, it should extend space
	dct->used = 0;
	dct->buckets = (fk_list **)fk_mem_alloc(sizeof(fk_list) * FK_DICT_INIT_SIZE);
	bzero(dct->buckets, sizeof(fk_list *) * FK_DICT_INIT_SIZE);
	return dct;
}

void fk_dict_destroy(fk_dict *dct)
{
	int i;
	fk_elt *elt;
	fk_list *lst;
	fk_node *nd, *nxt;

	for (i = 0; i < dct->size; i++) {
		lst = dct->buckets[i];
		if (lst == NULL) {
			continue;
		}
		nd = fk_list_iter_begin(lst, FK_LIST_ITER_H2T);
		while (!fk_list_iter_end(lst)) {
			elt = (fk_elt *)nd->data;		
			nxt = fk_list_iter_next(lst);
			fk_list_remove(lst, nd);//do not free nd->data
			fk_elt_key_unset(dct, elt);
			fk_elt_value_unset(dct, elt);
			fk_elt_destroy(elt);//free nd->data
			nd = nxt;
		}
		fk_mem_free(lst);//free the empty list
	}
	fk_mem_free(dct);//free the empty dict
}

void *fk_dict_get(fk_dict *dct, fk_str *key)
{
	fk_node *nd;
	fk_elt *elt;
	nd = fk_dict_search(dct, key, NULL);
	if (nd == NULL) {
		return NULL;
	}
	elt = (fk_elt *)nd->data;
	return elt->value;
}

fk_node *fk_dict_search(fk_dict *dct, fk_str *key, int *bidx)
{
	fk_node *nd;
	fk_elt *elt;
	fk_list *lst;
	int idx, hash, cmp;

	hash = fk_dict_hash(key);
	idx = hash & (dct->size - 1);
	if (bidx != NULL) {
		*bidx = idx;
	}
	lst = dct->buckets[idx];
	if (lst == NULL) {
		return NULL;
	}
	nd = lst->head;
	while (nd != NULL) {
		elt = (fk_elt *)nd->data;
		cmp = fk_str_cmp(elt->key, key);
		if (cmp == 0) {
			return nd;
		}
		nd = nd->next;
	}
	return NULL;
}

/*
uint32_t fk_dict_hash(fk_str *key)
{
	char *s;
	uint32_t hash;

	s = fk_str_raw(key);
	hash = 0;
	while (*s) {
		hash = (hash << 5) + hash + (uint8_t) * s++;
	}

	printf("===hash: %lu\n", hash);
	return hash;
}
*/

uint32_t fk_dict_hash(fk_str *key)
{
	int len;
	char *buf;
    uint32_t hash;

	hash = 5381;
	len = fk_str_len(key);
	buf = fk_str_raw(key);
    while (len--) {
		hash = ((hash << 5) + hash) + (*buf++);
	}
    return hash;
}

int fk_dict_add(fk_dict *dct, fk_str *key, void *value)
{
	fk_node *nd;
	fk_elt *elt;
	void *old_val;
	fk_list *lst;
	int idx;

	nd = fk_dict_search(dct, key, &idx);
	if (nd == NULL) {
		if (dct->used == dct->limit) {
			fk_dict_stretch(dct);//whether need to extend
		}
		lst = dct->buckets[idx];
		if (lst == NULL) {//need to
			dct->buckets[idx] = fk_list_create(NULL);
			lst = dct->buckets[idx];
		}
		elt = fk_elt_create();
		fk_elt_key_set(dct, elt, key);
		fk_elt_value_set(dct, elt, value);

		fk_list_insert(lst, elt);
		dct->used++;
		return 0;
	}

	//update the old value of the element
	old_val = ((fk_elt *)nd->data)->value;
	((fk_elt *)nd->data)->value = value;
	if (dct->eop->key_free != NULL) {
		dct->eop->key_free(key);
	}
	if (dct->eop->val_free != NULL) {
		dct->eop->val_free(old_val);
	}
	return 0;
}

int fk_dict_remove(fk_dict *dct, fk_str *key)
{
	fk_node *nd;
	fk_list *lst;
	fk_elt *elt;
	int idx;

	nd = fk_dict_search(dct, key, &idx);
	if (nd == NULL) {
		return -1;
	}

	lst = dct->buckets[idx];
	elt = nd->data;

	fk_list_remove(lst, nd);//do not free elt
	fk_elt_key_unset(dct, elt);
	fk_elt_value_unset(dct, elt);
	fk_elt_destroy(elt);//free nd->data
	dct->used--;

	return 0;
}

#ifdef FK_DEBUG
void fk_dict_print(fk_dict *dct)
{
	int i;
	fk_node *nd;
	fk_elt *elt;
	fk_list *lst;

	for (i = 0; i < dct->size; i++) {
		lst = dct->buckets[i];
		if (lst != NULL && lst->len > 0) {
			nd = lst->head;
			elt = nd->data;
			fk_str_print(elt->key);
		}
	}
}
#endif

int fk_dict_stretch(fk_dict *dct)
{
	fk_str *key;
	int hash, idx;
	int new_size, i;
	fk_node *nd, *nxt;
	fk_list **bks, *lst;

	if (dct->used < dct->limit) {
		return 0;
	}
	new_size = dct->size * 2;
	bks = (fk_list **)fk_mem_alloc(new_size * sizeof(fk_list *));
	bzero(bks, sizeof(fk_list *) * new_size);

	//rehash to the new buckets
	for (i = 0; i < dct->size; i++) {
		lst = dct->buckets[i];
		if (lst == NULL) {
			continue;
		}
		nd = fk_list_iter_begin(lst, FK_LIST_ITER_H2T);
		while (!fk_list_iter_end(lst)) {
			nxt = fk_list_iter_next(lst);
			fk_list_any_remove_only(lst, nd);//remove first
			key = ((fk_elt *)nd->data)->key;
			hash = fk_dict_hash(key);
			idx = hash & (new_size - 1);
			if (bks[idx] == NULL) {
				bks[idx] = fk_list_create(NULL);
				if (bks[idx] == NULL) {
					return -1;
				}
			}
			fk_list_insert_only(bks[idx], nd);//then insert
			nd = nxt;
		}
	}

	//free old buckets
	for (i = 0; i < dct->size; i++) {
		if (dct->buckets[i] != NULL) {
			fk_list_destroy(dct->buckets[i]);
		}
	}
	fk_mem_free(dct->buckets);

	dct->buckets = bks;
	dct->size = new_size;
	dct->limit = new_size / 2;

	return 0;
}
