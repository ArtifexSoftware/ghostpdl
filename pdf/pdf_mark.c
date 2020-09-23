/* Copyright (C) 2001-2020 Artifex Software, Inc.
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

/* pdfmark handling for the PDF interpreter */

#include "pdf_int.h"
#include "plmain.h"
#include "pdf_stack.h"
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_loop_detect.h"
#include "pdf_mark.h"

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


/*********** BEGIN obj_to_string module ************/

typedef int (*str_func)(pdf_context *ctx, pdf_obj *obj, byte **data, int *len);

/* Dispatch to get string representation of an object */
typedef struct {
    pdf_obj_type type;
    str_func func;
} obj_str_dispatch_t;

static int pdfi_obj_to_string(pdf_context *ctx, pdf_obj *obj, byte **data, int *len);

/* Write the pdfmark command to the device */
static int pdfi_mark_write(pdf_context *ctx, gs_param_string_array *array_list)
{
    gs_c_param_list list;
    int code;

    /* Set the list to writeable, and initialise it */
    gs_c_param_list_write(&list, ctx->memory);
    /* We don't want keys to be persistent, as we are going to throw
     * away our array, force them to be copied
     */
    gs_param_list_set_persistent_keys((gs_param_list *) &list, false);

    /* Make really sure the list is writable, but don't initialise it */
    gs_c_param_list_write_more(&list);

    /* Add the param string array to the list */
    code = param_write_string_array((gs_param_list *)&list, "pdfmark", array_list);
    if (code < 0)
        return code;

    /* Set the param list back to readable, so putceviceparams can readit (mad...) */
    gs_c_param_list_read(&list);

    /* and set the actual device parameters */
    code = gs_putdeviceparams(ctx->pgs->device, (gs_param_list *)&list);

    gs_c_param_list_release(&list);

    return code;
}

static int pdfi_mark_default_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    int size = 12;
    byte *buf;

    buf = gs_alloc_bytes(ctx->memory, size, "pdfi_mark_default_str(data)");
    if (buf == NULL)
        return_error(gs_error_VMerror);
    memcpy(buf, "/placeholder", size);
    *data = buf;
    *len = size;
    return code;
}

static int pdfi_mark_name_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    pdf_name *name = (pdf_name *)obj;
    int size = name->length + 1;
    byte *buf;

    buf = gs_alloc_bytes(ctx->memory, size, "pdfi_mark_name_str(data)");
    if (buf == NULL)
        return_error(gs_error_VMerror);
    buf[0] = '/';
    memcpy(buf+1, name->data, name->length);
    *data = buf;
    *len = size;
    return code;
}

static int pdfi_mark_real_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    int size = 15;
    pdf_num *number = (pdf_num *)obj;
    char *buf;

    buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_mark_real_str(data)");
    if (buf == NULL)
        return_error(gs_error_VMerror);
    snprintf(buf, size, "%.4f", number->value.d);
    *data = (byte *)buf;
    *len = strlen(buf);
    return code;
}

static int pdfi_mark_int_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    int size = 15;
    pdf_num *number = (pdf_num *)obj;
    char *buf;

    buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_mark_int_str(data)");
    if (buf == NULL)
        return_error(gs_error_VMerror);
    snprintf(buf, size, "%ld", number->value.i);
    *data = (byte *)buf;
    *len = strlen(buf);
    return code;
}

static int pdfi_mark_indirect_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    int size = 100;
    pdf_indirect_ref *ref = (pdf_indirect_ref *)obj;
    char *buf;

    buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_mark_indirect_str(data)");
    if (buf == NULL)
        return_error(gs_error_VMerror);
    snprintf(buf, size, "%ld %d R", ref->ref_object_num, ref->ref_generation_num);
    *data = (byte *)buf;
    *len = strlen(buf);
    return code;
}

static int pdfi_mark_bool_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    int size = 5;
    pdf_bool *bool = (pdf_bool *)obj;
    char *buf;

    buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_mark_bool_str(data)");
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

static int pdfi_mark_string_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    pdf_string *string = (pdf_string *)obj;
    int size;
    char *buf;
    bool non_ascii = false;
    int i;
    byte *ptr;

    /* See if there are any non-ascii chars */
    for (i=0,ptr=string->data;i<string->length;i++,ptr++) {
        if (*ptr > 127) {
            non_ascii = true;
            break;
        }
    }

    if (non_ascii) {
        size = string->length * 2 + 2;
        buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_mark_string_str(data)");
        if (buf == NULL)
            return_error(gs_error_VMerror);
        buf[0] = '<';
        for (i=0,ptr=string->data;i<string->length;i++,ptr++) {
            snprintf(buf+2*i+1, 3, "%02X", *ptr);
        }
        buf[size-1] = '>';
    } else {
        size = string->length + 2;
        buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_mark_string_str(data)");
        if (buf == NULL)
            return_error(gs_error_VMerror);
        buf[0] = '(';
        memcpy(buf+1, string->data, string->length);
        buf[size-1] = ')';
    }


    *len = size;
    *data = (byte *)buf;
    return code;
}

