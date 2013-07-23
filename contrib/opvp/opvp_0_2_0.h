/*

Copyright (c) 2003-2004, AXE, Inc.  All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/
/* $Id: opvp_0_2_0.h 85 2007-02-15 08:48:56Z sho-otani $ */
/* opvp_common.h  ver.1.00 rel.1.0  26 Nov 2004 */
/* OpenPrinting Vector Printer Driver Glue Code */

#ifndef	_FSGPD_0_2_0_H
#define	_FSGPD_0_2_0_H

#define _PDAPI_VERSION_MAJOR_	0
#define _PDAPI_VERSION_MINOR_	2

/* Return Values */
#define	OPVP_OK			0	/* error is -1 */

/* Error Codes */
#define	OPVP_FATALERROR		-101	/* unrecoverable error occurred within the */
                                                                        /* library */
#define	OPVP_BADREQUEST		-102	/* invalid function parameters */
#define	OPVP_BADCONTEXT		-103	/* bad parameter precontext */
#define	OPVP_NOTSUPPORTED	-104	/* printer or driver request not supported */
#define	OPVP_JOBCANCELED	-105	/* job cancelled */
#define	OPVP_PARAMERROR		-106	/* illegal parameter combination */

/* Info params */
#define	OPVP_INFO_PREFIX	"updf:"

/* Fix */
#define	OPVP_FIX_FRACT_WIDTH	8
#define	OPVP_FIX_FRACT_DENOM	(1<<OPVP_FIX_FRACT_WIDTH)
#define	OPVP_FIX_FLOOR_WIDTH	(sizeof(int)*8-OPVP_FIX_FRACT_WIDTH)
#if 0
typedef	struct {
        unsigned int	fract	: OPVP_FIX_FRACT_WIDTH;
        signed int	floor	: OPVP_FIX_FLOOR_WIDTH;
} OPVP_Fix;
#define	OPVP_i2Fix(i,fix)	(fix.fract=0,fix.floor=i)
#define	OPVP_Fix2f(fix,f)	(f=(double)fix.floor\
                                  +(double)(fix.fract)/OPVP_FIX_FRACT_DENOM)
#define	OPVP_f2Fix(f,fix)	(fix.fract=(f-floor(f))*OPVP_FIX_FRACT_DENOM,\
                                 fix.floor=floor(f))
#else
typedef	int	OPVP_Fix;
#define	OPVP_i2Fix(i,fix)	(fix=i<<8)
#define	OPVP_f2Fix(f,fix)	(fix=((int)floor(f)<<8)\
                                    |((int)((f-floor(f))*OPVP_FIX_FRACT_DENOM)\
                                      &0x000000ff))
#endif

/* Basic Types */
typedef	struct	_OPVP_Point {
        OPVP_Fix	x;
        OPVP_Fix	y;
} OPVP_Point;

typedef	struct	_OPVP_Rectangle {
        OPVP_Point	p0;		/* start point */
        OPVP_Point	p1;		/* end point */
} OPVP_Rectangle;

typedef	struct	_OPVP_RoundRectangle {
        OPVP_Point	p0;		/* start point */
        OPVP_Point	p1;		/* end point */
        OPVP_Fix	xellipse;
        OPVP_Fix	yellipse;
} OPVP_RoundRectangle;

/* Image Formats */
typedef	enum	_OPVP_ImageFormat {
        OPVP_iformatRaw			= 0,
        OPVP_iformatRLE			= 1,
        OPVP_iformatJPEG		= 2,
        OPVP_iformatPNG			= 3
} OPVP_ImageFormat;

/* Color Presentation */
typedef	enum	_OPVP_ColorMapping {
        OPVP_cmapDirect			= 0,
        OPVP_cmapIndexed		= 1
} OPVP_ColorMapping;

typedef	enum	_OPVP_ColorSpace {
        OPVP_cspaceBW			= 0,
        OPVP_cspaceDeviceGray		= 1,
        OPVP_cspaceDeviceCMY		= 2,
        OPVP_cspaceDeviceCMYK		= 3,
        OPVP_cspaceDeviceRGB		= 4,
        OPVP_cspaceStandardRGB		= 5,
        OPVP_cspaceStandardRGB64	= 6
} OPVP_ColorSpace;

