/* Copyright (C) 2003 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* The TrueType instruction interpreter interface definition. */

#ifndef incl_ttfoutl
#define incl_ttfoutl

#ifndef TFace_defined
#define TFace_defined
typedef struct _TFace TFace;
#endif

#ifndef TInstance_defined
#define TInstance_defined
typedef struct _TInstance TInstance;
#endif

#ifndef TExecution_Context_defined
#define TExecution_Context_defined
typedef struct _TExecution_Context TExecution_Context;
#endif

typedef struct ttfMemoryDescriptor_s ttfMemoryDescriptor;

typedef signed long F26Dot6;

typedef struct ttfMemory_s ttfMemory;
struct ttfMemory_s {   
    void *(*alloc_bytes)(ttfMemory *, int size,  const char *cname);
    void *(*alloc_struct)(ttfMemory *, const ttfMemoryDescriptor *,  const char *cname);
    void (*free)(ttfMemory *, void *p,  const char *cname);
} ;

typedef struct {
    double a, b, c, d, tx, ty;
} FloatMatrix;

typedef struct {
    double x, y;
} FloatPoint;

typedef enum {
    fNoError,
    fTableNotFound,
    fNameNotFound,
    fMemoryError,
    fUnimplemented,
    fCMapNotFound,
    fGlyphNotFound,
    fBadFontData
} FontError;

typedef struct ttfReader_s ttfReader;
struct ttfReader_s {
    bool   (*Eof)(ttfReader *);
    void   (*Read)(ttfReader *, void *p, int n);
    void   (*Seek)(ttfReader *, int nPos);
    int    (*Tell)(ttfReader *);
    bool   (*Error)(ttfReader *);
    bool   (*SeekExtraGlyph)(ttfReader *, int nIndex);
    void   (*ReleaseExtraGlyph)(ttfReader *);
};

typedef struct {
    int nPos, nLen;
} ttfPtrElem;

typedef struct ttfFont_s ttfFont;
struct ttfFont_s {
    ttfPtrElem t_cvt_;
    ttfPtrElem t_fpgm;
    ttfPtrElem t_glyf;
    ttfPtrElem t_head;
    ttfPtrElem t_hhea;
    ttfPtrElem t_hmtx;
    ttfPtrElem t_vhea;
    ttfPtrElem t_vmtx;
    ttfPtrElem t_loca;
    ttfPtrElem t_maxp;
    ttfPtrElem t_prep;
    ttfPtrElem t_cmap;
    unsigned short nUnitsPerEm;
    unsigned int nFlags;
    unsigned int nNumGlyphs;
    unsigned int nMaxPoints;
    unsigned int nMaxContours;
    unsigned int nMaxComponents;
    unsigned int nLongMetricsVert;
    unsigned int nLongMetricsHorz;
    unsigned int nIndexToLocFormat;
    byte  *onCurve;   
    F26Dot6 *x;         
    F26Dot6 *y;
    short *endPoints;
    TFace *face;
    TInstance *inst;
    TExecution_Context  *exec;
    ttfMemory *memory;
    void (*DebugRepaint)(ttfFont *);
    void (*DebugPrint)(ttfFont *, const char *s, ...);
};

void ttfFont__init(ttfFont *, ttfMemory *, 
		    void (*DebugRepaint)(ttfFont *),
		    void (*DebugPrint)(ttfFont *, const char *s, ...));
void ttfFont__finit(ttfFont *);
FontError ttfFont__Open(ttfFont *, ttfReader *r, unsigned int nTTC);

typedef struct ttfExport_s ttfExport;
struct ttfExport_s {
    bool bPoints, bOutline;
    void (*MoveTo)(ttfExport *, FloatPoint*);
    void (*LineTo)(ttfExport *, FloatPoint*);
    void (*CurveTo)(ttfExport *, FloatPoint*, FloatPoint*, FloatPoint*);
    void (*Close)(ttfExport *);
    void (*Point)(ttfExport *, FloatPoint*, bool bOnCurve, bool bNewPath);
    void (*SetWidth)(ttfExport *, FloatPoint*);
    void (*DebugPaint)(ttfExport *);
};

typedef struct {
    bool bOutline;
    bool bFirst;
    bool bVertical;
    int  nPointsTotal;
    int  nContoursTotal;
    F26Dot6 ppx, ppy;
    ttfReader *r;
    ttfExport *exp;
    ttfFont *pFont;
} ttfOutliner;

void ttfOutliner__init(ttfOutliner *, ttfFont *f, ttfReader *r, ttfExport *exp, 
			bool bOutline, bool bFirst, bool bVertical);
FontError ttfOutliner__Outline(ttfOutliner *, int glyphIndex, FloatMatrix *m1);

#endif
