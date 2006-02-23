/* a big puddle of puke needed for the headers */
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

extern int met_PaintPattern(const gs_client_color *pcc, gs_state *pgs);

/*
 * Cooking just extracts the string attributes and puts them into a C struct.
 */

private int
LinearGradientBrush_cook(void **ppdata, met_state_t *ms,
	const char *el, const char **attr)
{
    CT_LinearGradientBrush *brush;
    int i;

    brush = (void*) gs_alloc_bytes(ms->memory,
	    sizeof(CT_LinearGradientBrush),
	    "LinearGradientBrush_cook");
    if (!brush)
	return gs_throw(-1, "out of memory: CT_LinearGradientBrush");

    memset(brush, 0, sizeof(CT_LinearGradientBrush));

    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2)
    {
	if (!strcmp(attr[i], "Opacity"))
	    brush->Opacity = strdup(attr[i+1]);
	if (!strcmp(attr[i], "x:Key"))
	    brush->Key = strdup(attr[i+1]);
	if (!strcmp(attr[i], "Opacity"))
	    brush->Opacity = strdup(attr[i+1]);
	if (!strcmp(attr[i], "ColorInterpolationMode"))
	    brush->ColorInterpolationMode = strdup(attr[i+1]);
	if (!strcmp(attr[i], "SpreadMethod"))
	    brush->SpreadMethod = strdup(attr[i+1]);
	if (!strcmp(attr[i], "MappingMode"))
	    brush->MappingMode = strdup(attr[i+1]);
	if (!strcmp(attr[i], "Transform"))
	    brush->Transform = strdup(attr[i+1]);

	if (!strcmp(attr[i], "StartPoint"))
	    brush->StartPoint = strdup(attr[i+1]);
	if (!strcmp(attr[i], "EndPoint"))
	    brush->EndPoint = strdup(attr[i+1]);
    }

    /* what about child LinearGradientBrush.Transform */
    /* what about child LinearGradientBrush.GradientStops */

    *ppdata = brush;
    return gs_okay;
}

private int
RadialGradientBrush_cook(void **ppdata, met_state_t *ms,
	const char *el, const char **attr)
{
    CT_RadialGradientBrush *brush;
    int i;

    brush = (void*) gs_alloc_bytes(ms->memory,
	    sizeof(CT_RadialGradientBrush),
	    "RadialGradientBrush_cook");
    if (!brush)
	return gs_throw(-1, "out of memory: CT_RadialGradientBrush");

    memset(brush, 0, sizeof(CT_RadialGradientBrush));

    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2)
    {
	if (!strcmp(attr[i], "Opacity"))
	    brush->Opacity = strdup(attr[i+1]);
	if (!strcmp(attr[i], "x:Key"))
	    brush->Key = strdup(attr[i+1]);
	if (!strcmp(attr[i], "Opacity"))
	    brush->Opacity = strdup(attr[i+1]);
	if (!strcmp(attr[i], "ColorInterpolationMode"))
	    brush->ColorInterpolationMode = strdup(attr[i+1]);
	if (!strcmp(attr[i], "SpreadMethod"))
	    brush->SpreadMethod = strdup(attr[i+1]);
	if (!strcmp(attr[i], "MappingMode"))
	    brush->MappingMode = strdup(attr[i+1]);
	if (!strcmp(attr[i], "Transform"))
	    brush->Transform = strdup(attr[i+1]);

	if (!strcmp(attr[i], "Center"))
	    brush->Center = strdup(attr[i+1]);
	if (!strcmp(attr[i], "GradientOrigin"))
	    brush->GradientOrigin = strdup(attr[i+1]);
	if (!strcmp(attr[i], "RadiusX"))
	    brush->RadiusX = strdup(attr[i+1]);
	if (!strcmp(attr[i], "RadiusY"))
	    brush->RadiusY = strdup(attr[i+1]);
    }

    /* what about child RadialGradientBrush.Transform */
    /* what about child RadialGradientBrush.GradientStops */

    *ppdata = brush;
    return gs_okay;
}

private int LinearGradientBrush_done(void *data, met_state_t *ms)
{
    // no you don't! we're still around inside the pattern
    // gs_free_object(ms->memory, data, "LinearGradientBrush_done");
    return gs_okay;
}

private int RadialGradientBrush_done(void *data, met_state_t *ms)
{
    // no you don't! we're still around inside the pattern
    // gs_free_object(ms->memory, data, "RadialGradientBrush_done");
    return gs_okay;
}


/*
 * The paint procedures are called from the pattern struct, for now
 * located in metimage.c
 */

