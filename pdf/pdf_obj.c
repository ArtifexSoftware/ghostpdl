/* Copyright (C) 2020-2021 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

#include "ghostpdf.h"
#include "pdf_stack.h"
#include "pdf_array.h"
#include "pdf_dict.h"
#include "pdf_obj.h"
#include "pdf_cmap.h"
#include "pdf_font.h"
#include "pdf_deref.h" /* for replace_cache_entry() */
#include "pdf_mark.h"
#include "pdf_file.h" /* for pdfi_stream_to_buffer() */

/***********************************************************************************/
/* Functions to create the various kinds of 'PDF objects', Created objects have a  */
/* reference count of 0. Composite objects (dictionaries, arrays, strings) use the */
/* 'size' argument to create an object with the correct numbr of entries or of the */
/* requested size. Simple objects (integers etc) ignore this parameter.            */
/* Objects do not get their data assigned, that's up to the caller, but we do      */
/* set the length or size fields for composite objects.                             */

int pdfi_object_alloc(pdf_context *ctx, pdf_obj_type type, unsigned int size, pdf_obj **obj)
{
    int bytes = 0;

    switch(type) {
        case PDF_ARRAY_MARK:
        case PDF_DICT_MARK:
        case PDF_PROC_MARK:
        case PDF_NULL:
            bytes = sizeof(pdf_obj);
            break;
        case PDF_INT:
        case PDF_REAL:
            bytes = sizeof(pdf_num);
            break;
        case PDF_STRING:
        case PDF_NAME:
            bytes = sizeof(pdf_string);
            break;
        case PDF_ARRAY:
            bytes = sizeof(pdf_array);
            break;
        case PDF_DICT:
            bytes = sizeof(pdf_dict);
            break;
        case PDF_INDIRECT:
            bytes = sizeof(pdf_indirect_ref);
            break;
        case PDF_BOOL:
            bytes = sizeof(pdf_bool);
            break;
        case PDF_KEYWORD:
            bytes = sizeof(pdf_keyword);
            break;
        /* The following aren't PDF object types, but are objects we either want to
         * reference count, or store on the stack.
         */
        case PDF_XREF_TABLE:
            bytes = sizeof(xref_table_t);
            break;
        case PDF_STREAM:
            bytes = sizeof(pdf_stream);
            break;
        default:
            return_error(gs_error_typecheck);
    }
    *obj = (pdf_obj *)gs_alloc_bytes(ctx->memory, bytes, "pdfi_object_alloc");
    if (*obj == NULL)
        return_error(gs_error_VMerror);

    memset(*obj, 0x00, bytes);
    (*obj)->ctx = ctx;
    (*obj)->type = type;

    switch(type) {
        case PDF_NULL:
        case PDF_INT:
        case PDF_REAL:
        case PDF_INDIRECT:
        case PDF_BOOL:
        case PDF_ARRAY_MARK:
        case PDF_DICT_MARK:
        case PDF_PROC_MARK:
            break;
        case PDF_KEYWORD:
        case PDF_STRING:
        case PDF_NAME:
            {
                unsigned char *data = NULL;
                data = (unsigned char *)gs_alloc_bytes(ctx->memory, size, "pdfi_object_alloc");
                if (data == NULL) {
                    gs_free_object(ctx->memory, *obj, "pdfi_object_alloc");
                    *obj = NULL;
                    return_error(gs_error_VMerror);
                }
                ((pdf_string *)*obj)->data = data;
                ((pdf_string *)*obj)->length = size;
            }
            break;
        case PDF_ARRAY:
            {
                pdf_obj **values = NULL;

                ((pdf_array *)*obj)->size = size;
                if (size > 0) {
                    values = (pdf_obj **)gs_alloc_bytes(ctx->memory, size * sizeof(pdf_obj *), "pdfi_object_alloc");
                    if (values == NULL) {
                        gs_free_object(ctx->memory, *obj, "pdfi_object_alloc");
                        gs_free_object(ctx->memory, values, "pdfi_object_alloc");
                        *obj = NULL;
                        return_error(gs_error_VMerror);
                    }
                    ((pdf_array *)*obj)->values = values;
                    memset(((pdf_array *)*obj)->values, 0x00, size * sizeof(pdf_obj *));
                }
            }
            break;
        case PDF_DICT:
            {
                pdf_obj **keys = NULL, **values = NULL;

                ((pdf_dict *)*obj)->size = size;
                if (size > 0) {
                    keys = (pdf_obj **)gs_alloc_bytes(ctx->memory, size * sizeof(pdf_obj *), "pdfi_object_alloc");
                    values = (pdf_obj **)gs_alloc_bytes(ctx->memory, size * sizeof(pdf_obj *), "pdfi_object_alloc");
                    if (keys == NULL || values == NULL) {
                        gs_free_object(ctx->memory, *obj, "pdfi_object_alloc");
                        gs_free_object(ctx->memory, keys, "pdfi_object_alloc");
                        gs_free_object(ctx->memory, values, "pdfi_object_alloc");
                        *obj = NULL;
                        return_error(gs_error_VMerror);
                    }
                    ((pdf_dict *)*obj)->values = values;
                    ((pdf_dict *)*obj)->keys = keys;
                    memset(((pdf_dict *)*obj)->values, 0x00, size * sizeof(pdf_obj *));
                    memset(((pdf_dict *)*obj)->keys, 0x00, size * sizeof(pdf_obj *));
                }
            }
            break;
        /* The following aren't PDF object types, but are objects we either want to
         * reference count, or store on the stack.
         */
        case PDF_XREF_TABLE:
            break;
        default:
            break;
    }
#if REFCNT_DEBUG
    (*obj)->UID = ctx->UID++;
    dmprintf2(ctx->memory, "Allocated object of type %c with UID %"PRIi64"\n", (*obj)->type, (*obj)->UID);
#endif
    return 0;
}

