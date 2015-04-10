#ifndef _FK_STR_H_
#define _FK_STR_H_

typedef struct _fk_str {
	int len;//full length, including '\0'
	char data[];
} fk_str;

#define FK_STR_RAW(str) ((str)->data)

#define FK_STR_LEN(str) ((str)->len)

#define FK_STR_PRINT(str) printf("%s\n", (str)->data)

fk_str *fk_str_create(char *data, int len);

void fk_str_destroy(fk_str *src);

fk_str *fk_str_clone(fk_str *src);

int fk_str_cmp(fk_str *s1, fk_str *s2);

int fk_str_is_digit(fk_str *str);

void fk_str_2upper(fk_str *str);

#endif
