#ifndef _FK_BUF_H
#define _FK_BUF_H

#include <string.h>

#include <fk_macro.h>
#include <fk_util.h>

#define FK_BUF_INIT_LEN 	16

typedef struct _fk_buf {
	int len;
	int low;/*valid begin, init: 0, range: [0-->len], <= high */
	int high;/*free begin, init: 0, range: [0-->len], <= len */
	char data[];
} fk_buf;

fk_buf *fk_buf_create();
void fk_buf_destroy(fk_buf *buf);
void fk_buf_print(fk_buf *buf);

#define fk_buf_free_len(buf) 	((buf)->len - (buf)->high)

#define fk_buf_payload_len(buf)	((buf)->high - (buf)->low)

#define fk_buf_free_start(buf) 	((buf)->data + (buf)->high)

#define fk_buf_payload_start(buf) ((buf)->data + (buf)->low)

#define fk_buf_high_inc(buf, offset)	{	\
	(buf)->high += (offset);				\
	if ((buf)->high > (buf)->len) {			\
		(buf)->high = (buf)->len;			\
	}										\
}

#define fk_buf_low_inc(buf, offset)		{	\
   	(buf)->low += (offset);					\
	if ((buf)->low > (buf)->high) {			\
		(buf)->low = (buf)->high;			\
	}										\
}

#define fk_buf_stretch(buf)		{										\
	if ((buf)->len < FK_BUF_HIGHWAT) {									\
		(buf)->len <<= 1;												\
		(buf) = fk_mem_realloc((buf), sizeof(fk_buf) + (buf)->len);		\
	}																	\
}

#define fk_buf_shift(buf) 	{			\
	if ((buf)->low > 0) {				\
		memmove((buf)->data, 			\
			(buf)->data + (buf)->low, 	\
			(buf)->high - (buf)->low	\
		);								\
		(buf)->high -= (buf)->low;		\
		(buf)->low = 0;					\
	}									\
}

#define fk_buf_shrink(buf)	{							\
	if ((buf)->len >= FK_BUF_INIT_LEN					\
		&& (buf)->high - (buf)->low < FK_BUF_INIT_LEN) 	\
	{													\
		memmove((buf)->data,							\
			(buf)->data + (buf)->low,					\
			(buf)->high - (buf)->low					\
		);												\
		(buf)->high -= (buf)->low;						\
		(buf)->low = 0;									\
		(buf) = fk_mem_realloc((buf), 					\
				sizeof(fk_buf) + FK_BUF_INIT_LEN		\
		);												\
		(buf)->len = FK_BUF_INIT_LEN;					\
	}													\
}

#define fk_buf_adjust(buf, length)	{									\
	if (fk_buf_free_len((buf)) < (length)) {							\
		fk_buf_shift((buf));											\
	}																	\
	if (fk_buf_free_len((buf)) < (length)) {							\
		(buf)->len = (FK_BUF_HIGHWAT > fk_util_min_power((length)) ?	\
			 fk_util_min_power((length)) : FK_BUF_HIGHWAT);				\
		(buf) = (fk_buf *)fk_mem_realloc((buf), 						\
			sizeof(fk_buf) + (buf)->len);								\
	}																	\
}

#endif
