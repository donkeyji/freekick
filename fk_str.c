#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fk_util.h>
#include <fk_str.h>
#include <fk_mem.h>

/*
 * copy memory from src 
 * len: not including '\0'
 */
fk_str_t *fk_str_create(const char *src, size_t len)
{
	fk_str_t *new_one;

	new_one = (fk_str_t *)fk_mem_alloc(sizeof(fk_str_t) + len + 1);
	memcpy((void *)new_one->seq, (void *)src, len);
	new_one->seq[len] = '\0';/* append '\0' to the tail */
	new_one->len = len;
	return new_one;
}

void fk_str_destroy(fk_str_t *src)
{
	fk_mem_free(src);
}

fk_str_t *fk_str_clone(const fk_str_t *old_one)
{
	fk_str_t *new_one;

	new_one = (fk_str_t *)fk_mem_alloc(sizeof(fk_str_t) + old_one->len + 1);
	new_one->len = old_one->len;
	memcpy(new_one->seq, old_one->seq, new_one->len + 1);
	//new_one->seq[new_one->len] = '\0';
	return new_one;
}

int fk_str_cmp(const fk_str_t *s1, const fk_str_t *s2)
{
	int cmp;
	if (s1->len != s2->len) {
		return s1->len - s2->len;
	}
	//cmp = strcmp(s1->seq, s2->seq);
	cmp = memcmp(s1->seq, s2->seq, s1->len);
	return cmp;
}

int fk_str_is_positive(const fk_str_t *str)
{
	int rt;

	if (str->len < 1) {
		return 0;
	}

	rt = fk_util_is_positive_seq(str->seq, str->len);
	if (rt == 1) {
		return 1;
	} else {
		return 0;
	}
}

int fk_str_is_nonminus(const fk_str_t *str)
{
	int rt;

	if (str->len < 1) {
		return 0;
	}

	rt = fk_util_is_nonminus_seq(str->seq, str->len);
	if (rt == 1) {
		return 1;
	} else {
		return 0;
	}
}

int fk_str_is_digit(const fk_str_t *str)
{
	int rt;

	if (str->len < 1) {
		return 0;
	}

	rt = fk_util_is_digit_seq(str->seq, str->len);
	if (rt == 1) {
		return 1;
	} else {
		return 0;
	}
}

void fk_str_2upper(fk_str_t *str)
{
	char c;
	size_t i;

	for (i = 0; i < str->len; i++) {
		c = str->seq[i];
		if (c >= 'a' && c <= 'z') {
			str->seq[i] -= 32;
		}
	}
}

void fk_str_2lower(fk_str_t *str)
{
	char c;
	size_t i;

	for (i = 0; i < str->len; i++) {
		c = str->seq[i];
		if (c >= 'A' && c <= 'Z') {
			str->seq[i] += 32;
		}
	}
}

uint32_t fk_str_hash(const fk_str_t *str)
{
	const char *buf;
	size_t len;
    uint32_t hash;

	hash = 5381;
	buf = fk_str_raw(str);
	len = fk_str_len(str);
    while (len--) {
		hash = ((hash << 5) + hash) + (*buf++);
	}
    return hash;
}

