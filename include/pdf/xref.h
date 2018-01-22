//
// Created by sjw on 2018/1/18.
//

#ifndef HDCONTENTS_PDF_XREF_H
#define HDCONTENTS_PDF_XREF_H

/*
 * xref and object / stream api
 */

typedef struct pdf_xref_entry_s pdf_xref_entry;

struct pdf_xref_entry_s
{
    char type;		/* 0=unset (f)ree i(n)use (o)bjstm */
    unsigned char flags;	/* bit 0 = marked */
    unsigned short gen;	/* generation / objstm index */
    int num;		/* original object number (for decryption after renumbering) */
    int64_t ofs;		/* file offset / objstm object number */
    int64_t stm_ofs;	/* on-disk stream */
    hd_buffer *stm_buf;	/* in-memory stream (for updated objects) */
    pdf_obj *obj;		/* stored/cached object */
};

typedef struct pdf_xref_subsec_s pdf_xref_subsec;

struct pdf_xref_subsec_s
{
    pdf_xref_subsec *next;
    int len;
    int start;
    pdf_xref_entry *table;
};

struct pdf_xref_s
{
    int num_objects;
    pdf_xref_subsec *subsec;
    pdf_obj *trailer;
    pdf_obj *pre_repair_trailer;
    pdf_unsaved_sig *unsaved_sigs;
    pdf_unsaved_sig **unsaved_sigs_end;
    int64_t end_ofs; /* file offset to end of xref */
};

pdf_xref_entry *pdf_cache_object(hd_context *ctx, pdf_document *doc, int num);

int pdf_count_objects(hd_context *ctx, pdf_document *doc);
pdf_obj *pdf_resolve_indirect(hd_context *ctx, pdf_obj *ref);
pdf_obj *pdf_resolve_indirect_chain(hd_context *ctx, pdf_obj *ref);
pdf_obj *pdf_load_object(hd_context *ctx, pdf_document *doc, int num);

pdf_obj *pdf_trailer(hd_context *ctx, pdf_document *doc);

#endif //HDCONTENTS_PDF_XREF_H
