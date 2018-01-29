//
// Created by sjw on 2018/1/26.
//
#include "hdtd.h"
#include "pdf.h"

/*
 * Check if an object is a stream or not.
 */
int
pdf_obj_num_is_stream(hd_context *ctx, pdf_document *doc, int num)
{
    pdf_xref_entry *entry;

    if (num <= 0 || num >= pdf_xref_len(ctx, doc))
        return 0;

    entry = pdf_cache_object(ctx, doc, num);

    return entry->stm_ofs != 0 || entry->stm_buf;
}

int
pdf_is_stream(hd_context *ctx, pdf_obj *obj)
{
    pdf_document *doc = pdf_get_bound_document(ctx, obj);
    int num = pdf_obj_parent_num(ctx, obj);
    return pdf_obj_num_is_stream(ctx, doc, num);
}
/*
 * Build a filter for reading raw stream data.
 * This is a null filter to constrain reading to the stream length (and to
 * allow for other people accessing the file), followed by a decryption
 * filter.
 *
 * orig_num and orig_gen are used purely to seed the encryption.
 */
static hd_stream *
pdf_open_raw_filter(hd_context *ctx, hd_stream *chain, pdf_document *doc, pdf_obj *stmobj, int num, int *orig_num, int *orig_gen, hd_off_t offset)
{
    pdf_xref_entry *x = NULL;
    hd_stream *chain2;
    int len;

    if (num > 0 && num < pdf_xref_len(ctx, doc))
    {
        x = pdf_get_xref_entry(ctx, doc, num);
        *orig_num = x->num;
        *orig_gen = x->gen;
        if (x->stm_buf)
            return hd_open_buffer(ctx, x->stm_buf);
    }
    else
    {
        /* We only end up here when called from pdf_open_stream_with_offset to parse new format XRef sections. */
        /* New style XRef sections must have generation number 0. */
        *orig_num = num;
        *orig_gen = 0;
    }

    hd_var(chain);

    hd_try(ctx)
    {
        len = pdf_to_int(ctx, pdf_dict_get(ctx, stmobj, PDF_NAME_Length));

        /* don't close chain when we close this filter */
        chain2 = hd_keep_stream(ctx, chain);
        chain = NULL;
        chain = hd_open_null(ctx, chain2, len, offset);

    }
    hd_catch(ctx)
    {
        hd_drop_stream(ctx, chain);
        hd_rethrow(ctx);
    }

    return chain;
}

static void
build_compression_params(hd_context *ctx, pdf_obj *f, pdf_obj *p, hd_compression_params *params)
{
    int predictor = pdf_to_int(ctx, pdf_dict_get(ctx, p, PDF_NAME_Predictor));
    pdf_obj *columns_obj = pdf_dict_get(ctx, p, PDF_NAME_Columns);
    int columns = pdf_to_int(ctx, columns_obj);
    int colors = pdf_to_int(ctx, pdf_dict_get(ctx, p, PDF_NAME_Colors));
    int bpc = pdf_to_int(ctx, pdf_dict_get(ctx, p, PDF_NAME_BitsPerComponent));

    params->type = HD_IMAGE_RAW;

    if (pdf_name_eq(ctx, f, PDF_NAME_FlateDecode) || pdf_name_eq(ctx, f, PDF_NAME_Fl))
    {
        params->type = HD_IMAGE_FLATE;
        params->u.flate.predictor = predictor;
        params->u.flate.columns = columns;
        params->u.flate.colors = colors;
        params->u.flate.bpc = bpc;
    }
    else if (pdf_name_eq(ctx, f, PDF_NAME_LZWDecode) || pdf_name_eq(ctx, f, PDF_NAME_LZW))
    {
        pdf_obj *ec = pdf_dict_get(ctx, p, PDF_NAME_EarlyChange);

        params->type = HD_IMAGE_LZW;
        params->u.lzw.predictor = predictor;
        params->u.lzw.columns = columns;
        params->u.lzw.colors = colors;
        params->u.lzw.bpc = bpc;
        params->u.lzw.early_change = (ec ? pdf_to_int(ctx, ec) : 1);
    }
}

/*
 * Create a filter given a name and param dictionary.
 */
