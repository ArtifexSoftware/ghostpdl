/* Copyright (C) 1992, 1995, 1997, 1999 Aladdin Enterprises.  All rights reserved.
  
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
/* A bridge to True Type interpreter. */

#include "gx.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "gxttfb.h"
#include "gxfixed.h"
#include "gxpath.h"
#include "gxfcache.h"
#include "ttfmemd.h"
#include "gsstruct.h"
#include "gserrors.h"
#include "gsfont.h"
#include "gdebug.h"
#include "memory_.h"
#include "math_.h"
#include <stdarg.h>

gs_public_st_composite(st_gx_ttfReader, gx_ttfReader,
    "gx_ttfReader", gx_ttfReader_enum_ptrs, gx_ttfReader_reloc_ptrs);

private 
ENUM_PTRS_WITH(gx_ttfReader_enum_ptrs, gx_ttfReader *mptr)
    {
	/* The field 'glyph_data' may contain pointers from global to local memory
	   ( see a comment in gxttfb.h).
	   They must be NULL when a garbager is invoked.
	   Due to that we don't enumerate and don't relocate them.
	if (index < 2 + ST_GLYPH_DATA_NUM_PTRS)
	    return ENUM_USING(st_glyph_data, &mptr->glyph_data, sizeof(mptr->glyph_data), index - 2);
	 */
	DISCARD(mptr);
	return 0;
    }
    ENUM_PTR(0, gx_ttfReader, pfont);
    ENUM_PTR(1, gx_ttfReader, memory);
ENUM_PTRS_END

private RELOC_PTRS_WITH(gx_ttfReader_reloc_ptrs, gx_ttfReader *mptr)
    RELOC_PTR(gx_ttfReader, pfont);
    RELOC_PTR(gx_ttfReader, memory);
    RELOC_USING(st_glyph_data, &mptr->glyph_data, sizeof(mptr->glyph_data));
RELOC_PTRS_END

private bool gx_ttfReader__Eof(ttfReader *this)
{
    gx_ttfReader *r = (gx_ttfReader *)this;

    if (r->extra_glyph_index != -1)
	return r->pos >= r->glyph_data.bits.size;
    /* We can't know whether pfont->data.string_proc has more bytes,
       so we never report Eof for it. */
    return false;
}

private void gx_ttfReader__Read(ttfReader *this, void *p, int n)
{
    gx_ttfReader *r = (gx_ttfReader *)this;
    const byte *q;

    if (!r->error) {
	if (r->extra_glyph_index != -1) {
	    q = r->glyph_data.bits.data + r->pos;
	    r->error = (r->glyph_data.bits.size - r->pos < n ? 
			    gs_note_error(gs_error_invalidfont) : 0);
	} else {
	    r->error = r->pfont->data.string_proc(r->pfont, (ulong)r->pos, (ulong)n, &q);
	    if (r->error > 0) {
		/* Need a loop with pfont->data.string_proc . Not implemented yet. */
		r->error = gs_note_error(gs_error_unregistered);
	    }
	}
    }
    if (r->error) {
	memset(p, 0, n);
	return;
    }
    memcpy(p, q, n);
    r->pos += n;
}

private void gx_ttfReader__Seek(ttfReader *this, int nPos)
{
    gx_ttfReader *r = (gx_ttfReader *)this;

    r->pos = nPos;
}

private int gx_ttfReader__Tell(ttfReader *this)
{
    gx_ttfReader *r = (gx_ttfReader *)this;

    return r->pos;
}

private bool gx_ttfReader__Error(ttfReader *this)
{
    gx_ttfReader *r = (gx_ttfReader *)this;

    return r->error;
}

private int gx_ttfReader__LoadGlyph(ttfReader *this, int glyph_index, const byte **p, int *size)
{
    gx_ttfReader *r = (gx_ttfReader *)this;
    gs_font_type42 *pfont = r->pfont;
    int code;

    if (r->extra_glyph_index != -1)
	return 0; /* We only maintain a single glyph buffer.
	             It's enough because ttfOutliner__BuildGlyphOutline
		     is optimized for that, and pfont->data.get_outline 
		     implements a charstring cache. */
    code = pfont->data.get_outline(pfont, (uint)glyph_index, &r->glyph_data);
    r->extra_glyph_index = glyph_index;
    r->pos = 0;
    if (code < 0)
	r->error = code;
    else if (code > 0) {
	/* Should not happen. */
	r->error = gs_note_error(gs_error_unregistered);
    } else {
	*p = r->glyph_data.bits.data;
	*size = r->glyph_data.bits.size;
    }
    return 2; /* found */
}

