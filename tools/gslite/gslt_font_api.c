/* Copyright (C) 2006 artofcode LLC.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id: $ */
/* gslt OpenType font library implementation */

/*
 * gslt OpenType font API
 */

/* No shortage of gslib headers to include... */

#include <stdlib.h>
#include "stdio_.h"
#include "math_.h"
#include "string_.h"

#include "gp.h"

#include "gscdefs.h"
#include "gserror.h"
#include "gserrors.h"
#include "gslib.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gscoord.h"
#include "gspaint.h"
#include "gspath.h"
#include "gspath2.h"
#include "gsutil.h"

#include "gx.h"
#include "gxdevice.h"
#include "gxpath.h"

#include "gxfont.h"
#include "gxchar.h"
#include "gsgdata.h"
#include "gxfont42.h"
#include "gxfcache.h"

#include "gzstate.h"
#include "gzpath.h"

/*
 * Opaque font struct.
 */

#include "gslt.h"

#include "gslt_font_api.h"

struct gslt_font_s
{
    byte *data;
    int length;
    gs_font *font;
    int cmaptable;
    int cmapsubcount;
    int cmapsubtable;
};

static int gslt_init_truetype_font(gs_memory_t *mem, gs_font_dir *xfc, gslt_font_t *xf, int wmode);

/*
 * The font cache is a gs_font_dir.
 * It has some parameters that need to be set,
 * do so here.
 */

gs_font_dir *
gslt_new_font_cache(gs_memory_t *mem)
{
    gs_font_dir *fontdir;

    uint smax = 50;         /* number of scaled fonts */
    uint bmax = 500000;     /* space for cached chars */
    uint mmax = 200;        /* number of cached font/matrix pairs */
    uint cmax = 5000;       /* number of cached chars */
    uint upper = 32000;     /* max size of a single cached char */

    fontdir = gs_font_dir_alloc2_limits(mem, mem, smax, bmax, mmax, cmax, upper);
    if (!fontdir)
    {
        gs_throw(-1, "cannot gs_font_dir_alloc2_limits()");
        return NULL;
    }

    gs_setaligntopixels(fontdir, 1); /* no subpixels */
    gs_setgridfittt(fontdir, 3); /* see gx_ttf_outline for values */

    return fontdir;
}

void
gslt_free_font_cache(gs_memory_t *mem, gs_font_dir *fontdir)
{
    gs_free_object(mem, fontdir, "gs_font_dir");
}

/*
 * Big-endian memory accessor functions
 */

static inline int u16(byte *p)
{
    return (p[0] << 8) | p[1];
}

static inline int s16(byte *p)
{
    return (signed short)( (p[0] << 8) | p[1] );
}

