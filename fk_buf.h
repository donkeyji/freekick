#ifndef _FK_BUF_H
#define _FK_BUF_H

#include <string.h>

#define FK_BUF_LEN 			32
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

fk_buf *fk_buf_stretch(fk_buf *buf);

fk_buf *fk_buf_shrink(fk_buf *buf);

int fk_buf_shift(fk_buf *buf);

#define FK_BUF_IS_FULL(buf) 	((buf)->high == (buf)->len ? 1 : 0)

#define FK_BUF_IS_EMPTY(buf)	((buf)->low == (buf)->high ? 1 : 0)

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

#define FK_BUF_DATA(buf) ((buf)->data)

#define FK_BUF_FREE_LEN(buf) 	((buf)->len - (buf)->high)

#define FK_BUF_VALID_LEN(buf)	((buf)->high - (buf)->low)

#define FK_BUF_FREE_START(buf) 	((buf)->data + (buf)->high)

#define FK_BUF_VALID_START(buf) ((buf)->data + (buf)->low)

#endif
