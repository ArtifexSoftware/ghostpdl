/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* X Windows driver resource tables */
#include "std.h"	/* must precede any file that includes <sys/types.h> */
#include "x_.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gxdevice.h"
#include "gdevx.h"

/*
 * We segregate these tables into their own file because the definition of
 * the XtResource structure is botched -- it declares the strings as char *
 * rather than const char * -- and so compiling the statically initialized
 * tables with gcc -Wcast-qual produces dozens of bogus warnings.
 *
 * Astoundingly, not only does the X API specify these structures as not
 * being const, the Xt implementation actually writes into them.
 */

XtResource gdev_x_resources[] = {

/* (String) casts are here to suppress warnings about discarding `const' */
#define RINIT(a,b,t,s,o,it,n)\
  {(String)(a), (String)(b), (String)t, sizeof(s),\
   XtOffsetOf(gx_device_X, o), (String)it, (n)}
#define rpix(a,b,o,n)\
  RINIT(a,b,XtRPixel,Pixel,o,XtRString,(XtPointer)(n))
#define rdim(a,b,o,n)\
  RINIT(a,b,XtRDimension,Dimension,o,XtRImmediate,(XtPointer)(n))
#define rstr(a,b,o,n)\
  RINIT(a,b,XtRString,String,o,XtRString,(char*)(n))
#define rint(a,b,o,n)\
  RINIT(a,b,XtRInt,int,o,XtRImmediate,(XtPointer)(n))
#define rbool(a,b,o,n)\
  RINIT(a,b,XtRBoolean,Boolean,o,XtRImmediate,(XtPointer)(n))
#define rfloat(a,b,o,n)\
  RINIT(a,b,XtRFloat,float,o,XtRString,(XtPointer)(n))

    rpix(XtNbackground, XtCBackground, background, "XtDefaultBackground"),
    rpix(XtNborderColor, XtCBorderColor, borderColor, "XtDefaultForeground"),
    rdim(XtNborderWidth, XtCBorderWidth, borderWidth, 1),
    rpix(XtNforeground, XtCForeground, foreground, "XtDefaultForeground"),
    rstr(XtNgeometry, XtCGeometry, geometry, NULL),
    rint("maxGrayRamp", "MaxGrayRamp", maxGrayRamp, 128),
    rint("maxRGBRamp", "MaxRGBRamp", maxRGBRamp, 5),
    rstr("palette", "Palette", palette, "Color"),

    rbool("useBackingPixmap", "UseBackingPixmap", useBackingPixmap, True),
    rbool("useXPutImage", "UseXPutImage", useXPutImage, True),
    rbool("useXSetTile", "UseXSetTile", useXSetTile, True),
    rfloat("xResolution", "Resolution", xResolution, "0.0"),
    rfloat("yResolution", "Resolution", yResolution, "0.0"),

#undef RINIT
#undef rpix
#undef rdim
#undef rstr
#undef rint
#undef rbool
#undef rfloat
};

const int gdev_x_resource_count = XtNumber(gdev_x_resources);

String gdev_x_fallback_resources[] = {
    (String) "Ghostscript*Background: white",
    (String) "Ghostscript*Foreground: black",
    NULL
};
