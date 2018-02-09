//
// Created by sjw on 2018/1/15.
//

#ifndef HDCONTENTS_HDTD_DOCUMENT_H
#define HDCONTENTS_HDTD_DOCUMENT_H

#include "hdtd/system.h"
#include "hdtd/context.h"

/*
	Document interface
*/
typedef struct hd_document_s hd_document;
typedef struct hd_document_handler_s hd_document_handler;
typedef struct hd_page_s hd_page;

/*
	hd_page_drop_page_fn: Type for a function to release all the
	resources held by a page. Called automatically when the
	reference count for that page reaches zero.
*/
typedef void (hd_page_drop_page_fn)(hd_context *ctx, hd_page *page);

/*
	hd_page_run_page_contents_fn: Type for a function to run the
	contents of a page. See hd_run_page_contents for more
	information.
*/
typedef void (hd_page_run_page_contents_fn)(hd_context *ctx, hd_page *page, char* buffer, uint32_t *extract_len);

/*
	Structure definition is public so other classes can
	derive from it. Do not access the members directly.
*/
struct hd_page_s
{
    int refs;
    hd_page_drop_page_fn *drop_page;
    hd_page_run_page_contents_fn *run_page_contents;
};

/*
	hd_document_count_pages_fn: Type for a function to be called to
	count the number of pages in a document. See hd_count_pages for
	more information.
*/
typedef int (hd_document_count_pages_fn)(hd_context *ctx, hd_document *doc);

/*
	hd_document_load_page_fn: Type for a function to load a given
	page from a document. See hd_load_page for more information.
*/
typedef hd_page *(hd_document_load_page_fn)(hd_context *ctx, hd_document *doc, int number);

/*
	hd_new_page_of_size: Create and initialize a page struct.
*/
hd_page *hd_new_page_of_size(hd_context *ctx, int size);

#define hd_new_derived_page(CTX,TYPE) \
	((TYPE *)Memento_label(hd_new_page_of_size(CTX,sizeof(TYPE)),#TYPE))

/*
	hd_document_drop_fn: Type for a function to be called when
	the reference count for the hd_document drops to 0. The
	implementation should release any resources held by the
	document. The actual document pointer will be freed by the
	caller.
*/
typedef void (hd_document_drop_fn)(hd_context *ctx, hd_document *doc);

/*
	Structure definition is public so other classes can
	derive from it. Callers shoud not access the members
	directly, though implementations will need initialize
	functions directly.
*/
struct hd_document_s
{
    int refs;
    hd_document_drop_fn *drop_document;
    hd_document_count_pages_fn *count_pages;
    hd_document_load_page_fn *load_page;

};

/*
	hd_document_open_fn: Function type to open a document from a
	file.

	filename: file to open

	Pointer to opened document. Throws exception in case of error.
*/
typedef hd_document *(hd_document_open_fn)(hd_context *ctx, const char *filename);

/*
	hd_document_recognize_fn: Recognize a document type from
	a magic string.

	magic: string to recognise - typically a filename or mime
	type.

	Returns a number between 0 (not recognized) and 100
	(fully recognized) based on how certain the recognizer
	is that this is of the required type.
*/
typedef int (hd_document_recognize_fn)(hd_context *ctx, const char *magic);

/*
	hd_drop_document: Release an open document.

	The resource store in the context associated with hd_document
	is emptied, and any allocations for the document are freed when
	the last reference is dropped.

	Does not throw exceptions.
*/
void hd_drop_document(hd_context *ctx, hd_document *doc);

struct hd_document_handler_s
{
    hd_document_recognize_fn *recognize;
    hd_document_open_fn *open;
};

/*
	hd_new_document: Create and initialize a document struct.
*/
void *hd_new_document_of_size(hd_context *ctx, int size);

#define hd_new_derived_document(C,M) ((M*)Memento_label(hd_new_document_of_size(C, sizeof(M)), #M))

/*
	hd_keep_document: Keep a reference to an open document.
*/
hd_document *hd_keep_document(hd_context *ctx, hd_document *doc);

/*
	hd_register_document_handler: Register a handler
	for a document type.

	handler: The handler to register.
*/
void hd_register_document_handler(hd_context *ctx, const hd_document_handler *handler);

/*
	hd_register_document_handler: Register handlers
	for all the standard document types supported in
	this build.
*/
void hd_register_document_handlers(hd_context *ctx);

hd_document *hd_open_document(hd_context *ctx, const char *filename);

/*
	hd_drop_document: Release an open document.

*/
void hd_drop_document(hd_context *ctx, hd_document *doc);

/*
	hd_load_page: Load a page.

	number: page number, 0 is the first page of the document.
*/
hd_page *hd_load_page(hd_context *ctx, hd_document *doc, int number);

void hd_run_page_contents(hd_context *ctx, hd_page *page, char* buf, uint32_t *extract_len);

/*
	hd_drop_page: Free a loaded page.
*/
void hd_drop_page(hd_context *ctx, hd_page *page);

#endif //HDCONTENTS_HDTD_DOCUMENT_H
