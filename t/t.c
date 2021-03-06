#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include <fk_mem.h>
#include <fk_pool.h>
#include <fk_dict.h>
#include <fk_item.h>
#include <fk_list.h>
#include <fk_skiplist.h>


typedef struct {
    int   id;
    char  b;
    int   a;
} foo_t;

#define LEN (1024*1024*5)

void t_pool(void)
{
    int i;
    void **a;
    clock_t t1, t2;

    a = malloc(sizeof(void *) * LEN);

    fk_pool_t *pool = fk_pool_create(sizeof(foo_t), 1024);
    t1 = clock();
    for (i = 0; i < LEN; i++) {
        a[i] = fk_pool_malloc(pool);
    }
    t2 = clock();
    printf("time used for malloc: %lu\n", t2 - t1);

    t1 = clock();
    for (i = 0; i < LEN; i++) {
        fk_pool_free(pool, a[i]);
    }
    t2 = clock();
    printf("time used for free: %lu\n", t2 - t1);

    t1 = clock();
    for (i = 0; i < LEN; i++) {
        a[i] = malloc(sizeof(foo_t));
    }
    t2 = clock();
    printf("time used for malloc: %lu\n", t2 - t1);

    t1 = clock();
    for (i = 0; i < LEN; i++) {
        free(a[i]);
    }
    t2 = clock();
    printf("time used for free: %lu\n", t2 - t1);
}

void t_dict(void)
{
    fk_elt_t *elt;
    fk_dict_t *dd = fk_dict_create(NULL);
    printf("11111111\n");
    fk_str_t *ss = fk_str_create("huge", 4);
    fk_str_t *gg = fk_str_create("ooxx", 4);
    fk_item_t *oo = fk_item_create(FK_ITEM_STR, gg);

    fk_dict_add(dd, ss, oo);

    fk_dict_iter_t *iter = fk_dict_iter_begin(dd);
    printf("22222222\n");
    while ((elt = fk_dict_iter_next(iter)) != NULL) {
        ss = (fk_str_t *)(elt->key);
        oo = (fk_item_t *)(elt->value);
        gg = (fk_str_t *)fk_item_raw(oo);
        printf("%s\n", fk_str_raw(ss));
        printf("%s\n", fk_str_raw(gg));
    }
    printf("33333333\n");
}

int int_cmp(void *a, void *b)
{
    int x = *((int *)a);
    int y = *((int *)b);
    return x - y;
}

fk_node_op_t oopp = {
    NULL,
    NULL,
    int_cmp
};

void t_list(void)
{
    int i;
    fk_node_t *nd;
    int x[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    fk_list_t *ll = fk_list_create(&oopp);
    for (i = 0; i < 10; i++) {
        fk_list_insert_head(ll, x + i);
    }
    printf("list len: %zu\n", ll->len);
    fk_list_iter_t *iter = fk_list_iter_begin(ll, FK_LIST_ITER_H2T);
    nd = fk_list_iter_next(iter);
    while (nd != NULL) {
        printf("%d\n", *(int *)(nd->data));
        nd = fk_list_iter_next(iter);
    }
    fk_list_iter_end(iter);
}

void t_skiplist(void)
{
    int i, level;
    fk_skiplist_t *sl;
    fk_skipnode_t *p, *q;

    sl = fk_skiplist_create(NULL);

    fk_skiplist_insert(sl, 5, NULL);
    fk_skiplist_insert(sl, 3, NULL);
    fk_skiplist_insert(sl, 4, NULL);
    fk_skiplist_insert(sl, 11, NULL);
    fk_skiplist_insert(sl, 44, NULL);
    fk_skiplist_insert(sl, 24, NULL);
    fk_skiplist_insert(sl, 33, NULL);

    level = fk_skiplist_level(sl);
    printf("level: %d\n", level);

    for (i = level - 1; i >= 0; i--) {
        printf("%d---------\n", i);
        p = sl->head;
        q = p->next[i];
        while (q != NULL) {
            printf("%d\n", q->score);
            q = q->next[i];
        }
    }

    printf("remove node:\n");
    fk_skiplist_remove(sl, 44);
    fk_skiplist_remove(sl, 33);

    level = fk_skiplist_level(sl);
    printf("new level: %d\n", level);
    printf("after remove node:\n");
    for (i = level - 1; i >= 0; i--) {
        printf("%d---------\n", i);
        p = sl->head;
        q = p->next[i];
        while (q != NULL) {
            printf("%d\n", q->score);
            q = q->next[i];
        }
    }
    fk_skiplist_destroy(sl);
}

int main(void)
{
    //t_dict();
    //t_list();
    t_skiplist();
    return 0;
}
