#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <unistd.h>

#include <fk_conf.h>
#include <fk_log.h>
#include <fk_mem.h>
#include <fk_item.h>
#include <fk_svr.h>

typedef struct {
    char *line;
    size_t len;
} fk_zline_t;

static fk_zline_t *fk_zline_create(size_t len);
static void fk_zline_alloc(fk_zline_t *buf, size_t len);
static void fk_zline_destroy(fk_zline_t *buf);

/* related to dump */
static int fk_fkdb_dump(FILE *fp, uint32_t db_idx);
static int fk_fkdb_dump_str_elt(FILE *fp, fk_elt_t *elt);
static int fk_fkdb_dump_list_elt(FILE *fp, fk_elt_t *elt);
static int fk_fkdb_dump_dict_elt(FILE *fp, fk_elt_t *elt);

/* related to restore */
static int fk_fkdb_restore(FILE *fp, fk_zline_t *buf);
static int fk_fkdb_restore_str_elt(FILE *fp, fk_dict_t *db, fk_zline_t *buf);
static int fk_fkdb_restore_list_elt(FILE *fp, fk_dict_t *db, fk_zline_t *buf);
static int fk_fkdb_restore_dict_elt(FILE *fp, fk_dict_t *db, fk_zline_t *buf);

void
fk_fkdb_init(void)
{
}

