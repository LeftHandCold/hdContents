//
// Created by sjw on 2018/1/15.
//
#include "pdf-imp.h"

static inline int iswhite(int ch)
{
    return
            ch == '\000' || ch == '\011' || ch == '\012' ||
            ch == '\014' || ch == '\015' || ch == '\040';
}

/*
 * xref tables
 */

static void pdf_drop_xref_sections_imp(hd_context *ctx, pdf_document *doc, pdf_xref *xref_sections, int num_xref_sections)
{
    pdf_unsaved_sig *usig;
    int x, e;

    for (x = 0; x < num_xref_sections; x++)
    {
        pdf_xref *xref = &xref_sections[x];
        pdf_xref_subsec *sub = xref->subsec;

        while (sub != NULL)
        {
            pdf_xref_subsec *next_sub = sub->next;
            for (e = 0; e < sub->len; e++)
            {
                pdf_xref_entry *entry = &sub->table[e];

                if (entry->obj)
                {
                    pdf_drop_obj(ctx, entry->obj);
                    hd_drop_buffer(ctx, entry->stm_buf);
                }
            }
            hd_free(ctx, sub->table);
            hd_free(ctx, sub);
            sub = next_sub;
        }

        pdf_drop_obj(ctx, xref->pre_repair_trailer);
        pdf_drop_obj(ctx, xref->trailer);

        while ((usig = xref->unsaved_sigs) != NULL)
        {
            xref->unsaved_sigs = usig->next;
            pdf_drop_obj(ctx, usig->field);
            hd_free(ctx, usig);
        }
    }

    hd_free(ctx, xref_sections);
}

static void pdf_drop_xref_sections(hd_context *ctx, pdf_document *doc)
{
    pdf_drop_xref_sections_imp(ctx, doc, doc->saved_xref_sections, doc->saved_num_xref_sections);
    pdf_drop_xref_sections_imp(ctx, doc, doc->xref_sections, doc->num_xref_sections);

    doc->saved_xref_sections = NULL;
    doc->saved_num_xref_sections = 0;
    doc->xref_sections = NULL;
    doc->num_xref_sections = 0;
}

int
pdf_recognize(hd_context *doc, const char *magic)
{
    char *ext = strrchr(magic, '.');

    if (ext)
    {
        if (!strcasecmp(ext, ".pdf"))
            return 100;
    }
    if (!strcmp(magic, "pdf") || !strcmp(magic, "application/pdf"))
        return 100;

    return 1;
}

static void
extend_xref_index(hd_context *ctx, pdf_document *doc, int newlen)
{
	int i;

	doc->xref_index = hd_resize_array(ctx, doc->xref_index, newlen, sizeof(int));
	for (i = doc->max_xref_len; i < newlen; i++)
	{
		doc->xref_index[i] = 0;
	}
	doc->max_xref_len = newlen;
}


/* Ensure that the given xref has a single subsection
 * that covers the entire range. */
static void
ensure_solid_xref(hd_context *ctx, pdf_document *doc, int num, int which)
{
	pdf_xref *xref = &doc->xref_sections[which];
	pdf_xref_subsec *sub = xref->subsec;
	pdf_xref_subsec *new_sub;

	if (num < xref->num_objects)
		num = xref->num_objects;

	if (sub != NULL && sub->next == NULL && sub->start == 0 && sub->len >= num)
		return;

	new_sub = hd_malloc_struct(ctx, pdf_xref_subsec);
	hd_try(ctx)
	{
		new_sub->table = hd_calloc(ctx, num, sizeof(pdf_xref_entry));
		new_sub->start = 0;
		new_sub->len = num;
		new_sub->next = NULL;
	}
	hd_catch(ctx)
	{
		hd_free(ctx, new_sub);
		hd_rethrow(ctx);
	}

	/* Move objects over to the new subsection and destroy the old
	 * ones */
	sub = xref->subsec;
	while (sub != NULL)
	{
		pdf_xref_subsec *next = sub->next;
		int i;

		for (i = 0; i < sub->len; i++)
		{
			new_sub->table[i+sub->start] = sub->table[i];
		}
		hd_free(ctx, sub->table);
		hd_free(ctx, sub);
		sub = next;
	}
	xref->num_objects = num;
	xref->subsec = new_sub;
	if (doc->max_xref_len < num)
		extend_xref_index(ctx, doc, num);
}