/* Create a PDF number object from a numeric value. Attempts to create
 * either a REAL or INT as appropriate. As usual for the alloc functions
 * this returns an object with a reference count of 0.
 */
int pdfi_num_alloc(pdf_context *ctx, double d, pdf_num **num)
{
    uint64_t test = 0;
    int code = 0;

    test = (uint64_t)floor(d);
    if (d == test) {
        code = pdfi_object_alloc(ctx, PDF_INT, 0, (pdf_obj **)num);
        if (code < 0)
            return code;
        (*num)->value.i = test;
    }
    else {
        code = pdfi_object_alloc(ctx, PDF_REAL, 0, (pdf_obj **)num);
        if (code < 0)
            return code;
        (*num)->value.d = d;
    }

    return 0;
}

/***********************************************************************************/
/* Functions to free the various kinds of 'PDF objects'.                           */
/* All objects are reference counted, newly allocated objects, as noted above have */
/* a reference count of 0. Pushing an object onto the stack increments             */
/* its reference count, popping it from the stack decrements its reference count.  */
/* When an object's reference count is decremented to 0, pdfi_countdown calls      */
/* pdfi_free_object() to free it.                                                  */

static void pdfi_free_namestring(pdf_obj *o)
{
    /* Currently names and strings are the same, so a single cast is OK */
    pdf_name *n = (pdf_name *)o;

    if (n->data != NULL)
        gs_free_object(OBJ_MEMORY(n), n->data, "pdf interpreter free name or string data");
    gs_free_object(OBJ_MEMORY(n), n, "pdf interpreter free name or string");
}

static void pdfi_free_keyword(pdf_obj *o)
{
    /* Currently names and strings are the same, so a single cast is OK */
    pdf_keyword *k = (pdf_keyword *)o;

    if (k->data != NULL)
        gs_free_object(OBJ_MEMORY(k), k->data, "pdf interpreter free keyword data");
    gs_free_object(OBJ_MEMORY(k), k, "pdf interpreter free keyword");
}

static void pdfi_free_xref_table(pdf_obj *o)
{
    xref_table_t *xref = (xref_table_t *)o;

    gs_free_object(OBJ_MEMORY(xref), xref->xref, "pdfi_free_xref_table");
    gs_free_object(OBJ_MEMORY(xref), xref, "pdfi_free_xref_table");
}

static void pdfi_free_stream(pdf_obj *o)
{
    pdf_stream *stream = (pdf_stream *)o;

    pdfi_countdown(stream->stream_dict);
    gs_free_object(OBJ_MEMORY(o), o, "pdfi_free_stream");
}

