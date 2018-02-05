//
// Created by sjw on 2018/1/29.
//

#ifndef HDCONTENTS_PDF_FONT_H
#define HDCONTENTS_PDF_FONT_H

#include "pdf/cmap.h"


void pdf_load_encoding(const char **estrings, const char *encoding);
int pdf_lookup_agl(const char *name);

extern const char * const pdf_mac_roman[256];
extern const char * const pdf_mac_expert[256];
extern const char * const pdf_win_ansi[256];
extern const char * const pdf_standard[256];

typedef struct pdf_font_desc_s pdf_font_desc;

struct pdf_font_desc_s
{
    hd_storable storable;
    size_t size;

    /* FontDescriptor */
    int flags;

    /* Encoding (CMap) */
    pdf_cmap *encoding;
    pdf_cmap *to_ttf_cmap;
    size_t cid_to_gid_len;
    unsigned short *cid_to_gid;

    /* ToUnicode */
    pdf_cmap *to_unicode;
    size_t cid_to_ucs_len;
    unsigned short *cid_to_ucs;

    /* Metrics (given in the PDF file) */
    int wmode;

    int is_embedded;
};

void pdf_set_font_wmode(hd_context *ctx, pdf_font_desc *font, int wmode);

void pdf_load_to_unicode(hd_context *ctx, pdf_document *doc, pdf_font_desc *font, const char **strings, char *collection, pdf_obj *cmapstm);

int pdf_font_cid_to_gid(hd_context *ctx, pdf_font_desc *fontdesc, int cid);

pdf_font_desc *pdf_load_type3_font(hd_context *ctx, pdf_document *doc, pdf_obj *rdb, pdf_obj *obj);
pdf_font_desc *pdf_load_font(hd_context *ctx, pdf_document *doc, pdf_obj *rdb, pdf_obj *obj, int nested_depth);
pdf_font_desc *pdf_load_hail_mary_font(hd_context *ctx, pdf_document *doc);

pdf_font_desc *pdf_new_font_desc(hd_context *ctx);
pdf_font_desc *pdf_keep_font(hd_context *ctx, pdf_font_desc *fontdesc);
void pdf_drop_font(hd_context *ctx, pdf_font_desc *font);

#endif //HDCONTENTS_PDF_FONT_H
