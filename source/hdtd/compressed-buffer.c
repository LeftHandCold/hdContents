//
// Created by sjw on 2018/1/26.
//

#include "hdtd-imp.h"

/* This code needs to be kept out of stm_buffer.c to avoid it being
 * pulled into cmapdump.c */

void
hd_drop_compressed_buffer(hd_context *ctx, hd_compressed_buffer *buf)
{
    if (!buf)
        return;

    hd_drop_buffer(ctx, buf->buffer);
    hd_free(ctx, buf);
}

hd_stream *
hd_open_image_decomp_stream_from_buffer(hd_context *ctx, hd_compressed_buffer *buffer, int *l2factor)
{
    hd_stream *chain = hd_open_buffer(ctx, buffer->buffer);

    return hd_open_image_decomp_stream(ctx, chain, &buffer->params, l2factor);
}

hd_stream *
hd_open_image_decomp_stream(hd_context *ctx, hd_stream *chain, hd_compression_params *params, int *l2factor)
{
    int our_l2factor = 0;

    switch (params->type)
    {
        case HD_IMAGE_FAX:
        case HD_IMAGE_JPEG:
        case HD_IMAGE_RLD:
        case HD_IMAGE_LZW:
            return NULL;
        case HD_IMAGE_FLATE:
            chain = hd_open_flated(ctx, chain, 15);
            if (params->u.flate.predictor > 1)
            {
                //TODO:
            }
            return chain;
        default:
            break;
    }

    return chain;
}

hd_stream *
hd_open_compressed_buffer(hd_context *ctx, hd_compressed_buffer *buffer)
{
    int l2factor = 0;

    return hd_open_image_decomp_stream_from_buffer(ctx, buffer, &l2factor);
}

size_t
hd_compressed_buffer_size(hd_compressed_buffer *buffer)
{
    if (!buffer || !buffer->buffer)
        return 0;
    return (size_t)buffer->buffer->cap;
}