void pdfi_free_object(pdf_obj *o)
{
    switch(o->type) {
        case PDF_ARRAY_MARK:
        case PDF_DICT_MARK:
        case PDF_PROC_MARK:
        case PDF_NULL:
        case PDF_INT:
        case PDF_REAL:
        case PDF_INDIRECT:
        case PDF_BOOL:
            gs_free_object(OBJ_MEMORY(o), o, "pdf interpreter object refcount to 0");
            break;
        case PDF_STRING:
        case PDF_NAME:
            pdfi_free_namestring(o);
            break;
        case PDF_ARRAY:
            pdfi_free_array(o);
            break;
        case PDF_DICT:
            pdfi_free_dict(o);
            break;
        case PDF_STREAM:
            pdfi_free_stream(o);
            break;
        case PDF_KEYWORD:
            pdfi_free_keyword(o);
            break;
        case PDF_XREF_TABLE:
            pdfi_free_xref_table(o);
            break;
        case PDF_FONT:
            pdfi_free_font(o);
            break;
        case PDF_CMAP:
            pdfi_free_cmap(o);
            break;
        default:
            dbgmprintf(OBJ_MEMORY(o), "!!! Attempting to free unknown obect type !!!\n");
            break;
    }
}


/* Convert a pdf_dict to a pdf_stream.
 * do_convert -- convert the stream to use same object num as dict
 *               (This assumes the dict has not been cached.)
 * The stream will come with 1 refcnt, dict refcnt will be incremented by 1.
 */
int pdfi_obj_dict_to_stream(pdf_context *ctx, pdf_dict *dict, pdf_stream **stream, bool do_convert)
{
    int code = 0;
    pdf_stream *new_stream = NULL;

    if (dict->type != PDF_DICT)
        return_error(gs_error_typecheck);

    code = pdfi_object_alloc(ctx, PDF_STREAM, 0, (pdf_obj **)&new_stream);
    if (code < 0)
        goto error_exit;

    new_stream->ctx = ctx;
    pdfi_countup(new_stream);

    new_stream->stream_dict = dict;
    pdfi_countup(dict);

    /* this replaces the dict with the stream.
     * assumes it's not cached
     */
    if (do_convert) {
        new_stream->object_num = dict->object_num;
        new_stream->generation_num = dict->generation_num;
        dict->object_num = 0;
        dict->generation_num = 0;
    }
    *stream = new_stream;
    return 0;

 error_exit:
    pdfi_countdown(new_stream);
    return code;
}

/* Create a pdf_string from a c char * */
int pdfi_obj_charstr_to_string(pdf_context *ctx, const char *charstr, pdf_string **string)
{
    int code;
    int length = strlen(charstr);
    pdf_string *newstr = NULL;

    *string = NULL;

    code = pdfi_object_alloc(ctx, PDF_STRING, length, (pdf_obj **)&newstr);
    if (code < 0) goto exit;

    memcpy(newstr->data, (byte *)charstr, length);

    *string = newstr;
    pdfi_countup(newstr);
 exit:
    return code;
}

/* Create a pdf_name from a c char * */
int pdfi_obj_charstr_to_name(pdf_context *ctx, const char *charstr, pdf_name **name)
{
    int code;
    int length = strlen(charstr);
    pdf_name *newname = NULL;

    *name = NULL;

    code = pdfi_object_alloc(ctx, PDF_NAME, length, (pdf_obj **)&newname);
    if (code < 0) goto exit;

    memcpy(newname->data, (byte *)charstr, length);

    *name = newname;
    pdfi_countup(newname);
 exit:
    return code;
}

/************ bufstream module BEGIN **************/
#define INIT_BUF_SIZE 256

typedef struct {
    int len;  /* Length of buffer */
    int cur;  /* Current position */
    byte *data;
} pdfi_bufstream_t;


static int pdfi_bufstream_init(pdf_context *ctx, pdfi_bufstream_t *stream)
{
    stream->len = INIT_BUF_SIZE;
    stream->cur = 0;
    stream->data = gs_alloc_bytes(ctx->memory, stream->len, "pdfi_bufstream_init(data)");

    if (!stream->data)
        return_error(gs_error_VMerror);
    return 0;
}

static int pdfi_bufstream_free(pdf_context *ctx, pdfi_bufstream_t *stream)
{
    if (stream->data)
        gs_free_object(ctx->memory, stream->data, "pdfi_bufstream_free(data)");
    stream->len = 0;
    stream->cur = 0;
    stream->data = NULL;
    return 0;
}

