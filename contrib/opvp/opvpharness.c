/* Test harness for checking results after
   removal of globals in opvp device. Note,
   0.2 API not tested. Assumption is OpenPrinter
   is the first method called and ClosePrinter
   will be the final method called. If not,
   there will be problems. */

#include "opvp.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Yes globals, but here we don't care */
FILE *pFile = NULL;

/* Graphic state members of API that are set and get. */
typedef struct opvp_gstate_s {
    opvp_ctm_t ctm;
    opvp_cspace_t colorspace;
    opvp_fillmode_t fill_mode;
    opvp_float_t alpha;
    opvp_fix_t line_width;
    opvp_int_t line_dash_n;
    opvp_fix_t *line_dash_array;
    opvp_fix_t dash_offset;
    opvp_linestyle_t line_style;
    opvp_linecap_t line_cap;
    opvp_linejoin_t line_join;
    opvp_fix_t miter_limit;
    opvp_paintmode_t paintmode;
} opvp_gstate_t;

opvp_gstate_t opv_gstate;

static opvp_result_t
opvpClosePrinter(opvp_dc_t printerContext)
{
    fputs("opvpClosePrinter\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fclose(pFile);

    if (opv_gstate.line_dash_array != NULL)
        free(opv_gstate.line_dash_array);

    return 0;
}

static opvp_result_t
opvpStartJob(opvp_dc_t printerContext, const opvp_char_t *job_info)
{
    fputs("opvpStartJob\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tjob_info = %s\n", job_info);
    return 0;
}

static opvp_result_t
opvpEndJob(opvp_dc_t printerContext)
{
    fputs("opvpEndJob\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpAbortJob(opvp_dc_t printerContext)
{
    fputs("opvpAbortJob\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpStartDoc(opvp_dc_t printerContext, const opvp_char_t *docinfo)
{
    fputs("opvpStartDoc\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tjob_info = %s\n", docinfo);
    return 0;
}

static opvp_result_t
opvpEndDoc(opvp_dc_t printerContext)
{
    fputs("opvpEndDoc\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpStartPage(opvp_dc_t printerContext, const opvp_char_t *pageinfo)
{
    fputs("opvpStartPage\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tjob_info = %s\n", pageinfo);
    return 0;
}

static opvp_result_t
opvpEndPage(opvp_dc_t printerContext)
{
    fputs("opvpEndPage\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

/* Not used */
static opvp_result_t
opvpQueryDeviceCapability(opvp_dc_t printerContext, opvp_flag_t flag, opvp_int_t *a, opvp_byte_t *b)
{
    fputs("opvpQueryDeviceCapability\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

/* Not used */
static opvp_result_t
opvpQueryDeviceInfo(opvp_dc_t printerContext, opvp_flag_t flag, opvp_int_t *a, opvp_char_t *b)
{
    fputs("opvpQueryDeviceInfo\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpResetCTM(opvp_dc_t printerContext)
{
    fputs("opvpResetCTM\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    opv_gstate.ctm.a = 1.0;
    opv_gstate.ctm.b = 0.0;
    opv_gstate.ctm.c = 0.0;
    opv_gstate.ctm.d = 1.0;
    opv_gstate.ctm.e = 0.0;
    opv_gstate.ctm.f = 0.0;
    return 0;
}

static opvp_result_t
opvpSetCTM(opvp_dc_t printerContext, const opvp_ctm_t *pctm)
{
    fputs("opvpSetCTM\n", pFile);
    opv_gstate.ctm = *pctm;
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tctm.a = %f\n", pctm->a);
    fprintf(pFile, "\tctm.b = %f\n", pctm->b);
    fprintf(pFile, "\tctm.c = %f\n", pctm->c);
    fprintf(pFile, "\tctm.d = %f\n", pctm->d);
    fprintf(pFile, "\tctm.e = %f\n", pctm->e);
    fprintf(pFile, "\tctm.f = %f\n", pctm->f);
    return 0;
}

static opvp_result_t
opvpGetCTM(opvp_dc_t printerContext, opvp_ctm_t *pctm)
{
    fputs("opvpGetCTM\n", pFile);
    *pctm = opv_gstate.ctm;
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpInitGS(opvp_dc_t printerContext)
{
    fputs("opvpInitGS\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpSaveGS(opvp_dc_t printerContext)
{
    fputs("opvpSaveGS\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpRestoreGS(opvp_dc_t printerContext)
{
    fputs("opvpRestoreGS\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpQueryColorSpace(opvp_dc_t printerContext, opvp_int_t *n, opvp_cspace_t *colorspace)
{
    fputs("opvpQueryColorSpace\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    *n = 0;
    *colorspace = opv_gstate.colorspace;
    return 0;
}

static opvp_result_t
opvpSetColorSpace(opvp_dc_t printerContext, opvp_cspace_t colorspace)
{
    fputs("opvpSetColorSpace\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tcolorspace = %d\n", colorspace);
    opv_gstate.colorspace = colorspace;
    return 0;
}

static opvp_result_t
opvpGetColorSpace(opvp_dc_t printerContext, opvp_cspace_t *colorspace)
{
    fputs("opvpGetColorSpace\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    *colorspace = opv_gstate.colorspace;
    return 0;
}

static opvp_result_t
opvpSetFillMode(opvp_dc_t printerContext, opvp_fillmode_t fillmode)
{
    fputs("opvpSetFillMode\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tfillmode = %d\n", fillmode);
    opv_gstate.fill_mode = fillmode;
    return 0;
}

static opvp_result_t
opvpGetFillMode(opvp_dc_t printerContext, opvp_fillmode_t *fillmode)
{
    fputs("opvpGetFillMode\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    *fillmode = opv_gstate.fill_mode;
    return 0;
}

static opvp_result_t
opvpSetAlphaConstant(opvp_dc_t printerContext, opvp_float_t alpha)
{
    fputs("opvpSetAlphaConstant\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\talpha = %f\n", alpha);
    opv_gstate.alpha = alpha;
    return 0;
}

static opvp_result_t
opvpGetAlphaConstant(opvp_dc_t printerContext, opvp_float_t *alpha)
{
    fputs("opvpGetAlphaConstant\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    *alpha = opv_gstate.alpha;
    return 0;
}

static opvp_result_t
opvpSetLineWidth(opvp_dc_t printerContext, opvp_fix_t width)
{
    fputs("opvpSetLineWidth\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\twidth = %d\n", width);
    opv_gstate.line_width = width;
    return 0;
}

static opvp_result_t
opvpGetLineWidth(opvp_dc_t printerContext, opvp_fix_t *width)
{
    fputs("opvpGetLineWidth\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    *width = opv_gstate.line_width;
    return 0;
}

static opvp_result_t
opvpSetLineDash(opvp_dc_t printerContext, opvp_int_t n, const opvp_fix_t *dash)
{
    int k;

    fputs("opvpSetLineDash\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tn = %d\n", n);
    opv_gstate.line_dash_n = n;

    if (opv_gstate.line_dash_array != NULL)
        free(opv_gstate.line_dash_array);

    opv_gstate.line_dash_array = (opvp_fix_t*) malloc(n * sizeof(opvp_fix_t));
    if (opv_gstate.line_dash_array == NULL) {
        fprintf(pFile,"\t Error in dash array allocation.  Exiting.");
        fclose(pFile);
        exit(-1);
    }

    for (k = 0; k < n; k++) {
        opv_gstate.line_dash_array[k] =  dash[k];
        fprintf(pFile, "\tdash[%d] = %d\n", k, dash[k]);
    }
    return 0;
}

static opvp_result_t
opvpGetLineDash(opvp_dc_t printerContext, opvp_int_t *n, opvp_fix_t *dash)
{
    fputs("opvpGetLineDash\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);

    *n = opv_gstate.line_dash_n;
    dash = opv_gstate.line_dash_array;

    return 0;
}

static opvp_result_t
opvpSetLineDashOffset(opvp_dc_t printerContext, opvp_fix_t offset)
{
    fputs("opvpSetLineDashOffset\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\toffset = %d\n", offset);
    opv_gstate.dash_offset = offset;
    return 0;
}

static opvp_result_t
opvpGetLineDashOffset(opvp_dc_t printerContext, opvp_fix_t *offset)
{
    fputs("opvpGetLineDashOffset\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    *offset = opv_gstate.dash_offset;
    return 0;
}

static opvp_result_t
opvpSetLineStyle(opvp_dc_t printerContext, opvp_linestyle_t style)
{
    fputs("opvpSetLineStyle\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tstyle = %d\n", style);
    opv_gstate.line_style = style;
    return 0;
}

static opvp_result_t
opvpGetLineStyle(opvp_dc_t printerContext, opvp_linestyle_t *style)
{
    fputs("opvpGetLineStyle\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    *style = opv_gstate.line_style;
    return 0;
}

static opvp_result_t
opvpSetLineCap(opvp_dc_t printerContext, opvp_linecap_t cap)
{
    fputs("opvpSetLineCap\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tcap = %d\n", cap);
    opv_gstate.line_cap = cap;
    return 0;
}

static opvp_result_t
opvpGetLineCap(opvp_dc_t printerContext, opvp_linecap_t *cap)
{
    fputs("opvpGetLineCap\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    *cap = opv_gstate.line_cap;
    return 0;
}

static opvp_result_t
opvpSetLineJoin(opvp_dc_t printerContext, opvp_linejoin_t join)
{
    fputs("opvpSetLineJoin\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tjoin = %d\n", join);
    opv_gstate.line_join = join;
    return 0;
}

static opvp_result_t
opvpGetLineJoin(opvp_dc_t printerContext, opvp_linejoin_t *join)
{
    fputs("opvpGetLineJoin\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    *join = opv_gstate.line_join;
    return 0;
}

static opvp_result_t
opvpSetMiterLimit(opvp_dc_t printerContext, opvp_fix_t miter)
{
    fputs("opvpSetMiterLimit\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tmiter = %d\n", miter);
    opv_gstate.miter_limit = miter;
    return 0;
}

static opvp_result_t
opvpGetMiterLimit(opvp_dc_t printerContext, opvp_fix_t *miter)
{
    fputs("opvpGetMiterLimit\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    *miter = opv_gstate.miter_limit;
    return 0;
}

static opvp_result_t
opvpSetPaintMode(opvp_dc_t printerContext, opvp_paintmode_t paintmode)
{
    fputs("opvpSetPaintMode\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tpaintmode = %d\n", paintmode);
    opv_gstate.paintmode = paintmode;
    return 0;
}

static opvp_result_t
opvpGetPaintMode(opvp_dc_t printerContext, opvp_paintmode_t *paintmode)
{
    fputs("opvpGetPaintMode\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    *paintmode = opv_gstate.paintmode;
    return 0;
}

static opvp_result_t
opvpSetStrokeColor(opvp_dc_t printerContext, const opvp_brush_t *strokecolor)
{
    int k;

    fputs("opvpSetStrokeColor\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    if (strokecolor ==  NULL) {
        fprintf(pFile, "\tstrokecolor is NULL\n");
    } else {
        fprintf(pFile, "\tstrokecolor.colorSpace = %d\n", strokecolor->colorSpace);

        for (k = 0; k < 4; k++) {
            fprintf(pFile, "\tstrokecolor.color[%d] = %d\n", k, strokecolor->color[k]);
        }

        fprintf(pFile, "\tstrokecolor.xorg = %d\n", strokecolor->xorg);
        fprintf(pFile, "\tstrokecolor.yorg = %d\n", strokecolor->yorg);

        if (strokecolor->pbrush ==  NULL) {
            fprintf(pFile, "\tstrokecolor.pbrush is NULL\n");
        } else {
            fprintf(pFile, "\tstrokecolor.pbrush.type = %d\n", strokecolor->pbrush->type);
            fprintf(pFile, "\tstrokecolor.pbrush.width = %d\n", strokecolor->pbrush->width);
            fprintf(pFile, "\tstrokecolor.pbrush.height = %d\n", strokecolor->pbrush->height);
            fprintf(pFile, "\tstrokecolor.pbrush.pitch = %d\n", strokecolor->pbrush->pitch);
        }
    }
    return 0;
}

static opvp_result_t
opvpSetFillColor(opvp_dc_t printerContext, const opvp_brush_t *fillcolor)
{
    int k;

    fputs("opvpSetFillColor\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    if (fillcolor ==  NULL) {
        fprintf(pFile, "\tfillcolor is NULL\n");
    } else {
        fprintf(pFile, "\tfillcolor.colorSpace = %d\n", fillcolor->colorSpace);

        for (k = 0; k < 4; k++) {
            fprintf(pFile, "\tfillcolor.color[%d] = %d\n", k, fillcolor->color[k]);
        }

        fprintf(pFile, "\tfillcolor.xorg = %d\n", fillcolor->xorg);
        fprintf(pFile, "\tfillcolor.yorg = %d\n", fillcolor->yorg);

        if (fillcolor->pbrush ==  NULL) {
            fprintf(pFile, "\tfillcolor.pbrush is NULL\n");
        } else {
            fprintf(pFile, "\tfillcolor.pbrush.type = %d\n", fillcolor->pbrush->type);
            fprintf(pFile, "\tfillcolor.pbrush.width = %d\n", fillcolor->pbrush->width);
            fprintf(pFile, "\tfillcolor.pbrush.height = %d\n", fillcolor->pbrush->height);
            fprintf(pFile, "\tfillcolor.pbrush.pitch = %d\n", fillcolor->pbrush->pitch);
        }
    }
    return 0;
}

static opvp_result_t
opvpSetBgColor(opvp_dc_t printerContext, const opvp_brush_t *bgcolor)
{
    int k;

    fputs("opvpSetBgColor\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    if (bgcolor ==  NULL) {
        fprintf(pFile, "\tfillcbgcolorolor is NULL\n");
    } else {
        fprintf(pFile, "\tbgcolor.colorSpace = %d\n", bgcolor->colorSpace);

        for (k = 0; k < 4; k++) {
            fprintf(pFile, "\tbgcolor.color[%d] = %d\n", k, bgcolor->color[k]);
        }

        fprintf(pFile, "\tbgcolor.xorg = %d\n", bgcolor->xorg);
        fprintf(pFile, "\tbgcolor.yorg = %d\n", bgcolor->yorg);

        if (bgcolor->pbrush ==  NULL) {
            fprintf(pFile, "\tbgcolor.pbrush is NULL\n");
        } else {
            fprintf(pFile, "\tbgcolor.pbrush.type = %d\n", bgcolor->pbrush->type);
            fprintf(pFile, "\tbgcolor.pbrush.width = %d\n", bgcolor->pbrush->width);
            fprintf(pFile, "\tbgcolor.pbrush.height = %d\n", bgcolor->pbrush->height);
            fprintf(pFile, "\tbgcolor.pbrush.pitch = %d\n", bgcolor->pbrush->pitch);
        }
    }
    return 0;
}

static opvp_result_t
opvpNewPath(opvp_dc_t printerContext)
{
    fputs("opvpNewPath\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpEndPath(opvp_dc_t printerContext)
{
    fputs("opvpEndPath\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpStrokePath(opvp_dc_t printerContext)
{
    fputs("opvpStrokePath\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpFillPath(opvp_dc_t printerContext)
{
    fputs("opvpFillPath\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpStrokeFillPath(opvp_dc_t printerContext)
{
    fputs("opvpStrokeFillPath\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpSetClipPath(opvp_dc_t printerContext, opvp_cliprule_t cliprule)
{
    fputs("opvpSetClipPath\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tcliprule = %d\n", cliprule);
    return 0;
}

static opvp_result_t
opvpResetClipPath(opvp_dc_t printerContext)
{
    fputs("opvpResetClipPath\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpSetCurrentPoint(opvp_dc_t printerContext, opvp_fix_t pointx, opvp_fix_t pointy)
{
    fputs("opvpSetCurrentPoint\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tpointx = %d\n", pointx);
    fprintf(pFile, "\tpointy = %d\n", pointy);
    return 0;
}

static opvp_result_t
opvpLinePath(opvp_dc_t printerContext, opvp_pathmode_t pathmode, opvp_int_t npoints, const opvp_point_t *points)
{
    int k;

    fputs("opvpLinePath\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tpathmode = %d\n", pathmode);
    fprintf(pFile, "\tnpoints = %d\n", npoints);

    for (k = 0; k < npoints; k++) {
        fprintf(pFile, "\tpoints[%d].x = %d\n", k, points[k].x);
        fprintf(pFile, "\tpoints[%d].y = %d\n", k, points[k].y);
    }
    return 0;
}

/* Not used */
static opvp_result_t
opvpPolygonPath(opvp_dc_t printerContext, opvp_int_t n, const opvp_int_t *m, const opvp_point_t *p)
{
    fputs("opvpPolygonPath\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpRectanglePath(opvp_dc_t printerContext, opvp_int_t n, const opvp_rectangle_t *rects)
{
    int k;

    fputs("opvpRectanglePath\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tn = %d\n", n);

    for (k = 0; k < n; k++) {
        fprintf(pFile, "\trects[%d].p0.x = %d\n", k, rects[k].p0.x);
        fprintf(pFile, "\trects[%d].p0.y = %d\n", k, rects[k].p0.y);
        fprintf(pFile, "\trects[%d].p1.x = %d\n", k, rects[k].p1.x);
        fprintf(pFile, "\trects[%d].p1.y = %d\n", k, rects[k].p1.y);
    }
    return 0;
}

static opvp_result_t
opvpRoundRectanglePath(opvp_dc_t printerContext, opvp_int_t n, const opvp_roundrectangle_t *rects)
{
    int k;

    fputs("opvpRoundRectanglePath\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tn = %d\n", n);

    for (k = 0; k < n; k++) {
        fprintf(pFile, "\trects[%d].p0.x = %d\n", k, rects[k].p0.x);
        fprintf(pFile, "\trects[%d].p0.y = %d\n", k, rects[k].p0.y);
        fprintf(pFile, "\trects[%d].p1.x = %d\n", k, rects[k].p1.x);
        fprintf(pFile, "\trects[%d].p1.y = %d\n", k, rects[k].p1.y);
        fprintf(pFile, "\trects[%d].xellipse = %d\n", k, rects[k].xellipse);
        fprintf(pFile, "\trects[%d].yellipse = %d\n", k, rects[k].yellipse);
    }
    return 0;
}

static opvp_result_t
opvpBezierPath(opvp_dc_t printerContext, opvp_int_t n, const opvp_point_t *points)
{
    int k;

    fputs("opvpBezierPath\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tn = %d\n", n);

    for (k = 0; k < n; k++) {
        fprintf(pFile, "\tpoints[%d].x = %d\n", k, points[k].x);
        fprintf(pFile, "\trects[%d].y = %d\n", k, points[k].y);

    }
    return 0;
}

/* Not used */
static opvp_result_t
opvpArcPath(opvp_dc_t printerContext, opvp_arcmode_t mode, opvp_arcdir_t dir,
    opvp_fix_t a, opvp_fix_t b, opvp_fix_t c, opvp_fix_t d, opvp_fix_t e, opvp_fix_t f,
    opvp_fix_t g, opvp_fix_t h)
{
    fputs("opvpArcPath\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpDrawImage(opvp_dc_t printerContext, opvp_int_t sw, opvp_int_t sh, opvp_int_t raster,
    opvp_imageformat_t format, opvp_int_t dw, opvp_int_t dh, const void *data)
{
    int k;
    unsigned char *data_char = (unsigned char*) data;

    fputs("opvpDrawImage\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tsw = %d\n", sw);
    fprintf(pFile, "\tsh = %d\n", sh);
    fprintf(pFile, "\traster = %d\n", raster);
    fprintf(pFile, "\tformat = %d\n", format);
    fprintf(pFile, "\tdw = %d\n", dw);
    fprintf(pFile, "\tdh = %d\n", dh);

    /* No idea how big data is here... Try sw, as byte? */
    for (k = 0; k < sw; k++) {
        fprintf(pFile, "\tdata[%d] = %d\n", k, data_char[k]);
    }

    return 0;
}

static opvp_result_t
opvpStartDrawImage(opvp_dc_t printerContext, opvp_int_t sw, opvp_int_t sh, opvp_int_t raster,
    opvp_imageformat_t format, opvp_int_t dw, opvp_int_t dh)
{
    fputs("opvpStartDrawImage\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tsw = %d\n", sw);
    fprintf(pFile, "\tsh = %d\n", sh);
    fprintf(pFile, "\traster = %d\n", raster);
    fprintf(pFile, "\tformat = %d\n", format);
    fprintf(pFile, "\tdw = %d\n", dw);
    fprintf(pFile, "\tdh = %d\n", dh);
    return 0;
}

static opvp_result_t
opvpTransferDrawImage(opvp_dc_t printerContext, opvp_int_t count, const void *data)
{
    int k;
    unsigned char *data_char = (unsigned char*) data;

    fputs("opvpTransferDrawImage\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\tcount = %d\n", count);

    for (k = 0; k < count; k++) {
        fprintf(pFile, "\tdata[%d] = %d\n", k, data_char[k]);
    }
    return 0;
}

static opvp_result_t
opvpEndDrawImage(opvp_dc_t printerContext)
{
    fputs("opvpEndDrawImage\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

/* not used */
static opvp_result_t
opvpStartScanline(opvp_dc_t printerContext, opvp_int_t a)
{
    fputs("opvpStartScanline\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

/* not used */
static opvp_result_t
opvpScanline(opvp_dc_t printerContext, opvp_int_t a, const opvp_int_t *b)
{
    fputs("opvpScanline\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

/* not used */
static opvp_result_t
opvpEndScanline(opvp_dc_t printerContext)
{
    fputs("opvpEndScanline\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpStartRaster(opvp_dc_t printerContext, opvp_int_t width)
{
    fputs("opvpStartRaster\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\twidth = %d\n", width);
    return 0;
}

static opvp_result_t
opvpTransferRasterData(opvp_dc_t printerContext, opvp_int_t raster_size, const opvp_byte_t *data)
{
    int k;

    fputs("opvpTransferRasterData\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\traster_size = %d\n", raster_size);

    for (k = 0; k < raster_size; k++) {
        fprintf(pFile, "\tdata[%d] = %d\n", k, data[k]);
    }
    return 0;
}

static opvp_result_t
opvpSkipRaster(opvp_dc_t printerContext, opvp_int_t a)
{
    fputs("opvpSkipRaster\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    fprintf(pFile, "\ta = %d\n", a);
    return 0;
}

static opvp_result_t
opvpEndRaster(opvp_dc_t printerContext)
{
    fputs("opvpEndRaster\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpStartStream(opvp_dc_t printerContext)
{
    fputs("opvpStartStream\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

/* Not used */
static opvp_result_t
opvpTransferStreamData(opvp_dc_t printerContext, opvp_int_t a, const void *b)
{
    fputs("opvpTransferStreamData\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

static opvp_result_t
opvpEndStream(opvp_dc_t printerContext)
{
    fputs("opvpEndStream\n", pFile);
    fprintf(pFile, "\tContext = %d\n", printerContext);
    return 0;
}

opvp_int_t opvpErrorNo;

opvp_dc_t
opvpOpenPrinter(opvp_int_t outputFD, const opvp_char_t *printerModel, const opvp_int_t apiVersion[2],
        opvp_api_procs_t **apiProcs)
{
    static opvp_api_procs_t procs;
    opvp_fix_t fixed_value;

    procs.opvpEndStream = opvpEndStream;
    procs.opvpClosePrinter = opvpClosePrinter;
    procs.opvpStartJob = opvpStartJob;
    procs.opvpEndJob = opvpEndJob;
    procs.opvpAbortJob = opvpAbortJob;
    procs.opvpStartDoc = opvpStartDoc;
    procs.opvpEndDoc = opvpEndDoc;
    procs.opvpStartPage = opvpStartPage;
    procs.opvpEndPage = opvpEndPage;
    procs.opvpQueryDeviceCapability = opvpQueryDeviceCapability;
    procs.opvpQueryDeviceInfo = opvpQueryDeviceInfo;
    procs.opvpResetCTM = opvpResetCTM;
    procs.opvpSetCTM = opvpSetCTM;
    procs.opvpGetCTM = opvpGetCTM;
    procs.opvpInitGS = opvpInitGS;
    procs.opvpSaveGS = opvpSaveGS;
    procs.opvpRestoreGS = opvpRestoreGS;
    procs.opvpQueryColorSpace = opvpQueryColorSpace;
    procs.opvpSetColorSpace = opvpSetColorSpace;
    procs.opvpGetColorSpace = opvpGetColorSpace;
    procs.opvpSetFillMode = opvpSetFillMode;
    procs.opvpGetFillMode = opvpGetFillMode;
    procs.opvpSetAlphaConstant = opvpSetAlphaConstant;
    procs.opvpGetAlphaConstant = opvpGetAlphaConstant;
    procs.opvpSetLineWidth = opvpSetLineWidth;
    procs.opvpGetLineWidth = opvpGetLineWidth;
    procs.opvpSetLineDash = opvpSetLineDash;
    procs.opvpSetLineDash = opvpSetLineDash;
    procs.opvpGetLineDash = opvpGetLineDash;
    procs.opvpSetLineDashOffset = opvpSetLineDashOffset;
    procs.opvpGetLineDashOffset = opvpGetLineDashOffset;
    procs.opvpSetLineStyle = opvpSetLineStyle;
    procs.opvpGetLineStyle = opvpGetLineStyle;
    procs.opvpSetLineCap = opvpSetLineCap;
    procs.opvpGetLineCap = opvpGetLineCap;
    procs.opvpSetLineJoin = opvpSetLineJoin;
    procs.opvpGetLineJoin = opvpGetLineJoin;
    procs.opvpSetMiterLimit = opvpSetMiterLimit;
    procs.opvpGetMiterLimit = opvpGetMiterLimit;
    procs.opvpSetPaintMode = opvpSetPaintMode;
    procs.opvpGetPaintMode = opvpGetPaintMode;
    procs.opvpSetStrokeColor = opvpSetStrokeColor;
    procs.opvpSetFillColor = opvpSetFillColor;
    procs.opvpSetBgColor = opvpSetBgColor;
    procs.opvpNewPath = opvpNewPath;
    procs.opvpEndPath = opvpEndPath;
    procs.opvpStrokePath = opvpStrokePath;
    procs.opvpFillPath = opvpFillPath;
    procs.opvpStrokeFillPath = opvpStrokeFillPath;
    procs.opvpSetClipPath = opvpSetClipPath;
    procs.opvpResetClipPath = opvpResetClipPath;
    procs.opvpSetCurrentPoint = opvpSetCurrentPoint;
    procs.opvpLinePath = opvpLinePath;
    procs.opvpPolygonPath = opvpPolygonPath;
    procs.opvpRectanglePath = opvpRectanglePath;
    procs.opvpRoundRectanglePath = opvpRoundRectanglePath;
    procs.opvpBezierPath = opvpBezierPath;
    procs.opvpArcPath = opvpArcPath;
    procs.opvpDrawImage = opvpDrawImage;
    procs.opvpStartDrawImage = opvpStartDrawImage;
    procs.opvpTransferDrawImage = opvpTransferDrawImage;
    procs.opvpEndDrawImage = opvpEndDrawImage;
    procs.opvpStartScanline = opvpStartScanline;
    procs.opvpScanline = opvpScanline;
    procs.opvpEndScanline = opvpEndScanline;
    procs.opvpStartRaster = opvpStartRaster;
    procs.opvpTransferRasterData = opvpTransferRasterData;
    procs.opvpSkipRaster = opvpSkipRaster;
    procs.opvpEndRaster = opvpEndRaster;
    procs.opvpStartStream = opvpStartStream;
    procs.opvpTransferStreamData = opvpTransferStreamData;
    procs.opvpEndStream = opvpEndStream;

    *apiProcs = &procs;

    OPVP_F2FIX(1.0, fixed_value);

    opv_gstate.ctm.a = 1.0;
    opv_gstate.ctm.b = 0.0;
    opv_gstate.ctm.c = 0.0;
    opv_gstate.ctm.d = 1.0;
    opv_gstate.ctm.e = 0.0;
    opv_gstate.ctm.f = 0.0;
    opv_gstate.colorspace = OPVP_CSPACE_DEVICERGB;
    opv_gstate.fill_mode = OPVP_FILLMODE_EVENODD;
    opv_gstate.alpha = 1.0;
    opv_gstate.line_width = fixed_value;
    opv_gstate.line_dash_n = 0;
    opv_gstate.line_dash_array = NULL;
    opv_gstate.dash_offset = fixed_value;
    opv_gstate.line_style = OPVP_LINESTYLE_SOLID;
    opv_gstate.line_cap = OPVP_LINECAP_BUTT;
    opv_gstate.line_join = OPVP_LINEJOIN_MITER;
    opv_gstate.miter_limit = fixed_value;
    opv_gstate.paintmode = OPVP_PAINTMODE_OPAQUE;
    opv_gstate.colorspace = OPVP_CSPACE_DEVICERGB;

    pFile = fopen("opvp_command_dump.txt","w");
    if (pFile != NULL)
    {
        fputs("opvpOpenPrinter\n", pFile);
        return 0;
    }
    return -1;
}
