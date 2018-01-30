//
// Created by sjw on 2018/1/30.
//
#include <pdf.h>
#include "hdtd.h"
#include "hdtd-imp.h"

typedef struct hd_item_s hd_item;

struct hd_item_s
{
    void *key;
    hd_storable *val;
    size_t size;
    hd_item *next;
    hd_item *prev;
    hd_store *store;
    const hd_store_type *type;
};

/* Every entry in hd_store is protected by the alloc lock */
struct hd_store_s
{
    int refs;

    /* Every item in the store is kept in a doubly linked list, ordered
     * by usage (so LRU entries are at the end). */
    hd_item *head;
    hd_item *tail;

    /* We have a hash table that allows to quickly find a subset of the
     * entries (those whose keys are indirect objects). */
    hd_hash_table *hash;

    /* We keep track of the size of the store, and keep it below max. */
    size_t max;
    size_t size;

    int defer_reap_count;
    int needs_reaping;
};

void
hd_new_store_context(hd_context *ctx, size_t max)
{
    hd_store *store;
    store = hd_malloc_struct(ctx, hd_store);
    hd_try(ctx)
    {
        store->hash = hd_new_hash_table(ctx, 4096, sizeof(hd_store_hash), NULL);
    }
    hd_catch(ctx)
    {
        hd_free(ctx, store);
        hd_rethrow(ctx);
    }
    store->refs = 1;
    store->head = NULL;
    store->tail = NULL;
    store->size = 0;
    store->max = max;
    store->defer_reap_count = 0;
    store->needs_reaping = 0;
    ctx->store = store;
}

void *
hd_keep_storable(hd_context *ctx, const hd_storable *sc)
{
    /* Explicitly drop const to allow us to use const
     * sanely throughout the code. */
    hd_storable *s = (hd_storable *)sc;

    return hd_keep_imp(ctx, s, &s->refs);
}

void
hd_drop_storable(hd_context *ctx, const hd_storable *sc)
{
    /* Explicitly drop const to allow us to use const
     * sanely throughout the code. */
    hd_storable *s = (hd_storable *)sc;

    /*
        If we are dropping the last reference to an object, then
        it cannot possibly be in the store (as the store always
        keeps a ref to everything in it, and doesn't drop via
        this method. So we can simply drop the storable object
        itself without any operations on the hd_store.
     */
    if (hd_drop_imp(ctx, s, &s->refs))
        s->drop(ctx, s);
}

static void
evict(hd_context *ctx, hd_item *item)
{
    hd_store *store = ctx->store;
    int drop;

    store->size -= item->size;
    /* Unlink from the linked list */
    if (item->next)
        item->next->prev = item->prev;
    else
        store->tail = item->prev;
    if (item->prev)
        item->prev->next = item->next;
    else
        store->head = item->next;

    /* Drop a reference to the value (freeing if required) */
    if (item->val->refs > 0)
        (void)Memento_dropRef(item->val);
    drop = (item->val->refs > 0 && --item->val->refs == 0);

    /* Remove from the hash table */
    if (item->type->make_hash_key)
    {
        hd_store_hash hash = { NULL };
        hash.drop = item->val->drop;
        if (item->type->make_hash_key(ctx, &hash, item->key))
            hd_hash_remove(ctx, store->hash, &hash);
    }
    if (drop)
        item->val->drop(ctx, item->val);

    /* Always drops the key and drop the item */
    item->type->drop_key(ctx, item->key);
    hd_free(ctx, item);
}