/* Grab a copy of the stream's buffer */
static int pdfi_bufstream_copy(pdf_context *ctx, pdfi_bufstream_t *stream, byte **buf, int *len)
{
    *buf = stream->data;
    *len = stream->cur;
    stream->len = 0;
    stream->cur = 0;
    stream->data = NULL;
    return 0;
}

/* Increase the size of the buffer by doubling and added the known needed amount */
static int pdfi_bufstream_increase(pdf_context *ctx, pdfi_bufstream_t *stream, uint64_t needed)
{
    byte *data = NULL;
    uint64_t newsize;

    newsize = stream->len * 2 + needed;
    data = gs_alloc_bytes(ctx->memory, newsize, "pdfi_bufstream_increase(data)");
    if (!data)
        return_error(gs_error_VMerror);

    memcpy(data, stream->data, stream->len);
    gs_free_object(ctx->memory, stream->data, "pdfi_bufstream_increase(data)");
    stream->data = data;
    stream->len = newsize;

    return 0;
}

static int pdfi_bufstream_write(pdf_context *ctx, pdfi_bufstream_t *stream, byte *data, uint64_t len)
{
    int code = 0;

    if (stream->cur + len > stream->len) {
        code = pdfi_bufstream_increase(ctx, stream, len);
        if (code < 0)
            goto exit;
    }
    memcpy(stream->data + stream->cur, data, len);
    stream->cur += len;

 exit:
    return code;
}

/************ bufstream module END **************/


/* Create a c-string to use as object label
 * Uses the object_num to make it unique
 * (don't call this for objects with object_num=0, though I am not going to check that here)
 */
int pdfi_obj_get_label(pdf_context *ctx, pdf_obj *obj, char **label)
{
    int code = 0;
    int length;
    const char *template = "{Obj%dG%d}"; /* The '{' and '}' are special to pdfmark/pdfwrite driver */
    char *string = NULL;
    pdf_indirect_ref *ref = (pdf_indirect_ref *)obj;

    *label = NULL;
    length = strlen(template)+20;

    string = (char *)gs_alloc_bytes(ctx->memory, length, "pdf_obj_get_label(label)");
    if (string == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto exit;
    }

    if (obj->type == PDF_INDIRECT)
        snprintf(string, length, template, ref->ref_object_num, ref->ref_generation_num);
    else
        snprintf(string, length, template, obj->object_num, obj->generation_num);

    *label = string;
 exit:
    return code;
}

/*********** BEGIN obj_to_string module ************/

typedef int (*str_func)(pdf_context *ctx, pdf_obj *obj, byte **data, int *len);

/* Dispatch to get string representation of an object */
typedef struct {
    pdf_obj_type type;
    str_func func;
} obj_str_dispatch_t;

static int pdfi_obj_default_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    int size = 12;
    byte *buf;

    buf = gs_alloc_bytes(ctx->memory, size, "pdfi_obj_default_str(data)");
    if (buf == NULL)
        return_error(gs_error_VMerror);
    memcpy(buf, "/placeholder", size);
    *data = buf;
    *len = size;
    return code;
}

static int pdfi_obj_name_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    pdf_name *name = (pdf_name *)obj;
    int size = name->length + 1;
    byte *buf;

    buf = gs_alloc_bytes(ctx->memory, size, "pdfi_obj_name_str(data)");
    if (buf == NULL)
        return_error(gs_error_VMerror);
    buf[0] = '/';
    memcpy(buf+1, name->data, name->length);
    *data = buf;
    *len = size;
    return code;
}

static int pdfi_obj_real_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    int size = 15;
    pdf_num *number = (pdf_num *)obj;
    char *buf;

    buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_obj_real_str(data)");
    if (buf == NULL)
        return_error(gs_error_VMerror);
    snprintf(buf, size, "%.4f", number->value.d);
    *data = (byte *)buf;
    *len = strlen(buf);
    return code;
}

static int pdfi_obj_int_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    int size = 15;
    pdf_num *number = (pdf_num *)obj;
    char *buf;

    buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_obj_int_str(data)");
    if (buf == NULL)
        return_error(gs_error_VMerror);
    snprintf(buf, size, "%ld", number->value.i);
    *data = (byte *)buf;
    *len = strlen(buf);
    return code;
}