static hd_stream *
build_filter(hd_context *ctx, hd_stream *chain, pdf_document *doc, pdf_obj *f, pdf_obj *p, int num, int gen, hd_compression_params *params)
{
    hd_compression_params local_params;
    hd_stream *tmp;

    hd_var(chain);

    hd_try(ctx)
    {
        if (params == NULL)
            params = &local_params;

        build_compression_params(ctx, f, p, params);

        /* If we were using params we were passed in, and we successfully
         * recognised the image type, we can use the existing filter and
         * shortstop here. */
        if (params != &local_params && params->type != HD_IMAGE_RAW)
            break; /* nothing to do */

        else if (params->type != HD_IMAGE_RAW)
        {
            tmp = chain;
            chain = NULL;
            chain = hd_open_image_decomp_stream(ctx, tmp, params, NULL);
        }

        else if (pdf_name_eq(ctx, f, PDF_NAME_ASCIIHexDecode) || pdf_name_eq(ctx, f, PDF_NAME_AHx))
        {

        }

        else if (pdf_name_eq(ctx, f, PDF_NAME_ASCII85Decode) || pdf_name_eq(ctx, f, PDF_NAME_A85))
        {

        }

        else if (pdf_name_eq(ctx, f, PDF_NAME_JPXDecode))
            break; /* JPX decoding is special cased in the image loading code */

        else
            hd_warn(ctx, "unknown filter name (%s)", pdf_to_name(ctx, f));
    }
    hd_catch(ctx)
    {
        hd_drop_stream(ctx, chain);
        hd_rethrow(ctx);
    }

    return chain;
}

/*
 * Construct a filter to decode a stream, constraining
 * to stream length and decrypting.
 */
static hd_stream *
pdf_open_filter(hd_context *ctx, pdf_document *doc, hd_stream *chain, pdf_obj *stmobj, int num, hd_off_t offset, hd_compression_params *imparams)
{
    pdf_obj *filters;
    pdf_obj *params;
    int orig_num, orig_gen;

    filters = pdf_dict_geta(ctx, stmobj, PDF_NAME_Filter, PDF_NAME_F);
    params = pdf_dict_geta(ctx, stmobj, PDF_NAME_DecodeParms, PDF_NAME_DP);

    chain = pdf_open_raw_filter(ctx, chain, doc, stmobj, num, &orig_num, &orig_gen, offset);

    hd_var(chain);

    hd_try(ctx)
    {
        if (pdf_is_name(ctx, filters))
        {
            hd_stream *chain2 = chain;
            chain = NULL;
            chain = build_filter(ctx, chain2, doc, filters, params, orig_num, orig_gen, imparams);
        }
        else if (pdf_array_len(ctx, filters) > 0)
        {
            //TODO:
        }
    }
    hd_catch(ctx)
    {
        hd_drop_stream(ctx, chain);
        hd_rethrow(ctx);
    }

    return chain;
}

static hd_stream *
pdf_open_image_stream(hd_context *ctx, pdf_document *doc, int num, hd_compression_params *params)
{
    pdf_xref_entry *x;

    if (num <= 0 || num >= pdf_xref_len(ctx, doc))
        hd_throw(ctx, HD_ERROR_GENERIC, "object id out of range (%d 0 R)", num);

    x = pdf_cache_object(ctx, doc, num);
    if (x->stm_ofs == 0 && x->stm_buf == NULL)
        hd_throw(ctx, HD_ERROR_GENERIC, "object is not a stream");

    return pdf_open_filter(ctx, doc, doc->file, x->obj, num, x->stm_ofs, params);
}

hd_stream *
pdf_open_contents_stream(hd_context *ctx, pdf_document *doc, pdf_obj *obj)
{
    int num;

    num = pdf_to_num(ctx, obj);
    if (pdf_is_stream(ctx, obj))
        return pdf_open_image_stream(ctx, doc, num, NULL);

    hd_throw(ctx, HD_ERROR_GENERIC, "pdf object stream missing (%d 0 R)", num);
}

/*
 * Open a stream for reading uncompressed data.
 * Put the opened file in doc->stream.
 * Using doc->file while a stream is open is a Bad idea.
 */
hd_stream *
pdf_open_stream_number(hd_context *ctx, pdf_document *doc, int num)
{
    return pdf_open_image_stream(ctx, doc, num, NULL);
}

hd_stream *pdf_open_stream(hd_context *ctx, pdf_obj *ref)
{
    if (pdf_is_stream(ctx, ref))
        return pdf_open_stream_number(ctx, pdf_get_indirect_document(ctx, ref), pdf_to_num(ctx, ref));
    hd_throw(ctx, HD_ERROR_GENERIC, "object is not a stream");
}