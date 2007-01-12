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

/*$Id$ */

/* path related elements */

#include "memory_.h"
#include "gsmemory.h"
#include "gsmatrix.h"
#include "gserror.h"
#include "metcomplex.h"
#include "metelement.h"
#include "metgstate.h"
#include "metutil.h"
#include "gscoord.h"
#include "gspath.h"
#include "gspaint.h"
#include "math_.h"
#include "ctype_.h"
#include <stdlib.h> /* nb for atof */



/* this function is passed to the split routine (see below) */
private bool is_Data_delimeter(char b) 
{
    return (b == ',') || (isspace(b));
}

/* convert an xml point to a gs point.  nb lame */
private gs_point
getPoint(const char *metPoint)
{
    gs_point pt;
    /* gcc'ism */
    char pstr[strlen(metPoint) + 1];
    char *p = pstr;
    char *args[2];
    char ch;
    while ((ch = *metPoint)) {
        if (!isspace(ch))
            *p++ = ch;
        metPoint++;
    }
    *p = '\0'; /* null terminate p */

    met_split(pstr, args, is_Data_delimeter);
    pt.x = atof(args[0]);
    pt.y = atof(args[1]);
    return pt;
}

/* CT_PathFigure */
private int
PathFigure_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_PathFigure *aPathFigure = 
        (CT_PathFigure *)gs_alloc_bytes(ms->memory,
                                     sizeof(CT_PathFigure),
                                       "PathFigure_cook");
    int i;
    memset(aPathFigure, 0, sizeof(CT_PathFigure));
    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {
        if (!strcmp(attr[i], "IsClosed"))
            aPathFigure->isClosed = !strcmp(attr[i+1], "true");
        else if (!strcmp(attr[i], "isFilled"))
            aPathFigure->isFilled = !strcmp(attr[i+1], "true");
        else if (!MYSET(&aPathFigure->StartPoint, "StartPoint"))
            ;
        else {
            gs_warn2("unsupported attribute %s=%s",
                     attr[i], attr[i+1]);
        }
    }
    /* copy back the data for the parser. */
    *ppdata = aPathFigure;
    return 0;
}

/* action associated with this element */
private int
PathFigure_action(void *data, met_state_t *ms)
{
    CT_PathFigure *aPathFigure = data;
    int code;

    /* nb not sure what to make of PathFigure->isFilled.  Maybe a
       mistake in the spec. */

    /* tell the graphics state to close or not close future paths */
    met_setclosepath(ms->pgs, aPathFigure->isClosed);

    /* we do these now */
    if (aPathFigure->StartPoint) {
        gs_point start = getPoint(aPathFigure->StartPoint);
        gs_state *pgs = ms->pgs;
        code = gs_moveto(pgs, start.x, start.y);
        if (code < 0) {
            return code;
        }
    }
    return 0;
}

private int
PathFigure_done(void *data, met_state_t *ms)
{
    CT_PathFigure *aPathFigure = data;
    gs_free_object(ms->memory, aPathFigure->StartPoint, "PathFigure_done");
    gs_free_object(ms->memory, aPathFigure, "PathFigure_done");
    return 0; 
}

const met_element_t met_element_procs_PathFigure = {
    "PathFigure",
    {
        PathFigure_cook,
        PathFigure_action,
        PathFigure_done
    }
};

/* CT_Path */
private int
Path_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_Path *aPath = 
        (CT_Path *)gs_alloc_bytes(ms->memory,
                                     sizeof(CT_Path),
                                       "Path_cook");
    int i;
    /* hack for treeless demo */
    memset(aPath, 0, sizeof(CT_Path));
    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {
        if (!met_cmp_and_set(&aPath->Stroke,
                             attr[i], attr[i+1], "Stroke"))
            ;
        else if (!met_cmp_and_set(&aPath->Data,
                                  attr[i], attr[i+1], "Data")) 
            ;
        else if (!met_cmp_and_set(&aPath->Fill,
                                  attr[i], attr[i+1], "Fill"))
            ;
        else if (!strcmp(attr[i], "StrokeThickness"))
            aPath->StrokeThickness = atof(attr[i+1]);

        else {
            gs_warn2("unsupported attribute %s=%s",
                     attr[i], attr[i+1]);
        }
    }
    /* copy back the data for the parser. */
    *ppdata = aPath;
    return 0;
}

