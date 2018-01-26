//
// Created by sjw on 2018/1/15.
//

#ifndef HDCONTENTS_PDF_OBJECT_H
#define HDCONTENTS_PDF_OBJECT_H

typedef struct pdf_document_s pdf_document;

/* Defined in PDF 1.7 according to Acrobat limit. */
#define PDF_MAX_OBJECT_NUMBER 8388607

/*
 * Dynamic objects.
 * The same type of objects as found in PDF and PostScript.
 * Used by the filters and the hdContents parser.
 */

typedef struct pdf_obj_s pdf_obj;

struct pdf_obj_s
{
    short refs;
    unsigned char kind;
    unsigned char flags;
};

typedef struct pdf_obj_num_s
{
    pdf_obj super;
    union
    {
        int64_t i;
        float f;
    } u;
} pdf_obj_num;

typedef struct pdf_obj_string_s
{
    pdf_obj super;
    unsigned int len;
    char buf[1];
} pdf_obj_string;

typedef struct pdf_obj_name_s
{
    pdf_obj super;
    char n[1];
} pdf_obj_name;

typedef struct pdf_obj_array_s
{
    pdf_obj super;
    pdf_document *doc;
    int parent_num;
    int len;
    int cap;
    pdf_obj **items;
} pdf_obj_array;

typedef struct pdf_obj_dict_s
{
    pdf_obj super;
    pdf_document *doc;
    int parent_num;
    int len;
    int cap;
    struct keyval *items;
} pdf_obj_dict;

typedef struct pdf_obj_ref_s
{
    pdf_obj super;
    pdf_document *doc; /* Only needed for arrays, dicts and indirects */
    int num;
    int gen;
} pdf_obj_ref;

#define NAME(obj) ((pdf_obj_name *)(obj))
#define NUM(obj) ((pdf_obj_num *)(obj))
#define STRING(obj) ((pdf_obj_string *)(obj))
#define DICT(obj) ((pdf_obj_dict *)(obj))
#define ARRAY(obj) ((pdf_obj_array *)(obj))
#define REF(obj) ((pdf_obj_ref *)(obj))

pdf_obj *pdf_new_null(hd_context *ctx, pdf_document *doc);
pdf_obj *pdf_new_bool(hd_context *ctx, pdf_document *doc, int b);
pdf_obj *pdf_new_int(hd_context *ctx, pdf_document *doc, int64_t i);
pdf_obj *pdf_new_int_offset(hd_context *ctx, pdf_document *doc, hd_off_t off);
pdf_obj *pdf_new_real(hd_context *ctx, pdf_document *doc, float f);
pdf_obj *pdf_new_name(hd_context *ctx, pdf_document *doc, const char *str);
pdf_obj *pdf_new_string(hd_context *ctx, pdf_document *doc, const char *str, size_t len);
pdf_obj *pdf_new_text_string(hd_context *ctx, pdf_document *doc, const char *s);
pdf_obj *pdf_new_indirect(hd_context *ctx, pdf_document *doc, int num, int gen);
pdf_obj *pdf_new_array(hd_context *ctx, pdf_document *doc, int initialcap);
pdf_obj *pdf_new_dict(hd_context *ctx, pdf_document *doc, int initialcap);


/* safe, silent failure, no error reporting on type mismatches */
int pdf_to_bool(hd_context *ctx, pdf_obj *obj);
int pdf_to_int(hd_context *ctx, pdf_obj *obj);
int64_t pdf_to_int64(hd_context *ctx, pdf_obj *obj);
float pdf_to_real(hd_context *ctx, pdf_obj *obj);
const char *pdf_to_name(hd_context *ctx, pdf_obj *obj);
char *pdf_to_str_buf(hd_context *ctx, pdf_obj *obj);
int pdf_to_str_len(hd_context *ctx, pdf_obj *obj);
int pdf_to_num(hd_context *ctx, pdf_obj *obj);
int pdf_to_gen(hd_context *ctx, pdf_obj *obj);

int pdf_array_len(hd_context *ctx, pdf_obj *array);
pdf_obj *pdf_array_get(hd_context *ctx, pdf_obj *array, int i);
void pdf_array_put(hd_context *ctx, pdf_obj *array, int i, pdf_obj *obj);
void pdf_array_put_drop(hd_context *ctx, pdf_obj *array, int i, pdf_obj *obj);
void pdf_array_push(hd_context *ctx, pdf_obj *array, pdf_obj *obj);
void pdf_array_push_drop(hd_context *ctx, pdf_obj *array, pdf_obj *obj);
void pdf_array_insert(hd_context *ctx, pdf_obj *array, pdf_obj *obj, int index);
void pdf_array_insert_drop(hd_context *ctx, pdf_obj *array, pdf_obj *obj, int index);
void pdf_array_delete(hd_context *ctx, pdf_obj *array, int index);
int pdf_array_find(hd_context *ctx, pdf_obj *array, pdf_obj *obj);
int pdf_array_contains(hd_context *ctx, pdf_obj *array, pdf_obj *obj);

