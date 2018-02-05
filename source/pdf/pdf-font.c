//
// Created by sjw on 2018/1/29.
//
#include "hdtd.h"
#include "pdf.h"

/*
 * Create and destroy
 */

void
pdf_drop_font(hd_context *ctx, pdf_font_desc *fontdesc)
{
    hd_drop_storable(ctx, &fontdesc->storable);
}

static void
pdf_drop_font_imp(hd_context *ctx, hd_storable *fontdesc_)
{
    pdf_font_desc *fontdesc = (pdf_font_desc *)fontdesc_;


	if (fontdesc->encoding != NULL)
    	pdf_drop_cmap(ctx, fontdesc->encoding);
	if (fontdesc->to_unicode != NULL)
    	pdf_drop_cmap(ctx, fontdesc->to_unicode);
	if (fontdesc->cid_to_ucs != NULL)
    	hd_free(ctx, fontdesc->cid_to_ucs);
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

    fontdesc->to_unicode = NULL;
    fontdesc->cid_to_ucs_len = 0;
    fontdesc->cid_to_ucs = NULL;

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
                hd_throw(ctx, HD_ERROR_GENERIC, "pdf_load_system_cmap is TODO");
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

    const char *estrings[256];
    int i, k, n;

    hd_var(fontdesc);

    /* Load font file */
    hd_try(ctx)
    {
        fontdesc = pdf_new_font_desc(ctx);

        descriptor = pdf_dict_get(ctx, dict, PDF_NAME_FontDescriptor);
        fontdesc->flags = pdf_to_int(ctx, pdf_dict_get(ctx, descriptor, PDF_NAME_Flags));

        etable = hd_malloc_array(ctx, 256, sizeof(unsigned short));
        fontdesc->size += 256 * sizeof(unsigned short);
        for (i = 0; i < 256; i++)
        {
            estrings[i] = NULL;
            etable[i] = 0;
        }

        symbolic = fontdesc->flags & 4;
        encoding = pdf_dict_get(ctx, dict, PDF_NAME_Encoding);
        if (encoding)
        {
            if (pdf_is_name(ctx, encoding))
                pdf_load_encoding(estrings, pdf_to_name(ctx, encoding));

            if (pdf_is_dict(ctx, encoding))
            {
                pdf_obj *base, *diff, *item;

                base = pdf_dict_get(ctx, encoding, PDF_NAME_BaseEncoding);
                if (pdf_is_name(ctx, base))
                    pdf_load_encoding(estrings, pdf_to_name(ctx, base));
                else if (!fontdesc->is_embedded && !symbolic)
                    pdf_load_encoding(estrings, "StandardEncoding");

                diff = pdf_dict_get(ctx, encoding, PDF_NAME_Differences);
                if (pdf_is_array(ctx, diff))
                {
                    n = pdf_array_len(ctx, diff);
                    k = 0;
                    for (i = 0; i < n; i++)
                    {
                        item = pdf_array_get(ctx, diff, i);
                        if (pdf_is_int(ctx, item))
                            k = pdf_to_int(ctx, item);
                        if (pdf_is_name(ctx, item) && k >= 0 && k < nelem(estrings))
                            estrings[k++] = pdf_to_name(ctx, item);
                    }
                }
            }
        }
        else if (!fontdesc->is_embedded && !symbolic)
            pdf_load_encoding(estrings, "StandardEncoding");

        fontdesc->encoding = pdf_new_identity_cmap(ctx, 0, 1);
        fontdesc->size += pdf_cmap_size(ctx, fontdesc->encoding);

        hd_try(ctx)
        {
            pdf_load_to_unicode(ctx, doc, fontdesc, estrings, NULL, pdf_dict_get(ctx, dict, PDF_NAME_ToUnicode));
        }
        hd_catch(ctx)
        {
            hd_rethrow_if(ctx, HD_ERROR_TRYLATER);
            hd_warn(ctx, "cannot load ToUnicode CMap");
        }

    }
    hd_catch(ctx)
    {
        pdf_drop_font(ctx, fontdesc);
        hd_rethrow(ctx);
    }

    return fontdesc;
}

static pdf_font_desc *
pdf_load_simple_font(hd_context *ctx, pdf_document *doc, pdf_obj *dict)
{
    const char *basefont = pdf_to_name(ctx, pdf_dict_get(ctx, dict, PDF_NAME_BaseFont));
    return pdf_load_simple_font_by_name(ctx, doc, dict, basefont);
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
    pdf_obj *dfonts;
    pdf_obj *charprocs;
    pdf_font_desc *fontdesc = NULL;

    if ((fontdesc = pdf_find_item(ctx, pdf_drop_font_imp, dict)) != NULL)
    {
        return fontdesc;
    }

    subtype = pdf_dict_get(ctx, dict, PDF_NAME_Subtype);
    dfonts = pdf_dict_get(ctx, dict, PDF_NAME_DescendantFonts);
    charprocs = pdf_dict_get(ctx, dict, PDF_NAME_CharProcs);

    hd_var(fontdesc);

    hd_try(ctx)
    {
        //TODO:pdf_load_type3_font;
        if (pdf_name_eq(ctx, subtype, PDF_NAME_Type0))
            fontdesc = pdf_load_type0_font(ctx, doc, dict);
        else if (pdf_name_eq(ctx, subtype, PDF_NAME_Type1))
            fontdesc = pdf_load_simple_font(ctx, doc, dict);
        else if (pdf_name_eq(ctx, subtype, PDF_NAME_MMType1))
            fontdesc = pdf_load_simple_font(ctx, doc, dict);
        else if (pdf_name_eq(ctx, subtype, PDF_NAME_TrueType))
            fontdesc = pdf_load_simple_font(ctx, doc, dict);
        else if (pdf_name_eq(ctx, subtype, PDF_NAME_Type3))
        {
            fontdesc = NULL;
        }
        else if (charprocs)
        {
            hd_warn(ctx, "unknown font format, guessing type3.");
            fontdesc = NULL;
        }
        else if (dfonts)
        {
            hd_warn(ctx, "unknown font format, guessing type0.");
            fontdesc = pdf_load_type0_font(ctx, doc, dict);
        }
        else
        {
            hd_warn(ctx, "unknown font format, guessing type1 or truetype.");
            fontdesc = pdf_load_simple_font(ctx, doc, dict);
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