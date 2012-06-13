/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

#include "gslt_font_int.h"

#include "gslt.h"
#include "gslt_font.h"

void
gslt_retain_font_glyph(gs_memory_t *mem, gslt_glyph_bitmap_t *slot)
{
    gx_retain_cached_char(slot->cc);
}

void
gslt_release_font_glyph(gs_memory_t *mem, gslt_glyph_bitmap_t *slot)
{
    gx_release_cached_char(slot->cc);
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

        gs_moveto(pgs, 100.0, 100.0); // why?

        code = gs_text_begin(pgs, &params, xf->font->memory, &textenum);
        if (code != 0)
            return gs_throw1(-1, "cannot gs_text_begin() (%d)", code);

        code = gs_text_process(textenum);
        if (code != 0)
            return gs_throw1(-1, "cannot gs_text_process() (%d)", code);

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

    slot->cc = cc;
    slot->data = cc_bits(cc);
    gx_retain_cached_char(cc);

#define XXX
#ifndef XXX
    static int xxx = 0; /* declaration out in the wild not allowed in ansi c */
    dprintf1("glyph %d\n", xxx);
    debug_dump_bitmap(cc_bits(cc), cc_raster(cc), cc->height, "");
    {
        char fn[32];
        sprintf(fn, "glyph%d.pbm", xxx);
        FILE *fo = fopen(fn, "wb");
        if (!fo)
            return -1;
        fprintf(fo, "P4\n%d %d\n", cc->width, cc->height);
        int y;
        int s = (cc->width + 7) / 8;
        for (y = 0; y < cc->height; y++)
            fwrite(cc_bits(cc) + y * cc_raster(cc), 1, s, fo);
        fclose(fo);
    }
    xxx ++;
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
    int head, format, loca, glyf;
    int ofs, len;
    int idx, i, n;
    int hadv, vadv, vorg;
    int vtop, ymax, desc;
    int scale;

    /* some insane defaults */

    scale = 2048; /* units-per-em */
    mtx->hadv = 0.5;
    mtx->vadv = -1.0;
    mtx->vorg = 1.0;

    /*
     * Horizontal metrics are easy.
     */

    ofs = gslt_find_sfnt_table(xf, "hhea", &len);
    if (ofs < 0)
        return gs_throw(-1, "cannot find hhea table");

    if (len < 2 * 18)
        return gs_throw(-1, "hhea table is too short");

    vorg = s16(xf->data + ofs + 4); /* ascender is default vorg */
    desc = s16(xf->data + ofs + 6); /* descender */
    if (desc < 0)
        desc = -desc;
    n = u16(xf->data + ofs + 17 * 2);

    ofs = gslt_find_sfnt_table(xf, "hmtx", &len);
    if (ofs < 0)
        return gs_throw(-1, "cannot find hmtx table");

    idx = gid;
    if (idx > n - 1)
        idx = n - 1;

    hadv = u16(xf->data + ofs + idx * 4);
    vadv = 0;

    /*
     * Vertical metrics are hairy (with missing tables).
     */

    head = gslt_find_sfnt_table(xf, "head", &len);
    if (head > 0)
    {
        scale = u16(xf->data + head + 18); /* units per em */
    }

    ofs = gslt_find_sfnt_table(xf, "OS/2", &len);
    if (ofs > 0 && len > 70)
    {
        vorg = s16(xf->data + ofs + 68); /* sTypoAscender */
        desc = s16(xf->data + ofs + 70); /* sTypoDescender */
        if (desc < 0)
            desc = -desc;
    }

    ofs = gslt_find_sfnt_table(xf, "vhea", &len);
    if (ofs > 0)
    {
        if (len < 2 * 18)
            return gs_throw(-1, "vhea table is too short");

        n = u16(xf->data + ofs + 17 * 2);

        ofs = gslt_find_sfnt_table(xf, "vmtx", &len);
        if (ofs < 0)
            return gs_throw(-1, "cannot find vmtx table");

        idx = gid;
        if (idx > n - 1)
            idx = n - 1;

        vadv = u16(xf->data + ofs + idx * 4);
        vtop = u16(xf->data + ofs + idx * 4 + 2);

        glyf = gslt_find_sfnt_table(xf, "glyf", &len);
        loca = gslt_find_sfnt_table(xf, "loca", &len);
        if (head > 0 && glyf > 0 && loca > 0)
        {
            format = u16(xf->data + head + 50); /* indexToLocaFormat */

            if (format == 0)
                ofs = u16(xf->data + loca + gid * 2) * 2;
            else
                ofs = u32(xf->data + loca + gid * 4);

            ymax = u16(xf->data + glyf + ofs + 8); /* yMax */

            vorg = ymax + vtop;
        }
    }

    ofs = gslt_find_sfnt_table(xf, "VORG", &len);
    if (ofs > 0)
    {
        vorg = u16(xf->data + ofs + 6);
        n = u16(xf->data + ofs + 6);
        for (i = 0; i < n; i++)
        {
            if (u16(xf->data + ofs + 8 + 4 * i) == gid)
            {
                vorg = s16(xf->data + ofs + 8 + 4 * i + 2);
                break;
            }
        }
    }

    if (vadv == 0)
        vadv = vorg + desc;

    mtx->hadv = hadv / (float) scale;
    mtx->vadv = vadv / (float) scale;
    mtx->vorg = vorg / (float) scale;

    return 0;
}
