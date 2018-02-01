//
// Created by sjw on 23/01/2018.
//
#include "hdtd.h"
#include "pdf.h"

typedef struct pdf_run_processor_s pdf_run_processor;

struct pdf_run_processor_s
{
	pdf_processor super;
	pdf_font_desc *fontdesc;

};

static void
pdf_show_char(hd_context *ctx, pdf_run_processor *pr, int cid)
{
	pdf_font_desc *fontdesc = pr->fontdesc;
	int ucsbuf[8];
	int ucslen;


	ucslen = 0;
	if (fontdesc->to_unicode)
		ucslen = pdf_lookup_cmap_full(fontdesc->to_unicode, cid, ucsbuf);
	if (ucslen == 0 && (size_t)cid < fontdesc->cid_to_ucs_len)
	{
		ucsbuf[0] = fontdesc->cid_to_ucs[cid];
		ucslen = 1;
	}

	if ((ctx->flush_size < 62) && cid > 0 && ucslen > 0)
	{
		wchar_t *wc = (wchar_t *)&ucsbuf[0];
		switch (*wc)
		{
			case '/':
			case '\\':
			case '*':
			case '<':
			case '>':
			case '|':
			case '\'':
			case 0x0D:
			case 0x20:
			case '.':
			case ':':
				break;
			default:
				if (((*wc >= 'a' && *wc <= 'z')
					 || (*wc >= 'A' && *wc <= 'Z')
					 || (*wc >= '0' && *wc <= '9')))
				{
					memcpy(ctx->contents + ctx->flush_size, (wchar_t *)&ucsbuf[0], 2);
					ctx->flush_size += 2;
				}
				else
				{
					if (*wc >= 0x4e00 && *wc <= 0x9fa5)
					{
						memcpy(ctx->contents + ctx->flush_size, (wchar_t *)&ucsbuf[0], 2);
						ctx->flush_size += 2;
					}
				}
				break;
		}
	}
}

static void
show_string(hd_context *ctx, pdf_run_processor *pr, unsigned char *buf, int len)
{
	pdf_font_desc *fontdesc = pr->fontdesc;
	unsigned char *end = buf + len;
	unsigned int cpt;
	int cid;
    if (fontdesc == NULL)
    {
		//TODO:Temporarily think *buf is English char*
		for (int i = 0; i < len; ++i)
		{
			wchar_t *wc = (wchar_t *)&buf[i];
			switch (*wc)
			{
				case '/':
				case '\\':
				case '*':
				case '<':
				case '>':
				case '|':
				case '\'':
				case 0x0D:
				case 0x20:
				case '.':
				case ':':
					break;
				default:
					if (((*wc >= 'a' && *wc <= 'z')
						 || (*wc >= 'A' && *wc <= 'Z')
						 || (*wc >= '0' && *wc <= '9')))
					{
						memcpy(ctx->contents + ctx->flush_size, (wchar_t *)&buf[i], 2);
						ctx->flush_size += 2;
					}
					break;
			}
		}

        return;
    }

	while (buf < end)
	{

		int w = pdf_decode_cmap(fontdesc->encoding, buf, end, &cpt);
		buf += w;

		cid = pdf_lookup_cmap(fontdesc->encoding, cpt);
		if (cid >= 0)
			pdf_show_char(ctx, pr, cid);
		else
			hd_warn(ctx, "cannot encode character");
	}
}

static void
pdf_show_string(hd_context *ctx, pdf_run_processor *pr, unsigned char *buf, int len)
{
	pdf_font_desc *fontdesc = pr->fontdesc;

	if (!fontdesc)
	{
		hd_warn(ctx, "cannot draw text since font and size not set");
		return;
	}

	show_string(ctx, pr, buf, len);
}

static void
pdf_show_text(hd_context *ctx, pdf_run_processor *pr, pdf_obj *text)
{
	int i;

	if (pdf_is_array(ctx, text))
	{
		int n = pdf_array_len(ctx, text);
		for (i = 0; i < n; i++)
		{
			pdf_obj *item = pdf_array_get(ctx, text, i);
			if (pdf_is_string(ctx, item))
				show_string(ctx, pr, (unsigned char *)pdf_to_str_buf(ctx, item), pdf_to_str_len(ctx, item));
		}
	}
	else if (pdf_is_string(ctx, text))
	{
		pdf_show_string(ctx, pr, (unsigned char *)pdf_to_str_buf(ctx, text), pdf_to_str_len(ctx, text));
	}
}

/* text objects */

static void pdf_run_BT(hd_context *ctx, pdf_processor *proc)
{

}

static void pdf_run_ET(hd_context *ctx, pdf_processor *proc)
{

}

static void pdf_run_Tf(hd_context *ctx, pdf_processor *proc, const char *name, pdf_font_desc *font, float size)
{
	pdf_run_processor *pr = (pdf_run_processor *)proc;
	pr->fontdesc = font;
}

/* text showing */

static void pdf_run_TJ(hd_context *ctx, pdf_processor *proc, pdf_obj *obj)
{
	pdf_run_processor *pr = (pdf_run_processor *)proc;
	pdf_show_text(ctx, pr, obj);
}

static void pdf_run_Tj(hd_context *ctx, pdf_processor *proc, char *string, int string_len)
{
	pdf_run_processor *pr = (pdf_run_processor *)proc;
	pdf_show_string(ctx, pr, (unsigned char *)string, string_len);
}

static void pdf_run_END(hd_context *ctx, pdf_processor *proc)
{
}

static void
pdf_drop_run_processor(hd_context *ctx, pdf_processor *proc)
{
	pdf_run_processor *pr = (pdf_run_processor *)proc;
	pdf_drop_font(ctx, pr->fontdesc);
}

pdf_processor *
pdf_new_run_processor(hd_context *ctx, const char *usage, int nested)
{
	pdf_run_processor *proc = pdf_new_processor(ctx, sizeof *proc);
	{
		proc->super.usage = usage;
		proc->super.drop_processor = pdf_drop_run_processor;

		/* text objects */
		proc->super.op_BT = pdf_run_BT;
		proc->super.op_ET = pdf_run_ET;

        proc->super.op_Tf = pdf_run_Tf;

		/* text showing */
		proc->super.op_TJ = pdf_run_TJ;
		proc->super.op_Tj = pdf_run_Tj;

		proc->super.op_END = pdf_run_END;
	}

	return (pdf_processor*)proc;
}
