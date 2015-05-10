#ifndef _FK_VTR_H_
#define _FK_VTR_H_

#define FK_VTR_INIT_LEN	4

typedef struct _fk_vtr {
	int len;
	void *array[];
} fk_vtr;

fk_vtr *fk_vtr_create();
void fk_vtr_destroy(fk_vtr *vtr);

#define fk_vtr_len(vtr)		((vtr)->len)

#define fk_vtr_set(vtr, idx, val)	((vtr)->array)[(idx)] = (val);

#define fk_vtr_get(vtr, idx)	((vtr)->array)[(idx)]

#define fk_vtr_align(length)	(((length) + FK_VTR_INIT_LEN - 1) & ~(FK_VTR_INIT_LEN - 1))

#define fk_vtr_adjust(vtr, length)	{								\
	if ((length) > (vtr)->len) {									\
		(vtr) = (fk_vtr *)fk_mem_realloc((vtr),						\
				sizeof(fk_vtr) + 									\
				sizeof(void *) * fk_vtr_align((length)));			\
		bzero((vtr)->array + (vtr)->len, 							\
				sizeof(void *) * 									\
				(fk_vtr_align((length)) - (vtr)->len));				\
		(vtr)->len = (length);										\
	} else {														\
		if ((vtr)->len > FK_VTR_INIT_LEN 							\
				&& (length) < FK_VTR_INIT_LEN) 						\
		{															\
			(vtr) = (fk_vtr *)fk_mem_realloc((vtr), 				\
				sizeof(fk_vtr) + sizeof(void *) * FK_VTR_INIT_LEN);	\
			(vtr)->len = FK_VTR_INIT_LEN;							\
		}															\
	}																\
}

#define fk_vtr_stretch(vtr, length)		do {					\
	if ((length) > (vtr)->len) {								\
		(vtr) = (fk_vtr *)fk_mem_realloc((vtr),					\
				sizeof(fk_vtr) + 								\
				sizeof(void *) * fk_vtr_align((length)));		\
		bzero((vtr)->array + (vtr)->len, 						\
				sizeof(void *) * 								\
				(fk_vtr_align((length)) - (vtr)->len));			\
		(vtr)->len = (length);									\
	}															\
} while (0);

#define fk_vtr_shrink(vtr)		do {							\
	if ((vtr)->len > FK_VTR_INIT_LEN) {							\
		(vtr) = (fk_vtr *)fk_mem_realloc((vtr), 				\
			sizeof(fk_vtr) + sizeof(void *) * FK_VTR_INIT_LEN);	\
		(vtr)->len = FK_VTR_INIT_LEN;							\
	}															\
} while (0)

#endif
