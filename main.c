#include "hdtd.h"

int main() {
    
    hd_context *ctx;
    hd_document *doc;
    ctx = hd_new_context(NULL);
    if (!ctx)
    {
        fprintf(stderr, "cannot create mupdf context\n");
        return EXIT_FAILURE;
    }

    /* Register the default file types to handle. */
    hd_try(ctx)
        hd_register_document_handlers(ctx);
    hd_catch(ctx)
    {
        fprintf(stderr, "cannot register document handlers: %s\n", hd_caught_message(ctx));
        hd_drop_context(ctx);
        return EXIT_FAILURE;
    }

    /* Open the document. */
    hd_try(ctx)
        doc = hd_open_document(ctx, "F:\\f0711656.pdf");
    hd_catch(ctx) {
        fprintf(stderr, "cannot open document: %s\n", hd_caught_message(ctx));
        hd_drop_context(ctx);
        return EXIT_FAILURE;
    }
    hd_drop_document(ctx, doc);
    hd_drop_context(ctx);
    printf("hd_new_context is end\n");
}