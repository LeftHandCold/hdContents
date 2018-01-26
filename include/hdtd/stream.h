//
// Created by sjw on 2018/1/15.
//

#ifndef HDCONTENTS_HDTD_STREAM_H
#define HDCONTENTS_HDTD_STREAM_H

#include "hdtd/system.h"
#include "hdtd/context.h"

/*
	hd_stream is a buffered reader capable of seeking in both
	directions.

	Streams are reference counted, so references must be dropped
	by a call to hd_drop_stream.

	Only the data between rp and wp is valid.
*/
typedef struct hd_stream_s hd_stream;

/*
	hd_open_file: Open the named file and wrap it in a stream.

	filename: Path to a file. On non-Windows machines the filename should
	be exactly as it would be passed to fopen(2). On Windows machines, the
	path should be UTF-8 encoded so that non-ASCII characters can be
	represented. Other platforms do the encoding as standard anyway (and
	in most cases, particularly for MacOS and Linux, the encoding they
	use is UTF-8 anyway).
*/
hd_stream *hd_open_file(hd_context *ctx, const char *filename);

/*
	hd_drop_stream: Close an open stream.

	Drops a reference for the stream. Once no references remain
	the stream will be closed, as will any file descriptor the
	stream is using.

	Does not throw exceptions.
*/
void hd_drop_stream(hd_context *ctx, hd_stream *stm);

/*
	hd_tell: return the current reading position within a stream
*/
int64_t hd_tell(hd_context *ctx, hd_stream *stm);

/*
	hd_seek: Seek within a stream.

	stm: The stream to seek within.

	offset: The offset to seek to.

	whence: From where the offset is measured (see fseek).
*/
void hd_seek(hd_context *ctx, hd_stream *stm, int64_t offset, int whence);

/*
	hd_read: Read from a stream into a given data block.

	stm: The stream to read from.

	data: The data block to read into.

	len: The length of the data block (in bytes).

	Returns the number of bytes read. May throw exceptions.
*/
size_t hd_read(hd_context *ctx, hd_stream *stm, unsigned char *data, size_t len);

/*
	hd_stream_next_fn: A function type for use when implementing
	hd_streams. The supplied function of this type is called
	whenever data is required, and the current buffer is empty.

	stm: The stream to operate on.

	max: a hint as to the maximum number of bytes that the caller
	needs to be ready immediately. Can safely be ignored.

	Returns -1 if there is no more data in the stream. Otherwise,
	the function should find its internal state using stm->state,
	refill its buffer, update stm->rp and stm->wp to point to the
	start and end of the new data respectively, and then
	"return *stm->rp++".
*/
typedef int (hd_stream_next_fn)(hd_context *ctx, hd_stream *stm, size_t max);

/*
	hd_stream_close_fn: A function type for use when implementing
	hd_streams. The supplied function of this type is called
	when the stream is closed, to release the stream specific
	state information.

	state: The stream state to release.
*/
typedef void (hd_stream_close_fn)(hd_context *ctx, void *state);

/*
	hd_stream_seek_fn: A function type for use when implementing
	hd_streams. The supplied function of this type is called when
	hd_seek is requested, and the arguments are as defined for
	hd_seek.

	The stream can find it's private state in stm->state.
*/
typedef void (hd_stream_seek_fn)(hd_context *ctx, hd_stream *stm, hd_off_t offset, int whence);

/*
	hd_stream_meta_fn: A function type for use when implementing
	hd_streams. The supplied function of this type is called when
	hd_meta is requested, and the arguments are as defined for
	hd_meta.

	The stream can find it's private state in stm->state.
*/
typedef int (hd_stream_meta_fn)(hd_context *ctx, hd_stream *stm, int key, int size, void *ptr);

struct hd_stream_s
{
    int refs;
    int error;
    int eof;
    hd_off_t pos;
    int avail;
    int bits;
    unsigned char *rp, *wp;
    void *state;
    hd_stream_next_fn *next;
    hd_stream_close_fn *close;
    hd_stream_seek_fn *seek;
};

/*
	hd_new_stream: Create a new stream object with the given
	internal state and function pointers.

	state: Internal state (opaque to everything but implementation).

	next: Should provide the next set of bytes (up to max) of stream
	data. Return the number of bytes read, or EOF when there is no
	more data.

	close: Should clean up and free the internal state. May not
	throw exceptions.
*/
hd_stream *hd_new_stream(hd_context *ctx, void *state, hd_stream_next_fn *next, hd_stream_close_fn *close);

hd_stream *hd_keep_stream(hd_context *ctx, hd_stream *stm);

/*
	hd_open_buffer: Open a buffer as a stream.

	buf: The buffer to open. Ownership of the buffer is NOT passed in
	(this function takes its own reference).

	Returns pointer to newly created stream. May throw exceptions on
	failure to allocate.
*/
hd_stream *hd_open_buffer(hd_context *ctx, hd_buffer *buf);

/*
	hd_read_line: Read a line from stream into the buffer until either a
	terminating newline or EOF, which it replaces with a null byte ('\0').

	Returns buf on success, and NULL when end of file occurs while no characters
	have been read.
*/
char *hd_read_line(hd_context *ctx, hd_stream *stm, char *buf, size_t max);

