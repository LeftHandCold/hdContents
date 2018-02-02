//
// Created by sjw on 01/02/2018.
//

#include "hdtd.h"
#include "pdf.h"

#include "pdf-imp.h"

struct entry
{
    int num;
    int gen;
    int ofs;
    int stm_ofs;
    int stm_len;
};

static void add_root(hd_context *ctx, pdf_obj *obj, pdf_obj ***roots, int *num_roots, int *max_roots)
{
    if (*num_roots == *max_roots)
    {
        int new_max_roots = *max_roots * 2;
        if (new_max_roots == 0)
            new_max_roots = 4;
        *roots = hd_resize_array(ctx, *roots, new_max_roots, sizeof(**roots));
        *max_roots = new_max_roots;
    }
    (*roots)[(*num_roots)++] = pdf_keep_obj(ctx, obj);
}

static inline int hd_mini(int a, int b)
{
    return (a < b ? a : b);
}

static inline int hd_clampi(int i, int min, int max)
{
    return (i > min ? (i < max ? i : max) : min);
}

static int is_white(int c)
{
    return c == '\x00' || c == '\x09' || c == '\x0a' || c == '\x0c' || c == '\x0d' || c == '\x20';
}

int
pdf_repair_obj(hd_context *ctx, pdf_document *doc, pdf_lexbuf *buf, int64_t *stmofsp, int *stmlenp, pdf_obj **encrypt, pdf_obj **id, pdf_obj **page, int64_t *tmpofs, pdf_obj **root)
{
    hd_stream *file = doc->file;
    pdf_token tok;
    int stm_len;

    *stmofsp = 0;
    if (stmlenp)
        *stmlenp = -1;

    stm_len = 0;

    /* On entry to this function, we know that we've just seen
     * '<int> <int> obj'. We expect the next thing we see to be a
     * pdf object. Regardless of the type of thing we meet next
     * we only need to fully parse it if it is a dictionary. */
    tok = pdf_lex(ctx, file, buf);

    if (tok == PDF_TOK_OPEN_DICT)
    {
        pdf_obj *obj, *dict = NULL;

        hd_try(ctx)
        {
            dict = pdf_parse_dict(ctx, doc, file, buf);
        }
        hd_catch(ctx)
        {
            hd_rethrow_if(ctx, HD_ERROR_TRYLATER);
            /* Don't let a broken object at EOF overwrite a good one */
            if (file->eof)
                hd_rethrow(ctx);
            /* Silently swallow the error */
            dict = pdf_new_dict(ctx, NULL, 2);
        }

        /* We must be careful not to try to resolve any indirections
         * here. We have just read dict, so we know it to be a non
         * indirected dictionary. Before we look at any values that
         * we get back from looking up in it, we need to check they
         * aren't indirected. */

        if (encrypt || id || root)
        {
            obj = pdf_dict_get(ctx, dict, PDF_NAME_Type);
            if (!pdf_is_indirect(ctx, obj) && pdf_name_eq(ctx, obj, PDF_NAME_XRef))
            {
                if (encrypt)
                {
                    obj = pdf_dict_get(ctx, dict, PDF_NAME_Encrypt);
                    if (obj)
                    {
                        pdf_drop_obj(ctx, *encrypt);
                        *encrypt = pdf_keep_obj(ctx, obj);
                    }
                }

                if (id)
                {
                    obj = pdf_dict_get(ctx, dict, PDF_NAME_ID);
                    if (obj)
                    {
                        pdf_drop_obj(ctx, *id);
                        *id = pdf_keep_obj(ctx, obj);
                    }
                }

                if (root)
                    *root = pdf_keep_obj(ctx, pdf_dict_get(ctx, dict, PDF_NAME_Root));
            }
        }

        obj = pdf_dict_get(ctx, dict, PDF_NAME_Length);
        if (!pdf_is_indirect(ctx, obj) && pdf_is_int(ctx, obj))
            stm_len = pdf_to_int(ctx, obj);

        pdf_drop_obj(ctx, dict);
    }

    while ( tok != PDF_TOK_STREAM &&
            tok != PDF_TOK_ENDOBJ &&
            tok != PDF_TOK_ERROR &&
            tok != PDF_TOK_EOF &&
            tok != PDF_TOK_INT )
    {
        *tmpofs = hd_tell(ctx, file);
        if (*tmpofs < 0)
            hd_throw(ctx, HD_ERROR_GENERIC, "cannot tell in file");
        tok = pdf_lex(ctx, file, buf);
    }

    if (tok == PDF_TOK_STREAM)
    {
        int c = hd_read_byte(ctx, file);
        if (c == '\r') {
            c = hd_peek_byte(ctx, file);
            if (c == '\n')
                hd_read_byte(ctx, file);
        }

        *stmofsp = hd_tell(ctx, file);
        if (*stmofsp < 0)
            hd_throw(ctx, HD_ERROR_GENERIC, "cannot seek in file");

        if (stm_len > 0)
        {
            hd_seek(ctx, file, *stmofsp + stm_len, 0);
            hd_try(ctx)
            {
                tok = pdf_lex(ctx, file, buf);
            }
            hd_catch(ctx)
            {
                hd_rethrow_if(ctx, HD_ERROR_TRYLATER);
                hd_warn(ctx, "cannot find endstream token, falling back to scanning");
            }
            if (tok == PDF_TOK_ENDSTREAM)
                goto atobjend;
            hd_seek(ctx, file, *stmofsp, 0);
        }

        (void)hd_read(ctx, file, (unsigned char *) buf->scratch, 9);

        while (memcmp(buf->scratch, "endstream", 9) != 0)
        {
            c = hd_read_byte(ctx, file);
            if (c == EOF)
                break;
            memmove(&buf->scratch[0], &buf->scratch[1], 8);
            buf->scratch[8] = c;
        }

        if (stmlenp)
            *stmlenp = hd_tell(ctx, file) - *stmofsp - 9;

        atobjend:
        *tmpofs = hd_tell(ctx, file);
        if (*tmpofs < 0)
            hd_throw(ctx, HD_ERROR_GENERIC, "cannot tell in file");
        tok = pdf_lex(ctx, file, buf);
        if (tok != PDF_TOK_ENDOBJ)
            hd_warn(ctx, "object missing 'endobj' token");
        else
        {
            /* Read another token as we always return the next one */
            *tmpofs = hd_tell(ctx, file);
            if (*tmpofs < 0)
                hd_throw(ctx, HD_ERROR_GENERIC, "cannot tell in file");
            tok = pdf_lex(ctx, file, buf);
        }
    }
    return tok;
}

