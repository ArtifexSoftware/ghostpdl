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

/* metro state */

#ifndef metstate_INCLUDED
#  define metstate_INCLUDED

#include "stdpre.h"
#include "std.h"
#include "gstypes.h"
#include "gsstate.h"
#include "gsmemory.h"
#include "gsfont.h"
#include "pltop.h" /* for pjls pointer */
#include "pldict.h"

typedef struct met_state_s met_state_t;

struct met_state_s {
    gs_memory_t *memory;
    void *pzip;
    gs_state *pgs;
    void *client_data;
    pl_interp_instance_t *pjls; /* probably not used */
    int (*end_page)(met_state_t *pxs, int num_copies, int flush);
    int entries_recorded;
    pl_dict_t font_dict;
    pl_dict_t pattern_dict;
    gs_font_dir *font_dir;
};

/* allocate a metro state */
met_state_t *met_state_alloc(gs_memory_t *mem);

/* release a metro state */
void met_state_release(met_state_t *met);

/* inialize the metro state */
void met_state_init(met_state_t *met, gs_state *gs_state);

#endif				/* metstate_INCLUDED */
