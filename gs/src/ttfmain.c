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
/* A Free Type interface adapter. */
/* Uses code fragments from the FreeType project. */

#include "ttmisc.h"
#include "ttfoutl.h"
#include "ttfmemd.h"

#include "ttfinp.h"
#include "ttfsfnt.h"
#include "ttobjs.h"
#include "ttinterp.h"
#include "ttcalc.h"

/*
#ifdef DEBUG
void  Message( const char*  fmt, ... ) 
{   va_list ap;
    va_start(ap,fmt);
    vprintf(fmt,ap);
}
#endif
*/

#define wagOffset(type,field) ((int)((char *)&((type *)0)->field - (char *)(type *)0))

typedef struct { 
    F26Dot6 x;
    F26Dot6 y;
} F26Dot6Point;

typedef struct { 
    Fixed a, b, c, d, tx, ty;
} FixMatrix;

typedef struct { 
    FixMatrix m;
    int index;
    int flags;
    short arg1, arg2;
} ttfSubGlyphUsage;

typedef struct { 
    bool      bCompound;
    int32     contourCount;
    uint32    pointCount;
    F26Dot6Point  start;
    F26Dot6Point  advance;
    F26Dot6 sideBearing;
    short    *endPoints; /* [contourCount] */
    byte     *onCurve;   /* [pointCount] */
    F26Dot6  *x;         /* [pointCount] */
    F26Dot6  *y;         /* [pointCount] */
    F26Dot6   xMinB, yMinB, xMaxB, yMaxB;
} ttfGlyphOutline;

private void ttfGlyphOutline__init(ttfGlyphOutline *o)
{   o->contourCount = 0;
    o->pointCount = 0;
    o->endPoints = NULL;
    o->onCurve = NULL;
    o->x = NULL;
    o->y = NULL;
    o->bCompound=FALSE;
}

/*------------------------------------------------------------------- */

private Fixed AVE(F26Dot6 a, F26Dot6 b)
{   return (a + b) / 2;
}

private F26Dot6 shortToF26Dot6(short a)
{   return (F26Dot6)a << 6;
}

private void TransformF26Dot6PointFix(F26Dot6Point *pt, F26Dot6 dx, F26Dot6 dy, FixMatrix *m)
{   pt->x = MulDiv(dx, m->a, 65536) + MulDiv(dy, m->c, 65536) + (m->tx >> 10);
    pt->y = MulDiv(dx, m->b, 65536) + MulDiv(dy, m->d, 65536) + (m->ty >> 10);
}

private void TransformF26Dot6PointFloat(FloatPoint *pt, F26Dot6 dx, F26Dot6 dy, FloatMatrix *m)
{   pt->x = dx*m->a/64 + dy*m->c/64 + m->tx;
    pt->y = dx*m->b/64 + dy*m->d/64 + m->ty;
}

/*-------------------------------------------------------------------*/

private ttfPtrElem *ttfFont__get_table_ptr(ttfFont *f, char *id)
{
    if (!memcmp(id, "cvt ", 4))
	return &f->t_cvt_;
    if (!memcmp(id, "fpgm", 4))
	return &f->t_fpgm;
    if (!memcmp(id, "glyf", 4))
	return &f->t_glyf;
    if (!memcmp(id, "head", 4))
	return &f->t_head;
    if (!memcmp(id, "hhea", 4))
	return &f->t_hhea;
    if (!memcmp(id, "hmtx", 4))
	return &f->t_hmtx;
    if (!memcmp(id, "vhea", 4))
	return &f->t_vhea;
    if (!memcmp(id, "vmtx", 4))
	return &f->t_vmtx;
    if (!memcmp(id, "loca", 4))
	return &f->t_loca;
    if (!memcmp(id, "maxp", 4))
	return &f->t_maxp;
    if (!memcmp(id, "prep", 4))
	return &f->t_prep;
    if (!memcmp(id, "cmap", 4))
	return &f->t_cmap;
    return 0;
  };

/*-------------------------------------------------------------------*/

private int ExpandBuffer(ttfMemory *mem, void **pp, int nMax, int nStored, int nAddition, int elem_size)
{   int n;
    void *p;

    if(nAddition < 0)
	return 0;
    n = nStored + nAddition;
    if(n > 10000)
	return 0;
    if(n <= nMax)
	return nMax;
    p = mem->alloc_bytes(mem, n * elem_size, "ExpandBuffer");
    if(p == NULL)
	return FALSE;
    if(pp != NULL) { 
	memcpy(p, pp, nStored * elem_size);
	mem->free(mem, pp, "ExpandBuffer");
    }
  pp = p;
  return n;
}

/*-------------------------------------------------------------------*/

TT_Error  TT_Set_Instance_CharSizes(TT_Instance  instance,
                                       TT_F26Dot6   charWidth,
                                       TT_F26Dot6   charHeight)
{ 
    PInstance  ins = instance.z;

    if ( !ins )
	return TT_Err_Invalid_Instance_Handle;

    if (charWidth < 1*64)
	charWidth = 1*64;

    if (charHeight < 1*64)
	charHeight = 1*64;

    ins->metrics.x_scale1 = charWidth;
    ins->metrics.y_scale1 = charHeight;
    ins->metrics.x_scale2 = ins->owner->font->nUnitsPerEm;
    ins->metrics.y_scale2 = ins->owner->font->nUnitsPerEm;

    if (ins->owner->font->nFlags & 8) {
	ins->metrics.x_scale1 = (ins->metrics.x_scale1+32) & -64;
	ins->metrics.y_scale1 = (ins->metrics.y_scale1+32) & -64;
    }

    ins->metrics.x_ppem = ins->metrics.x_scale1/64;
    ins->metrics.y_ppem = ins->metrics.y_scale1/64;

    if (charWidth > charHeight)
	ins->metrics.pointSize = charWidth;
    else
	ins->metrics.pointSize = charHeight;

    ins->valid  = FALSE;
    return Instance_Reset(ins, FALSE);
  }

