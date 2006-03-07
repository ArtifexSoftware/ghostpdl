/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001, 2005 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id$*/

#include "metstate.h"
#include "metgstate.h"
#include "plfont.h"

/* allocate a metro state */
met_state_t *
met_state_alloc(gs_memory_t *mem)
{
    met_state_t *pmets = 
        (met_state_t *)gs_alloc_bytes(mem, 
                                      sizeof(met_state_t),
                                      "met_state_alloc");
    if (pmets == 0)
        return 0;
    pmets->memory = mem;
    /* nb belongs there should be a metro state initialization
       function somewhere */
    pl_dict_init(&pmets->font_dict, mem, pl_free_font);
    pmets->font_dir = gs_font_dir_alloc(mem);
    pmets->current_resource = 0;
    return pmets;
}

/* release a metro state */
void
met_state_release(met_state_t *pmets)
{
    gs_free_object(pmets->memory, pmets, "met_state_release");
}

void 
met_state_init(met_state_t *pmet, gs_state *pgs)
{
    pmet->pgs = pgs;
    met_gstate_init(pgs, pmet->memory);
}