static inline int u32(byte *p)
{
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

/*
 * Find the offset and length of an SFNT table.
 * Return -1 if no table by the specified name is found.
 */

static int
gslt_find_sfnt_table(gslt_font_t *xf, char *name, int *lengthp)
{
    uint ntables;
    uint i;

    if (xf->length < 12)
        return -1;

    ntables = u16(xf->data + 4);
    if (xf->length < 12 + ntables * 16)
        return -1;

    for (i = 0; i < ntables; i++)
    {
        byte *entry = xf->data + 12 + i * 16;
        if (!memcmp(entry, name, 4))
        {
            if (lengthp)
                *lengthp = u32(entry + 12);
            return u32(entry + 8);
        }
    }

    return -1;
}

/*
 * Locate the 'cmap' table and count the number of subtables.
 */

static int
gslt_load_sfnt_cmap(gslt_font_t *xf)
{
    byte *cmapdata;
    int offset, length;
    int nsubtables;

    offset = gslt_find_sfnt_table(xf, "cmap", &length);
    if (offset < 0)
        return -1;

    if (length < 4)
        return -1;

    cmapdata = xf->data + offset;

    nsubtables = u16(cmapdata + 2);
    if (nsubtables < 0)
        return -1;
    if (length < 4 + nsubtables * 8)
        return -1;

    xf->cmaptable = offset;
    xf->cmapsubcount = nsubtables;
    xf->cmapsubtable = 0;

    return 0;
}

/*
 * Return the number of cmap subtables.
 */

int
gslt_count_font_encodings(gslt_font_t *xf)
{
    return xf->cmapsubcount;
}

/*
 * Extract PlatformID and EncodingID for a cmap subtable.
 */

int
gslt_identify_font_encoding(gslt_font_t *xf, int idx, int *pid, int *eid)
{
    if (idx < 0 || idx >= xf->cmapsubcount)
        return -1;
    byte *cmapdata = xf->data + xf->cmaptable;
    byte *entry = cmapdata + 4 + idx * 8;
    *pid = u16(entry + 0);
    *eid = u16(entry + 2);
    return 0;
}

/*
 * Select a cmap subtable for use with encoding functions.
 */

int
gslt_select_font_encoding(gslt_font_t *xf, int idx)
{
    if (idx < 0 || idx >= xf->cmapsubcount)
        return -1;
    byte *cmapdata = xf->data + xf->cmaptable;
    byte *entry = cmapdata + 4 + idx * 8;
    xf->cmapsubtable = xf->cmaptable + u32(entry + 4);
    return 0;
}

/*
 * Load and initialize a font struct from a file.
 * Writing mode is set here; as a legacy from postscript
 * where the writing mode is per-font and not switchable.
 *
 * TODO: using a normal font with wmode 1 will fail if
 * there are no vmtx tables ... is this the right behavior?
 */

gslt_font_t *
gslt_new_font(gs_memory_t *mem, gs_font_dir *fontdir, char *buf, int buflen, int wmode)
{
    gslt_font_t *xf;
    byte *data = (byte*)buf;
    int length;
    int t;

    xf = (void*) gs_alloc_bytes(mem, sizeof(gslt_font_t), "gslt_font struct");
    if (!xf)
    {
        gs_free_object(mem, data, "gslt_font bytes");
        gs_throw(-1, "out of memory");
        return NULL;
    }

    xf->data = data;
    xf->length = length;
    xf->font = NULL;

    xf->cmaptable = 0;
    xf->cmapsubcount = 0;
    xf->cmapsubtable = 0;

    /* TODO: implement CFF fonts */

    t = gslt_init_truetype_font(mem, fontdir, xf, wmode);
    if (t < 0)
    {
        gslt_free_font(mem, xf);
        gs_throw(-1, "cannot init font");
        return NULL;
    }

    t = gslt_load_sfnt_cmap(xf);
    if (t < 0)
    {
        errprintf("warning: no cmap table found in font\n");
    }

    return xf;
}

void
gslt_free_font(gs_memory_t *mem, gslt_font_t *xf)
{
    gs_free_object(mem, xf, "gslt_font struct");
}

/*
 * Encode a character using the selected cmap subtable.
 * TODO: extend this to cover more cmap formats.
 */

int
gslt_encode_font_char(gslt_font_t *xf, int key)
{
    byte *table;

    /* no cmap selected: return identity */
    if (xf->cmapsubtable <= 0)
        return key;

    table = xf->data + xf->cmapsubtable;

    switch (u16(table))
    {
    case 0: /* Apple standard 1-to-1 mapping. */
        return table[key + 6];

    case 4: /* Microsoft/Adobe segmented mapping. */
        {
            int segCount2 = u16(table + 6);
            byte *endCount = table + 14;
            byte *startCount = endCount + segCount2 + 2;
            byte *idDelta = startCount + segCount2;
            byte *idRangeOffset = idDelta + segCount2;
            int i2;

            for (i2 = 0; i2 < segCount2 - 3; i2 += 2)
            {
                int delta, roff;
                int start = u16(startCount + i2);
                int glyph;

                if ( key < start )
                    return 0;
                if ( key > u16(endCount + i2) )
                    continue;
                delta = s16(idDelta + i2);
                roff = s16(idRangeOffset + i2);
                if ( roff == 0 )
                {
                    return ( key + delta ) & 0xffff; /* mod 65536 */
                    return 0;
                }
                glyph = u16(idRangeOffset + i2 + roff + ((key - start) << 1));
                return (glyph == 0 ? 0 : glyph + delta);
            }

            /*
             * The TrueType documentation says that the last range is
             * always supposed to end with 0xffff, so this shouldn't
             * happen; however, in some real fonts, it does.
             */
            return 0;
        }

    case 6: /* Single interval lookup. */
        {
            int firstCode = u16(table + 6);
            int entryCount = u16(table + 8);
            if ( key < firstCode || key >= firstCode + entryCount )
                return 0;
            return u16(table + 10 + ((key - firstCode) << 1));
        }

    default:
        errprintf("error: unknown cmap format: %d\n", u16(table));
        return 0;
    }

    return 0;
}

/*
 * A bunch of callback functions that the ghostscript
 * font machinery will call. The most important one
 * is the build_char function.
 */

static gs_glyph
gslt_callback_encode_char(gs_font *pfont, gs_char chr, gs_glyph_space_t spc)
{
    gslt_font_t *xf = pfont->client_data;
    int value;
    value = gslt_encode_font_char(xf, chr);
    if (value == 0)
        return gs_no_glyph;
    return value;
}

static gs_char
gslt_callback_decode_glyph(gs_font *p42, gs_glyph glyph, int ch)
{
    return GS_NO_CHAR;
}

static int
gslt_callback_glyph_name(gs_font *pf, gs_glyph glyph, gs_const_string *pstr)
{
    return 0;
}

static int
gslt_callback_string_proc(gs_font_type42 *p42, ulong offset, uint length, const byte **pdata)
{
    /* NB bounds check offset + length - use gs_object_size for memory
       buffers - if file read should fail */
    gslt_font_t *xf = p42->client_data;
    *pdata = xf->data + offset;
    return 0;
}

static int
gslt_callback_build_char(gs_text_enum_t *ptextenum, gs_state *pgs, gs_font *pfont,
        gs_char chr, gs_glyph glyph)
{
    gs_show_enum *penum = (gs_show_enum*)ptextenum;
    gs_font_type42 *p42 = (gs_font_type42*)pfont;
    const gs_rect *pbbox;
    float sbw[4], w2[6];
    int code;

    code = gs_type42_get_metrics(p42, glyph, sbw);
    if (code < 0)
        return code;

    w2[0] = sbw[2];
    w2[1] = sbw[3];

    pbbox =  &p42->FontBBox;
    w2[2] = pbbox->p.x;
    w2[3] = pbbox->p.y;
    w2[4] = pbbox->q.x;
    w2[5] = pbbox->q.y;

    /* Expand the bbox when stroking */
    if ( pfont->PaintType )
    {
        float expand = max(1.415, gs_currentmiterlimit(pgs)) * gs_currentlinewidth(pgs) / 2;
        w2[2] -= expand, w2[3] -= expand;
        w2[4] += expand, w2[5] += expand;
    }

    if ( (code = gs_moveto(pgs, 0.0, 0.0)) < 0 )
        return code;

    if ( (code = gs_setcachedevice(penum, pgs, w2)) < 0 )
        return code;

    code = gs_type42_append(glyph,
            (gs_imager_state *)pgs,
            gx_current_path(pgs),
            &penum->log2_scale,
            gs_show_in_charpath(penum) != cpm_show,
            p42->PaintType,
            penum->pair);
    if (code < 0)
        return code;

    code = (pfont->PaintType ? gs_stroke(pgs) : gs_fill(pgs));
    if (code < 0)
        return code;

    return 0;
}

/*
 * Initialize the ghostscript font machinery for a truetype
 * (type42 in postscript terminology) font.
 */

static int
gslt_init_truetype_font(gs_memory_t *mem, gs_font_dir *fontdir, gslt_font_t *xf, int wmode)
{
    xf->font = (void*) gs_alloc_struct(mem, gs_font_type42, &st_gs_font_type42, "gslt_font type42");
    if (!xf->font)
        return gs_throw(-1, "out of memory");

    /* no shortage of things to initialize */
    {
        gs_font_type42 *p42 = (gs_font_type42*) xf->font;

        /* Common to all fonts: */

        p42->next = 0;
        p42->prev = 0;
        p42->memory = mem;

        p42->dir = fontdir; /* NB also set by gs_definefont later */
        p42->base = xf->font; /* NB also set by gs_definefont later */
        p42->is_resource = false;
        gs_notify_init(&p42->notify_list, gs_memory_stable(mem));
        p42->id = gs_next_ids(mem, 1);

        p42->client_data = xf; /* that's us */

        gs_make_identity(&p42->FontMatrix);
        gs_make_identity(&p42->orig_FontMatrix); /* NB ... original or zeroes? */

        p42->FontType = ft_TrueType;
        p42->BitmapWidths = true;
        p42->ExactSize = fbit_use_outlines;
        p42->InBetweenSize = fbit_use_outlines;
        p42->TransformedChar = fbit_use_outlines;
        p42->WMode = wmode; /* TODO */
        p42->PaintType = 0;
        p42->StrokeWidth = 0;

        p42->procs.init_fstack = gs_default_init_fstack;
        p42->procs.next_char_glyph = gs_default_next_char_glyph;
        p42->procs.glyph_name = gslt_callback_glyph_name;
        p42->procs.decode_glyph = gslt_callback_decode_glyph;
        p42->procs.define_font = gs_no_define_font;
        p42->procs.make_font = gs_no_make_font;
        p42->procs.font_info = gs_default_font_info;
        p42->procs.glyph_info = gs_default_glyph_info;
        p42->procs.glyph_outline = gs_no_glyph_outline;
        p42->procs.encode_char = gslt_callback_encode_char;
        p42->procs.build_char = gslt_callback_build_char;

        p42->font_name.size = 0;
        p42->key_name.size = 0;

        /* Base font specific: */

        p42->FontBBox.p.x = 0;
        p42->FontBBox.p.y = 0;
        p42->FontBBox.q.x = 0;
        p42->FontBBox.q.y = 0;

        uid_set_UniqueID(&p42->UID, p42->id);

        p42->encoding_index = ENCODING_INDEX_UNKNOWN;
        p42->nearest_encoding_index = ENCODING_INDEX_UNKNOWN;

        p42->FAPI = 0;
        p42->FAPI_font_data = 0;

        /* Type 42 specific: */

        p42->data.string_proc = gslt_callback_string_proc;
        p42->data.proc_data = xf;
        gs_type42_font_init(p42);
    }

    gs_definefont(fontdir, xf->font);

    return 0;
}

/*
 * Draw a glyph to the device, and extract the bitmap from
 * the ccache afterwards... only works if the bitmap is not
 * too large.
 */

int
gslt_render_font_glyph(gs_state *pgs, gslt_font_t *xf, gs_matrix *tm, int gid, gslt_glyph_bitmap_t *slot)
{
    gs_fixed_point subpixel = {0, 0}; /* we don't use subpixel accurate device metrics */
    gs_log2_scale_point oversampling = {0, 0}; /* we don't use oversampling */
    gs_text_params_t params;
    gs_text_enum_t *textenum;
    gs_matrix matrix;
    cached_fm_pair *ppair;
    cached_char *cc;
    int code;

    /* get the real font matrix (this is a little dance) */
    gs_setfont(pgs, xf->font); /* set pgs->font and invalidate existing charmatrix */
    gs_setcharmatrix(pgs, tm); /* set the charmatrix to ctm * tm */
    gs_currentcharmatrix(pgs, &matrix, true); /* extract charmatrix (and multiply by FontMatrix) */

    // dprintf4("tm = [%g %g %g %g]\n", matrix.xx, matrix.xy, matrix.yx, matrix.yy);

    /* find the font/matrix pair (or add it) */
    code = gx_lookup_fm_pair(xf->font, &matrix, &oversampling, false, &ppair);
    if (code != 0)
        return gs_throw(-1, "cannot gx_lookup_fm_pair()");

    cc = gx_lookup_cached_char(xf->font, ppair, gid, 0, 1, &subpixel);
    if (!cc)
    {
        /* No luck ... now we need to get it into the cache somehow.
         *
         * We do this by rendering one glyph (that's why we need a device and pgs).
         * The renderer always renders the bitmap into the cache, and draws
         * from out of the cache when blitting to the device.
         *
         * Things don't get evicted from the cache until there is a collision,
         * so we have a safe window to snarf it back out of the cache
         * after it's been drawn to the device.
         */

        // dprintf1("cache miss for glyph %d\n", gid);

        params.operation = TEXT_FROM_SINGLE_GLYPH | TEXT_DO_DRAW | TEXT_RETURN_WIDTH;
        params.data.d_glyph = gid;
        params.size = 1;

        if (gs_text_begin(pgs, &params, xf->font->memory, &textenum) != 0)
            return gs_throw(-1, "cannot gs_text_begin()");
        if (gs_text_process(textenum) != 0)
            return gs_throw(-1, "cannot gs_text_process()");
        gs_text_release(textenum, "gslt font render");

        cc = gx_lookup_cached_char(xf->font, ppair, gid, 0, 1, &subpixel);
        if (!cc)
        {
            /* merde! it rendered but was not placed in the cache. */
            return gs_throw(-2, "cannot render from cache");
        }
    }

    /* copy values from the cache into the client struct */
    slot->w = cc->width;
    slot->h = cc->height;
    slot->stride = cc_raster(cc);
    slot->lsb = fixed2int(cc->offset.x);
    slot->top = fixed2int(cc->offset.y);
    slot->xadv = fixed2float(cc->wxy.x);
    slot->yadv = fixed2float(cc->wxy.y);
    slot->data = cc_bits(cc);

#ifdef XXX
    dprintf1("glyph %d\n", gid);
    debug_dump_bitmap(cc_bits(xf->cc), cc_raster(xf->cc), xf->cc->height, "");
    {
        char fn[32];
        sprintf(fn, "glyph%d.pbm", gid);
        FILE *fo = fopen(fn, "wb");
        if (!fo)
            return -1;
        fprintf(fo, "P4\n%d %d\n", xf->cc->width, xf->cc->height);
        int y;
        int s = (xf->cc->width + 7) / 8;
        for (y = 0; y < xf->cc->height; y++)
            fwrite(cc_bits(xf->cc) + y * cc_raster(xf->cc), 1, s, fo);
        fclose(fo);
    }
#endif

    return 0;
}

int
gslt_outline_font_glyph(gs_state *pgs, gslt_font_t *xf, int gid, gslt_outline_walker_t *walk)
{
    gs_text_params_t params;
    gs_text_enum_t *textenum;
    gs_matrix matrix;
    segment *seg;
    curve_segment *cseg;

    gs_gsave(pgs);
    gs_make_identity(&matrix);
    gs_setmatrix(pgs, &matrix);
    gs_scale(pgs, 1000.0, 1000.0); /* otherwise we hit serious precision problems with fixpoint math */

    /* set gstate params */
    gs_setfont(pgs, xf->font); /* set pgs->font and invalidate existing charmatrix */
    gs_make_identity(&matrix);
    gs_setcharmatrix(pgs, &matrix); /* set the charmatrix to identity */

    /* reset the path */
    gs_newpath(pgs);
    gs_moveto(pgs, 0.0, 0.0);

    /* draw the glyph, in charpath mode */
    params.operation = TEXT_FROM_SINGLE_GLYPH | TEXT_DO_FALSE_CHARPATH | TEXT_RETURN_WIDTH;
    params.data.d_glyph = gid;
    params.size = 1;

    if (gs_text_begin(pgs, &params, xf->font->memory, &textenum) != 0)
        return gs_throw(-1, "cannot gs_text_begin()");
    if (gs_text_process(textenum) != 0)
        return gs_throw(-1, "cannot gs_text_process()");
    gs_text_release(textenum, "gslt font outline");

    /* walk the resulting path */
    seg = (segment*)pgs->path->first_subpath;
    while (seg)
    {
        switch (seg->type)
        {
        case s_start:
            walk->moveto(walk->user,
                    fixed2float(seg->pt.x) * 0.001,
                    fixed2float(seg->pt.y) * 0.001);
            break;
        case s_line:
            walk->lineto(walk->user,
                    fixed2float(seg->pt.x) * 0.001,
                    fixed2float(seg->pt.y) * 0.001);
            break;
        case s_line_close:
            walk->closepath(walk->user);
            break;
        case s_curve:
            cseg = (curve_segment*)seg;
            walk->curveto(walk->user,
                    fixed2float(cseg->p1.x) * 0.001,
                    fixed2float(cseg->p1.y) * 0.001,
                    fixed2float(cseg->p2.x) * 0.001,
                    fixed2float(cseg->p2.y) * 0.001,
                    fixed2float(seg->pt.x) * 0.001,
                    fixed2float(seg->pt.y) * 0.001);
            break;
        }
        seg = seg->next;
    }

    /* and toss it away... */
    gs_newpath(pgs);

    gs_grestore(pgs);
    return 0;
}

int
gslt_measure_font_glyph(gs_state *pgs, gslt_font_t *xf, int gid, gslt_glyph_metrics_t *mtx)
{
    int code;
    float sbw[4];

    code = gs_type42_get_metrics((gs_font_type42*)xf->font, gid, sbw);
    if (code < 0)
        return code;

    // TODO: extract this somehow ...
    mtx->v[0] = 0.0;
    mtx->v[1] = 0.0;

    mtx->w[0] = sbw[2];
    mtx->w[1] = sbw[3];

    return 0;
}

#ifdef NEVER
int
gslt_measure_font_glyph(gslt_font_t *xf, int gid, gslt_glyph_metrics_t *mtx)
{
    /*
     * Gods this is slow ... gs_default_glyph_info builds the outline
     * Should cache this somehow...
     * anyway, it just returns 0 and i'm too lazy to find out why
     */

    gs_glyph_info_t info;
    int mask;
    int code;

    mask = GLYPH_INFO_OUTLINE_WIDTHS | GLYPH_INFO_WIDTHS;
    if (xf->font->WMode == 0)
        mask |= GLYPH_INFO_VVECTOR0 | GLYPH_INFO_WIDTH0;
    else
        mask |= GLYPH_INFO_VVECTOR1 | GLYPH_INFO_WIDTH1;

    code = xf->font->procs.glyph_info(xf->font, gid, NULL, GLYPH_INFO_OUTLINE_WIDTHS, &info);
    dprintf2("mask %x -> %x\n", mask, code);
    if (code == 0)
        return gs_throw(-1, "cannot font->glyph_info()");

    if (xf->font->WMode == 0)
    {
        mtx->x = 0.0;
        mtx->y = 0.0;
        mtx->w = info.width[0].x;
    }
    else
    {
        mtx->x = info.v.x;
        mtx->y = info.v.y;
        mtx->w = info.width[0].y;
    }

    return 0;
}
#endif

