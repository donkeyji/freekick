#include <stdio.h>

#include <fk_buf.h>
#include <fk_mem.h>
#include <fk_log.h>

fk_buf *fk_buf_create()
{
	fk_buf *buf;

	buf = (fk_buf *)fk_mem_alloc(sizeof(fk_buf) + FK_BUF_INIT_LEN);
	buf->len = FK_BUF_INIT_LEN;
	buf->low = 0;
	buf->high = 0;
	return buf;
}

void fk_buf_destroy(fk_buf *buf)
{
	fk_mem_free(buf);
}

void fk_buf_print(fk_buf *buf)
{
	int len;
	char ps[4096];

	len = fk_buf_valid_len(buf);
	memcpy(ps, fk_buf_valid_start(buf), len);
	ps[len] = '\0';
	printf("%s\n", ps);
}

void fk_buf_adjust(fk_buf *buf)
{
	//need to shitf left: the buf is full
	if (buf->len == buf->high 
		&& buf->low > 0) 
	{
		memmove(buf->data, 
			buf->data + buf->low, 
			buf->high - buf->low
		);
		buf->high -= buf->low;
		buf->low = 0;
		fk_log_debug("shift left\n");
	}																				

	//need to stretch
	if (buf->low == 0 
		&& buf->high == buf->len 
		&& buf->len < FK_BUF_HIGHWAT) 
	{
		fk_log_debug("stretch\n");
		fk_mem_realloc(buf, sizeof(fk_buf) + buf->len * 2);
		buf->len *= 2;
	}

	//need to shrink
	if (buf->len >= FK_BUF_HIGHWAT
		&& buf->high - buf->low < FK_BUF_INIT_LEN) 
	{
		memmove(buf->data, 
			buf->data + buf->low, 
			buf->high - buf->low
		);
		buf->high -= buf->low;
		buf->low = 0;
		buf = fk_mem_realloc(buf, sizeof(fk_buf) + FK_BUF_INIT_LEN);
		buf->len = FK_BUF_INIT_LEN;
		fk_log_debug("shrink\n");
	}
}