int pdf_dict_len(hd_context *ctx, pdf_obj *dict);
pdf_obj *pdf_dict_get_key(hd_context *ctx, pdf_obj *dict, int idx);
pdf_obj *pdf_dict_get_val(hd_context *ctx, pdf_obj *dict, int idx);
pdf_obj *pdf_dict_get(hd_context *ctx, pdf_obj *dict, pdf_obj *key);
pdf_obj *pdf_dict_getp(hd_context *ctx, pdf_obj *dict, const char *path);
pdf_obj *pdf_dict_getl(hd_context *ctx, pdf_obj *dict, ...);
pdf_obj *pdf_dict_geta(hd_context *ctx, pdf_obj *dict, pdf_obj *key, pdf_obj *abbrev);
pdf_obj *pdf_dict_gets(hd_context *ctx, pdf_obj *dict, const char *key);
pdf_obj *pdf_dict_getsa(hd_context *ctx, pdf_obj *dict, const char *key, const char *abbrev);
void pdf_dict_put(hd_context *ctx, pdf_obj *dict, pdf_obj *key, pdf_obj *val);
void pdf_dict_put_drop(hd_context *ctx, pdf_obj *dict, pdf_obj *key, pdf_obj *val);
void pdf_dict_get_put_drop(hd_context *ctx, pdf_obj *dict, pdf_obj *key, pdf_obj *val, pdf_obj **old_val);
void pdf_dict_puts(hd_context *ctx, pdf_obj *dict, const char *key, pdf_obj *val);
void pdf_dict_puts_drop(hd_context *ctx, pdf_obj *dict, const char *key, pdf_obj *val);
void pdf_dict_putp(hd_context *ctx, pdf_obj *dict, const char *path, pdf_obj *val);
void pdf_dict_putp_drop(hd_context *ctx, pdf_obj *dict, const char *path, pdf_obj *val);
void pdf_dict_putl(hd_context *ctx, pdf_obj *dict, pdf_obj *val, ...);
void pdf_dict_putl_drop(hd_context *ctx, pdf_obj *dict, pdf_obj *val, ...);
void pdf_dict_del(hd_context *ctx, pdf_obj *dict, pdf_obj *key);
void pdf_dict_dels(hd_context *ctx, pdf_obj *dict, const char *key);

int pdf_obj_parent_num(hd_context *ctx, pdf_obj *obj);

pdf_obj *pdf_keep_obj(hd_context *ctx, pdf_obj *obj);
void pdf_drop_obj(hd_context *ctx, pdf_obj *obj);

/* type queries */
int pdf_is_null(hd_context *ctx, pdf_obj *obj);
int pdf_is_bool(hd_context *ctx, pdf_obj *obj);
int pdf_is_int(hd_context *ctx, pdf_obj *obj);
int pdf_is_real(hd_context *ctx, pdf_obj *obj);
int pdf_is_number(hd_context *ctx, pdf_obj *obj);
int pdf_is_name(hd_context *ctx, pdf_obj *obj);
int pdf_is_string(hd_context *ctx, pdf_obj *obj);
int pdf_is_array(hd_context *ctx, pdf_obj *obj);
int pdf_is_dict(hd_context *ctx, pdf_obj *obj);
int pdf_is_indirect(hd_context *ctx, pdf_obj *obj);
int pdf_obj_num_is_stream(hd_context *ctx, pdf_document *doc, int num);
int pdf_is_stream(hd_context *ctx, pdf_obj *obj);
pdf_obj *pdf_resolve_obj(hd_context *ctx, pdf_obj *a);
int pdf_objcmp(hd_context *ctx, pdf_obj *a, pdf_obj *b);
int pdf_objcmp_resolve(hd_context *ctx, pdf_obj *a, pdf_obj *b);
pdf_document *pdf_get_indirect_document(hd_context *ctx, pdf_obj *obj);

static inline int pdf_name_eq(hd_context *ctx, pdf_obj *a, pdf_obj *b)
{
    if (a == b)
        return 1;
    if (a < PDF_OBJ_NAME__LIMIT && b < PDF_OBJ_NAME__LIMIT)
        return 0;
    return !pdf_objcmp_resolve(ctx, a, b);
}

/* obj marking and unmarking functions - to avoid infinite recursions. */
int pdf_obj_marked(hd_context *ctx, pdf_obj *obj);
int pdf_mark_obj(hd_context *ctx, pdf_obj *obj);
void pdf_unmark_obj(hd_context *ctx, pdf_obj *obj);

pdf_document *pdf_get_indirect_document(hd_context *ctx, pdf_obj *obj);
pdf_document *pdf_get_bound_document(hd_context *ctx, pdf_obj *obj);

#endif //HDCONTENTS_PDF_OBJECT_H
