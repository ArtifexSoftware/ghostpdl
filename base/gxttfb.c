/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* A bridge to True Type interpreter. */

#include "gx.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "gxttfb.h"
#include "gxfixed.h"
#include "gxpath.h"
#include "gxfcache.h"
#include "gxmatrix.h"
#include "gxhintn.h"
#include "gzpath.h"
#include "ttfmemd.h"
#include "gsstruct.h"
#include "gserrors.h"
#include "gsfont.h"
#include "gdebug.h"
#include "memory_.h"
#include "math_.h"
#include "gxgstate.h"
#include "gxpaint.h"
#include "gzspotan.h"
#include <stdarg.h>

gs_public_st_composite(st_gx_ttfReader, gx_ttfReader,
    "gx_ttfReader", gx_ttfReader_enum_ptrs, gx_ttfReader_reloc_ptrs);

static
ENUM_PTRS_WITH(gx_ttfReader_enum_ptrs, gx_ttfReader *mptr)
    {
        /* The fields 'pfont' and 'glyph_data' may contain pointers from global
           to local memory ( see a comment in gxttfb.h).
           They must be NULL when a garbager is invoked.
           Due to that we don't enumerate and don't relocate them.
         */
        DISCARD(mptr);
        return 0;
    }
    ENUM_PTR(0, gx_ttfReader, memory);
ENUM_PTRS_END

static RELOC_PTRS_WITH(gx_ttfReader_reloc_ptrs, gx_ttfReader *mptr)
    DISCARD(mptr);
    RELOC_PTR(gx_ttfReader, memory);
RELOC_PTRS_END

static bool gx_ttfReader__Eof(ttfReader *self)
{
    gx_ttfReader *r = (gx_ttfReader *)self;

    if (r->extra_glyph_index != -1)
        return r->pos >= r->glyph_data.bits.size;
    /* We can't know whether pfont->data.string_proc has more bytes,
       so we never report Eof for it. */
    return false;
}

static void gx_ttfReader__Read(ttfReader *self, void *p, int n)
{
    gx_ttfReader *r = (gx_ttfReader *)self;
    const byte *q;

    if (r->error >= 0) {
        if (r->extra_glyph_index != -1) {
            q = r->glyph_data.bits.data + r->pos;
            r->error = ((r->pos >= r->glyph_data.bits.size ||
                        r->glyph_data.bits.size - r->pos < n) ?
                            gs_note_error(gs_error_invalidfont) : 0);
            if (r->error == 0)
                memcpy(p, q, n);
        } else {
            unsigned int cnt;
            r->error = 0;

            for (cnt = 0; cnt < (uint)n; cnt += r->error) {
                r->error = r->pfont->data.string_proc(r->pfont, (ulong)r->pos + cnt, (ulong)n - cnt, &q);
                if (r->error < 0)
                    break;
                else if ( r->error == 0) {
                    memcpy((char *)p + cnt, q, n - cnt);
                    break;
                } else {
                    memcpy((char *)p + cnt, q, r->error);
                }
            }
        }
    }
    if (r->error < 0) {
        memset(p, 0, n);
        return;
    }
    r->pos += n;
}

static void gx_ttfReader__Seek(ttfReader *self, int nPos)
{
    gx_ttfReader *r = (gx_ttfReader *)self;

    r->pos = nPos;
}

static int gx_ttfReader__Tell(ttfReader *self)
{
    gx_ttfReader *r = (gx_ttfReader *)self;

    return r->pos;
}

static bool gx_ttfReader__Error(ttfReader *self)
{
    gx_ttfReader *r = (gx_ttfReader *)self;

    return r->error;
}

