/* Copyright (C) 2019-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/

/* Miscellaneous routines */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_misc.h"
#include "pdf_font_types.h"
#include "pdf_gstate.h"
#include "gspath.h"             /* For gs_strokepath() */
#include "gspaint.h"            /* For gs_erasepage() */
#include "gsicc_manage.h"       /* For gsicc_get_default_type() */
#include "gsstate.h"            /* for gs_setrenderingintent() */

/* Get current bbox, possibly from stroking current path (utility function) */
int pdfi_get_current_bbox(pdf_context *ctx, gs_rect *bbox, bool stroked)
{
    int code, code1;

    if (stroked) {
        code = pdfi_gsave(ctx);
        if (code < 0)
            return code;
        code = gs_strokepath(ctx->pgs);
        if (code < 0)
            goto exit;
    }
    code = gs_upathbbox(ctx->pgs, bbox, false);

 exit:
    if (stroked) {
        code1 = pdfi_grestore(ctx);
        if (code == 0)
            code = code1;
    }
    return code;
}

/* Get the current color space (the base one) from a color space
 */
gs_color_space_index pdfi_get_color_space_index(pdf_context *ctx, const gs_color_space *pcs)
{
    gs_color_space_index csi;

    /* Get the color space index */
    csi = gs_color_space_get_index(pcs);

    /* If its an Indexed space, then use the base space */
    if (csi == gs_color_space_index_Indexed)
        csi = gs_color_space_get_index(pcs->base_space);

    /* If its ICC, see if its a substitution for one of the device
     * spaces. If so then we will want to behave as if we were using the
     * device space.
     */
    if (csi == gs_color_space_index_ICC && pcs->cmm_icc_profile_data)
        csi = gsicc_get_default_type(pcs->cmm_icc_profile_data);

    return csi;
}

/* Get the current color space (the base one) from current graphics state.
 * index -- tells whether to pull from 0 or 1 (probably 0)
 */
gs_color_space_index pdfi_currentcolorspace(pdf_context *ctx, int index)
{
    const gs_color_space *pcs;

    pcs = ctx->pgs->color[index].color_space;

    return pdfi_get_color_space_index(ctx, pcs);
}


int
pdfi_name_strcmp(const pdf_name *n, const char *s)
{
    int len = strlen(s);
    if (n->length == len)
        return memcmp(n->data, s, len);
    return -1;
}

bool
pdfi_string_is(const pdf_string *n, const char *s)
{
    int len = strlen(s);
    if (n->length == len)
        return (memcmp(n->data, s, len) == 0);
    return false;
}

bool
pdfi_name_is(const pdf_name *n, const char *s)
{
    int len = strlen(s);
    if (n->length == len)
        return (memcmp(n->data, s, len) == 0);
    return false;
}

int
pdfi_name_cmp(const pdf_name *n1, const pdf_name *n2)
{
    if (n1->length != n2->length)
        return -1;
    return memcmp(n1->data, n2->data, n1->length);
}

int
pdfi_string_cmp(const pdf_string *n1, const pdf_string *n2)
{
    if (n1->length != n2->length)
        return -1;
    return memcmp(n1->data, n2->data, n1->length);
}

/* Set rendering intent, translating from name to number */
int pdfi_setrenderingintent(pdf_context *ctx, pdf_name *n)
{
    int code = 0;

    if (pdfi_name_is(n, "Perceptual")) {
        code = gs_setrenderingintent(ctx->pgs, 0);
    } else if (pdfi_name_is(n, "Saturation")) {
        code = gs_setrenderingintent(ctx->pgs, 2);
    } else if (pdfi_name_is(n, "RelativeColorimetric")) {
        code = gs_setrenderingintent(ctx->pgs, 1);
    } else if (pdfi_name_is(n, "AbsoluteColorimetric")) {
        code = gs_setrenderingintent(ctx->pgs, 3);
    } else {
        /* PDF 1.7 Reference, bottom of page 260 if a PDF uses an unknown rendering intent
         * then use RelativeColoimetric. But flag a warning.
         */
        pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_RENDERINGINTENT, "pdfi_setrenderingintent", "");
        code = gs_setrenderingintent(ctx->pgs, 1);
    }
    return code;
}

int pdfi_string_from_name(pdf_context *ctx, pdf_name *n, char **str, int *len)
{
    if (pdfi_type_of(n) != PDF_NAME)
        return gs_note_error(gs_error_typecheck);

    *str = NULL;
    *len = 0;

    *str = (char *)gs_alloc_bytes(ctx->memory, n->length + 1, "pdfi_string_from_name");
    if (*str == NULL)
        return gs_note_error(gs_error_VMerror);

    memcpy(*str, n->data, n->length);
    (*str)[n->length] = 0x00;
    *len = n->length;

    return 0;
}

