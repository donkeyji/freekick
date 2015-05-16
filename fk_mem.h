#ifndef _FK_MALLOC_H_
#define _FK_MALLOC_H_

#include <sys/types.h>

void *fk_mem_alloc(size_t size);
void *fk_mem_calloc(size_t count, size_t size);
void *fk_mem_realloc(void *ptr, size_t size);
void fk_mem_free(void *ptr);
size_t fk_mem_allocated();
size_t fk_mem_alloc_times();
size_t fk_mem_free_times();

#endif