static void pdf_populate_next_xref_level(hd_context *ctx, pdf_document *doc)
{
    pdf_xref *xref;
    doc->xref_sections = hd_resize_array(ctx, doc->xref_sections, doc->num_xref_sections + 1, sizeof(pdf_xref));
    doc->num_xref_sections++;

    xref = &doc->xref_sections[doc->num_xref_sections - 1];
    xref->subsec = NULL;
    xref->num_objects = 0;
    xref->trailer = NULL;
    xref->pre_repair_trailer = NULL;
    xref->unsaved_sigs = NULL;
    xref->unsaved_sigs_end = NULL;
}

pdf_obj *pdf_trailer(hd_context *ctx, pdf_document *doc)
{
	/* Return the document's final trailer */
	pdf_xref *xref = &doc->xref_sections[0];

	return xref ? xref->trailer : NULL;
}

void pdf_set_populating_xref_trailer(hd_context *ctx, pdf_document *doc, pdf_obj *trailer)
{
	/* Update the trailer of the xref section being populated */
	pdf_xref *xref = &doc->xref_sections[doc->num_xref_sections - 1];
	if (xref->trailer)
	{
		pdf_drop_obj(ctx, xref->pre_repair_trailer);
		xref->pre_repair_trailer = xref->trailer;
	}
	xref->trailer = pdf_keep_obj(ctx, trailer);
}

static void
pdf_drop_document_imp(hd_context *ctx, pdf_document *doc)
{
    hd_try(ctx)
    {
		pdf_drop_xref_sections(ctx, doc);
		hd_free(ctx, doc->xref_index);

		hd_drop_stream(ctx, doc->file);

        pdf_lexbuf_fin(ctx, &doc->lexbuf.base);
    }
    hd_catch(ctx)
        hd_rethrow(ctx);
}

static void
pdf_read_start_xref(hd_context *ctx, pdf_document *doc)
{
    unsigned char buf[1024];
    size_t i, n;
    int64_t t;

    hd_seek(ctx, doc->file, 0, SEEK_END);

    doc->file_size = hd_tell(ctx, doc->file);

    t = hd_max64(0, doc->file_size - (int64_t)sizeof buf);

    hd_seek(ctx, doc->file, t, SEEK_SET);

    n = hd_read(ctx, doc->file, buf, sizeof buf);
    if (n < 9)
        hd_throw(ctx, HD_ERROR_GENERIC, "cannot find startxref");

    i = n - 9;
    do
    {
        if (memcmp(buf + i, "startxref", 9) == 0)
        {
            i += 9;
            while (i < n && iswhite(buf[i]))
                i ++;
            doc->startxref = 0;
            while (i < n && buf[i] >= '0' && buf[i] <= '9')
            {
                if (doc->startxref >= INT64_MAX/10)
                    hd_throw(ctx, HD_ERROR_GENERIC, "startxref too large");
                doc->startxref = doc->startxref * 10 + (buf[i++] - '0');
            }
            if (doc->startxref != 0)
                return;
            break;
        }
    } while (i-- > 0);

    hd_throw(ctx, HD_ERROR_GENERIC, "cannot find startxref");
}

static void
hd_skip_space(hd_context *ctx, hd_stream *stm)
{
    do
    {
        int c = hd_peek_byte(ctx, stm);
        if (c > 32 && c != EOF)
            return;
        (void)hd_read_byte(ctx, stm);
    }
    while (1);
}

static int hd_skip_string(hd_context *ctx, hd_stream *stm, const char *str)
{
    while (*str)
    {
        int c = hd_peek_byte(ctx, stm);
        if (c == EOF || c != *str++)
            return 1;
        (void)hd_read_byte(ctx, stm);
    }
    return 0;
}

/*
 * trailer dictionary
 */