/*-------------------------------------------------------------------*/

void ttfFont__init(ttfFont *this, ttfMemory *mem, 
		    void (*DebugRepaint)(ttfFont *),
		    void (*DebugPrint)(ttfFont *, const char *s, ...))
{
    memset(this, 0, sizeof(*this));
    this->ttf_memory = mem;
    this->DebugRepaint = DebugRepaint;
    this->DebugPrint = DebugPrint;
}

void ttfFont__finit(ttfFont *this)
{   ttfMemory *mem = this->ttf_memory;

    if (this->exec)
	Context_Destroy(this->exec);
    this->exec = NULL;
    mem->free(mem, this->exec, "ttfFont__finit");
    this->exec = NULL;
    if (this->inst)
	Instance_Destroy(this->inst);
    mem->free(mem, this->inst, "ttfFont__finit");
    this->inst = NULL;
    if (this->face)
	Face_Destroy(this->face);
    mem->free(mem, this->face, "ttfFont__finit");
    this->face = NULL;
    mem->free(mem, this->onCurve, "ttfFont__finit");
    this->onCurve = NULL;
    mem->free(mem, this->x, "ttfFont__finit");
    this->x = NULL;
    mem->free(mem, this->y, "ttfFont__finit");
    this->y = NULL;
    mem->free(mem, this->endPoints, "ttfFont__finit");
    this->endPoints = NULL;
}

private bool ttfFont__ExpandContoursBuffer(ttfFont *this, int nStored, int nAddition)
{   int n = ExpandBuffer(this->ttf_memory, (void **)&this->endPoints, this->nMaxContours, nStored, 
			    nAddition, sizeof(this->endPoints[0]));

    if(!n)
	return FALSE;
    this->nMaxContours=n;
    return TRUE;
}

private bool ttfFont__ExpandPointsBuffer(ttfFont *this, int nStored, int nAddition)
{   int n1 = ExpandBuffer(this->ttf_memory, (void **)&this->onCurve, this->nMaxPoints, nStored, 
				    nAddition, sizeof(this->onCurve[0]));
    int n2 = ExpandBuffer(this->ttf_memory, (void **)&this->x, this->nMaxPoints, nStored, 
				    nAddition, sizeof(this->x[0]));
    int n3 = ExpandBuffer(this->ttf_memory, (void **)&this->y, this->nMaxPoints, nStored, 
				    nAddition, sizeof(this->y[0]));

    if (!n1 || !n2 || !n3)
	return FALSE;
    this->nMaxPoints = n1;
    return TRUE;
}

