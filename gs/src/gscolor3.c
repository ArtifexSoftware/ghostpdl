/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */


/* "Operators" for LanguageLevel 3 color facilities */
#include "gx.h"
#include "gserrors.h"
#include "gscspace.h"		/* for gscolor2.h */
#include "gsmatrix.h"		/* for gscolor2.h */
#include "gscolor2.h"
#include "gscolor3.h"
#include "gspath.h"
#include "gzstate.h"
#include "gxshade.h"

/* setsmoothness */
int
gs_setsmoothness(gs_state * pgs, floatp smoothness)
{
    pgs->smoothness =
	(smoothness < 0 ? 0 : smoothness > 1 ? 1 : smoothness);
    return 0;
}

/* currentsmoothness */
float
gs_currentsmoothness(const gs_state * pgs)
{
    return pgs->smoothness;
}

/* shfill */
int
gs_shfill(gs_state * pgs, const gs_shading_t * psh)
{
    int code = gs_gsave(pgs);

    if (code < 0)
	return code;
    if ((code = gs_setcolorspace(pgs, psh->params.ColorSpace)) < 0 ||
	(code = gs_clippath(pgs)) < 0 ||
	(code = gs_shading_fill_path(psh, pgs->path,
				     gs_currentdevice(pgs),
				     (gs_imager_state *)pgs)) < 0
	)
	DO_NOTHING;
    gs_grestore(pgs);
    return code;
}
