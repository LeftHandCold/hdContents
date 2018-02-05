//
// Created by sjw on 2018/1/22.
//
#include "hdtd.h"
#include "pdf.h"


pdf_obj *
pdf_page_contents(hd_context *ctx, pdf_page *page)
{
    return pdf_dict_get(ctx, page->obj, PDF_NAME_Contents);
}

static void
pdf_run_page_contents_with_usage(hd_context *ctx, pdf_document *doc, pdf_page *page)
{
    pdf_obj *resources;
    pdf_obj *contents;
    pdf_processor *proc = NULL;

    hd_var(proc);

    hd_try(ctx)
    {
        resources = pdf_page_resources(ctx, page);
        contents = pdf_page_contents(ctx, page);

        proc = pdf_new_run_processor(ctx);
        pdf_process_contents(ctx, proc, doc, resources, contents);
        pdf_close_processor(ctx, proc);
    }
    hd_always(ctx)
    {
        pdf_drop_processor(ctx, proc);
    }
    hd_catch(ctx)
        hd_rethrow(ctx);
}

void pdf_run_page_contents(hd_context *ctx, pdf_page *page, char* buf)
{
    pdf_document *doc = page->doc;

    hd_try(ctx)
    {
        pdf_run_page_contents_with_usage(ctx, doc, page);
        if (ctx->flush_size > 1)
            memcpy(buf, ctx->contents, ctx->flush_size);
    }
    hd_catch(ctx)
    {
        hd_rethrow(ctx);
    }
}