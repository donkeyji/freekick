/* c standard headers */
#include <stdio.h>
#include <stdlib.h>

/* unix headers */
#include <unistd.h>

/*
 * 1. use the better malloc/free supplied by jemalloc
 * 2. use the original malloc/free supplied by libc
 */
#if defined(FK_USE_JEMALLOC)
#include <jemalloc/jemalloc.h>
#define fk_mem_malloc_size  malloc_usable_size
#else
#if defined(__APPLE__)
#include <malloc/malloc.h>
#define fk_mem_malloc_size  malloc_size
#elif defined(__linux__)
#include <malloc.h>
#define fk_mem_malloc_size  malloc_usable_size
#elif defined(__FreeBSD__)
#include <stdlib.h>
#include <malloc_np.h>
#define fk_mem_malloc_size  malloc_usable_size
#elif defined(__CYGWIN__)
#include <malloc.h>
#define fk_mem_malloc_size  malloc_usable_size
#endif
#endif

/* local headers */
#include <fk_env.h>
#include <fk_mem.h>

/*
 * only works in the condition of single thread
 * the condition of multi-thread is not considered here
 */
static size_t total_alloc = 0;

static void fk_mem_panic(void);

void *
fk_mem_alloc(size_t size)
{
    void   *ptr;
    size_t  real_size;

    ptr = malloc(size);
    if (ptr == NULL) {
        fk_mem_panic();
    }
    real_size = fk_mem_malloc_size(ptr);
    total_alloc += real_size;
    return ptr;
}

void *
fk_mem_calloc(size_t count, size_t size)
{
    void   *ptr;
    size_t  real_size;

    ptr = calloc(count, size);
    if (ptr == NULL) {
        fk_mem_panic();
    }
    real_size = fk_mem_malloc_size(ptr);
    total_alloc += real_size;
    return ptr;
}

void *
fk_mem_realloc(void *ptr, size_t size)
{
    void   *new_ptr;
    size_t  old_size, new_size;

    old_size = fk_mem_malloc_size(ptr);

    new_ptr = realloc(ptr, size);
    if (new_ptr == NULL) {
        fk_mem_panic();
    }

    new_size = fk_mem_malloc_size(new_ptr);
    total_alloc = total_alloc - old_size + new_size;

    return new_ptr;
}

void
fk_mem_free(void *ptr)
{
    size_t  real_size;

    real_size = fk_mem_malloc_size(ptr);
    total_alloc -= real_size;
    /* how to get the size of the freeing memory??????? */
    free(ptr);
}

size_t
fk_mem_get_alloc(void)
{
    return total_alloc;
}

void
fk_mem_panic(void)
{
    fprintf(stderr, "out of memory");
    fflush(stderr);
    sleep(1);
    abort();
}