/* Raster Operation modes */
typedef	enum	_OPVP_ROP {
        OPVP_ropPset			= 0,
        OPVP_ropPreset			= 1,
        OPVP_ropOr			= 2,
        OPVP_ropAnd			= 3,
        OPVP_ropXor			= 4
} OPVP_ROP;

/* Fill, Paint, Clip */
typedef	enum	_OPVP_FillMode {
        OPVP_fillModeEvenOdd		= 0,
        OPVP_fillModeWinding		= 1
} OPVP_FillMode;

typedef	enum	_OPVP_PaintMode {
        OPVP_paintModeOpaque		= 0,
        OPVP_paintModeTransparent	= 1
} OPVP_PaintMode;

typedef	enum	_OPVP_ClipRule {
        OPVP_clipRuleEvenOdd		= 0,
        OPVP_clipRuleWinding		= 1
} OPVP_ClipRule;

/* Line */
typedef	enum	_OPVP_LineStyle {
        OPVP_lineStyleSolid		= 0,
        OPVP_lineStyleDash		= 1
} OPVP_LineStyle;

typedef	enum	_OPVP_LineCap {
        OPVP_lineCapButt		= 0,
        OPVP_lineCapRound		= 1,
        OPVP_lineCapSquare		= 2
} OPVP_LineCap;

typedef	enum	_OPVP_LineJoin {
        OPVP_lineJoinMiter		= 0,
        OPVP_lineJoinRound		= 1,
        OPVP_lineJoinBevel		= 2
} OPVP_LineJoin;

/* Brush */
typedef	enum	_OPVP_BrushDataType {
        OPVP_bdtypeNormal		= 0
} OPVP_BrushDataType;

typedef	struct	_OPVP_BrushData {
        OPVP_BrushDataType	type;
        int		width;
        int		height;
        int		pitch;
#if (_PDAPI_VERSION_MAJOR_ == 0 && _PDAPI_VERSION_MINOR_ < 2)
        void		*data;		/* pointer to actual data */
#else
        char		data[1];
#endif
} OPVP_BrushData;

typedef	struct	_OPVP_Brush {
        OPVP_ColorSpace	colorSpace;
        int		color[4];	/* aRGB quadruplet */
#if (_PDAPI_VERSION_MAJOR_ == 0 && _PDAPI_VERSION_MINOR_ < 2)
        OPVP_BrushData	*pbrush;	/* pointer to brush data */
                                        /* solid brush used, if null */
        int		xorg;		/* brush origin */
        int		yorg;		/* ignored for SetBgColor */
#else
        int		xorg;		/* brush origin */
        int		yorg;		/* ignored for SetBgColor */
        OPVP_BrushData	*pbrush;	/* pointer to brush data */
                                        /* solid brush used, if null */
#endif
} OPVP_Brush;

/* Misc. Flags */
#define	OPVP_Arc		0	/* circular arc */
#define	OPVP_Chord		1	/* arch */
#define	OPVP_Pie		2	/* pie section */
#define	OPVP_Clockwise		0	/* clockwise */
#define	OPVP_Counterclockwise	1	/* counter-clockwise */
#define	OPVP_PathClose		0	/* Close path when using LinePath */
#define	OPVP_PathOpen		1	/* Do not close path when using LinePath */

/* CTM */
typedef	struct	_OPVP_CTM {
        float		a;
        float		b;
        float		c;
        float		d;
        float		e;
        float		f;
} OPVP_CTM;

