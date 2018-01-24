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

static void
pdf_init_csi(hd_context *ctx, pdf_csi *csi, pdf_document *doc, pdf_obj *rdb, pdf_lexbuf *buf)
{
	memset(csi, 0, sizeof *csi);
	csi->doc = doc;
	csi->rdb = rdb;
	csi->buf = buf;
}

void
pdf_process_contents(hd_context *ctx, pdf_processor *proc, pdf_document *doc, pdf_obj *stmobj)
{
	
}