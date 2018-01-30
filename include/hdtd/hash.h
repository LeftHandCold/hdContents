//
// Created by sjw on 2018/1/30.
//

#ifndef HDCONTENTS_HDTD_HASH_H
#define HDCONTENTS_HDTD_HASH_H

#include "hdtd/system.h"
#include "hdtd/context.h"

/*
 * Generic hash-table with fixed-length keys.
 *
 * The keys and values are NOT reference counted by the hash table.
 * Callers are responsible for taking care the reference counts are correct.
 * Inserting a duplicate entry will NOT overwrite the old value, and will
 * return the old value.
 *
 * The drop_val callback function is only used to release values when the hash table
 * is destroyed.
 */

typedef struct hd_hash_table_s hd_hash_table;
typedef void (hd_hash_table_drop_fn)(hd_context *ctx, void *val);
typedef void (hd_hash_table_for_each_fn)(hd_context *ctx, void *state, void *key, int keylen, void *val);

hd_hash_table *hd_new_hash_table(hd_context *ctx, int initialsize, int keylen, hd_hash_table_drop_fn *drop_val);
void hd_drop_hash_table(hd_context *ctx, hd_hash_table *table);

void *hd_hash_find(hd_context *ctx, hd_hash_table *table, const void *key);
void *hd_hash_insert(hd_context *ctx, hd_hash_table *table, const void *key, void *val);
void hd_hash_remove(hd_context *ctx, hd_hash_table *table, const void *key);
void hd_hash_for_each(hd_context *ctx, hd_hash_table *table, void *state, hd_hash_table_for_each_fn *callback);

#endif //HDCONTENTS_HDTD_HASH_H
