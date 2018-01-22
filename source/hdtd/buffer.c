//
// Created by sjw on 2018/1/22.
//
#include "hdtd-imp.h"

hd_buffer *
hd_new_buffer(hd_context *ctx, size_t size)
{
    hd_buffer *b;

    size = size > 1 ? size : 16;

    b = hd_malloc_struct(ctx, hd_buffer);
    b->refs = 1;
    hd_try(ctx)
    {
        b->data = hd_malloc(ctx, size);
    }
    hd_catch(ctx)
    {
        hd_free(ctx, b);
        hd_rethrow(ctx);
    }
    b->cap = size;
    b->len = 0;

    return b;
}

hd_buffer *
hd_keep_buffer(hd_context *ctx, hd_buffer *buf)
{
    return hd_keep_imp(ctx, buf, &buf->refs);
}

void
hd_drop_buffer(hd_context *ctx, hd_buffer *buf)
{
    if (hd_drop_imp(ctx, buf, &buf->refs))
    {
        if (!buf->shared)
            hd_free(ctx, buf->data);
        hd_free(ctx, buf);
    }
}

void
hd_resize_buffer(hd_context *ctx, hd_buffer *buf, size_t size)
{
    if (buf->shared)
        hd_throw(ctx, HD_ERROR_GENERIC, "cannot resize a buffer with shared storage");
    buf->data = hd_resize_array(ctx, buf->data, size, 1);
    buf->cap = size;
    if (buf->len > buf->cap)
        buf->len = buf->cap;
}

void
hd_grow_buffer(hd_context *ctx, hd_buffer *buf)
{
    size_t newsize = (buf->cap * 3) / 2;
    if (newsize == 0)
        newsize = 256;
    hd_resize_buffer(ctx, buf, newsize);
}