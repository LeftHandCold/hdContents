//
// Created by sjw on 2018/1/26.
//

#ifndef HDCONTENTS_HDTD_COMPRESSED_BUFFER_H
#define HDCONTENTS_HDTD_COMPRESSED_BUFFER_H

#include "hdtd/system.h"
#include "hdtd/context.h"
#include "hdtd/buffer.h"
#include "hdtd/stream.h"

typedef struct hd_compression_params_s hd_compression_params;

typedef struct hd_compressed_buffer_s hd_compressed_buffer;
size_t hd_compressed_buffer_size(hd_compressed_buffer *buffer);

hd_stream *hd_open_compressed_buffer(hd_context *ctx, hd_compressed_buffer *);
hd_stream *hd_open_image_decomp_stream_from_buffer(hd_context *ctx, hd_compressed_buffer *, int *l2factor);
hd_stream *hd_open_image_decomp_stream(hd_context *ctx, hd_stream *, hd_compression_params *, int *l2factor);

enum
{
    HD_IMAGE_UNKNOWN = 0,

    /* Uncompressed samples */
            HD_IMAGE_RAW,

    /* Compressed samples */
            HD_IMAGE_FAX,
    HD_IMAGE_FLATE,
    HD_IMAGE_LZW,
    HD_IMAGE_RLD,

    HD_IMAGE_JPEG,
};

struct hd_compression_params_s
{
    int type;
    union {
        struct {
            int color_transform; /* Use -1 for unset */
        } jpeg;
        struct {
            int smask_in_data;
        } jpx;
        struct {
            int columns;
            int rows;
            int k;
            int end_of_line;
            int encoded_byte_align;
            int end_of_block;
            int black_is_1;
            int damaged_rows_before_error;
        } fax;
        struct
        {
            int columns;
            int colors;
            int predictor;
            int bpc;
        }
                flate;
        struct
        {
            int columns;
            int colors;
            int predictor;
            int bpc;
            int early_change;
        } lzw;
    } u;
};

struct hd_compressed_buffer_s
{
    hd_compression_params params;
    hd_buffer *buffer;
};

void hd_drop_compressed_buffer(hd_context *ctx, hd_compressed_buffer *buf);

#endif //HDCONTENTS_HDTD_COMPRESSED_BUFFER_H
