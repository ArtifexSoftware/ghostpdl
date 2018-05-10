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

int pdf_pop(pdf_context *ctx, int num);
int pdf_push(pdf_context *ctx, pdf_obj *o);
int pdf_mark_stack(pdf_context *ctx, pdf_obj_type type);
void pdf_clearstack(pdf_context *ctx);
int pdf_count_to_mark(pdf_context *ctx, uint64_t *count);
int pdf_clear_to_mark(pdf_context *ctx);

static inline void pdf_countup_impl(pdf_obj *o)
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

static inline void pdf_countdown_impl(pdf_obj *o)
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

#define pdf_countup(x) pdf_countup_impl((pdf_obj *)x)

#define pdf_countdown(x) pdf_countdown_impl((pdf_obj *)x)

#endif