/* action associated with this element */
private int
Path_action(void *data, met_state_t *ms)
{
    CT_Path *aPath = data;
    int code = 0;

    met_setstrokecolor(ms->pgs, aPath->Stroke);
    met_setfillcolor(ms->pgs, aPath->Fill);
    code = gs_setlinewidth(ms->pgs, aPath->StrokeThickness);

    if (code < 0)
        gs_rethrow(code, "setlinewidth failed");

    if (aPath->Data) {
        gs_state *pgs = ms->pgs;
        /* gcc ism */
        char pathcopy[strlen(aPath->Data) + 1];
        /* the number of argument will be less than the length of the
           data.  NB wasteful. */
        char *args[strlen(aPath->Data)];
        char **pargs = args;
        bool moveto;
        strcpy(pathcopy, aPath->Data);
        met_split(pathcopy, args, is_Data_delimeter);
        /* nb implement the spec for Data - this just prints some test
           examples, sans error checking. */
        moveto = false;
        while (*pargs) {
            gs_point pt;
            char arg = toupper(**pargs);
            if (arg == 'Z') {
                gs_closepath(pgs);
                pargs++;
            } else if (arg == 'F') {
                /* nb set fill rule */
                pargs++;
            } else if (arg == 'M' || arg == 'L') {
                moveto = (arg == 'M');
                pargs++;
                while (*pargs) {
                    if (isalpha(**pargs)) {
                        break;
                    }
                    pt.x = atof(*pargs++);
                    pt.y = atof(*pargs++);
                    if (moveto)
                        code = gs_moveto(pgs, pt.x, pt.y);
                    else
                        code = gs_lineto(pgs, pt.x, pt.y);
                    if (code < 0)
                        return code;
                }
            } else if (arg == 'C') {
                gs_point cpt0, cpt1, cpt2;
                *pargs++;
                while (*pargs) {
                    if (isalpha(**pargs)) {
                        break;
                    }
                    cpt0.x = atof(*pargs++); cpt0.y = atof(*pargs++);
                    cpt1.x = atof(*pargs++); cpt1.y = atof(*pargs++); 
                    cpt2.x = atof(*pargs++); cpt2.y = atof(*pargs++);
                    code = gs_curveto(pgs, cpt0.x, cpt0.y, cpt1.x, cpt1.y,
                                      cpt2.x, cpt2.y);
                    if (code < 0)
                        return code;
                }
            } else {
                *pargs++;
                dprintf("unknown operator continuing\n");
            }
        }
    }
    return code;
}


/* element constructor */
private int
PathGeometry_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_PathGeometry *aPathGeometry = 
        (CT_PathGeometry *)gs_alloc_bytes(ms->memory,
                                     sizeof(CT_PathGeometry),
                                       "PathGeometry_cook");
    int i;

    memset(aPathGeometry, 0, sizeof(CT_PathGeometry));

    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {
        if (!met_cmp_and_set(&aPathGeometry->FillRule,
                             attr[i], attr[i+1], "FillRule"))
            ;
        else {
            gs_warn2("unsupported attribute %s=%s",
                     attr[i], attr[i+1]);
        }

    }
    /* copy back the data for the parser. */
    *ppdata = aPathGeometry;
    return 0;
}

/* action associated with this element */
private int
PathGeometry_action(void *data, met_state_t *ms)
{
    CT_PathGeometry *aPathGeometry = data;
    met_setfillrule(ms->pgs, aPathGeometry->FillRule);
    return 0;
}

private int
PathGeometry_done(void *data, met_state_t *ms)
{
    CT_PathGeometry *aPathGeometry = data;  
      
    gs_free_object(ms->memory, aPathGeometry->FillRule, "PathGeometry_done");
    gs_free_object(ms->memory, aPathGeometry, "PathGeometry_done");
    return 0;
}

const met_element_t met_element_procs_PathGeometry = {
    "PathGeometry",
    {
        PathGeometry_cook,
        PathGeometry_action,
        PathGeometry_done
    }
};

private int
set_color(gs_state *pgs, bool forstroke)
{
    int code = 0;
    ST_RscRefColor (*color)(gs_state *);
    ST_RscRefColor met_color;

    if (forstroke)
        color = met_currentstrokecolor;
    else
        color = met_currentfillcolor;


    met_color = ((*color)(pgs));
    
    /* if this is a pattern nothing to do */
    if (met_iscolorpattern(pgs, met_color))
        return 0;

    if (met_color) {
        rgb_t rgb = met_hex2rgb(met_color);
        /* nb rgb color */
        code = gs_setrgbcolor(pgs, (floatp)rgb.r,
                              (floatp)rgb.g, (floatp)rgb.b);
    } else {
        code = gs_setnullcolor(pgs);
    }
    return code;
}