void
pdf_repair_xref(hd_context *ctx, pdf_document *doc)
{
    pdf_obj *dict, *obj = NULL;
    pdf_obj *length;

    pdf_obj *encrypt = NULL;
    pdf_obj *id = NULL;
    pdf_obj **roots = NULL;
    pdf_obj *info = NULL;

    struct entry *list = NULL;
    int listlen;
    int listcap;
    int maxnum = 0;

    int num = 0;
    int gen = 0;
    int64_t tmpofs, stm_ofs, numofs = 0, genofs = 0;
    int stm_len;
    pdf_token tok;
    int next;
    int i;
    size_t j, n;
    int c;
    pdf_lexbuf *buf = &doc->lexbuf.base;
    int num_roots = 0;
    int max_roots = 0;

    hd_var(encrypt);
    hd_var(id);
    hd_var(roots);
    hd_var(num_roots);
    hd_var(max_roots);
    hd_var(info);
    hd_var(list);
    hd_var(obj);

    hd_warn(ctx, "repairing PDF document");

    pdf_forget_xref(ctx, doc);

    hd_seek(ctx, doc->file, 0, 0);

    hd_try(ctx)
    {
        pdf_xref_entry *entry;
        listlen = 0;
        listcap = 1024;
        list = hd_malloc_array(ctx, listcap, sizeof(struct entry));

        /* look for '%PDF' version marker within first kilobyte of file */
        n = hd_read(ctx, doc->file, (unsigned char *)buf->scratch, hd_mini(buf->size, 1024));

        hd_seek(ctx, doc->file, 0, 0);
        if (n >= 4)
        {
            for (j = 0; j < n - 4; j++)
            {
                if (memcmp(&buf->scratch[j], "%PDF", 4) == 0)
                {
                    hd_seek(ctx, doc->file, (int64_t)(j + 8), 0); /* skip "%PDF-X.Y" */
                    break;
                }
            }
        }

        /* skip comment line after version marker since some generators
         * forget to terminate the comment with a newline */
        c = hd_read_byte(ctx, doc->file);
        while (c >= 0 && (c == ' ' || c == '%'))
            c = hd_read_byte(ctx, doc->file);
        hd_unread_byte(ctx, doc->file);

        while (1)
        {
            tmpofs = hd_tell(ctx, doc->file);
            if (tmpofs < 0)
                hd_throw(ctx, HD_ERROR_GENERIC, "cannot tell in file");

            hd_try(ctx)
            tok = pdf_lex_no_string(ctx, doc->file, buf);
            hd_catch(ctx)
            {
                hd_rethrow_if(ctx, HD_ERROR_TRYLATER);
                hd_warn(ctx, "skipping ahead to next token");
                do
                    c = hd_read_byte(ctx, doc->file);
                while (c != EOF && !is_white(c));
                continue;
            }

            /* If we have the next token already, then we'll jump
             * back here, rather than going through the top of
             * the loop. */
            have_next_token:

            if (tok == PDF_TOK_INT)
            {
                if (buf->i < 0)
                {
                    num = 0;
                    gen = 0;
                    continue;
                }
                numofs = genofs;
                num = gen;
                genofs = tmpofs;
                gen = buf->i;
            }

            else if (tok == PDF_TOK_OBJ)
            {
                pdf_obj *root = NULL;

                hd_try(ctx)
                {
                    stm_len = 0;
                    stm_ofs = 0;
                    tok = pdf_repair_obj(ctx, doc, buf, &stm_ofs, &stm_len, &encrypt, &id, NULL, &tmpofs, &root);
                    if (root)
                        add_root(ctx, root, &roots, &num_roots, &max_roots);
                }
                hd_always(ctx)
                {
                    pdf_drop_obj(ctx, root);
                }
                hd_catch(ctx)
                {
                    hd_rethrow_if(ctx, HD_ERROR_TRYLATER);
                    /* If we haven't seen a root yet, there is nothing
                     * we can do, but give up. Otherwise, we'll make
                     * do. */
                    if (!roots)
                        hd_rethrow(ctx);
                    hd_warn(ctx, "cannot parse object (%d %d R) - ignoring rest of file", num, gen);
                    break;
                }

                if (num <= 0 || num > PDF_MAX_OBJECT_NUMBER)
                {
                    hd_warn(ctx, "ignoring object with invalid object number (%d %d R)", num, gen);
                    goto have_next_token;
                }

                gen = hd_clampi(gen, 0, 65535);

                if (listlen + 1 == listcap)
                {
                    listcap = (listcap * 3) / 2;
                    list = hd_resize_array(ctx, list, listcap, sizeof(struct entry));
                }

                list[listlen].num = num;
                list[listlen].gen = gen;
                list[listlen].ofs = numofs;
                list[listlen].stm_ofs = stm_ofs;
                list[listlen].stm_len = stm_len;
                listlen ++;

                if (num > maxnum)
                    maxnum = num;

                goto have_next_token;
            }

                /* If we find a dictionary it is probably the trailer,
                 * but could be a stream (or bogus) dictionary caused
                 * by a corrupt file. */
            else if (tok == PDF_TOK_OPEN_DICT)
            {
                hd_try(ctx)
                {
                    dict = pdf_parse_dict(ctx, doc, doc->file, buf);
                }
                hd_catch(ctx)
                {
                    hd_rethrow_if(ctx, HD_ERROR_TRYLATER);
                    /* If this was the real trailer dict
                     * it was broken, in which case we are
                     * in trouble. Keep going though in
                     * case this was just a bogus dict. */
                    continue;
                }

                hd_try(ctx)
                {
                    obj = pdf_dict_get(ctx, dict, PDF_NAME_Encrypt);
                    if (obj)
                    {
                        pdf_drop_obj(ctx, encrypt);
                        encrypt = pdf_keep_obj(ctx, obj);
                    }

                    obj = pdf_dict_get(ctx, dict, PDF_NAME_ID);
                    if (obj && (!id || !encrypt || pdf_dict_get(ctx, dict, PDF_NAME_Encrypt)))
                    {
                        pdf_drop_obj(ctx, id);
                        id = pdf_keep_obj(ctx, obj);
                    }

                    obj = pdf_dict_get(ctx, dict, PDF_NAME_Root);
                    if (obj)
                        add_root(ctx, obj, &roots, &num_roots, &max_roots);

                    obj = pdf_dict_get(ctx, dict, PDF_NAME_Info);
                    if (obj)
                    {
                        pdf_drop_obj(ctx, info);
                        info = pdf_keep_obj(ctx, obj);
                    }
                }
                hd_always(ctx)
                pdf_drop_obj(ctx, dict);
                hd_catch(ctx)
                hd_rethrow(ctx);

                obj = NULL;
            }

            else if (tok == PDF_TOK_EOF)
                break;
            else
            {
                if (tok == PDF_TOK_ERROR)
                    hd_read_byte(ctx, doc->file);
                num = 0;
                gen = 0;
            }
        }

        if (listlen == 0)
            hd_throw(ctx, HD_ERROR_GENERIC, "no objects found");

        /* make xref reasonable */

        /*
            Dummy access to entry to assure sufficient space in the xref table
            and avoid repeated reallocs in the loop
        */
        /* Ensure that the first xref table is a 'solid' one from
         * 0 to maxnum. */
        pdf_ensure_solid_xref(ctx, doc, maxnum);

        for (i = 1; i < maxnum; i++)
        {
            entry = pdf_get_populating_xref_entry(ctx, doc, i);
            if (entry->obj != NULL)
                continue;
            entry->type = 'f';
            entry->ofs = 0;
            entry->gen = 0;
            entry->num = 0;

            entry->stm_ofs = 0;
        }

        for (i = 0; i < listlen; i++)
        {
            entry = pdf_get_populating_xref_entry(ctx, doc, list[i].num);
            entry->type = 'n';
            entry->ofs = list[i].ofs;
            entry->gen = list[i].gen;
            entry->num = list[i].num;

            entry->stm_ofs = list[i].stm_ofs;

        }

        entry = pdf_get_populating_xref_entry(ctx, doc, 0);
        entry->type = 'f';
        entry->ofs = 0;
        entry->gen = 65535;
        entry->num = 0;
        entry->stm_ofs = 0;

        next = 0;
        for (i = pdf_xref_len(ctx, doc) - 1; i >= 0; i--)
        {
            entry = pdf_get_populating_xref_entry(ctx, doc, i);
            if (entry->type == 'f')
            {
                entry->ofs = next;
                if (entry->gen < 65535)
                    entry->gen ++;
                next = i;
            }
        }

        /* create a repaired trailer, Root will be added later */

        obj = pdf_new_dict(ctx, doc, 5);
        /* During repair there is only a single xref section */
        pdf_set_populating_xref_trailer(ctx, doc, obj);
        pdf_drop_obj(ctx, obj);
        obj = NULL;

        obj = pdf_new_int(ctx, doc, maxnum + 1);
        pdf_dict_put(ctx, pdf_trailer(ctx, doc), PDF_NAME_Size, obj);
        pdf_drop_obj(ctx, obj);
        obj = NULL;

        if (roots)
        {
            for (i = num_roots-1; i > 0; i--)
            {
                if (pdf_is_dict(ctx, roots[i]))
                    break;
            }
            if (i >= 0)
            {
                pdf_dict_put(ctx, pdf_trailer(ctx, doc), PDF_NAME_Root, roots[i]);
            }
        }
        if (info)
        {
            pdf_dict_put(ctx, pdf_trailer(ctx, doc), PDF_NAME_Info, info);
            pdf_drop_obj(ctx, info);
            info = NULL;
        }

        if (encrypt)
        {
            if (pdf_is_indirect(ctx, encrypt))
            {
                /* create new reference with non-NULL xref pointer */
                obj = pdf_new_indirect(ctx, doc, pdf_to_num(ctx, encrypt), pdf_to_gen(ctx, encrypt));
                pdf_drop_obj(ctx, encrypt);
                encrypt = obj;
                obj = NULL;
            }
            pdf_dict_put(ctx, pdf_trailer(ctx, doc), PDF_NAME_Encrypt, encrypt);
            pdf_drop_obj(ctx, encrypt);
            encrypt = NULL;
        }

        if (id)
        {
            if (pdf_is_indirect(ctx, id))
            {
                /* create new reference with non-NULL xref pointer */
                obj = pdf_new_indirect(ctx, doc, pdf_to_num(ctx, id), pdf_to_gen(ctx, id));
                pdf_drop_obj(ctx, id);
                id = obj;
                obj = NULL;
            }
            pdf_dict_put(ctx, pdf_trailer(ctx, doc), PDF_NAME_ID, id);
            pdf_drop_obj(ctx, id);
            id = NULL;
        }

        hd_free(ctx, list);
    }
    hd_always(ctx)
    {
        for (i = 0; i < num_roots; i++)
            pdf_drop_obj(ctx, roots[i]);
        hd_free(ctx, roots);
    }
    hd_catch(ctx)
    {
        pdf_drop_obj(ctx, encrypt);
        pdf_drop_obj(ctx, id);
        pdf_drop_obj(ctx, obj);
        pdf_drop_obj(ctx, info);
        hd_free(ctx, list);
        hd_rethrow(ctx);
    }
}


