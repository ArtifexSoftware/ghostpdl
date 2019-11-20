/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* pxstate.c */
/* State allocation/initialization/cleanup */

#include "stdio_.h"             /* std.h + NULL */
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "scommon.h"
#include "pxparse.h"
#include "pxstate.h"
#include "pxfont.h"
#include "gsfont.h"
#include "gxfcache.h"

/* Allocate a px_state_t. */
px_state_t *
px_state_alloc(gs_memory_t * memory)
{
    px_state_t *pxs = (px_state_t *) gs_alloc_bytes(memory,
                                                    sizeof(px_state_t),
                                                    "px_state_alloc");
    px_gstate_t *pxgs = px_gstate_alloc(memory);

    if (pxs == 0 || pxgs == 0)
        goto fail;

    pxs->memory = memory;
    pxs->pxgs = pxgs;
    pxgs->pxs = pxs;
    px_state_init(pxs, NULL);

    /* allocate the font directory */
    pxs->font_dir = gs_font_dir_alloc(pxs->memory);
    if (pxs->font_dir == 0)
        goto fail;

    pxs->pcs = NULL;

    return pxs;

fail:
    gs_free_object(memory, pxgs, "px_gstate_alloc");
    gs_free_object(memory, pxs, "px_state_alloc");
    return 0;
}

/* Release a px_state_t */
void
px_state_release(px_state_t * pxs)
{
    /* free the font dictionary */
    px_dict_release(&pxs->font_dict);
    gs_free_object(pxs->memory, pxs->font_dir, "font_dir_alloc(dir)");
    /* Don't free pxgs since it'll get freed as pgs' client */
    gs_free_object(pxs->memory, pxs, "px_state_release");
}

/* Do one-time state initialization. */
/* There isn't much of this: most state is initialized per-session. */
void
px_state_init(px_state_t * pxs, gs_gstate * pgs)
{
    pxs->pgs = pgs;
    px_gstate_init(pxs->pxgs, pgs);
    pxs->error_report = eErrorPage;     /* default before first session */
    pxs->end_page = px_default_end_page;
    pxs->data_source_open = false;
    px_dict_init(&pxs->stream_dict, pxs->memory, NULL);
    px_dict_init(&pxs->builtin_font_dict, pxs->memory, px_free_font);
    px_dict_init(&pxs->font_dict, pxs->memory, px_free_font);
    px_dict_init(&pxs->page_pattern_dict, pxs->memory, px_free_pattern);
    px_dict_init(&pxs->session_pattern_dict, pxs->memory, px_free_pattern);
    pxs->have_page = false;
    pxs->warning_length = 0;
}

/* Do one-time finalization at the end of execution. */
void
px_state_finit(px_state_t * pxs)
{
    /* If streams persisted across sessions, we would release them here. */
#if 0
    px_dict_release(&pxs->stream_dict);
#endif
}
