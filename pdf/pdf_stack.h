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

/* Stack operations for the PDF interpreter */

#ifndef PDF_STACK_OPERATIONS
#define PDF_STACK_OPERATIONS

#include "pdf_int.h"

int pdfi_pop(pdf_context *ctx, int num);
int pdfi_push(pdf_context *ctx, pdf_obj *o);
int pdfi_mark_stack(pdf_context *ctx, pdf_obj_type type);
void pdfi_clearstack(pdf_context *ctx);
int pdfi_count_to_mark(pdf_context *ctx, uint64_t *count);
int pdfi_clear_to_mark(pdf_context *ctx);

static inline void pdfi_countup_impl(pdf_obj *o)
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

static inline void pdfi_countdown_impl(pdf_obj *o)
{
    if (o != NULL) {
#ifdef DEBUG
        if (o->refcnt == 0)
            emprintf(o->memory, "Decrementing object with refcount at 0!\n");
#endif
        o->refcnt--;
#if REFCNT_DEBUG
        dmprintf3(o->memory, "Decrementing reference count of object %"PRIi64", UID %"PRIi64", to %d\n", o->object_num, o->UID, o->refcnt);
#endif
        if (o->refcnt == 0) {
#if REFCNT_DEBUG
            pdf_context *ctx = (pdf_context *)o->ctx;
            if (ctx != NULL && ctx->cache_entries != 0) {
                pdf_obj_cache_entry *entry = ctx->cache_LRU, *next;

                while(entry) {
                    next = entry->next;
                    if (entry->o->object_num != 0 && entry->o->object_num == o->object_num)
                        dmprintf2(o->memory, "Freeing object %"PRIi64", UID %"PRIi64", but there is still a cache entry!\n", o->object_num, o->UID);
                    entry = next;
                }
                ctx->cache_LRU = ctx->cache_MRU = NULL;
                ctx->cache_entries = 0;
            }
            dmprintf2(o->memory, "Freeing object %"PRIi64", UID %"PRIi64"\n", o->object_num, o->UID);
#endif
                pdfi_free_object(o);
        }
    }
}

/* These two macros are present simply to add a cast to the generic object type, so that
 * we don't get warnings in the implementation routines, the alternative would be to use
 * a cast everywhere we use the inline functions above, or to have them take a void *
 *
 * Ordinarily we would capitalise the name of a macro to differentiate it from a function
 * we make an exception in this case because hte macro descends to an inline function which
 * can be debugged without expanding macros.
 */
#define pdfi_countup(x) pdfi_countup_impl((pdf_obj *)x)

#define pdfi_countdown(x) pdfi_countdown_impl((pdf_obj *)x)

#endif
