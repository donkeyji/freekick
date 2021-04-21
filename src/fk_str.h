#ifndef _FK_STR_H_
#define _FK_STR_H_

/* c standard headers */
#include <stdint.h>

/*
 * no unified status code definiton for this string module
 */

typedef struct {
    size_t    len;      /* valid length, excluding '\0' */
    char      seq[];
} fk_str_t;

#define fk_str_raw(str) ((str)->seq)

#define fk_str_len(str) ((str)->len)

fk_str_t *fk_str_create(const char *seq, size_t len);
void fk_str_destroy(fk_str_t *src);
fk_str_t *fk_str_clone(const fk_str_t *src);
int fk_str_cmp(const fk_str_t *s1, const fk_str_t *s2);
int fk_str_is_digit(const fk_str_t *str);
int fk_str_is_positive(const fk_str_t *str);
int fk_str_is_nonminus(const fk_str_t *str);
void fk_str_2upper(fk_str_t *str);
void fk_str_2lower(fk_str_t *str);
uint32_t fk_str_hash(const fk_str_t *str);

#endif