private int
Path_done(void *data, met_state_t *ms)
{
    gs_state *pgs = ms->pgs;
    int code;
    CT_Path *aPath = (CT_Path *)data;
    met_path_t pathtype = met_currentpathtype(pgs);
    /* this loop executes once */
    int (*fill)(gs_state *);

    if (met_currenteofill(pgs))
        fill = gs_eofill;
    else
        fill = gs_fill;

    do {
        /* case of stroke and file... uses a gsave/grestore to keep
           the path */
        if (pathtype == met_stroke_and_fill) {
            /* set the color before gsave to be consistent with the
               solo stroke and fill cases below we only use the gsave
               to preserve the path. */
            if ((code = set_color(pgs, false /* is stroke */)) < 0)
                break;
            if ((code = gs_gsave(pgs)) < 0)
                break;
            if ((code = gs_eofill(pgs)) < 0)
                break;
            /* nb should try to clean up gsave on error in set_color
               or gs_eofill... */
            if ((code = gs_grestore(pgs)) < 0)
                break;
            if ((code = set_color(pgs, true /* is stroke */)) < 0)
                break;
            if ((code = gs_stroke(pgs)) < 0)
                break;
        /* just fill or stroke not both */
        } else if (pathtype == met_fill_only) {
            if ((code = set_color(pgs, false /* is stroke */)) < 0)
                break;
            if ((code = gs_eofill(pgs)) < 0)
                break;
        } else if (pathtype == met_stroke_only) {
            if ((code = set_color(pgs, true /* is stroke */)) < 0)
                break;
            if ((code = gs_stroke(pgs)) < 0)
                break;
        } else {
            // gs_throw(-1, "Unknown path operation");
            // code = -1;
	    gs_warn("unknown path operation");
	    code = 0;
        }
            

    } while (0);

    gs_free_object(ms->memory, aPath->Stroke, "Path_done");
    gs_free_object(ms->memory, aPath->Data, "Path_done");
    gs_free_object(ms->memory, aPath->Fill, "Path_done");
    gs_free_object(ms->memory, aPath, "Path_done");
    if (code < 0)
        return gs_rethrow(code, "Path done failed");
    else
        return 0;
}

const met_element_t met_element_procs_Path = {
    "Path",
    {
        Path_cook,
        Path_action,
        Path_done
    }
};

/* CT_Path */

/* PolyLineSegment */
private int
PolyLineSegment_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_PolyLineSegment *aPolyLineSegment = 
        (CT_PolyLineSegment *)gs_alloc_bytes(ms->memory,
                                            sizeof(CT_PolyLineSegment),
                                            "PolyLineSegment_cook");

    int i;
    memset(aPolyLineSegment, 0, sizeof(CT_PolyLineSegment));

    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {
        if (!MYSET(&aPolyLineSegment->Points, "Points"))
            ;
        else {
            gs_warn2("unsupported attribute %s=%s",
                     attr[i], attr[i+1]);
        }
    }

    /* copy back the data for the parser. */
    *ppdata = aPolyLineSegment;
    return 0;
}

/* action associated with this element */
private int
PolyLineSegment_action(void *data, met_state_t *ms)
{
    CT_PolyLineSegment *aPolyLineSegment = data;
    gs_state *pgs = ms->pgs;
    int code = 0;
    if (aPolyLineSegment->Points) {
        char pstr[strlen(aPolyLineSegment->Points) + 1];
        /* the number of argument will be less than the length of the
           data.  NB wasteful. */
        char *args[strlen(aPolyLineSegment->Points)];
        char **pargs = args;
        strcpy(pstr, aPolyLineSegment->Points);
        met_split(pstr, args, is_Data_delimeter);
        while (*pargs) {
            gs_point pt;
            pt.x = atof(*pargs++);

            if (!*pargs) {
                dprintf("point does not have two coordinate\n");
                break;
            }

            pt.y = atof(*pargs++);
            code = gs_lineto(pgs, pt.x, pt.y);
            if (code < 0)
                return code;
        }
    }
    if (met_currentclosepath(pgs))
        code = gs_closepath(ms->pgs);
    return code;
}