static int pdfi_obj_getrefstr(pdf_context *ctx, uint64_t object_num, uint32_t generation, byte **data, int *len)
{
    int size = 100;
    char *buf;

    buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_obj_getrefstr(data)");
    if (buf == NULL)
        return_error(gs_error_VMerror);
    snprintf(buf, size, "%ld %d R", object_num, generation);
    *data = (byte *)buf;
    *len = strlen(buf);
    return 0;
}

static int pdfi_obj_indirect_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    pdf_indirect_ref *ref = (pdf_indirect_ref *)obj;
    char *buf;
    pdf_obj *object = NULL;
    bool use_label = true;

    if (ref->is_highlevelform) {
        code = pdfi_obj_getrefstr(ctx, ref->highlevel_object_num, 0, data, len);
        ref->is_highlevelform = false;
    } else {
        if (!ref->is_marking) {
            code = pdfi_dereference(ctx, ref->ref_object_num, ref->ref_generation_num, &object);
            if (code == gs_error_undefined) {
                /* Do something sensible for undefined reference (this would be a broken file) */
                /* TODO: Flag an error? */
                code = pdfi_obj_getrefstr(ctx, ref->ref_object_num, ref->ref_generation_num, data, len);
                goto exit;
            }
            if (code < 0 && code != gs_error_circular_reference)
                goto exit;
            if (code == 0) {
                if (object->type == PDF_STREAM) {
                    code = pdfi_mark_stream(ctx, (pdf_stream *)object);
                    if (code < 0) goto exit;
                } else if (object->type == PDF_DICT) {
                    code = pdfi_mark_dict(ctx, (pdf_dict *)object);
                    if (code < 0) goto exit;
                } else {
                    code = pdfi_obj_to_string(ctx, object, data, len);
                    if (code < 0) goto exit;
                    use_label = false;
                }
            }
        }
        if (use_label) {
            code = pdfi_obj_get_label(ctx, (pdf_obj *)ref, &buf);
            if (code < 0) goto exit;
            *data = (byte *)buf;
            *len = strlen(buf);
        }
    }

 exit:
    pdfi_countdown(object);
    return code;
}

static int pdfi_obj_bool_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    int size = 5;
    pdf_bool *bool = (pdf_bool *)obj;
    char *buf;

    buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_obj_bool_str(data)");
    if (buf == NULL)
        return_error(gs_error_VMerror);
    if (bool->value) {
        memcpy(buf, (byte *)"true", 4);
        *len = 4;
    } else {
        memcpy(buf, (byte *)"false", 5);
        *len = 5;
    }
    *data = (byte *)buf;
    return code;
}

static int pdfi_obj_null_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    int size = 4;
    char *buf;

    buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_obj_null_str(data)");
    if (buf == NULL)
        return_error(gs_error_VMerror);
    memcpy(buf, (byte *)"null", 4);
    *len = 4;
    *data = (byte *)buf;
    return code;
}

static int pdfi_obj_string_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    pdf_string *string = (pdf_string *)obj;
    int size;
    int string_len;
    char *buf;
    char *bufptr;
    bool non_ascii = false;
    int num_esc = 0;
    int i;
    byte *ptr;

    string_len = string->length;
    /* See if there are any non-ascii chars */
    for (i=0,ptr=string->data;i<string_len;i++,ptr++) {
        /* TODO: I wanted to convert non-ascii to hex strings, but there
         * are cases (such as /Author field) where the non-ascii is not really binary
         * and then pdfwrite barfs on it later.
         * see gdevpdfu.c/pdf_put_encoded_hex_string(), which is not implemented
         * and causes crashes...
         * See sample: tests_private/pdf/sumatra/1532_-_Freetype_crash.pdf
         *
         * For now, just disabling the generation of hex strings, which will match
         * what gs does.  Seems lame.
         */
#if 0
        if (*ptr > 127) {
            non_ascii = true;
            break;
        }
#endif
        /* TODO: I was going to just turn special chars into hexstrings, but it turns out
         * that the pdfwrite driver expects to be able to parse URI strings, and these
         * can have special characters.  So I will handle the minimum that seems needed for that.
         */
        switch (*ptr) {
        case '(':
        case ')':
        case '\\':
            num_esc ++;
            break;
        default:
            break;
        }
    }

    if (non_ascii) {
        size = string->length * 2 + 2;
        buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_obj_string_str(data)");
        if (buf == NULL)
            return_error(gs_error_VMerror);
        buf[0] = '<';
        for (i=0,ptr=string->data;i<string_len;i++,ptr++) {
            snprintf(buf+2*i+1, 3, "%02X", *ptr);
        }
        buf[size-1] = '>';
    } else {
        size = string->length + 2 + num_esc;
        buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_obj_string_str(data)");
        if (buf == NULL)
            return_error(gs_error_VMerror);
        buf[0] = '(';
        bufptr = buf + 1;
        for (i=0,ptr=string->data;i<string_len;i++) {
            switch (*ptr) {
            case '(':
            case ')':
            case '\\':
                *bufptr++ = '\\';
                break;
            default:
                break;
            }
            *bufptr++ = *ptr++;
        }
        buf[size-1] = ')';
    }


    *len = size;
    *data = (byte *)buf;
    return code;
}

