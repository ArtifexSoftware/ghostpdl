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
/* $Id$*/

/* metcomplex.h */

/* metro complex types */

#ifndef metcomplex_INCLUDED
#   define metcomplex_INCLUDED

#include "metsimple.h"

typedef struct CT_ArcSegment_s {
    ST_Point Point;
    ST_Point Size;
    ST_Double RotationAngle;
    ST_Boolean IsLargeArc;
    ST_SweepDirection SweepDirection;
    ST_Boolean IsStroked;
} CT_ArcSegment;


typedef enum {
    GradientStops,
} CT_CP_GradientStops;

typedef ST_Name CT_LinkTarget;

typedef struct CT_CP_LinkTargets_s {
    CT_LinkTarget LinkTarget;
    struct CT_CP_LinkTarget_s *next;
} CT_CP_LinkTarget;

typedef struct CT_Canvas_s {
    ST_RscRefMatrix RenderTransform;
    ST_RscRefAbbrGeomF Clip;
    ST_ZeroOne Opacity;
    ST_RscRef OpacityMask;
    ST_Name Name;
    ST_EdgeMode EdgeMode;
    ST_Name FixedPage; /* NB wrong see schema */
    ST_Name Key;  /* NB wrong see schema */
    /* NB missing fields see schema */
} CT_Canvas;

typedef struct CT_Glyphs_s {
    int BidiLevel;
    ST_CaretStops CaretStops;
    ST_UnicodeString DeviceFontName;
    ST_RscRefColor Fill;
    ST_GEZero FontRenderingEmSize;
    ST_Name FontUri; /* NB wrong see schema */
    ST_Double OriginX;
    ST_Double OriginY;
    ST_Boolean IsSideWays;
    ST_Indices Indices;
    ST_UnicodeString UnicodeString;
    ST_StyleSimulations StyleSimulations;
    ST_RscRefMatrix RenderTransform;
    ST_RscRefAbbrGeomF Clip;
    ST_ZeroOne Opacity;
    ST_RscRef OpacityMask;
    struct {
        unsigned BidiLevel : 1;
        unsigned OriginX : 1;
        unsigned OriginY : 1;
        unsigned FontRenderingEmSize : 1;
        unsigned IsSideWays : 1;
        unsigned Opacity : 1;
    } avail;

} CT_Glyphs;

typedef struct CT_MatrixTransform_s {
    ST_MatrixBare Matrix;
} CT_MatrixTransform;

typedef struct CT_SolidColorBrush_s {
    ST_ZeroOne Opacity;
    ST_Name Key;  /* NB wrong see schema */
    ST_Color Color;
} CT_SolidColorBrush;

typedef struct CT_LinearGradientBrush_s {
    char * Opacity;
    char * Key;
    char * ColorInterpolationMode;
    char * SpreadMethod;
    char * MappingMode;
    char * Transform;
    char * StartPoint;
    char * EndPoint;
} CT_LinearGradientBrush;

typedef struct CT_RadialGradientBrush_s {
    char * Opacity;
    char * Key;
    char * ColorInterpolationMode;
    char * SpreadMethod;
    char * MappingMode;
    char * Transform;
    char * Center;
    char * GradientOrigin;
    char * RadiusX;
    char * RadiusY;
} CT_RadialGradientBrush;

typedef struct CT_VisualBrush_s {
    ST_ZeroOne Opacity;
    ST_Name Key;  /* NB wrong see schema */
    ST_RscRefMatrix Transform;
    ST_ViewBox Viewbox;
    ST_ViewBox Viewport;
    ST_Stretch Fill; /* NB check this */
    ST_TileMode TileMode;
    ST_ViewUnits ViewboxUnits;
    ST_ViewUnits ViewportUnits;
    ST_RscRef Visual;
} CT_VisualBrush;

/* NB refactor common fields in brushes */
typedef struct CT_ImageBrush_s {
    ST_ZeroOne Opacity;
    ST_Name Key;  /* NB wrong see schema */
    ST_RscRefMatrix Transform;
    ST_ViewBox Viewbox;
    ST_ViewBox Viewport;
    ST_Stretch Fill; /* NB check this */
    ST_TileMode TileMode;
    ST_ViewUnits ViewboxUnits;
    ST_ViewUnits ViewportUnits;
    ST_Name ImageSource; /* NB wrong */
} CT_ImageBrush;

typedef struct CT_PathGeometry_s {
    ST_AbbrGeom Figures;
    ST_FillRule FillRule;
    ST_RscRefMatrix Transform;
    /* nb incomplete */
} CT_PathGeometry;

/* NB wrong, see schema */
typedef struct CT_ResourceDictionary_s {
    ST_Name anyURI;
    union {
        CT_Canvas Canvas;
        CT_Glyphs Glyphs;
        /* nb - uncomment and unravel circular
           definitions - exercise for the reader.
         CT_PATH Path */
        CT_SolidColorBrush SolidColorBrush;
        CT_LinearGradientBrush LinearGradientBrush;
        CT_RadialGradientBrush RadialGradientBrush;
        CT_VisualBrush VisualBrush;
        CT_ImageBrush ImageBrush;
        CT_MatrixTransform MatrixTransform;
        CT_PathGeometry PathGeometry;
    } res;
    ST_Name choice; /* nb the selector is yet another string */
} CT_ResourceDictionary;

/* NB check */
typedef struct CT_CP_Brush_s {
    union {
        CT_SolidColorBrush SolidColorBrush;
        CT_ImageBrush ImageBrush;
        CT_VisualBrush VisualBrush;
        CT_LinearGradientBrush LinearGradientBrush;
        CT_ResourceDictionary RadialGradientBrush;
    } Brush;
    ST_Name choice;
} CT_CP_Brush;

