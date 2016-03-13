#ifndef _FK_POOL_H_
#define _FK_POOL_H_

#include <stdint.h>

typedef struct fk_block_s fk_block_t;
struct fk_block_s {
    uint16_t       first;        /* the index of the first free unit */
    uint16_t       free_cnt;
    uint16_t       unit_cnt;
    uint16_t       unit_size;
    fk_block_t    *next;
    uint8_t        data[];
};

typedef struct {
    fk_block_t    *head;
    uint16_t       unit_size;
    uint16_t       init_cnt;
    uint32_t       empty_blks;
} fk_pool_t;

void fk_pool_init(void);
void fk_pool_exit(void);
fk_pool_t *fk_pool_create(uint16_t unit_size, uint16_t init_cnt);
void fk_pool_destroy(fk_pool_t *pool);
void *fk_pool_malloc(fk_pool_t *pool);
void fk_pool_free(fk_pool_t *pool, void *ptr);

#endif