static int
pdf_xref_size_from_old_trailer(hd_context *ctx, pdf_document *doc, pdf_lexbuf *buf)
{
    int len;
    char *s;
    int64_t t;
    pdf_token tok;
    int c;
    int size = 0;
    int64_t ofs;
    pdf_obj *trailer = NULL;
    size_t n;

    hd_var(trailer);

    /* Record the current file read offset so that we can reinstate it */
    ofs = hd_tell(ctx, doc->file);

    hd_skip_space(ctx, doc->file);
    if (hd_skip_string(ctx, doc->file, "xref"))
        hd_throw(ctx, HD_ERROR_GENERIC, "cannot find xref marker");
    hd_skip_space(ctx, doc->file);

    while (1)
    {
        c = hd_peek_byte(ctx, doc->file);
        if (!(c >= '0' && c <= '9'))
            break;
        hd_read_line(ctx, doc->file, buf->scratch, buf->size);
        s = buf->scratch;
        hd_strsep(&s, " "); /* ignore start ,the start is usually 0*/
        if (!s)
            hd_throw(ctx, HD_ERROR_GENERIC, "xref subsection length missing");
        len = atoi(hd_strsep(&s, " "));
        if (len < 0)
            hd_throw(ctx, HD_ERROR_GENERIC, "xref subsection length must be positive");
        /* broken pdfs where the section is not on a separate line */
        if (s && *s != '\0')
            hd_seek(ctx, doc->file, -(2 + (int)strlen(s)), SEEK_CUR);

        t = hd_tell(ctx, doc->file);
        if (t < 0)
            hd_throw(ctx, HD_ERROR_GENERIC, "cannot tell in file");

        /* Spec says xref entries should be 20 bytes, but it's not infrequent
         * to see 19, in particular for some PCLm drivers. Cope. */
        if (len > 0)
        {
            n = hd_read(ctx, doc->file, (unsigned char *)buf->scratch, 20);
            if (n < 19)
                hd_throw(ctx, HD_ERROR_GENERIC, "malformed xref table");
            if (n == 20 && buf->scratch[19] > 32)
                n = 19;
        }
        else
            n = 20;

        if (len > (int64_t)((INT64_MAX - t) / n))
            hd_throw(ctx, HD_ERROR_GENERIC, "xref has too many entries");

        hd_seek(ctx, doc->file, t + n * len, SEEK_SET);
    }

	/* Skip the Cross-reference-Table to the Trailer*/
    hd_try(ctx)
    {
        tok = pdf_lex(ctx, doc->file, buf);
        if (tok != PDF_TOK_TRAILER)
            hd_throw(ctx, HD_ERROR_GENERIC, "expected trailer marker");

        tok = pdf_lex(ctx, doc->file, buf);
        if (tok != PDF_TOK_OPEN_DICT)
            hd_throw(ctx, HD_ERROR_GENERIC, "expected trailer dictionary");

		//XXX:There's another call in pdf_read_old_xref
        trailer = pdf_parse_dict(ctx, doc, doc->file, buf);

        size = pdf_to_int(ctx, pdf_dict_get(ctx, trailer, PDF_NAME_Size));
		if (size < 0 || size > PDF_MAX_OBJECT_NUMBER + 1)
			hd_throw(ctx, HD_ERROR_GENERIC, "trailer Size entry out of range");
    }
    hd_always(ctx)
    {
        pdf_drop_obj(ctx, trailer);
    }
    hd_catch(ctx)
    {
        hd_rethrow(ctx);
    }

	/* reinstate the previous position*/
	hd_seek(ctx, doc->file, ofs, SEEK_SET);

	return size;

}

