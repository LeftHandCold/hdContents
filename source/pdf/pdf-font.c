//
// Created by sjw on 2018/1/29.
//
#include "hdtd.h"
#include "pdf.h"

void
pdf_drop_font(hd_context *ctx, pdf_font_desc *fontdesc)
{
    /*pdf_drop_cmap(ctx, fontdesc->encoding);
    pdf_drop_cmap(ctx, fontdesc->to_ttf_cmap);
    pdf_drop_cmap(ctx, fontdesc->to_unicode);*/
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
    pdf_obj *widths;
    pdf_obj *descriptor;
    pdf_font_desc *fontdesc = NULL;
    pdf_cmap *cmap;
    char collection[256];
    const char *basefont;
    int i, k;
    pdf_obj *obj;
    int dw;

    hd_var(fontdesc);

    hd_try(ctx)
    {

        /* Get font name and CID collection */

        basefont = pdf_to_name(ctx, pdf_dict_get(ctx, dict, PDF_NAME_BaseFont));

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
            }
        }
        else if (pdf_is_indirect(ctx, encoding))
        {
            //TODO:pdf_load_embedded_cmap
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

/*
 * FreeType and Rendering glue
 */

enum { UNKNOWN, TYPE1, TRUETYPE };

static pdf_font_desc *
pdf_load_simple_font_by_name(hd_context *ctx, pdf_document *doc, pdf_obj *dict, const char *basefont)
{
    pdf_obj *descriptor;
    pdf_obj *encoding;
    pdf_obj *widths;
    unsigned short *etable = NULL;
    pdf_font_desc *fontdesc = NULL;
    pdf_obj *subtype;
    int symbolic;
    int kind;
    int glyph;

    const char *estrings[256];
    char ebuffer[256][32];
    int i, k, n;
    int fterr;
    int has_lock = 0;

    hd_var(fontdesc);
    hd_var(etable);
    hd_var(has_lock);

    /* Load font file */
    hd_try(ctx)
    {
        fontdesc = pdf_new_font_desc(ctx);

        /* Some chinese documents mistakenly consider WinAnsiEncoding to be codepage 936 */
        if (descriptor && pdf_is_string(ctx, pdf_dict_get(ctx, descriptor, PDF_NAME_FontName)) &&
            !pdf_dict_get(ctx, dict, PDF_NAME_ToUnicode) &&
            pdf_name_eq(ctx, pdf_dict_get(ctx, dict, PDF_NAME_Encoding), PDF_NAME_WinAnsiEncoding) &&
            pdf_to_int(ctx, pdf_dict_get(ctx, descriptor, PDF_NAME_Flags)) == 4)
        {
            char *cp936fonts[] = {
                    "\xCB\xCE\xCC\xE5", "SimSun,Regular",
                    "\xBA\xDA\xCC\xE5", "SimHei,Regular",
                    "\xBF\xAC\xCC\xE5_GB2312", "SimKai,Regular",
                    "\xB7\xC2\xCB\xCE_GB2312", "SimFang,Regular",
                    "\xC1\xA5\xCA\xE9", "SimLi,Regular",
                    NULL
            };
            for (i = 0; cp936fonts[i]; i += 2)
                if (!strcmp(basefont, cp936fonts[i]))
                    break;
            if (cp936fonts[i])
            {
                hd_warn(ctx, "workaround for S22PDF lying about chinese font encodings");
                pdf_drop_font(ctx, fontdesc);
                fontdesc = NULL;
                fontdesc = pdf_new_font_desc(ctx);
                fontdesc->encoding = pdf_load_system_cmap(ctx, "GBK-EUC-H");
                fontdesc->to_unicode = pdf_load_system_cmap(ctx, "Adobe-GB1-UCS2");
                fontdesc->to_ttf_cmap = pdf_load_system_cmap(ctx, "Adobe-GB1-UCS2");
            }
        }
    }
    hd_catch(ctx)
    {
        if (fontdesc && etable != fontdesc->cid_to_gid)
            hd_free(ctx, etable);
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

static pdf_font_desc *
pdf_load_simple_font(hd_context *ctx, pdf_document *doc, pdf_obj *dict)
{
    const char *basefont = pdf_to_name(ctx, pdf_dict_get(ctx, dict, PDF_NAME_BaseFont));
    return pdf_load_simple_font_by_name(ctx, doc, dict, basefont);
}

pdf_font_desc *
pdf_load_font(hd_context *ctx, pdf_document *doc, pdf_obj *rdb, pdf_obj *dict, int nested_depth)
{
    pdf_obj *subtype;
    pdf_obj *dfonts;
    pdf_obj *charprocs;
    pdf_font_desc *fontdesc = NULL;
    int type3 = 0;

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

    //pdf_store_item(ctx, dict, fontdesc, fontdesc->size);

    return fontdesc;
}