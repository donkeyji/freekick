#ifndef _FK_VTR_H_
#define _FK_VTR_H_

#include <fk_str.h>

#define FK_VTR_INIT_LEN	4

typedef struct _fk_vtr {
	int len;
	fk_str **array;
} fk_vtr;

fk_vtr *fk_vtr_create();
void fk_vtr_destroy(fk_vtr *vtr);

#define FK_VTR_RAW(vtr)		(vtr)->array

#define FK_VTR_LEN(vtr)		(vtr)->len

#define FK_VTR_STRETCH(vtr, length)	{								\
	if ((length) > (vtr)->len) {									\
		(vtr)->array = (fk_str **)fk_mem_realloc((vtr)->array, 		\
				(sizeof(fk_str *) * (length)));						\
		bzero((vtr)->array + (vtr)->len, (length) - (vtr)->len);	\
		(vtr)->len = (length);										\
	}																\
}

#define FK_VTR_SHRINK(vtr, length)	{							\
	if ((length) < FK_VTR_INIT_LEN) {							\
		(vtr)->array = (fk_str **)fk_mem_realloc((vtr)->array, 	\
				sizeof(fk_str *) * FK_VTR_INIT_LEN);			\
		(vtr)->len = FK_VTR_INIT_LEN;							\
	}															\
}

#define FK_VTR_ADJUST(vtr, length)	{								\
	if ((length) > (vtr)->len) {									\
		(vtr)->array = (fk_str **)fk_mem_realloc((vtr)->array, 		\
				(sizeof(fk_str *) * (length)));						\
		bzero((vtr)->array + (vtr)->len, (length) - (vtr)->len);	\
		(vtr)->len = (length);										\
	} else {														\
		if ((vtr)->len > FK_VTR_INIT_LEN 							\
				&& (length) < FK_VTR_INIT_LEN) 						\
		{															\
			(vtr)->array = (fk_str **)fk_mem_realloc((vtr)->array, 	\
				sizeof(fk_str *) * FK_VTR_INIT_LEN);				\
			(vtr)->len = FK_VTR_INIT_LEN;							\
		}															\
	}																\
}

#endif