static int gx_ttfReader__LoadGlyph(ttfReader *self, int glyph_index, const byte **p, int *size)
{
    gx_ttfReader *r = (gx_ttfReader *)self;
    gs_font_type42 *pfont = r->pfont;
    int code;

    if (r->extra_glyph_index != -1)
        return 0; /* We only maintain a single glyph buffer.
                     It's enough because ttfOutliner__BuildGlyphOutline
                     is optimized for that, and pfont->data.get_outline
                     implements a charstring cache. */
    r->glyph_data.memory = pfont->memory;
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

static void gx_ttfReader__ReleaseGlyph(ttfReader *self, int glyph_index)
{
    gx_ttfReader *r = (gx_ttfReader *)self;

    if (r->extra_glyph_index != glyph_index)
        return;
    r->extra_glyph_index = -1;
    gs_glyph_data_free(&r->glyph_data, "gx_ttfReader__ReleaseExtraGlyph");
}

static void gx_ttfReader__Reset(gx_ttfReader *self)
{
    if (self->extra_glyph_index != -1) {
        self->extra_glyph_index = -1;
        gs_glyph_data_free(&self->glyph_data, "gx_ttfReader__Reset");
    }
    self->error = 0;
    self->pos = 0;
}

gx_ttfReader *gx_ttfReader__create(gs_memory_t *mem)
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
        r->error = 0;
        r->extra_glyph_index = -1;
        memset(&r->glyph_data, 0, sizeof(r->glyph_data));
        r->pfont = NULL;
        r->memory = mem;
        gx_ttfReader__Reset(r);
    }
    return r;
}

void gx_ttfReader__destroy(gx_ttfReader *self)
{
    gs_free_object(self->memory, self, "gx_ttfReader__destroy");
}

static int
gx_ttfReader__default_get_metrics(const ttfReader *ttf, uint glyph_index, bool bVertical,
                                  short *sideBearing, unsigned short *nAdvance)
{
    gx_ttfReader *self = (gx_ttfReader *)ttf;
    float sbw[4];
    int sbw_offset = bVertical;
    int code;
    int factor = self->pfont->data.unitsPerEm;

    code = self->pfont->data.get_metrics(self->pfont, glyph_index, bVertical, sbw);
    if (code < 0)
        return code;
    /* Due to an obsolete convention, simple_glyph_metrics scales
       the metrics into 1x1 rectangle as Postscript like.
       In same time, the True Type interpreter needs
       the original design units.
       Undo the scaling here with accurate rounding. */
    *sideBearing = (short)floor(sbw[0 + sbw_offset] * factor + 0.5);
    *nAdvance = (short)floor(sbw[2 + sbw_offset] * factor + 0.5);
    return 0;
}

void gx_ttfReader__set_font(gx_ttfReader *self, gs_font_type42 *pfont)
{
    self->pfont = pfont;
    self->super.get_metrics = gx_ttfReader__default_get_metrics;
}

/*----------------------------------------------*/

static void DebugRepaint(ttfFont *ttf)
{
}

#ifdef DEBUG
static int DebugPrint(ttfFont *ttf, const char *fmt, ...)
{
    char buf[500];
    va_list args;
    int count;

    if (gs_debug_c('Y')) {
        va_start(args, fmt);
        count = vsnprintf(buf, sizeof(buf), fmt, args);
        /* NB: moved debug output from stdout to stderr
         */
        errwrite(ttf->DebugMem, buf, count);
        va_end(args);
    }
    return 0;
}
#endif

static void WarnBadInstruction(gs_font_type42 *pfont, int glyph_index)
{
    char buf[gs_font_name_max + 1];
    int l;
    gs_font_type42 *base_font = pfont;

    while ((gs_font_type42 *)base_font->base != base_font)
        base_font = (gs_font_type42 *)base_font->base;
    if (!base_font->data.warning_bad_instruction) {
        l = min(sizeof(buf) - 1, base_font->font_name.size);
        memcpy(buf, base_font->font_name.chars, l);
        buf[l] = 0;
        if (glyph_index >= 0)
            emprintf2(pfont->memory,
                      "Failed to interpret TT instructions for glyph index %d of font %s. "
                      "Continue ignoring instructions of the font.\n",
                      glyph_index, buf);
        else
            emprintf1(pfont->memory,
                      "Failed to interpret TT instructions in font %s. "
                      "Continue ignoring instructions of the font.\n",
                      buf);
        base_font->data.warning_bad_instruction = true;
    }
}

