/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Graphics state alpha value access */
#include "gx.h"
#include "gsalpha.h"
#include "gxdcolor.h"
#include "gzstate.h"

/* setalpha */
int
gs_setalpha(gs_state * pgs, floatp alpha)
{
    pgs->alpha =
	(gx_color_value) (alpha < 0 ? 0 : alpha > 1 ? gx_max_color_value :
			  alpha * gx_max_color_value);
    gx_unset_dev_color(pgs);
    return 0;
}

/* currentalpha */
float
gs_currentalpha(const gs_state * pgs)
{
    return (float)pgs->alpha / gx_max_color_value;
}
