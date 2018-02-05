//
// Created by sjw on 2018/1/26.
//

#include "hdtd-imp.h"


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