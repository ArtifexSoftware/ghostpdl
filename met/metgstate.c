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
/*$Id: */

/* metgstate.c */
/* metro graphics state operators */

#include "std.h"
#include "metsimple.h"
#include "metstate.h"
#include "metgstate.h"
#include "metutil.h"
#include "gxstate.h" /* for gs_state_copy_reason_t */
#include "string_.h"
#include "ctype_.h"

typedef struct metgstate_s {
    char *Stroke;
    char *Fill;
    ST_ZeroOne Opacity;
    char *FillRule;
    bool isClosed;
    bool charpathmode;
    char *CharFill;
    met_path_child_t child;
    void *VisualBrush;
    gs_memory_t *pmem;
    met_state_t *ms;
} metgstate_t;

private void *
met_gstate_alloc(gs_memory_t *pmem)
{
    metgstate_t *pmg = (metgstate_t *)gs_alloc_bytes(pmem,
                               sizeof(metgstate_t),
                               "met_gstate_alloc");
    memset(pmg, 0, sizeof(metgstate_t));
    pmg->pmem = pmem;
    return pmg;
}

private int
met_gstate_copy_for(void *pto, void *pfrom, gs_state_copy_reason_t reason)
{
    /* reason is ignored for now */
    metgstate_t *pt = pto;
    metgstate_t *pf = pfrom;

    /* free old prior to deep copy */
    gs_free_object(pf->pmem, pt->Stroke, "met_gstate_free");
    gs_free_object(pf->pmem, pt->Fill, "met_gstate_free");
    gs_free_object(pf->pmem, pt->FillRule, "met_gstate_free");
    gs_free_object(pf->pmem, pt->CharFill, "met_gstate_free");

    /* struct copy */
    *pt = *pf;
    
    /* deep copy */    
    pt->Stroke = met_strdup(pf->pmem, pf->Stroke, "met_gstate_copy_for");
    pt->Fill = met_strdup(pf->pmem, pf->Fill, "met_gstate_copy_for");
    pt->FillRule = met_strdup(pf->pmem, pf->FillRule, "met_gstate_copy_for");
    pt->CharFill = met_strdup(pf->pmem, pf->CharFill, "met_gstate_copy_for");

    return 0;
}

private void
met_gstate_free(void *pold, gs_memory_t *pmem)
{
    metgstate_t *pmetgs = (metgstate_t *)pold;
    gs_free_object(pmem, pmetgs->Stroke, "met_gstate_free");
    gs_free_object(pmem, pmetgs->Fill, "met_gstate_free");
    gs_free_object(pmem, pmetgs->FillRule, "met_gstate_free");
    gs_free_object(pmem, pmetgs->CharFill, "met_gstate_free");
    // TODO free gradientstops
    gs_free_object(pmem, pmetgs, "met_gstate_free");
}

private const ST_RscRefColor met_pattern = "xxx";

ST_RscRefColor
met_currentstrokecolor(gs_state *pgs)
{
    metgstate_t *pmg = gs_state_client_data(pgs);
    return pmg->Stroke;
}
    
void
met_setstrokecolor(gs_state *pgs, const ST_RscRefColor color)
{
    metgstate_t *pmg = gs_state_client_data(pgs);

    gs_free_object(pmg->pmem, pmg->Stroke, "met_setstrokecolor");
    pmg->Stroke = met_strdup(pmg->pmem, color, "met_setstrokecolor");
}

ST_RscRefColor
met_currentfillcolor(gs_state *pgs)
{
    metgstate_t *pmg = gs_state_client_data(pgs);
    return pmg->Fill;
}
    
void
met_setfillcolor(gs_state *pgs, const ST_RscRefColor color)
{
    metgstate_t *pmg = gs_state_client_data(pgs);
    gs_free_object(pmg->pmem, pmg->Fill, "met_setfillcolor");
    pmg->Fill = met_strdup(pmg->pmem, color, "met_setfillcolor");
}

void
met_setpatternstroke(gs_state *pgs)
{
    met_setstrokecolor(pgs, met_pattern);
}

void
met_setpatternfill(gs_state *pgs)
{
    met_setfillcolor(pgs, met_pattern);
}