static int pdfi_obj_array_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    pdf_array *array = (pdf_array *)obj;
    pdf_obj *object = NULL;
    byte *itembuf = NULL;
    int itemsize;
    pdfi_bufstream_t bufstream;
    uint64_t index, arraysize;

    code = pdfi_bufstream_init(ctx, &bufstream);
    if (code < 0) goto exit;

    code = pdfi_bufstream_write(ctx, &bufstream, (byte *)"[", 1);
    if (code < 0) goto exit;

    arraysize = pdfi_array_size(array);
    for (index = 0; index < arraysize; index++) {
        code = pdfi_array_get_no_deref(ctx, array, index, &object);
        if (code < 0) goto exit;

        code = pdfi_obj_to_string(ctx, object, &itembuf, &itemsize);
        if (code < 0) goto exit;

        code = pdfi_bufstream_write(ctx, &bufstream, itembuf, itemsize);
        if (code < 0) goto exit;

        gs_free_object(ctx->memory, itembuf, "pdfi_obj_array_str(itembuf)");
        itembuf = NULL;
        itemsize = 0;
        pdfi_countdown(object);
        object = NULL;

        /* Put a space between elements unless last item */
        if (index+1 != arraysize) {
            code = pdfi_bufstream_write(ctx, &bufstream, (byte *)" ", 1);
            if (code < 0) goto exit;
        }
    }

    code = pdfi_bufstream_write(ctx, &bufstream, (byte *)"]", 1);
    if (code < 0) goto exit;

    /* Now copy the results out into the string we can keep */
    code = pdfi_bufstream_copy(ctx, &bufstream, data, len);

 exit:
    if (itembuf)
        gs_free_object(ctx->memory, itembuf, "pdfi_obj_array_str(itembuf)");
    pdfi_bufstream_free(ctx, &bufstream);
    pdfi_countdown(object);
    return code;
}

static int pdfi_obj_stream_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    byte *buf;
    pdf_stream *stream = (pdf_stream *)obj;
    int64_t bufsize;
    pdf_indirect_ref *streamref = NULL;

    /* TODO: How to deal with stream dictionaries?
     * /AP is one example that has special handling (up in pdf_annot.c), but there are others.
     * See 'pushpin' annotation in annotations-galore_II.ps
     *
     * This will just literally grab the stream data.
     */
    if (stream->is_marking) {
        code = pdfi_stream_to_buffer(ctx, stream, &buf, &bufsize);
        if (code < 0) goto exit;
        *data = buf;
        *len = (int)bufsize;
    } else {
        /* Create an indirect ref for the stream */
        code = pdfi_object_alloc(ctx, PDF_INDIRECT, 0, (pdf_obj **)&streamref);
        if (code < 0) goto exit;
        pdfi_countup(streamref);
        streamref->ref_object_num = stream->object_num;
        streamref->ref_generation_num = stream->generation_num;
        code = pdfi_obj_indirect_str(ctx, (pdf_obj *)streamref, data, len);
    }

 exit:
    pdfi_countdown(streamref);
    return code;
}

/* This fetches without dereferencing.  If you want to see the references inline,
 * then you need to pre-resolve them.  See pdfi_resolve_indirect().
 */
