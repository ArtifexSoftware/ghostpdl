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
#include "ttfmemd.h"
#include "gsstruct.h"
#include "gserrors.h"
#include "memory_.h"

gs_public_st_composite(st_gx_ttfReader, gx_ttfReader,
    "gx_ttfReader", gx_ttfReader_enum_ptrs, gx_ttfReader_reloc_ptrs);

private 
ENUM_PTRS_WITH(gx_ttfReader_enum_ptrs, gx_ttfReader *mptr)
    {
	return ENUM_USING(st_glyph_data, &mptr->glyph_data, sizeof(mptr->glyph_data), index - 2);
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

    return r->error;
}

private void gx_ttfReader__Read(ttfReader *this, void *p, int n)
{
    gx_ttfReader *r = (gx_ttfReader *)this;
    const byte *q;

    if (r->glyph_data_occupied) {
	q = r->glyph_data.bits.bytes + r->pos;
	r->error = (r->glyph_data.bits.size - r->pos < n);
    } else
	r->error = !!r->pfont->data.string_proc(r->pfont, (ulong)r->pos, (ulong)n, &q);
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

private bool gx_ttfReader__SeekExtraGlyph(ttfReader *this, int glyph_index)
{
    gx_ttfReader *r = (gx_ttfReader *)this;
    gs_font_type42 *pfont = r->pfont;
    int code;

    if (r->glyph_data_occupied)
	return true;

    code = pfont->data.get_outline(pfont, (uint)glyph_index, &r->glyph_data);
    r->glyph_data_occupied = true;
    r->pos = 0;
    if (code)
	r->error = true;
    return false;
}

private void gx_ttfReader__ReleaseExtraGlyph(ttfReader *this)
{
    gx_ttfReader *r = (gx_ttfReader *)this;

    if (!r->glyph_data_occupied)
	return;
    r->glyph_data_occupied = false;
    gs_glyph_data_free(&r->glyph_data, "gx_ttfReader__ReleaseExtraGlyph");
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
	r->super.SeekExtraGlyph = gx_ttfReader__SeekExtraGlyph;
	r->super.ReleaseExtraGlyph = gx_ttfReader__ReleaseExtraGlyph;
	r->pos = 0;
	r->error = false;
	r->glyph_data_occupied = false;
	r->pfont = pfont;
	r->memory = mem;
    }
    return r;
}

void gx_ttfReader__destroy(gx_ttfReader *this)
{
    gs_free_object(this->memory, this, "gx_ttfReader__destroy");
}

/*----------------------------------------------*/

private void DebugRepaint(ttfFont *ttf)
{
}

private void DebugPrint(ttfFont *ttf, const char *s, ...)
{
}

/*----------------------------------------------*/

typedef struct gx_ttfMemory_s {
    ttfMemory super;
    gs_memory_t *memory;
} gx_ttfMemory;

gs_private_st_ptrs1(st_gx_ttfMemory, gx_ttfMemory, "gx_ttfMemory", 
    gx_ttfMemory_enum_ptrs, gx_ttfMemory_reloc_ptrs, 
    memory);

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

/*----------------------------------------------*/

ttfFont *ttfFont__create(gs_memory_t *mem)
{
    gx_ttfMemory *m = gs_alloc_struct(mem, gx_ttfMemory, &st_gx_ttfMemory, "ttfFont__create");
    ttfFont *ttf;

    if (!m)
	return 0;
    m->super.alloc_struct = gx_ttfMemory__alloc_struct;
    m->super.alloc_bytes = gx_ttfMemory__alloc_bytes;
    m->super.free = gx_ttfMemory__free;
    m->memory = mem;
    ttf = gs_alloc_struct(mem, ttfFont, &st_ttfFont, "ttfFont__create");
    if (ttf == NULL)
	return 0;
    ttfFont__init(ttf, &m->super, DebugRepaint, DebugPrint);
    return ttf;
}

void ttfFont__destroy(ttfFont *this)
{   ttfMemory *mem = this->memory;

    ttfFont__finit(this);
    mem->free(mem, this, "ttfFont__destroy");
    mem->free(mem, mem, "ttfFont__destroy");
}

int ttfFont__Open_aux(ttfFont *this, ttfReader *r, unsigned int nTTC)
{
    switch(ttfFont__Open(this, r, nTTC)) {
	case fNoError:
	    return 0;
	case fMemoryError:
	    return_error(gs_error_VMerror);
	case fUnimplemented:
	    return_error(gs_error_unregistered);
	default:
	    return_error(gs_error_invalidfont);
    }
}
