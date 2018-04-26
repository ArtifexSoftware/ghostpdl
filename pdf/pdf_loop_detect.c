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

/* code for handling circular references */

#include "pdf_int.h"

int pdf_init_loop_detector(pdf_context *ctx)
{
    if (ctx->loop_detection) {
#ifdef DEBUG
        dmprintf(ctx->memory, "Attempt to initialise loop detector while one is in operation\n");
#endif
        return_error(gs_error_unknownerror);
    }

    ctx->loop_detection = (uint64_t *)gs_alloc_bytes(ctx->memory, INITIAL_LOOP_TRACKER_SIZE * sizeof (uint64_t), "allocate loop tracking array");
    if (ctx->loop_detection == NULL)
        return_error(gs_error_VMerror);

    ctx->loop_detection_entries = 0;
    ctx->loop_detection_size = INITIAL_LOOP_TRACKER_SIZE;
    return 0;
}

int pdf_free_loop_detector(pdf_context *ctx)
{
    if (ctx->loop_detection == NULL) {
#ifdef DEBUG
        dmprintf(ctx->memory, "Attempt to free loop detector without initialising it\n");
#endif
        return 0;
    }
    if (ctx->loop_detection != NULL)
        gs_free_object(ctx->memory, ctx->loop_detection, "Free array for loop tracking");
    ctx->loop_detection_entries = 0;
    ctx->loop_detection_size = 0;
    ctx->loop_detection = NULL;

    return 0;
}

int pdf_loop_detector_add_object(pdf_context *ctx, uint64_t object)
{
    if (ctx->loop_detection == NULL) {
#ifdef DEBUG
        dmprintf(ctx->memory, "Attempt to use loop detector without initialising it\n");
#endif
        return 0;
    }

    if (ctx->loop_detection_entries == ctx->loop_detection_size) {
        uint64_t *New;

        New = (uint64_t *)gs_alloc_bytes(ctx->memory, (ctx->loop_detection_size + INITIAL_LOOP_TRACKER_SIZE) * sizeof (uint64_t), "re-allocate loop tracking array");
        if (New == NULL) {
            return_error(gs_error_VMerror);
        }
        memcpy(New, ctx->loop_detection, ctx->loop_detection_entries * sizeof(uint64_t));
        gs_free_object(ctx->memory, ctx->loop_detection, "Free array for loop tracking");
        ctx->loop_detection_size += INITIAL_LOOP_TRACKER_SIZE;
        ctx->loop_detection = New;
    }
    ctx->loop_detection[ctx->loop_detection_entries++] = object;
    return 0;
}

bool pdf_loop_detector_check_object(pdf_context *ctx, uint64_t object)
{
    int i = 0;

    if (ctx->loop_detection == NULL) {
#ifdef DEBUG
        dmprintf(ctx->memory, "Attempt to use loop detector without initialising it\n");
#endif
        return 0;
    }

    for (i=0;i < ctx->loop_detection_entries;i++) {
        if (ctx->loop_detection[i] == object)
            return true;
    }
    return false;
}

int pdf_loop_detector_mark(pdf_context *ctx)
{
    if (ctx->loop_detection == NULL) {
#ifdef DEBUG
        dmprintf(ctx->memory, "Attempt to use loop detector without initialising it\n");
#endif
        return 0;
    }

    return pdf_loop_detector_add_object(ctx, 0);
}

int pdf_loop_detector_cleartomark(pdf_context *ctx)
{
    if (ctx->loop_detection == NULL) {
#ifdef DEBUG
        dmprintf(ctx->memory, "Attempt to use loop detector without initialising it\n");
#endif
        return 0;
    }

    while (ctx->loop_detection[--ctx->loop_detection_entries] != 0) {
        ctx->loop_detection[ctx->loop_detection_entries] = 0;
    }
    return 0;
}