/* Vector Driver API Proc. Entries */
typedef	struct	_OPVP_api_procs {
        int	(*OpenPrinter)(int,char *,int *,struct _OPVP_api_procs **);
        int	(*ClosePrinter)(int);
        int	(*StartJob)(int,char *);
        int	(*EndJob)(int);
        int	(*StartDoc)(int,char *);
        int	(*EndDoc)(int);
        int	(*StartPage)(int,char *);
        int	(*EndPage)(int);
#if (_PDAPI_VERSION_MAJOR_ > 0 || _PDAPI_VERSION_MINOR_ >= 2)
        int	(*QueryDeviceCapability)(int,int,int,char *);
        int	(*QueryDeviceInfo)(int,int,int,char *);
#endif
        int	(*ResetCTM)(int);
        int	(*SetCTM)(int,OPVP_CTM *);
        int	(*GetCTM)(int,OPVP_CTM *);
        int	(*InitGS)(int);
        int	(*SaveGS)(int);
        int	(*RestoreGS)(int);
        int	(*QueryColorSpace)(int,OPVP_ColorSpace *,int *);
        int	(*SetColorSpace)(int,OPVP_ColorSpace);
        int	(*GetColorSpace)(int,OPVP_ColorSpace *);
        int	(*QueryROP)(int,int *,int *);
        int	(*SetROP)(int,int);
        int	(*GetROP)(int,int *);
        int	(*SetFillMode)(int,OPVP_FillMode);
        int	(*GetFillMode)(int,OPVP_FillMode *);
        int	(*SetAlphaConstant)(int,float);
        int	(*GetAlphaConstant)(int,float *);
        int	(*SetLineWidth)(int,OPVP_Fix);
        int	(*GetLineWidth)(int,OPVP_Fix *);
        int	(*SetLineDash)(int,OPVP_Fix *,int);
        int	(*GetLineDash)(int,OPVP_Fix *,int *);
        int	(*SetLineDashOffset)(int,OPVP_Fix);
        int	(*GetLineDashOffset)(int,OPVP_Fix *);
        int	(*SetLineStyle)(int,OPVP_LineStyle);
        int	(*GetLineStyle)(int,OPVP_LineStyle *);
        int	(*SetLineCap)(int,OPVP_LineCap);
        int	(*GetLineCap)(int,OPVP_LineCap *);
        int	(*SetLineJoin)(int,OPVP_LineJoin);
        int	(*GetLineJoin)(int,OPVP_LineJoin *);
        int	(*SetMiterLimit)(int,OPVP_Fix);
        int	(*GetMiterLimit)(int,OPVP_Fix *);
        int	(*SetPaintMode)(int,OPVP_PaintMode);
        int	(*GetPaintMode)(int,OPVP_PaintMode *);
        int	(*SetStrokeColor)(int,OPVP_Brush *);
        int	(*SetFillColor)(int,OPVP_Brush *);
        int	(*SetBgColor)(int,OPVP_Brush *);
        int	(*NewPath)(int);
        int	(*EndPath)(int);
        int	(*StrokePath)(int);
        int	(*FillPath)(int);
        int	(*StrokeFillPath)(int);
        int	(*SetClipPath)(int,OPVP_ClipRule);
#if (_PDAPI_VERSION_MAJOR_ > 0 || _PDAPI_VERSION_MINOR_ >= 2)
        int	(*ResetClipPath)(int);
#endif
        int	(*SetCurrentPoint)(int,OPVP_Fix,OPVP_Fix);
        int	(*LinePath)(int,int,int,OPVP_Point *);
        int	(*PolygonPath)(int,int,int *,OPVP_Point *);
        int	(*RectanglePath)(int,int,OPVP_Rectangle *);
        int	(*RoundRectanglePath)(int,int,OPVP_RoundRectangle *);
#if (_PDAPI_VERSION_MAJOR_ == 0 && _PDAPI_VERSION_MINOR_ < 2)
        int	(*BezierPath)(int,int *,OPVP_Point *);
#else
        int	(*BezierPath)(int,int,OPVP_Point *);
#endif
        int	(*ArcPath)(int,int,int,OPVP_Fix,OPVP_Fix,OPVP_Fix,OPVP_Fix,
                           OPVP_Fix,OPVP_Fix,OPVP_Fix,OPVP_Fix);
        int	(*DrawBitmapText)(int,int,int,int,void *);
        int	(*DrawImage)(int,int,int,int,
                             OPVP_ImageFormat,OPVP_Rectangle,int,void *);
        int	(*StartDrawImage)(int,int,int,int,
                                  OPVP_ImageFormat,OPVP_Rectangle);
        int	(*TransferDrawImage)(int,int,void *);
        int	(*EndDrawImage)(int);
        int	(*StartScanline)(int,int);
        int	(*Scanline)(int,int,int *);
        int	(*EndScanline)(int);
        int	(*StartRaster)(int,int);
        int	(*TransferRasterData)(int,int,unsigned char *);
        int	(*SkipRaster)(int,int);
        int	(*EndRaster)(int);
        int	(*StartStream)(int);
        int	(*TransferStreamData)(int,int,void *);
        int	(*EndStream)(int);
} OPVP_api_procs;

#endif	/* _OPVP_COMMON_H_ */
