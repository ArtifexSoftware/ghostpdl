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

/* Global definitions for the 'Named Color' callback handling. */

#ifndef gsnamecl_INCLUDED
# define gsnamecl_INCLUDED

#include "gxfrac.h"
#include "gscsel.h"
#include "gxcspace.h"
#include "gsdevice.h"
#include "gsdcolor.h"

/*
 * Enable custom client callback color processing.  Note:  There is a sample
 * implementation in src/gsncdummy.c.
 */
#ifndef ENABLE_CUSTOM_COLOR_CALLBACK
#define ENABLE_CUSTOM_COLOR_CALLBACK 0	/* 0 --> disabled, 1 --> enabled */
#endif

#if ENABLE_CUSTOM_COLOR_CALLBACK
/* Ptr to custom color callback struct */
#define CUSTOM_COLOR_PTR void * custom_color_callback;
#define INIT_CUSTOM_COLOR_PTR   NULL,		/* Initial value */
#else
#define CUSTOM_COLOR_PTR
#define INIT_CUSTOM_COLOR_PTR
#endif

#define CustomColorCallbackParamName "CustomColorCallback"

/*
 * For comments upon the client API for working with the custom client
 * callback logic see the section labeled:  "CLIENT COLOR CALLBACK
 * APPLICATION INTERFACE" below.
 *
 * Also see the comments at the start of src/gsnamecl.c
 */

typedef struct gs_context_state_s i_ctx_t;

#define cs_proc_adjust_client_cspace_count(proc)\
  void proc(const gs_color_space *, int)

/*
 * Put the 'custom color' client callback parameter block pointer.  This value
 * is passed as a string type user paramter.  A string is being used since
 * PostScript does not support pointers as a type.  Note:  An integer type
 * is not being used since PS integers are nominally 32 bits.  Thus there
 * would be a problem using integers to pass pointer values on 64 bit systems.
 */
int custom_color_callback_put_params(gs_gstate * pgs, gs_param_list * plist);

/*
 * Get the custom client client callback parameter block pointer.  This value
 * is passed as a string type user paramter.  A string is being used since
 * PostScript does not support pointers as a type.  Note:  An integer type
 * is not being used since PS intergers are nominally 32 bits.  Thus there
 * would be a problem using integers to pass pointer values on 64 bit systems.
 */
int custom_color_callback_get_params(gs_gstate * pgs, gs_param_list * plist);

/*
 * Check if we want to use the callback color processing logic for the given
 * Separation color space.
 */
bool custom_color_callback_install_Separation(gs_color_space * pcs,
                                                        gs_gstate * pgs);

/*
 * Check if we want to use the custom client callback processing logic for the
 * given DeviceN color space.
 */
bool custom_color_callback_install_DeviceN(gs_color_space * pcs, gs_gstate * pgs);

/*
 * Convert a Separation color into device colorants using the custom client
 * client callback.
 */
int gx_remap_concrete_custom_color_Separation(const frac * pconc,
        const gs_color_space * pcs, gx_device_color * pdc,
        const gs_gstate * pgs, gx_device * dev, gs_color_select_t select);

/*
 * Convert a DeviceN color into device colorants using the custom client
 * client callback.
 */
int gx_remap_concrete_custom_color_DeviceN(const frac * pconc,
        const gs_color_space * pcs, gx_device_color * pdc,
        const gs_gstate * pgs, gx_device * dev, gs_color_select_t select);

/* "CLIENT COLOR CALLBACK APPLICATION INTERFACE" */
/*
 * In order to give some flexibility to the Ghostscript client API, we are
 * allowing the client to define a set of call back procedures for processing
 * color spaces.
 *
 * See the comments at the start of src/gsnamecl.c
 */
/*
 * The 'client color' call back structure definitions.  The call back structure
 * consists of a pointer to a list of client color space handling procedures
 * and a pointer to a client data structure.
 */

typedef struct client_custom_color_params_s {
    /* Client callback handlers */
    struct client_custom_color_procs_s * client_procs;
    /* For global client data */
    void * data;
} client_custom_color_params_t;

/*
 * Define a base type for client color space data.  Most clients will
 * overload this type with a structure of their own.  That type must
 * start with a pointer to a handler routine for the structure's
 * reference count.
 */
typedef struct client_color_space_data_s {
        cs_proc_adjust_client_cspace_count((*client_adjust_cspace_count));
} client_color_space__data_t;

/*
 * Define the client custom client callback procedures.
 */
