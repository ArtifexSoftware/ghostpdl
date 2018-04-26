/* Copyright (C) 2001-2018 Artifex Software, Inc.
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
#include "pdf_types.h"

#ifndef PDF_INTERPRETER
#define PDF_INTERPRETER

int pdf_read_token(pdf_context *ctx, pdf_stream *s);
int pdf_read_object(pdf_context *ctx, pdf_stream *s);
void pdf_free_object(pdf_obj *o);

int pdf_make_name(pdf_context *ctx, byte *key, uint32_t size, pdf_obj **o);
int pdf_make_dict(pdf_context *ctx, uint64_t size, pdf_dict **returned);

int pdf_dict_known(pdf_dict *d, char *Key, bool *known);
int pdf_dict_known_by_key(pdf_dict *d, pdf_name *Key, bool *known);
int pdf_dict_put(pdf_dict *d, pdf_obj *Key, pdf_obj *value);
int pdf_dict_get(pdf_dict *d, const char *Key, pdf_obj **o);
int pdf_dict_copy(pdf_dict *target, pdf_dict *source);

int pdf_array_get(pdf_array *a, uint64_t index, pdf_obj **o);

int pdf_dereference(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object);

int repair_pdf_file(pdf_context *ctx);

static inline void pdf_countup(pdf_obj *o)
{
    if (o != NULL) {
        o->refcnt++;
#if REFCNT_DEBUG
    dmprintf3(o->memory, "Incrementing reference count of object %"PRIi64", UID %"PRIi64", to %d\n", o->object_num, o->UID, o->refcnt);
#endif
    }
#if REFCNT_DEBUG
    else {
        dprintf("Incrementing reference count of NULL pointer\n");
    }
#endif
}

static inline void pdf_countdown(pdf_obj *o)
{
    if (o != NULL) {
#ifdef DEBUG
        if (o->refcnt == 0)
            emprintf(o->memory, "Decrementing objct with recount at 0!\n");
#endif
        o->refcnt--;
#if REFCNT_DEBUG
        dmprintf3(o->memory, "Decrementing reference count of object %"PRIi64", UID %"PRIi64", to %d\n", o->object_num, o->UID, o->refcnt);
#endif
        if (o->refcnt == 0) {
#if REFCNT_DEBUG
            dmprintf2(o->memory, "Freeing object %"PRIi64", UID %"PRIi64"\n", o->object_num, o->UID);
#endif
                pdf_free_object(o);
        }
    }
#if REFCNT_DEBUG
    else {
        dprintf("Decrementing reference count of NULL pointer\n");
    }
#endif
}


#endif