static pdf_xref_entry *
pdf_xref_find_subsection(hd_context *ctx, pdf_document *doc, int start, int len)
{
	pdf_xref *xref = &doc->xref_sections[doc->num_xref_sections-1];
	pdf_xref_subsec *sub;
	int num_objects;

	/* Different cases here. Case 1) We might be asking for a
	 * subsection (or a subset of a subsection) that we already
	 * have - Just return it. Case 2) We might be asking for a
	 * completely new subsection - Create it and return it.
	 * Case 3) We might have an overlapping one - Create a 'solid'
	 * subsection and return that. */

	/* Sanity check */
	for (sub = xref->subsec; sub != NULL; sub = sub->next)
	{
		if (start >= sub->start && start + len <= sub->start + sub->len)
			return &sub->table[start-sub->start]; /* Case 1 */
		if (start + len > sub->start && start <= sub->start + sub->len)
			break; /* Case 3 */
	}

	num_objects = xref->num_objects;
	if (num_objects < start + len)
		num_objects = start + len;

	if (sub == NULL)
	{
		/* Case 2 */
		sub = hd_malloc_struct(ctx, pdf_xref_subsec);
		hd_try(ctx)
		{
			sub->table = hd_calloc(ctx, len, sizeof(pdf_xref_entry));
			sub->start = start;
			sub->len = len;
			sub->next = xref->subsec;
			xref->subsec = sub;
		}
		hd_catch(ctx)
		{
			hd_free(ctx, sub);
			hd_rethrow(ctx);
		}
		xref->num_objects = num_objects;
		if (doc->max_xref_len < num_objects)
			extend_xref_index(ctx, doc, num_objects);
	}
	else
	{
		/* Case 3 */
		ensure_solid_xref(ctx, doc, num_objects, doc->num_xref_sections-1);
		xref = &doc->xref_sections[doc->num_xref_sections-1];
		sub = xref->subsec;
	}
	return &sub->table[start-sub->start];
}

static pdf_obj *
pdf_read_old_xref(hd_context *ctx, pdf_document *doc, pdf_lexbuf *buf)
{
    int start, len, c, i, xref_len, carried;
    hd_stream *file = doc->file;
    pdf_xref_entry *table;
    pdf_token tok;
    size_t n;
    char *s;

    xref_len = pdf_xref_size_from_old_trailer(ctx, doc, buf);

	hd_skip_space(ctx, doc->file);
	if (hd_skip_string(ctx, doc->file, "xref"))
		hd_throw(ctx, HD_ERROR_GENERIC, "cannot find xref marker");
	hd_skip_space(ctx, doc->file);
	
	while (1)
	{
		c = hd_peek_byte(ctx, file);
		if (!(c >= '0' && c <= '9'))
			break;

		hd_read_line(ctx, file, buf->scratch, buf->size);
		s = buf->scratch;
		start = atoi(hd_strsep(&s, " "));
		len = atoi(hd_strsep(&s, " "));

		/* broken pdfs where the section is not on a separate line */
		if (s && *s != '\0')
		{
			hd_warn(ctx, "broken xref subsection. proceeding anyway.");
			hd_seek(ctx, file, -(2 + (int)strlen(s)), SEEK_CUR);
		}

		if (start < 0 || start > PDF_MAX_OBJECT_NUMBER
			|| len < 0 || len > PDF_MAX_OBJECT_NUMBER
			|| start + len - 1 > PDF_MAX_OBJECT_NUMBER)
		{
			hd_throw(ctx, HD_ERROR_GENERIC, "xref subsection object numbers are out of range");
		}
		/* broken pdfs where size in trailer undershoots entries in xref sections */
		if (start + len > xref_len)
		{
			hd_warn(ctx, "broken xref subsection, proceeding anyway.");
		}

		table = pdf_xref_find_subsection(ctx, doc, start, len);
		/* Xref entries SHOULD be 20 bytes long, but we see 19 byte
			 * ones more frequently than we'd like (e.g. PCLm drivers).
			 * Cope with this by 'carrying' data forward. */
		carried = 0;
		for (i = 0; i < len; i++)
		{
			pdf_xref_entry *entry = &table[i];
			n = hd_read(ctx, file, (unsigned char *) buf->scratch + carried, 20-carried);
			if (n != 20-carried)
				hd_throw(ctx, HD_ERROR_GENERIC, "unexpected EOF in xref table");
			n += carried;
			if (!entry->type)
			{
				s = buf->scratch;

				/* broken pdfs where line start with white space */
				while (*s != '\0' && iswhite(*s))
					s++;

				entry->ofs = atoll(s);
				entry->gen = atoi(s + 11);
				entry->num = start + i;
				entry->type = s[17];
				if (s[17] != 'f' && s[17] != 'n' && s[17] != 'o')
					hd_throw(ctx, HD_ERROR_GENERIC, "unexpected xref type: 0x%x (%d %d R)", s[17], entry->num, entry->gen);
				/* If the last byte of our buffer isn't an EOL (or space), carry one byte forward */
				carried = s[19] > 32;
				if (carried)
					s[0] = s[19];
			}
		}
		if (carried)
			hd_unread_byte(ctx, file);
	}

	tok = pdf_lex(ctx, file, buf);
	if (tok != PDF_TOK_TRAILER)
		hd_throw(ctx, HD_ERROR_GENERIC, "expected trailer marker");

	tok = pdf_lex(ctx, file, buf);
	if (tok != PDF_TOK_OPEN_DICT)
		hd_throw(ctx, HD_ERROR_GENERIC, "expected trailer dictionary");

	return pdf_parse_dict(ctx, doc, file, buf);
}