typedef struct client_custom_color_procs_s {
    /*
     * Install a DeviceGray color space.
     */
    bool (* install_DeviceGray)(client_custom_color_params_t * pparams,
            gs_color_space * pcs, gs_gstate * pgs);
    /*
     * Convert a DeviceGray color into device color.
     */
    int (* remap_DeviceGray)(client_custom_color_params_t * pparams,
            const frac * pconc, const gs_color_space * pcs,
            gx_device_color * pdc, const gs_gstate * pgs,
            gx_device * dev, gs_color_select_t select);
    /*
     * Install a DeviceRGB color space.
     */
    bool (* install_DeviceRGB)(client_custom_color_params_t * pparams,
            gs_color_space * pcs, gs_gstate * pgs);
    /*
     * Convert a DeviceRGB color into device color.
     */
    int (* remap_DeviceRGB)(client_custom_color_params_t * pparams,
            const frac * pconc, const gs_color_space * pcs,
            gx_device_color * pdc, const gs_gstate * pgs,
            gx_device * dev, gs_color_select_t select);
    /*
     * Install a DeviceCMYK color space.
     */
    bool (* install_DeviceCMYK)(client_custom_color_params_t * pparams,
            gs_color_space * pcs, gs_gstate * pgs);
    /*
     * Convert a DeviceGray color into device color.
     */
    int (* remap_DeviceCMYK)(client_custom_color_params_t * pparams,
            const frac * pconc, const gs_color_space * pcs,
            gx_device_color * pdc, const gs_gstate * pgs,
            gx_device * dev, gs_color_select_t select);
    /*
     * Check if we want to use the callback color processing logic for the
     * given Separation color space.
     */
    bool (* install_Separation)(client_custom_color_params_t * pparams,
            gs_color_space * pcs, gs_gstate * pgs);
    /*
     * Convert a Separation color into device color.
     */
    int (* remap_Separation)(client_custom_color_params_t * pparams,
            const frac * pconc, const gs_color_space * pcs,
            gx_device_color * pdc, const gs_gstate * pgs,
            gx_device * dev, gs_color_select_t select);
    /*
     * Check if we want to use the callback color processing logic for the
     * given DeviceN color space.
     */
    bool (* install_DeviceN)(client_custom_color_params_t * pparams,
            gs_color_space * pcs, gs_gstate * pgs);
    /*
     * Convert a DeviceN color into device color.
     */
    int (* remap_DeviceN)(client_custom_color_params_t * pparams,
            const frac * pconc, const gs_color_space * pcs,
            gx_device_color * pdc, const gs_gstate * pgs,
            gx_device * dev, gs_color_select_t select);
    /*
     * Check if we want to use the callback color processing logic for the
     * given CIEBasedA color space.
     */
    bool (* install_CIEBasedA)(client_custom_color_params_t * pparams,
            gs_color_space * pcs, gs_gstate * pgs);
    /*
     * Please note that the 'complex' color spaces (CIEBasedA, CIEBasedABC,
     * CIEBasedDEF, CIEBasedDEFG, and ICCBased) have a different prototype,
     * versus the simpler color spces, for the callback for converting a
     * the color values.
     */
    /*
     * Convert a CIEBasedA color into device color.
     */
    int (* remap_CIEBasedA)(client_custom_color_params_t * pparams,
            const gs_client_color * pc, const gs_color_space * pcs,
            gx_device_color * pdc, const gs_gstate * pgs,
            gx_device * dev, gs_color_select_t select);
    /*
     * Check if we want to use the callback color processing logic for the
     * given CIEBasedABC color space.
     */
    bool (* install_CIEBasedABC)(client_custom_color_params_t * pparams,
            gs_color_space * pcs, gs_gstate * pgs);
    /*
     * Convert a CIEBasedABC color into device color.
     */
    int (* remap_CIEBasedABC)(client_custom_color_params_t * pparams,
            const gs_client_color * pc, const gs_color_space * pcs,
            gx_device_color * pdc, const gs_gstate * pgs,
            gx_device * dev, gs_color_select_t select);
    /*
     * Check if we want to use the callback color processing logic for the
     * given CIEBasedDEF color space.
     */
    bool (* install_CIEBasedDEF)(client_custom_color_params_t * pparams,
            gs_color_space * pcs, gs_gstate * pgs);
    /*
     * Convert a CIEBasedDEF color into device color.
     */
    int (* remap_CIEBasedDEF)(client_custom_color_params_t * pparams,
            const gs_client_color * pc, const gs_color_space * pcs,
            gx_device_color * pdc, const gs_gstate * pgs,
            gx_device * dev, gs_color_select_t select);
    /*
     * Check if we want to use the callback color processing logic for the
     * given CIEBasedDEFG color space.
     */
    bool (* install_CIEBasedDEFG)(client_custom_color_params_t * pparams,
            gs_color_space * pcs, gs_gstate * pgs);
    /*
     * Convert a CIEBasedDEFG color into device color.
     */
    int (* remap_CIEBasedDEFG)(client_custom_color_params_t * pparams,
            const gs_client_color * pc, const gs_color_space * pcs,
            gx_device_color * pdc, const gs_gstate * pgs,
            gx_device * dev, gs_color_select_t select);
    /*
     * Check if we want to use the callback color processing logic for the
     * given ICCBased color space.
     */
    bool (* install_ICCBased)(client_custom_color_params_t * pparams,
            gs_color_space * pcs, gs_gstate * pgs);
    /*
     * Convert a ICCBased color into device color.
     */
    int (* remap_ICCBased)(client_custom_color_params_t * pparams,
            const gs_client_color * pc, const gs_color_space * pcs,
            gx_device_color * pdc, const gs_gstate * pgs,
            gx_device * dev, gs_color_select_t select);

} client_custom_color_procs_t;

#endif		/* ifndef gsnamecl_INCLUDED */
