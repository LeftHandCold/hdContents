//
// Created by sjw on 2018/1/22.
//

#ifndef HDCONTENTS_PDF_PAGE_H
#define HDCONTENTS_PDF_PAGE_H

int pdf_count_pages(hd_context *ctx, pdf_document *doc);

/*
	pdf_load_page: Load a page and its resources.

	Locates the page in the PDF document and loads the page and its
	resources. After pdf_load_page is it possible to retrieve the size
	of the page using pdf_bound_page, or to render the page using
	pdf_run_page_*.

	number: page number, where 0 is the first page of the document.
*/
pdf_page *pdf_load_page(hd_context *ctx, pdf_document *doc, int number);

/*
	pdf_run_page_contents: Interpret a loaded page and render it on a device.
	Just the main page contents without the annotations
*/
void pdf_run_page_contents(hd_context *ctx, pdf_page *page);

/*
 * Page tree, pages and related objects
 */

struct pdf_page_s
{
    hd_page super;
    pdf_document *doc;
    pdf_obj *obj;
};

#endif //HDCONTENTS_PDF_PAGE_H
