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

#ifndef ttfInterpreter_DEFINED
#  define ttfInterpreter_DEFINED
typedef struct ttfInterpreter_s ttfInterpreter;
#endif

typedef struct ttfMemoryDescriptor_s ttfMemoryDescriptor;

typedef signed long F26Dot6;

typedef struct ttfMemory_s ttfMemory;
struct ttfMemory_s {   
    void *(*alloc_bytes)(ttfMemory *, int size,  const char *cname);
    void *(*alloc_struct)(ttfMemory *, const ttfMemoryDescriptor *,  const char *cname);
    void (*free)(ttfMemory *, void *p,  const char *cname);
} ;

typedef struct ttfSubGlyphUsage_s ttfSubGlyphUsage;

struct ttfInterpreter_s {
    TExecution_Context *exec;
    ttfSubGlyphUsage *usage;
    int usage_size;
    int usage_top;
    int lock;
    ttfMemory *ttf_memory;
};

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
    fBadFontData,
    fPatented
} FontError;

typedef struct ttfReader_s ttfReader;
struct ttfReader_s {
    bool   (*Eof)(ttfReader *);
    void   (*Read)(ttfReader *, void *p, int n);
    void   (*Seek)(ttfReader *, int nPos);
    int    (*Tell)(ttfReader *);
    bool   (*Error)(ttfReader *);
    int    (*SeekExtraGlyph)(ttfReader *, int nIndex);
    void   (*ReleaseExtraGlyph)(ttfReader *, int nIndex);
};

typedef struct {
    int nPos, nLen;
} ttfPtrElem;

#ifndef ttfFont_DEFINED
#  define ttfFont_DEFINED
typedef struct ttfFont_s ttfFont;
#endif
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
    unsigned int nMaxComponents;
    unsigned int nLongMetricsVert;
    unsigned int nLongMetricsHorz;
    unsigned int nIndexToLocFormat;
    bool    patented;
    bool    bOwnScale;
    TFace *face;
    TInstance *inst;
    TExecution_Context  *exec;
    ttfInterpreter *tti;
    void (*DebugRepaint)(ttfFont *);
    void (*DebugPrint)(ttfFont *, const char *s, ...);
};

void ttfFont__init(ttfFont *this, ttfMemory *mem, 
		    void (*DebugRepaint)(ttfFont *),
		    void (*DebugPrint)(ttfFont *, const char *s, ...));
void ttfFont__finit(ttfFont *this);
FontError ttfFont__Open(ttfInterpreter *, ttfFont *, ttfReader *r, 
			unsigned int nTTC, float w, float h);

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

int ttfInterpreter__obtain(ttfMemory *mem, ttfInterpreter **ptti);
void ttfInterpreter__release(ttfInterpreter **ptti);

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
FontError ttfOutliner__Outline(ttfOutliner *this, int glyphIndex,
	float orig_x, float orig_y, FloatMatrix *m1, bool grid_fit);

#endif
