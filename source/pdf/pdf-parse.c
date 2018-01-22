//
// Created by sjw on 2018/1/19.
//
#include "hdtd.h"
#include "pdf.h"

static pdf_obj *
pdf_new_text_string_utf16be(hd_context *ctx, pdf_document *doc, const char *s)
{
    int c, i = 0, n = hd_utflen(s);
    unsigned char *p = hd_malloc(ctx, n * 2 + 2);
    pdf_obj *obj;
    p[i++] = 254;
    p[i++] = 255;
    while (*s)
    {
        s += hd_chartorune(&c, s);
        p[i++] = (c>>8) & 0xff;
        p[i++] = (c) & 0xff;
    }
    hd_try(ctx)
    obj = pdf_new_string(ctx, doc, (char*)p, i);
    hd_always(ctx)
    hd_free(ctx, p);
    hd_catch(ctx)
    hd_rethrow(ctx);
    return obj;
}

/*
 * Create a PDF 'text string' by encoding input string as either ASCII or UTF-16BE.
 * In theory, we could also use PDFDocEncoding.
 */
pdf_obj *
pdf_new_text_string(hd_context *ctx, pdf_document *doc, const char *s)
{
    int i = 0;
    while (s[i] != 0)
    {
        if (((unsigned char)s[i]) >= 128)
            return pdf_new_text_string_utf16be(ctx, doc, s);
        ++i;
    }
    return pdf_new_string(ctx, doc, s, i);
}

pdf_obj *
pdf_parse_array(hd_context *ctx, pdf_document *doc, hd_stream *file, pdf_lexbuf *buf)
{
	pdf_obj *ary = NULL;
	pdf_obj *obj = NULL;
	int64_t a = 0, b = 0, n = 0;
	pdf_token tok;
	pdf_obj *op = NULL;

	hd_var(obj);

	ary = pdf_new_array(ctx, doc, 4);

	hd_try(ctx)
	{
		while (1)
		{
			tok = pdf_lex(ctx, file, buf);

			if (tok != PDF_TOK_INT && tok != PDF_TOK_R)
			{
				if (n > 0)
				{
					obj = pdf_new_int(ctx, doc, a);
					pdf_array_push_drop(ctx, ary, obj);
				}
				if (n > 1)
				{
					obj = pdf_new_int(ctx, doc, b);
					pdf_array_push_drop(ctx, ary, obj);
				}
				n = 0;
			}

			if (tok == PDF_TOK_INT && n == 2)
			{
				obj = pdf_new_int(ctx, doc, a);
				pdf_array_push_drop(ctx, ary, obj);
				a = b;
				n --;
			}

			switch (tok)
			{
				case PDF_TOK_CLOSE_ARRAY:
					op = ary;
					goto end;

				case PDF_TOK_INT:
					if (n == 0)
						a = buf->i;
					if (n == 1)
						b = buf->i;
					n ++;
					break;

				case PDF_TOK_R:
					if (n != 2)
						hd_throw(ctx, HD_ERROR_SYNTAX, "cannot parse indirect reference in array");
					obj = pdf_new_indirect(ctx, doc, a, b);
					pdf_array_push_drop(ctx, ary, obj);
					n = 0;
					break;

				case PDF_TOK_OPEN_ARRAY:
					obj = pdf_parse_array(ctx, doc, file, buf);
					pdf_array_push_drop(ctx, ary, obj);
					break;

				case PDF_TOK_OPEN_DICT:
					obj = pdf_parse_dict(ctx, doc, file, buf);
					pdf_array_push_drop(ctx, ary, obj);
					break;

				case PDF_TOK_NAME:
					obj = pdf_new_name(ctx, doc, buf->scratch);
					pdf_array_push_drop(ctx, ary, obj);
					break;
				case PDF_TOK_REAL:
					obj = pdf_new_real(ctx, doc, buf->f);
					pdf_array_push_drop(ctx, ary, obj);
					break;
				case PDF_TOK_STRING:
					obj = pdf_new_string(ctx, doc, buf->scratch, buf->len);
					pdf_array_push_drop(ctx, ary, obj);
					break;
				case PDF_TOK_TRUE:
					obj = pdf_new_bool(ctx, doc, 1);
					pdf_array_push_drop(ctx, ary, obj);
					break;
				case PDF_TOK_FALSE:
					obj = pdf_new_bool(ctx, doc, 0);
					pdf_array_push_drop(ctx, ary, obj);
					break;
				case PDF_TOK_NULL:
					obj = pdf_new_null(ctx, doc);
					pdf_array_push_drop(ctx, ary, obj);
					break;

				default:
					pdf_array_push_drop(ctx, ary, pdf_new_null(ctx, doc));
					break;
			}
		}
		end:
		{}
	}
	hd_catch(ctx)
	{
		pdf_drop_obj(ctx, ary);
		hd_rethrow(ctx);
	}
	return op;
}