static void WarnPatented(gs_font_type42 *pfont, ttfFont *ttf, const char *txt)
{
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
            emprintf2(pfont->memory,
                      "%s %s requires a patented True Type interpreter.\n",
                      txt,
                      buf);
            base_font->data.warning_patented = true;
        }
    }
}

/*----------------------------------------------*/

struct gx_ttfMemory_s {
    ttfMemory super;
    gs_memory_t *memory;
};

gs_private_st_simple(st_gx_ttfMemory, gx_ttfMemory, "gx_ttfMemory");
/* st_gx_ttfMemory::memory points to a root. */

static void *gx_ttfMemory__alloc_bytes(ttfMemory *self, int size,  const char *cname)
{
    gs_memory_t *mem = ((gx_ttfMemory *)self)->memory;

    return gs_alloc_bytes(mem, size, cname);
}

static void *gx_ttfMemory__alloc_struct(ttfMemory *self, const ttfMemoryDescriptor *d,  const char *cname)
{
    gs_memory_t *mem = ((gx_ttfMemory *)self)->memory;

    return mem->procs.alloc_struct(mem, (const gs_memory_struct_type_t *)d, cname);
}

static void gx_ttfMemory__free(ttfMemory *self, void *p,  const char *cname)
{
    gs_memory_t *mem = ((gx_ttfMemory *)self)->memory;

    gs_free_object(mem, p, cname);
}

/*----------------------------------------------*/

static inline float reminder(float v, int x)
{
    return ((v / x) - floor(v / x)) * x;
}

static void decompose_matrix(const gs_font_type42 *pfont, const gs_matrix * char_tm,
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
    } else
        design_grid1 = design_grid || !(gs_currentgridfittt(pfont->dir) & 1);
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
    gs_memory_t *mem = dir->memory->stable_memory;
    ttfFont *ttf;

    if (dir->ttm == NULL) {
        gx_ttfMemory *m = gs_alloc_struct(mem, gx_ttfMemory, &st_gx_ttfMemory, "ttfFont__create(gx_ttfMemory)");

        if (!m)
            return 0;
        m->super.alloc_struct = gx_ttfMemory__alloc_struct;
        m->super.alloc_bytes = gx_ttfMemory__alloc_bytes;
        m->super.free = gx_ttfMemory__free;
        m->memory = mem;
        dir->ttm = m;
    }
    if(ttfInterpreter__obtain(&dir->ttm->super, &dir->tti))
        return 0;
    if(gx_san__obtain(mem, &dir->san))
        return 0;
    ttf = gs_alloc_struct(mem, ttfFont, &st_ttfFont, "ttfFont__create");
    if (ttf == NULL)
        return 0;
#ifdef DEBUG
    ttfFont__init(ttf, &dir->ttm->super, DebugRepaint,
                  (gs_debug_c('Y') ? DebugPrint : NULL), mem);
#else
    ttfFont__init(ttf, &dir->ttm->super, DebugRepaint, NULL, mem);
#endif

    return ttf;
}

void ttfFont__destroy(ttfFont *self, gs_font_dir *dir)
{
    gs_memory_t *mem = dir->memory->stable_memory;

    ttfFont__finit(self);
    gs_free_object(mem, self, "ttfFont__destroy");
    ttfInterpreter__release(&dir->tti);
    gx_san__release(&dir->san);
    if (dir->tti == NULL && dir->ttm != NULL) {
        gs_free_object(mem, dir->ttm, "ttfFont__destroy(gx_ttfMemory)");
        dir->ttm = NULL;
    }
}

int ttfFont__Open_aux(ttfFont *self, ttfInterpreter *tti, gx_ttfReader *r, gs_font_type42 *pfont,
               const gs_matrix * char_tm, const gs_log2_scale_point *log2_scale,
               bool design_grid)
{
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
    switch(ttfFont__Open(tti, self, &r->super, nTTC, char_size.x, char_size.y, dg)) {
        case fNoError:
            return 0;
        case fMemoryError:
            return_error(gs_error_VMerror);
        case fUnimplemented:
            return_error(gs_error_unregistered);
        case fBadInstruction:
            WarnBadInstruction(pfont, -1);
            goto recover;
        case fPatented:
            WarnPatented(pfont, self, "The font");
        recover:
            self->patented = true;
            return 0;
        default:
            {	int code = r->super.Error(&r->super);

                if (code < 0)
                    return code;
                return_error(gs_error_invalidfont);
            }
    }
}