FontError ttfFont__Open(ttfFont *this, ttfReader *r, unsigned int nTTC)
{   char sVersion[4], sVersion0[4] = {0, 1, 0, 0};
    unsigned int nNumTables, i;
    unsigned int nMaxCompositePoints, nMaxCompositeContours;
    TT_Error code;
    int k;
    TT_Instance I;

    r->Read(r, sVersion, 4);
    if(!memcmp(sVersion, "ttcf", 4)) {
	unsigned int nFonts, nPos;

	r->Read(r, sVersion, 4);
	if(memcmp(sVersion, sVersion0, 4))
	    return fUnimplemented;
	nFonts = ttfReader__UInt(r);
	if(nTTC > nFonts)
	    nTTC = 0;
	for(i = 0; i <= nTTC; i++)
	    nPos = ttfReader__UInt(r);
	r->Seek(r, nPos);
	r->Read(r, sVersion, 4);
    }
    if(memcmp(sVersion, sVersion0, 4) && memcmp(sVersion, "true", 4))
	return fUnimplemented;
    nNumTables    = ttfReader__UShort(r);
    ttfReader__UShort(r); /* nSearchRange */
    ttfReader__UShort(r); /* nEntrySelector */
    ttfReader__UShort(r); /* nRangeShift */
    for (i = 0; i < nNumTables; i++) {
	char sTag[5];
	unsigned int nOffset, nLength;
	ttfPtrElem *e;

	sTag[4] = 0;
	r->Read(r, sTag, 4);
	ttfReader__UInt(r); /* nCheckSum */
	nOffset = ttfReader__UInt(r);
	nLength = ttfReader__UInt(r);
	e = ttfFont__get_table_ptr(this, sTag);
	if (e != NULL) {
	    e->nPos = nOffset;
	    e->nLen = nLength;
	}
    }
    r->Seek(r, this->t_head.nPos + wagOffset(sfnt_FontHeader, flags));
    this->nFlags = ttfReader__UShort(r);
    r->Seek(r, this->t_head.nPos + wagOffset(sfnt_FontHeader, unitsPerEm));
    this->nUnitsPerEm = ttfReader__UShort(r);
    r->Seek(r, this->t_head.nPos + wagOffset(sfnt_FontHeader, indexToLocFormat));
    this->nIndexToLocFormat = ttfReader__UShort(r);
    r->Seek(r, this->t_maxp.nPos + wagOffset(sfnt_maxProfileTable, numGlyphs));
    this->nNumGlyphs = ttfReader__UShort(r);
    this->nMaxPoints = ttfReader__UShort(r);
    this->nMaxContours = ttfReader__UShort(r);
    nMaxCompositePoints = ttfReader__UShort(r);
    nMaxCompositeContours = ttfReader__UShort(r);
    if (this->nMaxPoints < nMaxCompositePoints)
	this->nMaxPoints = nMaxCompositePoints;
    if (this->nMaxContours < nMaxCompositeContours)
	this->nMaxContours = nMaxCompositeContours;
    r->Seek(r, this->t_maxp.nPos + wagOffset(sfnt_maxProfileTable, maxComponentElements));
    this->nMaxComponents = ttfReader__UShort(r);
    if(this->nMaxComponents < 10)
	this->nMaxComponents = 10; /* work around DynaLab bug in lgoth.ttf */
    r->Seek(r, this->t_hhea.nPos + wagOffset(sfnt_MetricsHeader, numberLongMetrics));
    this->nLongMetricsHorz = ttfReader__UShort(r);
    if (this->t_vhea.nPos != 0) {
	r->Seek(r, this->t_vhea.nPos + wagOffset(sfnt_MetricsHeader, numberLongMetrics));
	this->nLongMetricsVert = ttfReader__UShort(r);
    } else
	this->nLongMetricsVert = 0;
    this->x = (F26Dot6 *)this->ttf_memory->alloc_bytes(this->ttf_memory, 
			    (this->nMaxPoints + 2) * sizeof(*this->x), "ttfFont__Open");
    this->y = (F26Dot6 *)this->ttf_memory->alloc_bytes(this->ttf_memory, 
			    (this->nMaxPoints + 2) * sizeof(*this->y), "ttfFont__Open");
    this->onCurve = (byte *)this->ttf_memory->alloc_bytes(this->ttf_memory, 
			    (this->nMaxPoints + 2) * sizeof(*this->onCurve), "ttfFont__Open");
    this->endPoints = (short *)this->ttf_memory->alloc_bytes(this->ttf_memory, 
			    this->nMaxContours * sizeof(*this->endPoints), "ttfFont__Open");
    if(this->x==NULL || this->y==NULL || this->onCurve==NULL || this->endPoints==NULL)
	return fMemoryError;
    this->face = this->ttf_memory->alloc_struct(this->ttf_memory, 
			    (const ttfMemoryDescriptor *)&st_TFace, "ttfFont__Open");
    if (this->face==NULL)
	return fMemoryError;
    memset(this->face, 0, sizeof(*this->face));
    this->face->r = r;
    this->face->font = this;
    code = Face_Create(this->face);
    if (code)
	return fMemoryError;
    code = r->Error(r);
    if (code < 0)
	return fBadFontData;
    this->inst = this->ttf_memory->alloc_struct(this->ttf_memory, 
			    (const ttfMemoryDescriptor *)&st_TInstance, "ttfFont__Open");
    if (this->inst == NULL)
	return fMemoryError;
    memset(this->inst, 0, sizeof(*this->inst));
    this->exec = this->ttf_memory->alloc_struct(this->ttf_memory, 
			    (const ttfMemoryDescriptor *)&st_TExecution_Context, "ttfFont__Open");
    if (this->exec==NULL)
	return fMemoryError;
    memset(this->exec, 0, sizeof(*this->exec));
    code = Context_Create(this->exec, this->face);
    if (code)
	return fBadFontData;
    code = Instance_Create(this->inst, this->face);
    if (code)
	return fBadFontData;
    for(k = 0; k < this->face->cvtSize; k++)
	this->inst->cvt[k] = shortToF26Dot6(this->face->cvt[k]);
    code = Instance_Init(this->inst);
    if (code)
	return fBadFontData;
    I.z = this->inst;
    code = TT_Set_Instance_CharSizes(I, shortToF26Dot6(this->nUnitsPerEm), shortToF26Dot6(this->nUnitsPerEm));
    /* Note : Free memory before checking the return code. */
    this->ttf_memory->free(this->ttf_memory, this->exec->pts.touch, "ttfFont__Open");
    this->exec->pts.touch = NULL;
    this->ttf_memory->free(this->ttf_memory, this->exec->pts.org_y, "ttfFont__Open");
    this->exec->pts.org_y = NULL;
    this->ttf_memory->free(this->ttf_memory, this->exec->pts.org_x, "ttfFont__Open");
    this->exec->pts.org_x = NULL;
    this->ttf_memory->free(this->ttf_memory, this->exec->pts.contours, "ttfFont__Open");
    this->exec->pts.contours = NULL;
    this->inst->metrics  = this->exec->metrics;
    if (code == TT_Err_Invalid_Engine) {
	this->patented = true;
	return fPatented;
    }
    if (code == TT_Err_Out_Of_Memory)
	return fMemoryError;
    if (code)
	return fBadFontData;
    return code;
}

private void ttfFont__StartGlyph(ttfFont *this)
{   Context_Load( this->exec, this->inst );
    if ( this->inst->GS.instruct_control & 2 )
	this->exec->GS = Default_GraphicsState;
    else
	this->exec->GS = this->inst->GS;
}

private void ttfFont__StopGlyph(ttfFont *this)
{
    Context_Save(this->exec, this->inst);
}

/*-------------------------------------------------------------------*/

private void  mount_zone( PGlyph_Zone  source,
                          PGlyph_Zone  target )
{
    Int  np, nc;

    np = source->n_points;
    nc = source->n_contours;

    target->org_x = source->org_x + np;
    target->org_y = source->org_y + np;
    target->cur_x = source->cur_x + np;
    target->cur_y = source->cur_y + np;
    target->touch = source->touch + np;

    target->contours = source->contours + nc;

    target->n_points   = 0;
    target->n_contours = 0;
}

private void  Init_Glyph_Component( PSubglyph_Record    element,
                                   PSubglyph_Record    original,
                                   PExecution_Context  exec )
{
    element->index     = -1;
    element->is_scaled = FALSE;
    element->is_hinted = FALSE;

    if (original)
	mount_zone( &original->zone, &element->zone );
    else
	element->zone = exec->pts;

    element->zone.n_contours = 0;
    element->zone.n_points   = 0;

    element->arg1 = 0;
    element->arg2 = 0;

    element->element_flag = 0;
    element->preserve_pps = FALSE;

    element->transform.xx = 1 << 16;
    element->transform.xy = 0;
    element->transform.yx = 0;
    element->transform.yy = 1 << 16;

    element->transform.ox = 0;
    element->transform.oy = 0;

    element->leftBearing  = 0;
    element->advanceWidth = 0;
  }