int LinearGradientBrush_paint(void *data, gs_state *pgs)
{
    CT_LinearGradientBrush *brush = data;

    double stops[8];
    double start[2];
    double end[2];
    int spread;
    int code;

    stops[0] = 0.0;
    stops[1] = 1.0;
    stops[2] = 0.0;
    stops[3] = 0.0;

    stops[4] = 1.0;
    stops[5] = 0.0;
    stops[6] = 0.0;
    stops[7] = 0.5;

dputs(gs_state_memory(pgs), "i'm painting linear\n");

    sscanf(brush->StartPoint, "%lg,%lg", &start[0], &start[1]);
    sscanf(brush->EndPoint, "%lg,%lg", &end[0], &end[1]);

    if (!brush->SpreadMethod)
	spread = XPS_PAD;
    else if (!strcmp(brush->SpreadMethod, "Pad"))
	spread = XPS_PAD;
    else if (!strcmp(brush->SpreadMethod, "Reflect"))
	spread = XPS_REFLECT;
    else if (!strcmp(brush->SpreadMethod, "Repeat"))
	spread = XPS_REPEAT;
    else
	spread = XPS_PAD;

    code = xps_draw_linear_gradient(gs_state_memory(pgs), pgs,
	    start, end, spread, stops, 2);
    if (code < 0)
	return gs_rethrow(code, "could not draw linear gradient brush");

    return gs_okay;
}

int RadialGradientBrush_paint(void *data, gs_state *pgs)
{
    CT_RadialGradientBrush *brush = data;

    int code;

    double stops[8];
    {
	stops[0] = 0.0;
	stops[1] = 0.9;
	stops[2] = 0.5;
	stops[3] = 0.1;
	stops[4] = 1.0;
	stops[5] = 0.1;
	stops[6] = 0.9;
	stops[7] = 0.5;
    }

    double center[2];
    double origin[2];
    double radiusx, radiusy;
    int spread;

    sscanf(brush->Center, "%lg,%lg", &center[0], &center[1]);
    sscanf(brush->GradientOrigin, "%lg,%lg", &origin[0], &origin[1]);
    sscanf(brush->RadiusX, "%lg", &radiusx);
    sscanf(brush->RadiusY, "%lg", &radiusy);

dputs(gs_state_memory(pgs), "i'm painting radial\n");

    if (!brush->SpreadMethod)
	spread = XPS_PAD;
    else if (!strcmp(brush->SpreadMethod, "Pad"))
	spread = XPS_PAD;
    else if (!strcmp(brush->SpreadMethod, "Reflect"))
	spread = XPS_REFLECT;
    else if (!strcmp(brush->SpreadMethod, "Repeat"))
	spread = XPS_REPEAT;
    else
	spread = XPS_PAD;

    code = xps_draw_radial_gradient(gs_state_memory(pgs), pgs,
	    center, origin, radiusx, radiusy,
	    spread, stops, 2);
    if (code < 0)
	return gs_rethrow(code, "could not draw radial gradient brush");

    return gs_okay;
}


/*
 * The action methods just create a pattern structure for later painting.
 */

private int CommonGradientBrush_action(void *data, met_state_t *ms, int type)
{
    gs_memory_t *mem = ms->memory;
    gs_state *pgs = ms->pgs;
    gs_matrix mat;
    gs_color_space cs;
    gs_client_pattern gspat;
    gs_client_color gscolor;

    met_pattern_t *metpat =
	(met_pattern_t *)gs_alloc_bytes(ms->memory,
					sizeof(met_pattern_t), "GradientBrush_action");

    metpat->linear = NULL;
    metpat->radial = NULL;
    metpat->raster_image = NULL;
    metpat->Viewbox.p.x = 0; metpat->Viewbox.p.y = 0;
    metpat->Viewbox.q.x = 0; metpat->Viewbox.q.y = 0;
    metpat->Viewport.p.x = 0; metpat->Viewport.p.y = 0;
    metpat->Viewport.q.x = 0; metpat->Viewport.q.y = 0;
    metpat->Transform.xx = 1.0; metpat->Transform.xy = 0.0;
    metpat->Transform.yx = 0.0; metpat->Transform.yy = 1.0;
    metpat->Transform.tx = 0.0; metpat->Transform.ty = 0.0;

    if (type == 'L')
	metpat->linear = data;
    else
	metpat->radial = data;

    gs_pattern1_init(&gspat);

    uid_set_UniqueID(&gspat.uid, gs_next_ids(mem, 1));
    gspat.PaintType = 1;
    gspat.TilingType = 1;

    /* there is no Viewbox */
    gspat.BBox.p.x = -1000;
    gspat.BBox.p.y = -1000;
    gspat.BBox.q.x = 1000;
    gspat.BBox.q.y = 1000;

    /* there is no tiling */
    gspat.XStep = 2000;
    gspat.YStep = 2000;

    gspat.PaintProc = met_PaintPattern;
    gspat.client_data = (void *)metpat;

    gs_gsave(pgs);
    gs_make_identity(&mat);
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
	else
	    return gs_throw(-1, "pattern has no context");
    }

    return gs_okay;
}

private int LinearGradientBrush_action(void *data, met_state_t *ms)
{
    return CommonGradientBrush_action(data, ms, 'L');
}

private int RadialGradientBrush_action(void *data, met_state_t *ms)
{
    return CommonGradientBrush_action(data, ms, 'R');
}


/*
 * Descriptor structs with callbacks for the parser.
 */

const met_element_t met_element_procs_LinearGradientBrush = {
    "LinearGradientBrush",
    {
	LinearGradientBrush_cook,
	LinearGradientBrush_action,
	LinearGradientBrush_done
    }
};

const met_element_t met_element_procs_RadialGradientBrush = {
    "RadialGradientBrush",
    {
	RadialGradientBrush_cook,
	RadialGradientBrush_action,
	RadialGradientBrush_done
    }
};

