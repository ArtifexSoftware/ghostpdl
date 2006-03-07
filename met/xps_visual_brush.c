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

/* visual brush */

#include "metcomplex.h"
#include "metelement.h"
#include "metimage.h"
#include "metgstate.h"
#include "metrecorder.h"
#include "metutil.h"
#include "gserror.h"
#include "gsmatrix.h"
#include "gscspace.h"
#include "gsptype1.h"
#include "gsutil.h"
#include "gscolor2.h"
#include "ctype_.h"
#include "string_.h"
#include "std.h"
#include <stdlib.h> /* nb for atof */

int
VisualBrush_paint(void *data, gs_state *pgs)
{
    int code;
    met_state_t *ms = data;
    gs_state *saved_pgs = ms->pgs;
    ms->pgs = pgs;
    code = met_playback(data);
    ms->pgs = saved_pgs;
    return code;
}

/* element constructor */
private int
VisualBrush_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_VisualBrush *aVisualBrush = 
        (CT_VisualBrush *)gs_alloc_bytes(ms->memory,
                                     sizeof(CT_VisualBrush),
                                       "VisualBrush_cook");
    int i;

    memset(aVisualBrush, 0, sizeof(CT_VisualBrush));

    /* parse attributes, filling in the zeroed out C struct */
#define MYSET(field, value)                                                   \
    met_cmp_and_set((field), attr[i], attr[i+1], (value))

    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {
        if (!MYSET(&aVisualBrush->Key, "Key"))
            ;
        else if (!MYSET(&aVisualBrush->Transform, "Transform"))
            ;
        else if (!MYSET(&aVisualBrush->Viewbox, "Viewbox"))
            ;
        else if (!MYSET(&aVisualBrush->Viewport, "Viewport"))
            ;
        else if (!MYSET(&aVisualBrush->Fill, "Fill"))
            ;
        else if (!MYSET(&aVisualBrush->TileMode, "TileMode"))
            ;
        else if (!MYSET(&aVisualBrush->ViewboxUnits, "ViewboxUnits"))
            ;
        else if (!MYSET(&aVisualBrush->ViewportUnits, "ViewportUnits"))
            ;
        else {
            gs_warn2("unsupported attribute %s=%s",
                     attr[i], attr[i+1]);
        }
    }

    /* copy back the data for the parser. */
    *ppdata = aVisualBrush;
    /* record me */
    return 1;
}

private bool is_Data_delimeter(char b) 
{
    return (b == ',') || (isspace(b));
}

/* action associated with this element */
private int
VisualBrush_action(void *data, met_state_t *ms)
{
    return 0;
}

extern int met_PaintPattern(const gs_client_color *pcc, gs_state *pgs);

