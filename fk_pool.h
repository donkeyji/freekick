#ifndef _FK_POOL_H_
#define _FK_POOL_H_

typedef struct _fk_block {
	unsigned short first;//the index of the first free unit
	unsigned short free_cnt;
	unsigned short unit_cnt;
	unsigned short unit_size;
	struct _fk_block *next;
	char data[];
} fk_block;

typedef struct _fk_pool {
	fk_block *head;
	unsigned short unit_size;
	unsigned short init_cnt;
	int empty_blks;
} fk_pool;

void fk_pool_init();

void fk_pool_end();

fk_pool *fk_pool_create(unsigned short unit_size, unsigned short init_cnt);

void fk_pool_destroy(fk_pool *pool);

void *fk_pool_malloc(fk_pool *pool);

void fk_pool_free(fk_pool *pool, void *ptr);

#endif
