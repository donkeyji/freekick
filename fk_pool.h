#ifndef _FK_POOL_H_
#define _FK_POOL_H_

#include <stdint.h>

typedef struct _fk_block {
	uint16_t first;/* the index of the first free unit */
	uint16_t free_cnt;
	uint16_t unit_cnt;
	uint16_t unit_size;
	struct _fk_block *next;
	char data[];
} fk_block;

typedef struct _fk_pool {
	fk_block *head;
	uint16_t unit_size;
	uint16_t init_cnt;
	uint32_t empty_blks;
} fk_pool;

void fk_pool_init();
void fk_pool_exit();
fk_pool *fk_pool_create(uint16_t unit_size, uint16_t init_cnt);
void fk_pool_destroy(fk_pool *pool);
void *fk_pool_malloc(fk_pool *pool);
void fk_pool_free(fk_pool *pool, void *ptr);

#endif
