#ifndef _FK_BUF_H_
#define _FK_BUF_H_

#include <string.h>

#include <fk_macro.h>
#include <fk_util.h>

#define FK_BUF_INIT_LEN 	16

typedef struct _fk_buf {
	size_t high_wat;
	size_t len;
	size_t low;/* valid begin, init: 0, range: [0-->len], <= high */
	size_t high;/* free begin, init: 0, range: [0-->len], <= len */
	char buffer[];
} fk_buf;

fk_buf *fk_buf_create(size_t high_wat);
void fk_buf_destroy(fk_buf *buf);
#ifdef FK_DEBUG
void fk_buf_print(const fk_buf *buf);
#endif

#define fk_buf_len(buf)				((buf)->len)

#define fk_buf_low(buf)				((buf)->low)

#define fk_buf_high(buf)			((buf)->high)

#define fk_buf_high_wat(buf)		((buf)->high_wat)

#define fk_buf_free_len(buf) 		((buf)->len - (buf)->high)

#define fk_buf_payload_len(buf)		((buf)->high - (buf)->low)

#define fk_buf_free_start(buf) 		((buf)->buffer + (buf)->high)

#define fk_buf_payload_start(buf) 	((buf)->buffer + (buf)->low)

#define fk_buf_reach_highwat(buf)	((buf)->len == (buf)->high_wat)

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

#define fk_buf_stretch(buf)		{				\
	if ((buf)->len < (buf)->high_wat) {			\
		(buf)->len <<= 1;						\
		(buf) = (fk_buf *)fk_mem_realloc((buf),	\
				sizeof(fk_buf) + (buf)->len);	\
	}											\
}

#define fk_buf_shift(buf) 	{			\
	if ((buf)->low > 0) {				\
		memmove((buf)->buffer, 			\
			(buf)->buffer + (buf)->low,	\
			(buf)->high - (buf)->low	\
		);								\
		(buf)->high -= (buf)->low;		\
		(buf)->low = 0;					\
	}									\
}

#define fk_buf_shrink(buf)	{							\
	if ((buf)->len >= FK_BUF_INIT_LEN &&				\
		fk_buf_payload_len((buf)) < FK_BUF_INIT_LEN)	\
	{													\
		fk_buf_shift((buf));							\
		(buf) = (fk_buf *)fk_mem_realloc((buf), 		\
				sizeof(fk_buf) + FK_BUF_INIT_LEN		\
		);												\
		(buf)->len = FK_BUF_INIT_LEN;					\
	}													\
}

#define fk_buf_adjust(buf, length)	{					\
	if (fk_buf_free_len((buf)) < (length)) {			\
		fk_buf_shift((buf));							\
	}													\
	if (fk_buf_free_len((buf)) < (length) &&			\
		(buf)->len < (buf)->high_wat)					\
	{													\
		(buf)->len = fk_util_min_power((buf)->len 		\
			+ (length) - fk_buf_free_len((buf)));		\
		(buf) = (fk_buf *)fk_mem_realloc((buf), 		\
				sizeof(fk_buf) + (buf)->len);			\
	}													\
}

#endif
