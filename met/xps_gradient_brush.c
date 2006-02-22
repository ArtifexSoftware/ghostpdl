/* a big puddle of puke needed for the headers */
#include "memory_.h"
#include "gsmemory.h"
#include "gsmatrix.h"
#include "gp.h"
#include "metcomplex.h"
#include "metelement.h"
#include "metutil.h"
#include "math_.h"
#include "ctype_.h"
#include "gsimage.h"
#include "gscspace.h"
#include "gsptype1.h"
#include <stdlib.h> /* nb for atof */
#include "zipparse.h"
#include "xps_gradient_draw.h"

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
	met_cmp_and_set(&brush->Opacity, attr[i], attr[i+1], "Opacity");
	met_cmp_and_set(&brush->Key, attr[i], attr[i+1], "x:Key");
	met_cmp_and_set(&brush->ColorInterpolationMode, attr[i], attr[i+1], "ColorInterpolationMode");
	met_cmp_and_set(&brush->SpreadMethod, attr[i], attr[i+1], "SpreadMethod");
	met_cmp_and_set(&brush->MappingMode, attr[i], attr[i+1], "MappingMode");
	met_cmp_and_set(&brush->Transform, attr[i], attr[i+1], "Transform");
	met_cmp_and_set(&brush->StartPoint, attr[i], attr[i+1], "StartPoint");
	met_cmp_and_set(&brush->EndPoint, attr[i], attr[i+1], "EndPoint");
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
	met_cmp_and_set(&brush->Opacity, attr[i], attr[i+1], "Opacity");
	met_cmp_and_set(&brush->Key, attr[i], attr[i+1], "x:Key");
	met_cmp_and_set(&brush->ColorInterpolationMode, attr[i], attr[i+1], "ColorInterpolationMode");
	met_cmp_and_set(&brush->SpreadMethod, attr[i], attr[i+1], "SpreadMethod");
	met_cmp_and_set(&brush->MappingMode, attr[i], attr[i+1], "MappingMode");
	met_cmp_and_set(&brush->Transform, attr[i], attr[i+1], "Transform");
	met_cmp_and_set(&brush->Center, attr[i], attr[i+1], "Center");
	met_cmp_and_set(&brush->GradientOrigin, attr[i], attr[i+1], "GradientOrigin");
	met_cmp_and_set(&brush->RadiusX, attr[i], attr[i+1], "RadiusX");
	met_cmp_and_set(&brush->RadiusY, attr[i], attr[i+1], "RadiusY");
    }

    /* what about child RadialGradientBrush.Transform */
    /* what about child RadialGradientBrush.GradientStops */

    *ppdata = brush;
    return gs_okay;
}

private int LinearGradientBrush_action(void *data, met_state_t *ms)
{
    CT_LinearGradientBrush *brush = data;

    double stops[8];
    double start[2];
    double end[2];
    int spread;
    int code;

    stops[0] = 0.0;
    stops[1] = 0.9;
    stops[2] = 0.5;
    stops[3] = 0.1;

    stops[4] = 1.0;
    stops[5] = 0.1;
    stops[6] = 0.9;
    stops[7] = 0.5;

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

    //gs_gsave(ms->pgs);
    gs_clip(ms->pgs);

    code = xps_draw_linear_gradient(ms->memory, ms->pgs,
	    start, end, spread, stops, 2);
    if (code < 0)
	return gs_rethrow(code, "could not draw linear gradient brush");

    //gs_grestore(ms->pgs);

    return gs_okay;
}

private int LinearGradientBrush_done(void *data, met_state_t *ms)
{
    gs_free_object(ms->memory, data, "LinearGradientBrush_done");
    return gs_okay;
}

private int RadialGradientBrush_action(void *data, met_state_t *ms)
{
    CT_RadialGradientBrush *brush = data;

    double stops[8];
    double center[2];
    double origin[2];
    double radiusx, radiusy;
    int spread;
    int code;

    stops[0] = 0.0;
    stops[1] = 0.9;
    stops[2] = 0.5;
    stops[3] = 0.1;

    stops[4] = 1.0;
    stops[5] = 0.1;
    stops[6] = 0.9;
    stops[7] = 0.5;

    sscanf(brush->Center, "%lg,%lg", &center[0], &center[1]);
    sscanf(brush->GradientOrigin, "%lg,%lg", &origin[0], &origin[1]);
    sscanf(brush->RadiusX, "%lg", &radiusx);
    sscanf(brush->RadiusY, "%lg", &radiusy);

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

    //gs_gsave(ms->pgs);
    gs_clip(ms->pgs);

    code = xps_draw_radial_gradient(ms->memory, ms->pgs,
	    center, origin, radiusx, radiusy,
	    spread, stops, 2);
    if (code < 0)
	return gs_rethrow(code, "could not draw radial gradient brush");

    //gs_grestore(ms->pgs);

    return gs_okay;
}

private int RadialGradientBrush_done(void *data, met_state_t *ms)
{
    gs_free_object(ms->memory, data, "RadialGradientBrush_done");
    return gs_okay;
}

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