static int pdfi_obj_dict_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    pdf_dict *dict = (pdf_dict *)obj;
    pdf_name *Key = NULL;
    pdf_obj *Value = NULL;
    byte *itembuf = NULL;
    int itemsize;
    pdfi_bufstream_t bufstream;
    uint64_t index, dictsize;
    uint64_t itemnum = 0;

    code = pdfi_bufstream_init(ctx, &bufstream);
    if (code < 0) goto exit;

    dictsize = pdfi_dict_entries(dict);
    /* Handle empty dict specially */
    if (dictsize == 0) {
        code = pdfi_bufstream_write(ctx, &bufstream, (byte *)"<< >>", 5);
        if (code < 0)
            goto exit;
        goto exit_copy;
    }

    code = pdfi_bufstream_write(ctx, &bufstream, (byte *)"<<\n", 3);
    if (code < 0) goto exit;

    /* Note: We specifically fetch without dereferencing, so there will be no circular
     * references to handle here.
     */
    /* Get each (key,val) pair from dict and setup param for it */
    code = pdfi_dict_key_first(ctx, dict, (pdf_obj **)&Key, &index);
    while (code >= 0) {
        code = pdfi_obj_to_string(ctx, (pdf_obj *)Key, &itembuf, &itemsize);
        if (code < 0) goto exit;

        code = pdfi_bufstream_write(ctx, &bufstream, itembuf, itemsize);
        if (code < 0) goto exit;

        gs_free_object(ctx->memory, itembuf, "pdfi_obj_dict_str(itembuf)");
        itembuf = NULL;
        itemsize = 0;

        /* Put a space between elements */
        code = pdfi_bufstream_write(ctx, &bufstream, (byte *)" ", 1);
        if (code < 0) goto exit;

        /* No dereference */
        code = pdfi_dict_get_no_deref(ctx, dict, (const pdf_name *)Key, &Value);
        if (code < 0) goto exit;
        code = pdfi_obj_to_string(ctx, Value, &itembuf, &itemsize);
        if (code < 0) goto exit;

        code = pdfi_bufstream_write(ctx, &bufstream, itembuf, itemsize);
        if (code < 0) goto exit;

        gs_free_object(ctx->memory, itembuf, "pdfi_obj_dict_str(itembuf)");
        itembuf = NULL;
        itemsize = 0;

        pdfi_countdown(Value);
        Value = NULL;
        pdfi_countdown(Key);
        Key = NULL;

        code = pdfi_dict_key_next(ctx, dict, (pdf_obj **)&Key, &index);
        if (code == gs_error_undefined) {
            code = 0;
            break;
        }
        if (code < 0) goto exit;

        /* Put a space between elements */
        if (++itemnum != dictsize) {
            code = pdfi_bufstream_write(ctx, &bufstream, (byte *)" ", 1);
            if (code < 0) goto exit;
        }
    }
    if (code < 0) goto exit;

    code = pdfi_bufstream_write(ctx, &bufstream, (byte *)"\n>>", 3);
    if (code < 0) goto exit;

 exit_copy:
    /* Now copy the results out into the string we can keep */
    code = pdfi_bufstream_copy(ctx, &bufstream, data, len);

 exit:
    if (itembuf)
        gs_free_object(ctx->memory, itembuf, "pdfi_obj_dict_str(itembuf)");
    pdfi_countdown(Key);
    pdfi_countdown(Value);
    pdfi_bufstream_free(ctx, &bufstream);
    return code;
}

obj_str_dispatch_t obj_str_dispatch[] = {
    {PDF_NAME, pdfi_obj_name_str},
    {PDF_ARRAY, pdfi_obj_array_str},
    {PDF_REAL, pdfi_obj_real_str},
    {PDF_INT, pdfi_obj_int_str},
    {PDF_BOOL, pdfi_obj_bool_str},
    {PDF_STRING, pdfi_obj_string_str},
    {PDF_DICT, pdfi_obj_dict_str},
    {PDF_STREAM, pdfi_obj_stream_str},
    {PDF_INDIRECT, pdfi_obj_indirect_str},
    {PDF_NULL, pdfi_obj_null_str},
    {0, NULL}
};

/* Recursive function to build a string from an object
 */
int pdfi_obj_to_string(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    obj_str_dispatch_t *dispatch_ptr;
    int code = 0;

    *data = NULL;
    *len = 0;
    for (dispatch_ptr = obj_str_dispatch; dispatch_ptr->func; dispatch_ptr ++) {
        if (obj->type == dispatch_ptr->type) {
            code = dispatch_ptr->func(ctx, obj, data, len);
            goto exit;
        }
    }
    /* Not implemented, use default */
    code = pdfi_obj_default_str(ctx, obj, data, len);
 exit:
    return code;
}

/*********** END obj_to_string module ************/
