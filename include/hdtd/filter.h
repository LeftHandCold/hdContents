//
// Created by sjw on 2018/1/26.
//

#ifndef HDCONTENTS_HDTD_FILTER_H
#define HDCONTENTS_HDTD_FILTER_H

#include "hdtd/system.h"
#include "hdtd/context.h"
#include "hdtd/buffer.h"
#include "hdtd/stream.h"

hd_stream *hd_open_copy(hd_context *ctx, hd_stream *chain);
hd_stream *hd_open_null(hd_context *ctx, hd_stream *chain, int len, hd_off_t offset);
hd_stream *hd_open_concat(hd_context *ctx, int max, int pad);

hd_stream *hd_open_flated(hd_context *ctx, hd_stream *chain, int window_bits);
hd_stream *hd_open_predict(hd_context *ctx, hd_stream *chain, int predictor, int columns, int colors, int bpc);

#endif //HDCONTENTS_HDTD_FILTER_H