private int
VisualBrush_done(void *data, met_state_t *ms)
{
        
    CT_VisualBrush *aVisualBrush = data;
    gs_memory_t *mem = ms->memory;
    gs_client_pattern gspat;
    gs_client_color gscolor;

    met_pattern_t *pat =
	(met_pattern_t *)gs_alloc_bytes(ms->memory,
					sizeof(met_pattern_t), "GradientBrush_action");
    memset(pat, 0, sizeof(*pat));

    gs_pattern1_init(&gspat);
    uid_set_UniqueID(&gspat.uid, gs_next_ids(mem, 1));
    gspat.PaintType = 1;
    gspat.TilingType = 1;

    /* nb gcc'isms ahead */
    if (aVisualBrush->Transform) {
        char transstring[strlen(aVisualBrush->Transform)];
        strcpy(transstring, aVisualBrush->Transform);
        /* nb wasteful */
        char *args[strlen(aVisualBrush->Transform)];
        char **pargs = args;
        met_split(transstring, args, is_Data_delimeter);
        /* nb checking */
        pat->Transform.xx = atof(pargs[0]);
        pat->Transform.xy = atof(pargs[1]);
        pat->Transform.yx = atof(pargs[2]);
        pat->Transform.yy = atof(pargs[3]);
        pat->Transform.tx = atof(pargs[4]);
        pat->Transform.ty = atof(pargs[5]);
    }

    if (aVisualBrush->Viewbox) {
        char viewbox[strlen(aVisualBrush->Viewbox)];
        strcpy(viewbox, aVisualBrush->Viewbox);
        /* nb wasteful */
        char *args[strlen(aVisualBrush->Viewbox)];
        char **pargs = args;
        met_split(viewbox, args, is_Data_delimeter);
        pat->Viewbox.p.x = atof(pargs[0]);
        pat->Viewbox.p.y = atof(pargs[1]);
        pat->Viewbox.q.x = pat->Viewbox.p.x + atof(pargs[2]);
        pat->Viewbox.q.y = pat->Viewbox.p.y + atof(pargs[3]);
        /* the spec says if the viewbox extant is zero in both
           directions take a point sample.  We are not exactly sure
           what this means.  Here is one interpretation. */
        if ((pat->Viewbox.p.x == pat->Viewbox.q.x) &&
            (pat->Viewbox.p.y == pat->Viewbox.q.y)) {
            pat->Viewbox.q.x = pat->Viewbox.p.x + 1.0;
            pat->Viewbox.q.y = pat->Viewbox.q.y + 1.0;
        }
        
    }
    if (aVisualBrush->Viewport) {
        /* do ever feel like your doing the same thing over and over
           again... */
        char viewport[strlen(aVisualBrush->Viewport)];
        strcpy(viewport, aVisualBrush->Viewport);
        /* nb wasteful */
        char *args[strlen(aVisualBrush->Viewbox)];
        char **pargs = args;
        met_split(viewport, args, is_Data_delimeter);
        pat->Viewport.p.x = atof(pargs[0]);
        pat->Viewport.p.y = atof(pargs[1]);
        pat->Viewport.q.x = pat->Viewport.p.x + atof(pargs[2]);
        pat->Viewport.q.y = pat->Viewport.p.y + atof(pargs[3]);
    }

    if (aVisualBrush->Transform) {
        char transform[strlen(aVisualBrush->Transform)];
        char *args[strlen(aVisualBrush->Transform)];
        strcpy(transform, aVisualBrush->Transform);
        char **pargs = args;
        met_split(transform, args, is_Data_delimeter);
        pat->Transform.xx = atof(pargs[0]);
        pat->Transform.xy = atof(pargs[1]);
        pat->Transform.yx = atof(pargs[2]);
        pat->Transform.yy = atof(pargs[3]);
        pat->Transform.tx = atof(pargs[4]);
        pat->Transform.ty = atof(pargs[5]);
    }
    
    /* NB: Viewbox(0,0,0,0) appears to be legal, adjust to sane value */
    pat->Viewbox.q.x = max( pat->Viewbox.q.x, 1 );
    pat->Viewbox.q.y = max( pat->Viewbox.q.y, 1 );

    gspat.BBox.p.x = 0;
    gspat.BBox.p.y = 0;
    gspat.BBox.q.x = pat->Viewbox.q.x - pat->Viewbox.p.x;
    gspat.BBox.q.y = pat->Viewbox.q.y - pat->Viewbox.p.y;

    gspat.XStep = pat->Viewbox.q.x - pat->Viewbox.p.x; 
    gspat.YStep = pat->Viewbox.q.y - pat->Viewbox.p.y;
    gspat.PaintProc = met_PaintPattern;
    gspat.client_data = (void *)pat;
    {
        gs_matrix mat;
        gs_color_space cs;
        gs_state *pgs = ms->pgs;
        gs_rect vbox = pat->Viewbox;
        gs_rect vport = pat->Viewport;
        gs_gsave(pgs);
        /* resolution scaling is done when the pattern is painted */
        /* translate the viewbox to the origin, scale the viewbox to
           the viewport and translate the viewbox back. */
        gs_make_translation(vport.p.x, vport.p.y, &mat);
        {
	    /* NB: don't scale smaller than a pixel */
            double scalex = (vport.q.x - vport.p.x) / (vbox.q.x - vbox.p.x);
            double scaley = (vport.q.y - vport.p.y) / (vbox.q.y - vbox.p.y);
            gs_matrix_scale(&mat, scalex, scaley, &mat);
        }
        gs_matrix_translate(mem, &mat, -vbox.p.x, vbox.p.y, &mat);

        /* nb defaults to RGB */
        gs_cspace_init_DeviceRGB(mem, &cs);
        gs_setcolorspace(pgs, &cs);
        gs_makepattern(&gscolor, &gspat, &mat, pgs, NULL);
        gs_grestore(pgs);
        gs_setpattern(pgs, &gscolor);
        {
            met_path_child_t parent = met_currentpathchild(pgs);
            if (parent == met_fill)
                met_setpatternfill(pgs);
            else if (parent == met_stroke)
                met_setpatternstroke(pgs);
            else {
                gs_throw(-1, "pattern has no context");
                return -1;
            }
        }
    }

    /* nb metro state pointer */
    pat->visual = ms;
    gs_free_object(ms->memory, data, "VisualBrush_done");
    return 0; /* incomplete */
}


const met_element_t met_element_procs_VisualBrush = {
    "VisualBrush",
    {
        VisualBrush_cook,
        VisualBrush_action,
        VisualBrush_done
    }
};