static size_t
ensure_space(hd_context *ctx, size_t tofree)
{
    hd_item *item, *prev;
    size_t count;
    hd_store *store = ctx->store;


    /* First check that we *can* free tofree; if not, we'd rather not
     * cache this. */
    count = 0;
    for (item = store->tail; item; item = item->prev)
    {
        if (item->val->refs == 1)
        {
            count += item->size;
            if (count >= tofree)
                break;
        }
    }

    /* If we ran out of items to search, then we can never free enough */
    if (item == NULL)
    {
        return 0;
    }

    /* Actually free the items */
    count = 0;
    for (item = store->tail; item; item = prev)
    {
        prev = item->prev;
        if (item->val->refs == 1)
        {
            /* Free this item. Evict has to drop the lock to
             * manage that, which could cause prev to be removed
             * in the meantime. To avoid that we bump its reference
             * count here. This may cause another simultaneous
             * evict process to fail to make enough space as prev is
             * pinned - but that will only happen if we're near to
             * the limit anyway, and it will only cause something to
             * not be cached. */
            count += item->size;
            if (prev)
            {
                (void)Memento_takeRef(prev->val);
                prev->val->refs++;
            }
            evict(ctx, item); /* Drops then retakes lock */
            /* So the store has 1 reference to prev, as do we, so
             * no other evict process can have thrown prev away in
             * the meantime. So we are safe to just decrement its
             * reference count here. */
            if (prev)
            {
                (void)Memento_dropRef(prev->val);
                --prev->val->refs;
            }

            if (count >= tofree)
                return count;
        }
    }

    return count;
}

static void
touch(hd_store *store, hd_item *item)
{
    if (item->next != item)
    {
        /* Already in the list - unlink it */
        if (item->next)
            item->next->prev = item->prev;
        else
            store->tail = item->prev;
        if (item->prev)
            item->prev->next = item->next;
        else
            store->head = item->next;
    }
    /* Now relink it at the start of the LRU chain */
    item->next = store->head;
    if (item->next)
        item->next->prev = item;
    else
        store->tail = item;
    store->head = item;
    item->prev = NULL;
}

void *
hd_store_item(hd_context *ctx, void *key, void *val_, size_t itemsize, const hd_store_type *type)
{
    hd_item *item = NULL;
    size_t size;
    hd_storable *val = (hd_storable *)val_;
    hd_store *store = ctx->store;
    hd_store_hash hash = { NULL };
    int use_hash = 0;

    if (!store)
        return NULL;

    hd_var(item);

    /* If we fail for any reason, we swallow the exception and continue.
     * All that the above program will see is that we failed to store
     * the item. */
    hd_try(ctx)
    {
        item = hd_malloc_struct(ctx, hd_item);
    }
    hd_catch(ctx)
    {
        return NULL;
    }

    if (type->make_hash_key)
    {
        hash.drop = val->drop;
        use_hash = type->make_hash_key(ctx, &hash, key);
    }

    type->keep_key(ctx, key);

    /* Fill out the item. To start with, we always set item->next == item
     * and item->prev == item. This is so that we can spot items that have
     * been put into the hash table without having made it into the linked
     * list yet. */
    item->key = key;
    item->val = val;
    item->size = itemsize;
    item->next = item;
    item->prev = item;
    item->type = type;

    /* If we can index it fast, put it into the hash table. This serves
     * to check whether we have one there already. */
    if (use_hash)
    {
        hd_item *existing = NULL;

        hd_try(ctx)
        {
            /* May drop and retake the lock */
            existing = hd_hash_insert(ctx, store->hash, hash.u.pi.i + hash.u.pi.ptr, item);
        }
        hd_catch(ctx)
        {
            /* Any error here means that item never made it into the
             * hash - so no one else can have a reference. */
            hd_free(ctx, item);
            type->drop_key(ctx, key);
            return NULL;
        }
        if (existing)
        {
            /* There was one there already! Take a new reference
             * to the existing one, and drop our current one. */
            touch(store, existing);
            if (existing->val->refs > 0)
            {
                (void)Memento_takeRef(existing->val);
                existing->val->refs++;
            }
            hd_free(ctx, item);
            type->drop_key(ctx, key);
            return existing->val;
        }
    }

    /* Now bump the ref */
    if (val->refs > 0)
    {
        (void)Memento_takeRef(val);
        val->refs++;
    }

    /* If we haven't got an infinite store, check for space within it */
    if (store->max != HD_STORE_UNLIMITED)
    {
        size = store->size + itemsize;
        while (size > store->max)
        {
            size_t saved;

            size = store->size + itemsize;
            if (size <= store->max)
                break;

            /* ensure_space may drop, then retake the lock */
            saved = ensure_space(ctx, size - store->max);
            size -= saved;
            if (saved == 0)
            {
                /* Failed to free any space. */
                /* We used to 'unstore' it here, but that's wrong.
                 * If we've already spent the memory to malloc it
                 * then not putting it in the store just means that
                 * a resource used multiple times will just be malloced
                 * again. Better to put it in the store, have the
                 * store account for it, and for it to potentially be reused.
                 * When the caller drops the reference to it, it can then
                 * be dropped from the store on the next attempt to store
                 * anything else. */
                break;
            }
        }
    }
    store->size += itemsize;

    /* Regardless of whether it's indexed, it goes into the linked list */
    touch(store, item);

    return NULL;
}

