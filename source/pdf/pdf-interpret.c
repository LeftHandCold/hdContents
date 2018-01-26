//
// Created by sjw on 23/01/2018.
//
#include "hdtd.h"
#include "pdf.h"

void *
pdf_new_processor(hd_context *ctx, int size)
{
	return Memento_label(hd_calloc(ctx, 1, size), "pdf_processor");
}

void
pdf_close_processor(hd_context *ctx, pdf_processor *proc)
{
	if (proc && proc->close_processor)
	{
		proc->close_processor(ctx, proc);
		proc->close_processor = NULL;
	}
}

void
pdf_drop_processor(hd_context *ctx, pdf_processor *proc)
{
	if (proc)
	{
		if (proc->close_processor)
			hd_warn(ctx, "dropping unclosed PDF processor");
		if (proc->drop_processor)
			proc->drop_processor(ctx, proc);
	}
	hd_free(ctx, proc);
}

#define A(a) (a)
#define B(a,b) (a | b << 8)
#define C(a,b,c) (a | b << 8 | c << 16)

static void
pdf_process_keyword(hd_context *ctx, pdf_processor *proc, pdf_csi *csi, hd_stream *stm, char *word)
{
    float *s = csi->stack;
    int key;

    key = word[0];
    if (word[1])
    {
        key |= word[1] << 8;
        if (word[2])
        {
            key |= word[2] << 16;
            if (word[3])
                key = 0;
        }
    }

    switch (key)
    {
        /* text objects */
        case B('B','T'): csi->in_text = 1; if (proc->op_BT) proc->op_BT(ctx, proc); break;
        case B('E','T'): csi->in_text = 0; if (proc->op_ET) proc->op_ET(ctx, proc); break;

        /* text showing */
        case B('T','J'): if (proc->op_TJ) proc->op_TJ(ctx, proc, csi->obj); break;
        case B('T','j'):
            if (proc->op_Tj)
            {
                if (csi->string_len > 0)
                    proc->op_Tj(ctx, proc, csi->string, csi->string_len);
                else
                    proc->op_Tj(ctx, proc, pdf_to_str_buf(ctx, csi->obj), pdf_to_str_len(ctx, csi->obj));
            }
            break;
    }
}
static void
pdf_init_csi(hd_context *ctx, pdf_csi *csi, pdf_document *doc, pdf_lexbuf *buf)
{
	memset(csi, 0, sizeof *csi);
	csi->doc = doc;
	csi->buf = buf;
}

static void
pdf_clear_stack(hd_context *ctx, pdf_csi *csi)
{
    int i;

    pdf_drop_obj(ctx, csi->obj);
    csi->obj = NULL;

    csi->name[0] = 0;
    csi->string_len = 0;
    for (i = 0; i < csi->top; i++)
        csi->stack[i] = 0;

    csi->top = 0;
}

