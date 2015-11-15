#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <unistd.h>

#include <fk_log.h>
#include <fk_mem.h>
#include <freekick.h>

typedef struct _fk_zline {
	char *line;
	size_t len;
} fk_zline;

static fk_zline *fk_zline_create(size_t len);
static void fk_zline_adjust(fk_zline *buf, size_t len);
static void fk_zline_destroy(fk_zline *buf);

/* related to dump */
static int fk_svr_db_dump(FILE *fp, unsigned db_idx);
static int fk_svr_db_str_elt_dump(FILE *fp, fk_elt *elt);
static int fk_svr_db_list_elt_dump(FILE *fp, fk_elt *elt);
static int fk_svr_db_dict_elt_dump(FILE *fp, fk_elt *elt);

/* related to restore */
static int fk_svr_db_restore(FILE *fp, fk_zline *buf);
static int fk_svr_db_str_elt_restore(FILE *fp, fk_dict *db, fk_zline *buf);
static int fk_svr_db_list_elt_restore(FILE *fp, fk_dict *db, fk_zline *buf);
static int fk_svr_db_dict_elt_restore(FILE *fp, fk_dict *db, fk_zline *buf);

void fk_svr_db_save_background()
{
	int rt;

	if (setting.dump != 1) {
		return;
	}

	if (server.save_done != 1) {
		return;
	}

	fk_log_debug("to save db\n");
	rt = fork();
	if (rt < 0) {/* should not exit here, just return */
		fk_log_error("fork: %s\n", strerror(errno));
		return;
	} else if (rt > 0) {/* mark the save_done */
		server.save_done = 0;
		server.save_pid = rt;/* save the child process ID */
		return;
	} else {
		/* execute only in child process */
		if (fk_svr_db_save() == FK_ERR) {
			fk_log_error("db save failed\n");
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
	}
}

int fk_svr_db_save()
{
	int rt;
	FILE *fp;
	unsigned i;
	char *temp_db;

	temp_db = "freekick-temp.db";

	/* step 1: write to a temporary file */
	fp = fopen(temp_db, "w+");
	if (fp == NULL) {
		return FK_ERR;
	}

	for (i = 0; i < server.dbcnt; i++) {
		rt = fk_svr_db_dump(fp, i);
		if (rt == FK_ERR) {
			fclose(fp);
			remove(temp_db);/* remove this temporary db file */
			return FK_ERR;
		}
	}
	fclose(fp);/* close before rename */

	/* step 2: rename temporary file to server.db_file */
	rt = rename(temp_db, fk_str_raw(server.db_file));
	if (rt < 0) {
		remove(temp_db);/* remove this temporary db file */
		return FK_ERR;
	}

	return FK_OK;
}

/*
 * to do: 
 * error handling
 */
int fk_svr_db_dump(FILE *fp, unsigned db_idx)
{
	fk_elt *elt;
	int type, rt;
	fk_dict *dct;
	size_t len, wz;
	fk_dict_iter *iter;

	dct = server.db[db_idx];
	if (fk_dict_len(dct) == 0) {
		return FK_OK;
	}

	/* I do not think it's necessary to call htonl() */
	wz = fwrite(&db_idx, sizeof(db_idx), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}

	/* dump the len of the dict */
	len = fk_dict_len(dct);
	wz = fwrite(&len, sizeof(len), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}

	/* dict body */
	iter = fk_dict_iter_begin(dct);
	while ((elt = fk_dict_iter_next(iter)) != NULL) {
		type = fk_item_type(((fk_item *)fk_elt_value(elt)));
		wz = fwrite(&type, sizeof(type), 1, fp);
		if (wz == 0) {
			fk_dict_iter_end(iter);/* need to release iterator */
			return FK_ERR;
		}
		switch (type) {
		case FK_ITEM_STR:
			rt = fk_svr_db_str_elt_dump(fp, elt);
			break;
		case FK_ITEM_LIST:
			rt = fk_svr_db_list_elt_dump(fp, elt);
			break;
		case FK_ITEM_DICT:
			rt = fk_svr_db_dict_elt_dump(fp, elt);
			break;
		}
		if (rt == FK_ERR) {
			fk_dict_iter_end(iter);/* need to release iterator */
			return FK_ERR;
		}
	}
	fk_dict_iter_end(iter);/* must release this iterator of fk_dict */

	return FK_OK;
}

int fk_svr_db_str_elt_dump(FILE *fp, fk_elt *elt)
{
	size_t len, wz;
	fk_str *key, *value;
	fk_item *kitm, *vitm;

	kitm = (fk_item *)fk_elt_key(elt);
	vitm = (fk_item *)fk_elt_value(elt);

	key = (fk_str *)(fk_item_raw(kitm));
	value = (fk_str *)(fk_item_raw(vitm));

	/* key dump */
	len = fk_str_len(key);
	wz = fwrite(&len, sizeof(len), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}
	wz = fwrite(fk_str_raw(key), fk_str_len(key), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}

	/* value dump */
	len = fk_str_len(value);
	wz = fwrite(&len, sizeof(len), 1, fp);
	/* 
	 * when len == 0 ==> wz == 0
	 * when len > 0  ==> wz > 0
	 */
	if (wz == 0) {
		return FK_ERR;
	}
	wz = fwrite(fk_str_raw(value), fk_str_len(value), 1, fp);
	if (len > 0 && wz == 0) {
		return FK_ERR;
	}

	return FK_OK;
}

int fk_svr_db_list_elt_dump(FILE *fp, fk_elt *elt)
{
	fk_node *nd;
	fk_list *lst;
	size_t len, wz;
	fk_str *key, *vs;
	fk_list_iter *iter;
	fk_item *kitm, *vitm, *nitm;

	kitm = (fk_item *)fk_elt_key(elt);
	vitm = (fk_item *)fk_elt_value(elt);

	key = (fk_str *)(fk_item_raw(kitm));
	lst = (fk_list *)(fk_item_raw(vitm));

	/* key dump */
	len = fk_str_len(key);
	wz = fwrite(&len, sizeof(len), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}
	wz = fwrite(fk_str_raw(key), fk_str_len(key), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}

	/* size dump */
	len = fk_list_len(lst);
	wz = fwrite(&len, sizeof(len), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}

	/* value dump */
	iter = fk_list_iter_begin(lst, FK_LIST_ITER_H2T);
	while ((nd = fk_list_iter_next(iter)) != NULL) {
		nitm = (fk_item *)(fk_node_raw(nd));
		vs = (fk_str *)(fk_item_raw(nitm));
		len = fk_str_len(vs);
		wz = fwrite(&len, sizeof(len), 1, fp);
		if (wz == 0) {
			fk_list_iter_end(iter);
			return FK_ERR;
		}

		wz = fwrite(fk_str_raw(vs), fk_str_len(vs), 1, fp);
		if (len > 0 && wz == 0) {
			fk_list_iter_end(iter);
			return FK_ERR;
		}
	}
	fk_list_iter_end(iter);/* must release this iterator of fk_list */

	return FK_OK;
}

int fk_svr_db_dict_elt_dump(FILE *fp, fk_elt *elt)
{
	fk_elt *selt;
	fk_dict *dct;
	size_t len, wz;
	fk_dict_iter *iter;
	fk_str *key, *skey, *svs;
	fk_item *kitm, *vitm, *skitm, *svitm;

	kitm = (fk_item *)fk_elt_key(elt);
	vitm = (fk_item *)fk_elt_value(elt);

	key = (fk_str *)(fk_item_raw(kitm));
	dct = (fk_dict *)(fk_item_raw(vitm));

	/* key dump */
	len = fk_str_len(key);
	wz = fwrite(&len, sizeof(len), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}
	wz = fwrite(fk_str_raw(key), fk_str_len(key), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}

	/* size dump */
	len = fk_dict_len(dct);
	wz = fwrite(&len, sizeof(len), 1, fp);
	if (wz == 0) {
		return FK_ERR;
	}

	iter = fk_dict_iter_begin(dct);
	while ((selt = fk_dict_iter_next(iter)) != NULL) {
		skitm = (fk_item *)fk_elt_key(selt);
		svitm = (fk_item *)fk_elt_value(selt);

		skey = (fk_str *)(fk_item_raw(skitm));
		svs = (fk_str *)(fk_item_raw(svitm));

		len = fk_str_len(skey);
		wz = fwrite(&len, sizeof(len), 1, fp);
		if (wz == 0) {
			fk_dict_iter_end(iter);
			return FK_ERR;
		}
		wz = fwrite(fk_str_raw(skey), fk_str_len(skey), 1, fp);
		if (wz == 0) {
			fk_dict_iter_end(iter);
			return FK_ERR;
		}

		len = fk_str_len(svs);
		wz = fwrite(&len, sizeof(len), 1, fp);
		if (wz == 0) {
			fk_dict_iter_end(iter);
			return FK_ERR;
		}
		wz = fwrite(fk_str_raw(svs), fk_str_len(svs), 1, fp);
		if (len > 0 && wz == 0) {
			fk_dict_iter_end(iter);
			return FK_ERR;
		}
	}
	fk_dict_iter_end(iter);/* must release this iterator of fk_dict */

	return FK_OK;
}

fk_zline *fk_zline_create(size_t len)
{
	fk_zline *buf; 
	buf = fk_mem_alloc(sizeof(fk_zline));
	buf->len = len;
	buf->line = (char *)fk_mem_alloc(buf->len);

	return buf;
}

void fk_zline_adjust(fk_zline *buf, size_t len)
{
	if (buf->len < len) {
		buf->len = fk_util_min_power(len);
		buf->line = (char *)fk_mem_realloc(buf->line, buf->len);
	}
}

void fk_zline_destroy(fk_zline *buf)
{
	fk_mem_free(buf->line);
	fk_mem_free(buf);
}

void fk_svr_db_load(fk_str *db_file)
{
	int rt;
	FILE *fp; 
	long tail;
	fk_zline *buf;

	if (setting.dump != 1) {
		return;
	}

	buf = fk_zline_create(4096);
	fp = fopen(fk_str_raw(db_file), "r");
	if (fp == NULL) {/* db not exist */
		return;
	}
	fseek(fp, 0, SEEK_END);/* move to the tail */
	tail = ftell(fp);/* record the position of the tail */
	fseek(fp, 0, SEEK_SET);/* rewind to the head */

	while (ftell(fp) != tail) {
		rt = fk_svr_db_restore(fp, buf);
		if (rt == FK_ERR) {
			fk_log_error("load db body failed\n");
			exit(EXIT_FAILURE);
		}
	}
	fk_zline_destroy(buf);/* must free this block */
	fclose(fp);

	return;
}

/* 
 * return value: -1
 * 1. error occurs when reading 
 * 2. reaching to the end of file 
 */
int fk_svr_db_restore(FILE *fp, fk_zline *buf)
{
	int rt;
	fk_dict *db;
	size_t cnt, i, rz;
	unsigned type, idx;

	/* 
	 * to do:
	 * check the range of "idx"
	 */
	/* restore the index */
	rz = fread(&idx, sizeof(idx), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	if (idx >= server.dbcnt) {
		return FK_ERR;
	}
	db = server.db[idx];

	/* restore len of dictionary */
	rz = fread(&cnt, sizeof(cnt), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}

	/* load all the elements */
	for (i = 0; i < cnt; i++) {
		rz = fread(&type, sizeof(type), 1, fp);
		if (rz == 0) {
			return FK_ERR;
		}

		switch (type) {
		case FK_ITEM_STR:
			rt = fk_svr_db_str_elt_restore(fp, db, buf);
			break;
		case FK_ITEM_LIST:
			rt = fk_svr_db_list_elt_restore(fp, db, buf);
			break;
		case FK_ITEM_DICT:
			rt = fk_svr_db_dict_elt_restore(fp, db, buf);
			break;
		default:
			rt = -1;
			break;
		}
		if (rt == FK_ERR) {
			return FK_ERR;
		}
	}
	return FK_OK;
}

/* 
 * return value: -1
 * 1. error occurs when reading 
 * 2. reaching to the end of file 
 */
int fk_svr_db_str_elt_restore(FILE *fp, fk_dict *db, fk_zline *buf)
{
	fk_str *key, *value;
	fk_item *kitm, *vitm;
	size_t klen, vlen, rz;

	rz = fread(&klen, sizeof(klen), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	if (klen > FK_STR_HIGHWAT) {
		return FK_ERR;
	}

	fk_zline_adjust(buf, klen);
	rz = fread(buf->line, klen, 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	key = fk_str_create(buf->line, klen);
	kitm = fk_item_create(FK_ITEM_STR, key);

	rz = fread(&vlen, sizeof(vlen), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	if (vlen > FK_STR_HIGHWAT) {
		return FK_ERR;
	}
	fk_zline_adjust(buf, vlen);
	rz = fread(buf->line, vlen, 1, fp);
	if (vlen > 0 && rz == 0) {
		return FK_ERR;
	}

	value = fk_str_create(buf->line, vlen);
	vitm = fk_item_create(FK_ITEM_STR, value);

	fk_dict_add(db, kitm, vitm);

	return FK_OK;
}

/* 
 * return value: -1
 * 1. error occurs when reading 
 * 2. reaching to the end of file 
 */
int fk_svr_db_list_elt_restore(FILE *fp, fk_dict *db, fk_zline *buf)
{
	fk_list *lst;
	fk_str *key, *nds;
	fk_item *kitm, *vitm, *nitm;
	size_t klen, llen, i, nlen, rz;

	rz = fread(&klen, sizeof(klen), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	if (klen > FK_STR_HIGHWAT) {
		return FK_ERR;
	}
	fk_zline_adjust(buf, klen);
	rz = fread(buf->line, klen, 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	key = fk_str_create(buf->line, klen);
	kitm = fk_item_create(FK_ITEM_STR, key);

	rz = fread(&llen, sizeof(llen), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}

	lst = fk_list_create(&db_list_op);
	for (i = 0; i < llen; i++) {
		rz = fread(&nlen, sizeof(nlen), 1, fp);
		if (rz == 0) {
			return FK_ERR;
		}
		if (nlen > FK_STR_HIGHWAT) {
			return FK_ERR;
		}
		fk_zline_adjust(buf, nlen);
		rz = fread(buf->line, nlen, 1, fp);
		if (nlen > 0 && rz == 0) {
			return FK_ERR;
		}
		nds = fk_str_create(buf->line, nlen);
		nitm = fk_item_create(FK_ITEM_STR, nds);
		fk_list_tail_insert(lst, nitm);
	}
	vitm = fk_item_create(FK_ITEM_LIST, lst);
	fk_dict_add(db, kitm, vitm);

	return FK_OK;
}

/* 
 * return value: -1
 * 1. error occurs when reading 
 * 2. reaching to the end of file 
 */
int fk_svr_db_dict_elt_restore(FILE *fp, fk_dict *db, fk_zline *buf)
{
	fk_dict *sdct;
	fk_str *key, *skey, *svalue;
	fk_item *kitm, *skitm, *vitm, *svitm;
	size_t klen, sklen, svlen, dlen, i, rz;

	rz = fread(&klen, sizeof(klen), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	if (klen > FK_STR_HIGHWAT) {
		return FK_ERR;
	}
	fk_zline_adjust(buf, klen);
	rz = fread(buf->line, klen, 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}
	key = fk_str_create(buf->line, klen);
	kitm = fk_item_create(FK_ITEM_STR, key);

	rz = fread(&dlen, sizeof(dlen), 1, fp);
	if (rz == 0) {
		return FK_ERR;
	}

	sdct = fk_dict_create(&db_dict_eop);
	for (i = 0; i < dlen; i++) {
		rz = fread(&sklen, sizeof(sklen), 1, fp);
		if (rz == 0) {
			return FK_ERR;
		}
		if (sklen > FK_STR_HIGHWAT) {
			return FK_ERR;
		}
		fk_zline_adjust(buf, sklen);
		rz = fread(buf->line, sklen, 1, fp);
		if (rz == 0) {
			return FK_ERR;
		}
		skey = fk_str_create(buf->line, sklen);
		skitm = fk_item_create(FK_ITEM_STR, skey);

		rz = fread(&svlen, sizeof(svlen), 1, fp);
		if (rz == 0) {
			return FK_ERR;
		}
		if (svlen > FK_STR_HIGHWAT) {
			return FK_ERR;
		}
		fk_zline_adjust(buf, svlen);
		rz = fread(buf->line, svlen, 1, fp);
		if (svlen > 0 && rz == 0) {
			return FK_ERR;
		}
		svalue = fk_str_create(buf->line, svlen);
		svitm = fk_item_create(FK_ITEM_STR, svalue);

		fk_dict_add(sdct, skitm, svitm);
	}
	vitm = fk_item_create(FK_ITEM_DICT, sdct);
	fk_dict_add(db, kitm, vitm);

	return FK_OK;
}