void *
hd_find_item(hd_context *ctx, hd_store_drop_fn *drop, void *key, const hd_store_type *type)
{
    hd_item *item;
    hd_store *store = ctx->store;
    hd_store_hash hash = { NULL };
    int use_hash = 0;

    if (!store)
        return NULL;

    if (!key)
        return NULL;

    if (type->make_hash_key)
    {
        hash.drop = drop;
        use_hash = type->make_hash_key(ctx, &hash, key);
    }

    if (use_hash)
    {
        /* We can find objects keyed on indirected objects quickly */
        item = hd_hash_find(ctx, store->hash, hash.u.pi.i + hash.u.pi.ptr);
    }
    else
    {
        /* Others we have to hunt for slowly */
        for (item = store->head; item; item = item->next)
        {
            if (item->val->drop == drop && !type->cmp_key(ctx, item->key, key))
                break;
        }
    }
    if (item)
    {
        /* LRU the block. This also serves to ensure that any item
         * picked up from the hash before it has made it into the
         * linked list does not get whipped out again due to the
         * store being full. */
        touch(store, item);
        /* And bump the refcount before returning */
        if (item->val->refs > 0)
        {
            (void)Memento_takeRef(item->val);
            item->val->refs++;
        }
        return (void *)item->val;
    }

    return NULL;
}

void
hd_remove_item(hd_context *ctx, hd_store_drop_fn *drop, void *key, const hd_store_type *type)
{
    hd_item *item;
    hd_store *store = ctx->store;
    int dodrop;
    hd_store_hash hash = { NULL };
    int use_hash = 0;

    if (type->make_hash_key)
    {
        hash.drop = drop;
        use_hash = type->make_hash_key(ctx, &hash, key);
    }

    if (use_hash)
    {
        /* We can find objects keyed on indirect objects quickly */
        item = hd_hash_find(ctx, store->hash, &hash);
        if (item)
            hd_hash_remove(ctx, store->hash, &hash);
    }
    else
    {
        /* Others we have to hunt for slowly */
        for (item = store->head; item; item = item->next)
            if (item->val->drop == drop && !type->cmp_key(ctx, item->key, key))
                break;
    }
    if (item)
    {
        /* Momentarily things can be in the hash table without being
         * in the list. Don't attempt to unlink these. We indicate
         * such items by setting item->next == item. */
        if (item->next != item)
        {
            if (item->next)
                item->next->prev = item->prev;
            else
                store->tail = item->prev;
            if (item->prev)
                item->prev->next = item->next;
            else
                store->head = item->next;
        }
        if (item->val->refs > 0)
            (void)Memento_dropRef(item->val);
        dodrop = (item->val->refs > 0 && --item->val->refs == 0);
        if (dodrop)
            item->val->drop(ctx, item->val);
        type->drop_key(ctx, item->key);
        hd_free(ctx, item);
    }
}