private void gx_ttfReader__ReleaseGlyph(ttfReader *this, int glyph_index)
{
    gx_ttfReader *r = (gx_ttfReader *)this;

    if (r->extra_glyph_index != glyph_index)
	return;
    r->extra_glyph_index = -1;
    gs_glyph_data_free(&r->glyph_data, "gx_ttfReader__ReleaseExtraGlyph");
}

private void gx_ttfReader__Reset(gx_ttfReader *this)
{
    if (this->extra_glyph_index != -1) {
	this->extra_glyph_index = -1;
	gs_glyph_data_free(&this->glyph_data, "gx_ttfReader__Reset");
    }
    this->error = false;
    this->pos = 0;
}

gx_ttfReader *gx_ttfReader__create(gs_memory_t *mem, gs_font_type42 *pfont)
{
    gx_ttfReader *r = gs_alloc_struct(mem, gx_ttfReader, &st_gx_ttfReader, "gx_ttfReader__create");

    if (r != NULL) {
	r->super.Eof = gx_ttfReader__Eof;
	r->super.Read = gx_ttfReader__Read;
	r->super.Seek = gx_ttfReader__Seek;
	r->super.Tell = gx_ttfReader__Tell;
	r->super.Error = gx_ttfReader__Error;
	r->super.LoadGlyph = gx_ttfReader__LoadGlyph;
	r->super.ReleaseGlyph = gx_ttfReader__ReleaseGlyph;
	r->pos = 0;
	r->error = false;
	r->extra_glyph_index = -1;
	memset(&r->glyph_data, 0, sizeof(r->glyph_data));
	r->pfont = pfont;
	r->memory = mem;
	gx_ttfReader__Reset(r);
    }
    return r;
}

void gx_ttfReader__destroy(gx_ttfReader *this)
{
    gs_free_object(this->memory, this, "gx_ttfReader__destroy");
}

/*----------------------------------------------*/

#if NEW_TT_INTERPRETER
private void DebugRepaint(ttfFont *ttf)
{
}

private void DebugPrint(ttfFont *ttf, const char *fmt, ...)
{
    char buf[500];
    va_list args;
    int count;

    if (gs_debug_c('Y')) {
	va_start(args, fmt);
	count = vsprintf(buf, fmt, args);
	outwrite(buf, count);
	va_end(args);
    }
}
#endif

private void WarnPatented(gs_font_type42 *pfont, ttfFont *ttf, const char *txt)
{
#if NEW_TT_INTERPRETER
    if (!ttf->design_grid) {
	char buf[gs_font_name_max + 1];
	int l;
	gs_font_type42 *base_font = pfont;

	while ((gs_font_type42 *)base_font->base != base_font)
	    base_font = (gs_font_type42 *)base_font->base;
	if (!base_font->data.warning_patented) {
	    l = min(sizeof(buf) - 1, base_font->font_name.size);
	    memcpy(buf, base_font->font_name.chars, l);
	    buf[l] = 0;
	    eprintf2("%s %s requires a patented True Type interpreter.\n", txt, buf);
	    base_font->data.warning_patented = true;
	}
    }
#endif
}

/*----------------------------------------------*/

typedef struct gx_ttfMemory_s {
    ttfMemory super;
    gs_memory_t *memory;
} gx_ttfMemory;

gs_private_st_simple(st_gx_ttfMemory, gx_ttfMemory, "gx_ttfMemory");
/* st_gx_ttfMemory::memory points to a root. */

#if NEW_TT_INTERPRETER 
private void *gx_ttfMemory__alloc_bytes(ttfMemory *this, int size,  const char *cname)
{
    gs_memory_t *mem = ((gx_ttfMemory *)this)->memory;

    return gs_alloc_bytes(mem, size, cname);
}

private void *gx_ttfMemory__alloc_struct(ttfMemory *this, const ttfMemoryDescriptor *d,  const char *cname)
{
    gs_memory_t *mem = ((gx_ttfMemory *)this)->memory;

    return mem->procs.alloc_struct(mem, (const gs_memory_struct_type_t *)d, cname);
}

private void gx_ttfMemory__free(ttfMemory *this, void *p,  const char *cname)
{
    gs_memory_t *mem = ((gx_ttfMemory *)this)->memory;

    gs_free_object(mem, p, cname);
}
#endif

/*----------------------------------------------*/

static inline float reminder(float v, int x)
{
    return ((v / x) - floor(v / x)) * x;
}

