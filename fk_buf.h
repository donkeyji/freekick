#ifndef _FK_BUF_H
#define _FK_BUF_H

#include <string.h>

#define FK_BUF_INIT_LEN 	16
#define FK_BUF_HIGHWAT 		128

typedef struct _fk_buf {
	int len;
	int low;//valid begin, init: 0, range: [0-->len], <= high
	int high;//free begin, init: 0, range: [0-->len], <= len
	char data[];
} fk_buf;

fk_buf *fk_buf_create();
void fk_buf_destroy(fk_buf *buf);
void fk_buf_print(fk_buf *buf);
void fk_buf_adjust(fk_buf *buf);
//fk_buf *FK_BUF_STRETCH(fk_buf *buf);
//fk_buf *FK_BUF_SHRINK(fk_buf *buf);
//int FK_BUF_SHIFT(fk_buf *buf);

#define FK_BUF_RAW(buf) 		((buf)->data)

#define FK_BUF_LOW(buf)		((buf)->low)

#define FK_BUF_HIGH(buf)		((buf)->high)

#define FK_BUF_CHAR_PTR(buf, idx) ((buf)->data + (idx))

#define FK_BUF_CHAR(buf, idx)	(((buf)->data)[(idx)])

#define FK_BUF_TOTAL_LEN(buf)	(buf)->len

#define FK_BUF_FREE_LEN(buf) 	((buf)->len - (buf)->high)

#define FK_BUF_VALID_LEN(buf)	((buf)->high - (buf)->low)

#define FK_BUF_FREE_START(buf) 	((buf)->data + (buf)->high)

#define FK_BUF_VALID_START(buf) ((buf)->data + (buf)->low)

#define FK_BUF_HIGH_INC(buf, offset)	{	\
	(buf)->high += (offset);				\
	if ((buf)->high > (buf)->len) {			\
		(buf)->high = (buf)->len;			\
	}										\
}

#define FK_BUF_LOW_INC(buf, offset)		{	\
   	(buf)->low += (offset);					\
	if ((buf)->low > (buf)->high) {			\
		(buf)->low = (buf)->high;			\
	}										\
}

#define FK_BUF_STRETCH(buf)		{										\
	if ((buf)->len < FK_BUF_HIGHWAT) {									\
		(buf) = fk_mem_realloc((buf), sizeof(fk_buf) + (buf)->len * 2);	\
		(buf)->len *= 2;												\
	}																	\
}

#define FK_BUF_SHIFT(buf) 	{			\
	if ((buf)->low > 0) {				\
		memmove((buf)->data, 			\
			(buf)->data + (buf)->low, 	\
			(buf)->high - (buf)->low	\
		);								\
		(buf)->high -= (buf)->low;		\
		(buf)->low = 0;					\
	}									\
}

#define FK_BUF_SHRINK(buf)	{							\
	if ((buf)->len >= FK_BUF_HIGHWAT					\
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

#endif
