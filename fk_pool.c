#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <fk_list.h>
#include <fk_str.h>
#include <fk_obj.h>
#include <fk_mem.h>
#include <fk_pool.h>

#define FK_POOL_BLOCK_NOCONTAIN(blk, ptr)		((uintptr_t)(ptr) < (uintptr_t)(blk)->data || (uintptr_t)(ptr) > (uintptr_t)((blk)->data + (blk)->unit_size * (blk)->unit_cnt))

#define FK_POOL_BLOCK_ISFULL(blk)		((blk)->free_cnt == 0 ? 1:0)
#define FK_POOL_BLOCK_ISEMPTY(blk)		((blk)->free_cnt == (blk)->unit_cnt ? 1:0)

#define FK_POOL_BLOCK_ALLOC(blk, ptr)	{					\
	(blk)->free_cnt--;										\
	(ptr) = (blk)->data + (blk)->unit_size * (blk)->first;	\
	(blk)->first = *((unsigned short *)(ptr));				\
}

#define FK_POOL_BLOCK_FREE(blk, ptr) {											\
	(blk)->free_cnt++;															\
	*((unsigned short *)(ptr)) = (blk)->first;									\
	(blk)->first = ((char*)(ptr) - (char*)(blk)->data) / (blk)->unit_size;		\
}

#define fk_pool_align_cal(unit_size)	((unit_size) + fk_pool_align - 1) & ~(fk_pool_align - 1)

#define FK_POOL_MAX_EMPTY_BLOCKS 1

static fk_block *fk_pool_block_create(unsigned short unit_size, unsigned short unit_cnt);
static void fk_pool_block_destroy(fk_block *blk);

static unsigned short fk_pool_align = 2;
static fk_pool *node_pool = NULL;
static fk_pool *obj_pool = NULL;
static fk_pool *str_pool = NULL;

void fk_pool_init()
{
	node_pool = fk_pool_create(sizeof(fk_node), 1024);
	obj_pool = fk_pool_create(sizeof(fk_obj), 1024);
	str_pool = fk_pool_create(sizeof(fk_str), 1024);
}

void fk_pool_end()
{
	fk_pool_destroy(node_pool);
	fk_pool_destroy(obj_pool);
	fk_pool_destroy(str_pool);
}

fk_pool *fk_pool_create(unsigned short unit_size, unsigned short init_cnt)
{
	fk_pool *pool;

	pool = (fk_pool *)fk_mem_alloc(sizeof(fk_pool));

	if (unit_size > 4) {
		pool->unit_size = fk_pool_align_cal(unit_size);
	} else if (unit_size <= 2) {
		pool->unit_size = 2;
	} else {
		pool->unit_size = 4;
	}

	pool->init_cnt = init_cnt;
	pool->empty_blks = 0;
	pool->head = NULL;
	return pool;
}

void *fk_pool_malloc(fk_pool *pool)
{
	void *ptr;
	fk_block *blk;

	blk = pool->head;

	//to find the first non-full block
	while (blk != NULL && FK_POOL_BLOCK_ISFULL(blk)) {
		blk = blk->next;
	}

	// to create a new block
	if (blk == NULL) {
		blk = (fk_block *)fk_pool_block_create(pool->unit_size, pool->init_cnt);
		if (blk == NULL) {
			return NULL;
		}
		blk->next = pool->head;
		pool->head = blk;//inset to the list as head
	}

	FK_POOL_BLOCK_ALLOC(blk, ptr);

	return ptr;
}

void fk_pool_free(fk_pool *pool, void *ptr)
{
	fk_block *cur_blk, *prev_blk;

	cur_blk = pool->head;
	prev_blk = cur_blk;//could not be 'NULL'

	while (cur_blk != NULL && FK_POOL_BLOCK_NOCONTAIN(cur_blk, ptr)) {
		prev_blk = cur_blk;
		cur_blk = cur_blk->next;
	}
	if (cur_blk == NULL) {//there is no this block
		return;
	}
	FK_POOL_BLOCK_FREE(cur_blk, ptr);

	//if the count of empty blocks beyond the upper limit
	if (FK_POOL_BLOCK_ISEMPTY(cur_blk)) {
		if (pool->empty_blks == FK_POOL_MAX_EMPTY_BLOCKS) {
			if (cur_blk == pool->head) {
				pool->head = cur_blk->next;
			} else {
				prev_blk->next = cur_blk->next;
			}
			fk_pool_block_destroy(cur_blk);
			return;
		} else {
			pool->empty_blks++;
		}
	}

	//take this block as the head
	if (cur_blk != pool->head) {
		prev_blk->next = cur_blk->next;
		cur_blk->next = pool->head;
		pool->head = cur_blk;
	}
}

void fk_pool_destroy(fk_pool *pool)
{
}

fk_block *fk_pool_block_create(unsigned short unit_size, unsigned short unit_cnt)
{
	int i;
	char *ptr;
	fk_block *blk;

	blk = (fk_block *)fk_mem_alloc(sizeof(fk_block) + unit_size * unit_cnt);
	blk->free_cnt = unit_cnt;
	blk->unit_cnt = unit_cnt;
	blk->unit_size = unit_size;
	blk->first = 0;//the 0 index unit

	ptr = blk->data;
	for (i = 0; i < unit_cnt - 1; i++) {
		*((unsigned short *)ptr) = i + 1; //point to the index of the next free unit
		ptr += unit_size;
	}
	*((unsigned short *)ptr) = -1; //the last unit

	return blk;
}

void fk_pool_block_destroy(fk_block *blk)
{
	blk->next = NULL;
	fk_mem_free(blk);
}
