#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <fk_mem.h>
#include <fk_pool.h>
#include <fk_dict.h>
#include <fk_obj.h>
#include <fk_list.h>


typedef struct {
    int id;
    char b;
    int a;
} foo;

#define LEN 1024*1024*5

/*
void t_pool()
{
    int i;
    void **a;
    clock_t t1, t2;

    a = malloc(sizeof(void *) * LEN);

    fk_pool *pool = fk_pool_create(sizeof(foo), 1024);
    t1 = clock();
    for (i=0; i<LEN; i++) {
        a[i] = fk_pool_malloc(pool);
    }
    t2 = clock();
    printf("time used for malloc: %lu\n", t2 - t1);

    t1 = clock();
    for (i=0; i<LEN; i++) {
        fk_pool_free(pool, a[i]);
    }
    t2 = clock();
    printf("time used for free: %lu\n", t2 - t1);

    t1 = clock();
    for (i=0; i<LEN; i++) {
        a[i] = malloc(sizeof(foo));
    }
    t2 = clock();
    printf("time used for malloc: %lu\n", t2 - t1);

    t1 = clock();
    for (i=0; i<LEN; i++) {
        free(a[i]);
    }
    t2 = clock();
    printf("time used for free: %lu\n", t2 - t1);
}
*/

void t_dict()
{
	fk_dict *dd = fk_dict_create();
	fk_str *ss = fk_str_create("huge", 4);
	fk_str *gg = fk_str_create("ooxx", 4);
	printf("xxxxxxxx\n");
	fk_obj *oo = fk_mem_alloc(sizeof(fk_obj));
	oo->type = FK_OBJ_STR;
	oo->data = gg;
	printf("yyyyyyyyyy\n");

	fk_dict_add(dd, ss, oo);

	fk_dict_print(dd);
}

int main()
{
	fk_list_init();
	t_dict();	
    return 0;
}