private void  cur_to_org( Int  n, PGlyph_Zone  zone )
{
    Int  k;

    for ( k = 0; k < n; k++ )
	zone->org_x[k] = zone->cur_x[k];

    for ( k = 0; k < n; k++ )
	zone->org_y[k] = zone->cur_y[k];
}

private void  org_to_cur( Int  n, PGlyph_Zone  zone )
{
    Int  k;

    for ( k = 0; k < n; k++ )
	zone->cur_x[k] = zone->org_x[k];

    for ( k = 0; k < n; k++ )
	zone->cur_y[k] = zone->org_y[k];
}


/*-------------------------------------------------------------------*/

void ttfOutliner__init(ttfOutliner *this, ttfFont *f, ttfReader *r, ttfExport *exp, 
			bool bOutline, bool bFirst, bool bVertical) 
{
    this->r = r; 
    this->bOutline = bOutline;
    this->bFirst = bFirst;
    this->pFont = f;
    this->bVertical = bVertical;
    this->exp = exp;
}

private int ttfOutliner__SeekGlyph(ttfOutliner *this, unsigned int nGlyphIndex, unsigned int *nNextGlyphPtr)
{   ttfFont *pFont = this->pFont;
    ttfReader *r = this->r;
    int nLoc, nMul, p, q;
    bool bBeg;
    
    if (nGlyphIndex > pFont->nNumGlyphs)
	return 0;
    if (!nGlyphIndex)
	return 1; /* empty glyph */
    nLoc = pFont->t_glyf.nPos;
    nMul = ((pFont->nIndexToLocFormat == SHORT_INDEX_TO_LOC_FORMAT) ? 2 : 4);
    r->Seek(r, pFont->t_loca.nPos + nMul * nGlyphIndex);
    p = ((nMul == 2) ? (int)ttfReader__UShort(r) <<1  : (int)ttfReader__UInt(r));
    if (p < 0 || p > pFont->t_glyf.nLen)
	return 0;
    bBeg = TRUE;
    if (p == 0) {
	int i;
	for (i = nGlyphIndex - 1; i >= 0; i--) {
	    r->Seek(r, pFont->t_loca.nPos + nMul * nGlyphIndex);
	    q = ((nMul == 2) ? (int)ttfReader__UShort(r) << 1 : (int)ttfReader__UInt(r));
	    if (q > 0)
		return 1;
        }
	bBeg = (i <=0 );
	r->Seek(r, pFont->t_loca.nPos + nMul * (nGlyphIndex + 1));
    }
    q = 0;
    for (nGlyphIndex++; nGlyphIndex <= pFont->nNumGlyphs; nGlyphIndex++) {
	q = ((nMul ==2 ) ? (int)ttfReader__UShort(r) << 1 : (int)ttfReader__UInt(r));
	if(q || bBeg) 
	    break;
    }
    if(p == q)
	return 1; /* empty glyph */
    *nNextGlyphPtr = ((q <p ) ? 0 : q + nLoc);
    r->Seek(r, p + nLoc);
    return 2;
}

private void MoveGlyphOutline(ttfGlyphOutline* out, FixMatrix *m)
{   F26Dot6* x = out->x;
    F26Dot6* y = out->y;
    short count = out->pointCount;
    F26Dot6Point p;
    for (; count != 0; --count) {
	TransformF26Dot6PointFix(&p,*x,*y,m);
	*x++=p.x;
	*y++=p.y;
    }
    TransformF26Dot6PointFix(&p,out->start.x,out->start.y,m);
    out->start.x=p.x;
    out->start.y=p.y;
}