static int pdfi_mark_array_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
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

        gs_free_object(ctx->memory, itembuf, "pdfi_mark_array_str(itembuf)");
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
        gs_free_object(ctx->memory, itembuf, "pdfi_mark_array_str(itembuf)");
    pdfi_bufstream_free(ctx, &bufstream);
    pdfi_countdown(object);
    return code;
}

/* This fetches without dereferencing.  If you want to see the references inline,
 * then you need to pre-resolve them.  See pdfi_resolve_indirect().
 */
static int pdfi_mark_dict_str(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
{
    int code = 0;
    pdf_dict *dict = (pdf_dict *)obj;
    pdf_name *Key = NULL;
    pdf_obj *Value = NULL;
    byte *itembuf = NULL;
    int itemsize;
    pdfi_bufstream_t bufstream;
    uint64_t index, dictsize;

    code = pdfi_bufstream_init(ctx, &bufstream);
    if (code < 0) goto exit;

    /* TODO: How to deal with stream dictionaries?
     * /AP is one example that has special handling (up in pdf_annot.c), but there are others.
     * See 'pushpin' annotation in annotations-galore_II.ps
     *
     * For now we simply put in an empty dictionary << >>
     * I don't want to just grab the representation of the dictionary part,
     * because when this is used with annotations that would result in an invalid
     * stream in the output.
     */
    if (pdfi_dict_is_stream(ctx, dict)) {
        code = pdfi_bufstream_write(ctx, &bufstream, (byte *)"<< >>", 5);
        goto exit_copy;
    }

    code = pdfi_bufstream_write(ctx, &bufstream, (byte *)"<<\n", 3);
    if (code < 0) goto exit;

    dictsize = pdfi_dict_entries(dict);

    /* Note: We specifically fetch without dereferencing, so there will be no circular
     * references to handle here.
     */
    /* Get each (key,val) pair from dict and setup param for it */
    for (index=0; index<dictsize; index ++) {
        Key = (pdf_name *)dict->keys[index];
        code = pdfi_obj_to_string(ctx, (pdf_obj *)Key, &itembuf, &itemsize);
        if (code < 0)
            goto exit;

        code = pdfi_bufstream_write(ctx, &bufstream, itembuf, itemsize);
        if (code < 0)
            goto exit;

        gs_free_object(ctx->memory, itembuf, "pdfi_mark_dict_str(itembuf)");
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

        gs_free_object(ctx->memory, itembuf, "pdfi_mark_dict_str(itembuf)");
        itembuf = NULL;
        itemsize = 0;

        pdfi_countdown(Value);
        Value = NULL;

        /* Put a space between elements */
        if (index + 1 != dictsize) {
            code = pdfi_bufstream_write(ctx, &bufstream, (byte *)" ", 1);
            if (code < 0) goto exit;
        }
    }

    code = pdfi_bufstream_write(ctx, &bufstream, (byte *)"\n>>", 3);
    if (code < 0)
        goto exit;


 exit_copy:
    /* Now copy the results out into the string we can keep */
    code = pdfi_bufstream_copy(ctx, &bufstream, data, len);

 exit:
    if (itembuf)
        gs_free_object(ctx->memory, itembuf, "pdfi_mark_dict_str(itembuf)");
    pdfi_countdown(Value);
    pdfi_bufstream_free(ctx, &bufstream);
    return code;
}

obj_str_dispatch_t obj_str_dispatch[] = {
    {PDF_NAME, pdfi_mark_name_str},
    {PDF_ARRAY, pdfi_mark_array_str},
    {PDF_REAL, pdfi_mark_real_str},
    {PDF_INT, pdfi_mark_int_str},
    {PDF_BOOL, pdfi_mark_bool_str},
    {PDF_STRING, pdfi_mark_string_str},
    {PDF_DICT, pdfi_mark_dict_str},
    {PDF_INDIRECT, pdfi_mark_indirect_str},
    {0, NULL}
};

/* Recursive function to build a string from an object
 */
static int pdfi_obj_to_string(pdf_context *ctx, pdf_obj *obj, byte **data, int *len)
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
    code = pdfi_mark_default_str(ctx, obj, data, len);
 exit:
    return code;
}

/*********** END obj_to_string module ************/

