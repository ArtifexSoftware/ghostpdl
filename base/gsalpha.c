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


/* Graphics state alpha value access */
#include "gx.h"
#include "gsalpha.h"
#include "gxdcolor.h"
#include "gzstate.h"

/* setalpha */
int
gs_setalpha(gs_gstate * pgs, double alpha)
{
    pgs->alpha =
        (gx_color_value) (alpha < 0 ? 0 : alpha > 1 ? gx_max_color_value :
                          alpha * gx_max_color_value);
    gx_unset_dev_color(pgs);
    return 0;
}

/* currentalpha */
float
gs_currentalpha(const gs_gstate * pgs)
{
    return (float)pgs->alpha / gx_max_color_value;
}