private FontError ttfOutliner__BuildGlyphOutline(ttfOutliner *this, int glyphIndex, ttfGlyphOutline* gOutline)
{   ttfFont *pFont = this->pFont;
    ttfReader *r = this->r;
    short sideBearing;
    FontError error = fNoError;
    short arg1, arg2;
    short count;
    unsigned int nMtxPos, nMtxGlyph = glyphIndex, nLongMetrics, i;
    unsigned short nAdvance;
    unsigned int nNextGlyphPtr = 0;
    unsigned int nPosBeg;
    TExecution_Context *exec;
    TSubglyph_Record  subglyph;
    ttfSubGlyphUsage *usage = NULL;

    if(this->bVertical && pFont->t_vhea.nPos && pFont->t_vmtx.nPos) {
	nLongMetrics = pFont->nLongMetricsVert;
	nMtxPos = pFont->t_vmtx.nPos;
    } else {
	nLongMetrics = pFont->nLongMetricsHorz;
	nMtxPos = pFont->t_hmtx.nPos;
    }
    if (this->bVertical && (!pFont->t_vhea.nPos || pFont->t_vmtx.nPos) && nMtxGlyph < nLongMetrics) {
	/* A bad font fix. */
	nMtxGlyph = nLongMetrics;
	if(nMtxGlyph >= pFont->nNumGlyphs)
	    nMtxGlyph = pFont->nNumGlyphs - 1;
    }
    if (nMtxGlyph < nLongMetrics) {
	r->Seek(r, nMtxPos + 4 * nMtxGlyph);
	nAdvance = ttfReader__Short(r);
	sideBearing = ttfReader__Short(r);
    } else {
	r->Seek(r, nMtxPos + 4 * (nLongMetrics - 1));
	nAdvance = ttfReader__Short(r);
	r->Seek(r, nMtxPos + 4 * nLongMetrics + 2 * (nMtxGlyph - nLongMetrics));
	sideBearing = ttfReader__Short(r);
    }
    if (r->Error(r))
	goto errex;
    gOutline->sideBearing = shortToF26Dot6(sideBearing);
    gOutline->advance.x = shortToF26Dot6(nAdvance);
    gOutline->start.x = 0;
    gOutline->start.y = 0;
    gOutline->advance.y = 0;
    this->bFirst = FALSE;


    if (!this->bOutline)
	return fNoError;
    if (!r->SeekExtraGlyph(r, glyphIndex)) /* Incremental font support. */
	switch (ttfOutliner__SeekGlyph(this, glyphIndex, &nNextGlyphPtr)) {
	    case 0:
		return fGlyphNotFound;
	    case 1:
		return fNoError;
	}
    if (r->Eof(r)) {
	r->ReleaseExtraGlyph(r, glyphIndex);
	return fNoError;
    }
    if (r->Error(r))
	goto errex;
    nPosBeg = r->Tell(r);

    gOutline->contourCount = ttfReader__Short(r);
    gOutline->xMinB = shortToF26Dot6(ttfReader__Short(r));
    gOutline->yMinB = shortToF26Dot6(ttfReader__Short(r));
    gOutline->xMaxB = shortToF26Dot6(ttfReader__Short(r));
    gOutline->yMaxB = shortToF26Dot6(ttfReader__Short(r));

    /* FreeType stuff beg */
    exec = pFont->exec;
    Init_Glyph_Component(&subglyph, NULL, pFont->exec);
    subglyph.leftBearing = shortToF26Dot6(sideBearing);
    subglyph.advanceWidth = shortToF26Dot6(nAdvance);
    subglyph.bbox.xMin = gOutline->xMinB;
    subglyph.bbox.xMax = gOutline->xMaxB;
    subglyph.bbox.yMin = gOutline->yMinB;
    subglyph.bbox.yMax = gOutline->yMaxB;
    subglyph.pp1.x = gOutline->xMinB - gOutline->sideBearing;
    subglyph.pp1.y = 0;
    subglyph.pp2.x = subglyph.pp1.x + gOutline->advance.x;
    subglyph.pp2.y = 0;
    /* FreeType stuff end */

    if (gOutline->contourCount == 0)
	gOutline->pointCount = 0;
    else if (gOutline->contourCount == -1) {
	short flags, index, bHaveInstructions = 0;
	unsigned int nUsage = 0;
	unsigned int nPoints = this->nPointsTotal, nContours = this->nContoursTotal;
	unsigned int nPos;
	unsigned int n_ins;

	gOutline->bCompound = TRUE;
	usage = pFont->ttf_memory->alloc_bytes(pFont->ttf_memory, pFont->nMaxComponents * sizeof(ttfSubGlyphUsage), "ttfOutliner__BuildGlyphOutline");
	if (usage == NULL) {
	    error = fMemoryError; goto ex;
        }
	gOutline->contourCount = gOutline->pointCount = 0;
	do { 
	    FixMatrix m;
	    ttfSubGlyphUsage *e;

	    if (nUsage >= pFont->nMaxComponents) {
		error = fMemoryError; goto ex;
	    }
	    flags = ttfReader__UShort(r);
	    index = ttfReader__UShort(r);
	    bHaveInstructions |= (flags&WE_HAVE_INSTRUCTIONS);
	    if (flags & ARG_1_AND_2_ARE_WORDS) {
		arg1 = ttfReader__Short(r);
		arg2 = ttfReader__Short(r);
            } else {
		if (flags & ARGS_ARE_XY_VALUES) {
		    /* offsets are signed */
		    arg1 = ttfReader__SignedByte(r);
		    arg2 = ttfReader__SignedByte(r);
                } else { /* anchor points are unsigned */
		    arg1 = ttfReader__Byte(r);
		    arg2 = ttfReader__Byte(r);
                }
            }
	    m.b = m.c = 0;
	    if (flags & WE_HAVE_A_SCALE)
		m.a = m.d = (Fixed)ttfReader__Short(r) << 2;
	    else if (flags & WE_HAVE_AN_X_AND_Y_SCALE) {
		m.a = (Fixed)ttfReader__Short(r) << 2;
		m.d = (Fixed)ttfReader__Short(r) << 2;
	    } else if (flags & WE_HAVE_A_TWO_BY_TWO) {
		m.a = (Fixed)ttfReader__Short(r)<<2;
		m.b = (Fixed)ttfReader__Short(r)<<2;
		m.c = (Fixed)ttfReader__Short(r)<<2;
		m.d = (Fixed)ttfReader__Short(r)<<2;
            } else 
		m.a = m.d = 65536;
	    e = &usage[nUsage];
	    e->m = m;
	    e->index = index;
	    e->arg1 = arg1;
	    e->arg2 = arg2;
	    e->flags = flags;
	    nUsage++;
        } while (flags & MORE_COMPONENTS);
	/* Some fonts have bad WE_HAVE_INSTRUCTIONS, so use nNextGlyphPtr : */
	if (r->Error(r))
	    goto errex;
	nPos = r->Tell(r);
	n_ins = ((!r->Eof(r) && (bHaveInstructions || nPos < nNextGlyphPtr)) ? ttfReader__UShort(r) : 0);
	nPos = r->Tell(r);
	r->ReleaseExtraGlyph(r, glyphIndex);
	for (i = 0; i < nUsage; i++) {
	    ttfGlyphOutline out;
	    ttfSubGlyphUsage *e = &usage[i];
	    int j;

	    ttfGlyphOutline__init(&out);
	    error = ttfOutliner__BuildGlyphOutline(this, e->index, &out);
	    if(error!=fNoError)
		goto ex;
	    gOutline->onCurve   = pFont->onCurve   + nPoints;
	    gOutline->x         = pFont->x         + nPoints;
	    gOutline->y         = pFont->y         + nPoints;
	    gOutline->endPoints = pFont->endPoints + nContours;
	    if (flags & ARGS_ARE_XY_VALUES) {
		e->m.tx = Scale_X( &exec->metrics, e->arg1 )<<10;
		e->m.ty = Scale_Y( &exec->metrics, e->arg2 )<<10;
            } else {
		e->m.tx = ((gOutline->x)[e->arg1] - (out.x)[e->arg2])<<10;
		e->m.ty = ((gOutline->y)[e->arg1] - (out.y)[e->arg2])<<10;
            }
	    MoveGlyphOutline(&out, &e->m);
	    for (j = 0; j < out.contourCount; j++)
		out.endPoints[j] += gOutline->pointCount;
	    gOutline->contourCount += out.contourCount;
	    gOutline->pointCount += out.pointCount;
	    if (!out.bCompound) {
		this->nContoursTotal += out.contourCount;
		this->nPointsTotal += out.pointCount;
            }
	    if(e->flags & USE_MY_METRICS) {
		gOutline->advance.x = out.advance.x; 
		gOutline->sideBearing = out.sideBearing;
            }
        }
	if (n_ins && n_ins <= exec->owner->maxProfile.maxSizeOfInstructions && 
		!(pFont->inst->GS.instruct_control & 1)) {
	    TT_Error code;

	    exec->glyphSize = n_ins;
	    r->SeekExtraGlyph(r, glyphIndex); /* incremental font support */
	    r->Seek(r, nPos);
	    r->Read(r, exec->glyphIns, n_ins);
	    if (r->Error(r))
		goto errex;
	    code = Set_CodeRange(exec, TT_CodeRange_Glyph, exec->glyphIns, exec->glyphSize);
	    if (!code) {
		TGlyph_Zone *pts = &exec->pts;
		int nPoints = gOutline->pointCount + 2;
		int k;
		F26Dot6 x;

		exec->pts = subglyph.zone;
		pts->touch = gOutline->onCurve;
		pts->org_x = gOutline->x;
		pts->org_y = gOutline->y;
		pts->contours = gOutline->endPoints;
		pts->n_points = nPoints;
		pts->n_contours = gOutline->contourCount;
		/* add phantom points : */
		subglyph.pp1.x = gOutline->xMinB - gOutline->sideBearing;
		subglyph.pp1.y = 0;
		subglyph.pp2.x = subglyph.pp1.x + gOutline->advance.x;
		subglyph.pp1.y = 0;
		pts->org_x[nPoints - 2] = subglyph.pp1.x;
		pts->org_y[nPoints - 2] = subglyph.pp1.y;
		pts->org_x[nPoints - 1] = subglyph.pp2.x;
		pts->org_y[nPoints - 1] = subglyph.pp2.y;
		pts->touch[nPoints - 1] = 0;
		pts->touch[nPoints - 2] = 0;
		/* if hinting, round the phantom points : */
		x = pts->org_x[nPoints - 2];
		x = ((x + 32) & -64) - x;
		if (x)
		    for (k = 0; k < nPoints; k++)
			pts->org_x[k] += x;
		for (k = 0; k < nPoints; k++)
		    pts->touch[k] = pts->touch[k] & TT_Flag_On_Curve;
		org_to_cur(nPoints, pts);
		exec->is_composite = TRUE;
		if (pFont->patented)
		    code = TT_Err_Invalid_Engine;
		else
		    code = Context_Run(exec, FALSE);
		if (!code)
		    cur_to_org(nPoints, pts);
		else if (code == TT_Err_Invalid_Engine)
		    error = fPatented;
		else
		    error = fBadFontData;
		gOutline->sideBearing = gOutline->xMinB - subglyph.pp1.x;
		gOutline->advance.x = subglyph.pp2.x - subglyph.pp1.x;
            }
        }
    } else if (gOutline->contourCount > 0) {
	uint16 i;
	int nPoints;
	bool bInsOK;
	byte *onCurve, *stop, flag;

	if (gOutline->contourCount > 200) {
	    error = fBadFontData; goto ex;
        }
	if (!ttfFont__ExpandContoursBuffer(pFont, this->nContoursTotal, gOutline->contourCount)) {
	    error = fMemoryError; goto ex;
        }
	gOutline->endPoints = pFont->endPoints + this->nContoursTotal;
	for (i = 0; i<gOutline->contourCount; i++)
	    gOutline->endPoints[i] = ttfReader__Short(r);
	for (i = 1; i < gOutline->contourCount; i++)
	    if (gOutline->endPoints[i - 1] >= gOutline->endPoints[i]) {
		error = fBadFontData; goto ex;
	    }
	nPoints = gOutline->pointCount = gOutline->endPoints[gOutline->contourCount - 1] + 1;
	if (!ttfFont__ExpandPointsBuffer(pFont, this->nPointsTotal, nPoints)) {
	    error=fMemoryError; goto ex;
        }
	gOutline->onCurve   = pFont->onCurve   + this->nPointsTotal;
	gOutline->x         = pFont->x         + this->nPointsTotal;
	gOutline->y         = pFont->y         + this->nPointsTotal;
	exec->glyphSize = ttfReader__Short(r);
	if (exec->glyphSize > pFont->face->maxProfile.maxSizeOfInstructions) {
	    error = fMemoryError; goto ex;
        }
	r->Read(r, exec->glyphIns, exec->glyphSize);
	if (r->Error(r))
	    goto errex;
	bInsOK = !Set_CodeRange(exec,TT_CodeRange_Glyph, exec->glyphIns, exec->glyphSize);
	onCurve = gOutline->onCurve;
	stop = onCurve + gOutline->pointCount;

	while (onCurve < stop) {
	    *onCurve++ = flag = ttfReader__Byte(r);
	    if (flag & REPEAT_FLAGS) {
		count = ttfReader__Byte(r);
		for (--count; count >= 0; --count)
		    *onCurve++ = flag;
            }
        }
	/*  Lets do X */
	{   short coord = (this->bVertical ? 0 : sideBearing - (gOutline->xMinB >> 6));
	    F26Dot6* x = gOutline->x;
	    onCurve = gOutline->onCurve;
	    while (onCurve < stop) {
		if ((flag = *onCurve++) & XSHORT) {
		    if (flag & SHORT_X_IS_POS)
			coord += ttfReader__Byte(r);
		    else
		    coord -= ttfReader__Byte(r);
		} else if (!(flag & NEXT_X_IS_ZERO))
		coord += ttfReader__Short(r);
		*x++ = Scale_X(&exec->metrics, coord);
	    }
	}
	/*  Lets do Y */
	{   short coord = 0;
	    F26Dot6* y = gOutline->y;
	    onCurve = gOutline->onCurve;
	    while (onCurve < stop) {
		if((flag = *onCurve) & YSHORT)
		    if ( flag & SHORT_Y_IS_POS )
			coord += ttfReader__Byte(r);
		    else
			coord -= ttfReader__Byte(r);
		else if(!(flag & NEXT_Y_IS_ZERO))
		    coord += ttfReader__Short(r);
		*y++ = Scale_Y( &exec->metrics, coord );
            
		/*  Filter off the extra bits */
		*onCurve++ = flag & ONCURVE;
	    }
	}
	if (r->Error(r))
	    goto errex;
	if (exec->glyphSize && bInsOK && !(pFont->inst->GS.instruct_control&1)) {
	    TGlyph_Zone *pts = &exec->pts;
	    int n_points = nPoints + 2;
	    int k;
	    F26Dot6 x;
	    TT_Error code;

	    pts->touch = gOutline->onCurve;
	    pts->org_x = gOutline->x;
	    pts->org_y = gOutline->y;
	    pts->contours = gOutline->endPoints;
	    exec->is_composite = FALSE;
	    pts->touch[nPoints    ] = 0; /* phantom point */
	    pts->touch[nPoints + 1] = 0; /* phantom point */
	    pts->org_x[nPoints    ] = subglyph.pp1.x; /* phantom point */
	    pts->org_x[nPoints + 1] = subglyph.pp2.x; /* phantom point */
	    pts->org_y[nPoints    ] = 0;
	    pts->org_y[nPoints + 1] = 0;
	    pts->n_points   = n_points;
	    pts->n_contours = gOutline->contourCount;
	    x = pts->org_x[n_points - 2];
	    x = ((x + 32) & -64) - x;
	    if (x)
		for (k = 0; k < n_points; k++)
		    pts->org_x[k] += x;
	    org_to_cur(n_points, pts);
	    pts->cur_x[n_points - 1] = (pts->cur_x[n_points - 1] + 32) & -64;
	    exec->is_composite = FALSE;
	    for (k = 0; k < n_points; k++)
		pts->touch[k] &= TT_Flag_On_Curve;
	    if (pFont->patented)
		code = TT_Err_Invalid_Engine;
	    else
		code = Context_Run(exec, FALSE );
	    if (!code)
		cur_to_org(n_points, pts);
	    else if (code == TT_Err_Invalid_Engine)
		error = fPatented;
	    else
		error = fBadFontData;
	    gOutline->sideBearing = gOutline->xMinB - subglyph.pp1.x;
	    gOutline->advance.x = subglyph.pp2.x - subglyph.pp1.x;
        }
    } else
	error = fBadFontData;
    goto ex;
errex:;
    error = fBadFontData;
ex:;
    if (usage != NULL)
	pFont->ttf_memory->free(pFont->ttf_memory, usage, "ttfOutliner__BuildGlyphOutline");
    r->ReleaseExtraGlyph(r, glyphIndex);
    return error;
}