private void decompose_matrix(const gs_font_type42 *pfont, const gs_matrix * char_tm, 
    const gs_log2_scale_point *log2_scale, bool design_grid,
    gs_point *char_size, gs_point *subpix_origin, gs_matrix *post_transform, bool *dg)
{
    /* 
     *	char_tm maps to subpixels. 
     */
    /*
     *	We use a Free Type 1 True Type interpreter, which cannot perform
     *	a grid-fitting with skewing/rotation. It appears acceptable
     *	because we want to minimize invocations of patented instructions.
     *	We believe that skewing/rotation requires the patented intrivial cases
     *	of projection/freedom vectors.
     */
    int scale_x = 1 << log2_scale->x;
    int scale_y = 1 << log2_scale->y;
    bool atp = gs_currentaligntopixels(pfont->dir);
    bool design_grid1;

    char_size->x = hypot(char_tm->xx, char_tm->xy);
    char_size->y = hypot(char_tm->yx, char_tm->yy);
    if (char_size->x <= 2 && char_size->y <= 2) {
    	/* Disable the grid fitting for very small fonts. */
	design_grid1 = true;
    } else {
#if NEW_TT_INTERPRETER
	design_grid1 = design_grid || !gs_currentgridfittt(pfont->dir);
#else
	design_grid1 = design_grid; /* gs_currentgridfittt is undefined. */;
#endif
    }
    *dg = design_grid1;
    subpix_origin->x = (atp ? 0 : reminder(char_tm->tx, scale_x) / scale_x);
    subpix_origin->y = (atp ? 0 : reminder(char_tm->ty, scale_y) / scale_y);
    post_transform->xx = char_tm->xx / (design_grid1 ? 1 : char_size->x);
    post_transform->xy = char_tm->xy / (design_grid1 ? 1 : char_size->x);
    post_transform->yx = char_tm->yx / (design_grid1 ? 1 : char_size->y);
    post_transform->yy = char_tm->yy / (design_grid1 ? 1 : char_size->y);
    post_transform->tx = char_tm->tx - subpix_origin->x;
    post_transform->ty = char_tm->ty - subpix_origin->y;
}

/*----------------------------------------------*/

ttfFont *ttfFont__create(gs_font_dir *dir)
{
#if NEW_TT_INTERPRETER 
    gs_memory_t *mem = dir->memory;
    gx_ttfMemory *m = gs_alloc_struct(mem, gx_ttfMemory, &st_gx_ttfMemory, "ttfFont__create");
    ttfFont *ttf;

    if (!m)
	return 0;
    m->super.alloc_struct = gx_ttfMemory__alloc_struct;
    m->super.alloc_bytes = gx_ttfMemory__alloc_bytes;
    m->super.free = gx_ttfMemory__free;
    m->memory = mem;
    if(ttfInterpreter__obtain(&m->super, &dir->tti))
	return 0;
    ttf = gs_alloc_struct(mem, ttfFont, &st_ttfFont, "ttfFont__create");
    if (ttf == NULL)
	return 0;
    ttfFont__init(ttf, &m->super, DebugRepaint, DebugPrint);
    return ttf;
#else
    return 0;
#endif
}

void ttfFont__destroy(ttfFont *this, gs_font_dir *dir)
{   
#if NEW_TT_INTERPRETER 
    ttfMemory *mem = this->tti->ttf_memory;

    ttfFont__finit(this);
    mem->free(mem, this, "ttfFont__destroy");
    ttfInterpreter__release(&dir->tti);
#endif
}

int ttfFont__Open_aux(ttfFont *this, ttfInterpreter *tti, gx_ttfReader *r, gs_font_type42 *pfont,
    	       const gs_matrix * char_tm, const gs_log2_scale_point *log2_scale,
	       bool design_grid)
{
#if NEW_TT_INTERPRETER 
    gs_point char_size, subpix_origin;
    gs_matrix post_transform;
    /* 
     * Ghostscript proceses a TTC index in gs/lib/gs_ttf.ps, 
     * and *pfont already adjusted to it.
     * Therefore TTC headers never comes here. 
     */
    unsigned int nTTC = 0; 
    bool dg;

    decompose_matrix(pfont, char_tm, log2_scale, design_grid, &char_size, &subpix_origin, &post_transform, &dg);
    switch(ttfFont__Open(tti, this, &r->super, nTTC, char_size.x, char_size.y, dg)) {
	case fNoError:
	    return 0;
	case fMemoryError:
	    return_error(gs_error_VMerror);
	case fUnimplemented:
	    return_error(gs_error_unregistered);
	case fPatented:
	    WarnPatented(pfont, this, "The font");
	    this->patented = true;
	    return 0;
	default:
	    {	int code = r->super.Error(&r->super);

		if (code < 0)
		    return code;
		return_error(gs_error_invalidfont);
	    }
    }
#else
    return 0;
#endif
}

