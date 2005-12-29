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
/* $Id:*/

/* metsimple.h */

/* metro simple types, NB work to be done on checking -- patterns,
   collapsing whitespace etc. */

#ifndef metsimple_INCLUDED
#   define metsimple_INCLUDED

typedef char * ST_AbbrGeom;
typedef char * ST_AbbrGeomF;
typedef char * ST_Array;
typedef char * ST_BleedBox;
typedef bool ST_Boolean;
typedef char * ST_CaretStops;
typedef char * ST_ClrIntMode;
typedef char * ST_Color;
typedef char * ST_CombineMode;
typedef char * ST_ContentBox;
typedef char * ST_DashCap;
typedef double ST_Double;
typedef char * ST_EdgeMode;
typedef char * ST_EvenArrayPos;
typedef char * ST_FillRule;
typedef double ST_GEOne;
typedef double ST_GEZero;
typedef char * ST_Indices;
typedef char * ST_LineCap;
typedef char * ST_LineJoin;
typedef char * ST_MappingMode;
typedef char * ST_Matrix;   /* NB orig. def. was  a union of MatrixDepr and MatrixBare */
typedef char * ST_MatrixDepr;
typedef char * ST_MatrixBare;
typedef char * ST_Name; /* NB really an xsd ID type */
typedef char * ST_Point;
typedef char * ST_Points;
typedef char * ST_RscRef;
typedef char * ST_RscRefAbbrGeomF; /* NB union ST_AbbrGeomF and ST_RscRef */
typedef char * ST_RscRefColor; /* NB union ST_Color and ST_RscRef */
typedef char * ST_RscRefMatrix; /* NB another union */
typedef char * ST_SpreadMethod; 
typedef char * ST_Stretch;
typedef char * ST_StyleSimulations;
typedef char * ST_SweepDirection;
typedef char * ST_TileMode;
typedef char * ST_UnicodeString;
typedef char * ST_ViewBox;
typedef char * ST_ViewUnits;
typedef double ST_ZeroOne;

/* accessors */


#endif /* metsimple_INCLUDED */
