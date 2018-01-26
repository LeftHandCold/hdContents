//
// Created by sjw on 2018/1/26.
//

#include "hdtd.h"

#include <zlib.h>

#include <string.h>

typedef struct hd_flate_s hd_flate;

struct hd_flate_s
{
    hd_stream *chain;
    z_stream z;
    unsigned char buffer[4096];
};

static void *zalloc_flate(void *opaque, unsigned int items, unsigned int size)
{
    return hd_malloc_array_no_throw(opaque, items, size);
}

static void zfree_flate(void *opaque, void *ptr)
{
    hd_free(opaque, ptr);
}

static int
next_flated(hd_context *ctx, hd_stream *stm, size_t required)
{
    hd_flate *state = stm->state;
    hd_stream *chain = state->chain;
    z_streamp zp = &state->z;
    int code;
    unsigned char *outbuf = state->buffer;
    int outlen = sizeof(state->buffer);

    if (stm->eof)
        return EOF;

    zp->next_out = outbuf;
    zp->avail_out = outlen;

    while (zp->avail_out > 0)
    {
        zp->avail_in = (uInt)hd_available(ctx, chain, 1);
        zp->next_in = chain->rp;

        code = inflate(zp, Z_SYNC_FLUSH);

        chain->rp = chain->wp - zp->avail_in;

        if (code == Z_STREAM_END)
        {
            break;
        }
        else if (code == Z_BUF_ERROR)
        {
            hd_warn(ctx, "premature end of data in flate filter");
            break;
        }
        else if (code == Z_DATA_ERROR && zp->avail_in == 0)
        {
            hd_warn(ctx, "ignoring zlib error: %s", zp->msg);
            break;
        }
        else if (code == Z_DATA_ERROR && !strcmp(zp->msg, "incorrect data check"))
        {
            hd_warn(ctx, "ignoring zlib error: %s", zp->msg);
            chain->rp = chain->wp;
            break;
        }
        else if (code != Z_OK)
        {
            hd_throw(ctx, HD_ERROR_GENERIC, "zlib error: %s", zp->msg);
        }
    }

    stm->rp = state->buffer;
    stm->wp = state->buffer + outlen - zp->avail_out;
    stm->pos += outlen - zp->avail_out;
    if (stm->rp == stm->wp)
    {
        stm->eof = 1;
        return EOF;
    }
    return *stm->rp++;
}

static void
close_flated(hd_context *ctx, void *state_)
{
    hd_flate *state = (hd_flate *)state_;
    int code;

    code = inflateEnd(&state->z);
    if (code != Z_OK)
        hd_warn(ctx, "zlib error: inflateEnd: %s", state->z.msg);

    hd_drop_stream(ctx, state->chain);
    hd_free(ctx, state);
}

hd_stream *
hd_open_flated(hd_context *ctx, hd_stream *chain, int window_bits)
{
    hd_flate *state = NULL;
    int code = Z_OK;

    hd_var(code);
    hd_var(state);

    hd_try(ctx)
    {
        state = hd_malloc_struct(ctx, hd_flate);
        state->chain = chain;

        state->z.zalloc = zalloc_flate;
        state->z.zfree = zfree_flate;
        state->z.opaque = ctx;
        state->z.next_in = NULL;
        state->z.avail_in = 0;

        code = inflateInit2(&state->z, window_bits);
        if (code != Z_OK)
            hd_throw(ctx, HD_ERROR_GENERIC, "zlib error: inflateInit: %s", state->z.msg);
    }
    hd_catch(ctx)
    {
        if (state && code == Z_OK)
            inflateEnd(&state->z);
        hd_free(ctx, state);
        hd_drop_stream(ctx, chain);
        hd_rethrow(ctx);
    }
    return hd_new_stream(ctx, state, next_flated, close_flated);
}