void
fk_fkdb_bgsave(void)
{
    int rt;

    if (setting.dump != 1) {
        return;
    }

    if (server.save_pid != -1) {
        return;
    }

    fk_log_debug("to save db\n");
    rt = fork();
    if (rt < 0) {/* should not exit here, just return */
        fk_log_error("fork: %s\n", strerror(errno));
        return;
    } else if (rt > 0) {/* mark the save_pid */
        server.save_pid = rt;/* save the child process ID */
        return;
    } else {
        /* execute only in child process */
        if (fk_fkdb_save() == FK_SVR_ERR) {
            fk_log_error("db save failed\n");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
}

int
fk_fkdb_save(void)
{
    int rt;
    FILE *fp;
    uint32_t i;
    char *temp_db;

    temp_db = "freekick-temp.db";

    /* step 1: write to a temporary file */
    fp = fopen(temp_db, "w+");
    if (fp == NULL) {
        return FK_SVR_ERR;
    }

    for (i = 0; i < server.dbcnt; i++) {
        rt = fk_fkdb_dump(fp, i);
        if (rt == FK_SVR_ERR) {
            fclose(fp);
            remove(temp_db);/* remove this temporary db file */
            return FK_SVR_ERR;
        }
    }
    fclose(fp);/* close before rename */

    /* step 2: rename temporary file to server.db_file */
    rt = rename(temp_db, fk_str_raw(setting.db_file));
    if (rt < 0) {
        remove(temp_db);/* remove this temporary db file */
        return FK_SVR_ERR;
    }

    return FK_SVR_OK;
}

/*
 * to do:
 * error handling
 */
int
fk_fkdb_dump(FILE *fp, uint32_t db_idx)
{
    fk_elt_t *elt;
    int type, rt;
    fk_dict_t *dct;
    size_t len, wz;
    fk_dict_iter_t *iter;

    dct = server.db[db_idx];
    if (fk_dict_len(dct) == 0) {
        return FK_SVR_OK;
    }

    /* I do not think it's necessary to call htonl() */
    wz = fwrite(&db_idx, sizeof(db_idx), 1, fp);
    if (wz == 0) {
        return FK_SVR_ERR;
    }

    /* dump the len of the dict */
    len = fk_dict_len(dct);
    wz = fwrite(&len, sizeof(len), 1, fp);
    if (wz == 0) {
        return FK_SVR_ERR;
    }

    /* dict body */
    iter = fk_dict_iter_begin(dct);
    while ((elt = fk_dict_iter_next(iter)) != NULL) {
        type = fk_item_type(((fk_item_t *)fk_elt_value(elt)));
        wz = fwrite(&type, sizeof(type), 1, fp);
        if (wz == 0) {
            fk_dict_iter_end(iter);/* need to release iterator */
            return FK_SVR_ERR;
        }
        switch (type) {
        case FK_ITEM_STR:
            rt = fk_fkdb_dump_str_elt(fp, elt);
            break;
        case FK_ITEM_LIST:
            rt = fk_fkdb_dump_list_elt(fp, elt);
            break;
        case FK_ITEM_DICT:
            rt = fk_fkdb_dump_dict_elt(fp, elt);
            break;
        }
        if (rt == FK_SVR_ERR) {
            fk_dict_iter_end(iter);/* need to release iterator */
            return FK_SVR_ERR;
        }
    }
    fk_dict_iter_end(iter);/* must release this iterator of fk_dict_t */

    return FK_SVR_OK;
}

int
fk_fkdb_dump_str_elt(FILE *fp, fk_elt_t *elt)
{
    size_t len, wz;
    fk_str_t *key, *value;
    fk_item_t *kitm, *vitm;

    kitm = (fk_item_t *)fk_elt_key(elt);
    vitm = (fk_item_t *)fk_elt_value(elt);

    key = (fk_str_t *)(fk_item_raw(kitm));
    value = (fk_str_t *)(fk_item_raw(vitm));

    /* key dump */
    len = fk_str_len(key);
    wz = fwrite(&len, sizeof(len), 1, fp);
    if (wz == 0) {
        return FK_SVR_ERR;
    }
    wz = fwrite(fk_str_raw(key), fk_str_len(key), 1, fp);
    if (wz == 0) {
        return FK_SVR_ERR;
    }

    /* value dump */
    len = fk_str_len(value);
    wz = fwrite(&len, sizeof(len), 1, fp);
    /*
     * when len == 0 ==> wz == 0
     * when len > 0  ==> wz > 0
     */
    if (wz == 0) {
        return FK_SVR_ERR;
    }
    wz = fwrite(fk_str_raw(value), fk_str_len(value), 1, fp);
    if (len > 0 && wz == 0) {
        return FK_SVR_ERR;
    }

    return FK_SVR_OK;
}

int
fk_fkdb_dump_list_elt(FILE *fp, fk_elt_t *elt)
{
    fk_node_t *nd;
    fk_list_t *lst;
    size_t len, wz;
    fk_str_t *key, *vs;
    fk_list_iter_t *iter;
    fk_item_t *kitm, *vitm, *nitm;

    kitm = (fk_item_t *)fk_elt_key(elt);
    vitm = (fk_item_t *)fk_elt_value(elt);

    key = (fk_str_t *)(fk_item_raw(kitm));
    lst = (fk_list_t *)(fk_item_raw(vitm));

    /* key dump */
    len = fk_str_len(key);
    wz = fwrite(&len, sizeof(len), 1, fp);
    if (wz == 0) {
        return FK_SVR_ERR;
    }
    wz = fwrite(fk_str_raw(key), fk_str_len(key), 1, fp);
    if (wz == 0) {
        return FK_SVR_ERR;
    }

    /* size dump */
    len = fk_list_len(lst);
    wz = fwrite(&len, sizeof(len), 1, fp);
    if (wz == 0) {
        return FK_SVR_ERR;
    }

    /* value dump */
    iter = fk_list_iter_begin(lst, FK_LIST_ITER_H2T);
    while ((nd = fk_list_iter_next(iter)) != NULL) {
        nitm = (fk_item_t *)(fk_node_raw(nd));
        vs = (fk_str_t *)(fk_item_raw(nitm));
        len = fk_str_len(vs);
        wz = fwrite(&len, sizeof(len), 1, fp);
        if (wz == 0) {
            fk_list_iter_end(iter);
            return FK_SVR_ERR;
        }

        wz = fwrite(fk_str_raw(vs), fk_str_len(vs), 1, fp);
        if (len > 0 && wz == 0) {
            fk_list_iter_end(iter);
            return FK_SVR_ERR;
        }
    }
    fk_list_iter_end(iter);/* must release this iterator of fk_list_t */

    return FK_SVR_OK;
}

int
fk_fkdb_dump_dict_elt(FILE *fp, fk_elt_t *elt)
{
    fk_elt_t *selt;
    fk_dict_t *dct;
    size_t len, wz;
    fk_dict_iter_t *iter;
    fk_str_t *key, *skey, *svs;
    fk_item_t *kitm, *vitm, *skitm, *svitm;

    kitm = (fk_item_t *)fk_elt_key(elt);
    vitm = (fk_item_t *)fk_elt_value(elt);

    key = (fk_str_t *)(fk_item_raw(kitm));
    dct = (fk_dict_t *)(fk_item_raw(vitm));

    /* key dump */
    len = fk_str_len(key);
    wz = fwrite(&len, sizeof(len), 1, fp);
    if (wz == 0) {
        return FK_SVR_ERR;
    }
    wz = fwrite(fk_str_raw(key), fk_str_len(key), 1, fp);
    if (wz == 0) {
        return FK_SVR_ERR;
    }

    /* size dump */
    len = fk_dict_len(dct);
    wz = fwrite(&len, sizeof(len), 1, fp);
    if (wz == 0) {
        return FK_SVR_ERR;
    }

    iter = fk_dict_iter_begin(dct);
    while ((selt = fk_dict_iter_next(iter)) != NULL) {
        skitm = (fk_item_t *)fk_elt_key(selt);
        svitm = (fk_item_t *)fk_elt_value(selt);

        skey = (fk_str_t *)(fk_item_raw(skitm));
        svs = (fk_str_t *)(fk_item_raw(svitm));

        len = fk_str_len(skey);
        wz = fwrite(&len, sizeof(len), 1, fp);
        if (wz == 0) {
            fk_dict_iter_end(iter);
            return FK_SVR_ERR;
        }
        wz = fwrite(fk_str_raw(skey), fk_str_len(skey), 1, fp);
        if (wz == 0) {
            fk_dict_iter_end(iter);
            return FK_SVR_ERR;
        }

        len = fk_str_len(svs);
        wz = fwrite(&len, sizeof(len), 1, fp);
        if (wz == 0) {
            fk_dict_iter_end(iter);
            return FK_SVR_ERR;
        }
        wz = fwrite(fk_str_raw(svs), fk_str_len(svs), 1, fp);
        if (len > 0 && wz == 0) {
            fk_dict_iter_end(iter);
            return FK_SVR_ERR;
        }
    }
    fk_dict_iter_end(iter);/* must release this iterator of fk_dict_t */

    return FK_SVR_OK;
}

fk_zline_t *
fk_zline_create(size_t len)
{
    fk_zline_t *buf;
    buf = fk_mem_alloc(sizeof(fk_zline_t));
    buf->len = len;
    buf->line = (char *)fk_mem_alloc(buf->len);

    return buf;
}

void
fk_zline_alloc(fk_zline_t *buf, size_t len)
{
    if (buf->len < len) {
        buf->len = fk_util_min_power(len);
        buf->line = (char *)fk_mem_realloc(buf->line, buf->len);
    }
}

void
fk_zline_destroy(fk_zline_t *buf)
{
    fk_mem_free(buf->line);
    fk_mem_free(buf);
}

void
fk_fkdb_load(fk_str_t *db_file)
{
    int rt;
    FILE *fp;
    long tail;
    fk_zline_t *buf;

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
        rt = fk_fkdb_restore(fp, buf);
        if (rt == FK_SVR_ERR) {
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
int
fk_fkdb_restore(FILE *fp, fk_zline_t *buf)
{
    int rt;
    uint32_t idx;
    unsigned type;
    fk_dict_t *db;
    size_t cnt, i, rz;

    /*
     * to do:
     * check the range of "idx"
     */
    /* restore the index */
    rz = fread(&idx, sizeof(idx), 1, fp);
    if (rz == 0) {
        return FK_SVR_ERR;
    }
    if (idx >= server.dbcnt) {
        return FK_SVR_ERR;
    }
    db = server.db[idx];

    /* restore len of dictionary */
    rz = fread(&cnt, sizeof(cnt), 1, fp);
    if (rz == 0) {
        return FK_SVR_ERR;
    }

    /* load all the elements */
    for (i = 0; i < cnt; i++) {
        rz = fread(&type, sizeof(type), 1, fp);
        if (rz == 0) {
            return FK_SVR_ERR;
        }

        switch (type) {
        case FK_ITEM_STR:
            rt = fk_fkdb_restore_str_elt(fp, db, buf);
            break;
        case FK_ITEM_LIST:
            rt = fk_fkdb_restore_list_elt(fp, db, buf);
            break;
        case FK_ITEM_DICT:
            rt = fk_fkdb_restore_dict_elt(fp, db, buf);
            break;
        default:
            rt = -1;
            break;
        }
        if (rt == FK_SVR_ERR) {
            return FK_SVR_ERR;
        }
    }
    return FK_SVR_OK;
}

/*
 * return value: -1
 * 1. error occurs when reading
 * 2. reaching to the end of file
 */
int
fk_fkdb_restore_str_elt(FILE *fp, fk_dict_t *db, fk_zline_t *buf)
{
    fk_str_t *key, *value;
    fk_item_t *kitm, *vitm;
    size_t klen, vlen, rz;

    rz = fread(&klen, sizeof(klen), 1, fp);
    if (rz == 0) {
        return FK_SVR_ERR;
    }
    if (klen > FK_ARG_HIGHWAT) {
        return FK_SVR_ERR;
    }

    fk_zline_alloc(buf, klen);
    rz = fread(buf->line, klen, 1, fp);
    if (rz == 0) {
        return FK_SVR_ERR;
    }
    key = fk_str_create(buf->line, klen);
    kitm = fk_item_create(FK_ITEM_STR, key);

    rz = fread(&vlen, sizeof(vlen), 1, fp);
    if (rz == 0) {
        return FK_SVR_ERR;
    }
    if (vlen > FK_ARG_HIGHWAT) {
        return FK_SVR_ERR;
    }
    fk_zline_alloc(buf, vlen);
    rz = fread(buf->line, vlen, 1, fp);
    if (vlen > 0 && rz == 0) {
        return FK_SVR_ERR;
    }

    value = fk_str_create(buf->line, vlen);
    vitm = fk_item_create(FK_ITEM_STR, value);

    fk_dict_add(db, kitm, vitm);

    return FK_SVR_OK;
}

/*
 * return value: -1
 * 1. error occurs when reading
 * 2. reaching to the end of file
 */
int
fk_fkdb_restore_list_elt(FILE *fp, fk_dict_t *db, fk_zline_t *buf)
{
    fk_list_t *lst;
    fk_str_t *key, *nds;
    fk_item_t *kitm, *vitm, *nitm;
    size_t klen, llen, i, nlen, rz;

    rz = fread(&klen, sizeof(klen), 1, fp);
    if (rz == 0) {
        return FK_SVR_ERR;
    }
    if (klen > FK_ARG_HIGHWAT) {
        return FK_SVR_ERR;
    }
    fk_zline_alloc(buf, klen);
    rz = fread(buf->line, klen, 1, fp);
    if (rz == 0) {
        return FK_SVR_ERR;
    }
    key = fk_str_create(buf->line, klen);
    kitm = fk_item_create(FK_ITEM_STR, key);

    rz = fread(&llen, sizeof(llen), 1, fp);
    if (rz == 0) {
        return FK_SVR_ERR;
    }

    lst = fk_list_create(&db_list_op);
    for (i = 0; i < llen; i++) {
        rz = fread(&nlen, sizeof(nlen), 1, fp);
        if (rz == 0) {
            return FK_SVR_ERR;
        }
        if (nlen > FK_ARG_HIGHWAT) {
            return FK_SVR_ERR;
        }
        fk_zline_alloc(buf, nlen);
        rz = fread(buf->line, nlen, 1, fp);
        if (nlen > 0 && rz == 0) {
            return FK_SVR_ERR;
        }
        nds = fk_str_create(buf->line, nlen);
        nitm = fk_item_create(FK_ITEM_STR, nds);
        fk_list_insert_tail(lst, nitm);
    }
    vitm = fk_item_create(FK_ITEM_LIST, lst);
    fk_dict_add(db, kitm, vitm);

    return FK_SVR_OK;
}

/*
 * return value: -1
 * 1. error occurs when reading
 * 2. reaching to the end of file
 */
int
fk_fkdb_restore_dict_elt(FILE *fp, fk_dict_t *db, fk_zline_t *buf)
{
    fk_dict_t *sdct;
    fk_str_t *key, *skey, *svalue;
    fk_item_t *kitm, *skitm, *vitm, *svitm;
    size_t klen, sklen, svlen, dlen, i, rz;

    rz = fread(&klen, sizeof(klen), 1, fp);
    if (rz == 0) {
        return FK_SVR_ERR;
    }
    if (klen > FK_ARG_HIGHWAT) {
        return FK_SVR_ERR;
    }
    fk_zline_alloc(buf, klen);
    rz = fread(buf->line, klen, 1, fp);
    if (rz == 0) {
        return FK_SVR_ERR;
    }
    key = fk_str_create(buf->line, klen);
    kitm = fk_item_create(FK_ITEM_STR, key);

    rz = fread(&dlen, sizeof(dlen), 1, fp);
    if (rz == 0) {
        return FK_SVR_ERR;
    }

    sdct = fk_dict_create(&db_dict_eop);
    for (i = 0; i < dlen; i++) {
        rz = fread(&sklen, sizeof(sklen), 1, fp);
        if (rz == 0) {
            return FK_SVR_ERR;
        }
        if (sklen > FK_ARG_HIGHWAT) {
            return FK_SVR_ERR;
        }
        fk_zline_alloc(buf, sklen);
        rz = fread(buf->line, sklen, 1, fp);
        if (rz == 0) {
            return FK_SVR_ERR;
        }
        skey = fk_str_create(buf->line, sklen);
        skitm = fk_item_create(FK_ITEM_STR, skey);

        rz = fread(&svlen, sizeof(svlen), 1, fp);
        if (rz == 0) {
            return FK_SVR_ERR;
        }
        if (svlen > FK_ARG_HIGHWAT) {
            return FK_SVR_ERR;
        }
        fk_zline_alloc(buf, svlen);
        rz = fread(buf->line, svlen, 1, fp);
        if (svlen > 0 && rz == 0) {
            return FK_SVR_ERR;
        }
        svalue = fk_str_create(buf->line, svlen);
        svitm = fk_item_create(FK_ITEM_STR, svalue);

        fk_dict_add(sdct, skitm, svitm);
    }
    vitm = fk_item_create(FK_ITEM_DICT, sdct);
    fk_dict_add(db, kitm, vitm);

    return FK_SVR_OK;
}
