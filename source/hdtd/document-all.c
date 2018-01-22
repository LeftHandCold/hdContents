//
// Created by sjw on 2018/1/15.
//
#include "hdtd.h"

extern hd_document_handler pdf_document_handler;

void hd_register_document_handlers(hd_context *ctx)
{
#if HD_ENABLE_PDF
    hd_register_document_handler(ctx, &pdf_document_handler);
#endif /* HD_ENABLE_PDF */
}
