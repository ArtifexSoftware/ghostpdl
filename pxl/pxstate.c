/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* pxstate.c */
/* State allocation/initialization/cleanup */

#include "stdio_.h"			/* std.h + NULL */
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "scommon.h"
#include "pxparse.h"
#include "pxstate.h"
#include "pxfont.h"
#include "gxfcache.h"

/* Import the initialization procedure table from pxtop.c. */
typedef int (*px_init_proc)(px_state_t *);
extern const px_init_proc px_init_table[];

/* Allocate a px_state_t. */
px_state_t *
px_state_alloc(gs_memory_t *memory)
{	
    px_state_t *pxs = (px_state_t *)gs_alloc_bytes(memory,
                                                   sizeof(px_state_t),
                                                   "px_state_alloc");
    px_gstate_t *pxgs = px_gstate_alloc(memory);

    if ( pxs == 0 || pxgs == 0 ) { 
        gs_free_object(memory, pxgs, "px_gstate_alloc");
        gs_free_object(memory, pxs, "px_state_alloc");
        return 0;
    }
    pxs->memory = memory;
    pxs->pxgs = pxgs;
    pxgs->pxs = pxs;
    px_state_init(pxs, NULL);
    /* Run module initialization code. */
    { 
        const px_init_proc *init;
        for ( init = px_init_table; *init; ++init )
	    (*init)(pxs);
    }
    return pxs;
}

/* Release a px_state_t */
void
px_state_release(px_state_t *pxs)
{
    /* free the font dictionary */
    px_dict_release(&pxs->font_dict);
    /* Don't free pxgs since it'll get freed as pgs' client */
    gs_free_object(pxs->memory, pxs, "px_state_release");
}

/* Do one-time state initialization. */
/* There isn't much of this: most state is initialized per-session. */
void
px_state_init(px_state_t *pxs, gs_state *pgs)
{	pxs->pgs = pgs;
	px_gstate_init(pxs->pxgs, pgs);
	pxs->error_report = eErrorPage;	/* default before first session */
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
px_state_finit(px_state_t *pxs)
{	/* If streams persisted across sessions, we would release them here. */
#if 0
	px_dict_release(&pxs->stream_dict);
#endif
}
