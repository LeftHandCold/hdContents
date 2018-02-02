//
// Created by sjw on 2018/1/18.
//

#ifndef HDCONTENTS_PDF_PARSE_H
#define HDCONTENTS_PDF_PARSE_H


/*
 * tokenizer and low-level object parser
 */

typedef enum
{
    PDF_TOK_ERROR, PDF_TOK_EOF,
    PDF_TOK_OPEN_ARRAY, PDF_TOK_CLOSE_ARRAY,
    PDF_TOK_OPEN_DICT, PDF_TOK_CLOSE_DICT,
    PDF_TOK_OPEN_BRACE, PDF_TOK_CLOSE_BRACE,
    PDF_TOK_NAME, PDF_TOK_INT, PDF_TOK_REAL, PDF_TOK_STRING, PDF_TOK_KEYWORD,
    PDF_TOK_R, PDF_TOK_TRUE, PDF_TOK_FALSE, PDF_TOK_NULL,
    PDF_TOK_OBJ, PDF_TOK_ENDOBJ,
    PDF_TOK_STREAM, PDF_TOK_ENDSTREAM,
    PDF_TOK_XREF, PDF_TOK_TRAILER, PDF_TOK_STARTXREF,
    PDF_NUM_TOKENS
} pdf_token;

void pdf_lexbuf_init(hd_context *ctx, pdf_lexbuf *lexbuf, int size);
void pdf_lexbuf_fin(hd_context *ctx, pdf_lexbuf *lexbuf);
int pdf_lexbuf_grow(hd_context *ctx, pdf_lexbuf *lexbuf);

pdf_token pdf_lex(hd_context *ctx, hd_stream *f, pdf_lexbuf *lexbuf);
pdf_token pdf_lex_no_string(hd_context *ctx, hd_stream *f, pdf_lexbuf *lexbuf);

pdf_obj *pdf_parse_array(hd_context *ctx, pdf_document *doc, hd_stream *f, pdf_lexbuf *buf);
pdf_obj *pdf_parse_dict(hd_context *ctx, pdf_document *doc, hd_stream *f, pdf_lexbuf *buf);
pdf_obj *pdf_parse_stm_obj(hd_context *ctx, pdf_document *doc, hd_stream *f, pdf_lexbuf *buf);
pdf_obj *pdf_parse_indirect_obj(hd_context *ctx, pdf_document *doc, hd_stream *f, pdf_lexbuf *buf, int *num, int *gen, hd_off_t *stm_ofs);

#endif //HDCONTENTS_PDF_PARSE_H
