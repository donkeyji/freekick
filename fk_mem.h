#ifndef _FK_MEM_H_
#define _FK_MEM_H_

/* unix headers */
#include <sys/types.h>

void *fk_mem_alloc(size_t size);
void *fk_mem_calloc(size_t count, size_t size);
void *fk_mem_realloc(void *ptr, size_t size);
void fk_mem_free(void *ptr);
size_t fk_mem_get_alloc(void);

#endif
