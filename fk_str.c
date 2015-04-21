#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fk_str.h>
#include <fk_mem.h>

// copy memory from src
// len: not including '\0'
fk_str *fk_str_create(char *src, int len)
{
	fk_str *new_one;

	new_one = (fk_str *)fk_mem_alloc(sizeof(fk_str) + len + 1);
	memcpy((void *)new_one->data, (void *)src, len);
	new_one->data[len] = '\0';//append '\0' to the tail
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
	memcpy(new_one->data, old_one->data, new_one->len);
	//new_one->data[new_one->len] = '\0';
	return new_one;
}

int fk_str_cmp(fk_str *s1, fk_str *s2)
{
	int cmp;
	if (s1->len != s2->len) {
		return s1->len - s2->len;
	}
	cmp = strcmp(s1->data, s2->data);
	return cmp;
}

int fk_str_is_digit(fk_str *str)
{
	int i;
	char c;

	if (str->len < 2) {
		return 0;
	}

	for (i = 0; i < str->len - 1; i++) {
		c = str->data[i];
		if (i == 0) {
			if (c != '-' && c != '+' && (c <= '0' || c > '9')) {
				return 0;
			} else {
				if (c == '-' || c == '+') {
					if (str->len <= 2) {
						return 0;
					}
				}
			}
		} else {
			if (c < '0' || c > '9') {
				return 0;
			}
		}
	}
	return 1;
}

void fk_str_2upper(fk_str *str)
{
	int i;
	char c;

	for (i = 0; i < str->len - 1; i++) {
		c = str->data[i];
		if (c >= 'a' && c <= 'z') {
			str->data[i] -= 32;
		}
	}
}
