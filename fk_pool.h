#ifndef _FK_POOL_H_
#define _FK_POOL_H_

#include <stdint.h>

typedef struct fk_block_s fk_block;
struct fk_block_s {
	uint16_t first;/* the index of the first free unit */
	uint16_t free_cnt;
	uint16_t unit_cnt;
	uint16_t unit_size;
	fk_block *next;
	char data[];
};

typedef struct {
	fk_block *head;
	uint16_t unit_size;
	uint16_t init_cnt;
	uint32_t empty_blks;
} fk_pool;

void fk_pool_init(void);
void fk_pool_exit(void);
fk_pool *fk_pool_create(uint16_t unit_size, uint16_t init_cnt);
void fk_pool_destroy(fk_pool *pool);
void *fk_pool_malloc(fk_pool *pool);
void fk_pool_free(fk_pool *pool, void *ptr);

#endif