typedef struct CT_CP_Resources_s {
    CT_ResourceDictionary ResourceDictionary;
} CT_CP_Resources;

typedef struct CT_CP_Transform_s {
    CT_MatrixTransform MatrixTransform;
    struct CT_CP_Transform_s *next;
} CT_CP_Transform;
        

typedef struct CT_DocumentReference_s {
    ST_Name anyURI;
} CT_DocumentReference;


typedef struct CT_PageContent_s {
    ST_Name anyUri; /* nb wrong */
    ST_GEOne Width;
    ST_GEOne Height;
    CT_CP_LinkTarget LinkTargets;
} CT_PageContent;

typedef struct CT_FixedDocument_s {
    CT_PageContent PageContent;
    struct CT_FixedDocument_s *next;
} CT_FixedDocument;

typedef struct CT_FixedDocumentSequence_s {
    CT_DocumentReference DocumentReference;
    struct CT_FixedDocumentSequence_s *next;
} CT_FixedDocumentSequence;

    
typedef struct CT_GradientStop_s {
    char * Color;
    char * Offset;
} CT_GradientStop;

/* nb line segment variants have the same structure */
typedef struct CT_PolyLineSegment_s {
    ST_Points Points;
    ST_Boolean isStroked;
    struct {
        unsigned Points : 1;
        unsigned isStroked : 1;
    } avail;
} CT_PolyLineSegment;

typedef struct CT_PolyQuadraticBezierSegment_s {
    ST_Points Points;
    ST_Boolean isStroked;
} CT_PolyQuadraticBezierSegment;

typedef struct CT_PolyBezierSegment_s {
    ST_Points Points;
    ST_Boolean isStroked;
} CT_PolyBezierSegment;

typedef struct CT_PathFigure_s {
    ST_Boolean isClosed;
    ST_Point StartPoint;
    ST_Boolean isFilled;
    union {
        CT_ArcSegment ArcSegment;
        CT_PolyBezierSegment PolyBezierSegment;
        CT_PolyLineSegment PolyLineSegment;
        CT_PolyQuadraticBezierSegment PolyQuadraticBezierSegment;
    } seg;
    ST_Name choice;
} CT_PathFigure;
        
typedef struct CT_CP_Geometry {
    CT_PathGeometry PathGeometry;
} CT_CP_Geometry;

typedef struct CT_Path_s {
    ST_RscRefAbbrGeomF Data; 
    ST_RscRefColor Fill;
    ST_RscRefMatrix RenderTransform;
    ST_RscRefAbbrGeomF Clip;
    ST_ZeroOne Opacity;
    ST_RscRef OpacityMask;
    ST_RscRefColor Stroke;
    ST_EvenArrayPos StrokeDashArray;
    ST_DashCap StrokeDashCap;
    ST_Double StrokeDashOffset;
    ST_LineCap StrokeEndLineCap;
    ST_LineCap StrokeStartLineCap;
    ST_LineJoin StrokeLineJoin;
    ST_GEOne StrokeMiterLimit;
    ST_GEZero StrokeThickness;
    ST_Name Name;
    CT_CP_Transform Path_RenderTransform;
    CT_CP_Geometry Path_Clip;
    CT_CP_Brush Path_OpacityMask;
    CT_CP_Brush Path_Fill;
    CT_CP_Brush Path_Stroke;
    CT_CP_Geometry Path_Data;
    /* incomplete see schema */
    struct {
        unsigned Data : 1; 
        unsigned Fill : 1;
        unsigned RenderTransform : 1;
        unsigned Clip : 1;
        unsigned Opacity : 1;
        unsigned OpacityMask : 1;
        unsigned Stroke : 1;
        unsigned StrokeDashArray : 1;
        unsigned StrokeDashCap : 1;
        unsigned StrokeDashOffset : 1;
        unsigned StrokeEndLineCap : 1;
        unsigned StrokeStartLineCap : 1;
        unsigned StrokeLineJoin : 1;
        unsigned StrokeMiterLimit : 1;
        unsigned StrokeThickness : 1;
        unsigned Name : 1;
        unsigned Path_RenderTransform : 1;
        unsigned Path_Clip : 1;
        unsigned Path_OpacityMask : 1;
        unsigned Path_Fill : 1;
        unsigned Path_Stroke : 1;
        unsigned Path_Data : 1;
    } avail;
} CT_Path;

typedef struct CT_FixedPage_s {
    ST_GEOne Width;
    ST_GEOne Height;
    ST_ContentBox ContentBox;
    ST_BleedBox BleedBox;
    ST_Matrix RenderTransform;
    /* nb - not sure about xml:lang */
    ST_Name Name;
    /* nb parts missing */
    union {
        CT_Canvas Canvas;
        CT_Glyphs Glyphs;
        CT_Path Path;
    } vis;
    ST_Name choice;
    struct {
        unsigned Width : 1;
        unsigned Height : 1;
        unsigned ContentBox : 1;
        unsigned BleedBox : 1;
        unsigned RenderTransform : 1;
        unsigned Name : 1;
    } avail;
} CT_FixedPage;

typedef struct CT_CP_Visual_s {
    union {
        CT_Canvas Canvas;
        CT_Glyphs Glyphs;
        CT_Path Path;
    } vis;
    ST_Name choice;
} CT_CP_Visual;

#endif /* metcomplex_INCLUDED */
