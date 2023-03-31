/* Copyright (C) 2018-2023 Artifex Software, Inc.
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

/* code for handling circular references */

#include "pdf_int.h"
#include "pdf_loop_detect.h"

static int pdfi_init_loop_detector(pdf_context *ctx)
{
    if (ctx->loop_detection) {
        dbgmprintf(ctx->memory, "Attempt to initialise loop detector while one is in operation\n");
        return_error(gs_error_unknownerror);
    }

    ctx->loop_detection = (uint64_t *)gs_alloc_bytes(ctx->memory, INITIAL_LOOP_TRACKER_SIZE * sizeof (uint64_t), "allocate loop tracking array");
    if (ctx->loop_detection == NULL)
        return_error(gs_error_VMerror);

    ctx->loop_detection_entries = 0;
    ctx->loop_detection_size = INITIAL_LOOP_TRACKER_SIZE;
    return 0;
}

static int pdfi_free_loop_detector(pdf_context *ctx)
{
    if (ctx->loop_detection == NULL) {
        dbgmprintf(ctx->memory, "Attempt to free loop detector without initialising it\n");
        return 0;
    }
    if (ctx->loop_detection != NULL)
        gs_free_object(ctx->memory, ctx->loop_detection, "Free array for loop tracking");
    ctx->loop_detection_entries = 0;
    ctx->loop_detection_size = 0;
    ctx->loop_detection = NULL;

    return 0;
}

int pdfi_loop_detector_add_object(pdf_context *ctx, uint64_t object)
{
    if (ctx->loop_detection == NULL) {
        dbgmprintf(ctx->memory, "Attempt to use loop detector without initialising it\n");
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

bool pdfi_loop_detector_check_object(pdf_context *ctx, uint64_t object)
{
    int i = 0;

    if (ctx->loop_detection == NULL) {
        dbgmprintf(ctx->memory, "Attempt to use loop detector without initialising it\n");
        return 0;
    }

    for (i=0;i < ctx->loop_detection_entries;i++) {
        if (ctx->loop_detection[i] == object) {
            char info_string[256];
            gs_snprintf(info_string, sizeof(info_string), "Error! circular reference to object %"PRIu64" detected.\n", object);
            pdfi_set_error(ctx, 0, NULL, E_PDF_CIRCULARREF, "pdfi_loop_detector_check_object", info_string);
            return true;
        }
    }
    return false;
}

int pdfi_loop_detector_mark(pdf_context *ctx)
{
    int code = 0;

    if (ctx->loop_detection == NULL) {
        code = pdfi_init_loop_detector(ctx);
        if (code < 0)
            return code;
    }

    return pdfi_loop_detector_add_object(ctx, 0);
}

int pdfi_loop_detector_cleartomark(pdf_context *ctx)
{
    if (ctx->loop_detection == NULL) {
        dbgmprintf(ctx->memory, "Attempt to use loop detector without initialising it\n");
        return 0;
    }

    while (ctx->loop_detection[--ctx->loop_detection_entries] != 0) {
        ctx->loop_detection[ctx->loop_detection_entries] = 0;
    }
    /* FIXME - potential optimisation
     * Instead of freeing the loop detection array every tiome we are done with it
     * and then reallocating a new one next time we need one, we could just keep
     * the existing (empty) array. I suspect this would provide a small performance
     * improvement.
     */
    if (ctx->loop_detection_entries == 0)
        pdfi_free_loop_detector(ctx);
    return 0;
}
