//
// Created by sjw on 2018/1/29.
//

#ifndef HDCONTENTS_PDF_FONT_H
#define HDCONTENTS_PDF_FONT_H

#include "pdf/cmap.h"


void pdf_load_encoding(const char **estrings, const char *encoding);
int pdf_lookup_agl(const char *name);

typedef struct pdf_font_desc_s pdf_font_desc;

struct pdf_font_desc_s
{
    hd_storable storable;
    size_t size;

    /* FontDescriptor */
    int flags;

    /* Encoding (CMap) */
    pdf_cmap *encoding;

    /* ToUnicode */
    pdf_cmap *to_unicode;
    size_t cid_to_ucs_len;
    unsigned short *cid_to_ucs;

    /* Metrics (given in the PDF file) */
    int wmode;

    int is_embedded;
};


void pdf_load_to_unicode(hd_context *ctx, pdf_font_desc *font, const char **strings, char *collection, pdf_obj *cmapstm);

pdf_font_desc *pdf_load_type3_font(hd_context *ctx, pdf_document *doc, pdf_obj *rdb, pdf_obj *obj);
pdf_font_desc *pdf_load_font(hd_context *ctx, pdf_document *doc, pdf_obj *obj);

pdf_font_desc *pdf_new_font_desc(hd_context *ctx);
void pdf_drop_font(hd_context *ctx, pdf_font_desc *font);

#endif //HDCONTENTS_PDF_FONT_H
