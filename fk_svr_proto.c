#include <stdlib.h>

#include <fk_log.h>
#include <fk_svr.h>

static uint32_t fk_proto_dict_key_hash(void *key);
static int fk_proto_dict_key_cmp(void *k1, void *k2);

/* all proto to deal */
static fk_proto protos[] = {
    {"SET", 	FK_PROTO_WRITE, 	3, 					fk_cmd_set	 	},
    {"SETNX", 	FK_PROTO_WRITE, 	3, 					fk_cmd_setnx	},
    {"MSET", 	FK_PROTO_WRITE,		-3, 				fk_cmd_mset	 	},
    {"MGET", 	FK_PROTO_READ,		-2, 				fk_cmd_mget	 	},
    {"GET", 	FK_PROTO_READ, 		2, 					fk_cmd_get	 	},
    {"DEL", 	FK_PROTO_WRITE,		-2, 				fk_cmd_del	 	},
    {"FLUSHDB",	FK_PROTO_WRITE, 	1, 					fk_cmd_flushdb	},
    {"EXISTS",	FK_PROTO_READ, 		2, 					fk_cmd_exists	},
    {"FLUSHALL", FK_PROTO_WRITE, 	1, 					fk_cmd_flushall	},
    {"HSET", 	FK_PROTO_WRITE, 	4, 					fk_cmd_hset	 	},
    {"HGET", 	FK_PROTO_READ, 		3, 					fk_cmd_hget	 	},
    {"LPUSH", 	FK_PROTO_WRITE,		-3, 				fk_cmd_lpush 	},
    {"RPUSH", 	FK_PROTO_WRITE,		-3, 				fk_cmd_rpush 	},
    {"LPOP", 	FK_PROTO_READ, 		2, 					fk_cmd_lpop	 	},
    {"RPOP", 	FK_PROTO_READ, 		2, 					fk_cmd_rpop	 	},
    {"LLEN",	FK_PROTO_READ,		2,					fk_cmd_llen		},
    {"SAVE",	FK_PROTO_READ,		1,					fk_cmd_save		},
    {"SELECT",	FK_PROTO_WRITE,		2,					fk_cmd_select	},
    {"EVAL",	FK_PROTO_SCRIPT,	-3,					fk_cmd_eval		},
    {"ZADD",	FK_PROTO_WRITE,		-4,					fk_cmd_zadd		},
    {NULL, 		FK_PROTO_INVALID, 	0, 					NULL			}
};

static fk_dict_t *pmap = NULL;

static fk_elt_op_t proto_dict_eop = {
    fk_proto_dict_key_hash,
    fk_proto_dict_key_cmp,
    NULL,
    NULL,
    NULL,
    NULL
};

void
fk_proto_init(void)
{
    int i;
    char *name;
    fk_str_t *key;
    void *value;

    pmap = fk_dict_create(&proto_dict_eop);
    if (pmap == NULL) {
        exit(EXIT_FAILURE);
    }

    for (i = 0; protos[i].type != FK_PROTO_INVALID; i++) {
        name = protos[i].name;
        value = protos + i;
        key = fk_str_create(name, strlen(name));
        fk_dict_add(pmap, key, value);
    }
#ifdef FK_DEBUG
    fk_elt_t *elt;
    fk_dict_iter_t *iter = fk_dict_iter_begin(pmap);
    while ((elt = fk_dict_iter_next(iter)) != NULL) {
        key = (fk_str_t *)fk_elt_key(elt);
        value = fk_elt_value(elt);
        fk_log_debug("proto key: %s\n", fk_str_raw(key));
    }
    fk_dict_iter_end(iter);
#endif
}

fk_proto *
fk_proto_search(fk_str_t *name)
{
    fk_proto *pto;

    pto = (fk_proto *)fk_dict_get(pmap, name);

    return pto;
}

uint32_t
fk_proto_dict_key_hash(void *key)
{
    fk_str_t *s;

    s = (fk_str_t *)key;
    return fk_str_hash(s);
}

int
fk_proto_dict_key_cmp(void *k1, void *k2)
{
    fk_str_t *s1, *s2;

    s1 = (fk_str_t *)k1;
    s2 = (fk_str_t *)k2;

    return fk_str_cmp(s1, s2);
}
