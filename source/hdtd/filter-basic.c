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

/* Concat filter concatenates several streams into one */

struct concat_filter
{
    int max;
    int count;
    int current;
    int pad; /* 1 if we should add whitespace padding between streams */
    unsigned char ws_buf;
    hd_stream *chain[1];
};

static int
next_concat(hd_context *ctx, hd_stream *stm, size_t max)
{
    struct concat_filter *state = (struct concat_filter *)stm->state;
    size_t n;

    while (state->current < state->count)
    {
        /* Read the next block of underlying data. */
        if (stm->wp == state->chain[state->current]->wp)
            state->chain[state->current]->rp = stm->wp;
        n = hd_available(ctx, state->chain[state->current], max);
        if (n)
        {
            stm->rp = state->chain[state->current]->rp;
            stm->wp = state->chain[state->current]->wp;
            stm->pos += (int64_t)n;
            return *stm->rp++;
        }
        else
        {
            if (state->chain[state->current]->error)
            {
                stm->error = 1;
                break;
            }
            state->current++;
            hd_drop_stream(ctx, state->chain[state->current-1]);
            if (state->pad)
            {
                stm->rp = (&state->ws_buf)+1;
                stm->wp = stm->rp + 1;
                stm->pos++;
                return 32;
            }
        }
    }

    stm->rp = stm->wp;

    return EOF;
}

static void
close_concat(hd_context *ctx, void *state_)
{
    struct concat_filter *state = (struct concat_filter *)state_;
    int i;

    for (i = state->current; i < state->count; i++)
    {
        hd_drop_stream(ctx, state->chain[i]);
    }
    hd_free(ctx, state);
}

hd_stream *
hd_open_concat(hd_context *ctx, int len, int pad)
{
    struct concat_filter *state;

    state = hd_calloc(ctx, 1, sizeof(struct concat_filter) + (len-1)*sizeof(hd_stream *));
    state->max = len;
    state->count = 0;
    state->current = 0;
    state->pad = pad;
    state->ws_buf = 32;

    return hd_new_stream(ctx, state, next_concat, close_concat);
}

void
hd_concat_push_drop(hd_context *ctx, hd_stream *concat, hd_stream *chain)
{
    struct concat_filter *state = (struct concat_filter *)concat->state;

    if (state->count == state->max)
    {
        hd_drop_stream(ctx, chain);
        hd_throw(ctx, HD_ERROR_GENERIC, "Concat filter size exceeded");
    }

    state->chain[state->count++] = chain;
}
