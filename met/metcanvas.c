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

/* the following is a template for an element implemenation.  Copy the
   template and then replace the word ELEMENT (case matters) with the
   name of the element - i.e. FixedPage, Path, etc.  This fills in the
   boilerplate of the template

   -------------  BEGIN TEMPLATE --------------- */

/* element constructor */

#include <stdlib.h>  /* NB for atof */
#include "memory_.h"
#include "ctype_.h"
#include "metcomplex.h"
#include "metstate.h"
#include "metelement.h"
#include "metutil.h"
#include "metparse.h"
#include "gserror.h"
#include "gscoord.h"
#include "gxfixed.h" /* for gdevbbox.h stuff */
#include "gxdevcli.h" /* for gdevbbox.h stuff */
#include "gdevbbox.h"
#include "metgstate.h"

private int
Canvas_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_Canvas *aCanvas = 
        (CT_Canvas *)gs_alloc_bytes(ms->memory,
                                     sizeof(CT_Canvas),
                                       "Canvas_cook");
    int i;

    memset(aCanvas, 0, sizeof(CT_Canvas));
    aCanvas->Opacity = 1;
#define MYSET(field, value)                                                   \
    met_cmp_and_set((field), attr[i], attr[i+1], (value))

    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {
        if (!MYSET(&aCanvas->RenderTransform, "RenderTransform"))
            ;
        else if (!MYSET(&aCanvas->Clip, "Clip"))
            ;
        else if (!MYSET(&aCanvas->Name, "Name"))
            ;
        else if (!MYSET(&aCanvas->EdgeMode, "EdgeMode"))
            ;
        else if (!MYSET(&aCanvas->FixedPage, "FixedPage"))
            ;
        else if (!MYSET(&aCanvas->Key, "Key"))
            ;
        else if (!strcmp(attr[i], "Opacity")) {
            aCanvas->Opacity = atof(attr[i+1]);
        }
        else
            gs_warn2("unsupported attribute %s=%s",
                     attr[i], attr[i+1]);
    }
#undef MYSET
    /* copy back the data for the parser. */
    *ppdata = aCanvas;
    return 0;
#ifdef NOT_YET
    return 1; /* record me */
#endif
}

/* NB duplication */
private bool is_Data_delimeter(char b) 
{
    return (b == ',') || (isspace(b));
}

/* action associated with this element */
private int
Canvas_action(void *data, met_state_t *ms)
{
    CT_Canvas *aCanvas = data;
    gs_state *pgs = ms->pgs;
    int code;
    gs_matrix canvas_mat;

    if ((code = gs_gsave(pgs)) < 0)
        return gs_rethrow(code, "gsave failed");
    if ((code = met_get_transform(&canvas_mat, aCanvas->RenderTransform)) < 0)
        return gs_rethrow(code, "transform failed");
    if ((code = gs_concat(pgs, &canvas_mat)) < 0)
        return gs_rethrow(code, "concat failed");
    return 0;
}

private int
Canvas_done(void *data, met_state_t *ms)
{
        
    gs_grestore(ms->pgs);
#ifdef NOT_YET
    int i;
    data_element_t *ds = (data_element_t *)data;
    gs_state *pgs = ms->pgs;
    gs_memory_t *mem = ms->memory;
    gx_device_bbox bdev;

    gs_gsave(pgs);
    gx_device_bbox_init(&bdev, NULL, mem);
    /* bias the reference count idiocy since bdev is an automatic
       variable that shouldn't be freed */
    gx_device_retain(&bdev, true);
    gs_setdevice_no_erase(pgs, &bdev);
    
    for (i = 1; i < ms->entries_recorded; i++) {
        /* already cooked so just do action and destructor.  assume
           the parser has not given us elements without
           implementations. */
        met_element_t *metp = ds[i].met_element_procs;
        void *ds_data = ds[i].data;
        
        int code = (*metp->action)(ds_data, ms);
        if (code < 0)
            return -1;
        code = (*metp->done)(ds_data, ms);
        if (code < 0)
            return -1;
    }
    {
        gs_rect extant_rect;
        gx_device_bbox_bbox(&bdev, &extant_rect);
	dprintf4(mem, "Bounding box: [%g %g %g %g]\n",
		 extant_rect.p.x, extant_rect.p.y, extant_rect.q.x, extant_rect.q.y);
    }
    gs_grestore(pgs);
    gs_free_object(ms->memory, ds[0].data, "Canvas_done");
#endif
    gs_free_object(ms->memory, data, "Canvas_done");
    return 0; /* incomplete */
}


