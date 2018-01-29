//
// Created by sjw on 2018/1/29.
//

#include "hdtd.h"
#include "pdf.h"

size_t
pdf_cmap_size(hd_context *ctx, pdf_cmap *cmap)
{
    if (cmap == NULL)
        return 0;

    return pdf_cmap_size(ctx, cmap->usecmap) +
           cmap->rcap * sizeof *cmap->ranges +
           cmap->xcap * sizeof *cmap->xranges +
           cmap->mcap * sizeof *cmap->mranges;
}

/*
 * Load CMap stream in PDF file
 */
pdf_cmap *
pdf_load_embedded_cmap(hd_context *ctx, pdf_document *doc, pdf_obj *stmobj)
{
    hd_stream *file = NULL;
    pdf_cmap *cmap = NULL;
    pdf_obj *obj;

    hd_var(file);
    hd_var(cmap);

    hd_try(ctx)
    {
        file = pdf_open_stream(ctx, stmobj);
        cmap = pdf_load_cmap(ctx, file);

    }
    hd_always(ctx)
    {
        hd_drop_stream(ctx, file);
    }
    hd_catch(ctx)
    {
        pdf_drop_cmap(ctx, cmap);
        hd_rethrow(ctx);
    }

    return cmap;
}

/*
 * Create an Identity-* CMap (for both 1 and 2-byte encodings)
 */
pdf_cmap *
pdf_new_identity_cmap(hd_context *ctx, int wmode, int bytes)
{
    pdf_cmap *cmap = pdf_new_cmap(ctx);
    hd_try(ctx)
    {
        unsigned int high = (1 << (bytes * 8)) - 1;
        if (wmode)
            hd_strlcpy(cmap->cmap_name, "Identity-V", sizeof cmap->cmap_name);
        else
            hd_strlcpy(cmap->cmap_name, "Identity-H", sizeof cmap->cmap_name);
        pdf_add_codespace(ctx, cmap, 0, high, bytes);
        pdf_map_range_to_range(ctx, cmap, 0, high, 0);
        pdf_sort_cmap(ctx, cmap);
        pdf_set_cmap_wmode(ctx, cmap, wmode);
    }
    hd_catch(ctx)
    {
        pdf_drop_cmap(ctx, cmap);
        hd_rethrow(ctx);
    }
    return cmap;
}

/*
 * Load predefined CMap from system.
 */
pdf_cmap *
pdf_load_system_cmap(hd_context *ctx, const char *cmap_name)
{
    pdf_cmap *cmap;
    return cmap;
}