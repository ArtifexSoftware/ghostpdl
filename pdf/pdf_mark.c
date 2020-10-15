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
#include "pdf_obj.h"

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
    gs_matrix ctm_placeholder;

    /* If ctm not provided, make a placeholder */
    if (!ctm) {
        gs_currentmatrix(ctx->pgs, &ctm_placeholder);
        ctm = &ctm_placeholder;
    }

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
    code = pdfi_dict_key_first(ctx, dict, (pdf_obj **)&Key, &index);
    while (code >= 0) {
        code = pdfi_dict_get_by_key(ctx, dict, Key, &Value);
        if (code < 0) goto exit;
        code = pdfi_mark_setparam_pair(ctx, Key, Value, parray+(keynum*2));
        if (code < 0) goto exit;

        pdfi_countdown(Key);
        Key = NULL;
        pdfi_countdown(Value);
        Value = NULL;

        code = pdfi_dict_key_next(ctx, dict, (pdf_obj **)&Key, &index);
        if (code == gs_error_undefined) {
            code = 0;
            break;
        }
        keynum ++;
    }
    if (code < 0) goto exit;

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
