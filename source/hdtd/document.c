//
// Created by sjw on 2018/1/15.
//
#include "hdtd.h"
#include "hdtd-imp.h"

enum
{
    HD_DOCUMENT_HANDLER_MAX = 10
};

#define DEFW (450)
#define DEFH (600)
#define DEFEM (12)

struct hd_document_handler_context_s
{
    int refs;
    int count;
    const hd_document_handler *handler[HD_DOCUMENT_HANDLER_MAX];
};

void hd_new_document_handler_context(hd_context *ctx)
{
    ctx->handler = hd_malloc_struct(ctx, hd_document_handler_context);
    ctx->handler->refs = 1;
}

void *
hd_new_document_of_size(hd_context *ctx, int size)
{
    hd_document *doc = hd_calloc(ctx, 1, size);
    doc->refs = 1;
    return doc;
}

void hd_register_document_handler(hd_context *ctx, const hd_document_handler *handler)
{
    hd_document_handler_context *dc;
    int i;

    if (!handler)
        return;

    dc = ctx->handler;
    if (dc == NULL)
        hd_throw(ctx, HD_ERROR_GENERIC, "Document handler list not found");

    for (i = 0; i < dc->count; i++)
        if (dc->handler[i] == handler)
            return;

    if (dc->count >= HD_DOCUMENT_HANDLER_MAX)
        hd_throw(ctx, HD_ERROR_GENERIC, "Too many document handlers");

    dc->handler[dc->count++] = handler;
}

hd_document *
hd_open_document(hd_context *ctx, const char *filename)
{
    int i, score;
    int best_i, best_score;
    hd_document_handler_context *dc;
    hd_stream *file;
    hd_document *doc;

    if (filename == NULL)
        hd_throw(ctx, HD_ERROR_GENERIC, "no document to open");

    dc = ctx->handler;
    if (dc->count == 0)
        hd_throw(ctx, HD_ERROR_GENERIC, "No document handlers registered");

    best_i = -1;
    best_score = 0;
    for (i = 0; i < dc->count; i++)
    {
        score = dc->handler[i]->recognize(ctx, filename);
        if (best_score < score)
        {
            best_score = score;
            best_i = i;
        }
    }

    if (best_i < 0)
        hd_throw(ctx, HD_ERROR_GENERIC, "cannot find document handler for file: '%s'", filename);

    if (dc->handler[best_i]->open)
        return dc->handler[best_i]->open(ctx, filename);

    return NULL;
}

void
hd_drop_document(hd_context *ctx, hd_document *doc)
{
    if (hd_drop_imp(ctx, doc, &doc->refs))
    {
        if (doc->drop_document)
            doc->drop_document(ctx, doc);
        hd_free(ctx, doc);
    }
}

void hd_drop_document_handler_context(hd_context *ctx)
{
    if (!ctx)
        return;

    if (hd_drop_imp(ctx, ctx->handler, &ctx->handler->refs))
    {
        hd_free(ctx, ctx->handler);
        ctx->handler = NULL;
    }
}