static pdf_obj *
pdf_read_new_xref(hd_context *ctx, pdf_document *doc, pdf_lexbuf *buf)
{
    /* TODO: no implement*/
    pdf_obj *trailer = NULL;
    return trailer;
}

static pdf_obj *
pdf_read_xref(hd_context *ctx, pdf_document *doc, int64_t ofs, pdf_lexbuf *buf)
{
    pdf_obj *trailer;
    int c;

    hd_seek(ctx, doc->file, ofs, SEEK_SET);

    while (iswhite(hd_peek_byte(ctx, doc->file)))
        hd_read_byte(ctx, doc->file);

    c = hd_peek_byte(ctx, doc->file);
    if (c == 'x')
        trailer = pdf_read_old_xref(ctx, doc, buf);
    else if (c >= '0' && c <= '9')
        trailer = pdf_read_new_xref(ctx, doc, buf);
    else
        hd_throw(ctx, HD_ERROR_GENERIC, "cannot recognize xref format");

    return trailer;
}

static int64_t
read_xref_section(hd_context *ctx, pdf_document *doc, int64_t ofs, pdf_lexbuf *buf)
{
    pdf_obj *trailer = NULL;
    int64_t xrefstmofs = 0;
    int64_t prevofs = 0;

    hd_var(trailer);

    hd_try(ctx)
    {
        trailer = pdf_read_xref(ctx, doc, ofs, buf);

        pdf_set_populating_xref_trailer(ctx, doc, trailer);

        /* FIXME: do we overwrite free entries properly? */
        /* FIXME: Does this work properly with progression? */
        xrefstmofs = pdf_to_int64(ctx, pdf_dict_get(ctx, trailer, PDF_NAME_XRefStm));
        if (xrefstmofs)
        {
            if (xrefstmofs < 0)
                hd_throw(ctx, HD_ERROR_GENERIC, "negative xref stream offset");

            /*
                Read the XRefStm stream, but throw away the resulting trailer. We do not
                follow any Prev tag therein, as specified on Page 108 of the PDF reference
                1.7
            */
            pdf_drop_obj(ctx, pdf_read_xref(ctx, doc, xrefstmofs, buf));
        }

        prevofs = pdf_to_int64(ctx, pdf_dict_get(ctx, trailer, PDF_NAME_Prev));
        if (prevofs < 0)
            hd_throw(ctx, HD_ERROR_GENERIC, "negative xref stream offset for previous xref stream");
    }
    hd_always(ctx)
    {
        pdf_drop_obj(ctx, trailer);
    }
    hd_catch(ctx)
    {
        hd_rethrow(ctx);
    }

    return prevofs;
}