/*----------------------------------------------*/

typedef struct gx_ttfExport_s {
    ttfExport super;
    gx_path *path;
    gs_fixed_point w;
    int error;
} gx_ttfExport;

private void gx_ttfExport__MoveTo(ttfExport *this, FloatPoint *p)
{
    gx_ttfExport *e = (gx_ttfExport *)this;

    if (!e->error)
	e->error = gx_path_add_point(e->path, float2fixed(p->x), float2fixed(p->y));
}

private void gx_ttfExport__LineTo(ttfExport *this, FloatPoint *p)
{
    gx_ttfExport *e = (gx_ttfExport *)this;

    if (!e->error)
	e->error = gx_path_add_line_notes(e->path, float2fixed(p->x), float2fixed(p->y), sn_none);
}

private void gx_ttfExport__CurveTo(ttfExport *this, FloatPoint *p0, FloatPoint *p1, FloatPoint *p2)
{
    gx_ttfExport *e = (gx_ttfExport *)this;

    if (!e->error)
	e->error = gx_path_add_curve_notes(e->path, float2fixed(p0->x), float2fixed(p0->y), 
				     float2fixed(p1->x), float2fixed(p1->y), 
				     float2fixed(p2->x), float2fixed(p2->y), sn_none);
}

private void gx_ttfExport__Close(ttfExport *this)
{
    gx_ttfExport *e = (gx_ttfExport *)this;

    if (!e->error)
	e->error = gx_path_close_subpath_notes(e->path, sn_none);
}

private void gx_ttfExport__Point(ttfExport *this, FloatPoint *p, bool bOnCurve, bool bNewPath)
{
    /* Never called. */
}

private void gx_ttfExport__SetWidth(ttfExport *this, FloatPoint *p)
{
    gx_ttfExport *e = (gx_ttfExport *)this;

    e->w.x = float2fixed(p->x); 
    e->w.y = float2fixed(p->y); 
}

private void gx_ttfExport__DebugPaint(ttfExport *this)
{
}

/*----------------------------------------------*/

int gx_ttf_outline(ttfFont *ttf, gx_ttfReader *r, gs_font_type42 *pfont, int glyph_index, 
	const gs_matrix *m, const gs_log2_scale_point *pscale, 
	gx_path *path, bool design_grid)
{
    gx_ttfExport e;
    ttfOutliner o;
    gs_point char_size, subpix_origin;
    gs_matrix post_transform;
    /* Ghostscript proceses a TTC index in gs/lib/gs_ttf.ps, */
    /* so that TTC never comes here. */
    FloatMatrix m1;
    bool dg;

    decompose_matrix(pfont, m, pscale, design_grid, &char_size, &subpix_origin, &post_transform, &dg);
    m1.a = post_transform.xx;
    m1.b = post_transform.xy;
    m1.c = post_transform.yx;
    m1.d = post_transform.yy;
    m1.tx = post_transform.tx;
    m1.ty = post_transform.ty;
    e.super.bPoints = false;
    e.super.bOutline = true;
    e.super.MoveTo = gx_ttfExport__MoveTo;
    e.super.LineTo = gx_ttfExport__LineTo;
    e.super.CurveTo = gx_ttfExport__CurveTo;
    e.super.Close = gx_ttfExport__Close;
    e.super.Point = gx_ttfExport__Point;
    e.super.SetWidth = gx_ttfExport__SetWidth;
    e.super.DebugPaint = gx_ttfExport__DebugPaint;
    e.error = 0;
    e.path = path;
    e.w.x = 0;
    e.w.y = 0;
    gx_ttfReader__Reset(r);
    ttfOutliner__init(&o, ttf, &r->super, &e.super, true, false, pfont->WMode != 0);
    switch(ttfOutliner__Outline(&o, glyph_index, 
	    subpix_origin.x, subpix_origin.y, &m1)) {
	case fNoError:
	    return 0;
	case fMemoryError:
	    return_error(gs_error_VMerror);
	case fUnimplemented:
	    return_error(gs_error_unregistered);
	case fPatented:
	    /* The returned outline did not apply a bytecode (it is not grid-fitted). */
	    WarnPatented(pfont, ttf, "Some glyphs of the font");
	    return 0;
	default:
	    {	int code = r->super.Error(&r->super);

		if (code < 0)
		    return code;
		return_error(gs_error_invalidfont);
	    }
    }
}