static void
pdf_repair_obj_stm(hd_context *ctx, pdf_document *doc, int stm_num)
{
    pdf_obj *obj;
    hd_stream *stm = NULL;
    pdf_token tok;
    int i, n, count;
    pdf_lexbuf buf;

    hd_var(stm);

    pdf_lexbuf_init(ctx, &buf, PDF_LEXBUF_SMALL);

    hd_try(ctx)
    {
        obj = pdf_load_object(ctx, doc, stm_num);

        count = pdf_to_int(ctx, pdf_dict_get(ctx, obj, PDF_NAME_N));

        pdf_drop_obj(ctx, obj);

        stm = pdf_open_stream_number(ctx, doc, stm_num);

        for (i = 0; i < count; i++)
        {
            pdf_xref_entry *entry;

            tok = pdf_lex(ctx, stm, &buf);
            if (tok != PDF_TOK_INT)
                hd_throw(ctx, HD_ERROR_GENERIC, "corrupt object stream (%d 0 R)", stm_num);

            n = buf.i;
            if (n < 0)
            {
                hd_warn(ctx, "ignoring object with invalid object number (%d %d R)", n, i);
                continue;
            }
            else if (n >= pdf_xref_len(ctx, doc))
            {
                hd_warn(ctx, "ignoring object with invalid object number (%d %d R)", n, i);
                continue;
            }

            entry = pdf_get_populating_xref_entry(ctx, doc, n);
            entry->ofs = stm_num;
            entry->gen = i;
            entry->num = n;
            entry->stm_ofs = 0;
            pdf_drop_obj(ctx, entry->obj);
            entry->obj = NULL;
            entry->type = 'o';

            tok = pdf_lex(ctx, stm, &buf);
            if (tok != PDF_TOK_INT)
                hd_throw(ctx, HD_ERROR_GENERIC, "corrupt object stream (%d 0 R)", stm_num);
        }
    }
    hd_always(ctx)
    {
        hd_drop_stream(ctx, stm);
        pdf_lexbuf_fin(ctx, &buf);
    }
    hd_catch(ctx)
    {
        hd_rethrow(ctx);
    }
}