private int
PolyLineSegment_done(void *data, met_state_t *ms)
{
    CT_PolyLineSegment *aPolyLineSegment = data;

    gs_free_object(ms->memory, aPolyLineSegment->Points, "PolyLineSegment_done");
    gs_free_object(ms->memory, aPolyLineSegment, "PolyLineSegment_done");
    return 0;
}

const met_element_t met_element_procs_PolyLineSegment = {
    "PolyLineSegment",
    {
        PolyLineSegment_cook,
        PolyLineSegment_action,
        PolyLineSegment_done
    }
};

/* element constructor */
private int
Path_Fill_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_CP_Brush *aPath_Fill = 
        (CT_CP_Brush *)gs_alloc_bytes(ms->memory,
                                      sizeof(CT_CP_Brush),
                                      "Path_Fill_cook");
    int i;

    memset(aPath_Fill, 0, sizeof(CT_CP_Brush));

    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {

    }

    /* copy back the data for the parser. */
    *ppdata = aPath_Fill;
    return 0;
}

/* action associated with this element */
private int
Path_Fill_action(void *data, met_state_t *ms)
{
    /* CT_CP_Brush *aPath_Fill = data; */
    met_setpathchild(ms->pgs, met_fill);
    return 0;
}

private int
Path_Fill_done(void *data, met_state_t *ms)
{
    gs_free_object(ms->memory, data, "Path_Fill_done");
    return 0; 
}

const met_element_t met_element_procs_Path_Fill = {
    "Path_Fill",
    {
        Path_Fill_cook,
        Path_Fill_action,
        Path_Fill_done
    }
};

/* element constructor */
private int
SolidColorBrush_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_SolidColorBrush *aSolidColorBrush = 
        (CT_SolidColorBrush *)gs_alloc_bytes(ms->memory,
                                     sizeof(CT_SolidColorBrush),
                                       "SolidColorBrush_cook");

    int i;

    memset(aSolidColorBrush, 0, sizeof(CT_SolidColorBrush));

    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {
        if (!MYSET(&aSolidColorBrush->Color, "Color"))
            ;
        else {
            gs_warn2("unsupported attribute %s=%s",
                     attr[i], attr[i+1]);
        }
    }
    /* copy back the data for the parser. */
    *ppdata = aSolidColorBrush;
    return 0;
}

/* action associated with this element */
private int
SolidColorBrush_action(void *data, met_state_t *ms)
{
    CT_SolidColorBrush *aSolidColorBrush = data;
    met_setfillcolor(ms->pgs, aSolidColorBrush->Color);
    return 0;
}

private int
SolidColorBrush_done(void *data, met_state_t *ms)
{
    CT_SolidColorBrush *aSolidColorBrush = data;

    gs_free_object(ms->memory, aSolidColorBrush->Color, "SolidColorBrush_done");
    gs_free_object(ms->memory, aSolidColorBrush, "SolidColorBrush_done");
    return 0;
}

const met_element_t met_element_procs_SolidColorBrush = {
    "SolidColorBrush",
    {
        SolidColorBrush_cook,
        SolidColorBrush_action,
        SolidColorBrush_done
    }
};

private int
Path_Stroke_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_CP_Brush *aPath_Stroke = 
        (CT_CP_Brush *)gs_alloc_bytes(ms->memory,
                                      sizeof(CT_CP_Brush),
                                      "Path_Stroke_cook");

    int i;

    memset(aPath_Stroke, 0, sizeof(CT_CP_Brush));

    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {

    }

    /* copy back the data for the parser. */
    *ppdata = aPath_Stroke;
    return 0;
}

/* action associated with this element */
private int
Path_Stroke_action(void *data, met_state_t *ms)
{
    /* CT_CP_Brush *aPath_Stroke = data; */
    met_setpathchild(ms->pgs, met_stroke);
    return 0;

}

private int
Path_Stroke_done(void *data, met_state_t *ms)
{
        
    gs_free_object(ms->memory, data, "Path_Stroke_done");
    return 0; /* incomplete */
}


const met_element_t met_element_procs_Path_Stroke = {
    "Path_Stroke",
    {
        Path_Stroke_cook,
        Path_Stroke_action,
        Path_Stroke_done
    }
};

