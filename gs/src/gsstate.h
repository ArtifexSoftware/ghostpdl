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

/*$RCSfile$ $Revision$ */
/* Public graphics state API */

#ifndef gsstate_INCLUDED
#  define gsstate_INCLUDED

/* Opaque type for a graphics state */
#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s gs_state;
#endif

/* opague type for overprint compositor parameters */
#ifndef gs_overprint_params_t_DEFINED
#  define gs_overprint_params_t_DEFINED
typedef struct gs_overprint_params_s    gs_overprint_params_t;
#endif

/* Initial allocation and freeing */
gs_state *gs_state_alloc(gs_memory_t *);	/* 0 if fails */
int gs_state_free(gs_state *);

/* Initialization, saving, restoring, and copying */
int gs_gsave(gs_state *), gs_grestore(gs_state *), gs_grestoreall(gs_state *);
int gs_grestore_only(gs_state *);
int gs_gsave_for_save(gs_state *, gs_state **), gs_grestoreall_for_restore(gs_state *, gs_state *);
gs_state *gs_gstate(gs_state *);
gs_state *gs_state_copy(gs_state *, gs_memory_t *);
int gs_copygstate(gs_state * /*to */ , const gs_state * /*from */ ),
      gs_currentgstate(gs_state * /*to */ , const gs_state * /*from */ ),
      gs_setgstate(gs_state * /*to */ , const gs_state * /*from */ );

int gs_state_update_overprint(gs_state *, const gs_overprint_params_t *);
bool gs_currentoverprint(const gs_state *);
void gs_setoverprint(gs_state *, bool);
int gs_currentoverprintmode(const gs_state *);
int gs_setoverprintmode(gs_state *, int);

int gs_do_set_overprint(gs_state *);

int gs_initgraphics(gs_state *);

/* Device control */
#include "gsdevice.h"

/* Line parameters and quality */
#include "gsline.h"

/* Color and gray */
#include "gscolor.h"

/* Halftone screen */
#include "gsht.h"
#include "gscsel.h"
int gs_setscreenphase(gs_state *, int, int, gs_color_select_t);
int gs_currentscreenphase(const gs_state *, gs_int_point *, gs_color_select_t);

#define gs_sethalftonephase(pgs, px, py)\
  gs_setscreenphase(pgs, px, py, gs_color_select_all)
#define gs_currenthalftonephase(pgs, ppt)\
  gs_currentscreenphase(pgs, ppt, 0)
int gx_imager_setscreenphase(gs_imager_state *, int, int, gs_color_select_t);

/* Miscellaneous */
int gs_setfilladjust(gs_state *, floatp, floatp);
int gs_currentfilladjust(const gs_state *, gs_point *);
void gs_setlimitclamp(gs_state *, bool);
bool gs_currentlimitclamp(const gs_state *);
#include "gscpm.h"
gs_in_cache_device_t gs_incachedevice(const gs_state *);

#endif /* gsstate_INCLUDED */