/*----------------------------------------------*/

typedef struct gx_ttfExport_s {
    ttfExport super;
    gx_path *path;
    gs_fixed_point w;
    int error;
    bool monotonize;
} gx_ttfExport;

static void gx_ttfExport__MoveTo(ttfExport *self, FloatPoint *p)
{
    gx_ttfExport *e = (gx_ttfExport *)self;

    if (e->error >= 0)
        e->error = gx_path_add_point(e->path, float2fixed(p->x), float2fixed(p->y));
}

static void gx_ttfExport__LineTo(ttfExport *self, FloatPoint *p)
{
    gx_ttfExport *e = (gx_ttfExport *)self;

    if (e->error >= 0)
        e->error = gx_path_add_line_notes(e->path, float2fixed(p->x), float2fixed(p->y), sn_none);
}

static void gx_ttfExport__CurveTo(ttfExport *self, FloatPoint *p0, FloatPoint *p1, FloatPoint *p2)
{
    gx_ttfExport *e = (gx_ttfExport *)self;

    if (e->error >= 0) {
        if (e->monotonize) {
            curve_segment s;

            s.notes = sn_none;
            s.p1.x = float2fixed(p0->x), s.p1.y = float2fixed(p0->y),
            s.p2.x = float2fixed(p1->x), s.p2.y = float2fixed(p1->y),
            s.pt.x = float2fixed(p2->x), s.pt.y = float2fixed(p2->y);
            e->error = gx_curve_monotonize(e->path, &s);
        } else
            e->error = gx_path_add_curve_notes(e->path, float2fixed(p0->x), float2fixed(p0->y),
                                     float2fixed(p1->x), float2fixed(p1->y),
                                     float2fixed(p2->x), float2fixed(p2->y), sn_none);
    }
}

static void gx_ttfExport__Close(ttfExport *self)
{
    gx_ttfExport *e = (gx_ttfExport *)self;

    if (e->error >= 0)
        e->error = gx_path_close_subpath_notes(e->path, sn_none);
}

static void gx_ttfExport__Point(ttfExport *self, FloatPoint *p, bool bOnCurve, bool bNewPath)
{
    /* Never called. */
}

static void gx_ttfExport__SetWidth(ttfExport *self, FloatPoint *p)
{
    gx_ttfExport *e = (gx_ttfExport *)self;

    e->w.x = float2fixed(p->x);
    e->w.y = float2fixed(p->y);
}

static void gx_ttfExport__DebugPaint(ttfExport *self)
{
}

/*----------------------------------------------*/

static int
path_to_hinter(t1_hinter *h, gx_path *path)
{   int code;
    gs_path_enum penum;
    gs_fixed_point pts[3];
    gs_fixed_point p = {0, 0}; /* initialize to avoid a warning */
    bool first = true;
    int op;

    code = gx_path_enum_init(&penum, path);
    if (code < 0)
        return code;
    while ((op = gx_path_enum_next(&penum, pts)) != 0) {
        switch (op) {
            case gs_pe_moveto:
                if (first) {
                    first = false;
                    p = pts[0];
                    code = t1_hinter__rmoveto(h, p.x, p.y);
                } else
                    code = t1_hinter__rmoveto(h, pts[0].x - p.x, pts[0].y - p.y);
                break;
            case gs_pe_lineto:
            case gs_pe_gapto:
                code = t1_hinter__rlineto(h, pts[0].x - p.x, pts[0].y - p.y);
                break;
            case gs_pe_curveto:
                code = t1_hinter__rcurveto(h, pts[0].x - p.x, pts[0].y - p.y,
                                        pts[1].x - pts[0].x, pts[1].y - pts[0].y,
                                        pts[2].x - pts[1].x, pts[2].y - pts[1].y);
                pts[0] = pts[2];
                break;
            case gs_pe_closepath:
                code = t1_hinter__closepath(h);
                break;
            default:
                return_error(gs_error_unregistered);
        }
        if (code < 0)
            return code;
        p = pts[0];
    }
    return 0;
}

