#ifndef _FK_ITEM_H_
#define _FK_ITEM_H_

/* c standard headers */
#include <stdint.h>

#define FK_ITEM_NIL     0
#define FK_ITEM_STR     1
#define FK_ITEM_LIST    2
#define FK_ITEM_DICT    3
#define FK_ITEM_SKLIST  4

typedef struct {
    uint8_t     type;
    uint32_t    ref;       /* reference count */
    void       *entity;    /* point to the real data of the itm */
} fk_item_t;

void fk_item_init(void);
fk_item_t *fk_item_create(uint8_t type, void *entity);
void fk_item_dec_ref(fk_item_t *itm);
void fk_item_inc_ref(fk_item_t *itm);

#define fk_item_type(itm)   ((itm)->type)
#define fk_item_ref(itm)    ((itm)->ref)
#define fk_item_raw(itm)    ((itm)->entity)

#endif
