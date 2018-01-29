//
// Created by sjw on 2018/1/29.
//

#include "hdtd.h"
#include "pdf.h"

/* Load or synthesize ToUnicode map for fonts */

static void
pdf_remap_cmap_range(hd_context *ctx, pdf_cmap *ucs_from_gid,
                     unsigned int cpt, unsigned int gid, unsigned int n, pdf_cmap *ucs_from_cpt)
{
    unsigned int k;
    int ucsbuf[8];
    int ucslen;

    for (k = 0; k <= n; ++k)
    {
        ucslen = pdf_lookup_cmap_full(ucs_from_cpt, cpt + k, ucsbuf);
        if (ucslen == 1)
            pdf_map_range_to_range(ctx, ucs_from_gid, gid + k, gid + k, ucsbuf[0]);
        else if (ucslen > 1)
            pdf_map_one_to_many(ctx, ucs_from_gid, gid + k, ucsbuf, ucslen);
    }
}

static pdf_cmap *
pdf_remap_cmap(hd_context *ctx, pdf_cmap *gid_from_cpt, pdf_cmap *ucs_from_cpt)
{
    pdf_cmap *ucs_from_gid;
    unsigned int a, b, x;
    int i;

    ucs_from_gid = pdf_new_cmap(ctx);

    hd_try(ctx)
    {
        if (gid_from_cpt->usecmap)
            ucs_from_gid->usecmap = pdf_remap_cmap(ctx, gid_from_cpt->usecmap, ucs_from_cpt);

        for (i = 0; i < gid_from_cpt->rlen; ++i)
        {
            a = gid_from_cpt->ranges[i].low;
            b = gid_from_cpt->ranges[i].high;
            x = gid_from_cpt->ranges[i].out;
            pdf_remap_cmap_range(ctx, ucs_from_gid, a, x, b - a, ucs_from_cpt);
        }

        for (i = 0; i < gid_from_cpt->xlen; ++i)
        {
            a = gid_from_cpt->xranges[i].low;
            b = gid_from_cpt->xranges[i].high;
            x = gid_from_cpt->xranges[i].out;
            pdf_remap_cmap_range(ctx, ucs_from_gid, a, x, b - a, ucs_from_cpt);
        }

        /* Font encoding CMaps don't have one-to-many mappings, so we can ignore the mranges. */

        pdf_sort_cmap(ctx, ucs_from_gid);
    }
    hd_catch(ctx)
    {
        pdf_drop_cmap(ctx, ucs_from_gid);
        hd_rethrow(ctx);
    }

    return ucs_from_gid;
}

void
pdf_load_to_unicode(hd_context *ctx, pdf_document *doc, pdf_font_desc *font,
                    const char **strings, char *collection, pdf_obj *cmapstm)
{
    unsigned int cpt;

    if (pdf_is_stream(ctx, cmapstm))
    {
        pdf_cmap *ucs_from_cpt = pdf_load_embedded_cmap(ctx, doc, cmapstm);
        font->to_unicode = pdf_remap_cmap(ctx, font->encoding, ucs_from_cpt);
        pdf_drop_cmap(ctx, ucs_from_cpt);
        font->size += pdf_cmap_size(ctx, font->to_unicode);
    }

    else if (collection)
    {
        //TODO:pdf_load_system_cmap

        return;
    }

    if (strings)
    {
        /* TODO one-to-many mappings */

    }

    if (!font->to_unicode && !font->cid_to_ucs)
    {
        /* TODO: synthesize a ToUnicode if it's a freetype font with
         * cmap and/or post tables or if it has glyph names. */
    }
}