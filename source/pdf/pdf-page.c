//
// Created by sjw on 2018/1/22.
//

#include "pdf.h"

int
pdf_count_pages(hd_context *ctx, pdf_document *doc)
{
    if (doc->page_count == 0)
        doc->page_count = pdf_to_int(ctx, pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/Pages/Count"));
    return doc->page_count;
}

enum
{
    LOCAL_STACK_SIZE = 16
};

static pdf_obj *
pdf_lookup_page_loc_imp(hd_context *ctx, pdf_document *doc, pdf_obj *node, int *skip, pdf_obj **parentp, int *indexp)
{
    pdf_obj *kids;
    pdf_obj *hit = NULL;
    int i, len;
    pdf_obj *local_stack[LOCAL_STACK_SIZE];
    pdf_obj **stack = &local_stack[0];
    int stack_max = LOCAL_STACK_SIZE;
    int stack_len = 0;

    hd_var(hit);
    hd_var(stack);
    hd_var(stack_len);
    hd_var(stack_max);

    hd_try(ctx)
    {
        do
        {
            kids = pdf_dict_get(ctx, node, PDF_NAME_Kids);
            len = pdf_array_len(ctx, kids);

            if (len == 0)
                hd_throw(ctx, HD_ERROR_GENERIC, "malformed page tree");

            /* Every node we need to unmark goes into the stack */
            if (stack_len == stack_max)
            {
                if (stack == &local_stack[0])
                {
                    stack = hd_malloc_array(ctx, stack_max * 2, sizeof(*stack));
                    memcpy(stack, &local_stack[0], stack_max * sizeof(*stack));
                }
                else
                    stack = hd_resize_array(ctx, stack, stack_max * 2, sizeof(*stack));
                stack_max *= 2;
            }
            stack[stack_len++] = node;

            if (pdf_mark_obj(ctx, node))
                hd_throw(ctx, HD_ERROR_GENERIC, "cycle in page tree");

            for (i = 0; i < len; i++)
            {
                pdf_obj *kid = pdf_array_get(ctx, kids, i);
                pdf_obj *type = pdf_dict_get(ctx, kid, PDF_NAME_Type);
                if (type ? pdf_name_eq(ctx, type, PDF_NAME_Pages) : pdf_dict_get(ctx, kid, PDF_NAME_Kids) && !pdf_dict_get(ctx, kid, PDF_NAME_MediaBox))
                {
                    int count = pdf_to_int(ctx, pdf_dict_get(ctx, kid, PDF_NAME_Count));
                    if (*skip < count)
                    {
                        node = kid;
                        break;
                    }
                    else
                    {
                        *skip -= count;
                    }
                }
                else
                {
                    if (type ? !pdf_name_eq(ctx, type, PDF_NAME_Page) : !pdf_dict_get(ctx, kid, PDF_NAME_MediaBox))
                        hd_warn(ctx, "non-page object in page tree (%s)", pdf_to_name(ctx, type));
                    if (*skip == 0)
                    {
                        if (parentp) *parentp = node;
                        if (indexp) *indexp = i;
                        hit = kid;
                        break;
                    }
                    else
                    {
                        (*skip)--;
                    }
                }
            }
        }
        while (hit == NULL);
    }
    hd_always(ctx)
    {
        for (i = stack_len; i > 0; i--)
            pdf_unmark_obj(ctx, stack[i-1]);
        if (stack != &local_stack[0])
            hd_free(ctx, stack);
    }
    hd_catch(ctx)
    {
        hd_rethrow(ctx);
    }

    return hit;
}

pdf_obj *
pdf_lookup_page_loc(hd_context *ctx, pdf_document *doc, int needle, pdf_obj **parentp, int *indexp)
{
    pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME_Root);
    pdf_obj *node = pdf_dict_get(ctx, root, PDF_NAME_Pages);
    int skip = needle;
    pdf_obj *hit;

    if (!node)
        hd_throw(ctx, HD_ERROR_GENERIC, "cannot find page tree");

    hit = pdf_lookup_page_loc_imp(ctx, doc, node, &skip, parentp, indexp);
    if (!hit)
        hd_throw(ctx, HD_ERROR_GENERIC, "cannot find page %d in page tree", needle);
    return hit;
}

pdf_obj *
pdf_lookup_page_obj(hd_context *ctx, pdf_document *doc, int needle)
{
    pdf_obj *hit;
    hd_try(ctx)
        hit = pdf_lookup_page_loc(ctx, doc, needle, NULL, NULL);
    hd_catch(ctx)
        hd_rethrow(ctx);
    return hit;
}

static pdf_obj *
pdf_lookup_inherited_page_item(hd_context *ctx, pdf_obj *node, pdf_obj *key)
{
    pdf_obj *node2 = node;
    pdf_obj *val = NULL;

    hd_var(node);
    hd_try(ctx)
    {
        do
        {
            val = pdf_dict_get(ctx, node, key);
            if (val)
                break;
            if (pdf_mark_obj(ctx, node))
                hd_throw(ctx, HD_ERROR_GENERIC, "cycle in page tree (parents)");
            node = pdf_dict_get(ctx, node, PDF_NAME_Parent);
        }
        while (node);
    }
    hd_always(ctx)
    {
        do
        {
            pdf_unmark_obj(ctx, node2);
            if (node2 == node)
                break;
            node2 = pdf_dict_get(ctx, node2, PDF_NAME_Parent);
        }
        while (node2);
    }
    hd_catch(ctx)
    {
        hd_rethrow(ctx);
    }

    return val;
}

pdf_obj *
pdf_page_resources(hd_context *ctx, pdf_page *page)
{
    return pdf_lookup_inherited_page_item(ctx, page->obj, PDF_NAME_Resources);
}

static void
pdf_drop_page_imp(hd_context *ctx, pdf_page *page)
{
    pdf_document *doc = page->doc;

    pdf_drop_obj(ctx, page->obj);

    hd_drop_document(ctx, &page->doc->super);
}

static pdf_page *
pdf_new_page(hd_context *ctx, pdf_document *doc)
{
    pdf_page *page = hd_new_derived_page(ctx, pdf_page);

    page->doc = (pdf_document*) hd_keep_document(ctx, &doc->super);

    page->super.drop_page = (hd_page_drop_page_fn *)pdf_drop_page_imp;
    page->super.run_page_contents = (hd_page_run_page_contents_fn *)pdf_run_page_contents;

    page->obj = NULL;
    return page;
}

pdf_page *
pdf_load_page(hd_context *ctx, pdf_document *doc, int number)
{
    pdf_page *page;
    pdf_obj *pageobj, *obj;

    hd_try(ctx)
        pageobj = pdf_lookup_page_obj(ctx, doc, number);
    hd_catch(ctx)
        hd_rethrow(ctx);

    page = pdf_new_page(ctx, doc);
    page->obj = pdf_keep_obj(ctx, pageobj);
    return page;

}