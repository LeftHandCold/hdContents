//
// Created by sjw on 2018/1/30.
//

#ifndef HDCONTENTS_HDTD_STORE_H
#define HDCONTENTS_HDTD_STORE_H

#include "hdtd/system.h"
#include "hdtd/context.h"

typedef struct hd_storable_s hd_storable;

typedef void (hd_store_drop_fn)(hd_context *, hd_storable *);

struct hd_storable_s {
    int refs;
    hd_store_drop_fn *drop;
};

#define HD_INIT_STORABLE(S_,RC,DROP) \
	do { hd_storable *S = &(S_)->storable; S->refs = (RC); \
	S->drop = (DROP); \
	} while (0)

void *hd_keep_storable(hd_context *, const hd_storable *);
void hd_drop_storable(hd_context *, const hd_storable *);

typedef struct hd_store_hash_s
{
    hd_store_drop_fn *drop;
    union
    {
        struct
        {
            const void *ptr;
            int i;
        } pi; /* 8 or 12 bytes */
        struct
        {
            int id;
            float m[4];
            void *ptr;
        } im; /* 20 bytes */
        struct
        {
            unsigned char src_md5[16];
            unsigned char dst_md5[16];
            unsigned int ri:2;
            unsigned int bp:1;
            unsigned int bpp16:1;
            unsigned int proof:1;
            unsigned int src_extras:5;
            unsigned int dst_extras:5;
            unsigned int copy_spots:1;
        } link; /* 36 bytes */
    } u;
} hd_store_hash; /* 40 or 44 bytes */

typedef struct hd_store_type_s
{
    int (*make_hash_key)(hd_context *ctx, hd_store_hash *hash, void *key);
    void *(*keep_key)(hd_context *ctx, void *key);
    void (*drop_key)(hd_context *ctx, void *key);
    int (*cmp_key)(hd_context *ctx, void *a, void *b);
    void (*format_key)(hd_context *ctx, char *buf, int size, void *key);
    int (*needs_reap)(hd_context *ctx, void *key);
} hd_store_type;

/*
	hd_store_new_context: Create a new store inside the context

	max: The maximum size (in bytes) that the store is allowed to grow
	to. HD_STORE_UNLIMITED means no limit.
*/
void hd_new_store_context(hd_context *ctx, size_t max);

/*
	hd_drop_store_context: Drop a reference to the store.
*/
void hd_drop_store_context(hd_context *ctx);

/*
	hd_keep_store_context: Take a reference to the store.
*/
hd_store *hd_keep_store_context(hd_context *ctx);

/*
	hd_store_item: Add an item to the store.

	Add an item into the store, returning NULL for success. If an item
	with the same key is found in the store, then our item will not be
	inserted, and the function will return a pointer to that value
	instead. This function takes its own reference to val, as required
	(i.e. the caller maintains ownership of its own reference).

	key: The key used to index the item.

	val: The value to store.

	itemsize: The size in bytes of the value (as counted towards the
	store size).

	type: Functions used to manipulate the key.
*/
void *hd_store_item(hd_context *ctx, void *key, void *val, size_t itemsize, const hd_store_type *type);

/*
	hd_find_item: Find an item within the store.

	drop: The function used to free the value (to ensure we get a value
	of the correct type).

	key: The key used to index the item.

	type: Functions used to manipulate the key.

	Returns NULL for not found, otherwise returns a pointer to the value
	indexed by key to which a reference has been taken.
*/
void *hd_find_item(hd_context *ctx, hd_store_drop_fn *drop, void *key, const hd_store_type *type);

/*
	hd_remove_item: Remove an item from the store.

	If an item indexed by the given key exists in the store, remove it.

	drop: The function used to free the value (to ensure we get a value
	of the correct type).

	key: The key used to find the item to remove.

	type: Functions used to manipulate the key.
*/
void hd_remove_item(hd_context *ctx, hd_store_drop_fn *drop, void *key, const hd_store_type *type);

/*
	hd_empty_store: Evict everything from the store.
*/
void hd_empty_store(hd_context *ctx);

/*
	hd_debug_store: Dump the contents of the store for debugging.
*/
void hd_debug_store(hd_context *ctx);

typedef int (hd_store_filter_fn)(hd_context *ctx, void *arg, void *key);

void hd_filter_store(hd_context *ctx, hd_store_filter_fn *fn, void *arg, const hd_store_type *type);

#endif //HDCONTENTS_HDTD_STORE_H