void
pdf_repair_obj_stms(hd_context *ctx, pdf_document *doc)
{
    pdf_obj *dict;
    int i;
    int xref_len = pdf_xref_len(ctx, doc);

    for (i = 0; i < xref_len; i++)
    {
        pdf_xref_entry *entry = pdf_get_populating_xref_entry(ctx, doc, i);

        if (entry->stm_ofs)
        {
            dict = pdf_load_object(ctx, doc, i);
            hd_try(ctx)
            {
                if (pdf_name_eq(ctx, pdf_dict_get(ctx, dict, PDF_NAME_Type), PDF_NAME_ObjStm))
                    pdf_repair_obj_stm(ctx, doc, i);
            }
            hd_catch(ctx)
            {
                hd_warn(ctx, "ignoring broken object stream (%d 0 R)", i);
            }
            pdf_drop_obj(ctx, dict);
        }
    }

    /* Ensure that streamed objects reside inside a known non-streamed object */
    for (i = 0; i < xref_len; i++)
    {
        pdf_xref_entry *entry = pdf_get_populating_xref_entry(ctx, doc, i);

        if (entry->type == 'o' && pdf_get_populating_xref_entry(ctx, doc, entry->ofs)->type != 'n')
            hd_throw(ctx, HD_ERROR_GENERIC, "invalid reference to non-object-stream: %d (%d 0 R)", (int)entry->ofs, i);
    }
}