static void
pdf_read_xref_sections(hd_context *ctx, pdf_document *doc, int64_t ofs, pdf_lexbuf *buf, int read_previous)
{
    int i, len, cap;
    int64_t *offsets;

    len = 0;
    cap = 10;
    offsets = hd_malloc_array(ctx, cap, sizeof(*offsets));

    hd_try(ctx)
    {
        while(ofs)
        {
            for (i = 0; i < len; i ++)
            {
                if (offsets[i] == ofs)
                    break;
            }
            if (i < len)
            {
                hd_warn(ctx, "ignoring xref section recursion at offset %lu", ofs);
                break;
            }
            if (len == cap)
            {
                cap *= 2;
                offsets = hd_resize_array(ctx, offsets, cap, sizeof(*offsets));
            }

            offsets[len++] = ofs;

            pdf_populate_next_xref_level(ctx, doc);
            ofs = read_xref_section(ctx, doc, ofs, buf);
            if (!read_previous)
                break;
        }
    }
    hd_always(ctx)
    {
        hd_free(ctx, offsets);
    }
    hd_catch(ctx)
    {
        hd_rethrow(ctx);
    }
}

pdf_xref_entry *
pdf_cache_object(hd_context *ctx, pdf_document *doc, int num)
{
    /* TODO:*/
    pdf_xref_entry *x;
    return x;
}

pdf_obj *
pdf_resolve_indirect(hd_context *ctx, pdf_obj *ref)
{
    if (pdf_is_indirect(ctx, ref))
    {
        pdf_document *doc = pdf_get_indirect_document(ctx, ref);
        int num = pdf_to_num(ctx, ref);
        pdf_xref_entry *entry;

        if (!doc)
            return NULL;
        if (num <= 0)
        {
            hd_warn(ctx, "invalid indirect reference (%d 0 R)", num);
            return NULL;
        }

        hd_try(ctx)
        entry = pdf_cache_object(ctx, doc, num);
        hd_catch(ctx)
        {
            hd_rethrow_if(ctx, HD_ERROR_TRYLATER);
            hd_warn(ctx, "cannot load object (%d 0 R) into cache", num);
            return NULL;
        }

        ref = entry->obj;
    }
    return ref;
}

pdf_obj *
pdf_resolve_indirect_chain(hd_context *ctx, pdf_obj *ref)
{
    int sanity = 10;

    while (pdf_is_indirect(ctx, ref))
    {
        if (--sanity == 0)
        {
            hd_warn(ctx, "too many indirections (possible indirection cycle involving %d 0 R)", pdf_to_num(ctx, ref));
            return NULL;
        }

        ref = pdf_resolve_indirect(ctx, ref);
    }

    return ref;
}

static pdf_document *
pdf_new_document(hd_context *ctx, hd_stream *file)
{
    pdf_document *doc = hd_new_derived_document(ctx, pdf_document);
    doc->super.drop_document = (hd_document_drop_fn *)pdf_drop_document_imp;
    pdf_lexbuf_init(ctx, &doc->lexbuf.base, PDF_LEXBUF_LARGE);
    doc->file = file;

    return doc;
}

static void
pdf_init_document(hd_context *ctx, pdf_document *doc)
{
	int repaired = 0;
    hd_try(ctx)
    {
        pdf_read_start_xref(ctx, doc);
        pdf_read_xref_sections(ctx, doc, doc->startxref, &doc->lexbuf.base, 1);
    }
    hd_catch(ctx)
    {
        hd_rethrow_if(ctx, HD_ERROR_TRYLATER);
        hd_warn(ctx, "trying to repair broken xref");
		repaired = 1;
    }

	hd_try(ctx)
	{
		if (repaired)
		{
			/*TODO:Some files are not at the bottom of the xref*/
		}
	}
	hd_catch(ctx)
	{

	}
}

pdf_document *
pdf_open_document(hd_context *ctx, const char *filename)
{
    hd_stream *file = NULL;
    pdf_document *doc = NULL;

    hd_var(file);
    hd_var(doc);

    hd_try(ctx)
    {
        file = hd_open_file(ctx, filename);
        doc = pdf_new_document(ctx, file);
        pdf_init_document(ctx, doc);
        printf("doc len is %d\n", doc->startxref);
    }
    hd_always(ctx)
    {
        hd_drop_stream(ctx, file);
    }
    hd_catch(ctx)
    {
        hd_drop_document(ctx, &doc->super);
        hd_rethrow(ctx);
    }
    return doc;
}

hd_document_handler pdf_document_handler = {
        pdf_recognize,
        (hd_document_open_fn *) pdf_open_document
};