void
met_setfillrule(gs_state *pgs, const ST_FillRule fill)
{
    metgstate_t *pmg = gs_state_client_data(pgs);
    gs_free_object(pmg->pmem, pmg->FillRule, "met_setfillrulecolor");
    pmg->FillRule = met_strdup(pmg->pmem, fill, "met_setfillrulecolor");
}

ST_FillRule
met_currentfillrule(gs_state *pgs)
{
    metgstate_t *pmg = gs_state_client_data(pgs);
    return pmg->FillRule;
}    

void
met_setopacity(gs_state *pgs, const ST_ZeroOne opacity)
{
    metgstate_t *pmg = gs_state_client_data(pgs);
    pmg->Opacity = opacity;
}

ST_ZeroOne
met_currentopacity(gs_state *pgs)
{
    metgstate_t *pmg = gs_state_client_data(pgs);
    return pmg->Opacity;
}

met_path_t
met_currentpathtype(gs_state *pgs) 
{ 
    if (met_currentstrokecolor(pgs) && met_currentfillcolor(pgs))
        return met_stroke_and_fill;
    if (met_currentfillcolor(pgs))
        return met_fill_only;
    if (met_currentstrokecolor(pgs))
        return met_stroke_only;
    return met_path_undefined;
}

/* if set all paths should be closed */
void 
met_setclosepath(gs_state *pgs, bool close)
{
    metgstate_t *pmg = gs_state_client_data(pgs);
    pmg->isClosed = close;
}

bool met_currentclosepath(gs_state *pgs)
{
    metgstate_t *pmg = gs_state_client_data(pgs);
    return pmg->isClosed;
}   

bool met_currenteofill(gs_state *pgs)
{
    /* if not set even odd */
    if (!met_currentfillrule(pgs))
        return true;

    return (!strcasecmp(met_currentfillrule(pgs), "EvenOdd"));
}

met_path_child_t met_currentpathchild(gs_state *pgs)
{
    metgstate_t *pmg = gs_state_client_data(pgs);
    return pmg->child;
}

void met_setpathchild(gs_state *pgs, met_path_child_t child)
{
    metgstate_t *pmg = gs_state_client_data(pgs);
    pmg->child = child;
}

bool
met_iscolorpattern(gs_state *pgs, ST_RscRefColor color)
{
    if (!color)
        return false;
    return !strcmp(color, met_pattern);
}

void
met_setcharpathmode(gs_state *pgs, bool mode)
{
    metgstate_t *pmg = gs_state_client_data(pgs);
    pmg->charpathmode = mode;
}

bool
met_currentcharpathmode(gs_state *pgs)
{
    metgstate_t *pmg = gs_state_client_data(pgs);
    return pmg->charpathmode;
}

void
met_setcharfillcolor(gs_state *pgs, ST_RscRefColor color)
{
    metgstate_t *pmg = gs_state_client_data(pgs);
    pmg->CharFill = color;
}

ST_RscRefColor met_currentcharfillcolor(gs_state *pgs)
{
    metgstate_t *pmg = gs_state_client_data(pgs);
    return pmg->CharFill;
}

private metgstate_t *
client_data(gs_state *pgs)
{
    return gs_state_client_data(pgs);
}

/* nb should be a resource type */
void
met_setvisualbrush(gs_state *pgs, void *vb)
{
    client_data(pgs)->VisualBrush = vb;
}
    
void *met_currentvisualbrush(gs_state *pgs)
{
    return client_data(pgs)->VisualBrush;
}

private const gs_state_client_procs met_gstate_procs = {
    met_gstate_alloc,
    0,
    met_gstate_free,
    met_gstate_copy_for
};


/* constructor for gstate */
void
met_gstate_init(gs_state *pgs, gs_memory_t *pmem, met_state_t *ms)
{
    metgstate_t *pmg = met_gstate_alloc(pmem);
    pmg->Stroke = 0;
    pmg->Opacity = 0;
    pmg->FillRule = 0;
    pmg->isClosed = 0;
    pmg->charpathmode = 0;
    pmg->CharFill = 0;
    pmg->child = met_none;
    pmg->pmem = pmem;
    pmg->VisualBrush = 0;
    pmg->ms = ms;
    gs_state_set_client(pgs, pmg, &met_gstate_procs, false);
}

met_state_t *
met_state_from_gstate(gs_state *pgs)
{
    return client_data(pgs)->ms;
}

