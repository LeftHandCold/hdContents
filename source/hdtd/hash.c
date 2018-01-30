//
// Created by sjw on 2018/1/30.
//

#include "hdtd.h"
#include "hdtd-imp.h"

#include <string.h>
#include <assert.h>

/*
Simple hashtable with open addressing linear probe.
Unlike text book examples, removing entries works
correctly in this implementation, so it won't start
exhibiting bad behaviour if entries are inserted
and removed frequently.
*/

enum { MAX_KEY_LEN = 48 };
typedef struct hd_hash_entry_s hd_hash_entry;

struct hd_hash_entry_s
{
    unsigned char key[MAX_KEY_LEN];
    void *val;
};

struct hd_hash_table_s
{
    int keylen;
    int size;
    int load;
    hd_hash_table_drop_fn *drop_val;
    hd_hash_entry *ents;
};

static unsigned hash(const unsigned char *s, int len)
{
    unsigned val = 0;
    int i;
    for (i = 0; i < len; i++)
    {
        val += s[i];
        val += (val << 10);
        val ^= (val >> 6);
    }
    val += (val << 3);
    val ^= (val >> 11);
    val += (val << 15);
    return val;
}

hd_hash_table *
hd_new_hash_table(hd_context *ctx, int initialsize, int keylen, hd_hash_table_drop_fn *drop_val)
{
    hd_hash_table *table;

    assert(keylen <= MAX_KEY_LEN);

    table = hd_malloc_struct(ctx, hd_hash_table);
    table->keylen = keylen;
    table->size = initialsize;
    table->load = 0;
    table->drop_val = drop_val;
    hd_try(ctx)
    {
        table->ents = hd_malloc_array(ctx, table->size, sizeof(hd_hash_entry));
        memset(table->ents, 0, sizeof(hd_hash_entry) * table->size);
    }
    hd_catch(ctx)
    {
        hd_free(ctx, table);
        hd_rethrow(ctx);
    }

    return table;
}

void
hd_drop_hash_table(hd_context *ctx, hd_hash_table *table)
{
    if (!table)
        return;

    if (table->drop_val)
    {
        int i, n = table->size;
        for (i = 0; i < n; ++i)
        {
            void *v = table->ents[i].val;
            if (v)
                table->drop_val(ctx, v);
        }
    }

    hd_free(ctx, table->ents);
    hd_free(ctx, table);
}

static void *
do_hash_insert(hd_context *ctx, hd_hash_table *table, const void *key, void *val)
{
    hd_hash_entry *ents;
    unsigned size;
    unsigned pos;

    ents = table->ents;
    size = table->size;
    pos = hash(key, table->keylen) % size;

    while (1)
    {
        if (!ents[pos].val)
        {
            memcpy(ents[pos].key, key, table->keylen);
            ents[pos].val = val;
            table->load ++;
            return NULL;
        }

        if (memcmp(key, ents[pos].key, table->keylen) == 0)
        {
            /* This is legal, but should rarely happen. */
            if (val != ents[pos].val)
                hd_warn(ctx, "assert: overwrite hash slot with different value!");
            else
                hd_warn(ctx, "assert: overwrite hash slot with same value");
            return ents[pos].val;
        }

        pos = (pos + 1) % size;
    }
}

/* Entered with the lock taken, held throughout and at exit, UNLESS the lock
 * is the alloc lock in which case it may be momentarily dropped. */
static void
hd_resize_hash(hd_context *ctx, hd_hash_table *table, int newsize)
{
    hd_hash_entry *oldents = table->ents;
    hd_hash_entry *newents;
    int oldsize = table->size;
    int oldload = table->load;
    int i;

    if (newsize < oldload * 8 / 10)
    {
        hd_warn(ctx, "assert: resize hash too small");
        return;
    }

    newents = hd_malloc_array_no_throw(ctx, newsize, sizeof(hd_hash_entry));
    if (newents == NULL)
        hd_throw(ctx, HD_ERROR_GENERIC, "hash table resize failed; out of memory (%d entries)", newsize);
    table->ents = newents;
    memset(table->ents, 0, sizeof(hd_hash_entry) * newsize);
    table->size = newsize;
    table->load = 0;

    for (i = 0; i < oldsize; i++)
    {
        if (oldents[i].val)
        {
            do_hash_insert(ctx, table, oldents[i].key, oldents[i].val);
        }
    }

    hd_free(ctx, oldents);
}

void *
hd_hash_find(hd_context *ctx, hd_hash_table *table, const void *key)
{
    hd_hash_entry *ents = table->ents;
    unsigned size = table->size;
    unsigned pos = hash(key, table->keylen) % size;


    while (1)
    {
        if (!ents[pos].val)
            return NULL;

        if (memcmp(key, ents[pos].key, table->keylen) == 0)
            return ents[pos].val;

        pos = (pos + 1) % size;
    }
}

void *
hd_hash_insert(hd_context *ctx, hd_hash_table *table, const void *key, void *val)
{
    if (table->load > table->size * 8 / 10)
        hd_resize_hash(ctx, table, table->size * 2);
    return do_hash_insert(ctx, table, key, val);
}

static void
do_removal(hd_context *ctx, hd_hash_table *table, const void *key, unsigned hole)
{
    hd_hash_entry *ents = table->ents;
    unsigned size = table->size;
    unsigned look, code;


    ents[hole].val = NULL;

    look = hole + 1;
    if (look == size)
        look = 0;

    while (ents[look].val)
    {
        code = hash(ents[look].key, table->keylen) % size;
        if ((code <= hole && hole < look) ||
            (look < code && code <= hole) ||
            (hole < look && look < code))
        {
            ents[hole] = ents[look];
            ents[look].val = NULL;
            hole = look;
        }

        look++;
        if (look == size)
            look = 0;
    }

    table->load --;
}

void
hd_hash_remove(hd_context *ctx, hd_hash_table *table, const void *key)
{
    hd_hash_entry *ents = table->ents;
    unsigned size = table->size;
    unsigned pos = hash(key, table->keylen) % size;


    while (1)
    {
        if (!ents[pos].val)
        {
            hd_warn(ctx, "assert: remove non-existent hash entry");
            return;
        }

        if (memcmp(key, ents[pos].key, table->keylen) == 0)
        {
            do_removal(ctx, table, key, pos);
            return;
        }

        pos++;
        if (pos == size)
            pos = 0;
    }
}

void
hd_hash_for_each(hd_context *ctx, hd_hash_table *table, void *state, hd_hash_table_for_each_fn *callback)
{
    int i;
    for (i = 0; i < table->size; ++i)
        if (table->ents[i].val)
            callback(ctx, state, table->ents[i].key, table->keylen, table->ents[i].val);
}
