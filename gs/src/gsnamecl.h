/*
  Copyright (C) 2005 artofcode LLC, Artofex Software Inc.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/
/* $Id$ */
/* Global definitions for the 'Named Color' callback handling. */

#ifndef gsnamecl_INCLUDED
# define gsnamecl_INCLUDED

#include "gxfrac.h"
#include "gsccolor.h"
#include "gscsel.h"

/*
 * Enable 'named color' callback color processing.  Note:  There is a sample
 * implementation in src/gsncdemo.c.
 */
#define ENABLE_NAMED_COLOR_CALLBACK 0	/* 0 --> disabled, 1 --> enabled */

/*
 * For comments upon the client API for working with the 'named color'
 * callback logic see the section labeled:  "NAMED COLOR CALLBACK CLIENT
 * APPLICATION INTERFACE" below.
 */

/*
 * Type for holding 'named color' callback related parameters
 */
typedef struct named_color_params_s {
    /*
     * If true, then use the 'named color' callback color processing path
     * for determining colorants.
     */
    bool use_named_color_callback;
    /*
     * Storage for the 'named color' callback related parameters.
     * Note:  This storage is actually for the client.  This may be enough
     * for some clients.  It is enough for our demo example.  A more complete
     * implementation would probably include a client callback for for when
     * a color space is finished (i.e. the reference count is zero or it
     * is garbage collected).
     */
    int color_index[GS_CLIENT_COLOR_MAX_COMPONENTS];
} named_color_params_t;

#ifndef gs_color_space
typedef struct gs_color_space_s gs_color_space;
#endif

#ifndef gx_device_color
typedef struct gx_device_color_s gx_device_color;
#endif

#ifndef gs_state
typedef struct gs_state_s gs_state;
#endif

#ifndef gs_imager_state
typedef struct gs_imager_state_s gs_imager_state;
#endif

#ifndef gx_device
typedef struct gx_device_s gx_device;
#endif

#ifndef gs_param_list
typedef struct gs_param_list_s gs_param_list;
#endif

/*
 * Put the 'named color' client callback parameter block pointer.  This value
 * is passed as a string type device paramter.  A string is being used since
 * PostScript does not support pointers as a type.  Note:  An integer type
 * is not being used since PS intergers are nominally 32 bits.  Thus there
 * would be a problem using integers to pass pointer values on 64 bit systems.
 */
int named_color_put_params(gx_device * dev, gs_param_list * plist);

/*
 * Get the 'named color' client callback parameter block pointer.  This value
 * is passed as a string type device paramter.  A string is being used since
 * PostScript does not support pointers as a type.  Note:  An integer type
 * is not being used since PS intergers are nominally 32 bits.  Thus there
 * would be a problem using integers to pass pointer values on 64 bit systems.
 */
int named_color_get_params(gx_device * dev, gs_param_list * plist);

/*
 * Check if we want to use the 'named color' callback processing logic for the
 * given Separation color space.
 */
bool named_color_install_Separation(gs_color_space * pcs, gs_state * pgs);

/*
 * Check if we want to use the 'named color' callback processing logic for the
 * given DeviceN color space.
 */
bool named_color_install_DeviceN(gs_color_space * pcs, gs_state * pgs);

/*
 * Convert a Separation color into device colorants using the 'named color'
 * client callback.
 */
int gx_remap_concrete_named_color_Separation(const frac * pconc,
	const gs_color_space * pcs, gx_device_color * pdc,
       	const gs_imager_state * pis, gx_device * dev, gs_color_select_t select);

/*
 * Convert a DeviceN color into device colorants using the 'named color'
 * client callback.
 */
int gx_remap_concrete_named_color_DeviceN(const frac * pconc,
	const gs_color_space * pcs, gx_device_color * pdc,
       	const gs_imager_state * pis, gx_device * dev, gs_color_select_t select);

/* "CLIENT NAMED COLOR APPLICATION INTERFACE" */
/*
 * In order to give some flexibility to the Ghostscript client API, we are
 * allowing the client to define a set of call back procedures for processing
 * color spaces with 'named colors', i.e. Separation and DeviceN.
 */
/*
 * The 'named color' call back structure definitions.  The call back structure
 * consists of a pointer to a list of client 'named color' color handling
 * procedures and a pointer to a client data structure.
 */
typedef struct client_named_color_procs_s client_named_color_procs_t;
typedef struct client_named_color_params_s {
    client_named_color_procs_t * client_procs;	/* Client callback handlers */
    void * data;				/* For client data */
} client_named_color_params_t;

/*
 * Define the client 'named color' callback procedures.
 */
typedef struct client_named_color_procs_s {
    /*
     * Check if we want to use the callback color processing logic for the
     * given Separation color space.
     */
    bool (* install_Separation)(client_named_color_params_t * pparams,
	    gs_color_space * pcs, gs_state * pgs);
    /*
     * Convert a Separation color into device color.
     */
    int (* remap_Separation)(client_named_color_params_t * pparams,
	    const frac * pconc, const gs_color_space * pcs,
	    gx_device_color * pdc, const gs_imager_state * pis,
	    gx_device * dev, gs_color_select_t select);
    /*
     * Check if we want to use the callback color processing logic for the
     * given DeviceN color space.
     */
    bool (* install_DeviceN)(client_named_color_params_t * pparams,
	    gs_color_space * pcs, gs_state * pgs);
    /*
     * Convert a DeviceN color into device color.
     */
    int (* remap_DeviceN)(client_named_color_params_t * pparams, const frac * pconc,
	    const gs_color_space * pcs, gx_device_color * pdc,
       	    const gs_imager_state * pis, gx_device * dev,
	    gs_color_select_t select);

} client_named_color_procs_t;
    
#endif		/* ifndef gsnamecl_INCLUDED */
