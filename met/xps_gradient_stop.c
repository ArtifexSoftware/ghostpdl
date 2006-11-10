/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001, 2005 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id: */

/* gradient stops */

/* meh. */
#include "memory_.h"
#include "gsmemory.h"
#include "gsmatrix.h"
#include "gp.h"
#include "metcomplex.h"
#include "metelement.h"
#include "metutil.h"
#include "metstate.h"
#include "metgstate.h"
#include "math_.h"
#include "ctype_.h"
#include "gsimage.h"
#include "metimage.h"
#include "gscspace.h"
#include "gscolor2.h"
#include "gsptype1.h"
#include <stdlib.h> /* nb for atof */
#include "zipparse.h"
#include "xps_gradient_draw.h"
#include "gxstate.h"
#include "gsutil.h"

private int
GradientStop_cook(void **ppdata, met_state_t *ms,
	        const char *el, const char **attr)
{
    CT_GradientStop *stop;
    int i;

    stop = (CT_GradientStop *) gs_alloc_bytes(ms->memory, sizeof(CT_GradientStop),
			            "GradientStop_cook");
    if (!stop)
	return gs_throw(-1, "out of memory: CT_GradientStop");

    stop->Color = (char*)"#000000";
    stop->Offset = 0.0;
    stop->next = NULL;

    for (i = 0; attr[i]; i += 2)
    {
	if (!strcmp(attr[i], "Color"))
	    stop->Color = met_strdup(ms->memory, attr[i+1]);
	if (!strcmp(attr[i], "Offset"))
	    stop->Offset = atof(attr[i+1]);
    }

    dprintf2("GradientStop %s %g\n", stop->Color, stop->Offset);

    *ppdata = stop;

    met_appendgradientstop(ms, stop);

    return 0;
}

private int
GradientStop_done(void *data, met_state_t *ms)
{
    return 0;
}

private int
GradientStop_action(void *data, met_state_t *ms, int type)
{
    return 0;
}

const met_element_t met_element_procs_GradientStop = {
    "GradientStop",
    {
	GradientStop_cook,
	GradientStop_action,
	GradientStop_done
    }
};

