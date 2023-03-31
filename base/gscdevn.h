/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/* Client interface to DeviceN color */

#ifndef gscdevn_INCLUDED
#  define gscdevn_INCLUDED

#include "gscspace.h"
#include "gsfunc.h"

/*
 * Construct a new a DeviceN color space.
 *
 * Note that the client is responsible for memory management of the
 * tint transform Function.
 */
int
gs_cspace_new_DeviceN(
    gs_color_space **ppcs,
    uint num_components,
    gs_color_space *palt_cspace,
    gs_memory_t *pmem
    );

/* Set the tint transformation procedure for a DeviceN color space. */
/* VMS limits procedure names to 31 characters, and some systems only */
/* compare the first 23 characters. */
extern int gs_cspace_set_devn_proc(
                                      gs_color_space * pcspace,
                        int (*proc)(const float *,
                                       float *,
                                       const gs_gstate *,
                                       void *
                                      ),
                                      void *proc_data
                                      );

/* Set the DeviceN tint transformation procedure to a Function. */
int gs_cspace_set_devn_function(gs_color_space *pcspace,
                                   gs_function_t *pfn);

/*
 * If the DeviceN tint transformation procedure is a Function,
 * return the function object, otherwise return 0.
 */
gs_function_t *gs_cspace_get_devn_function(const gs_color_space *pcspace);

/* Map a DeviceN color using a Function. */
int map_devn_using_function(const float *in, float *out,
                        const gs_gstate *pgs, void *data);

/* Serialize a DeviceN map. */
int gx_serialize_device_n_map(const gs_color_space * pcs, gs_device_n_map * m, stream * s);

/*
 * DeviceN and NChannel color spaces can have an attributes dict.  In the
 * attribute dict can be a Colorants dict which contains Separation color
 * spaces.  If the Colorant dict is present, the PS logic will build each of
 * the Separation color spaces in a temp gstate and then call this procedure
 * to attach the Separation color space to the DeviceN color space.
 * The parameter to this procedure is a colorant name.  The Separation
 * color space is in the current (temp) gstate.  The DeviceN color space is
 * in the next gstate down in the gstate list (pgs->saved).
 */
int gs_attachcolorant(char *sep_name, gs_gstate * pgs);

/*
 * This is the same routine as above, but more general. Instead of assuming that
 * the current coloru space is hte colorant space, and the saved graphics state
 * contains the DeviceN space, we pass both those in, along with the memory
 * allocater we want the routine to use.
 * This is for the pdfi PDF interpreter (and potentially any other non-PostScript
 * interpreter)
 */
int gs_attach_colorant_to_space(char *sep_name, gs_color_space *pcs, gs_color_space *colorant_space, gs_memory_t *mem);

#endif /* gscdevn_INCLUDED */
