#ifndef _FK_SkLIST_H_
#define _FK_SkLIST_H_

#include <sys/types.h>

typedef struct _fk_sknode {
	void *data;
	struct _fk_sknode *prev;
	struct _fk_sknode *next[];
} fk_sknode;

typedef struct _fk_sklist {
	fk_sknode *head;
	int level;
	size_t len;
} fk_sklist;

fk_sklist *fk_sklist_create();
void fk_sklist_destroy(fk_sklist *sl);
void fk_sklist_insert(fk_sklist *sl, void *val);
void fk_sklist_remove(fk_sklist *sl, fk_sknode *nd);
fk_sknode *fk_sklist_search(fk_sklist *sl, void *val);

#endif