/* element constructor */
private int
ArcSegment_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_ArcSegment *aArcSegment = 
        (CT_ArcSegment *)gs_alloc_bytes(ms->memory,
                                     sizeof(CT_ArcSegment),
                                       "ArcSegment_cook");
    int i;

    memset(aArcSegment, 0, sizeof(CT_ArcSegment));

    /* set defaults */
    aArcSegment->IsStroked = true;
      
    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {
        if (!MYSET(&aArcSegment->Point, "Point"))
            ;
        else if (!MYSET(&aArcSegment->Size, "Size"))
            ;
        else if (!MYSET(&aArcSegment->SweepDirection, "SweepDirection"))
            ;
        else if (!strcmp(attr[i], "RotationAngle"))
            aArcSegment->RotationAngle = atof(attr[i+1]);
        else if (!strcmp(attr[i], "IsLargeArc"))
            aArcSegment->IsLargeArc = !strcmp(attr[i+1], "true");
        else if (!strcmp(attr[i], "IsStroked"))
            aArcSegment->IsStroked = !strcmp(attr[i+1], "true");
        else {
            gs_warn2("unsupported attribute %s=%s",
                     attr[i], attr[i+1]);
        }
    }

    /* copy back the data for the parser. */
    *ppdata = aArcSegment;
    return 0;
}

/* given two vectors find the angle between */
private double
angle_between(const gs_point u, const gs_point v)
{
    double det = u.x * v.y - u.y * v.x;
    double sign = (det < 0 ? -1.0 : 1.0);
    double magu = u.x * u.x + u.y * u.y;
    double magv = v.x * v.x + v.y * v.y;
    double udotv = u.x * v.x + u.y * v.y;
    return sign * acos(udotv / (magu * magv));
}

/* ArcSegment pretty much follows the SVG algorithm for converting an
   arc in endpoint representation to an arc in centerpoint
   representation.  Once in centerpoint it can be given to the
   graphics library in the form of a postscript arc. */
private int
ArcSegment_action(void *data, met_state_t *ms)
{
    CT_ArcSegment *aArcSegment = data;
    gs_state *pgs = ms->pgs;
    gs_point start, end, size, midpoint, halfdis, thalfdis, tcenter, center;
    double sign, rot, start_angle, delta_angle;
    int code;
    bool isLargeArc, isClockwise;

    /* should be available */
    if ((code = gs_currentpoint(pgs, &start)) < 0)
        return gs_rethrow(code, "missing start point");
    
    /* get the end point */
    end = getPoint(aArcSegment->Point);

    /* get the radii - M$ calls these the "size" */
    size = getPoint(aArcSegment->Size);
    
    /* set the large arc flag */
    isLargeArc = aArcSegment->IsLargeArc;

    /* set cw flag, nb probably should ignore case */
    if (!strcmp(aArcSegment->SweepDirection, "Counterclockwise"))
        isClockwise = false;
    else
        isClockwise = true;
    sign = (isClockwise == isLargeArc ? -1.0 : 1.0);
    
    /* get the rotation angle. */
    rot = aArcSegment->RotationAngle;

    /* get the distance of the radical line */
    halfdis.x = (start.x - end.x) / 2.0;
    halfdis.y = (start.y - end.y) / 2.0;
    
#define SQR(x) ((x)*(x))

    /* rotate the halfdis vector so x axis parallel ellipse x axis */
    {
        gs_matrix rotmat;
        double correction;
        gs_make_rotation(-rot, &rotmat);
        if ((code = gs_distance_transform(halfdis.x, halfdis.y, &rotmat, &thalfdis)) < 0)
            gs_rethrow(code, "transform failed");
        /* correct radii if necessary */
        correction = (SQR(thalfdis.x) / SQR(size.x)) + (SQR(thalfdis.y) / SQR(size.y));
        if (correction > 1.0) {
            size.x *= sqrt(correction);
            size.y *= sqrt(correction);
        }
    }

#define MULTOFSQUARES(x, y) (SQR((x)) * SQR((y)))
    {
        
        double scale_num = (MULTOFSQUARES(size.x, size.y)) -
            (MULTOFSQUARES(size.x, thalfdis.y)) -
            (MULTOFSQUARES(size.y, thalfdis.x));
        double scale_denom = MULTOFSQUARES(size.x, thalfdis.y) + MULTOFSQUARES(size.y, thalfdis.x);
        double scale = sign * sqrt(((scale_num / scale_denom) < 0) ? 0 : (scale_num / scale_denom));
        /* translated center */
        tcenter.x = scale * ((size.x * thalfdis.y)/size.y);
        tcenter.y = scale * ((-size.y * thalfdis.x)/size.x);

        /* of the original radical line */
        midpoint.x = (end.x + start.x) / 2.0;
        midpoint.y = (end.y + start.y) / 2.0;
        
        center.x = tcenter.x + midpoint.x;
        center.y = tcenter.y + midpoint.y;
        {
            gs_point coord1, coord2, coord3, coord4;
            coord1.x = 1;
            coord1.y = 0;
            coord2.x = (thalfdis.x - tcenter.x) / size.x; 
            coord2.y = (thalfdis.y - tcenter.y) / size.y;
            coord3.x = (thalfdis.x - tcenter.x) / size.x;
            coord3.y = (thalfdis.y - tcenter.y) / size.y;
            coord4.x = (-thalfdis.x - tcenter.x) / size.x;
            coord4.y = (-thalfdis.y - tcenter.y) / size.y;
            start_angle = angle_between(coord1, coord2);
            delta_angle = angle_between(coord3, coord4);
            if (delta_angle < 0 && !isClockwise)
                delta_angle += (degrees_to_radians * 360);
            if (delta_angle > 0 && isClockwise)
                delta_angle -= (degrees_to_radians * 360);
                
        }
    }

    {
        gs_matrix save_ctm;
        gs_currentmatrix(pgs, &save_ctm);
        gs_translate(pgs, center.x, center.y);
        gs_rotate(pgs, rot);
        gs_scale(pgs, size.x, size.y);
    
        if ((code = gs_arcn(pgs, 0, 0, 1,
                            radians_to_degrees * start_angle,
                            radians_to_degrees * (start_angle + delta_angle))) < 0)
            return gs_rethrow(code, "arc failed");
        

        /* restore the ctm */
        gs_setmatrix(pgs, &save_ctm);
    }
    return 0;
}

