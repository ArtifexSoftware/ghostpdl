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
#include "math_.h"
#include "gx.h"
#include "gxchrout.h"
#include "gxfarith.h"
#include "gxistate.h"

/*
 * Determine the flatness for rendering a character in an outline font.
 * This may be less than the flatness in the imager state.
 * The second argument is the default scaling for the font: 0.001 for
 * Type 1 fonts, 1.0 for TrueType fonts.
 */
double
gs_char_flatness(const gs_imager_state *pis, floatp default_scale)
{
    /*
     * Set the flatness to a value that is likely to produce reasonably
     * good-looking curves, regardless of its current value in the
     * graphics state.  If the character is very small, set the flatness
     * to zero, which will produce very accurate curves.
     */
    double cxx = fabs(pis->ctm.xx), cyy = fabs(pis->ctm.yy);

    if (is_fzero(cxx) || (cyy < cxx && !is_fzero(cyy)))
	cxx = cyy;
    if (!is_xxyy(&pis->ctm)) {
	double cxy = fabs(pis->ctm.xy), cyx = fabs(pis->ctm.yx);

	if (is_fzero(cxx) || (cxy < cxx && !is_fzero(cxy)))
	    cxx = cxy;
	if (is_fzero(cxx) || (cyx < cxx && !is_fzero(cyx)))
	    cxx = cyx;
    }
    /* Correct for the default scaling. */
    cxx *= 0.001 / default_scale;
    /* Don't let the flatness be worse than the default. */
    if (cxx > pis->flatness)
	cxx = pis->flatness;
    /* If the character is tiny, force accurate curves. */
    if (cxx < 0.2)
	cxx = 0;
    return cxx;
}