static void
pdf_process_stream(hd_context *ctx, pdf_processor *proc, pdf_csi *csi, hd_stream *stm)
{
    pdf_document *doc = csi->doc;
    pdf_lexbuf *buf = csi->buf;

    pdf_token tok = PDF_TOK_ERROR;
    //whether to enter []
    int in_text_array = 0;
    int syntax_errors = 0;

    /* make sure we have a clean slate if we come here from flush_text */
    pdf_clear_stack(ctx, csi);

    hd_var(in_text_array);
    hd_var(tok);

    do
    {
        hd_try(ctx)
        {
            do
            {
                tok = pdf_lex(ctx, stm, buf);
                if (in_text_array)
                {
                    switch (tok)
                    {
                        case PDF_TOK_CLOSE_ARRAY:
                            in_text_array = 0;
                            break;
                        case PDF_TOK_REAL:
                            pdf_array_push_drop(ctx, csi->obj, pdf_new_real(ctx, doc, buf->f));
                            break;
                        case PDF_TOK_INT:
                            pdf_array_push_drop(ctx, csi->obj, pdf_new_int(ctx, doc, buf->i));
                            break;
                        case PDF_TOK_STRING:
                            pdf_array_push_drop(ctx, csi->obj, pdf_new_string(ctx, doc, buf->scratch, buf->len));
                            break;
                        case PDF_TOK_EOF:
                            break;
                        case PDF_TOK_KEYWORD:
                            if (buf->scratch[0] == 'T' && (buf->scratch[1] == 'w' || buf->scratch[1] == 'c') && buf->scratch[2] == 0)
                            {
                                int n = pdf_array_len(ctx, csi->obj);
                                if (n > 0)
                                {
                                    pdf_obj *o = pdf_array_get(ctx, csi->obj, n-1);
                                    if (pdf_is_number(ctx, o))
                                    {
                                        csi->stack[0] = pdf_to_real(ctx, o);
                                        pdf_array_delete(ctx, csi->obj, n-1);
                                        pdf_process_keyword(ctx, proc, csi, stm, buf->scratch);
                                    }
                                }
                            }
                            /* Deliberate Fallthrough! */
                        default:
                            hd_throw(ctx, HD_ERROR_SYNTAX, "syntax error in array");
                    }
                }
                else switch (tok)
                {
                    case PDF_TOK_ENDSTREAM:
                    case PDF_TOK_EOF:
                        tok = PDF_TOK_EOF;
                        break;

                    case PDF_TOK_OPEN_ARRAY:
                        if (csi->obj)
                        {
                            pdf_drop_obj(ctx, csi->obj);
                            csi->obj = NULL;
                        }
                        if (csi->in_text)
                        {
                            in_text_array = 1;
                            csi->obj = pdf_new_array(ctx, doc, 4);
                        }
                        else
                        {
                            csi->obj = pdf_parse_array(ctx, doc, stm, buf);
                        }
                        break;

                    case PDF_TOK_OPEN_DICT:
                        if (csi->obj)
                        {
                            pdf_drop_obj(ctx, csi->obj);
                            csi->obj = NULL;
                        }
                        csi->obj = pdf_parse_dict(ctx, doc, stm, buf);
                        break;

                    case PDF_TOK_NAME:
                        if (csi->name[0])
                        {
                            pdf_drop_obj(ctx, csi->obj);
                            csi->obj = NULL;
                            csi->obj = pdf_new_name(ctx, doc, buf->scratch);
                        }
                        else
                            hd_strlcpy(csi->name, buf->scratch, sizeof(csi->name));
                        break;

                    case PDF_TOK_INT:
                        if (csi->top < nelem(csi->stack)) {
                            csi->stack[csi->top] = buf->i;
                            csi->top ++;
                        }
                        else
                            hd_throw(ctx, HD_ERROR_SYNTAX, "stack overflow");
                        break;

                    case PDF_TOK_REAL:
                        if (csi->top < nelem(csi->stack)) {
                            csi->stack[csi->top] = buf->f;
                            csi->top ++;
                        }
                        else
                            hd_throw(ctx, HD_ERROR_SYNTAX, "stack overflow");
                        break;

                    case PDF_TOK_STRING:
                        if (buf->len <= sizeof(csi->string))
                        {
                            memcpy(csi->string, buf->scratch, buf->len);
                            csi->string_len = buf->len;
                        }
                        else
                        {
                            if (csi->obj)
                            {
                                pdf_drop_obj(ctx, csi->obj);
                                csi->obj = NULL;
                            }
                            csi->obj = pdf_new_string(ctx, doc, buf->scratch, buf->len);
                        }
                        break;

                    case PDF_TOK_KEYWORD:
                        pdf_process_keyword(ctx, proc, csi, stm, buf->scratch);
                        pdf_clear_stack(ctx, csi);
                        break;

                    default:
                        hd_throw(ctx, HD_ERROR_SYNTAX, "syntax error in content stream");
                }
            }
            while (tok != PDF_TOK_EOF);
        }
        hd_always(ctx)
        {
            pdf_clear_stack(ctx, csi);
        }
        hd_catch(ctx)
        {

        }
    }
    while (tok != PDF_TOK_EOF);

}

void
pdf_process_contents(hd_context *ctx, pdf_processor *proc, pdf_document *doc, pdf_obj *stmobj)
{
	pdf_csi csi;
	pdf_lexbuf buf;
	hd_stream *stm = NULL;

	if (!stmobj)
		return;

	hd_var(stm);

	pdf_lexbuf_init(ctx, &buf, PDF_LEXBUF_SMALL);
	pdf_init_csi(ctx, &csi, doc, &buf);

	hd_try(ctx)
	{
		stm = pdf_open_contents_stream(ctx, doc, stmobj);
		pdf_process_stream(ctx, proc, &csi, stm);
		//pdf_process_end(ctx, proc, &csi);
	}
	hd_always(ctx)
	{
		hd_drop_stream(ctx, stm);
		pdf_clear_stack(ctx, &csi);
		pdf_lexbuf_fin(ctx, &buf);
	}
	hd_catch(ctx)
	{
		hd_rethrow(ctx);
	}


}