#ifndef _FK_VTR_H_
#define _FK_VTR_H_

#define FK_VTR_INIT_LEN	4

typedef struct _fk_vtr {
	int len;
	void **array;
} fk_vtr;

fk_vtr *fk_vtr_create();
void fk_vtr_destroy(fk_vtr *vtr);

#define fk_vtr_raw(vtr)		(vtr)->array

#define fk_vtr_len(vtr)		(vtr)->len

#define fk_vtr_stretch(vtr, length)	{								\
	if ((length) > (vtr)->len) {									\
		(vtr)->array = (void **)fk_mem_realloc((vtr)->array, 		\
				(sizeof(void *) * (length)));						\
		bzero((vtr)->array + (vtr)->len, (length) - (vtr)->len);	\
		(vtr)->len = (length);										\
	}																\
}

#define fk_vtr_shrink(vtr, length)	{							\
	if ((length) < FK_VTR_INIT_LEN) {							\
		(vtr)->array = (void **)fk_mem_realloc((vtr)->array, 	\
				sizeof(void *) * FK_VTR_INIT_LEN);			\
		(vtr)->len = FK_VTR_INIT_LEN;							\
	}															\
}

#define fk_vtr_adjust(vtr, length)	{								\
	if ((length) > (vtr)->len) {									\
		(vtr)->array = (void **)fk_mem_realloc((vtr)->array, 		\
				(sizeof(void *) * (length)));						\
		bzero((vtr)->array + (vtr)->len, (length) - (vtr)->len);	\
		(vtr)->len = (length);										\
	} else {														\
		if ((vtr)->len > FK_VTR_INIT_LEN 							\
				&& (length) < FK_VTR_INIT_LEN) 						\
		{															\
			(vtr)->array = (void **)fk_mem_realloc((vtr)->array, 	\
				sizeof(void *) * FK_VTR_INIT_LEN);				\
			(vtr)->len = FK_VTR_INIT_LEN;							\
		}															\
	}																\
}

#endif
