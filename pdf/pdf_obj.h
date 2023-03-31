/* Copyright (C) 2020-2023 Artifex Software, Inc.
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

#ifndef PDF_OBJECTS
#define PDF_OBJECTS

int pdfi_object_alloc(pdf_context *ctx, pdf_obj_type type, unsigned int size, pdf_obj **obj);
void pdfi_free_object(pdf_obj *o);
int pdfi_obj_to_string(pdf_context *ctx, pdf_obj *obj, byte **data, int *len);
int pdfi_obj_dict_to_stream(pdf_context *ctx, pdf_dict *dict, pdf_stream **stream, bool do_convert);
int pdfi_stream_to_dict(pdf_context *ctx, pdf_stream *stream, pdf_dict **dict);
int pdfi_obj_charstr_to_string(pdf_context *ctx, const char *charstr, pdf_string **string);
int pdfi_obj_charstr_to_name(pdf_context *ctx, const char *charstr, pdf_name **name);
int pdfi_obj_get_label(pdf_context *ctx, pdf_obj *obj, char **label);
int pdfi_num_alloc(pdf_context *ctx, double d, pdf_num **num);

static inline int
pdfi_obj_to_real(pdf_context *ctx, pdf_obj *obj, double *d)
{
    pdf_num *num = (pdf_num *)obj;

    switch (pdfi_type_of(num)) {
        case PDF_INT:
            *d = (double)num->value.i;
            break;
        case PDF_REAL:
            *d = num->value.d;
            break;
        default:
            return_error(gs_error_typecheck);
    }

    return 0;
}

static inline int
pdfi_obj_to_float(pdf_context *ctx, pdf_obj *obj, float *f)
{
    pdf_num *num = (pdf_num *)obj;

    switch (pdfi_type_of(num)) {
        case PDF_INT:
            *f = (float)num->value.i;
            break;
        case PDF_REAL:
            *f = (float)num->value.d;
            break;
        default:
            return_error(gs_error_typecheck);
    }

    return 0;
}

static inline int
pdfi_obj_to_int(pdf_context *ctx, pdf_obj *obj, int64_t *i)
{
    pdf_num *num = (pdf_num *)obj;
    int64_t tmp;

    switch (pdfi_type_of(num)) {
        case PDF_INT:
            *i = num->value.i;
            break;
        case PDF_REAL:
            /* We shouldn't be given a real here. We will grudgingly accept
             * (with a warning) an int given as a real, but will error out
             * otherwise. If we find a case where we need to accept reals
             * as ints, we'll do a new version of this function called something
             * like pdfi_obj_real_as_int what will just cast it down. */
            tmp = (int64_t)num->value.d;
            if ((double)tmp != num->value.d) {
                return_error(gs_error_typecheck);
            }
            pdfi_set_warning(ctx, 0, NULL, W_PDF_INT_AS_REAL, "pdfi_obj_to_int", NULL);
            *i = tmp;
            break;
        default:
            return_error(gs_error_typecheck);
    }

    return 0;
}

/* NOTE: the buffer object takes ownership of "data" */
static inline int
pdfi_buffer_set_data(pdf_obj *o, byte *data, int32_t length)
{
    pdf_buffer *b = (pdf_buffer *)o;
    if (pdfi_type_of(b) != PDF_BUFFER) {
        return_error(gs_error_typecheck);
    }

    if (b->data) {
        gs_free_object(OBJ_MEMORY(b), b->data, "pdfi_buffer_set_data(data)");
    }
    b->data = data;
    b->length = length;
    return 0;
}


#endif
