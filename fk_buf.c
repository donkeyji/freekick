#include <assert.h>
#include <stdio.h>

#include <fk_buf.h>
#include <fk_mem.h>

fk_buf *fk_buf_create(size_t highwat)
{
	fk_buf *buf;

	assert(highwat >= FK_BUF_INIT_LEN);
	buf = (fk_buf *)fk_mem_alloc(sizeof(fk_buf) + FK_BUF_INIT_LEN);
	buf->highwat = highwat;
	buf->len = FK_BUF_INIT_LEN;
	buf->low = 0;
	buf->high = 0;
	return buf;
}

void fk_buf_destroy(fk_buf *buf)
{
	fk_mem_free(buf);
}

#ifdef FK_DEBUG
void fk_buf_print(const fk_buf *buf)
{
	size_t len;
	char ps[4096];

	len = fk_buf_payload_len(buf);
	memcpy(ps, fk_buf_payload_start(buf), len);
	ps[len] = '\0';
	printf("%s\n", ps);
}
#endif
