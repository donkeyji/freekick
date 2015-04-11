#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <fk_dict.h>
#include <fk_mem.h>

#define FK_DICT_INIT_SIZE 4

#define fk_dict_elt_create(elt, key, value, eop)		\
		(elt) = (fk_elt *)fk_mem_alloc(sizeof(fk_elt));	\
		if ((eop)->key_copy != NULL) {						\
			(elt)->key = (eop)->key_copy((key));			\
		} else {										\
			(elt)->key = (key);							\
		}												\
		if ((eop)->val_copy != NULL) {					\
			(elt)->value = (eop)->val_copy((value));	\
		} else {										\
			(elt)->value = (value);						\
		}												\
 
#define fk_dict_elt_destroy(elt, eop)	{				\
	if ((eop)->key_free != NULL) {							\
		(eop)->key_free((elt)->key);						\
	}													\
	if ((eop)->val_free != NULL) {						\
		(eop)->val_free((elt)->value);					\
	}													\
	fk_mem_free(elt);									\
}

static int fk_dict_hash(fk_str *key);
static int fk_dict_stretch(fk_dict *dct);
static fk_node *fk_dict_search(fk_dict *dct, fk_str *key);

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
	if (dct == NULL) {
		return NULL;
	}
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
		nd = lst->head;
		while (nd != NULL) {
			elt = (fk_elt *)nd->data;		
			nxt = nd->next;
			fk_list_remove(lst, nd);//do not free nd->data
			fk_dict_elt_destroy(elt, dct->eop);//free nd->data
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
	nd = fk_dict_search(dct, key);
	if (nd == NULL) {
		return NULL;
	}
	elt = (fk_elt *)nd->data;
	return elt->value;
}

fk_node *fk_dict_search(fk_dict *dct, fk_str *key)
{
	fk_node *nd;
	fk_elt *elt;
	fk_list *lst;
	int idx, hash, cmp;

	hash = fk_dict_hash(key);
	idx = hash & (dct->size - 1);
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

int fk_dict_hash(fk_str *key)
{
	char *s;
	unsigned long hash;

	s = FK_STR_RAW(key);
	hash = 0;
	while (*s) {
		hash = (hash << 5) + hash + (unsigned char) * s++;
	}

	return hash;
}

int fk_dict_add(fk_dict *dct, fk_str *key, void *value)
{
	fk_node *nd;
	fk_elt *elt;
	void *old_val;
	fk_list *lst;
	int hash, idx;

	nd = fk_dict_search(dct, key);
	if (nd == NULL) {
		if (dct->used == dct->limit) {
			fk_dict_stretch(dct);//whether need to extend
		}
		hash = fk_dict_hash(key);
		idx = hash & (dct->size - 1);
		lst = dct->buckets[idx];
		if (lst == NULL) {//need to
			dct->buckets[idx] = fk_list_create(NULL);
			lst = dct->buckets[idx];
		}
		fk_dict_elt_create(elt, key, value, dct->eop);
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
	int hash, idx;

	nd = fk_dict_search(dct, key);
	if (nd == NULL) {
		return -1;
	}

	hash = fk_dict_hash(key);
	idx = hash & (dct->size - 1);
	lst = dct->buckets[idx];
	elt = nd->data;

	fk_list_remove(lst, nd);//do not free elt
	fk_dict_elt_destroy(elt, dct->eop);//free the elt
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
			FK_STR_PRINT(elt->key);
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
	if (bks == NULL) {
		return -1;
	}
	bzero(bks, sizeof(fk_list *) * new_size);

	//rehash to the new buckets
	for (i = 0; i < dct->size; i++) {
		lst = dct->buckets[i];
		if (lst == NULL) {
			continue;
		}
		nd = fk_list_iter_begin(lst, FK_ITER_FORWARD);
		while (nd != fk_list_iter_end(lst)) {
			nxt = fk_list_iter_next(lst);
			fk_list_remove_only(lst, nd);//remove first
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
