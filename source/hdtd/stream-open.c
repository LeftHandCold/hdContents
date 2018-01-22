//
// Created by sjw on 2018/1/15.
//
#include "hdtd-imp.h"

void
hd_drop_stream(hd_context *ctx, hd_stream *stm)
{
    if (hd_drop_imp(ctx, stm, &stm->refs))
    {
        if (stm->close)
            stm->close(ctx, stm->state);
        hd_free(ctx, stm);
    }
}

hd_stream *
hd_new_stream(hd_context *ctx, void *state, hd_stream_next_fn *next, hd_stream_close_fn *close)
{
    hd_stream *stm;

    hd_try(ctx)
    {
        stm = hd_malloc_struct(ctx, hd_stream);
    }
    hd_catch(ctx)
    {
        close(ctx, state);
    }

    stm->refs = 1;
    stm->error = 0;
    stm->eof = 0;
    stm->pos = 0;

    stm->bits = 0;
    stm->avail = 0;

    stm->rp = NULL;
    stm->wp = NULL;

    stm->state = state;
    stm->next = next;
    stm->close = close;
    stm->seek = NULL;

    return stm;
}

/* File stream */

typedef struct hd_file_stream_s
{
    FILE *file;
    unsigned char buffer[4096];
} hd_file_stream;

static int next_file(hd_context *ctx, hd_stream *stm, size_t n)
{
    hd_file_stream *state = stm->state;

    /* n is only a hint, that we can safely ignore */
    n = fread(state->buffer, 1, sizeof(state->buffer), state->file);
    if (n < sizeof(state->buffer) && ferror(state->file))
        hd_throw(ctx, HD_ERROR_GENERIC, "read error: %s", strerror(errno));
    stm->rp = state->buffer;
    stm->wp = state->buffer + n;
    stm->pos += (hd_off_t)n;

    if (n == 0)
        return EOF;
    return *stm->rp++;
}

static void seek_file(hd_context *ctx, hd_stream *stm, hd_off_t offset, int whence)
{
    hd_file_stream *state = stm->state;
    hd_off_t n = hd_fseek(state->file, offset, whence);
    if (n < 0)
        hd_throw(ctx, HD_ERROR_GENERIC, "cannot seek: %s", strerror(errno));
    stm->pos = hd_ftell(state->file);
    stm->rp = state->buffer;
    stm->wp = state->buffer;
}

static void close_file(hd_context *ctx, void *state_)
{
    hd_file_stream *state = state_;
    int n = fclose(state->file);
    if (n < 0)
        hd_warn(ctx, "close error: %s", strerror(errno));
    hd_free(ctx, state);
}

hd_stream *
hd_open_file_ptr(hd_context *ctx, FILE *file)
{
    hd_stream *stm;
    hd_file_stream *state = hd_malloc_struct(ctx, hd_file_stream);
    state->file = file;

    stm = hd_new_stream(ctx, state, next_file, close_file);
    stm->seek = seek_file;

    return stm;
}

hd_stream *
hd_open_file(hd_context *ctx, const char *name)
{
    FILE *f;
#if defined(_WIN32) || defined(_WIN64)
    char *s = (char*)name;
    wchar_t *wname, *d;
    int c;
    d = wname = hd_malloc(ctx, (strlen(name)+1) * sizeof(wchar_t));
    while (*s) {
        s += hd_chartorune(&c, s);
        *d++ = c;
    }
    *d = 0;
    f = _wfopen(wname, L"rb");
    hd_free(ctx, wname);
#else
    f = hd_fopen(name, "rb");
#endif
    if (f == NULL)
        hd_throw(ctx, HD_ERROR_GENERIC, "cannot open %s: %s", name, strerror(errno));
    return hd_open_file_ptr(ctx, f);
}