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

/*
	hd_open_document: Open a PDF, XPS or CBZ document.

	Open a document file and read its basic structure so pages and
	objects can be located. MuPDF will try to repair broken
	documents (without actually changing the file contents).

	The returned hd_document is used when calling most other
	document related functions.

	filename: a path to a file as it would be given to open(2).
*/
hd_document *hd_open_document(hd_context *ctx, const char *filename);

/*
	hd_drop_document: Release an open document.

	The resource store in the context associated with hd_document
	is emptied, and any allocations for the document are freed when
	the last reference is dropped.

	Does not throw exceptions.
*/
void hd_drop_document(hd_context *ctx, hd_document *doc);

#endif //HDCONTENTS_HDTD_DOCUMENT_H
