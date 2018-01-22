//
// Created by sjw on 2018/1/18.
//
#include "hdtd-imp.h"

#include <string.h>

#define MIN_BOMB (100 << 20)

size_t
hd_read(hd_context *ctx, hd_stream *stm, unsigned char *buf, size_t len)
{
    size_t count, n;

    count = 0;
    do
    {
        n = hd_available(ctx, stm, len);
        if (n > len)
            n = len;
        if (n == 0)
            break;

        memcpy(buf, stm->rp, n);
        stm->rp += n;
        buf += n;
        count += n;
        len -= n;
    }
    while (len > 0);

    return count;
}

static unsigned char skip_buf[4096];

size_t hd_skip(hd_context *ctx, hd_stream *stm, size_t len)
{
    size_t count, l, total = 0;

    while (len)
    {
        l = len;
        if (l > sizeof(skip_buf))
            l = sizeof(skip_buf);
        count = hd_read(ctx, stm, skip_buf, l);
        total += count;
        if (count < l)
            break;
        len -= count;
    }
    return total;
}

char *
hd_read_line(hd_context *ctx, hd_stream *stm, char *mem, size_t n)
{
    char *s = mem;
    int c = EOF;
    while (n > 1)
    {
        c = hd_read_byte(ctx, stm);
        if (c == EOF)
            break;
        if (c == '\r') {
            c = hd_peek_byte(ctx, stm);
            if (c == '\n')
                hd_read_byte(ctx, stm);
            break;
        }
        if (c == '\n')
            break;
        *s++ = c;
        n--;
    }
    if (n)
        *s = '\0';
    return (s == mem && c == EOF) ? NULL : mem;
}

int64_t
hd_tell(hd_context *ctx, hd_stream *stm)
{
    return stm->pos - (stm->wp - stm->rp);
}

void
hd_seek(hd_context *ctx, hd_stream *stm, int64_t offset, int whence)
{
    stm->avail = 0; /* Reset bit reading */
    if (stm->seek)
    {
        if (whence == 1)
        {
            offset += hd_tell(ctx, stm);
            whence = 0;
        }
        stm->seek(ctx, stm, offset, whence);
        stm->eof = 0;
    }
    else if (whence != 2)
    {
        if (whence == 0)
            offset -= hd_tell(ctx, stm);
        if (offset < 0)
            hd_warn(ctx, "cannot seek backwards");
        /* dog slow, but rare enough */
        while (offset-- > 0)
        {
            if (hd_read_byte(ctx, stm) == EOF)
            {
                hd_warn(ctx, "seek failed");
                break;
            }
        }
    }
    else
        hd_warn(ctx, "cannot seek");
}