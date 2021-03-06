#ifndef _FK_VTR_H_
#define _FK_VTR_H_

/* unix headers */
#include  <strings.h>

typedef struct {
    size_t    init_len;
    size_t    len;
    void    **array;
} fk_vtr_t;

fk_vtr_t *fk_vtr_create(size_t init_len);
void fk_vtr_destroy(fk_vtr_t *vtr);

#define fk_vtr_len(vtr)             ((vtr)->len)

#define fk_vtr_set(vtr, idx, val)   ((vtr)->array)[(idx)] = (val);

#define fk_vtr_get(vtr, idx)        ((vtr)->array)[(idx)]

#define fk_vtr_stretch(vtr, length)     do {                        \
    if ((length) > (vtr)->len) {                                    \
        (vtr)->array = (void **)fk_mem_realloc((vtr)->array,        \
                sizeof(void *) * (length));                         \
        memset((vtr)->array + (vtr)->len,                           \
                0,                                                  \
                sizeof(void *) * ((length) - (vtr)->len));          \
        (vtr)->len = (length);                                      \
    }                                                               \
} while (0);

#define fk_vtr_shrink(vtr)      do {                                \
    if ((vtr)->len > (vtr)->init_len) {                             \
        (vtr)->array = (void **)fk_mem_realloc((vtr)->array,        \
            sizeof(void *) * (vtr)->init_len);                      \
        (vtr)->len = (vtr)->init_len;                               \
    }                                                               \
} while (0)

#endif
