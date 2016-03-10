#include <assert.h>
#include <stdio.h>

#include <fk_buf.h>
#include <fk_mem.h>

fk_buf_t *
fk_buf_create(size_t init_len, size_t highwat)
{
    fk_buf_t *buf;

    assert(highwat >= init_len);
    buf = (fk_buf_t *)fk_mem_alloc(sizeof(fk_buf_t) + init_len);
    buf->highwat = highwat;
    buf->init_len = init_len;
    buf->len = init_len;
    buf->low = 0;
    buf->high = 0;
    return buf;
}

void
fk_buf_destroy(fk_buf_t *buf)
{
    fk_mem_free(buf);
}

#ifdef FK_DEBUG
void
fk_buf_print(const fk_buf_t *buf)
{
    size_t len;
    char ps[4096];

    len = fk_buf_payload_len(buf);
    memcpy(ps, fk_buf_payload_start(buf), len);
    ps[len] = '\0';
    printf("%s\n", ps);
}
#endif
