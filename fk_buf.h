#ifndef _FK_BUF_H_
#define _FK_BUF_H_

/* c standard headers */
#include <string.h>

/* local headers */
#include <fk_util.h>

typedef struct {
    size_t    highwat;        /* a constant */
    size_t    init_len;       /* a constant */
    size_t    len;            /* a variable */
    size_t    low;            /* valid begin, init: 0, range: [0-->len], <= high */
    size_t    high;           /* free begin, init: 0, range: [0-->len], <= len */
    char      buffer[];
} fk_buf_t;

fk_buf_t *fk_buf_create(size_t init_len, size_t highwat);
void fk_buf_destroy(fk_buf_t *buf);
#ifdef FK_DEBUG
void fk_buf_print(const fk_buf_t *buf);
#endif

#define fk_buf_len(buf)             ((buf)->len)

#define fk_buf_low(buf)             ((buf)->low)

#define fk_buf_high(buf)            ((buf)->high)

#define fk_buf_highwat(buf)         ((buf)->highwat)

#define fk_buf_free_len(buf)        ((buf)->len - (buf)->high)

#define fk_buf_payload_len(buf)     ((buf)->high - (buf)->low)

#define fk_buf_free_start(buf)      ((buf)->buffer + (buf)->high)

#define fk_buf_payload_start(buf)   ((buf)->buffer + (buf)->low)

#define fk_buf_reach_highwat(buf)   ((buf)->len == (buf)->highwat)

#define fk_buf_high_inc(buf, offset)    {   \
    (buf)->high += (offset);                \
    if ((buf)->high > (buf)->len) {         \
        (buf)->high = (buf)->len;           \
    }                                       \
}

#define fk_buf_low_inc(buf, offset)     {   \
    (buf)->low += (offset);                 \
    if ((buf)->low > (buf)->high) {         \
        (buf)->low = (buf)->high;           \
    }                                       \
}

#define fk_buf_stretch(buf)     {                   \
    if ((buf)->len < (buf)->highwat) {              \
        (buf)->len <<= 1;                           \
        (buf) = (fk_buf_t *)fk_mem_realloc((buf),   \
                sizeof(fk_buf_t) + (buf)->len);     \
    }                                               \
}

#define fk_buf_shift(buf)   {           \
    if ((buf)->low > 0) {               \
        memmove((buf)->buffer,          \
            (buf)->buffer + (buf)->low, \
            (buf)->high - (buf)->low    \
        );                              \
        (buf)->high -= (buf)->low;      \
        (buf)->low = 0;                 \
    }                                   \
}

#define fk_buf_shrink(buf)  {                           \
    if ((buf)->len >= (buf)->init_len &&                \
        fk_buf_payload_len((buf)) < (buf)->init_len)    \
    {                                                   \
        fk_buf_shift((buf));                            \
        (buf) = (fk_buf_t *)fk_mem_realloc((buf),       \
                sizeof(fk_buf_t) + (buf)->init_len      \
        );                                              \
        (buf)->len = (buf)->init_len;                   \
    }                                                   \
}

#define fk_buf_alloc(buf, length)   {                   \
    if (fk_buf_free_len((buf)) < (length)) {            \
        fk_buf_shift((buf));                            \
    }                                                   \
    if (fk_buf_free_len((buf)) < (length) &&            \
        (buf)->len < (buf)->highwat)                    \
    {                                                   \
        (buf)->len = fk_util_min_power((buf)->len       \
            + (length) - fk_buf_free_len((buf)));       \
        (buf) = (fk_buf_t *)fk_mem_realloc((buf),       \
                sizeof(fk_buf_t) + (buf)->len);         \
    }                                                   \
}

#endif
