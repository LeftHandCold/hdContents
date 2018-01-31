//
// Created by sjw on 2018/1/29.
//
#include "hdtd.h"
#include "pdf.h"

/*
 * Create and destroy
 */

pdf_font_desc *
pdf_keep_font(hd_context *ctx, pdf_font_desc *fontdesc)
{
    return hd_keep_storable(ctx, &fontdesc->storable);
}

void
pdf_drop_font(hd_context *ctx, pdf_font_desc *fontdesc)
{
    hd_drop_storable(ctx, &fontdesc->storable);
}

static void
pdf_drop_font_imp(hd_context *ctx, hd_storable *fontdesc_)
{
    pdf_font_desc *fontdesc = (pdf_font_desc *)fontdesc_;

    pdf_drop_cmap(ctx, fontdesc->encoding);
    //pdf_drop_cmap(ctx, fontdesc->to_ttf_cmap);
    pdf_drop_cmap(ctx, fontdesc->to_unicode);
    //hd_free(ctx, fontdesc->cid_to_gid);
    //hd_free(ctx, fontdesc->cid_to_ucs);
    hd_free(ctx, fontdesc);
}

pdf_font_desc *
pdf_new_font_desc(hd_context *ctx)
{
    pdf_font_desc *fontdesc;

    fontdesc = hd_malloc_struct(ctx, pdf_font_desc);
    fontdesc->size = sizeof(pdf_font_desc);

    fontdesc->flags = 0;

    fontdesc->encoding = NULL;
    fontdesc->to_ttf_cmap = NULL;
    fontdesc->cid_to_gid_len = 0;
    fontdesc->cid_to_gid = NULL;

    fontdesc->to_unicode = NULL;
    fontdesc->cid_to_ucs_len = 0;
    fontdesc->cid_to_ucs = NULL;

    fontdesc->wmode = 0;

    fontdesc->is_embedded = 0;

    return fontdesc;
}

static inline int hd_mini(int a, int b)
{
    return (a < b ? a : b);
}
/*
 * CID Fonts
 */

static pdf_font_desc *
load_cid_font(hd_context *ctx, pdf_document *doc, pdf_obj *dict, pdf_obj *encoding, pdf_obj *to_unicode)
{
    pdf_font_desc *fontdesc = NULL;
    pdf_cmap *cmap;
    char collection[256];
    pdf_obj *obj;

    hd_var(fontdesc);

    hd_try(ctx)
    {

        {
            pdf_obj *cidinfo;
            char tmpstr[64];
            int tmplen;

            cidinfo = pdf_dict_get(ctx, dict, PDF_NAME_CIDSystemInfo);
            if (!cidinfo)
                hd_throw(ctx, HD_ERROR_SYNTAX, "cid font is missing info");

            obj = pdf_dict_get(ctx, cidinfo, PDF_NAME_Registry);
            tmplen = hd_mini(sizeof tmpstr - 1, pdf_to_str_len(ctx, obj));
            memcpy(tmpstr, pdf_to_str_buf(ctx, obj), tmplen);
            tmpstr[tmplen] = '\0';
            hd_strlcpy(collection, tmpstr, sizeof collection);

            hd_strlcat(collection, "-", sizeof collection);

            obj = pdf_dict_get(ctx, cidinfo, PDF_NAME_Ordering);
            tmplen = hd_mini(sizeof tmpstr - 1, pdf_to_str_len(ctx, obj));
            memcpy(tmpstr, pdf_to_str_buf(ctx, obj), tmplen);
            tmpstr[tmplen] = '\0';
            hd_strlcat(collection, tmpstr, sizeof collection);
        }
        /* Encoding */

        if (pdf_is_name(ctx, encoding))
        {
            if (pdf_name_eq(ctx, encoding, PDF_NAME_Identity_H))
                cmap = pdf_new_identity_cmap(ctx, 0, 2);
            else if (pdf_name_eq(ctx, encoding, PDF_NAME_Identity_V))
                cmap = pdf_new_identity_cmap(ctx, 1, 2);
            else
            {
                //TODO:pdf_load_system_cmap
				cmap = NULL;
            }
        }
        else if (pdf_is_indirect(ctx, encoding))
        {
			cmap = pdf_load_embedded_cmap(ctx, doc, encoding);
        }
        else
        {
            hd_throw(ctx, HD_ERROR_GENERIC, "font missing encoding");
        }

        /* Load font file */

        fontdesc = pdf_new_font_desc(ctx);

        fontdesc->encoding = cmap;
        fontdesc->size += pdf_cmap_size(ctx, fontdesc->encoding);

        fontdesc->wmode = pdf_cmap_wmode(ctx, fontdesc->encoding);

        pdf_load_to_unicode(ctx, doc, fontdesc, NULL, collection, to_unicode);

    }
    hd_catch(ctx)
    {
        pdf_drop_font(ctx, fontdesc);
        hd_rethrow(ctx);
    }

    return fontdesc;
}

static pdf_font_desc *
pdf_load_type0_font(hd_context *ctx, pdf_document *doc, pdf_obj *dict)
{
    pdf_obj *dfonts;
    pdf_obj *dfont;
    pdf_obj *subtype;
    pdf_obj *encoding;
    pdf_obj *to_unicode;

    dfonts = pdf_dict_get(ctx, dict, PDF_NAME_DescendantFonts);
    if (!dfonts)
        hd_throw(ctx, HD_ERROR_SYNTAX, "cid font is missing descendant fonts");

    dfont = pdf_array_get(ctx, dfonts, 0);

    subtype = pdf_dict_get(ctx, dfont, PDF_NAME_Subtype);
    encoding = pdf_dict_get(ctx, dict, PDF_NAME_Encoding);
    to_unicode = pdf_dict_get(ctx, dict, PDF_NAME_ToUnicode);

    if (pdf_is_name(ctx, subtype) && pdf_name_eq(ctx, subtype, PDF_NAME_CIDFontType0))
        return load_cid_font(ctx, doc, dfont, encoding, to_unicode);
    if (pdf_is_name(ctx, subtype) && pdf_name_eq(ctx, subtype, PDF_NAME_CIDFontType2))
        return load_cid_font(ctx, doc, dfont, encoding, to_unicode);
    hd_throw(ctx, HD_ERROR_SYNTAX, "unknown cid font type");
}

pdf_font_desc *
pdf_load_font(hd_context *ctx, pdf_document *doc, pdf_obj *rdb, pdf_obj *dict, int nested_depth)
{
    pdf_obj *subtype;
    pdf_font_desc *fontdesc = NULL;

    if ((fontdesc = pdf_find_item(ctx, pdf_drop_font_imp, dict)) != NULL)
    {
        return fontdesc;
    }

    subtype = pdf_dict_get(ctx, dict, PDF_NAME_Subtype);

    hd_var(fontdesc);

    hd_try(ctx)
    {
        if (pdf_name_eq(ctx, subtype, PDF_NAME_Type0))
            fontdesc = pdf_load_type0_font(ctx, doc, dict);
        else if (pdf_name_eq(ctx, subtype, PDF_NAME_TrueType))
        {
            //TODO:pdf_load_simple_font;
            fontdesc = NULL;
        }

    }
    hd_catch(ctx)
    {
        pdf_drop_font(ctx, fontdesc);
        hd_rethrow(ctx);
    }

    if (fontdesc != NULL)
        pdf_store_item(ctx, dict, fontdesc, fontdesc->size);

    return fontdesc;
}