const met_element_t met_element_procs_Canvas = {
    "Canvas",
    {
        Canvas_cook,
        Canvas_action,
        Canvas_done
    }
};

private int
Canvas_RenderTransform_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_MatrixTransform *aRenderTransform = 
        (CT_MatrixTransform *)gs_alloc_bytes(ms->memory,
                                     sizeof(CT_MatrixTransform),
                                       "Canvas_RenderTransform_cook");
    int i;

    memset(aRenderTransform, 0, sizeof(CT_MatrixTransform));

#define MYSET(field, value)                                                   \
    met_cmp_and_set((field), attr[i], attr[i+1], (value))

    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {

	if(!MYSET(&aRenderTransform, "MatrixTransform"))
	    ;
        else
            gs_warn2("unsupported attribute %s=%s",
                     attr[i], attr[i+1]);
    }
    /* parse attributes, filling in the zeroed out C struct */

#undef MYSET
    /* copy back the data for the parser. */
    *ppdata = aRenderTransform;
    return 0;
}

private int
Canvas_RenderTransform_action(void *data, met_state_t *ms)
{
    /*  nothing here
    CT_MatrixTransform *aRenderTransform = data;
    gs_state *pgs = ms->pgs;
    int code;
    gs_matrix canvas_mat;
    */
    return 0;
}

private int
Canvas_RenderTransform_done(void *data, met_state_t *ms)
{
    gs_free_object(ms->memory, data, "Canvas_RenderTransform_done");
    return 0; /* incomplete */
}

const met_element_t met_element_procs_Canvas_RenderTransform = {
    "Canvas_RenderTransform",
    {
        Canvas_RenderTransform_cook,
        Canvas_RenderTransform_action,
        Canvas_RenderTransform_done
    }
};

private int
MatrixTransform_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_MatrixTransform *aMatrixTransform = 
        (CT_MatrixTransform *)gs_alloc_bytes(ms->memory,
                                     sizeof(CT_MatrixTransform),
                                       "MatrixTransform_cook");
    int i;

    memset(aMatrixTransform, 0, sizeof(CT_MatrixTransform));

#define MYSET(field, value)                                                   \
    met_cmp_and_set((field), attr[i], attr[i+1], (value))

    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {

	if(!MYSET(&aMatrixTransform, "Matrix"))
	    ;
	// NB: x:Key iff in a Resource Dictionary
        else
            gs_warn2("unsupported attribute %s=%s",
                     attr[i], attr[i+1]);
    }
    /* parse attributes, filling in the zeroed out C struct */

#undef MYSET
    /* copy back the data for the parser. */
    *ppdata = aMatrixTransform;
    return 0;
}

private int
MatrixTransform_action(void *data, met_state_t *ms)
{
    CT_MatrixTransform *aMatrixTransform = data;
    gs_state *pgs = ms->pgs;
    int code;
    gs_matrix canvas_mat;

    if ((code = met_get_transform(&canvas_mat, aMatrixTransform)) < 0)
        return gs_rethrow(code, "transform failed");

    if ((code = gs_concat(pgs, &canvas_mat)) < 0)
        return gs_rethrow(code, "concat failed");

    return 0;
}

private int
MatrixTransform_done(void *data, met_state_t *ms)
{
    gs_free_object(ms->memory, data, "MatrixTransform_done");
    return 0; /* incomplete */
}

const met_element_t met_element_procs_MatrixTransform = {
    "MatrixTransform",
    {
        MatrixTransform_cook,
        MatrixTransform_action,
        MatrixTransform_done
    }
};