private int
ArcSegment_done(void *data, met_state_t *ms)
{
    CT_ArcSegment *aArcSegment = (CT_ArcSegment *)data;

    gs_free_object(ms->memory, aArcSegment->Point, "ArcSegment_done");
    gs_free_object(ms->memory, aArcSegment->Size, "ArcSegment_done");
    gs_free_object(ms->memory, aArcSegment->SweepDirection, "ArcSegment_done");
    gs_free_object(ms->memory, aArcSegment, "ArcSegment_done");
    return 0; 
}


const met_element_t met_element_procs_ArcSegment = {
    "ArcSegment",
    {
        ArcSegment_cook,
        ArcSegment_action,
        ArcSegment_done
    }
};


/* element constructor */
private int
PolyBezierSegment_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_PolyBezierSegment *aPolyBezierSegment = 
        (CT_PolyBezierSegment *)gs_alloc_bytes(ms->memory,
                                     sizeof(CT_PolyBezierSegment),
                                       "PolyBezierSegment_cook");
    int i;

    memset(aPolyBezierSegment, 0, sizeof(CT_PolyBezierSegment));

    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {
        if (!MYSET(&aPolyBezierSegment->Points, "Points"))
            ;
        else {
            gs_warn2("unsupported attribute %s=%s",
                     attr[i], attr[i+1]);
        }
    }
    /* copy back the data for the parser. */
    *ppdata = aPolyBezierSegment;
    return 0;
}

/* action associated with this element.  NB very similar to other
   LineSegment routines refactor. */
private int
PolyBezierSegment_action(void *data, met_state_t *ms)
{
    CT_PolyBezierSegment *aPolyBezierSegment = data;
    gs_state *pgs = ms->pgs;
    int code = 0;
    if (aPolyBezierSegment->Points) {
        char pstr[strlen(aPolyBezierSegment->Points) + 1];
        /* the number of argument will be less than the length of the
           data.  NB wasteful. */
        char *args[strlen(aPolyBezierSegment->Points)];
        char **pargs = args;
        int points_parsed = 0;
        strcpy(pstr, aPolyBezierSegment->Points);
        met_split(pstr, args, is_Data_delimeter);
        while (*pargs) {
            gs_point pt[3]; /* the two control point and the end point */

            pt[points_parsed % 3].x = atof(*pargs++);

            if (!*pargs) {
                dprintf("point does not have two coordinate\n");
                break;
            }
            pt[points_parsed % 3].y = atof(*pargs++);
            points_parsed++;
            if (points_parsed % 3 == 0) {
                code = gs_curveto(pgs, pt[0].x, pt[0].y,
                                  pt[1].x, pt[1].y,
                                  pt[2].x, pt[2].y);
                if (code < 0)
                    return code;
            }
        }
    }
    if (met_currentclosepath(pgs))
        code = gs_closepath(ms->pgs);
    return code;
}

