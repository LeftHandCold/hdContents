//
// Created by sjw on 2018/1/30.
//

#ifndef HDCONTENTS_PDF_RESOURCE_H
#define HDCONTENTS_PDF_RESOURCE_H

/*
 * PDF interface to store
 */
void pdf_store_item(hd_context *ctx, pdf_obj *key, void *val, size_t itemsize);
void *pdf_find_item(hd_context *ctx, hd_store_drop_fn *drop, pdf_obj *key);
void pdf_remove_item(hd_context *ctx, hd_store_drop_fn *drop, pdf_obj *key);
void pdf_empty_store(hd_context *ctx, pdf_document *doc);

#endif //HDCONTENTS_PDF_RESOURCE_H
