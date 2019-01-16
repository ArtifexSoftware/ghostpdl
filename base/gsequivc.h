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


/* Header for routines for determining equivalent color for spot colors */

/* For more information, see comment at the start of src/gsequivc.c */

#ifndef gsequivc_INCLUDED
# define gsequivc_INCLUDED

#include "stdpre.h"
#include "gxfrac.h"
#include "gxcindex.h"
#include "gxdevcli.h"

/*
 * Define the maximum number of spot colors supported by this device.
 * This value is arbitrary.  It is set simply to define a limit on
 * on the separation_name_array and separation_order map.
 */
#define GX_DEVICE_MAX_SEPARATIONS GX_DEVICE_COLOR_MAX_COMPONENTS

/*
 * Structure for holding a CMYK color.
 */
typedef struct cmyk_color_s {
    bool color_info_valid;
    frac c;
    frac m;
    frac y;
    frac k;
} cmyk_color;

/* if you make any additions/changes to the equivalent_cmyk_color_params or the
   cmyk_color structrs you need to make the appropriate additions/changes 
   to the compare_equivalent_cmyk_color_params() function in gdevdevn.c */

/*
 * Structure for holding parameters for collecting the equivalent CMYK
 * for a spot colorant.
 */
typedef struct equivalent_cmyk_color_params_s {
    bool all_color_info_valid;
    cmyk_color color[GX_DEVICE_MAX_SEPARATIONS];
} equivalent_cmyk_color_params;

/*
 * If possible, update the equivalent CMYK color for spot colors.
 */
int update_spot_equivalent_cmyk_colors(gx_device * pdev,
                const gs_gstate * pgs, gs_devn_params * pdevn_params,
                equivalent_cmyk_color_params * pparams);

/*
 * Utiliy routine:  Capture equivalent color when given a modified
 * color space.
 */
void capture_spot_equivalent_cmyk_colors(gx_device * pdev,
                const gs_gstate * pgs, const gs_client_color * pcc,
                const gs_color_space * pcs, int sep_num,
                equivalent_cmyk_color_params * pparams);

/* Used in named color replacement to detect that we are doing the equivalent
   computation */
bool named_color_equivalent_cmyk_colors(const gs_gstate * pgs);

#endif		/* define gsequivc_INCLUDED */
