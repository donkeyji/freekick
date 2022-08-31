#ifndef _FK_NUM_H
#define _FK_NUM_H

typedef struct {
    long long val;
} fk_num_t;

#define fk_num_raw(num) ((num)->val)

fk_num_t *fk_num_create(long long v);
void fk_num_destroy(fk_num_t *n);

#endif
