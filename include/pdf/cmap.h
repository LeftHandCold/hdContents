//
// Created by sjw on 2018/1/29.
//

#ifndef HDCONTENTS_PDF_CMAP_H
#define HDCONTENTS_PDF_CMAP_H

typedef struct pdf_cmap_s pdf_cmap;
typedef struct pdf_range_s pdf_range;
typedef struct pdf_xrange_s pdf_xrange;
typedef struct pdf_mrange_s pdf_mrange;

#define PDF_MRANGE_CAP 8

struct pdf_range_s
{
    unsigned short low, high, out;
};

struct pdf_xrange_s
{
    unsigned int low, high, out;
};

struct pdf_mrange_s
{
    unsigned int low, out;
};

typedef struct cmap_splay_s cmap_splay;

struct pdf_cmap_s
{
    char cmap_name[32];

    char usecmap_name[32];
    pdf_cmap *usecmap;

    int wmode;

    int codespace_len;
    struct
    {
        int n;
        unsigned int low;
        unsigned int high;
    } codespace[40];

    int rlen, rcap;
    pdf_range *ranges;

    int xlen, xcap;
    pdf_xrange *xranges;

    int mlen, mcap;
    pdf_mrange *mranges;

    int dlen, dcap;
    int *dict;

    int tlen, tcap, ttop;
    cmap_splay *tree;

};

pdf_cmap *pdf_new_cmap(hd_context *ctx);
pdf_cmap *pdf_keep_cmap(hd_context *ctx, pdf_cmap *cmap);
void pdf_drop_cmap(hd_context *ctx, pdf_cmap *cmap);
void pdf_drop_cmap_imp(hd_context *ctx);
size_t pdf_cmap_size(hd_context *ctx, pdf_cmap *cmap);

int pdf_cmap_wmode(hd_context *ctx, pdf_cmap *cmap);
void pdf_set_cmap_wmode(hd_context *ctx, pdf_cmap *cmap, int wmode);
void pdf_set_usecmap(hd_context *ctx, pdf_cmap *cmap, pdf_cmap *usecmap);

void pdf_add_codespace(hd_context *ctx, pdf_cmap *cmap, unsigned int low, unsigned int high, int n);
void pdf_map_range_to_range(hd_context *ctx, pdf_cmap *cmap, unsigned int srclo, unsigned int srchi, int dstlo);
void pdf_map_one_to_many(hd_context *ctx, pdf_cmap *cmap, unsigned int one, int *many, int len);
void pdf_sort_cmap(hd_context *ctx, pdf_cmap *cmap);

int pdf_lookup_cmap(pdf_cmap *cmap, unsigned int cpt);
int pdf_lookup_cmap_full(pdf_cmap *cmap, unsigned int cpt, int *out);
int pdf_decode_cmap(pdf_cmap *cmap, unsigned char *s, unsigned char *e, unsigned int *cpt);

pdf_cmap *pdf_new_identity_cmap(hd_context *ctx, int wmode, int bytes);
pdf_cmap *pdf_load_cmap(hd_context *ctx, hd_stream *file);
pdf_cmap *pdf_load_system_cmap(hd_context *ctx, const char *name);
pdf_cmap *pdf_load_builtin_cmap(hd_context *ctx, const char *name);
pdf_cmap *pdf_load_embedded_cmap(hd_context *ctx, pdf_document *doc, pdf_obj *ref);

#endif //HDCONTENTS_PDF_CMAP_H