int pdfi_free_string_from_name(pdf_context *ctx, char *str)
{
    if (str != NULL)
        gs_free_object(ctx->memory, str, "pdfi_free_string_from_name");
    return 0;
}

void normalize_rectangle(double *d)
{
    double d1[4];
    int i;

    if (d[0] < d[2]) {
        d1[0] = d[0];
        d1[2] = d[2];
    } else {
        d1[0] = d[2];
        d1[2] = d[0];
    }
    if (d[1] < d[3]) {
        d1[1] = d[1];
        d1[3] = d[3];
    } else {
        d1[1] = d[3];
        d1[3] = d[1];
    }
    for (i=0;i<=3;i++){
        d[i] = d1[i];
    }
}

/* Free an array of cstrings, sets the pointer to null */
void pdfi_free_cstring_array(pdf_context *ctx, char ***pstrlist)
{
    char **ptr = *pstrlist;

    if (ptr == NULL)
        return;

    while (*ptr) {
        gs_free_object(ctx->memory, *ptr, "pdfi_free_cstring_array(item)");
        ptr ++;
    }
    gs_free_object(ctx->memory, *pstrlist, "pdfi_free_cstring_array(array)");
    *pstrlist = NULL;
}

/* Parse an argument string of names into an array of cstrings */
/* Format: /Item1,/Item2,/Item3 (no white space) */
int pdfi_parse_name_cstring_array(pdf_context *ctx, char *data, uint64_t size, char ***pstrlist)
{
    char **strlist = NULL;
    char **templist = NULL;
    int numitems = 0, item;
    int strnum;
    uint64_t i;
    char *strptr;
    int code = 0;

    /* Free it if it already exists */
    if (*pstrlist != NULL)
        pdfi_free_cstring_array(ctx, pstrlist);

    /* find out how many '/' characters there are -- this is the max possible number
     * of items in the list
     */
    for (i=0, strptr = data; i<size; i++,strptr++) {
        if (*strptr == '/')
            numitems ++;
        /* early exit if we hit a null */
        if (*strptr == 0)
            break;
    }

    /* Allocate space for the array of char * (plus one extra for null termination) */
    strlist = (char **)gs_alloc_bytes(ctx->memory, (numitems+1)*sizeof(char *),
                                       "pdfi_parse_cstring_array(strlist)");
    if (strlist == NULL)
        return_error(gs_error_VMerror);

    memset(strlist, 0, (numitems+1)*sizeof(char *));

    /* Allocate a temp array */
    templist = (char **)gs_alloc_bytes(ctx->memory, (numitems+1)*sizeof(char *),
                                       "pdfi_parse_cstring_array(templist)");
    if (templist == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto exit;
    }

    memset(templist, 0, (numitems+1)*sizeof(char *));

    /* Find start ptr of each string */
    item = 0;
    for (i=0, strptr = data; i<size; i++,strptr++) {
        if (*strptr == '/') {
            templist[item] = strptr+1;
            item++;
        }
    }

    /* Find each substring, alloc, copy into string array */
    strnum = 0;
    for (i=0; i<numitems; i++) {
        char *curstr, *nextstr;
        int length;
        char *newstr;

        curstr = templist[i];
        nextstr = templist[i+1];
        if (!curstr)
            break;
        if (*curstr == '/' || *curstr == ',') {
            /* Empty string, skip it */
            continue;
        }
        if (nextstr == NULL) {
            length = size-(curstr-data);
        } else {
            length = nextstr - curstr - 1;
        }
        if (curstr[length-1] == ',')
            length --;

        /* Allocate the string and copy it */
        newstr = (char *)gs_alloc_bytes(ctx->memory, length+1,
                                       "pdfi_parse_cstring_array(newstr)");
        if (newstr == NULL) {
            code = gs_note_error(gs_error_VMerror);
            goto exit;
        }
        memcpy(newstr, curstr, length);
        newstr[length+1] = 0; /* Null terminate */
        strlist[strnum] = newstr;
        strnum ++;
    }

    *pstrlist = strlist;

 exit:
    if (code < 0)
        pdfi_free_cstring_array(ctx, &strlist);
    if (templist)
        gs_free_object(ctx->memory, templist, "pdfi_parse_cstring_array(templist(array))");
    return code;
}