#define exch(a,b) a^=b; b^=a; a^=b;

static void
transpose_path(gx_path *path)
{   segment *s = (segment *)path->first_subpath;

    exch(path->bbox.p.x, path->bbox.p.y);
    exch(path->bbox.q.x, path->bbox.q.y);
    for (; s; s = s->next) {
        if (s->type == s_curve) {
            curve_segment *c = (curve_segment *)s;

            exch(c->p1.x, c->p1.y);
            exch(c->p2.x, c->p2.y);
        }
        exch(s->pt.x, s->pt.y);
    }
}

typedef struct {
    t1_hinter super;
    int transpose;
    fixed midx;
} t1_hinter_aux;

static int
stem_hint_handler(void *client_data, gx_san_sect *ss)
{
    t1_hinter_aux *h = (t1_hinter_aux *)client_data;

    if (ss->side_mask == 3) {
        /* Orient horizontal hints to help with top/bottom alignment zones.
           Otherwize glyphs may get a random height due to serif adjustsment. */
        if (ss->xl > h->midx && h->transpose)
            return (h->transpose ? t1_hinter__hstem : t1_hinter__vstem)
                        (&h->super, ss->xr, ss->xl - ss->xr);
        else
            return (h->transpose ? t1_hinter__hstem : t1_hinter__vstem)
                        (&h->super, ss->xl, ss->xr - ss->xl);
    } else
        return t1_hinter__overall_hstem(&h->super, ss->xl, ss->xr - ss->xl, ss->side_mask);
}

#define OVERALL_HINT 0 /* Overall hints help to emulate Type 1 alignment zones
                          (except for overshoot suppression.)
                          For example, without it comparefiles/type42_glyph_index.ps
                          some glyphs have different height due to
                          serifs are aligned in same way as horizontal stems,
                          but both sides of a stem have same priority.

                          This stuff appears low useful, because horizontal
                          hint orientation performs this job perfectly.
                          fixme : remove.
                          fixme : remove side_mask from gxhintn.c .
                          */

