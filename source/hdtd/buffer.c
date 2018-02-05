//
// Created by sjw on 2018/1/22.
//
#include "hdtd-imp.h"

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

