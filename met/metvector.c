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

#include "metelement.h"

/* XML style data type for a path
</Path>
   Data="ST_RscRefAbbrGeomF [0..1]"
   Fill="ST_RscRefColor [0..1]"
   RenderTransform="ST_RscRefMatrix [0..1]"
   Clip="ST_RscRefAbbrGeomF [0..1]"
   Opacity="ST_ZeroOne [0..1]"
   OpacityMask="ST_RscRef [0..1]"
   Stroke="ST_RscRefColor [0..1]"
   StrokeDashArray="ST_EvenArrayPos [0..1]"
   StrokeDashCap="ST_DashCap [0..1]"
   StrokeDashOffset="ST_Double [0..1]"
   StrokeEndLineCap="ST_LineCap [0..1]"
   StrokeStartLineCap="ST_LineCap [0..1]"
   StrokeLineJoin="ST_LineJoin [0..1]"
   StrokeMiterLimit="ST_GEOne [0..1]"
   StrokeThickness="ST_GEZero [0..1]"
   Name="ST_Name [0..1]"
   FixedPage.NavigateUri="xs:anyURI [0..1]"
   xml:lang="[0..1]"
   x:Key="[0..1]">
     <Path.RenderTransform> ... </Path.RenderTransform> [0..1]
     <Path.Clip> ... </Path.Clip> [0..1]
     <Path.OpacityMask> ... </Path.OpacityMask> [0..1]
     <Path.Fill> ... </Path.Fill> [0..1]
     <Path.Stroke> ... </Path.Stroke> [0..1]
     <Path.Data> ... </Path.Data> [0..1]
</Path>

*/