static int pdfi_mark_setparam_obj(pdf_context *ctx, pdf_obj *obj, gs_param_string *entry)
{
    int code = 0;
    byte *data = NULL;
    int size = 0;

    code = pdfi_obj_to_string(ctx, obj, &data, &size);
    if (code < 0)
        return code;
    entry->data = data;
    entry->size = size;
    entry->persistent = false;
    return 0;
}

static int pdfi_mark_setparam_pair(pdf_context *ctx, pdf_name *Key, pdf_obj *Value,
                                   gs_param_string *entry)
{
    int code = 0;

    /* Handle the Key */
    if (Key->type != PDF_NAME) {
        code = gs_note_error(gs_error_typecheck);
        goto exit;
    }

    code = pdfi_mark_setparam_obj(ctx, (pdf_obj *)Key, entry);
    if (code < 0)
        goto exit;

    code = pdfi_mark_setparam_obj(ctx, Value, entry+1);
    if (code < 0)
        goto exit;

 exit:
    return code;
}


/* Note: this isn't part of the obj_to_string() stuff */
static int pdfi_mark_ctm_str(pdf_context *ctx, gs_matrix *ctm, byte **data, int *len)
{
    int size = 100;
    char *buf;

    buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_mark_ctm_str(data)");
    if (buf == NULL)
        return_error(gs_error_VMerror);
    snprintf(buf, size, "[%.4f %.4f %.4f %.4f %.4f%.4f]",
             ctm->xx, ctm->xy, ctm->yx, ctm->yy, ctm->tx, ctm->ty);
    *data = (byte *)buf;
    *len = strlen(buf);
    return 0;
}

/* Apparently the strings to pass to the device are:
   key1 val1 .... keyN valN CTM "ANN"
   CTM is (for example) "[1.0 0 0 1.0 0 0]"

   the /pdfmark command has this array of strings as a parameter, i.e.
   [ key1 val1 .... keyN valN CTM "ANN" ] /pdfmark

   Apparently the "type" doesn't have a leading "/" but the other names in the
   keys do need the "/"

   See plparams.c/process_pdfmark()

*/
int pdfi_mark_from_dict(pdf_context *ctx, pdf_dict *dict, gs_matrix *ctm, const char *type)
{
    int code = 0;
    int size;
    uint64_t dictsize;
    uint64_t index;
    uint64_t keynum = 0;
    pdf_name *Key = NULL;
    pdf_obj *Value = NULL;
    gs_param_string *parray = NULL;
    gs_param_string_array array_list;
    byte *ctm_data = NULL;
    int ctm_len;

    dictsize = pdfi_dict_entries(dict);
    size = dictsize*2 + 2; /* pairs + CTM + type */

    parray = (gs_param_string *)gs_alloc_bytes(ctx->memory, size*sizeof(gs_param_string),
                                               "pdfi_mark_from_dict(parray)");
    if (parray == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto exit;
    }
    memset(parray, 0, size *sizeof(gs_param_string));


    /* Get each (key,val) pair from dict and setup param for it */
    code = pdfi_dict_first(ctx, dict, (pdf_obj **)&Key, &Value, &index);
    if (code < 0)
        goto exit;
    do {
        pdfi_mark_setparam_pair(ctx, Key, Value, parray+(keynum*2));

        pdfi_countdown(Key);
        Key = NULL;
        pdfi_countdown(Value);
        Value = NULL;

        if (++keynum == dictsize)
            break;
        code = pdfi_dict_next(ctx, dict, (pdf_obj **)&Key, &Value, &index);
        if (code < 0)
            goto exit;
    } while (1);

    /* CTM */
    code = pdfi_mark_ctm_str(ctx, ctm, &ctm_data, &ctm_len);
    if (code < 0) goto exit;
    parray[size-2].data = ctm_data;
    parray[size-2].size = ctm_len;

    /* Type (e.g. ANN, DOCINFO) */
    parray[size-1].data = (const byte *)type;
    parray[size-1].size = strlen(type);

    array_list.data = parray;
    array_list.persistent = false;
    array_list.size = size;

    code = pdfi_mark_write(ctx, &array_list);

 exit:
    pdfi_countdown(Key);
    Key = NULL;
    pdfi_countdown(Value);
    Value = NULL;
    for (keynum = 0; keynum < dictsize; keynum ++) {
        gs_free_object(ctx->memory, (byte *)parray[keynum*2].data, "pdfi_mark_from_dict(parray)");
        gs_free_object(ctx->memory, (byte *)parray[keynum*2+1].data, "pdfi_mark_from_dict(parray)");
    }
    if (ctm_data)
        gs_free_object(ctx->memory, ctm_data, "pdfi_mark_from_dict(ctm_data)");
    gs_free_object(ctx->memory, parray, "pdfi_mark_from_dict(parray)");
    return code;
}
