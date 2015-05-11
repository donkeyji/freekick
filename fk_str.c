#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fk_util.h>
#include <fk_str.h>
#include <fk_mem.h>

// copy memory from src
// len: not including '\0'
fk_str *fk_str_create(char *src, size_t len)
{
	fk_str *new_one;

	new_one = (fk_str *)fk_mem_alloc(sizeof(fk_str) + len + 1);
	memcpy((void *)new_one->seq, (void *)src, len);
	new_one->seq[len] = '\0';//append '\0' to the tail
	new_one->len = len + 1;
	return new_one;
}

void fk_str_destroy(fk_str *src)
{
	fk_mem_free(src);
}

fk_str *fk_str_clone(fk_str *old_one)
{
	fk_str *new_one;

	new_one = (fk_str *)fk_mem_alloc(sizeof(fk_str) + old_one->len);
	new_one->len = old_one->len;
	memcpy(new_one->seq, old_one->seq, new_one->len);
	//new_one->seq[new_one->len] = '\0';
	return new_one;
}

int fk_str_cmp(fk_str *s1, fk_str *s2)
{
	int cmp;
	if (s1->len != s2->len) {
		return s1->len - s2->len;
	}
	//cmp = strcmp(s1->seq, s2->seq);
	cmp = memcmp(s1->seq, s2->seq, s1->len);
	return cmp;
}

int fk_str_is_positive(fk_str *str)
{
	int rt;

	if (str->len < 2) {
		return 0;
	}

	rt = fk_util_is_positive_seq(str->seq, str->len - 1);
	if (rt == 1) {
		return 1;
	} else {
		return 0;
	}
}

int fk_str_is_nonminus(fk_str *str)
{
	int rt;

	if (str->len < 2) {
		return 0;
	}

	rt = fk_util_is_nonminus_seq(str->seq, str->len - 1);
	if (rt == 1) {
		return 1;
	} else {
		return 0;
	}
}

int fk_str_is_digit(fk_str *str)
{
	int rt;

	if (str->len < 2) {
		return 0;
	}

	rt = fk_util_is_digit_seq(str->seq, str->len - 1);
	if (rt == 1) {
		return 1;
	} else {
		return 0;
	}
}

void fk_str_2upper(fk_str *str)
{
	int i;
	char c;

	for (i = 0; i < str->len - 1; i++) {
		c = str->seq[i];
		if (c >= 'a' && c <= 'z') {
			str->seq[i] -= 32;
		}
	}
}

void fk_str_2lower(fk_str *str)
{
	int i;
	char c;

	for (i = 0; i < str->len - 1; i++) {
		c = str->seq[i];
		if (c >= 'A' && c <= 'Z') {
			str->seq[i] += 32;
		}
	}
}
