//
// Created by sjw on 2018/1/22.
//

#ifndef HDCONTENTS_PDF_INTERPRET_H
#define HDCONTENTS_PDF_INTERPRET_H

typedef struct pdf_csi_s pdf_csi;
typedef struct pdf_processor_s pdf_processor;

void *pdf_new_processor(hd_context *ctx, int size);
void pdf_close_processor(hd_context *ctx, pdf_processor *proc);
void pdf_drop_processor(hd_context *ctx, pdf_processor *proc);

struct pdf_processor_s
{
    void (*close_processor)(hd_context *ctx, pdf_processor *proc);
    void (*drop_processor)(hd_context *ctx, pdf_processor *proc);

    /* text objects */
    void (*op_BT)(hd_context *ctx, pdf_processor *proc);
    void (*op_ET)(hd_context *ctx, pdf_processor *proc);

    void (*op_Tf)(hd_context *ctx, pdf_processor *proc, const char *name, pdf_font_desc *font, float size);

    void (*op_TJ)(hd_context *ctx, pdf_processor *proc, pdf_obj *array);
    void (*op_Tj)(hd_context *ctx, pdf_processor *proc, char *str, int len);

    /* END is used to signify end of stream (finalise and close down) */
    void (*op_END)(hd_context *ctx, pdf_processor *proc);
};

struct pdf_csi_s
{
    /* input */
    pdf_document *doc;
    pdf_obj *rdb;
    pdf_lexbuf *buf;

    /* state */
    int gstate;
    int xbalance;
    int in_text;

    /* stack */
    pdf_obj *obj;
    char name[256];
    char string[256];
    int string_len;
    int top;
    float stack[32];
};

/*
	pdf_new_run_processor: Create a new "run" processor. This maps
	from PDF operators to hd_device level calls.
*/
pdf_processor *pdf_new_run_processor(hd_context *ctx);

/* Functions to actually process annotations, glyphs and general stream objects */
void pdf_process_contents(hd_context *ctx, pdf_processor *proc, pdf_document *doc, pdf_obj *obj, pdf_obj *res);

#endif //HDCONTENTS_PDF_INTERPRET_H