pdf_obj *
pdf_parse_dict(hd_context *ctx, pdf_document *doc, hd_stream *file, pdf_lexbuf *buf)
{
    pdf_obj *dict;
    pdf_obj *key = NULL;
    pdf_obj *val = NULL;
    pdf_token tok;
    int64_t a, b;

    dict = pdf_new_dict(ctx, doc, 8);

    hd_var(key);
    hd_var(val);

    hd_try(ctx)
    {
        int s = 1;
        while (1)
        {
            s--;
            tok = pdf_lex(ctx, file, buf);
            skip:
            if (tok == PDF_TOK_CLOSE_DICT)
                break;

            /* for BI .. ID .. EI in content streams */
            if (tok == PDF_TOK_KEYWORD && !strcmp(buf->scratch, "ID"))
                break;

            if (tok != PDF_TOK_NAME)
            {
                hd_throw(ctx, HD_ERROR_SYNTAX, "invalid key in dict");
            }

            key = pdf_new_name(ctx, doc, buf->scratch);

            tok = pdf_lex(ctx, file, buf);

            switch (tok)
            {
                case PDF_TOK_OPEN_ARRAY:
                    val = pdf_parse_array(ctx, doc, file, buf);
                    break;

                case PDF_TOK_OPEN_DICT:
                    val = pdf_parse_dict(ctx, doc, file, buf);
                    break;

                case PDF_TOK_NAME: val = pdf_new_name(ctx, doc, buf->scratch); break;
                case PDF_TOK_REAL: val = pdf_new_real(ctx, doc, buf->f); break;
                case PDF_TOK_STRING: val = pdf_new_string(ctx, doc, buf->scratch, buf->len); break;
                case PDF_TOK_TRUE: val = pdf_new_bool(ctx, doc, 1); break;
                case PDF_TOK_FALSE: val = pdf_new_bool(ctx, doc, 0); break;
                case PDF_TOK_NULL: val = pdf_new_null(ctx, doc); break;

                case PDF_TOK_INT:
                    /* 64-bit to allow for numbers > INT_MAX and overflow */
                    a = buf->i;
                    tok = pdf_lex(ctx, file, buf);
                    if (tok == PDF_TOK_CLOSE_DICT || tok == PDF_TOK_NAME ||
                        (tok == PDF_TOK_KEYWORD && !strcmp(buf->scratch, "ID")))
                    {
                        val = pdf_new_int(ctx, doc, a);
                        pdf_dict_put(ctx, dict, key, val);
                        pdf_drop_obj(ctx, val);
                        val = NULL;
                        pdf_drop_obj(ctx, key);
                        key = NULL;
                        goto skip;
                    }
                    if (tok == PDF_TOK_INT)
                    {
                        b = buf->i;
                        tok = pdf_lex(ctx, file, buf);
                        if (tok == PDF_TOK_R)
                        {
                            val = pdf_new_indirect(ctx, doc, a, b);
                            break;
                        }
                    }
                    hd_warn(ctx, "invalid indirect reference in dict");
                    val = pdf_new_null(ctx, doc);
                    break;

                default:
                    val = pdf_new_null(ctx, doc);
                    break;
            }

            pdf_dict_put(ctx, dict, key, val);
            pdf_drop_obj(ctx, val);
            val = NULL;
            pdf_drop_obj(ctx, key);
            key = NULL;
        }
    }
    hd_catch(ctx)
    {
        pdf_drop_obj(ctx, dict);
        pdf_drop_obj(ctx, key);
        pdf_drop_obj(ctx, val);
        hd_rethrow(ctx);
    }
    return dict;
}