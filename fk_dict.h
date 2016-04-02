#ifndef _FK_DICT_H_
#define _FK_DICT_H_

/* c standard library headers */
#include <stdint.h>

/* local headers */
#include <fk_list.h>
#include <fk_str.h>

#define FK_DICT_OK      0
#define FK_DICT_ERR     -1

typedef struct {
    uint32_t    (*key_hash)(void *key);            /* gen hash */
    int         (*key_cmp)(void *k1, void *k2);    /* key compare */
    void       *(*key_copy)(void *key);
    void        (*key_free)(void *key);
    void       *(*val_copy)(void *val);
    void        (*val_free)(void *val);
} fk_elt_op_t;

typedef struct fk_elt_s fk_elt_t;
struct fk_elt_s {
    void        *key;
    void        *value;
    fk_elt_t    *next;
    fk_elt_t    *prev;
};

fk_rawlist_def(fk_elt_t, fk_elt_list_t);

typedef struct {
    size_t             size;
    size_t             size_mask;
    size_t             used;
    size_t             limit;
    fk_elt_list_t    **buckets;
    fk_elt_op_t       *eop;
} fk_dict_t;

typedef struct {
    fk_dict_t    *dct;
    fk_elt_t     *cur, *next;
    intmax_t      idx;           /* use the biggest integer */
} fk_dict_iter_t;

fk_dict_t *fk_dict_create(fk_elt_op_t *eop);
void fk_dict_destroy(fk_dict_t *dct);
int fk_dict_add(fk_dict_t *dct, void *key, void *value);
int fk_dict_replace(fk_dict_t *dct, void *key, void *value);
int fk_dict_remove(fk_dict_t *dct, void *key);
void *fk_dict_get(fk_dict_t *dct, void *key);
void fk_dict_empty(fk_dict_t *dct);

#ifdef FK_DEBUG
void fk_dict_print(fk_dict_t *dct);
#endif

#define fk_dict_len(dct)    ((dct)->used)
#define fk_elt_key(elt)     ((elt)->key)
#define fk_elt_value(elt)   ((elt)->value)

fk_dict_iter_t *fk_dict_iter_begin(fk_dict_t *dct);
fk_elt_t *fk_dict_iter_next(fk_dict_iter_t *iter);
void fk_dict_iter_end(fk_dict_iter_t *iter);

#endif
