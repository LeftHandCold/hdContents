//
// Created by sjw on 2018/1/26.
//
#include "hdtd.h"

struct null_filter
{
    hd_stream *chain;
    size_t remain;
    int64_t offset;
    unsigned char buffer[4096];
};

static int
next_null(hd_context *ctx, hd_stream *stm, size_t max)
{
    struct null_filter *state = stm->state;
    size_t n;

    if (state->remain == 0)
        return EOF;
    hd_seek(ctx, state->chain, state->offset, 0);
    n = hd_available(ctx, state->chain, max);
    if (n > state->remain)
        n = state->remain;
    if (n > sizeof(state->buffer))
        n = sizeof(state->buffer);
    memcpy(state->buffer, state->chain->rp, n);
    stm->rp = state->buffer;
    stm->wp = stm->rp + n;
    if (n == 0)
        return EOF;
    state->chain->rp += n;
    state->remain -= n;
    state->offset += (hd_off_t)n;
    stm->pos += (hd_off_t)n;
    return *stm->rp++;
}

static void
close_null(hd_context *ctx, void *state_)
{
    struct null_filter *state = (struct null_filter *)state_;
    hd_stream *chain = state->chain;
    hd_free(ctx, state);
    hd_drop_stream(ctx, chain);
}

hd_stream *
hd_open_null(hd_context *ctx, hd_stream *chain, int len, hd_off_t offset)
{
    struct null_filter *state = NULL;

    if (len < 0)
        len = 0;
    hd_try(ctx)
    {
        state = hd_malloc_struct(ctx, struct null_filter);
        state->chain = chain;
        state->remain = len;
        state->offset = offset;
    }
    hd_catch(ctx)
    {
        hd_drop_stream(ctx, chain);
        hd_rethrow(ctx);
    }

    return hd_new_stream(ctx, state, next_null, close_null);
}
