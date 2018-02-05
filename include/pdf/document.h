//
// Created by sjw on 2018/1/15.
//

#ifndef HDCONTENTS_PDF_DOCUMENT_H
#define HDCONTENTS_PDF_DOCUMENT_H

typedef struct pdf_lexbuf_s pdf_lexbuf;
typedef struct pdf_lexbuf_large_s pdf_lexbuf_large;
typedef struct pdf_xref_s pdf_xref;
typedef struct pdf_page_s pdf_page;

enum
{
    PDF_LEXBUF_SMALL = 256,
    PDF_LEXBUF_LARGE = 65536
};

struct pdf_lexbuf_s
{
    int size;
    int base_size;
    int len;
    int64_t i;
    float f;
    char *scratch;
    char buffer[PDF_LEXBUF_SMALL];
};

struct pdf_lexbuf_large_s
{
    pdf_lexbuf base;
    char buffer[PDF_LEXBUF_LARGE - PDF_LEXBUF_SMALL];
};

int pdf_recognize(hd_context *doc, const char *magic);
/*
	pdf_open_document: Open a PDF document.

	Open a PDF document by reading its cross reference table, so
	HdContents can locate PDF objects inside the file. Upon an broken
	cross reference table or other parse errors HdContents will restart
	parsing the file from the beginning to try to rebuild a
	(hopefully correct) cross reference table to allow further
	processing of the file.

	The returned pdf_document should be used when calling most
	other PDF functions. Note that it wraps the context, so those
	functions implicitly get access to the global state in
	context.

	filename: a path to a file as it would be given to open(2).
*/
pdf_document *pdf_open_document(hd_context *ctx, const char *filename);


/* Unsaved signature fields */
typedef struct pdf_unsaved_sig_s pdf_unsaved_sig;

struct pdf_unsaved_sig_s
{
    pdf_obj *field;
    pdf_unsaved_sig *next;
};

struct pdf_document_s
{
    hd_document super;

    hd_stream *file;

    hd_off_t startxref;
    hd_off_t file_size;

    int max_xref_len;
    int num_xref_sections;
    int saved_num_xref_sections;
    pdf_xref *xref_sections;
    pdf_xref *saved_xref_sections;
	int *xref_index;

	int page_count;

    pdf_lexbuf_large lexbuf;
};
#endif //HDCONTENTS_PDF_DOCUMENT_H
