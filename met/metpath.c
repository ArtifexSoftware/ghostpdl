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

/* path related elements */

#include "memory_.h"
#include "gsmemory.h"
#include "gsmatrix.h"
#include "metcomplex.h"
#include "metelement.h"
#include "metutil.h"
#include "gspath.h"
#include "math_.h"
#include "ctype_.h"
#include <stdlib.h> /* nb for atof */

/* nb use the library */
typedef struct rgb_s {
    int r;
    int g;
    int b;
} rgb_t;

/* temporary hacks for the treeless demo */
typedef struct pathfigure_state_s {
    bool fill;
    bool stroke;
    bool closed;
    bool fill_color_set;
    bool stroke_color_set;
    rgb_t fill_color;
    rgb_t stroke_color;
} pathfigure_state_t;

private pathfigure_state_t nb_pathstate;

bool patternset = false;

/* I am sure there is a better (more robust) meme for this... */
private rgb_t
hex2rgb(char *hexstring)
{
    char *hextable = "0123456789ABCDEF";
    rgb_t rgb;
#define HEXTABLEINDEX(chr) (strchr(hextable, (chr)) - hextable)
    rgb.r = HEXTABLEINDEX(hexstring[1]) * 16 +
        HEXTABLEINDEX(hexstring[2]);
    rgb.g = HEXTABLEINDEX(hexstring[3]) * 16 +
        HEXTABLEINDEX(hexstring[4]);
    rgb.b = HEXTABLEINDEX(hexstring[5]) * 16 +
        HEXTABLEINDEX(hexstring[6]);
    return rgb;
}

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
    while (ch = *metPoint++) {
        if (!isspace(ch))
            *p++ = ch;
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
        if (!strcmp(attr[i], "IsClosed")) {
            aPathFigure->isClosed = !strcmp(attr[i+1], "true");
        } else if (!strcmp(attr[i], "StartPoint")) {
            aPathFigure->StartPoint = attr[i+1];
        } else if (!strcmp(attr[i], "isFilled")) {
            aPathFigure->isFilled  = attr[i+1];
        } else {
            dprintf2(ms->memory, "unsupported attribute %s=%s\n",
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

    /* nb not sure about states default, filled, stroke */
    if (aPathFigure->isFilled) {
        nb_pathstate.fill = 1;
    } else {
        nb_pathstate.stroke = 1;
    }

    if (aPathFigure->isClosed) {
        nb_pathstate.closed = 1;
    }

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
    gs_free_object(ms->memory, data, "PathFigure_done");
    return 0; /* incomplete */
}

const met_element_t met_element_procs_PathFigure = {
    "PathFigure",
    PathFigure_cook,
    PathFigure_action,
    PathFigure_done
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
    memset(&nb_pathstate, 0, sizeof(nb_pathstate));
    memset(aPath, 0, sizeof(CT_Path));
    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {
        if (!met_cmp_and_set(&aPath->Stroke,
                             attr[i], attr[i+1], "Stroke"))
            ;
        else if (!met_cmp_and_set(&aPath->StrokeThickness,
                                  attr[i], attr[i+1], "StrokeThickness")) 
            ;
        else if (!met_cmp_and_set(&aPath->Data,
                                  attr[i], attr[i+1], "Data")) 
            ;
        else if (!met_cmp_and_set(&aPath->Fill,
                                  attr[i], attr[i+1], "Fill"))
            ;
        
        else {
            dprintf2(ms->memory, "unsupported attribute %s=%s\n",
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
    gs_memory_t *mem = ms->memory;

    if (aPath->Stroke) {
        rgb_t rgb = hex2rgb(aPath->Stroke);
        nb_pathstate.stroke_color_set = true;
        nb_pathstate.stroke_color = rgb;
        nb_pathstate.stroke = 1;
    }
    
    /* NB code dup */
    if (aPath->Fill) {
        rgb_t rgb = hex2rgb(aPath->Fill);
        nb_pathstate.fill_color_set = true;
        nb_pathstate.fill_color = rgb;
        nb_pathstate.fill = 1;
    }

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
        while (**pargs != 'Z' && **pargs != 'z') {
            gs_point pt;
            if ((**pargs == 'M') || (**pargs == 'L')) {
                moveto = (**pargs == 'M');
                pargs++;
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
    }
    return code;
}

private int
set_color(gs_state *pgs, bool forstroke)
{
    int code = 0;
    if (patternset)
        return 0;

    if (forstroke) {
        if (nb_pathstate.stroke_color_set) {
            rgb_t rgb = nb_pathstate.stroke_color;
            code = gs_setrgbcolor(pgs, (floatp)rgb.r,
                                  (floatp)rgb.g, (floatp)rgb.b);
        } else {
            code = gs_setnullcolor(pgs);
        }
    } else {
        if (nb_pathstate.fill_color_set) {
            rgb_t rgb = nb_pathstate.fill_color;
            code = gs_setrgbcolor(pgs, (floatp)rgb.r,
                                  (floatp)rgb.g, (floatp)rgb.b);
        } else {
            code = gs_setnullcolor(pgs);
        }
    }
    return code;
}

private int
Path_done(void *data, met_state_t *ms)
{
    gs_state *pgs = ms->pgs;
    int code;
    int times = 1;

    /* this loop executes once and is only needed for jump of the
       break.  Somebody told me not to use goto's */
    while (times--) {
        /* case of stroke and file... uses a gsave/grestore to keep
           the path */
        if (nb_pathstate.fill && nb_pathstate.stroke) {
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
        } else if (nb_pathstate.fill) {
            if ((code = set_color(pgs, false /* is stroke */)) < 0)
                break;
            if ((code = gs_eofill(pgs)) < 0)
                break;
        } else if (nb_pathstate.stroke) {
            if ((code = set_color(pgs, true /* is stroke */)) < 0)
                break;
            if ((code = gs_stroke(pgs)) < 0)
                break;
        }
    }
    /* hack nb fixme */
    patternset = false;
    gs_free_object(ms->memory, data, "Path_done");
    return code;
}

const met_element_t met_element_procs_Path = {
    "Path",
    Path_cook,
    Path_action,
    Path_done
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
        if (!strcmp(attr[i], "Points")) {
            aPolyLineSegment->Points = attr[i + 1];
        } else {
            dprintf2(ms->memory, "unsupported attribute %s=%s\n",
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
    gs_memory_t *mem = ms->memory;
    int code = 0;
    if (aPolyLineSegment->Points) {
        char *s;
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
                dprintf(mem, "point does not have two coordinate\n");
                break;
            }

            pt.y = atof(*pargs++);
            code = gs_lineto(pgs, pt.x, pt.y);
            if (code < 0)
                return code;
        }
    }
    if (nb_pathstate.closed)
        code = gs_closepath(ms->pgs);
    return code;
}

private int
PolyLineSegment_done(void *data, met_state_t *ms)
{
    gs_free_object(ms->memory, data, "PolyLineSegment_done");
    return 0; /* incomplete */
}

const met_element_t met_element_procs_PolyLineSegment = {
    "PolyLineSegment",
    PolyLineSegment_cook,
    PolyLineSegment_action,
    PolyLineSegment_done
};

/* nb Path_Fill needs data modeling work - it looks like this is an
   extraneous level of state.  For now this does nothing
   interesting */

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
    CT_CP_Brush *aPath_Fill = data;
    nb_pathstate.fill = 1;
    return 0;
}

private int
Path_Fill_done(void *data, met_state_t *ms)
{
    gs_free_object(ms->memory, data, "Path_Fill_done");
    return 0; /* incomplete */
}

const met_element_t met_element_procs_Path_Fill = {
    "Path_Fill",
    Path_Fill_cook,
    Path_Fill_action,
    Path_Fill_done
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
        if (!strcmp(attr[i], "Color")) {
            aSolidColorBrush->Color = attr[i+1];
        } else {
            dprintf2(ms->memory, "unsupported attribute %s=%s\n",
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
    if (aSolidColorBrush->Color) {
        rgb_t rgb = hex2rgb(aSolidColorBrush->Color);
        nb_pathstate.fill_color_set = true;
        nb_pathstate.fill_color = rgb;
    }
    return 0;
}

private int
SolidColorBrush_done(void *data, met_state_t *ms)
{
    gs_free_object(ms->memory, data, "SolidColorBrush_done");
    return 0; /* incomplete */
}

const met_element_t met_element_procs_SolidColorBrush = {
    "SolidColorBrush",
    SolidColorBrush_cook,
    SolidColorBrush_action,
    SolidColorBrush_done
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
    CT_CP_Brush *aPath_Stroke = data;
    nb_pathstate.stroke = 1;
    return 0;

}

private int
Path_Stroke_done(void *data, met_state_t *ms)
{
        
    gs_free_object(ms->memory, data, "ELEMENT_done");
    return 0; /* incomplete */
}


const met_element_t met_element_procs_Path_Stroke = {
    "Path_Stroke",
    Path_Stroke_cook,
    Path_Stroke_action,
    Path_Stroke_done
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

#define MYSET(field, value)                                                   \
    met_cmp_and_set((field), attr[i], attr[i+1], (value))

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
        else if (!strcmp(attr[i], "RotationAngle")) {
            aArcSegment->RotationAngle = atof(attr[i+1]);
            aArcSegment->avail.RotationAngle = 1;
        } else if (!strcmp(attr[i], "IsLargeArc")) {
            aArcSegment->IsLargeArc = !strcmp(attr[i+1], "true");
            aArcSegment->avail.IsLargeArc = 1;
        }
        else if (!strcmp(attr[i], "IsStroked")) {
            aArcSegment->IsStroked = !strcmp(attr[i+1], "true");
            aArcSegment->avail.IsStroked = 1;
        } else {
            dprintf2(ms->memory, "unsupported attribute %s=%s\n",
                     attr[i], attr[i+1]);
        }
    }

    /* copy back the data for the parser. */
    *ppdata = aArcSegment;
    return 0;
}

/* action associated with this element.  NB a work in progress */
private int
ArcSegment_action(void *data, met_state_t *ms)
{
    /*                        ArcSegment
                        Size='100,50'
                        RotationAngle='45'
                        IsLargeArc='true'
                        SweepDirection='Counterclockwise'
                        Point='200,100'
                        ArcSegment has no implementation
                        /ArcSegment
    */
    
    CT_ArcSegment *aArcSegment = data;
    gs_state *pgs = ms->pgs;
    gs_memory_t *mem = ms->memory;
    gs_point start, sstart, rstart, end, send, rend, size, midpoint;
    double len;
    double cosang, ang, slope, scalex;
    gs_matrix rot_mat, save_ctm;
    int code;

    /* should be available */
    if ((code = gs_currentpoint(pgs, &start)) < 0)
        return code;
    
    /* get the end point */
    end = getPoint(aArcSegment->Point);

    /* get the radii - M$ calls these the "size" */
    size = getPoint(aArcSegment->Size);
    
    /* create a scaling factor for x based on the ratio of the y
       radius to the x radius */
    scalex = size.y / size.x;
    
    /* scale each x so we get two circles of radius y instead of two
       ellipses */
    sstart.x = start.x * scalex; sstart.y = start.y;
    send.x = end.x * scalex; send.y = end.y;

    /* find the midpoint of these two points then rotate about that
       point 90 degrees the end points of these lines should be the
       centerpoints of the two circles (after a little scaling see
       below).  NB need error checking */ 
    midpoint.x = (send.x + sstart.x) / 2.0;
    midpoint.y = (send.y + sstart.y) / 2.0;
    
    gs_make_identity(&rot_mat);
    gs_matrix_translate(mem, &rot_mat, midpoint.x, midpoint.y, &rot_mat);

    /* it turns out the length of the seqment between centerpoints of
       the circles is 2 * (r^2 - (1/2 d)^2)^1/2 where r is size.y and
       d is the euclidean distance between the points of intersection.
       Simplifying we get (4r^2 - d^2)^1/2 */
    {
        double dis = hypot(send.x - sstart.x, send.y - sstart.y);
        double dis2 = sqrt((4 * size.y * size.y) - (dis * dis));
        double sc = dis/dis2;
        gs_matrix_scale(&rot_mat, sc, sc, &rot_mat);
        gs_matrix_rotate(&rot_mat, 90, &rot_mat);
        gs_point_transform(mem, send.x, send.y, &rot_mat, &rend);
        gs_point_transform(mem, sstart.x, sstart.y, &rot_mat, &rstart);
    }

    /*    slope = (end.y - start.y) / (end.y - start.y); */

    /* the cos(x) = (start "dot" end) / |start||end| */
    /*    cosang =          (start.x * end.x + start.y + end.y) / */
              /* -------------------------------------------------- */
    /*                 (hypot(start.x, start.y) * hypot(end.x, end.y)); */
    /* the angle between the points is the arccosine */
    /*    ang = acos(cosang); */

    /* the radius - nb not finished. */
    /*    size = getPoint(aArcSegment->Size); */
    
    gs_currentmatrix(pgs, &save_ctm);

    gs_translate(pgs, rstart.x, rstart.y);
    gs_scale(pgs, size.x, size.y);

    if ((code = gs_arc(pgs, 0, 0, 1.0, 0, 360.0)) < 0)
        return code;

    /* restore the ctm */
    gs_setmatrix(pgs, &save_ctm);
    gs_translate(pgs, rend.x, rend.y);
    gs_scale(pgs, size.x, size.y);

    if ((code = gs_arc(pgs, 0, 0, 1.0, 0, 360.0)) < 0)
        return code;

    /* restore the ctm */
    gs_setmatrix(pgs, &save_ctm);

    if (aArcSegment->avail.IsStroked) {
        if (aArcSegment->IsStroked == false)
            nb_pathstate.stroke = 0;
        else
            nb_pathstate.stroke = 1;
    }
    return 0;
}

private int
ArcSegment_done(void *data, met_state_t *ms)
{
        
    gs_free_object(ms->memory, data, "ArcSegment_done");
    return 0; /* incomplete */
}


const met_element_t met_element_procs_ArcSegment = {
    "ArcSegment",
    ArcSegment_cook,
    ArcSegment_action,
    ArcSegment_done
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
        if (!strcmp(attr[i], "Points")) {
            aPolyBezierSegment->Points = attr[i + 1];
        } else {
            dprintf2(ms->memory, "unsupported attribute %s=%s\n",
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
    gs_memory_t *mem = ms->memory;
    int code = 0;
    if (aPolyBezierSegment->Points) {
        char *s;
        char pstr[strlen(aPolyBezierSegment->Points) + 1];
        /* the number of argument will be less than the length of the
           data.  NB wasteful. */
        char *args[strlen(aPolyBezierSegment->Points)];
        char **pargs = args;
        int points_parsed = 0;
        strcpy(pstr, aPolyBezierSegment->Points);
        met_split(pstr, args, is_Data_delimeter);
        while (*pargs) {
            gs_point pt[3]; /* the three control points */

            pt[points_parsed % 3].x = atof(*pargs++);

            if (!*pargs) {
                dprintf(mem, "point does not have two coordinate\n");
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
    if (nb_pathstate.closed)
        code = gs_closepath(ms->pgs);
    return code;
}

private int
PolyBezierSegment_done(void *data, met_state_t *ms)
{
        
    gs_free_object(ms->memory, data, "PolyBezierSegment_done");
    return 0; /* incomplete */
}


const met_element_t met_element_procs_PolyBezierSegment = {
    "PolyBezierSegment",
    PolyBezierSegment_cook,
    PolyBezierSegment_action,
    PolyBezierSegment_done
};