#define AVECTOR_BUG 1 /* Work around a bug in AVector fonts. */

private void ttfOutliner__DrawGlyphOutline(ttfOutliner *this, ttfGlyphOutline* out, FloatMatrix *m)
{   ttfFont *pFont = this->pFont;
    ttfExport *exp = this->exp;
    short* endP = out->endPoints;
    byte* onCurve = out->onCurve;
    F26Dot6* x = out->x;
    F26Dot6* y = out->y;
    F26Dot6 px, py;
    short sp, ctr;
    FloatPoint p0, p1, p2, p3;

#   if AVECTOR_BUG
	TExecution_Context *exec=pFont->exec;
	short xMinB = out->xMinB >> 6, xMaxB=out->xMaxB >> 6;
	short yMinB = out->yMinB >> 6, yMaxB=out->yMaxB >> 6;
	short expand=pFont->nUnitsPerEm*2;
	F26Dot6 xMin, xMax, yMin, yMax;

	xMinB -= expand;
	yMinB -= expand;
	xMaxB += expand;
	yMaxB += expand;
	xMin = Scale_X(&exec->metrics, xMinB);
	xMax = Scale_X(&exec->metrics, xMaxB);
	yMin = Scale_X(&exec->metrics, yMinB);
	yMax = Scale_X(&exec->metrics, yMaxB);
#   endif

    sp = -1;
    for (ctr = out->contourCount; ctr != 0; --ctr) {
	short pt, pts = *endP - sp;
	short ep = pts - 1;

	if (pts < 3) {
	    x += pts;
	    y += pts;
	    onCurve += pts;
	    sp = *endP++;
	    continue;   /* skip 1 and 2 point contours */
        }

	if (exp->bPoints) {
	    for (pt = 0; pt <= ep; pt++) {
		px = x[pt], py = y[pt];
#		if AVECTOR_BUG
		    if (x[pt] < xMin || xMax < x[pt] || y[pt] < yMin || yMax < y[pt]) {
			short prevIndex = pt == 0 ? ep : pt - 1;
			short nextIndex = pt == ep ? 0 : pt + 1;
			if (nextIndex > ep)
			    nextIndex = 0;
			px=AVE(x[prevIndex], x[nextIndex]);
			py=AVE(y[prevIndex], y[nextIndex]);
		    }
#		endif
		TransformF26Dot6PointFloat(&p0, px, py, m);
		exp->Point(exp, &p0, onCurve[pt], !pt);
            }
        }

	if (exp->bOutline) {
	    pt = 0;
	    if(onCurve[ep] & 1) {
		px = x[ep];
		py = y[ep];
            } else if (onCurve[0] & 1) {
		px = x[0];
		py = y[0];
		pt = 1;
            } else {
		px = AVE(x[0], x[ep]);
		py = AVE(y[0], y[ep]);
            }
	    this->ppx = px; this->ppy = py;
	    TransformF26Dot6PointFloat(&p0, px, py, m);
	    exp->MoveTo(exp, &p0);

	    for (; pt <= ep; pt++) {
		short prevIndex = pt == 0 ? ep : pt - 1;
		short nextIndex = pt == ep ? 0 : pt + 1;
		if (onCurve[pt] & 1) {
		    if (onCurve[prevIndex] & 1) {
			px = x[pt];
			py = y[pt];
			if (this->ppx != px || this->ppy != py) {
			    TransformF26Dot6PointFloat(&p1, px, py, m);
			    exp->LineTo(exp, &p1);
			    this->ppx = px; this->ppy = py;
			    p0 = p1;
                        }
                    }
                } else { 
		    F26Dot6 prevX, prevY, nextX, nextY;

		    px = x[pt];
		    py = y[pt];
#		    if AVECTOR_BUG
			if(x[pt] < xMin || xMax < x[pt] || y[pt] < yMin || yMax < y[pt]) {
			    px=AVE(x[prevIndex], x[nextIndex]);
			    py=AVE(y[prevIndex], y[nextIndex]);
			}
#		    endif
		    if (onCurve[prevIndex] & 1) {
			prevX = x[prevIndex];
			prevY = y[prevIndex];
                    } else {
			prevX = AVE(x[prevIndex], px);
			prevY = AVE(y[prevIndex], py);
                    }
		    if (onCurve[nextIndex] & 1) {
			nextX = x[nextIndex];
			nextY = y[nextIndex];
                    } else {
			nextX = AVE(px, x[nextIndex]);
			nextY = AVE(py, y[nextIndex]);
                    }
		    if (this->ppx != nextX || this->ppy != nextY) {
			double dx1, dy1, dx2, dy2, dx3, dy3;
			const double prec = 1e-6;

			TransformF26Dot6PointFloat(&p1, (prevX + (px << 1)) / 3, (prevY + (py << 1)) / 3, m);
			TransformF26Dot6PointFloat(&p2, (nextX + (px << 1)) / 3, (nextY + (py << 1)) / 3, m);
			TransformF26Dot6PointFloat(&p3, nextX, nextY, m);
			dx1 = p1.x - p0.x, dy1 = p1.y - p0.y;
			dx2 = p2.x - p0.x, dy2 = p2.y - p0.y;
			dx3 = p3.x - p0.x, dy3 = p3.y - p0.y;
			if (fabs(dx1 * dy3 - dy1 * dx3) > prec * fabs(dx1 * dx3 - dy1 * dy3) || 
			    fabs(dx2 * dy3 - dy2 * dx3) > prec * fabs(dx2 * dx3 - dy2 * dy3))
			    exp->CurveTo(exp, &p1, &p2, &p3);
			else
			    exp->LineTo(exp, &p3);
			this->ppx = nextX; this->ppy = nextY;
			p0 = p3;
                    }
                }
            }
	    exp->Close(exp);
        }
	x += pts;
	y += pts;
	onCurve += pts;
	sp = *endP++;
    }
}

