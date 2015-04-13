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
}

int int_cmp(void *a, void *b)
{
	int x = *((int*)a);
	int y = *((int*)b);
	return x - y;
}

fk_node_op oopp = {
	NULL,
	NULL,
	int_cmp
};

void t_list()
{
	int i;
	fk_node *nd;
	int x[10] = {5, 1, 4, 2, 5, 3, 1, 1, 9, 2};
	fk_list *ll = fk_list_create(&oopp);
	for (i = 0; i < 10; i++) {
		//printf("i: %d\n", i);
		fk_list_insert(ll, x+i);
		//printf("%d\n", *(int *)(ll->head->data));
	}
	printf("len: %d\n", ll->len);
	nd = ll->head; 
	while (nd != NULL) { 
		printf("%d\n", *(int*)(nd->data));
		nd = nd->next;
		//printf("nd: %p\n", nd);
	}
}

int main()
{
	fk_list_init();
	//t_dict();	
	t_list();
    return 0;
}
