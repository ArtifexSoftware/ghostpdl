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


#include "math_.h"
#include "gx.h"
#include "gxchrout.h"
#include "gxfarith.h"
#include "gxgstate.h"

/*
 * Determine the flatness for rendering a character in an outline font.
 * This may be less than the flatness in the gs_gstate.
 * The second argument is the default scaling for the font: 0.001 for
 * Type 1 fonts, 1.0 for TrueType fonts.
 */
double
gs_char_flatness(const gs_gstate *pgs, double default_scale)
{
    /*
     * Set the flatness to a value that is likely to produce reasonably
     * good-looking curves, regardless of its current value in the
     * graphics state.  If the character is very small, set the flatness
     * to zero, which will produce very accurate curves.
     */
    double cxx = fabs(pgs->ctm.xx), cyy = fabs(pgs->ctm.yy);

    if (is_fzero(cxx) || (cyy < cxx && !is_fzero(cyy)))
        cxx = cyy;
    if (!is_xxyy(&pgs->ctm)) {
        double cxy = fabs(pgs->ctm.xy), cyx = fabs(pgs->ctm.yx);

        if (is_fzero(cxx) || (cxy < cxx && !is_fzero(cxy)))
            cxx = cxy;
        if (is_fzero(cxx) || (cyx < cxx && !is_fzero(cyx)))
            cxx = cyx;
    }
    /* Correct for the default scaling. */
    cxx *= 0.001 / default_scale;
    /* Don't let the flatness be worse than the default. */
    if (cxx > pgs->flatness)
        cxx = pgs->flatness;
    /* If the character is tiny, force accurate curves. */
    if (cxx < 0.2)
        cxx = 0;
    return cxx;
}
