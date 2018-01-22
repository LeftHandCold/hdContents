//
// Created by sjw on 2018/1/15.
//

#ifndef HDCONTENTS_HDTD_IMP_H
#define HDCONTENTS_HDTD_IMP_H

#include "hdtd.h"

struct hd_buffer_s
{
    int refs;
    unsigned char *data;
    size_t cap, len;
    int shared;
};

void hd_new_document_handler_context(hd_context *ctx);
void hd_drop_document_handler_context(hd_context *ctx);

static inline void *
hd_keep_imp(hd_context *ctx, void *p, int *refs)
{
    if (p)
    {
        (void)Memento_checkIntPointerOrNull(refs);
        if (*refs > 0)
        {
            (void)Memento_takeRef(p);
            ++*refs;
        }
    }
    return p;
}

static inline void *
hd_keep_imp8(hd_context *ctx, void *p, int8_t *refs)
{
    if (p)
    {
        (void)Memento_checkBytePointerOrNull(refs);
        if (*refs > 0)
        {
            (void)Memento_takeRef(p);
            ++*refs;
        }
    }
    return p;
}

static inline void *
hd_keep_imp16(hd_context *ctx, void *p, int16_t *refs)
{
    if (p)
    {
        (void)Memento_checkShortPointerOrNull(refs);
        if (*refs > 0)
        {
            (void)Memento_takeRef(p);
            ++*refs;
        }
    }
    return p;
}


static inline int
hd_drop_imp(hd_context *ctx, void *p, int *refs)
{
    if (p)
    {
        int drop;
        if (*refs > 0)
            (void)Memento_dropRef(p);
        if (*refs > 0)
            drop = --*refs == 0;
        else
            drop = 0;
        return drop;
    }
    return 0;
}

static inline int
hd_drop_imp8(hd_context *ctx, void *p, int8_t *refs)
{
    if (p)
    {
        int drop;
        (void)Memento_checkBytePointerOrNull(refs);
        if (*refs > 0)
        {
            (void)Memento_dropByteRef(p);
            drop = --*refs == 0;
        }
        else
            drop = 0;
        return drop;
    }
    return 0;
}

static inline int
hd_drop_imp16(hd_context *ctx, void *p, int16_t *refs)
{
    if (p)
    {
        int drop;
        (void)Memento_checkShortPointerOrNull(refs);
        if (*refs > 0)
        {
            (void)Memento_dropShortRef(p);
            drop = --*refs == 0;
        }
        else
            drop = 0;
        return drop;
    }
    return 0;
}
#endif //HDCONTENTS_HDTD_IMP_H