private int
PolyBezierSegment_done(void *data, met_state_t *ms)
{
    CT_PolyBezierSegment *aPolyBezierSegment = data;        

    gs_free_object(ms->memory, aPolyBezierSegment->Points, "PolyBezierSegment_done");
    gs_free_object(ms->memory, aPolyBezierSegment, "PolyBezierSegment_done");
    return 0;
}


const met_element_t met_element_procs_PolyBezierSegment = {
    "PolyBezierSegment",
    {
        PolyBezierSegment_cook,
        PolyBezierSegment_action,
        PolyBezierSegment_done
    }
};


/* element constructor */
private int
PolyQuadraticBezierSegment_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_PolyQuadraticBezierSegment *aPolyQuadraticBezierSegment = 
        (CT_PolyQuadraticBezierSegment *)gs_alloc_bytes(ms->memory,
                                     sizeof(CT_PolyQuadraticBezierSegment),
                                       "PolyQuadraticBezierSegment_cook");
    int i;

    memset(aPolyQuadraticBezierSegment, 0, sizeof(CT_PolyQuadraticBezierSegment));

    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {
        if (!MYSET(&aPolyQuadraticBezierSegment->Points, "Points"))
            ;
        else {
            gs_warn2("unsupported attribute %s=%s",
                     attr[i], attr[i+1]);
        }
    }

    /* copy back the data for the parser. */
    *ppdata = aPolyQuadraticBezierSegment;
    return 0;
}


/* NB code duplication with Segments ahead */

/* action associated with this element */
private int
PolyQuadraticBezierSegment_action(void *data, met_state_t *ms)
{
    CT_PolyQuadraticBezierSegment *aPolyQuadraticBezierSegment = data;
    gs_state *pgs = ms->pgs;
    int code = 0;
    if (aPolyQuadraticBezierSegment->Points) {
        char pstr[strlen(aPolyQuadraticBezierSegment->Points) + 1];
        /* the number of argument will be less than the length of the
           data.  NB wasteful. */
        char *args[strlen(aPolyQuadraticBezierSegment->Points)];
        char **pargs = args;
        int points_parsed = 0;
        strcpy(pstr, aPolyQuadraticBezierSegment->Points);
        met_split(pstr, args, is_Data_delimeter);
        while (*pargs) {
            gs_point pt[2]; /* control point and end point */

            pt[points_parsed % 2].x = atof(*pargs++);

            if (!*pargs) {
                dprintf("point does not have two coordinate\n");
                break;
            }
            pt[points_parsed % 2].y = atof(*pargs++);
            points_parsed++;
            if (points_parsed % 2 == 0) {
                gs_point start_pt;
                code = gs_currentpoint(pgs, &start_pt);
                if (code < 0)
                    gs_rethrow(code, "currentpoint failed");
                code = gs_curveto(pgs,
                                  (start_pt.x + 2 * pt[0].x) / 3,
                                  (start_pt.x + 2 * pt[0].y) / 3,
                                  (pt[1].x + 2 * pt[0].x) / 3,
                                  (pt[1].y + 2 * pt[0].y) / 3,
                                  pt[1].x, pt[1].y);
                if (code < 0)
                    return gs_rethrow(code, "curveto failied");
            }
        }
    }
    if (met_currentclosepath(pgs))
        code = gs_closepath(ms->pgs);
    return code;
}

private int
PolyQuadraticBezierSegment_done(void *data, met_state_t *ms)
{
    CT_PolyQuadraticBezierSegment *aPolyQuadraticBezierSegment = data;  
      
    gs_free_object(ms->memory, aPolyQuadraticBezierSegment->Points, 
		   "PolyQuadraticBezierSegment_done");
    gs_free_object(ms->memory, aPolyQuadraticBezierSegment, "PolyQuadraticBezierSegment_done");
    return 0;
}


const met_element_t met_element_procs_PolyQuadraticBezierSegment = {
    "PolyQuadraticBezierSegment",
    {
        PolyQuadraticBezierSegment_cook,
        PolyQuadraticBezierSegment_action,
        PolyQuadraticBezierSegment_done
    }
};
