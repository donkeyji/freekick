#ifndef _FK_LIST_H_
#define _FK_LIST_H_

#include <sys/types.h>

#define FK_LIST_ITER_H2T 1
#define FK_LIST_ITER_T2H 0

#define fk_rawlist_def(type, name)      \
typedef struct {                        \
    type     *head;                     \
    type     *tail;                     \
    size_t    len;                      \
} name

#define fk_rawlist_create(type)     (type *)fk_mem_alloc(sizeof(type))

#define fk_rawlist_destroy(lst)     fk_mem_free((lst))

#define fk_rawlist_head(lst)        ((lst)->head)

#define fk_rawlist_tail(lst)        ((lst)->tail)

#define fk_rawlist_len(lst)         ((lst)->len)

#define fk_rawlist_init(lst)    {       \
    (lst)->head = NULL;                 \
    (lst)->tail = NULL;                 \
    (lst)->len = 0;                     \
}

/* insert to the head */
#define fk_rawlist_insert_head(lst, nd) {   \
    if ((lst)->len == 0) {                  \
        (nd)->next = NULL;                  \
        (nd)->prev = NULL;                  \
        (lst)->tail = (nd);                 \
    } else {                                \
        (nd)->prev = NULL;                  \
        (nd)->next = (lst)->head;           \
        (lst)->head->prev = (nd);           \
    }                                       \
    (lst)->head = (nd);                     \
    (lst)->len++;                           \
}

/* insert to the tail */
#define fk_rawlist_insert_tail(lst, nd) {   \
    if ((lst)->len == 0) {                  \
        (nd)->next = NULL;                  \
        (nd)->prev = NULL;                  \
        (lst)->head = (nd);                 \
    } else {                                \
        (nd)->prev = (lst)->tail;           \
        (nd)->next = NULL;                  \
        (lst)->tail->next = (nd);           \
    }                                       \
    (lst)->tail = (nd);                     \
    (lst)->len++;                           \
}

/* remove specified node */
#define fk_rawlist_remove_anyone(lst, nd) { \
    if ((nd)->prev != NULL) {               \
        (nd)->prev->next = (nd)->next;      \
    } else {                                \
        (lst)->head = (nd)->next;           \
    }                                       \
    if ((nd)->next != NULL) {               \
        (nd)->next->prev = (nd)->prev;      \
    } else {                                \
        (lst)->tail = (nd)->prev;           \
    }                                       \
    (nd)->prev = NULL;                      \
    (nd)->next = NULL;                      \
    (lst)->len--;                           \
}

typedef struct fk_node_s fk_node_t;
struct fk_node_s {
    fk_node_t    *prev;
    fk_node_t    *next;
    void         *data;
};

typedef struct {
    /* method specified */
    void    *(*data_copy)(void *);
    void     (*data_free)(void *);
    int      (*data_cmp)(void *, void *);
} fk_node_op_t;

typedef struct {
    fk_node_t       *head;
    fk_node_t       *tail;
    size_t           len;
    fk_node_op_t    *nop;
} fk_list_t;

typedef struct {
    fk_list_t    *lst;
    fk_node_t    *cur;     /* return this */
    fk_node_t    *next;    /* record the next */
    int           dir;     /* 0, 1 */
} fk_list_iter_t;

fk_list_t *fk_list_create(fk_node_op_t *nop);
void fk_list_empty(fk_list_t *lst);
void fk_list_destroy(fk_list_t *lst);

void fk_list_insert_head(fk_list_t *lst, void *val);
void fk_list_insert_tail(fk_list_t *lst, void *val);
void fk_list_sorted_insert(fk_list_t *lst, void *val);
void fk_list_remove_anyone(fk_list_t *lst, fk_node_t *nd);

#define fk_list_insert_head_only        fk_rawlist_insert_head
#define fk_list_insert_tail_only        fk_rawlist_insert_tail
#define fk_list_remove_anyone_only      fk_rawlist_remove_anyone
void fk_list_insert_sorted_only(fk_list_t *lst, fk_node_t *nd);

fk_node_t *fk_list_search(fk_list_t *lst, void *key);

fk_list_iter_t *fk_list_iter_begin(fk_list_t *lst, int dir);
fk_node_t *fk_list_iter_next(fk_list_iter_t *iter);
void fk_list_iter_end(fk_list_iter_t *iter);

#define fk_list_head        fk_rawlist_head
#define fk_list_tail        fk_rawlist_tail
#define fk_list_len         fk_rawlist_len

#define fk_node_raw(nd)     ((nd)->data)

#endif