static int grid_fit(gx_device_spot_analyzer *padev, gx_path *path,
        gs_font_type42 *pfont, const gs_log2_scale_point *pscale, gx_ttfExport *e, ttfOutliner *o)
{
    /* Not completed yet. */
    gs_gstate gs_stub;
    gx_fill_params params;
    gx_device_color devc_stub;
    int code;
    t1_hinter_aux h;
    gs_matrix m, fm, fmb;
    gs_matrix_fixed ctm_temp;
    bool atp = gs_currentaligntopixels(pfont->dir);
    int FontType = 1; /* Will apply Type 1 hinter. */
    fixed sbx = 0, sby = 0; /* stub */
    double scale = 1.0 / o->pFont->nUnitsPerEm;
    gs_fixed_rect bbox;

    m.xx = o->post_transform.a;
    m.xy = o->post_transform.b;
    m.yx = o->post_transform.c;
    m.yy = o->post_transform.d;
    m.tx = o->post_transform.tx;
    m.ty = o->post_transform.ty;
    code = gs_matrix_fixed_from_matrix(&ctm_temp, &m);
    if (code < 0)
        return code;
    code = gs_matrix_scale(&pfont->FontMatrix, scale, scale, &fm);
    if (code < 0)
        return code;
    code = gs_matrix_scale(&pfont->base->FontMatrix, scale, scale, &fmb);
    if (code < 0)
        return code;
    t1_hinter__init(&h.super, path); /* Will export to */
    code = t1_hinter__set_mapping(&h.super, &ctm_temp,
                        &fm, &fmb,
                        pscale->x, pscale->x, 0, 0,
                        ctm_temp.tx_fixed, ctm_temp.ty_fixed, atp);
    if (code < 0)
        return code;
    if (!h.super.disable_hinting) {
        o->post_transform.a = o->post_transform.d = 1;
        o->post_transform.b = o->post_transform.c = 0;
        o->post_transform.tx = o->post_transform.ty = 0;
        ttfOutliner__DrawGlyphOutline(o);
        if (e->error < 0)
            return e->error;
        code = t1_hinter__set_font42_data(&h.super, FontType, &pfont->data, false);
        if (code < 0)
            return code;
        code = t1_hinter__sbw(&h.super, sbx, sby, e->w.x, e->w.y);
        if (code < 0)
            return code;
        code = gx_path_bbox(path, &bbox);
        if (code < 0)
            return code;
        memset(&gs_stub, 0, sizeof(gs_stub));
        gs_stub.memory = padev->memory;
        set_nonclient_dev_color(&devc_stub, 1);
        params.rule = gx_rule_winding_number;
        params.adjust.x = params.adjust.y = 0;
        params.flatness = fixed2float(max(bbox.q.x - bbox.p.x, bbox.q.y - bbox.p.y)) / 100.0;

        for (h.transpose = 0; h.transpose < 2; h.transpose++) {
            h.midx = (padev->xmin + padev->xmax) / 2;
            if (h.transpose)
                transpose_path(path);
            gx_san_begin(padev);
            code = dev_proc(padev, fill_path)((gx_device *)padev,
                            &gs_stub, path, &params, &devc_stub, NULL);
            gx_san_end(padev);
            if (code >= 0)
                code = gx_san_generate_stems(padev, OVERALL_HINT && h.transpose,
                                &h, stem_hint_handler);
            if (h.transpose)
                transpose_path(path);
            if (code < 0)
                return code;
        }

        /*  fixme : Storing hints permanently would be useful.
            Note that if (gftt & 1), the outline and hints are already scaled.
        */
        code = path_to_hinter(&h.super, path);
        if (code < 0)
            return code;
        code = gx_path_new(path);
        if (code < 0)
            return code;
        code = t1_hinter__endglyph(&h.super);
    } else {
        ttfOutliner__DrawGlyphOutline(o);
        if (e->error < 0)
            return e->error;
    }
    return code;
}

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
    uint gftt = gs_currentgridfittt(pfont->dir);
    bool ttin = (gftt & 1);
    /*	gs_currentgridfittt values (binary) :
        00 - no grid fitting;
        01 - Grid fit with TT interpreter; On failure warn and render unhinted.
        10 - Interpret in the design grid and then autohint.
        11 - Grid fit with TT interpreter; On failure render autohinted.
    */
    bool auth = (gftt & 2);

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
    e.monotonize = auth;
    gx_ttfReader__Reset(r);
    ttfOutliner__init(&o, ttf, &r->super, &e.super, true, false, pfont->WMode != 0);
    switch(ttfOutliner__Outline(&o, glyph_index, subpix_origin.x, subpix_origin.y, &m1)) {
        case fBadInstruction:
            WarnBadInstruction(pfont, glyph_index);
            goto recover;
        case fPatented:
            /* The returned outline did not apply a bytecode (it is not grid-fitted). */
            if (!auth)
                WarnPatented(pfont, ttf, "Some glyphs of the font");
        recover :
            if (!design_grid && auth)
                return grid_fit(pfont->dir->san, path, pfont, pscale, &e, &o);
            /* Falls through. */
        case fNoError:
            if (!design_grid && !ttin && auth)
                return grid_fit(pfont->dir->san, path, pfont, pscale, &e, &o);
            ttfOutliner__DrawGlyphOutline(&o);
            if (e.error)
                return e.error;
            return 0;
        case fMemoryError:
            return_error(gs_error_VMerror);
        case fUnimplemented:
            return_error(gs_error_unregistered);
        default:
            {	int code = r->super.Error(&r->super);

                if (code < 0)
                    return code;
                return_error(gs_error_invalidfont);
            }
    }
}