void
hd_empty_store(hd_context *ctx)
{
    hd_store *store = ctx->store;

    if (store == NULL)
        return;

    /* Run through all the items in the store */
    while (store->head)
        evict(ctx, store->head); /* Drops then retakes lock */
}

hd_store *
hd_keep_store_context(hd_context *ctx)
{
    if (ctx == NULL || ctx->store == NULL)
        return NULL;
    return hd_keep_imp(ctx, ctx->store, &ctx->store->refs);
}

void
hd_drop_store_context(hd_context *ctx)
{
    if (!ctx)
        return;
    if (hd_drop_imp(ctx, ctx->store, &ctx->store->refs))
    {
        hd_empty_store(ctx);
        hd_drop_hash_table(ctx, ctx->store->hash);
        hd_free(ctx, ctx->store);
        ctx->store = NULL;
    }
}

static void
hd_debug_store_item(hd_context *ctx, void *state, void *key_, int keylen, void *item_)
{
    unsigned char *key = key_;
    hd_item *item = item_;
    int i;
    char buf[256];
    item->type->format_key(ctx, buf, sizeof buf, item->key);
    printf("hash[");
    for (i=0; i < keylen; ++i)
        printf("%02x", key[i]);
    printf("][refs=%d][size=%d] key=%s val=%p\n", item->val->refs, (int)item->size, buf, item->val);
}

void
hd_debug_store(hd_context *ctx)
{
    hd_item *item, *next;
    char buf[256];
    hd_store *store = ctx->store;

    printf("-- resource store contents --\n");

    for (item = store->head; item; item = next)
    {
        next = item->next;
        if (next)
        {
            (void)Memento_takeRef(next->val);
            next->val->refs++;
        }
        item->type->format_key(ctx, buf, sizeof buf, item->key);
        printf("store[*][refs=%d][size=%d] key=%s val=%p\n",
               item->val->refs, (int)item->size, buf, item->val);
        if (next)
        {
            (void)Memento_dropRef(next->val);
            next->val->refs--;
        }
    }

    printf("-- resource store hash contents --\n");
    hd_hash_for_each(ctx, store->hash, NULL, hd_debug_store_item);
    printf("-- end --\n");
}

void hd_filter_store(hd_context *ctx, hd_store_filter_fn *fn, void *arg, const hd_store_type *type)
{
    hd_store *store;
    hd_item *item, *prev, *remove;

    store = ctx->store;
    if (store == NULL)
        return;

    /* Filter the items */
    remove = NULL;
    for (item = store->tail; item; item = prev)
    {
        prev = item->prev;
        if (item->type != type)
            continue;

        if (fn(ctx, arg, item->key) == 0)
            continue;

        /* We have to drop it */
        store->size -= item->size;

        /* Unlink from the linked list */
        if (item->next)
            item->next->prev = item->prev;
        else
            store->tail = item->prev;
        if (item->prev)
            item->prev->next = item->next;
        else
            store->head = item->next;

        /* Remove from the hash table */
        if (item->type->make_hash_key)
        {
            hd_store_hash hash = { NULL };
            hash.drop = item->val->drop;
            if (item->type->make_hash_key(ctx, &hash, item->key))
                hd_hash_remove(ctx, store->hash, &hash);
        }

        /* Store whether to drop this value or not in 'prev' */
        if (item->val->refs > 0)
            (void)Memento_dropRef(item->val);
        item->prev = (item->val->refs > 0 && --item->val->refs == 0) ? item : NULL;

        /* Store it in our removal chain - just singly linked */
        item->next = remove;
        remove = item;
    }

    /* Now drop the remove chain */
    for (item = remove; item != NULL; item = remove)
    {
        remove = item->next;

        /* Drop a reference to the value (freeing if required) */
        if (item->prev) /* See above for our abuse of prev here */
            item->val->drop(ctx, item->val);

        /* Always drops the key and drop the item */
        item->type->drop_key(ctx, item->key);
        hd_free(ctx, item);
    }
}