FontError ttfOutliner__Outline(ttfOutliner *this, int glyphIndex, FloatMatrix *m1)
{   ttfFont *pFont = this->pFont;
    ttfExport *exp = this->exp;
    ttfGlyphOutline out;
    FontError error;
    FloatPoint p1;
    FloatMatrix m = *m1;

    ttfGlyphOutline__init(&out);
    this->nPointsTotal = 0;
    this->nContoursTotal = 0;
    out.advance.x = out.advance.y = 0;
    ttfFont__StartGlyph(pFont);
    error = ttfOutliner__BuildGlyphOutline(this, glyphIndex, &out);
    ttfFont__StopGlyph(pFont);
    if (pFont->nUnitsPerEm <= 0)
	pFont->nUnitsPerEm = 1024;
    m.a /= pFont->nUnitsPerEm;
    m.b /= pFont->nUnitsPerEm;
    m.c /= pFont->nUnitsPerEm;
    m.d /= pFont->nUnitsPerEm;
    TransformF26Dot6PointFloat(&p1, out.advance.x, out.advance.y, &m);
    exp->SetWidth(exp, &p1);
    if (error != fNoError)
	return error;
    if (this->bOutline)
	ttfOutliner__DrawGlyphOutline(this, &out, &m);
    /*
    else {
	FixPoint p0, p2, p3;

	TransformPointFix(&p0, 0, out.yMinG, &m1);
	TransformPointFix(&p1, out.advance.x, out.yMaxG, &m1);
	p2 = p0;
	p3 = p1;
	p2.x = p1.x;
	p3.y = p0.y;
	o.bOutline = TRUE;
	o.MoveTo(&p0);
	o.LineTo(&p1);
	o.LineTo(&p2);
	o.LineTo(&p3);
	o.Close();
	o.bOutline = bOutline;
    }
    */
    return fNoError;
}

