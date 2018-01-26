//
// Created by sjw on 23/01/2018.
//
#include "hdtd.h"
#include "pdf.h"

typedef struct pdf_run_processor_s pdf_run_processor;

struct pdf_run_processor_s
{
	pdf_processor super;
	int clip;
	int clip_even_odd;
};

static void
show_string(hd_context *ctx, pdf_run_processor *pr, unsigned char *buf, int len)
{
    printf("buf is %x-%x-%x-%x\n", buf[0], buf[1], buf[2], buf[3]);
    unsigned char *end = buf + len;
}

static void
pdf_show_string(hd_context *ctx, pdf_run_processor *pr, unsigned char *buf, int len)
{
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

		/* text showing */
		proc->super.op_TJ = pdf_run_TJ;
		proc->super.op_Tj = pdf_run_Tj;

		proc->super.op_END = pdf_run_END;
	}
}