/*
	hd_available: Ask how many bytes are available immediately from
	a given stream.

	stm: The stream to read from.

	max: A hint for the underlying stream; the maximum number of
	bytes that we are sure we will want to read. If you do not know
	this number, give 1.

	Returns the number of bytes immediately available between the
	read and write pointers. This number is guaranteed only to be 0
	if we have hit EOF. The number of bytes returned here need have
	no relation to max (could be larger, could be smaller).
*/
static inline size_t hd_available(hd_context *ctx, hd_stream *stm, size_t max)
{
    size_t len = stm->wp - stm->rp;
    int c = EOF;

    if (len)
        return len;
    hd_try(ctx)
    {
        c = stm->next(ctx, stm, max);
    }
    hd_catch(ctx)
    {
        hd_rethrow_if(ctx, HD_ERROR_TRYLATER);
        hd_warn(ctx, "read error; treating as end of file");
        stm->error = 1;
        c = EOF;
    }
    if (c == EOF)
    {
        stm->eof = 1;
        return 0;
    }
    stm->rp--;
    return stm->wp - stm->rp;
}

/*
	hd_read_byte: Read the next byte from a stream.

	stm: The stream t read from.

	Returns -1 for end of stream, or the next byte. May
	throw exceptions.
*/
static inline int hd_read_byte(hd_context *ctx, hd_stream *stm)
{
    int c = EOF;

    if (stm->rp != stm->wp)
        return *stm->rp++;
    hd_try(ctx)
    {
        c = stm->next(ctx, stm, 1);
    }
    hd_catch(ctx)
    {
        hd_rethrow_if(ctx, HD_ERROR_TRYLATER);
        hd_warn(ctx, "read error; treating as end of file");
        stm->error = 1;
        c = EOF;
    }
    if (c == EOF)
        stm->eof = 1;
    return c;
}

/*
	hd_peek_byte: Peek at the next byte in a stream.

	stm: The stream to peek at.

	Returns -1 for EOF, or the next byte that will be read.
*/
static inline int hd_peek_byte(hd_context *ctx, hd_stream *stm)
{
    int c;

    if (stm->rp != stm->wp)
        return *stm->rp;

    c = stm->next(ctx, stm, 1);
    if (c != EOF)
        stm->rp--;
    return c;
}

/*
	hd_unread_byte: Unread the single last byte successfully
	read from a stream. Do not call this without having
	successfully read a byte.

	stm: The stream to operate upon.
*/
static inline void hd_unread_byte(hd_context *ctx HD_UNUSED, hd_stream *stm)
{
    stm->rp--;
}

static inline int hd_is_eof(hd_context *ctx, hd_stream *stm)
{
    if (stm->rp == stm->wp)
    {
        if (stm->eof)
            return 1;
        return hd_peek_byte(ctx, stm) == EOF;
    }
    return 0;
}

/*
	hd_read_bits: Read the next n bits from a stream (assumed to
	be packed most significant bit first).

	stm: The stream to read from.

	n: The number of bits to read, between 1 and 8*sizeof(int)
	inclusive.

	Returns -1 for EOF, or the required number of bits.
*/
static inline unsigned int hd_read_bits(hd_context *ctx, hd_stream *stm, int n)
{
    int x;

    if (n <= stm->avail)
    {
        stm->avail -= n;
        x = (stm->bits >> stm->avail) & ((1 << n) - 1);
    }
    else
    {
        x = stm->bits & ((1 << stm->avail) - 1);
        n -= stm->avail;
        stm->avail = 0;

        while (n > 8)
        {
            x = (x << 8) | hd_read_byte(ctx, stm);
            n -= 8;
        }

        if (n > 0)
        {
            stm->bits = hd_read_byte(ctx, stm);
            stm->avail = 8 - n;
            x = (x << n) | (stm->bits >> stm->avail);
        }
    }

    return x;
}

/*
	hd_read_rbits: Read the next n bits from a stream (assumed to
	be packed least significant bit first).

	stm: The stream to read from.

	n: The number of bits to read, between 1 and 8*sizeof(int)
	inclusive.

	Returns (unsigned int)-1 for EOF, or the required number of bits.
*/
static inline unsigned int hd_read_rbits(hd_context *ctx, hd_stream *stm, int n)
{
    int x;

    if (n <= stm->avail)
    {
        x = stm->bits & ((1 << n) - 1);
        stm->avail -= n;
        stm->bits = stm->bits >> n;
    }
    else
    {
        unsigned int used = 0;

        x = stm->bits & ((1 << stm->avail) - 1);
        n -= stm->avail;
        used = stm->avail;
        stm->avail = 0;

        while (n > 8)
        {
            x = (hd_read_byte(ctx, stm) << used) | x;
            n -= 8;
            used += 8;
        }

        if (n > 0)
        {
            stm->bits = hd_read_byte(ctx, stm);
            x = ((stm->bits & ((1 << n) - 1)) << used) | x;
            stm->avail = 8 - n;
            stm->bits = stm->bits >> n;
        }
    }

    return x;
}

/*
	hd_sync_bits: Called after reading bits to tell the stream
	that we are about to return to reading bytewise. Resyncs
	the stream to whole byte boundaries.
*/
static inline void hd_sync_bits(hd_context *ctx HD_UNUSED, hd_stream *stm)
{
    stm->avail = 0;
}

static inline int hd_is_eof_bits(hd_context *ctx, hd_stream *stm)
{
    return hd_is_eof(ctx, stm) && (stm->avail == 0 || stm->bits == EOF);
}
#endif //HDCONTENTS_HDTD_STREAM_H
