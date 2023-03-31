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

/* Path stroking procedures for Ghostscript library */
#include "math_.h"
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gsdcolor.h"
#include "gsptype1.h"
#include "gxfixed.h"
#include "gxfarith.h"
#include "gxmatrix.h"
#include "gscoord.h"
#include "gsdevice.h"
#include "gxdevice.h"
#include "gxhttile.h"
#include "gxgstate.h"
#include "gzline.h"
#include "gzpath.h"
#include "gzcpath.h"
#include "gxpaint.h"
#include "gxscanc.h"
#include "gxfill.h"
#include "gxdcolor.h"
#include "assert_.h"
#include <stdlib.h>             /* for qsort */
#include <limits.h>             /* For INT_MAX */

/* Overview of the scan conversion algorithm.
 *
 * The normal scan conversion algorithm runs through a path, converting
 * it into a sequence of edges. It then runs through those edges from
 * top to bottom keeping a list of which ones are "active", and ordering
 * them so that it can read out a list of intersection points from left
 * to right across any given scanline (or scan "band" when working with
 * trapezoids).
 *
 * This scan conversion algorithm avoids the need to maintain an active
 * line list, and to repeatedly re-sort lines. It is thus faster, at
 * the cost of using more memory, and not being able to cope with
 * trapezoids.
 *
 * Conceptually, the idea is to make an (initially empty) table. Each
 * row of the table holds the set of intersection data for a given
 * scanline. We therefore just need to run through the path once,
 * decomposing it to a sequence of edges. We then step along each edge
 * adding intersection information into each row of the table as we go.
 * Each piece of intersection information includes the point at which
 * the edge crosses the scanline, and the direction in which it does so
 * (up or down).
 *
 * At the end of this process, we can then sort each rows data, and
 * simply 'fill in' the scanline according to the winding rule.
 *
 * This copes well with 'centre of a pixel' fill modes, but 'any part
 * of a pixel' requires some extra work. Let's describe 'centre of a
 * pixel' first.
 *
 * Assume we have a path with n segments in, and a bbox that crosses
 * a region x wide, y high.
 *
 * 1) Create a table, A, 1 int entry per scan line. Run through the path,
 * segment by segment counting how many intersections occur on each
 * scanline. (O(y * n))
 *
 * 2) Create a table, B, with as many entries per scanline as determined in
 * the table A. (O(y * n))
 *
 * [Each entry is a (xcoord,direction) tuple. xcoord = the xcoord where
 * an edge crosses the horizontal line through the middle of the pixel.
 * direction = 0 if the edge is rising, 1 if falling.]
 *
 * 3) Run through the path segment by segment, inserting entries for each
 * scanline intersection in table B. (O(y * n))
 *
 * 4) Sort the scanline intersections of table B (on left,right,direction).
 * (O(y * n log n) for current code)
 *
 * 5) Filter the scanline intersections according to the winding rule.
 * (O(y * n))
 *
 * 6) Fill rectangles according to each set of scanline intersections.
 * (O(y * n))
 *
 * So worst case complexity (when every segment crosses every scanline) is
 * O(y * n log n).
 *
 * NOTE: If we use a binary comparison based sort, then the best we can manage
 * is n log n for step 4. If we use a radix based sort, we can get O(n).
 * Consider this if we ever need it.
 *
 * In order to cope with 'any part of a pixel' it no longer suffices
 * to keep a single intersection point for each scanline intersection.
 * Instead we keep the interval of a scanline that the edge intersects.
 * Thus each entry is a (left,right,direction) tuple. left = the
 * leftmost point at which this edge intersects this scanline. right =
 * the rightmost point at which this edge intersects this scanline.
 * direction = 0 for rising edges, 1 for falling edges.
 *
 * The rest of the algorithm is unchanged, apart from additional care
 * being required when filling the scanlines to allow for the fact
 * that edges are no longer point intersections.
 *
 * The first set of routines (gx_scan_convert and gx_fill_edgebuffer)
 * implement the "pixel centre" covered routines by drawing rectangle
 * high scanlines at a time. The second set of routines
 * (gx_scan_convert_app and gx_fill_edgebuffer_app) is the equivalent,
 * for "Any Part of Pixel" covered.
 *
 * The third and fourth are the same things, but using trapezoids
 * that can be multiple scanlines high rather than scanlines.
 *
 * In order to do trapezoid extraction, we extend the edge intersection
 * information to be (left,right,id,direction) (for the "centre pixel"
 * variants) and (left,left_id,right,right_id,direction) (for the "any
 * part of a pixel" variants). The 'id' is a int that is guaranteed
 * unique for each flattened line in path.
 *
 * If we spot that each scanlines data has the same set of ids in the
 * same order, then we can 'collate' them into a trapezoid.
 */

/* NOTE: code in this file assumes that fixed and int can be used
 * interchangably. */

#undef DEBUG_SCAN_CONVERTER
#undef DEBUG_OUTPUT_SC_AS_PS

typedef int64_t fixed64;

enum
{
    DIRN_UNSET = -1,
    DIRN_UP = 0,
    DIRN_DOWN = 1
};

/* Centre of a pixel routines */

static int intcmp(const void *a, const void *b)
{
    return *((int*)a) - *((int *)b);
}

#if defined(DEBUG_SCAN_CONVERTER)
int debugging_scan_converter = 1;

static void
gx_edgebuffer_print(gx_edgebuffer * edgebuffer)
{
    int i;

    dlprintf1("Edgebuffer %x\n", edgebuffer);
    dlprintf4("xmin=%x xmax=%x base=%x height=%x\n",
              edgebuffer->xmin, edgebuffer->xmax, edgebuffer->base, edgebuffer->height);
    for (i=0; i < edgebuffer->height; i++) {
        int  offset = edgebuffer->index[i];
        int *row    = &edgebuffer->table[offset];
        int count   = *row++;
        dlprintf3("%d @ %d: %d =", i, offset, count);
        while (count-- > 0) {
            int v = *row++;
            dlprintf2(" %x:%d", v&~1, v&1);
        }
        dlprintf("\n");
    }
}
#endif

#ifdef DEBUG_OUTPUT_SC_AS_PS
static void coord(const char *str, fixed x, fixed y)
{
    if (x > 0)
        dlprintf1(" 16#%x ", x);
    else
        dlprintf1("0 16#%x sub ", -x);
    if (y > 0)
        dlprintf1(" 16#%x ", y);
    else
        dlprintf1("0 16#%x sub ", -y);
    dlprintf1("%s %%PS\n", str);
}
#endif

typedef void (zero_filler_fn)(int *, const fixed *);

static void mark_line_zero(fixed sx, fixed ex, fixed *zf)
{
    if (sx < zf[0])
        zf[0] = sx;
    if (ex < zf[0])
        zf[0] = ex;
    if (sx > zf[1])
        zf[1] = sx;
    if (ex > zf[1])
        zf[1] = ex;
}

static void mark_curve_zero(fixed sx, fixed c1x, fixed c2x, fixed ex, int depth, fixed *zf)
{
    fixed ax = (sx + c1x)>>1;
    fixed bx = (c1x + c2x)>>1;
    fixed cx = (c2x + ex)>>1;
    fixed dx = (ax + bx)>>1;
    fixed fx = (bx + cx)>>1;
    fixed gx = (dx + fx)>>1;

    assert(depth >= 0);
    if (depth == 0)
        mark_line_zero(sx, ex, zf);
    else {
        depth--;
        mark_curve_zero(sx, ax, dx, gx, depth, zf);
        mark_curve_zero(gx, fx, cx, ex, depth, zf);
    }
}

static void mark_curve_big_zero(fixed64 sx, fixed64 c1x, fixed64 c2x, fixed64 ex, int depth, fixed *zf)
{
    fixed64 ax = (sx + c1x)>>1;
    fixed64 bx = (c1x + c2x)>>1;
    fixed64 cx = (c2x + ex)>>1;
    fixed64 dx = (ax + bx)>>1;
    fixed64 fx = (bx + cx)>>1;
    fixed64 gx = (dx + fx)>>1;

    assert(depth >= 0);
    if (depth == 0)
        mark_line_zero((fixed)sx, (fixed)ex, zf);
    else {
        depth--;
        mark_curve_big_zero(sx, ax, dx, gx, depth, zf);
        mark_curve_big_zero(gx, fx, cx, ex, depth, zf);
    }
}

static void mark_curve_top_zero(fixed sx, fixed c1x, fixed c2x, fixed ex, int depth, fixed *zf)
{
    fixed test = (sx^(sx<<1))|(c1x^(c1x<<1))|(c2x^(c2x<<1))|(ex^(ex<<1));

    if (test < 0)
        mark_curve_big_zero(sx, c1x, c2x, ex, depth, zf);
    else
        mark_curve_zero(sx, c1x, c2x, ex, depth, zf);
}

static int
zero_case(gx_device      * gs_restrict pdev,
          gx_path        * gs_restrict path,
          gs_fixed_rect  * gs_restrict ibox,
          int            * gs_restrict index,
          int            * gs_restrict table,
          fixed                        fixed_flat,
          zero_filler_fn *             fill)
{
    const subpath *psub;
    fixed zf[2];

    /* Step 2 continued: Now we run through the path, filling in the real
     * values. */
    for (psub = path->first_subpath; psub != 0;) {
        const segment *pseg = (const segment *)psub;
        fixed ex = pseg->pt.x;
        fixed sy = pseg->pt.y;
        fixed ix = ex;
        int iy = fixed2int(pseg->pt.y);

        zf[0] = ex;
        zf[1] = ex;

        while ((pseg = pseg->next) != 0 &&
               pseg->type != s_start
            ) {
            fixed sx = ex;
            ex = pseg->pt.x;

            switch (pseg->type) {
                default:
                case s_start: /* Should never happen */
                case s_dash:  /* We should never be seeing a dash here */
                    assert("This should never happen" == NULL);
                    break;
                case s_curve: {
                    const curve_segment *const pcur = (const curve_segment *)pseg;
                    int k = gx_curve_log2_samples(sx, sy, pcur, fixed_flat);

                    mark_curve_top_zero(sx, pcur->p1.x, pcur->p2.x, ex, k, zf);
                    break;
                }
                case s_gap:
                case s_line:
                case s_line_close:
                    mark_line_zero(sx, ex, zf);
                    break;
            }
        }
        /* And close any open segments */
        mark_line_zero(ex, ix, zf);
        fill(&table[index[iy-ibox->p.y]], zf);
        psub = (const subpath *)pseg;
    }

    return 0;
}

static void mark_line(fixed sx, fixed sy, fixed ex, fixed ey, int base_y, int height, int *table, int *index)
{
    int64_t delta;
    int iy, ih;
    fixed clip_sy, clip_ey;
    int dirn = DIRN_UP;
    int *row;

#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter)
        dlprintf6("Marking line from %x,%x to %x,%x (%x,%x)\n", sx, sy, ex, ey, fixed2int(sy + fixed_half-1) - base_y, fixed2int(ey + fixed_half-1) - base_y);
#endif
#ifdef DEBUG_OUTPUT_SC_AS_PS
    dlprintf("0.001 setlinewidth 0 0 0 setrgbcolor %%PS\n");
    coord("moveto", sx, sy);
    coord("lineto", ex, ey);
    dlprintf("stroke %%PS\n");
#endif

    if (fixed2int(sy + fixed_half-1) == fixed2int(ey + fixed_half-1))
        return;
    if (sy > ey) {
        int t;
        t = sy; sy = ey; ey = t;
        t = sx; sx = ex; ex = t;
        dirn = DIRN_DOWN;
    }
    /* Lines go from sy to ey, closed at the start, open at the end. */
    /* We clip them to a region to make them closed at both ends. */
    /* Thus the first scanline marked (>= sy) is: */
    clip_sy = ((sy + fixed_half - 1) & ~(fixed_1-1)) | fixed_half;
    /* The last scanline marked (< ey) is: */
    clip_ey = ((ey - fixed_half - 1) & ~(fixed_1-1)) | fixed_half;
    /* Now allow for banding */
    if (clip_sy < int2fixed(base_y) + fixed_half)
        clip_sy = int2fixed(base_y) + fixed_half;
    if (ey <= clip_sy)
        return;
    if (clip_ey > int2fixed(base_y + height - 1) + fixed_half)
        clip_ey = int2fixed(base_y + height - 1) + fixed_half;
    if (sy > clip_ey)
        return;
    delta = (int64_t)clip_sy - (int64_t)sy;
    if (delta > 0)
    {
        int64_t dx = (int64_t)ex - (int64_t)sx;
        int64_t dy = (int64_t)ey - (int64_t)sy;
        int advance = (int)((dx * delta + (dy>>1)) / dy);
        sx += advance;
        sy += delta;
    }
    delta = (int64_t)ey - (int64_t)clip_ey;
    if (delta > 0)
    {
        int64_t dx = (int64_t)ex - (int64_t)sx;
        int64_t dy = (int64_t)ey - (int64_t)sy;
        int advance = (int)((dx * delta + (dy>>1)) / dy);
        ex -= advance;
        ey -= delta;
    }
    ex -= sx;
    ey -= sy;
    ih = fixed2int(ey);
    assert(ih >= 0);
    iy = fixed2int(sy) - base_y;
#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter)
        dlprintf2("    iy=%x ih=%x\n", iy, ih);
#endif
    assert(iy >= 0 && iy < height);
    /* We always cross at least one scanline */
    row = &table[index[iy]];
    *row = (*row)+1; /* Increment the count */
    row[*row] = (sx&~1) | dirn;
    if (ih == 0)
        return;
    if (ex >= 0) {
        int x_inc, n_inc, f;

        /* We want to change sx by ex in ih steps. So each step, we add
         * ex/ih to sx. That's x_inc + n_inc/ih.
         */
        x_inc = ex/ih;
        n_inc = ex-(x_inc*ih);
        f     = ih>>1;
        delta = ih;
        do {
            int count;
            iy++;
            sx += x_inc;
            f  -= n_inc;
            if (f < 0) {
                f += ih;
                sx++;
            }
            assert(iy >= 0 && iy < height);
            row = &table[index[iy]];
            count = *row = (*row)+1; /* Increment the count */
            row[count] = (sx&~1) | dirn;
        } while (--delta);
    } else {
        int x_dec, n_dec, f;

        ex = -ex;
        /* We want to change sx by ex in ih steps. So each step, we subtract
         * ex/ih from sx. That's x_dec + n_dec/ih.
         */
        x_dec = ex/ih;
        n_dec = ex-(x_dec*ih);
        f     = ih>>1;
        delta = ih;
        do {
            int count;
            iy++;
            sx -= x_dec;
            f  -= n_dec;
            if (f < 0) {
                f += ih;
                sx--;
            }
            assert(iy >= 0 && iy < height);
            row = &table[index[iy]];
            count = *row = (*row)+1; /* Increment the count */
            row[count] = (sx&~1) | dirn;
        } while (--delta);
    }
}

static void mark_curve(fixed sx, fixed sy, fixed c1x, fixed c1y, fixed c2x, fixed c2y, fixed ex, fixed ey, fixed base_y, fixed height, int *table, int *index, int depth)
{
    fixed ax = (sx + c1x)>>1;
    fixed ay = (sy + c1y)>>1;
    fixed bx = (c1x + c2x)>>1;
    fixed by = (c1y + c2y)>>1;
    fixed cx = (c2x + ex)>>1;
    fixed cy = (c2y + ey)>>1;
    fixed dx = (ax + bx)>>1;
    fixed dy = (ay + by)>>1;
    fixed fx = (bx + cx)>>1;
    fixed fy = (by + cy)>>1;
    fixed gx = (dx + fx)>>1;
    fixed gy = (dy + fy)>>1;

    assert(depth >= 0);
    if (depth == 0)
        mark_line(sx, sy, ex, ey, base_y, height, table, index);
    else {
        depth--;
        mark_curve(sx, sy, ax, ay, dx, dy, gx, gy, base_y, height, table, index, depth);
        mark_curve(gx, gy, fx, fy, cx, cy, ex, ey, base_y, height, table, index, depth);
    }
}

static void mark_curve_big(fixed64 sx, fixed64 sy, fixed64 c1x, fixed64 c1y, fixed64 c2x, fixed64 c2y, fixed64 ex, fixed64 ey, fixed base_y, fixed height, int *table, int *index, int depth)
{
    fixed64 ax = (sx + c1x)>>1;
    fixed64 ay = (sy + c1y)>>1;
    fixed64 bx = (c1x + c2x)>>1;
    fixed64 by = (c1y + c2y)>>1;
    fixed64 cx = (c2x + ex)>>1;
    fixed64 cy = (c2y + ey)>>1;
    fixed64 dx = (ax + bx)>>1;
    fixed64 dy = (ay + by)>>1;
    fixed64 fx = (bx + cx)>>1;
    fixed64 fy = (by + cy)>>1;
    fixed64 gx = (dx + fx)>>1;
    fixed64 gy = (dy + fy)>>1;

    assert(depth >= 0);
    if (depth == 0)
        mark_line((fixed)sx, (fixed)sy, (fixed)ex, (fixed)ey, base_y, height, table, index);
    else {
        depth--;
        mark_curve_big(sx, sy, ax, ay, dx, dy, gx, gy, base_y, height, table, index, depth);
        mark_curve_big(gx, gy, fx, fy, cx, cy, ex, ey, base_y, height, table, index, depth);
    }
}

static void mark_curve_top(fixed sx, fixed sy, fixed c1x, fixed c1y, fixed c2x, fixed c2y, fixed ex, fixed ey, fixed base_y, fixed height, int *table, int *index, int depth)
{
    fixed test = (sx^(sx<<1))|(sy^(sy<<1))|(c1x^(c1x<<1))|(c1y^(c1y<<1))|(c2x^(c2x<<1))|(c2y^(c2y<<1))|(ex^(ex<<1))|(ey^(ey<<1));

    if (test < 0)
        mark_curve_big(sx, sy, c1x, c1y, c2x, c2y, ex, ey, base_y, height, table, index, depth);
    else
        mark_curve(sx, sy, c1x, c1y, c2x, c2y, ex, ey, base_y, height, table, index, depth);
}

static int make_bbox(gx_path       * path,
               const gs_fixed_rect * clip,
                     gs_fixed_rect * bbox,
                     gs_fixed_rect * ibox,
                     fixed           adjust)
{
    int           code;
    int           ret = 0;

    /* Find the bbox - fixed */
    code = gx_path_bbox(path, bbox);
    if (code < 0)
        return code;

    if (bbox->p.y == bbox->q.y) {
        /* Zero height path */
        if (!clip ||
            (bbox->p.y >= clip->p.y && bbox->q.y <= clip->q.y)) {
            /* Either we're not clipping, or we are vertically inside the clip */
            if (clip) {
                if (bbox->p.x < clip->p.x)
                    bbox->p.x = clip->p.x;
                if (bbox->q.x > clip->q.x)
                    bbox->q.x = clip->q.x;
            }
            if (bbox->p.x <= bbox->q.x) {
                /* Zero height rectangle, not clipped completely away */
                ret = 1;
            }
        }
    }

    if (clip) {
        if (bbox->p.y < clip->p.y)
            bbox->p.y = clip->p.y;
        if (bbox->q.y > clip->q.y)
            bbox->q.y = clip->q.y;
    }

    /* Convert to bbox - int */
    ibox->p.x = fixed2int(bbox->p.x+adjust-(adjust?1:0));
    ibox->p.y = fixed2int(bbox->p.y+adjust-(adjust?1:0));
    ibox->q.x = fixed2int(bbox->q.x-adjust+fixed_1);
    ibox->q.y = fixed2int(bbox->q.y-adjust+fixed_1);

    return ret;
}

static inline int
make_table_template(gx_device     * pdev,
                    gx_path       * path,
                    gs_fixed_rect * ibox,
                    int             intersection_size,
                    int             adjust,
                    int           * scanlinesp,
                    int          ** indexp,
                    int          ** tablep)
{
    int             scanlines;
    const subpath * gs_restrict psub;
    int           * gs_restrict index;
    int           * gs_restrict table;
    int             i;
    int64_t         offset;
    int             delta;
    fixed           base_y;

    *scanlinesp = 0;
    *indexp     = NULL;
    *tablep     = NULL;

    if (pdev->max_fill_band != 0)
        ibox->p.y &= ~(pdev->max_fill_band-1);
    base_y = ibox->p.y;

    /* Previously we took adjust as a fixed distance to add to miny/maxy
     * to allow for the expansion due to 'any part of a pixel'. This causes
     * problems with over/underflow near INT_MAX/INT_MIN, so instead we
     * take adjust as boolean telling us whether to expand y by 1 or not, and
     * then adjust the assignments into the index as appropriate. This
     * solves Bug 697970. */

    /* Step 1: Make us a table */
    scanlines = ibox->q.y-base_y;
    /* +1+adjust simplifies the loop below */
    index = (int *)gs_alloc_bytes(pdev->memory,
                                  (scanlines+1+adjust) * sizeof(*index),
                                  "scanc index buffer");
    if (index == NULL)
        return_error(gs_error_VMerror);

    /* Step 1 continued: Blank the index */
    memset(index, 0, (scanlines+1)*sizeof(int));

    /* Step 1 continued: Run through the path, filling in the index */
    for (psub = path->first_subpath; psub != 0;) {
        const segment * gs_restrict pseg = (const segment *)psub;
        fixed          ey = pseg->pt.y;
        fixed          iy = ey;
        int            iey = fixed2int(iy) - base_y;

        assert(pseg->type == s_start);

        /* Allow for 2 extra intersections on the start scanline.
         * This copes with the 'zero height rectangle' case. */
        if (iey >= 0 && iey < scanlines)
        {
            index[iey] += 2;
            if (iey+1 < scanlines)
                index[iey+1] -= 2;
        }

        while ((pseg = pseg->next) != 0 &&
               pseg->type != s_start
            ) {
            fixed sy = ey;
            ey = pseg->pt.y;

            switch (pseg->type) {
                default:
                case s_start: /* Should never happen */
                case s_dash:  /* We should never be seeing a dash here */
                    assert("This should never happen" == NULL);
                    break;
                case s_curve: {
                    const curve_segment *const gs_restrict pcur = (const curve_segment *)pseg;
                    fixed c1y = pcur->p1.y;
                    fixed c2y = pcur->p2.y;
                    fixed maxy = sy, miny = sy;
                    int imaxy, iminy;
                    if (miny > c1y)
                        miny = c1y;
                    if (miny > c2y)
                        miny = c2y;
                    if (miny > ey)
                        miny = ey;
                    if (maxy < c1y)
                        maxy = c1y;
                    if (maxy < c2y)
                        maxy = c2y;
                    if (maxy < ey)
                        maxy = ey;
#ifdef DEBUG_SCAN_CONVERTER
                    if (debugging_scan_converter)
                        dlprintf2("Curve (%x->%x) ", miny, maxy);
#endif
                    iminy = fixed2int(miny) - base_y;
                    if (iminy <= 0)
                        iminy = 0;
                    else
                        iminy -= adjust;
                    if (iminy < scanlines) {
                        imaxy = fixed2int(maxy) - base_y;
                        if (imaxy >= 0) {
#ifdef DEBUG_SCAN_CONVERTER
                            if (debugging_scan_converter)
                                dlprintf1("+%x ", iminy);
#endif
                            index[iminy]+=3;
                            if (imaxy < scanlines) {
#ifdef DEBUG_SCAN_CONVERTER
                                if (debugging_scan_converter)
                                    dlprintf1("-%x ", imaxy+1);
#endif
                                index[imaxy+1+adjust]-=3;
                            }
                        }
                    }
#ifdef DEBUG_SCAN_CONVERTER
                    if (debugging_scan_converter)
                        dlprintf("\n");
#endif
                    break;
                }
                case s_gap:
                case s_line:
                case s_line_close: {
                    fixed miny, maxy;
                    int imaxy, iminy;
                    if (sy == ey) {
#ifdef DEBUG_SCAN_CONVERTER
                        if (debugging_scan_converter)
                            dlprintf("Line (Horiz)\n");
#endif
                        break;
                    }
                    if (sy < ey)
                        miny = sy, maxy = ey;
                    else
                        miny = ey, maxy = sy;
#ifdef DEBUG_SCAN_CONVERTER
                    if (debugging_scan_converter)
                        dlprintf2("Line (%x->%x) ", miny, maxy);
#endif
                    iminy = fixed2int(miny) - base_y;
                    if (iminy <= 0)
                        iminy = 0;
                    else
                        iminy -= adjust;
                    if (iminy < scanlines) {
                        imaxy = fixed2int(maxy) - base_y;
                        if (imaxy >= 0) {
#ifdef DEBUG_SCAN_CONVERTER
                            if (debugging_scan_converter)
                                dlprintf1("+%x ", iminy);
#endif
                            index[iminy]++;
                            if (imaxy < scanlines) {
#ifdef DEBUG_SCAN_CONVERTER
                                if (debugging_scan_converter)
                                    dlprintf1("-%x ", imaxy+1);
#endif
                                index[imaxy+1+adjust]--;
                            }
                        }
                    }
#ifdef DEBUG_SCAN_CONVERTER
                    if (debugging_scan_converter)
                        dlprintf("\n");
#endif
                    break;
                }
            }
        }

        /* And close any segments that need it */
        if (ey != iy) {
            fixed miny, maxy;
            int imaxy, iminy;
            if (iy < ey)
                miny = iy, maxy = ey;
            else
                miny = ey, maxy = iy;
#ifdef DEBUG_SCAN_CONVERTER
            if (debugging_scan_converter)
                dlprintf2("Close (%x->%x) ", miny, maxy);
#endif
            iminy = fixed2int(miny) - base_y;
            if (iminy <= 0)
                iminy = 0;
            else
                iminy -= adjust;
            if (iminy < scanlines) {
                imaxy = fixed2int(maxy) - base_y;
                if (imaxy >= 0) {
#ifdef DEBUG_SCAN_CONVERTER
                    if (debugging_scan_converter)
                        dlprintf1("+%x ", iminy);
#endif
                    index[iminy]++;
                    if (imaxy < scanlines) {
#ifdef DEBUG_SCAN_CONVERTER
                        if (debugging_scan_converter)
                            dlprintf1("-%x ", imaxy+1);
#endif
                        index[imaxy+1+adjust]--;
                    }
                }
            }
#ifdef DEBUG_SCAN_CONVERTER
            if (debugging_scan_converter)
                dlprintf("\n");
#endif
        }
#ifdef DEBUG_SCAN_CONVERTER
        if (debugging_scan_converter)
            dlprintf("\n");
#endif
        psub = (const subpath *)pseg;
    }

    /* Step 1 continued: index now contains a list of deltas (how the
     * number of intersects on line x differs from the number on line x-1).
     * First convert them to be the real number of intersects on that line.
     * Sum these values to get us the total number of intersects. Then
     * convert the table to be a list of offsets into the real intersect
     * buffer. */
    offset = 0;
    delta  = 0;
    for (i=0; i < scanlines+adjust; i++) {
        delta    += intersection_size*index[i];  /* delta = Num ints on this scanline. */
        index[i]  = offset;                      /* Offset into table for this lines data. */
        offset   += delta+1;                     /* Adjust offset for next line. */
    }
    /* Ensure we always have enough room for our zero height rectangle hack. */
    if (offset < 2*intersection_size)
        offset += 2*intersection_size;
    offset *= sizeof(*table);

    /* Try to keep the size to 1Meg. This is enough for the vast majority
     * of files. Allow us to grow above this if it would mean dropping
     * the height below a suitably small number (set to be larger than
     * any max_fill_band we might meet). */
    if (scanlines > 16 && offset > 1024*1024) { /* Arbitrary */
        gs_free_object(pdev->memory, index, "scanc index buffer");
        return offset/(1024*1024) + 1;
    }

    /* In the case where we have let offset be large, at least make sure
     * it's not TOO large for us to malloc. */
    if (offset != (int64_t)(uint)offset)
    {
        gs_free_object(pdev->memory, index, "scanc index buffer");
        return_error(gs_error_VMerror);
    }

    /* End of step 1: index[i] = offset into table 2 for scanline i's
     * intersection data. offset = Total number of int entries required for
     * table. */

    /* Step 2: Collect the real intersections */
    table = (int *)gs_alloc_bytes(pdev->memory, offset,
                                  "scanc intersects buffer");
    if (table == NULL) {
        gs_free_object(pdev->memory, index, "scanc index buffer");
        return_error(gs_error_VMerror);
    }

    /* Step 2 continued: initialise table's data; each scanlines data starts
     * with a count of the number of intersects so far, followed by a record
     * of the intersect points on this scanline. */
    for (i=0; i < scanlines; i++) {
        table[index[i]] = 0;
    }

    *scanlinesp = scanlines;
    *tablep     = table;
    *indexp     = index;

    return 0;
}

static int make_table(gx_device     * pdev,
                      gx_path       * path,
                      gs_fixed_rect * ibox,
                      int           * scanlines,
                      int          ** index,
                      int          ** table)
{
    return make_table_template(pdev, path, ibox, 1, 1, scanlines, index, table);
}

static void
fill_zero(int *row, const fixed *x)
{
    int n = *row = (*row)+2; /* Increment the count */
    row[n-1] = (x[0]&~1);
    row[n  ] = (x[1]|1);
}

int gx_scan_convert(gx_device     * gs_restrict pdev,
                    gx_path       * gs_restrict path,
              const gs_fixed_rect * gs_restrict clip,
                    gx_edgebuffer * gs_restrict edgebuffer,
                    fixed                       fixed_flat)
{
    gs_fixed_rect  ibox;
    gs_fixed_rect  bbox;
    int            scanlines;
    const subpath *psub;
    int           *index;
    int           *table;
    int            i;
    int            code;
    int            zero;

    edgebuffer->index = NULL;
    edgebuffer->table = NULL;

    /* Bale out if no actual path. We see this with the clist */
    if (path->first_subpath == NULL)
        return 0;

    zero = make_bbox(path, clip, &bbox, &ibox, fixed_half);
    if (zero < 0)
        return zero;

    if (ibox.q.y <= ibox.p.y)
        return 0;

    code = make_table(pdev, path, &ibox, &scanlines, &index, &table);
    if (code != 0) /* >0 means "retry with smaller height" */
        return code;

    if (scanlines == 0)
        return 0;

    if (zero) {
        code = zero_case(pdev, path, &ibox, index, table, fixed_flat, fill_zero);
    } else {

    /* Step 2 continued: Now we run through the path, filling in the real
     * values. */
    for (psub = path->first_subpath; psub != 0;) {
        const segment *pseg = (const segment *)psub;
        fixed ex = pseg->pt.x;
        fixed ey = pseg->pt.y;
        fixed ix = ex;
        fixed iy = ey;

        while ((pseg = pseg->next) != 0 &&
               pseg->type != s_start
            ) {
            fixed sx = ex;
            fixed sy = ey;
            ex = pseg->pt.x;
            ey = pseg->pt.y;

            switch (pseg->type) {
                default:
                case s_start: /* Should never happen */
                case s_dash:  /* We should never be seeing a dash here */
                    assert("This should never happen" == NULL);
                    break;
                case s_curve: {
                    const curve_segment *const pcur = (const curve_segment *)pseg;
                    int k = gx_curve_log2_samples(sx, sy, pcur, fixed_flat);

                    mark_curve_top(sx, sy, pcur->p1.x, pcur->p1.y, pcur->p2.x, pcur->p2.y, ex, ey, ibox.p.y, scanlines, table, index, k);
                    break;
                }
                case s_gap:
                case s_line:
                case s_line_close:
                    if (sy != ey)
                        mark_line(sx, sy, ex, ey, ibox.p.y, scanlines, table, index);
                    break;
            }
        }
        /* And close any open segments */
        if (iy != ey)
            mark_line(ex, ey, ix, iy, ibox.p.y, scanlines, table, index);
        psub = (const subpath *)pseg;
    }
    }

    /* Step 2 complete: We now have a complete list of intersection data in
     * table, indexed by index. */

    edgebuffer->base   = ibox.p.y;
    edgebuffer->height = scanlines;
    edgebuffer->xmin   = ibox.p.x;
    edgebuffer->xmax   = ibox.q.x;
    edgebuffer->index  = index;
    edgebuffer->table  = table;

#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter) {
        dlprintf("Before sorting:\n");
        gx_edgebuffer_print(edgebuffer);
    }
#endif

    /* Step 3: Sort the intersects on x */
    for (i=0; i < scanlines; i++) {
        int *row = &table[index[i]];
        int  rowlen = *row++;

        /* Bubblesort short runs, qsort longer ones. */
        /* FIXME: Check "6" below */
        if (rowlen <= 6) {
            int j, k;
            for (j = 0; j < rowlen-1; j++) {
                int t = row[j];
                for (k = j+1; k < rowlen; k++) {
                    int s = row[k];
                    if (t > s)
                         row[k] = t, t = row[j] = s;
                }
            }
        } else
            qsort(row, rowlen, sizeof(int), intcmp);
    }

    return 0;
}

/* Step 5: Filter the intersections according to the rules */
int
gx_filter_edgebuffer(gx_device       * gs_restrict pdev,
                     gx_edgebuffer   * gs_restrict edgebuffer,
                     int                        rule)
{
    int i;

#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter) {
        dlprintf("Before filtering:\n");
        gx_edgebuffer_print(edgebuffer);
    }
#endif

    for (i=0; i < edgebuffer->height; i++) {
        int *row      = &edgebuffer->table[edgebuffer->index[i]];
        int *rowstart = row;
        int  rowlen   = *row++;
        int *rowout   = row;

        while (rowlen > 0)
        {
            int left, right;

            if (rule == gx_rule_even_odd) {
                /* Even Odd */
                left  = (*row++)&~1;
                right = (*row++)&~1;
                rowlen -= 2;
            } else {
                /* Non-Zero */
                int w;

                left = *row++;
                w = ((left&1)-1) | (left&1);
                rowlen--;
                do {
                    right  = *row++;
                    rowlen--;
                    w += ((right&1)-1) | (right&1);
                } while (w != 0);
                left &= ~1;
                right &= ~1;
            }

            if (right > left) {
                *rowout++ = left;
                *rowout++ = right;
            }
        }
        *rowstart = (rowout-rowstart)-1;
    }
    return 0;
}

/* Step 6: Fill the edgebuffer */
int
gx_fill_edgebuffer(gx_device       * gs_restrict pdev,
             const gx_device_color * gs_restrict pdevc,
                   gx_edgebuffer   * gs_restrict edgebuffer,
                   int                        log_op)
{
    int i, code;

    for (i=0; i < edgebuffer->height; i++) {
        int *row    = &edgebuffer->table[edgebuffer->index[i]];
        int  rowlen = *row++;

        while (rowlen > 0) {
            int left, right;

            left  = *row++;
            right = *row++;
            rowlen -= 2;
            left  = fixed2int(left + fixed_half);
            right = fixed2int(right + fixed_half);
            right -= left;
            if (right > 0) {
#ifdef DEBUG_OUTPUT_SC_AS_PS
                dlprintf("0.001 setlinewidth 1 0.5 0 setrgbcolor %% orange %%PS\n");
                coord("moveto", int2fixed(left), int2fixed(edgebuffer->base+i));
                coord("lineto", int2fixed(left+right), int2fixed(edgebuffer->base+i));
                coord("lineto", int2fixed(left+right), int2fixed(edgebuffer->base+i+1));
                coord("lineto", int2fixed(left), int2fixed(edgebuffer->base+i+1));
                dlprintf("closepath stroke %%PS\n");
#endif
                if (log_op < 0)
                    code = dev_proc(pdev, fill_rectangle)(pdev, left, edgebuffer->base+i, right, 1, pdevc->colors.pure);
                else
                    code = gx_fill_rectangle_device_rop(left, edgebuffer->base+i, right, 1, pdevc, pdev, (gs_logical_operation_t)log_op);
                if (code < 0)
                    return code;
            }
        }
    }
    return 0;
}

/* Any part of a pixel routines */

static int edgecmp(const void *a, const void *b)
{
    int left  = ((int*)a)[0];
    int right = ((int*)b)[0];
    left -= right;
    if (left)
        return left;
    return ((int*)a)[1] - ((int*)b)[1];
}

#ifdef DEBUG_SCAN_CONVERTER
static void
gx_edgebuffer_print_app(gx_edgebuffer * edgebuffer)
{
    int i;
    int borked = 0;

    if (!debugging_scan_converter)
        return;

    dlprintf1("Edgebuffer %x\n", edgebuffer);
    dlprintf4("xmin=%x xmax=%x base=%x height=%x\n",
              edgebuffer->xmin, edgebuffer->xmax, edgebuffer->base, edgebuffer->height);
    for (i=0; i < edgebuffer->height; i++) {
        int  offset = edgebuffer->index[i];
        int *row    = &edgebuffer->table[offset];
        int count   = *row++;
        int c       = count;
        int wind    = 0;
        dlprintf3("%x @ %d: %d =", i, offset, count);
        while (count-- > 0) {
            int left  = *row++;
            int right = *row++;
            int w     = -(left&1) | 1;
            wind += w;
            dlprintf3(" (%x,%x)%c", left&~1, right, left&1 ? 'v' : '^');
        }
        if (wind != 0 || c & 1) {
            dlprintf(" <- BROKEN");
            borked = 1;
        }
        dlprintf("\n");
    }
    if (borked) {
        borked = borked; /* Breakpoint here */
    }
}
#endif

typedef struct
{
    fixed  left;
    fixed  right;
    fixed  y;
    signed char d; /* 0 up (or horiz), 1 down, -1 uninited */
    unsigned char first;
    unsigned char saved;

    fixed  save_left;
    fixed  save_right;
    int    save_iy;
    int    save_d;

    int    scanlines;
    int   *table;
    int   *index;
    int    base;
} cursor;

static inline void
cursor_output(cursor * gs_restrict cr, int iy)
{
    int *row;
    int count;

    if (iy >= 0 && iy < cr->scanlines) {
        if (cr->first) {
            /* Save it for later in case we join up */
            cr->save_left  = cr->left;
            cr->save_right = cr->right;
            cr->save_iy    = iy;
            cr->save_d     = cr->d;
            cr->saved      = 1;
        } else if (cr->d != DIRN_UNSET) {
            /* Enter it into the table */
            row = &cr->table[cr->index[iy]];
            *row = count = (*row)+1; /* Increment the count */
            row[2 * count - 1] = (cr->left&~1) | cr->d;
            row[2 * count    ] = cr->right;
        } else {
            assert(cr->left == max_fixed && cr->right == min_fixed);
        }
    }
    cr->first = 0;
}

static inline void
cursor_output_inrange(cursor * gs_restrict cr, int iy)
{
    int *row;
    int count;

    assert(iy >= 0 && iy < cr->scanlines);
    if (cr->first) {
        /* Save it for later in case we join up */
        cr->save_left  = cr->left;
        cr->save_right = cr->right;
        cr->save_iy    = iy;
        cr->save_d     = cr->d;
        cr->saved      = 1;
    } else {
        /* Enter it into the table */
        assert(cr->d != DIRN_UNSET);

        row = &cr->table[cr->index[iy]];
        *row = count = (*row)+1; /* Increment the count */
        row[2 * count - 1] = (cr->left&~1) | cr->d;
        row[2 * count    ] = cr->right;
    }
    cr->first = 0;
}

/* Step the cursor in y, allowing for maybe crossing a scanline */
static inline void
cursor_step(cursor * gs_restrict cr, fixed dy, fixed x, int skip)
{
    int new_iy;
    int iy = fixed2int(cr->y) - cr->base;

    cr->y += dy;
    new_iy = fixed2int(cr->y) - cr->base;
    if (new_iy != iy) {
        if (!skip)
            cursor_output(cr, iy);
        cr->left = x;
        cr->right = x;
    } else {
        if (x < cr->left)
            cr->left = x;
        if (x > cr->right)
            cr->right = x;
    }
}

/* Step the cursor in y, never by enough to cross a scanline. */
static inline void
cursor_never_step_vertical(cursor * gs_restrict cr, fixed dy, fixed x)
{
    assert(fixed2int(cr->y+dy) == fixed2int(cr->y));

    cr->y += dy;
}

/* Step the cursor in y, never by enough to cross a scanline,
 * knowing that we are moving left, and that the right edge
 * has already been accounted for. */
static inline void
cursor_never_step_left(cursor * gs_restrict cr, fixed dy, fixed x)
{
    assert(fixed2int(cr->y+dy) == fixed2int(cr->y));

    if (x < cr->left)
        cr->left = x;
    cr->y += dy;
}

/* Step the cursor in y, never by enough to cross a scanline,
 * knowing that we are moving right, and that the left edge
 * has already been accounted for. */
static inline void
cursor_never_step_right(cursor * gs_restrict cr, fixed dy, fixed x)
{
    assert(fixed2int(cr->y+dy) == fixed2int(cr->y));

    if (x > cr->right)
        cr->right = x;
    cr->y += dy;
}

/* Step the cursor in y, always by enough to cross a scanline. */
static inline void
cursor_always_step(cursor * gs_restrict cr, fixed dy, fixed x, int skip)
{
    int iy = fixed2int(cr->y) - cr->base;

    if (!skip)
        cursor_output(cr, iy);
    cr->y += dy;
    cr->left = x;
    cr->right = x;
}

/* Step the cursor in y, always by enough to cross a scanline, as
 * part of a vertical line, knowing that we are moving from a
 * position guaranteed to be in the valid y range. */
static inline void
cursor_always_step_inrange_vertical(cursor * gs_restrict cr, fixed dy, fixed x)
{
    int iy = fixed2int(cr->y) - cr->base;

    cursor_output(cr, iy);
    cr->y += dy;
}

/* Step the cursor in y, always by enough to cross a scanline, as
 * part of a left moving line, knowing that we are moving from a
 * position guaranteed to be in the valid y range. */
static inline void
cursor_always_inrange_step_left(cursor * gs_restrict cr, fixed dy, fixed x)
{
    int iy = fixed2int(cr->y) - cr->base;

    cr->y += dy;
    cursor_output_inrange(cr, iy);
    cr->right = x;
}

/* Step the cursor in y, always by enough to cross a scanline, as
 * part of a right moving line, knowing that we are moving from a
 * position guaranteed to be in the valid y range. */
static inline void
cursor_always_inrange_step_right(cursor * gs_restrict cr, fixed dy, fixed x)
{
    int iy = fixed2int(cr->y) - cr->base;

    cr->y += dy;
    cursor_output_inrange(cr, iy);
    cr->left = x;
}

static inline void cursor_init(cursor * gs_restrict cr, fixed y, fixed x)
{
    assert(y >= int2fixed(cr->base) && y <= int2fixed(cr->base + cr->scanlines));

    cr->y = y;
    cr->left = x;
    cr->right = x;
    cr->d = DIRN_UNSET;
}

static inline void cursor_left_merge(cursor * gs_restrict cr, fixed x)
{
    if (x < cr->left)
        cr->left = x;
}

static inline void cursor_left(cursor * gs_restrict cr, fixed x)
{
    cr->left = x;
}

static inline void cursor_right_merge(cursor * gs_restrict cr, fixed x)
{
    if (x > cr->right)
        cr->right = x;
}

static inline void cursor_right(cursor * gs_restrict cr, fixed x)
{
    cr->right = x;
}

static inline int cursor_down(cursor * gs_restrict cr, fixed x)
{
    int skip = 0;
    if ((cr->y & 0xff) == 0)
        skip = 1;
    if (cr->d == DIRN_UP)
    {
        if (!skip)
            cursor_output(cr, fixed2int(cr->y) - cr->base);
        cr->left = x;
        cr->right = x;
    }
    cr->d = DIRN_DOWN;
    return skip;
}

static inline void cursor_up(cursor * gs_restrict cr, fixed x)
{
    if (cr->d == DIRN_DOWN)
    {
        cursor_output(cr, fixed2int(cr->y) - cr->base);
        cr->left = x;
        cr->right = x;
    }
    cr->d = DIRN_UP;
}

static inline void
cursor_flush(cursor * gs_restrict cr, fixed x)
{
    int iy;

    /* This should only happen if we were entirely out of bounds,
     * or if everything was within a zero height horizontal
     * rectangle from the start point. */
    if (cr->first) {
        int iy = fixed2int(cr->y) - cr->base;
        /* Any zero height rectangle counts as filled, except
         * those on the baseline of a pixel. */
        if (cr->d == DIRN_UNSET && (cr->y & 0xff) == 0)
            return;
        assert(cr->left != max_fixed && cr->right != min_fixed);
        if (iy >= 0 && iy < cr->scanlines) {
            int *row = &cr->table[cr->index[iy]];
            int count = *row = (*row)+2; /* Increment the count */
            row[2 * count - 3] = (cr->left & ~1) | DIRN_UP;
            row[2 * count - 2] = (cr->right & ~1);
            row[2 * count - 1] = (cr->right & ~1) | DIRN_DOWN;
            row[2 * count    ] = cr->right;
        }
        return;
    }

    /* Merge save into current if we can */
    iy = fixed2int(cr->y) - cr->base;
    if (cr->saved && iy == cr->save_iy &&
        (cr->d == cr->save_d || cr->save_d == DIRN_UNSET)) {
        if (cr->left > cr->save_left)
            cr->left = cr->save_left;
        if (cr->right < cr->save_right)
            cr->right = cr->save_right;
        cursor_output(cr, iy);
        return;
    }

    /* Merge not possible */
    cursor_output(cr, iy);
    if (cr->saved) {
        cr->left  = cr->save_left;
        cr->right = cr->save_right;
        assert(cr->save_d != DIRN_UNSET);
        if (cr->save_d != DIRN_UNSET)
            cr->d = cr->save_d;
        cursor_output(cr, cr->save_iy);
    }
}

static inline void
cursor_null(cursor *cr)
{
    cr->right = min_fixed;
    cr->left  = max_fixed;
    cr->d     = DIRN_UNSET;
}

static void mark_line_app(cursor * gs_restrict cr, fixed sx, fixed sy, fixed ex, fixed ey)
{
    int isy, iey;
    fixed saved_sy = sy;
    fixed saved_ex = ex;
    fixed saved_ey = ey;
    int truncated;

    if (sx == ex && sy == ey)
        return;

    isy = fixed2int(sy) - cr->base;
    iey = fixed2int(ey) - cr->base;
#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter)
        dlprintf6("Marking line (app) from %x,%x to %x,%x (%x,%x)\n", sx, sy, ex, ey, isy, iey);
#endif
#ifdef DEBUG_OUTPUT_SC_AS_PS
    dlprintf("0.001 setlinewidth 0 0 0 setrgbcolor %%PS\n");
    coord("moveto", sx, sy);
    coord("lineto", ex, ey);
    dlprintf("stroke %%PS\n");
#endif

    /* Horizontal motion at the bottom of a pixel is ignored */
    if (sy == ey && (sy & 0xff) == 0)
        return;

    assert(cr->y == sy &&
           ((cr->left <= sx && cr->right >= sx) || ((sy & 0xff) == 0)) &&
           cr->d >= DIRN_UNSET && cr->d <= DIRN_DOWN);

    if (isy < iey) {
        /* Rising line */
        if (iey < 0 || isy >= cr->scanlines) {
            /* All line is outside. */
            if ((ey & 0xff) == 0)
                cursor_null(cr);
            else {
                cr->left = ex;
                cr->right = ex;
            }
            cr->y = ey;
            cr->first = 0;
            return;
        }
        if (isy < 0) {
            /* Move sy up */
            int64_t y = (int64_t)ey - (int64_t)sy;
            fixed new_sy = int2fixed(cr->base);
            int64_t dy = (int64_t)new_sy - (int64_t)sy;
            sx += (int)((((int64_t)(ex-sx))*dy + y/2)/y);
            sy = new_sy;
            cursor_null(cr);
            cr->y = sy;
            isy = 0;
        }
        truncated = iey > cr->scanlines;
        if (truncated) {
            /* Move ey down */
            int64_t y = ey - sy;
            fixed new_ey = int2fixed(cr->base + cr->scanlines);
            int64_t dy = (int64_t)ey - (int64_t)new_ey;
            saved_ex = ex;
            saved_ey = ey;
            ex -= (int)((((int64_t)(ex-sx))*dy + y/2)/y);
            ey = new_ey;
            iey = cr->scanlines;
        }
    } else {
        /* Falling line */
        if (isy < 0 || iey >= cr->scanlines) {
            /* All line is outside. */
            if ((ey & 0xff) == 0)
                cursor_null(cr);
            else {
                cr->left = ex;
                cr->right = ex;
            }
            cr->y = ey;
            cr->first = 0;
            return;
        }
        truncated = iey < 0;
        if (truncated) {
            /* Move ey up */
            int64_t y = (int64_t)ey - (int64_t)sy;
            fixed new_ey = int2fixed(cr->base);
            int64_t dy = (int64_t)ey - (int64_t)new_ey;
            ex -= (int)((((int64_t)(ex-sx))*dy + y/2)/y);
            ey = new_ey;
            iey = 0;
        }
        if (isy >= cr->scanlines) {
            /* Move sy down */
            int64_t y = (int64_t)ey - (int64_t)sy;
            fixed new_sy = int2fixed(cr->base + cr->scanlines);
            int64_t dy = (int64_t)new_sy - (int64_t)sy;
            sx += (int)((((int64_t)(ex-sx))*dy + y/2)/y);
            sy = new_sy;
            cursor_null(cr);
            cr->y = sy;
            isy = cr->scanlines;
        }
    }

    cursor_left_merge(cr, sx);
    cursor_right_merge(cr, sx);

    assert(cr->left <= sx);
    assert(cr->right >= sx);
    assert(cr->y == sy);

    /* A note: The code below used to be of the form:
     *   if (isy == iey)   ... deal with horizontal lines
     *   else if (ey > sy) {
     *     fixed y_steps = ey - sy;
     *      ... deal with rising lines ...
     *   } else {
     *     fixed y_steps = ey - sy;
     *     ... deal with falling lines
     *   }
     * but that lead to problems, for instance, an example seen
     * has sx=2aa8e, sy=8aee7, ex=7ffc1686, ey=8003e97a.
     * Thus isy=84f, iey=ff80038a. We can see that ey < sy, but
     * sy - ey < 0!
     * We therefore rejig our code so that the choice between
     * cases is done based on the sign of y_steps rather than
     * the relative size of ey and sy.
     */

    /* First, deal with lines that don't change scanline.
     * This accommodates horizontal lines. */
    if (isy == iey) {
        if (saved_sy == saved_ey) {
            /* Horizontal line. Don't change cr->d, don't flush. */
            if ((ey & 0xff) == 0)
                goto no_merge;
        } else if (saved_sy > saved_ey) {
            /* Falling line, flush if previous was rising */
            int skip = cursor_down(cr, sx);
            if ((ey & 0xff) == 0) {
                /* We are falling to the baseline of a subpixel, so output
                 * for the current pixel, and leave the cursor nulled. */
                if (sx <= ex) {
                    cursor_right_merge(cr, ex);
                } else {
                    cursor_left_merge(cr, ex);
                }
                if (!skip)
                    cursor_output(cr, fixed2int(cr->y) - cr->base);
                cursor_null(cr);
                goto no_merge;
            }
        } else {
            /* Rising line, flush if previous was falling */
            cursor_up(cr, sx);
            if ((ey & 0xff) == 0) {
                cursor_null(cr);
                goto no_merge;
            }
        }
        if (sx <= ex) {
            cursor_right_merge(cr, ex);
        } else {
            cursor_left_merge(cr, ex);
        }
no_merge:
        cr->y = ey;
        if (sy > saved_ey)
            goto endFalling;
    } else if (iey > isy) {
        /* We want to change from sy to ey, which are guaranteed to be on
         * different scanlines. We do this in 3 phases.
         * Phase 1 gets us from sy to the next scanline boundary.
         * Phase 2 gets us all the way to the last scanline boundary.
         * Phase 3 gets us from the last scanline boundary to ey.
         */
        /* We want to change from sy to ey, which are guaranteed to be on
         * different scanlines. We do this in 3 phases.
         * Phase 1 gets us from sy to the next scanline boundary. (We may exit after phase 1).
         * Phase 2 gets us all the way to the last scanline boundary. (This may be a null operation)
         * Phase 3 gets us from the last scanline boundary to ey. (We are guaranteed to have output the cursor at least once before phase 3).
         */
        int phase1_y_steps = (-sy) & (fixed_1 - 1);
        int phase3_y_steps = ey & (fixed_1 - 1);
        ufixed y_steps = (ufixed)ey - (ufixed)sy;

        cursor_up(cr, sx);

        if (sx == ex) {
            /* Vertical line. (Rising) */

            /* Phase 1: */
            if (phase1_y_steps) {
                /* If phase 1 will move us into a new scanline, then we must
                 * flush it before we move. */
                cursor_step(cr, phase1_y_steps, sx, 0);
                sy += phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0) {
                    cursor_null(cr);
                    goto end;
                }
            }

            /* Phase 3: precalculation */
            y_steps -= phase3_y_steps;

            /* Phase 2: */
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps > 0) {
                cursor_always_step(cr, fixed_1, sx, 0);
                y_steps--;
                while (y_steps) {
                    cursor_always_step_inrange_vertical(cr, fixed_1, sx);
                    y_steps--;
                }
            }

            /* Phase 3 */
            assert(cr->left == sx && cr->right == sx);
            if (phase3_y_steps == 0)
                cursor_null(cr);
            else
                cr->y += phase3_y_steps;
        } else if (sx < ex) {
            /* Lines increasing in x. (Rightwards, rising) */
            int phase1_x_steps, phase3_x_steps;
            fixed x_steps = ex - sx;

            /* Phase 1: */
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                sx += phase1_x_steps;
                cursor_right_merge(cr, sx);
                x_steps -= phase1_x_steps;
                cursor_step(cr, phase1_y_steps, sx, 0);
                sy += phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0) {
                    cursor_null(cr);
                    goto end;
                }
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;
            assert((y_steps & (fixed_1 - 1)) == 0);

            /* Phase 2: */
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps) {
                /* We want to change sx by x_steps in y_steps steps.
                 * So each step, we add x_steps/y_steps to sx. That's x_inc + n_inc/y_steps. */
                int x_inc = x_steps/y_steps;
                int n_inc = x_steps - (x_inc * y_steps);
                int f = y_steps/2;
                int d = y_steps;

                /* Special casing the first iteration, allows us to simplify
                 * the following loop. */
                sx += x_inc;
                f -= n_inc;
                if (f < 0)
                    f += d, sx++;
                cursor_right_merge(cr, sx);
                cursor_always_step(cr, fixed_1, sx, 0);
                y_steps--;

                while (y_steps) {
                    sx += x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx++;
                    cursor_right(cr, sx);
                    cursor_always_inrange_step_right(cr, fixed_1, sx);
                    y_steps--;
                };
            }

            /* Phase 3 */
            assert(cr->left <= ex && cr->right >= sx);
            if (phase3_y_steps == 0)
                cursor_null(cr);
            else {
                cursor_right(cr, ex);
                cr->y += phase3_y_steps;
            }
        } else {
            /* Lines decreasing in x. (Leftwards, rising) */
            int phase1_x_steps, phase3_x_steps;
            fixed x_steps = sx - ex;

            /* Phase 1: */
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                x_steps -= phase1_x_steps;
                sx -= phase1_x_steps;
                cursor_left_merge(cr, sx);
                cursor_step(cr, phase1_y_steps, sx, 0);
                sy += phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0) {
                    cursor_null(cr);
                    goto end;
                }
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;
            assert((y_steps & (fixed_1 - 1)) == 0);

            /* Phase 2: */
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps) {
                /* We want to change sx by x_steps in y_steps steps.
                 * So each step, we sub x_steps/y_steps from sx. That's x_inc + n_inc/ey. */
                int x_inc = x_steps/y_steps;
                int n_inc = x_steps - (x_inc * y_steps);
                int f = y_steps/2;
                int d = y_steps;

                /* Special casing the first iteration, allows us to simplify
                 * the following loop. */
                sx -= x_inc;
                f -= n_inc;
                if (f < 0)
                    f += d, sx--;
                cursor_left_merge(cr, sx);
                cursor_always_step(cr, fixed_1, sx, 0);
                y_steps--;

                while (y_steps) {
                    sx -= x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx--;
                    cursor_left(cr, sx);
                    cursor_always_inrange_step_left(cr, fixed_1, sx);
                    y_steps--;
                }
            }

            /* Phase 3 */
            assert(cr->right >= ex && cr->left <= sx);
            if (phase3_y_steps == 0)
                cursor_null(cr);
            else {
                cursor_left(cr, ex);
                cr->y += phase3_y_steps;
            }
        }
    } else {
        /* So lines decreasing in y. */
        /* We want to change from sy to ey, which are guaranteed to be on
         * different scanlines. We do this in 3 phases.
         * Phase 1 gets us from sy to the next scanline boundary. This never causes an output.
         * Phase 2 gets us all the way to the last scanline boundary. This is guaranteed to cause an output.
         * Phase 3 gets us from the last scanline boundary to ey. We are guaranteed to have outputted by now.
         */
        int phase1_y_steps = sy & (fixed_1 - 1);
        int phase3_y_steps = (-ey) & (fixed_1 - 1);
        ufixed y_steps = (ufixed)sy - (ufixed)ey;

        int skip = cursor_down(cr, sx);

        if (sx == ex) {
            /* Vertical line. (Falling) */

            /* Phase 1: */
            if (phase1_y_steps) {
                /* Phase 1 in a falling line never moves us into a new scanline. */
                cursor_never_step_vertical(cr, -phase1_y_steps, sx);
                sy -= phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0)
                    goto endFallingLeftOnEdgeOfPixel;
            }

            /* Phase 3: precalculation */
            y_steps -= phase3_y_steps;
            assert((y_steps & (fixed_1 - 1)) == 0);

            /* Phase 2: */
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps) {
                cursor_always_step(cr, -fixed_1, sx, skip);
                skip = 0;
                y_steps--;
                while (y_steps) {
                    cursor_always_step_inrange_vertical(cr, -fixed_1, sx);
                    y_steps--;
                }
            }

            /* Phase 3 */
            if (phase3_y_steps == 0) {
endFallingLeftOnEdgeOfPixel:
                cursor_always_step_inrange_vertical(cr, 0, sx);
                cursor_null(cr);
            } else {
                cursor_step(cr, -phase3_y_steps, sx, skip);
                assert(cr->left == sx && cr->right == sx);
            }
        } else if (sx < ex) {
            /* Lines increasing in x. (Rightwards, falling) */
            int phase1_x_steps, phase3_x_steps;
            fixed x_steps = ex - sx;

            /* Phase 1: */
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                x_steps -= phase1_x_steps;
                sx += phase1_x_steps;
                /* Phase 1 in a falling line never moves us into a new scanline. */
                cursor_never_step_right(cr, -phase1_y_steps, sx);
                sy -= phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0)
                    goto endFallingRightOnEdgeOfPixel;
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;
            assert((y_steps & (fixed_1 - 1)) == 0);

            /* Phase 2: */
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps) {
                /* We want to change sx by x_steps in y_steps steps.
                 * So each step, we add x_steps/y_steps to sx. That's x_inc + n_inc/ey. */
                int x_inc = x_steps/y_steps;
                int n_inc = x_steps - (x_inc * y_steps);
                int f = y_steps/2;
                int d = y_steps;

                cursor_always_step(cr, -fixed_1, sx, skip);
                skip = 0;
                sx += x_inc;
                f -= n_inc;
                if (f < 0)
                    f += d, sx++;
                cursor_right(cr, sx);
                y_steps--;

                while (y_steps) {
                    cursor_always_inrange_step_right(cr, -fixed_1, sx);
                    sx += x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx++;
                    cursor_right(cr, sx);
                    y_steps--;
                }
            }

            /* Phase 3 */
            if (phase3_y_steps == 0) {
endFallingRightOnEdgeOfPixel:
                cursor_always_step_inrange_vertical(cr, 0, sx);
                cursor_null(cr);
            } else {
                cursor_step(cr, -phase3_y_steps, sx, skip);
                cursor_right(cr, ex);
                assert(cr->left == sx && cr->right == ex);
            }
        } else {
            /* Lines decreasing in x. (Falling) */
            int phase1_x_steps, phase3_x_steps;
            fixed x_steps = sx - ex;

            /* Phase 1: */
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                x_steps -= phase1_x_steps;
                sx -= phase1_x_steps;
                /* Phase 1 in a falling line never moves us into a new scanline. */
                cursor_never_step_left(cr, -phase1_y_steps, sx);
                sy -= phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0)
                    goto endFallingVerticalOnEdgeOfPixel;
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;
            assert((y_steps & (fixed_1 - 1)) == 0);

            /* Phase 2: */
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps) {
                /* We want to change sx by x_steps in y_steps steps.
                 * So each step, we sub x_steps/y_steps from sx. That's x_inc + n_inc/ey. */
                int x_inc = x_steps/y_steps;
                int n_inc = x_steps - (x_inc * y_steps);
                int f = y_steps/2;
                int d = y_steps;

                cursor_always_step(cr, -fixed_1, sx, skip);
                skip = 0;
                sx -= x_inc;
                f -= n_inc;
                if (f < 0)
                    f += d, sx--;
                cursor_left(cr, sx);
                y_steps--;

                while (y_steps) {
                    cursor_always_inrange_step_left(cr, -fixed_1, sx);
                    sx -= x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx--;
                    cursor_left(cr, sx);
                    y_steps--;
                }
            }

            /* Phase 3 */
            if (phase3_y_steps == 0) {
endFallingVerticalOnEdgeOfPixel:
                cursor_always_step_inrange_vertical(cr, 0, sx);
                cursor_null(cr);
            } else {
                cursor_step(cr, -phase3_y_steps, sx, skip);
                cursor_left(cr, ex);
                assert(cr->left == ex && cr->right == sx);
            }
        }
endFalling: {}
    }

end:
    if (truncated) {
        cr->left = saved_ex;
        cr->right = saved_ex;
        cr->y = saved_ey;
    }
}

static void mark_curve_app(cursor *cr, fixed sx, fixed sy, fixed c1x, fixed c1y, fixed c2x, fixed c2y, fixed ex, fixed ey, int depth)
{
        int ax = (sx + c1x)>>1;
        int ay = (sy + c1y)>>1;
        int bx = (c1x + c2x)>>1;
        int by = (c1y + c2y)>>1;
        int cx = (c2x + ex)>>1;
        int cy = (c2y + ey)>>1;
        int dx = (ax + bx)>>1;
        int dy = (ay + by)>>1;
        int fx = (bx + cx)>>1;
        int fy = (by + cy)>>1;
        int gx = (dx + fx)>>1;
        int gy = (dy + fy)>>1;

        assert(depth >= 0);
        if (depth == 0)
            mark_line_app(cr, sx, sy, ex, ey);
        else {
            depth--;
            mark_curve_app(cr, sx, sy, ax, ay, dx, dy, gx, gy, depth);
            mark_curve_app(cr, gx, gy, fx, fy, cx, cy, ex, ey, depth);
        }
}

static void mark_curve_big_app(cursor *cr, fixed64 sx, fixed64 sy, fixed64 c1x, fixed64 c1y, fixed64 c2x, fixed64 c2y, fixed64 ex, fixed64 ey, int depth)
{
    fixed64 ax = (sx + c1x)>>1;
    fixed64 ay = (sy + c1y)>>1;
    fixed64 bx = (c1x + c2x)>>1;
    fixed64 by = (c1y + c2y)>>1;
    fixed64 cx = (c2x + ex)>>1;
    fixed64 cy = (c2y + ey)>>1;
    fixed64 dx = (ax + bx)>>1;
    fixed64 dy = (ay + by)>>1;
    fixed64 fx = (bx + cx)>>1;
    fixed64 fy = (by + cy)>>1;
    fixed64 gx = (dx + fx)>>1;
    fixed64 gy = (dy + fy)>>1;

    assert(depth >= 0);
    if (depth == 0)
        mark_line_app(cr, (fixed)sx, (fixed)sy, (fixed)ex, (fixed)ey);
    else {
        depth--;
        mark_curve_big_app(cr, sx, sy, ax, ay, dx, dy, gx, gy, depth);
        mark_curve_big_app(cr, gx, gy, fx, fy, cx, cy, ex, ey, depth);
    }
}

static void mark_curve_top_app(cursor *cr, fixed sx, fixed sy, fixed c1x, fixed c1y, fixed c2x, fixed c2y, fixed ex, fixed ey, int depth)
{
    fixed test = (sx^(sx<<1))|(sy^(sy<<1))|(c1x^(c1x<<1))|(c1y^(c1y<<1))|(c2x^(c2x<<1))|(c2y^(c2y<<1))|(ex^(ex<<1))|(ey^(ey<<1));

    if (test < 0)
        mark_curve_big_app(cr, sx, sy, c1x, c1y, c2x, c2y, ex, ey, depth);
    else
        mark_curve_app(cr, sx, sy, c1x, c1y, c2x, c2y, ex, ey, depth);
}

static int make_table_app(gx_device     * pdev,
                          gx_path       * path,
                          gs_fixed_rect * ibox,
                          int           * scanlines,
                          int          ** index,
                          int          ** table)
{
    return make_table_template(pdev, path, ibox, 2, 0, scanlines, index, table);
}

static void
fill_zero_app(int *row, const fixed *x)
{
    int n = *row = (*row)+2; /* Increment the count */
    row[2*n-3] = (x[0]&~1);
    row[2*n-2] = (x[1]&~1);
    row[2*n-1] = (x[1]&~1)|1;
    row[2*n  ] = x[1];
}

int gx_scan_convert_app(gx_device     * gs_restrict pdev,
                        gx_path       * gs_restrict path,
                  const gs_fixed_rect * gs_restrict clip,
                        gx_edgebuffer * gs_restrict edgebuffer,
                        fixed                    fixed_flat)
{
    gs_fixed_rect  ibox;
    gs_fixed_rect  bbox;
    int            scanlines;
    const subpath *psub;
    int           *index;
    int           *table;
    int            i;
    cursor         cr;
    int            code;
    int            zero;

    edgebuffer->index = NULL;
    edgebuffer->table = NULL;

    /* Bale out if no actual path. We see this with the clist */
    if (path->first_subpath == NULL)
        return 0;

    zero = make_bbox(path, clip, &bbox, &ibox, 0);
    if (zero < 0)
        return zero;

    if (ibox.q.y <= ibox.p.y)
        return 0;

    code = make_table_app(pdev, path, &ibox, &scanlines, &index, &table);
    if (code != 0) /* > 0 means "retry with smaller height" */
        return code;

    if (scanlines == 0)
        return 0;

    if (zero) {
        code = zero_case(pdev, path, &ibox, index, table, fixed_flat, fill_zero_app);
    } else {

    /* Step 2 continued: Now we run through the path, filling in the real
     * values. */
    cr.scanlines = scanlines;
    cr.index     = index;
    cr.table     = table;
    cr.base      = ibox.p.y;
    for (psub = path->first_subpath; psub != 0;) {
        const segment *pseg = (const segment *)psub;
        fixed ex = pseg->pt.x;
        fixed ey = pseg->pt.y;
        fixed ix = ex;
        fixed iy = ey;
        fixed sx, sy;

        if ((ey & 0xff) == 0) {
            cr.left  = max_fixed;
            cr.right = min_fixed;
        } else {
            cr.left = cr.right = ex;
        }
        cr.y = ey;
        cr.d = DIRN_UNSET;
        cr.first = 1;
        cr.saved = 0;

        while ((pseg = pseg->next) != 0 &&
               pseg->type != s_start
            ) {
            sx = ex;
            sy = ey;
            ex = pseg->pt.x;
            ey = pseg->pt.y;

            switch (pseg->type) {
                default:
                case s_start: /* Should never happen */
                case s_dash:  /* We should never be seeing a dash here */
                    assert("This should never happen" == NULL);
                    break;
                case s_curve: {
                    const curve_segment *const pcur = (const curve_segment *)pseg;
                    int k = gx_curve_log2_samples(sx, sy, pcur, fixed_flat);

                    mark_curve_top_app(&cr, sx, sy, pcur->p1.x, pcur->p1.y, pcur->p2.x, pcur->p2.y, ex, ey, k);
                    break;
                }
                case s_gap:
                case s_line:
                case s_line_close:
                    mark_line_app(&cr, sx, sy, ex, ey);
                    break;
            }
        }
        /* And close any open segments */
        mark_line_app(&cr, ex, ey, ix, iy);
        cursor_flush(&cr, ex);
        psub = (const subpath *)pseg;
    }
    }

    /* Step 2 complete: We now have a complete list of intersection data in
     * table, indexed by index. */

    edgebuffer->base   = ibox.p.y;
    edgebuffer->height = scanlines;
    edgebuffer->xmin   = ibox.p.x;
    edgebuffer->xmax   = ibox.q.x;
    edgebuffer->index  = index;
    edgebuffer->table  = table;

#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter) {
        dlprintf("Before sorting:\n");
        gx_edgebuffer_print_app(edgebuffer);
    }
#endif

    /* Step 3: Sort the intersects on x */
    for (i=0; i < scanlines; i++) {
        int *row = &table[index[i]];
        int  rowlen = *row++;

        /* Bubblesort short runs, qsort longer ones. */
        /* FIXME: Verify the figure 6 below */
        if (rowlen <= 6) {
            int j, k;
            for (j = 0; j < rowlen-1; j++) {
                int * gs_restrict t = &row[j<<1];
                for (k = j+1; k < rowlen; k++) {
                    int * gs_restrict s = &row[k<<1];
                    int tmp;
                    if (t[0] < s[0])
                        continue;
                    if (t[0] > s[0])
                        goto swap01;
                    if (t[1] <= s[1])
                        continue;
                    if (0) {
swap01:
                        tmp = t[0], t[0] = s[0], s[0] = tmp;
                    }
                    tmp = t[1], t[1] = s[1], s[1] = tmp;
                }
            }
        } else
            qsort(row, rowlen, 2*sizeof(int), edgecmp);
    }

    return 0;
}

/* Step 5: Filter the intersections according to the rules */
int
gx_filter_edgebuffer_app(gx_device       * gs_restrict pdev,
                         gx_edgebuffer   * gs_restrict edgebuffer,
                         int                        rule)
{
    int i;

#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter) {
        dlprintf("Before filtering:\n");
        gx_edgebuffer_print_app(edgebuffer);
    }
#endif

    for (i=0; i < edgebuffer->height; i++) {
        int *row      = &edgebuffer->table[edgebuffer->index[i]];
        int  rowlen   = *row++;
        int *rowstart = row;
        int *rowout   = row;
        int  ll, lr, rl, rr, wind, marked_to;

        /* Avoid double setting pixels, by keeping where we have marked to. */
        marked_to = INT_MIN;
        while (rowlen > 0) {
            if (rule == gx_rule_even_odd) {
                /* Even Odd */
                ll = (*row++)&~1;
                lr = *row;
                row += 2;
                rowlen-=2;

                /* We will fill solidly from ll to at least lr, possibly further */
                assert(rowlen >= 0);
                rr = (*row++);
                if (rr > lr)
                    lr = rr;
            } else {
                /* Non-Zero */
                int w;

                ll = *row++;
                lr = *row++;
                wind = -(ll&1) | 1;
                ll &= ~1;
                rowlen--;

                assert(rowlen > 0);
                do {
                    rl = *row++;
                    rr = *row++;
                    w = -(rl&1) | 1;
                    rl &= ~1;
                    rowlen--;
                    if (rr > lr)
                        lr = rr;
                    wind += w;
                    if (wind == 0)
                        break;
                } while (rowlen > 0);
            }

            if (marked_to >= lr)
                continue;

            if (marked_to >= ll) {
                if (rowout == rowstart)
                    ll = marked_to;
                else {
                    rowout -= 2;
                    ll = *rowout;
                }
            }

            if (lr >= ll) {
                *rowout++ = ll;
                *rowout++ = lr;
                marked_to = lr;
            }
        }
        rowstart[-1] = rowout - rowstart;
    }
    return 0;
}

/* Step 6: Fill */
int
gx_fill_edgebuffer_app(gx_device       * gs_restrict pdev,
                 const gx_device_color * gs_restrict pdevc,
                       gx_edgebuffer   * gs_restrict edgebuffer,
                       int                        log_op)
{
    int i, code;

    for (i=0; i < edgebuffer->height; i++) {
        int *row    = &edgebuffer->table[edgebuffer->index[i]];
        int  rowlen = *row++;
        int  left, right;

        while (rowlen > 0) {
            left  = *row++;
            right = *row++;
            left  = fixed2int(left);
            right = fixed2int(right + fixed_1 - 1);
            rowlen -= 2;

            right -= left;
            if (right > 0) {
                if (log_op < 0)
                    code = dev_proc(pdev, fill_rectangle)(pdev, left, edgebuffer->base+i, right, 1, pdevc->colors.pure);
                else
                    code = gx_fill_rectangle_device_rop(left, edgebuffer->base+i, right, 1, pdevc, pdev, (gs_logical_operation_t)log_op);
                if (code < 0)
                    return code;
            }
        }
    }
    return 0;
}

/* Centre of a pixel trapezoid routines */

static int intcmp_tr(const void *a, const void *b)
{
    int left  = ((int*)a)[0];
    int right = ((int*)b)[0];
    if (left != right)
        return left - right;
    return ((int*)a)[1] - ((int*)b)[1];
}

#ifdef DEBUG_SCAN_CONVERTER
static void
gx_edgebuffer_print_tr(gx_edgebuffer * edgebuffer)
{
    int i;

    if (!debugging_scan_converter)
        return;

    dlprintf1("Edgebuffer %x\n", edgebuffer);
    dlprintf4("xmin=%x xmax=%x base=%x height=%x\n",
              edgebuffer->xmin, edgebuffer->xmax, edgebuffer->base, edgebuffer->height);
    for (i=0; i < edgebuffer->height; i++)
    {
        int  offset = edgebuffer->index[i];
        int *row    = &edgebuffer->table[offset];
        int count   = *row++;
        dlprintf3("%d @ %d: %d =", i, offset, count);
        while (count-- > 0) {
            int e  = *row++;
            int id = *row++;
            dlprintf3(" %x%c%d", e, id&1 ? 'v' : '^', id>>1);
        }
        dlprintf("\n");
    }
}
#endif

static void mark_line_tr(fixed sx, fixed sy, fixed ex, fixed ey, int base_y, int height, int *table, int *index, int id)
{
    int64_t delta;
    int iy, ih;
    fixed clip_sy, clip_ey;
    int dirn = DIRN_UP;
    int *row;

#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter)
        dlprintf6("Marking line (tr) from %x,%x to %x,%x (%x,%x)\n", sx, sy, ex, ey, fixed2int(sy + fixed_half-1) - base_y, fixed2int(ey + fixed_half-1) - base_y);
#endif
#ifdef DEBUG_OUTPUT_SC_AS_PS
    dlprintf("0.001 setlinewidth 0 0 0 setrgbcolor %%PS\n");
    coord("moveto", sx, sy);
    coord("lineto", ex, ey);
    dlprintf("stroke %%PS\n");
#endif

    if (fixed2int(sy + fixed_half-1) == fixed2int(ey + fixed_half-1))
        return;
    if (sy > ey) {
        int t;
        t = sy; sy = ey; ey = t;
        t = sx; sx = ex; ex = t;
        dirn = DIRN_DOWN;
    }
    /* Lines go from sy to ey, closed at the start, open at the end. */
    /* We clip them to a region to make them closed at both ends. */
    /* Thus the first scanline marked (>= sy) is: */
    clip_sy = ((sy + fixed_half - 1) & ~(fixed_1-1)) | fixed_half;
    /* The last scanline marked (< ey) is: */
    clip_ey = ((ey - fixed_half - 1) & ~(fixed_1-1)) | fixed_half;
    /* Now allow for banding */
    if (clip_sy < int2fixed(base_y) + fixed_half)
        clip_sy = int2fixed(base_y) + fixed_half;
    if (ey <= clip_sy)
        return;
    if (clip_ey > int2fixed(base_y + height - 1) + fixed_half)
        clip_ey = int2fixed(base_y + height - 1) + fixed_half;
    if (sy > clip_ey)
        return;
    delta = (int64_t)clip_sy - (int64_t)sy;
    if (delta > 0)
    {
        int64_t dx = (int64_t)ex - (int64_t)sx;
        int64_t dy = (int64_t)ey - (int64_t)sy;
        int advance = (int)((dx * delta + (dy>>1)) / dy);
        sx += advance;
        sy += delta;
    }
    delta = (int64_t)ey - (int64_t)clip_ey;
    if (delta > 0)
    {
        int64_t dx = (int64_t)ex - (int64_t)sx;
        int64_t dy = (int64_t)ey - (int64_t)sy;
        int advance = (int)((dx * delta + (dy>>1)) / dy);
        ex -= advance;
        ey -= delta;
    }
    ex -= sx;
    ey -= sy;
    ih = fixed2int(ey);
    assert(ih >= 0);
    iy = fixed2int(sy) - base_y;
#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter)
        dlprintf2("    iy=%x ih=%x\n", iy, ih);
#endif
    assert(iy >= 0 && iy < height);
    id = (id<<1) | dirn;
    /* We always cross at least one scanline */
    row = &table[index[iy]];
    *row = (*row)+1; /* Increment the count */
    row[*row * 2 - 1] = sx;
    row[*row * 2    ] = id;
    if (ih == 0)
        return;
    if (ex >= 0) {
        int x_inc, n_inc, f;

        /* We want to change sx by ex in ih steps. So each step, we add
         * ex/ih to sx. That's x_inc + n_inc/ih.
         */
        x_inc = ex/ih;
        n_inc = ex-(x_inc*ih);
        f     = ih>>1;
        delta = ih;
        do {
            int count;
            iy++;
            sx += x_inc;
            f  -= n_inc;
            if (f < 0) {
                f += ih;
                sx++;
            }
            assert(iy >= 0 && iy < height);
            row = &table[index[iy]];
            count = *row = (*row)+1; /* Increment the count */
            row[count * 2 - 1] = sx;
            row[count * 2    ] = id;
        }
        while (--delta);
    } else {
        int x_dec, n_dec, f;

        ex = -ex;
        /* We want to change sx by ex in ih steps. So each step, we subtract
         * ex/ih from sx. That's x_dec + n_dec/ih.
         */
        x_dec = ex/ih;
        n_dec = ex-(x_dec*ih);
        f     = ih>>1;
        delta = ih;
        do {
            int count;
            iy++;
            sx -= x_dec;
            f  -= n_dec;
            if (f < 0) {
                f += ih;
                sx--;
            }
            assert(iy >= 0 && iy < height);
            row = &table[index[iy]];
            count = *row = (*row)+1; /* Increment the count */
            row[count * 2 - 1] = sx;
            row[count * 2    ] = id;
         }
         while (--delta);
    }
}

static void mark_curve_tr(fixed sx, fixed sy, fixed c1x, fixed c1y, fixed c2x, fixed c2y, fixed ex, fixed ey, fixed base_y, fixed height, int *table, int *index, int *id, int depth)
{
    fixed ax = (sx + c1x)>>1;
    fixed ay = (sy + c1y)>>1;
    fixed bx = (c1x + c2x)>>1;
    fixed by = (c1y + c2y)>>1;
    fixed cx = (c2x + ex)>>1;
    fixed cy = (c2y + ey)>>1;
    fixed dx = (ax + bx)>>1;
    fixed dy = (ay + by)>>1;
    fixed fx = (bx + cx)>>1;
    fixed fy = (by + cy)>>1;
    fixed gx = (dx + fx)>>1;
    fixed gy = (dy + fy)>>1;

    assert(depth >= 0);
    if (depth == 0) {
        *id += 1;
        mark_line_tr(sx, sy, ex, ey, base_y, height, table, index, *id);
    } else {
        depth--;
        mark_curve_tr(sx, sy, ax, ay, dx, dy, gx, gy, base_y, height, table, index, id, depth);
        mark_curve_tr(gx, gy, fx, fy, cx, cy, ex, ey, base_y, height, table, index, id, depth);
    }
}

static void mark_curve_big_tr(fixed64 sx, fixed64 sy, fixed64 c1x, fixed64 c1y, fixed64 c2x, fixed64 c2y, fixed64 ex, fixed64 ey, fixed base_y, fixed height, int *table, int *index, int *id, int depth)
{
    fixed64 ax = (sx + c1x)>>1;
    fixed64 ay = (sy + c1y)>>1;
    fixed64 bx = (c1x + c2x)>>1;
    fixed64 by = (c1y + c2y)>>1;
    fixed64 cx = (c2x + ex)>>1;
    fixed64 cy = (c2y + ey)>>1;
    fixed64 dx = (ax + bx)>>1;
    fixed64 dy = (ay + by)>>1;
    fixed64 fx = (bx + cx)>>1;
    fixed64 fy = (by + cy)>>1;
    fixed64 gx = (dx + fx)>>1;
    fixed64 gy = (dy + fy)>>1;

    assert(depth >= 0);
    if (depth == 0) {
        *id += 1;
        mark_line_tr((fixed)sx, (fixed)sy, (fixed)ex, (fixed)ey, base_y, height, table, index, *id);
    } else {
        depth--;
        mark_curve_big_tr(sx, sy, ax, ay, dx, dy, gx, gy, base_y, height, table, index, id, depth);
        mark_curve_big_tr(gx, gy, fx, fy, cx, cy, ex, ey, base_y, height, table, index, id, depth);
    }
}

static void mark_curve_top_tr(fixed sx, fixed sy, fixed c1x, fixed c1y, fixed c2x, fixed c2y, fixed ex, fixed ey, fixed base_y, fixed height, int *table, int *index, int *id, int depth)
{
    fixed test = (sx^(sx<<1))|(sy^(sy<<1))|(c1x^(c1x<<1))|(c1y^(c1y<<1))|(c2x^(c2x<<1))|(c2y^(c2y<<1))|(ex^(ex<<1))|(ey^(ey<<1));

    if (test < 0)
        mark_curve_big_tr(sx, sy, c1x, c1y, c2x, c2y, ex, ey, base_y, height, table, index, id, depth);
    else
        mark_curve_tr(sx, sy, c1x, c1y, c2x, c2y, ex, ey, base_y, height, table, index, id, depth);
}

static int make_table_tr(gx_device     * pdev,
                         gx_path       * path,
                         gs_fixed_rect * ibox,
                         int           * scanlines,
                         int          ** index,
                         int          ** table)
{
    return make_table_template(pdev, path, ibox, 2, 1, scanlines, index, table);
}

static void
fill_zero_tr(int *row, const fixed *x)
{
    int n = *row = (*row)+2; /* Increment the count */
    row[2*n-3] = x[0];
    row[2*n-2] = 0;
    row[2*n-1] = x[1];
    row[2*n  ] = 1;
}

int gx_scan_convert_tr(gx_device     * gs_restrict pdev,
                       gx_path       * gs_restrict path,
                 const gs_fixed_rect * gs_restrict clip,
                       gx_edgebuffer * gs_restrict edgebuffer,
                       fixed                    fixed_flat)
{
    gs_fixed_rect  ibox;
    gs_fixed_rect  bbox;
    int            scanlines;
    const subpath *psub;
    int           *index;
    int           *table;
    int            i;
    int            code;
    int            id = 0;
    int            zero;

    edgebuffer->index = NULL;
    edgebuffer->table = NULL;

    /* Bale out if no actual path. We see this with the clist */
    if (path->first_subpath == NULL)
        return 0;

    zero = make_bbox(path, clip, &bbox, &ibox, fixed_half);
    if (zero < 0)
        return zero;

    if (ibox.q.y <= ibox.p.y)
        return 0;

    code = make_table_tr(pdev, path, &ibox, &scanlines, &index, &table);
    if (code != 0) /* > 0 means "retry with smaller height" */
        return code;

    if (scanlines == 0)
        return 0;

    if (zero) {
        code = zero_case(pdev, path, &ibox, index, table, fixed_flat, fill_zero_tr);
    } else {

    /* Step 3: Now we run through the path, filling in the real
     * values. */
    for (psub = path->first_subpath; psub != 0;) {
        const segment *pseg = (const segment *)psub;
        fixed ex = pseg->pt.x;
        fixed ey = pseg->pt.y;
        fixed ix = ex;
        fixed iy = ey;

        while ((pseg = pseg->next) != 0 &&
               pseg->type != s_start
            ) {
            fixed sx = ex;
            fixed sy = ey;
            ex = pseg->pt.x;
            ey = pseg->pt.y;

            switch (pseg->type) {
                default:
                case s_start: /* Should never happen */
                case s_dash:  /* We should never be seeing a dash here */
                    assert("This should never happen" == NULL);
                    break;
                case s_curve: {
                    const curve_segment *const pcur = (const curve_segment *)pseg;
                    int k = gx_curve_log2_samples(sx, sy, pcur, fixed_flat);

                    mark_curve_top_tr(sx, sy, pcur->p1.x, pcur->p1.y, pcur->p2.x, pcur->p2.y, ex, ey, ibox.p.y, scanlines, table, index, &id, k);
                    break;
                }
                case s_gap:
                case s_line:
                case s_line_close:
                    if (sy != ey)
                        mark_line_tr(sx, sy, ex, ey, ibox.p.y, scanlines, table, index, ++id);
                    break;
            }
        }
        /* And close any open segments */
        if (iy != ey)
            mark_line_tr(ex, ey, ix, iy, ibox.p.y, scanlines, table, index, ++id);
        psub = (const subpath *)pseg;
    }
    }

    //if (zero) {
    //    if (table[0] == 0) {
    //        /* Zero height rectangle fills a span */
    //        table[0] = 2;
    //        table[1] = int2fixed(fixed2int(bbox.p.x + fixed_half));
    //        table[2] = 0;
    //        table[3] = int2fixed(fixed2int(bbox.q.x + fixed_half));
    //        table[4] = 1;
    //    }
    //}

    /* Step 2 complete: We now have a complete list of intersection data in
     * table, indexed by index. */

    edgebuffer->base   = ibox.p.y;
    edgebuffer->height = scanlines;
    edgebuffer->xmin   = ibox.p.x;
    edgebuffer->xmax   = ibox.q.x;
    edgebuffer->index  = index;
    edgebuffer->table  = table;

#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter) {
        dlprintf("Before sorting:\n");
        gx_edgebuffer_print_tr(edgebuffer);
    }
#endif

    /* Step 4: Sort the intersects on x */
    for (i=0; i < scanlines; i++) {
        int *row = &table[index[i]];
        int  rowlen = *row++;

        /* Bubblesort short runs, qsort longer ones. */
        /* FIXME: Verify the figure 6 below */
        if (rowlen <= 6) {
            int j, k;
            for (j = 0; j < rowlen-1; j++) {
                int * gs_restrict t = &row[j<<1];
                for (k = j+1; k < rowlen; k++) {
                    int * gs_restrict s = &row[k<<1];
                    int tmp;
                    if (t[0] < s[0])
                        continue;
                    if (t[0] == s[0]) {
                        if (t[1] <= s[1])
                            continue;
                    } else
                        tmp = t[0], t[0] = s[0], s[0] = tmp;
                    tmp = t[1], t[1] = s[1], s[1] = tmp;
                }
            }
        } else
            qsort(row, rowlen, 2*sizeof(int), intcmp_tr);
    }

    return 0;
}

/* Step 5: Filter the intersections according to the rules */
int
gx_filter_edgebuffer_tr(gx_device       * gs_restrict pdev,
                        gx_edgebuffer   * gs_restrict edgebuffer,
                        int                           rule)
{
    int i;

#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter) {
        dlprintf("Before filtering\n");
        gx_edgebuffer_print_tr(edgebuffer);
    }
#endif

    for (i=0; i < edgebuffer->height; i++) {
        int *row      = &edgebuffer->table[edgebuffer->index[i]];
        int  rowlen   = *row++;
        int *rowstart = row;
        int *rowout   = row;

        while (rowlen > 0) {
            int left, lid, right, rid;

            if (rule == gx_rule_even_odd) {
                /* Even Odd */
                left  = *row++;
                lid   = *row++;
                right = *row++;
                rid   = *row++;
                rowlen -= 2;
            } else {
                /* Non-Zero */
                int w;

                left = *row++;
                lid  = *row++;
                w = ((lid&1)-1) | 1;
                rowlen--;
                do {
                    right = *row++;
                    rid   = *row++;
                    rowlen--;
                    w += ((rid&1)-1) | 1;
                } while (w != 0);
            }

            if (right > left) {
                *rowout++ = left;
                *rowout++ = lid;
                *rowout++ = right;
                *rowout++ = rid;
            }
        }
        rowstart[-1] = (rowout-rowstart)>>1;
    }
    return 0;
}

/* Step 6: Fill the edgebuffer */
int
gx_fill_edgebuffer_tr(gx_device       * gs_restrict pdev,
                const gx_device_color * gs_restrict pdevc,
                      gx_edgebuffer   * gs_restrict edgebuffer,
                      int                        log_op)
{
    int i, j, code;
    int mfb = pdev->max_fill_band;

#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter) {
        dlprintf("Before filling\n");
        gx_edgebuffer_print_tr(edgebuffer);
    }
#endif

    for (i=0; i < edgebuffer->height; ) {
        int *row    = &edgebuffer->table[edgebuffer->index[i]];
        int  rowlen = *row++;
        int *row2;
        int *rowptr;
        int *row2ptr;
        int y_band_max;

        if (mfb) {
            y_band_max = (i & ~(mfb-1)) + mfb;
            if (y_band_max > edgebuffer->height)
                y_band_max = edgebuffer->height;
        } else {
            y_band_max = edgebuffer->height;
        }

        /* See how many scanlines match i */
        for (j = i+1; j < y_band_max; j++) {
            int row2len;

            row2    = &edgebuffer->table[edgebuffer->index[j]];
            row2len = *row2++;
            row2ptr = row2;
            rowptr  = row;

            if (rowlen != row2len)
                break;
            while (row2len > 0) {
                if ((rowptr[1]&~1) != (row2ptr[1]&~1))
                    goto rowdifferent;
                rowptr  += 2;
                row2ptr += 2;
                row2len--;
            }
        }
rowdifferent:{}

        /* So j is the first scanline that doesn't match i */

        if (j == i+1) {
            while (rowlen > 0) {
                int left, right;

                left  = row[0];
                right = row[2];
                row += 4;
                rowlen -= 2;

                left  = fixed2int(left + fixed_half);
                right = fixed2int(right + fixed_half);
                right -= left;
                if (right > 0) {
#ifdef DEBUG_OUTPUT_SC_AS_PS
                    dlprintf("0.001 setlinewidth 1 0 1 setrgbcolor %% purple %%PS\n");
                    coord("moveto", int2fixed(left), int2fixed(edgebuffer->base+i));
                    coord("lineto", int2fixed(left+right), int2fixed(edgebuffer->base+i));
                    coord("lineto", int2fixed(left+right), int2fixed(edgebuffer->base+i+1));
                    coord("lineto", int2fixed(left), int2fixed(edgebuffer->base+i+1));
                    dlprintf("closepath stroke %%PS\n");
#endif
                    if (log_op < 0)
                        code = dev_proc(pdev, fill_rectangle)(pdev, left, edgebuffer->base+i, right, 1, pdevc->colors.pure);
                    else
                        code = gx_fill_rectangle_device_rop(left, edgebuffer->base+i, right, 1, pdevc, pdev, (gs_logical_operation_t)log_op);
                    if (code < 0)
                        return code;
                }
            }
        } else {
            gs_fixed_edge le;
            gs_fixed_edge re;

#ifdef DEBUG_OUTPUT_SC_AS_PS
#ifdef DEBUG_OUTPUT_SC_AS_PS_TRAPS_AS_RECTS
            int k;
            for (k = i; k < j; k++)
            {
                int row2len;
                int left, right;
                row2    = &edgebuffer->table[edgebuffer->index[k]];
                row2len = *row2++;
                while (row2len > 0) {
                    left = row2[0];
                    right = row2[2];
                    row2 += 4;
                    row2len -= 2;

                    left  = fixed2int(left + fixed_half);
                    right = fixed2int(right + fixed_half);
                    right -= left;
                    if (right > 0) {
                        dlprintf("0.001 setlinewidth 1 0 0.5 setrgbcolor %%PS\n");
                        coord("moveto", int2fixed(left), int2fixed(edgebuffer->base+k));
                        coord("lineto", int2fixed(left+right), int2fixed(edgebuffer->base+k));
                        coord("lineto", int2fixed(left+right), int2fixed(edgebuffer->base+k+1));
                        coord("lineto", int2fixed(left), int2fixed(edgebuffer->base+k+1));
                        dlprintf("closepath stroke %%PS\n");
                    }
                }
            }
#endif
#endif

            le.start.y = re.start.y = int2fixed(edgebuffer->base+i) + fixed_half;
            le.end.y   = re.end.y   = int2fixed(edgebuffer->base+j) - (fixed_half-1);
            row2    = &edgebuffer->table[edgebuffer->index[j-1]+1];
            while (rowlen > 0) {
                le.start.x = row[0];
                re.start.x = row[2];
                le.end.x   = row2[0];
                re.end.x   = row2[2];
                row += 4;
                row2 += 4;
                rowlen -= 2;

                assert(re.start.x >= le.start.x);
                assert(re.end.x >= le.end.x);

#ifdef DEBUG_OUTPUT_SC_AS_PS
                dlprintf("0.001 setlinewidth 0 1 1 setrgbcolor %%cyan %%PS\n");
                coord("moveto", le.start.x, le.start.y);
                coord("lineto", le.end.x, le.end.y);
                coord("lineto", re.end.x, re.end.y);
                coord("lineto", re.start.x, re.start.y);
                dlprintf("closepath stroke %%PS\n");
#endif
                code = dev_proc(pdev, fill_trapezoid)(
                                pdev,
                                &le,
                                &re,
                                le.start.y,
                                le.end.y,
                                0, /* bool swap_axes */
                                pdevc, /*const gx_drawing_color *pdcolor */
                                log_op);
                if (code < 0)
                    return code;
            }
        }

        i = j;
    }
    return 0;
}

/* Any part of a pixel trapezoid routines */

static int edgecmp_tr(const void *a, const void *b)
{
    int left  = ((int*)a)[0];
    int right = ((int*)b)[0];
    if (left != right)
        return left - right;
    left = ((int*)a)[2] - ((int*)b)[2];
    if (left != 0)
        return left;
    left = ((int*)a)[1] - ((int*)b)[1];
    if (left != 0)
        return left;
    return ((int*)a)[3] - ((int*)b)[3];
}

#ifdef DEBUG_SCAN_CONVERTER
static void
gx_edgebuffer_print_filtered_tr_app(gx_edgebuffer * edgebuffer)
{
    int i;

    if (!debugging_scan_converter)
        return;

    dlprintf1("Edgebuffer %x\n", edgebuffer);
    dlprintf4("xmin=%x xmax=%x base=%x height=%x\n",
              edgebuffer->xmin, edgebuffer->xmax, edgebuffer->base, edgebuffer->height);
    for (i=0; i < edgebuffer->height; i++)
    {
        int  offset = edgebuffer->index[i];
        int *row    = &edgebuffer->table[offset];
        int count   = *row++;
        dlprintf3("%x @ %d: %d =", i, offset, count);
        while (count-- > 0) {
            int left  = *row++;
            int lid   = *row++;
            int right = *row++;
            int rid   = *row++;
            dlprintf4(" (%x:%d,%x:%d)", left, lid, right, rid);
        }
        dlprintf("\n");
    }
}

static void
gx_edgebuffer_print_tr_app(gx_edgebuffer * edgebuffer)
{
    int i;
    int borked = 0;

    if (!debugging_scan_converter)
        return;

    dlprintf1("Edgebuffer %x\n", edgebuffer);
    dlprintf4("xmin=%x xmax=%x base=%x height=%x\n",
              edgebuffer->xmin, edgebuffer->xmax, edgebuffer->base, edgebuffer->height);
    for (i=0; i < edgebuffer->height; i++)
    {
        int  offset = edgebuffer->index[i];
        int *row    = &edgebuffer->table[offset];
        int count   = *row++;
        int c       = count;
        int wind    = 0;
        dlprintf3("%x @ %d: %d =", i, offset, count);
        while (count-- > 0) {
            int left  = *row++;
            int lid   = *row++;
            int right = *row++;
            int rid   = *row++;
            int ww    = lid & 1;
            int w     = -ww | 1;
            lid >>= 1;
            wind += w;
            dlprintf5(" (%x:%d,%x:%d)%c", left, lid, right, rid, ww ? 'v' : '^');
        }
        if (wind != 0 || c & 1) {
            dlprintf(" <- BROKEN");
            borked = 1;
        }
        dlprintf("\n");
    }
    if (borked) {
        borked = borked; /* Breakpoint here */
    }
}
#endif

typedef struct
{
    fixed  left;
    int    lid;
    fixed  right;
    int    rid;
    fixed  y;
    signed char  d; /* 0 up (or horiz), 1 down, -1 uninited */
    unsigned char first;
    unsigned char saved;
    fixed  save_left;
    int    save_lid;
    fixed  save_right;
    int    save_rid;
    int    save_iy;
    int    save_d;

    int    scanlines;
    int   *table;
    int   *index;
    int    base;
} cursor_tr;

static inline void
cursor_output_tr(cursor_tr * gs_restrict cr, int iy)
{
    int *row;
    int count;

    if (iy >= 0 && iy < cr->scanlines) {
        if (cr->first) {
            /* Save it for later in case we join up */
            cr->save_left  = cr->left;
            cr->save_lid   = cr->lid;
            cr->save_right = cr->right;
            cr->save_rid   = cr->rid;
            cr->save_iy    = iy;
            cr->save_d     = cr->d;
            cr->saved      = 1;
        } else if (cr->d != DIRN_UNSET) {
            /* Enter it into the table */
            row = &cr->table[cr->index[iy]];
            *row = count = (*row)+1; /* Increment the count */
            row[4 * count - 3] = cr->left;
            row[4 * count - 2] = cr->d | (cr->lid<<1);
            row[4 * count - 1] = cr->right;
            row[4 * count    ] = cr->rid;
        } else {
            assert(cr->left == max_fixed && cr->right == min_fixed);
        }
    }
    cr->first = 0;
}

static inline void
cursor_output_inrange_tr(cursor_tr * gs_restrict cr, int iy)
{
    int *row;
    int count;

    assert(iy >= 0 && iy < cr->scanlines);
    if (cr->first) {
        /* Save it for later in case we join up */
        cr->save_left  = cr->left;
        cr->save_lid   = cr->lid;
        cr->save_right = cr->right;
        cr->save_rid   = cr->rid;
        cr->save_iy    = iy;
        cr->save_d     = cr->d;
        cr->saved      = 1;
    } else {
        /* Enter it into the table */
        assert(cr->d != DIRN_UNSET);

        row = &cr->table[cr->index[iy]];
        *row = count = (*row)+1; /* Increment the count */
        row[4 * count - 3] = cr->left;
        row[4 * count - 2] = cr->d | (cr->lid<<1);
        row[4 * count - 1] = cr->right;
        row[4 * count    ] = cr->rid;
    }
    cr->first = 0;
}

/* Step the cursor in y, allowing for maybe crossing a scanline */
static inline void
cursor_step_tr(cursor_tr * gs_restrict cr, fixed dy, fixed x, int id, int skip)
{
    int new_iy;
    int iy = fixed2int(cr->y) - cr->base;

    cr->y += dy;
    new_iy = fixed2int(cr->y) - cr->base;
    if (new_iy != iy) {
        if (!skip)
            cursor_output_tr(cr, iy);
        cr->left = x;
        cr->lid = id;
        cr->right = x;
        cr->rid = id;
    } else {
        if (x < cr->left) {
            cr->left = x;
            cr->lid = id;
        }
        if (x > cr->right) {
            cr->right = x;
            cr->rid = id;
        }
    }
}

/* Step the cursor in y, never by enough to cross a scanline. */
static inline void
cursor_never_step_vertical_tr(cursor_tr * gs_restrict cr, fixed dy, fixed x, int id)
{
    assert(fixed2int(cr->y+dy) == fixed2int(cr->y));

    cr->y += dy;
}

/* Step the cursor in y, never by enough to cross a scanline,
 * knowing that we are moving left, and that the right edge
 * has already been accounted for. */
static inline void
cursor_never_step_left_tr(cursor_tr * gs_restrict cr, fixed dy, fixed x, int id)
{
    assert(fixed2int(cr->y+dy) == fixed2int(cr->y));

    if (x < cr->left)
    {
        cr->left = x;
        cr->lid = id;
    }
    cr->y += dy;
}

/* Step the cursor in y, never by enough to cross a scanline,
 * knowing that we are moving right, and that the left edge
 * has already been accounted for. */
static inline void
cursor_never_step_right_tr(cursor_tr * gs_restrict cr, fixed dy, fixed x, int id)
{
    assert(fixed2int(cr->y+dy) == fixed2int(cr->y));

    if (x > cr->right)
    {
        cr->right = x;
        cr->rid = id;
    }
    cr->y += dy;
}

/* Step the cursor in y, always by enough to cross a scanline. */
static inline void
cursor_always_step_tr(cursor_tr * gs_restrict cr, fixed dy, fixed x, int id, int skip)
{
    int iy = fixed2int(cr->y) - cr->base;

    if (!skip)
        cursor_output_tr(cr, iy);
    cr->y += dy;
    cr->left = x;
    cr->lid = id;
    cr->right = x;
    cr->rid = id;
}

/* Step the cursor in y, always by enough to cross a scanline, as
 * part of a vertical line, knowing that we are moving from a
 * position guaranteed to be in the valid y range. */
static inline void
cursor_always_step_inrange_vertical_tr(cursor_tr * gs_restrict cr, fixed dy, fixed x, int id)
{
    int iy = fixed2int(cr->y) - cr->base;

    cursor_output_tr(cr, iy);
    cr->y += dy;
}

/* Step the cursor in y, always by enough to cross a scanline, as
 * part of a left moving line, knowing that we are moving from a
 * position guaranteed to be in the valid y range. */
static inline void
cursor_always_inrange_step_left_tr(cursor_tr * gs_restrict cr, fixed dy, fixed x, int id)
{
    int iy = fixed2int(cr->y) - cr->base;

    cr->y += dy;
    cursor_output_inrange_tr(cr, iy);
    cr->right = x;
    cr->rid = id;
}

/* Step the cursor in y, always by enough to cross a scanline, as
 * part of a right moving line, knowing that we are moving from a
 * position guaranteed to be in the valid y range. */
static inline void
cursor_always_inrange_step_right_tr(cursor_tr * gs_restrict cr, fixed dy, fixed x, int id)
{
    int iy = fixed2int(cr->y) - cr->base;

    cr->y += dy;
    cursor_output_inrange_tr(cr, iy);
    cr->left = x;
    cr->lid = id;
}

static inline void cursor_init_tr(cursor_tr * gs_restrict cr, fixed y, fixed x, int id)
{
    assert(y >= int2fixed(cr->base) && y <= int2fixed(cr->base + cr->scanlines));

    cr->y = y;
    cr->left = x;
    cr->lid = id;
    cr->right = x;
    cr->rid = id;
    cr->d = DIRN_UNSET;
}

static inline void cursor_left_merge_tr(cursor_tr * gs_restrict cr, fixed x, int id)
{
    if (x < cr->left) {
        cr->left = x;
        cr->lid  = id;
    }
}

static inline void cursor_left_tr(cursor_tr * gs_restrict cr, fixed x, int id)
{
    cr->left = x;
    cr->lid  = id;
}

static inline void cursor_right_merge_tr(cursor_tr * gs_restrict cr, fixed x, int id)
{
    if (x > cr->right) {
        cr->right = x;
        cr->rid   = id;
    }
}

static inline void cursor_right_tr(cursor_tr * gs_restrict cr, fixed x, int id)
{
    cr->right = x;
    cr->rid   = id;
}

static inline int cursor_down_tr(cursor_tr * gs_restrict cr, fixed x, int id)
{
    int skip = 0;
    if ((cr->y & 0xff) == 0)
        skip = 1;
    if (cr->d == DIRN_UP)
    {
        if (!skip)
            cursor_output_tr(cr, fixed2int(cr->y) - cr->base);
        cr->left = x;
        cr->lid = id;
        cr->right = x;
        cr->rid = id;
    }
    cr->d = DIRN_DOWN;
    return skip;
}

static inline void cursor_up_tr(cursor_tr * gs_restrict cr, fixed x, int id)
{
    if (cr->d == DIRN_DOWN)
    {
        cursor_output_tr(cr, fixed2int(cr->y) - cr->base);
        cr->left = x;
        cr->lid = id;
        cr->right = x;
        cr->rid = id;
    }
    cr->d = DIRN_UP;
}

static inline void
cursor_flush_tr(cursor_tr * gs_restrict cr, fixed x, int id)
{
    int iy;

    /* This should only happen if we were entirely out of bounds,
     * or if everything was within a zero height horizontal
     * rectangle from the start point. */
    if (cr->first) {
        int iy = fixed2int(cr->y) - cr->base;
        /* Any zero height rectangle counts as filled, except
         * those on the baseline of a pixel. */
        if (cr->d == DIRN_UNSET && (cr->y & 0xff) == 0)
            return;
        assert(cr->left != max_fixed && cr->right != min_fixed);
        if (iy >= 0 && iy < cr->scanlines) {
            int *row = &cr->table[cr->index[iy]];
            int count = *row = (*row)+2; /* Increment the count */
            row[4 * count - 7] = cr->left;
            row[4 * count - 6] = DIRN_UP | (cr->lid<<1);
            row[4 * count - 5] = cr->right;
            row[4 * count - 4] = cr->rid;
            row[4 * count - 3] = cr->right;
            row[4 * count - 2] = DIRN_DOWN | (cr->rid<<1);
            row[4 * count - 1] = cr->right;
            row[4 * count    ] = cr->rid;
        }
        return;
    }

    /* Merge save into current if we can */
    iy = fixed2int(cr->y) - cr->base;
    if (cr->saved && iy == cr->save_iy &&
        (cr->d == cr->save_d || cr->save_d == DIRN_UNSET)) {
        if (cr->left > cr->save_left) {
            cr->left = cr->save_left;
            cr->lid  = cr->save_lid;
        }
        if (cr->right < cr->save_right) {
            cr->right = cr->save_right;
            cr->rid = cr->save_rid;
        }
        cursor_output_tr(cr, iy);
        return;
    }

    /* Merge not possible */
    cursor_output_tr(cr, iy);
    if (cr->saved) {
        cr->left  = cr->save_left;
        cr->lid   = cr->save_lid;
        cr->right = cr->save_right;
        cr->rid   = cr->save_rid;
        assert(cr->save_d != DIRN_UNSET);
        if (cr->save_d != DIRN_UNSET)
            cr->d = cr->save_d;
        cursor_output_tr(cr, cr->save_iy);
    }
}

static inline void
cursor_null_tr(cursor_tr *cr)
{
    cr->right = min_fixed;
    cr->left  = max_fixed;
    cr->d     = DIRN_UNSET;
}

static void mark_line_tr_app(cursor_tr * gs_restrict cr, fixed sx, fixed sy, fixed ex, fixed ey, int id)
{
    int isy, iey;
    fixed saved_sy = sy;
    fixed saved_ex = ex;
    fixed saved_ey = ey;
    int truncated;

    if (sy == ey && sx == ex)
        return;

    isy = fixed2int(sy) - cr->base;
    iey = fixed2int(ey) - cr->base;

#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter)
        dlprintf6("Marking line (tr_app) from %x,%x to %x,%x (%x,%x)\n", sx, sy, ex, ey, isy, iey);
#endif
#ifdef DEBUG_OUTPUT_SC_AS_PS
    dlprintf("0.001 setlinewidth 0 0 0 setrgbcolor %%PS\n");
    coord("moveto", sx, sy);
    coord("lineto", ex, ey);
    dlprintf("stroke %%PS\n");
#endif

    /* Horizontal motion at the bottom of a pixel is ignored */
    if (sy == ey && (sy & 0xff) == 0)
        return;

    assert(cr->y == sy &&
           ((cr->left <= sx && cr->right >= sx) || ((sy & 0xff) == 0)) &&
           cr->d >= DIRN_UNSET && cr->d <= DIRN_DOWN);

    if (isy < iey) {
        /* Rising line */
        if (iey < 0 || isy >= cr->scanlines) {
            /* All line is outside. */
            if ((ey & 0xff) == 0) {
                cursor_null_tr(cr);
            } else {
                cr->left = ex;
                cr->lid = id;
                cr->right = ex;
                cr->rid = id;
            }
            cr->y = ey;
            cr->first = 0;
            return;
        }
        if (isy < 0) {
            /* Move sy up */
            int64_t y = (int64_t)ey - (int64_t)sy;
            fixed new_sy = int2fixed(cr->base);
            int64_t dy = (int64_t)new_sy - (int64_t)sy;
            sx += (int)(((((int64_t)ex-sx))*dy + y/2)/y);
            sy = new_sy;
            cursor_null_tr(cr);
            cr->y = sy;
            isy = 0;
        }
        truncated = iey > cr->scanlines;
        if (truncated) {
            /* Move ey down */
            int64_t y = (int64_t)ey - (int64_t)sy;
            fixed new_ey = int2fixed(cr->base + cr->scanlines);
            int64_t dy = (int64_t)ey - (int64_t)new_ey;
            saved_ex = ex;
            saved_ey = ey;
            ex -= (int)(((((int64_t)ex-sx))*dy + y/2)/y);
            ey = new_ey;
            iey = cr->scanlines;
        }
    } else {
        /* Falling line */
        if (isy < 0 || iey >= cr->scanlines) {
            /* All line is outside. */
            if ((ey & 0xff) == 0) {
                cursor_null_tr(cr);
            } else {
                cr->left = ex;
                cr->lid = id;
                cr->right = ex;
                cr->rid = id;
            }
            cr->y = ey;
            cr->first = 0;
            return;
        }
        truncated = iey < 0;
        if (truncated) {
            /* Move ey up */
            int64_t y = (int64_t)ey - (int64_t)sy;
            fixed new_ey = int2fixed(cr->base);
            int64_t dy = (int64_t)ey - (int64_t)new_ey;
            ex -= (int)(((((int64_t)ex-sx))*dy + y/2)/y);
            ey = new_ey;
            iey = 0;
        }
        if (isy >= cr->scanlines) {
            /* Move sy down */
            int64_t y = (int64_t)ey - (int64_t)sy;
            fixed new_sy = int2fixed(cr->base + cr->scanlines);
            int64_t dy = (int64_t)new_sy - (int64_t)sy;
            sx += (int)(((((int64_t)ex-sx))*dy + y/2)/y);
            sy = new_sy;
            cursor_null_tr(cr);
            cr->y = sy;
            isy = cr->scanlines;
        }
    }

    cursor_left_merge_tr(cr, sx, id);
    cursor_right_merge_tr(cr, sx, id);

    assert(cr->left <= sx);
    assert(cr->right >= sx);
    assert(cr->y == sy);

    /* A note: The code below used to be of the form:
     *   if (isy == iey)   ... deal with horizontal lines
     *   else if (ey > sy) {
     *     fixed y_steps = ey - sy;
     *      ... deal with rising lines ...
     *   } else {
     *     fixed y_steps = ey - sy;
     *     ... deal with falling lines
     *   }
     * but that lead to problems, for instance, an example seen
     * has sx=2aa8e, sy=8aee7, ex=7ffc1686, ey=8003e97a.
     * Thus isy=84f, iey=ff80038a. We can see that ey < sy, but
     * sy - ey < 0!
     * We therefore rejig our code so that the choice between
     * cases is done based on the sign of y_steps rather than
     * the relative size of ey and sy.
     */

    /* First, deal with lines that don't change scanline.
     * This accommodates horizontal lines. */
    if (isy == iey) {
        if (saved_sy == saved_ey) {
            /* Horizontal line. Don't change cr->d, don't flush. */
            if ((ey & 0xff) == 0) {
                cursor_null_tr(cr);
                goto no_merge;
            }
        } else if (saved_sy > saved_ey) {
            /* Falling line, flush if previous was rising */
            int skip = cursor_down_tr(cr, sx, id);
            if ((ey & 0xff) == 0) {
                /* We are falling to the baseline of a subpixel, so output
                 * for the current pixel, and leave the cursor nulled. */
                if (sx <= ex) {
                    cursor_right_merge_tr(cr, ex, id);
                } else {
                    cursor_left_merge_tr(cr, ex, id);
                }
                if (!skip)
                    cursor_output_tr(cr, fixed2int(cr->y) - cr->base);
                cursor_null_tr(cr);
                goto no_merge;
            }
        } else {
            /* Rising line, flush if previous was falling */
            cursor_up_tr(cr, sx, id);
            if ((ey & 0xff) == 0) {
                cursor_null_tr(cr);
                goto no_merge;
            }
        }
        if (sx <= ex) {
            cursor_right_merge_tr(cr, ex, id);
        } else {
            cursor_left_merge_tr(cr, ex, id);
        }
no_merge:
        cr->y = ey;
        if (sy > saved_ey)
            goto endFalling;
    } else if (iey > isy) {
        /* So lines increasing in y. */
        /* We want to change from sy to ey, which are guaranteed to be on
         * different scanlines. We do this in 3 phases.
         * Phase 1 gets us from sy to the next scanline boundary. (We may exit after phase 1).
         * Phase 2 gets us all the way to the last scanline boundary. (This may be a null operation)
         * Phase 3 gets us from the last scanline boundary to ey. (We are guaranteed to have output the cursor at least once before phase 3).
         */
        int phase1_y_steps = (-sy) & (fixed_1 - 1);
        int phase3_y_steps = ey & (fixed_1 - 1);
        ufixed y_steps = (ufixed)ey - (ufixed)sy;

        cursor_up_tr(cr, sx, id);

        if (sx == ex) {
            /* Vertical line. (Rising) */

            /* Phase 1: */
            if (phase1_y_steps) {
                /* If phase 1 will move us into a new scanline, then we must
                 * flush it before we move. */
                cursor_step_tr(cr, phase1_y_steps, sx, id, 0);
                sy += phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0) {
                    cursor_null_tr(cr);
                    goto end;
                }
            }

            /* Phase 3: precalculation */
            y_steps -= phase3_y_steps;

            /* Phase 2: */
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps > 0) {
                cursor_always_step_tr(cr, fixed_1, sx, id, 0);
                y_steps--;
                while (y_steps) {
                    cursor_always_step_inrange_vertical_tr(cr, fixed_1, sx, id);
                    y_steps--;
                }
            }

            /* Phase 3 */
            assert(cr->left == sx && cr->right == sx && cr->lid == id && cr->rid == id);
            if (phase3_y_steps == 0)
                cursor_null_tr(cr);
            else
                cr->y += phase3_y_steps;
        } else if (sx < ex) {
            /* Lines increasing in x. (Rightwards, rising) */
            int phase1_x_steps, phase3_x_steps;
            /* Use unsigned int here, to allow for extreme cases like
             * ex = 0x7fffffff, sx = 0x80000000 */
            unsigned int x_steps = ex - sx;

            /* Phase 1: */
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                sx += phase1_x_steps;
                cursor_right_merge_tr(cr, sx, id);
                x_steps -= phase1_x_steps;
                cursor_step_tr(cr, phase1_y_steps, sx, id, 0);
                sy += phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0) {
                    cursor_null_tr(cr);
                    goto end;
                }
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;
            assert((y_steps & (fixed_1 - 1)) == 0);

            /* Phase 2: */
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps) {
                /* We want to change sx by x_steps in y_steps steps.
                 * So each step, we add x_steps/y_steps to sx. That's x_inc + n_inc/y_steps. */
                int x_inc = x_steps/y_steps;
                int n_inc = x_steps - (x_inc * y_steps);
                int f = y_steps/2;
                int d = y_steps;

                /* Special casing the first iteration, allows us to simplify
                 * the following loop. */
                sx += x_inc;
                f -= n_inc;
                if (f < 0)
                    f += d, sx++;
                cursor_right_merge_tr(cr, sx, id);
                cursor_always_step_tr(cr, fixed_1, sx, id, 0);
                y_steps--;

                while (y_steps) {
                    sx += x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx++;
                    cursor_right_tr(cr, sx, id);
                    cursor_always_inrange_step_right_tr(cr, fixed_1, sx, id);
                    y_steps--;
                };
            }

            /* Phase 3 */
            assert(cr->left <= ex && cr->lid == id && cr->right >= sx);
            if (phase3_y_steps == 0)
                cursor_null_tr(cr);
            else {
                cursor_right_tr(cr, ex, id);
                cr->y += phase3_y_steps;
            }
        } else {
            /* Lines decreasing in x. (Leftwards, rising) */
            int phase1_x_steps, phase3_x_steps;
            /* Use unsigned int here, to allow for extreme cases like
             * sx = 0x7fffffff, ex = 0x80000000 */
            unsigned int x_steps = sx - ex;

            /* Phase 1: */
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                x_steps -= phase1_x_steps;
                sx -= phase1_x_steps;
                cursor_left_merge_tr(cr, sx, id);
                cursor_step_tr(cr, phase1_y_steps, sx, id, 0);
                sy += phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0) {
                    cursor_null_tr(cr);
                    goto end;
                }
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;
            assert((y_steps & (fixed_1 - 1)) == 0);

            /* Phase 2: */
            y_steps = fixed2int((unsigned int)y_steps);
            assert(y_steps >= 0);
            if (y_steps) {
                /* We want to change sx by x_steps in y_steps steps.
                 * So each step, we sub x_steps/y_steps from sx. That's x_inc + n_inc/ey. */
                int x_inc = x_steps/y_steps;
                int n_inc = x_steps - (x_inc * y_steps);
                int f = y_steps/2;
                int d = y_steps;

                /* Special casing the first iteration, allows us to simplify
                 * the following loop. */
                sx -= x_inc;
                f -= n_inc;
                if (f < 0)
                    f += d, sx--;
                cursor_left_merge_tr(cr, sx, id);
                cursor_always_step_tr(cr, fixed_1, sx, id, 0);
                y_steps--;

                while (y_steps) {
                    sx -= x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx--;
                    cursor_left_tr(cr, sx, id);
                    cursor_always_inrange_step_left_tr(cr, fixed_1, sx, id);
                    y_steps--;
                }
            }

            /* Phase 3 */
            assert(cr->right >= ex && cr->rid == id && cr->left <= sx);
            if (phase3_y_steps == 0)
                cursor_null_tr(cr);
            else {
                cursor_left_tr(cr, ex, id);
                cr->y += phase3_y_steps;
            }
        }
    } else {
        /* So lines decreasing in y. */
        /* We want to change from sy to ey, which are guaranteed to be on
         * different scanlines. We do this in 3 phases.
         * Phase 1 gets us from sy to the next scanline boundary. This never causes an output.
         * Phase 2 gets us all the way to the last scanline boundary. This is guaranteed to cause an output.
         * Phase 3 gets us from the last scanline boundary to ey. We are guaranteed to have outputted by now.
         */
        int phase1_y_steps = sy & (fixed_1 - 1);
        int phase3_y_steps = (-ey) & (fixed_1 - 1);
        ufixed y_steps = (ufixed)sy - (ufixed)ey;

        int skip = cursor_down_tr(cr, sx, id);

        if (sx == ex) {
            /* Vertical line. (Falling) */

            /* Phase 1: */
            if (phase1_y_steps) {
                /* Phase 1 in a falling line never moves us into a new scanline. */
                cursor_never_step_vertical_tr(cr, -phase1_y_steps, sx, id);
                sy -= phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0)
                    goto endFallingLeftOnEdgeOfPixel;
            }

            /* Phase 3: precalculation */
            y_steps -= phase3_y_steps;
            assert((y_steps & (fixed_1 - 1)) == 0);

            /* Phase 2: */
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps) {
                cursor_always_step_tr(cr, -fixed_1, sx, id, skip);
                skip = 0;
                y_steps--;
                while (y_steps) {
                    cursor_always_step_inrange_vertical_tr(cr, -fixed_1, sx, id);
                    y_steps--;
                }
            }

            /* Phase 3 */
            if (phase3_y_steps == 0) {
endFallingLeftOnEdgeOfPixel:
                cursor_always_step_inrange_vertical_tr(cr, 0, sx, id);
                cursor_null_tr(cr);
            } else {
                cursor_step_tr(cr, -phase3_y_steps, sx, id, skip);
                assert(cr->left == sx && cr->lid == id && cr->right == sx && cr->rid == id);
            }
        } else if (sx < ex) {
            /* Lines increasing in x. (Rightwards, falling) */
            int phase1_x_steps, phase3_x_steps;
            /* Use unsigned int here, to allow for extreme cases like
             * ex = 0x7fffffff, sx = 0x80000000 */
            unsigned int x_steps = ex - sx;

            /* Phase 1: */
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                x_steps -= phase1_x_steps;
                sx += phase1_x_steps;
                /* Phase 1 in a falling line never moves us into a new scanline. */
                cursor_never_step_right_tr(cr, -phase1_y_steps, sx, id);
                sy -= phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0)
                    goto endFallingRightOnEdgeOfPixel;
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;
            assert((y_steps & (fixed_1 - 1)) == 0);

            /* Phase 2: */
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps) {
                /* We want to change sx by x_steps in y_steps steps.
                 * So each step, we add x_steps/y_steps to sx. That's x_inc + n_inc/ey. */
                int x_inc = x_steps/y_steps;
                int n_inc = x_steps - (x_inc * y_steps);
                int f = y_steps/2;
                int d = y_steps;

                cursor_always_step_tr(cr, -fixed_1, sx, id, skip);
                skip = 0;
                sx += x_inc;
                f -= n_inc;
                if (f < 0)
                    f += d, sx++;
                cursor_right_tr(cr, sx, id);
                y_steps--;

                while (y_steps) {
                    cursor_always_inrange_step_right_tr(cr, -fixed_1, sx, id);
                    sx += x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx++;
                    cursor_right_tr(cr, sx, id);
                    y_steps--;
                }
            }

            /* Phase 3 */
            if (phase3_y_steps == 0) {
endFallingRightOnEdgeOfPixel:
                cursor_always_step_inrange_vertical_tr(cr, 0, sx, id);
                cursor_null_tr(cr);
            } else {
                cursor_step_tr(cr, -phase3_y_steps, sx, id, skip);
                cursor_right_tr(cr, ex, id);
                assert(cr->left == sx && cr->lid == id && cr->right == ex && cr->rid == id);
            }
        } else {
            /* Lines decreasing in x. (Falling) */
            int phase1_x_steps, phase3_x_steps;
            /* Use unsigned int here, to allow for extreme cases like
             * sx = 0x7fffffff, ex = 0x80000000 */
            unsigned int x_steps = sx - ex;

            /* Phase 1: */
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                x_steps -= phase1_x_steps;
                sx -= phase1_x_steps;
                /* Phase 1 in a falling line never moves us into a new scanline. */
                cursor_never_step_left_tr(cr, -phase1_y_steps, sx, id);
                sy -= phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0)
                    goto endFallingVerticalOnEdgeOfPixel;
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;
            assert((y_steps & (fixed_1 - 1)) == 0);

            /* Phase 2: */
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps) {
                /* We want to change sx by x_steps in y_steps steps.
                 * So each step, we sub x_steps/y_steps from sx. That's x_inc + n_inc/ey. */
                int x_inc = x_steps/y_steps;
                int n_inc = x_steps - (x_inc * y_steps);
                int f = y_steps/2;
                int d = y_steps;

                cursor_always_step_tr(cr, -fixed_1, sx, id, skip);
                skip = 0;
                sx -= x_inc;
                f -= n_inc;
                if (f < 0)
                    f += d, sx--;
                cursor_left_tr(cr, sx, id);
                y_steps--;

                while (y_steps) {
                    cursor_always_inrange_step_left_tr(cr, -fixed_1, sx, id);
                    sx -= x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx--;
                    cursor_left_tr(cr, sx, id);
                    y_steps--;
                }
            }

            /* Phase 3 */
            if (phase3_y_steps == 0) {
endFallingVerticalOnEdgeOfPixel:
                cursor_always_step_inrange_vertical_tr(cr, 0, sx, id);
                cursor_null_tr(cr);
            } else {
                cursor_step_tr(cr, -phase3_y_steps, sx, id, skip);
                cursor_left_tr(cr, ex, id);
                assert(cr->left == ex && cr->lid == id && cr->right == sx && cr->rid == id);
            }
        }
endFalling: {}
    }

end:
    if (truncated) {
        cr->left = saved_ex;
        cr->lid = id;
        cr->right = saved_ex;
        cr->rid = id;
        cr->y = saved_ey;
    }
}

static void mark_curve_tr_app(cursor_tr * gs_restrict cr, fixed sx, fixed sy, fixed c1x, fixed c1y, fixed c2x, fixed c2y, fixed ex, fixed ey, int depth, int * gs_restrict id)
{
        int ax = (sx + c1x)>>1;
        int ay = (sy + c1y)>>1;
        int bx = (c1x + c2x)>>1;
        int by = (c1y + c2y)>>1;
        int cx = (c2x + ex)>>1;
        int cy = (c2y + ey)>>1;
        int dx = (ax + bx)>>1;
        int dy = (ay + by)>>1;
        int fx = (bx + cx)>>1;
        int fy = (by + cy)>>1;
        int gx = (dx + fx)>>1;
        int gy = (dy + fy)>>1;

        assert(depth >= 0);
        if (depth == 0) {
            *id += 1;
            mark_line_tr_app(cr, sx, sy, ex, ey, *id);
        } else {
            depth--;
            mark_curve_tr_app(cr, sx, sy, ax, ay, dx, dy, gx, gy, depth, id);
            mark_curve_tr_app(cr, gx, gy, fx, fy, cx, cy, ex, ey, depth, id);
        }
}

static void mark_curve_big_tr_app(cursor_tr * gs_restrict cr, fixed64 sx, fixed64 sy, fixed64 c1x, fixed64 c1y, fixed64 c2x, fixed64 c2y, fixed64 ex, fixed64 ey, int depth, int * gs_restrict id)
{
    fixed64 ax = (sx + c1x)>>1;
    fixed64 ay = (sy + c1y)>>1;
    fixed64 bx = (c1x + c2x)>>1;
    fixed64 by = (c1y + c2y)>>1;
    fixed64 cx = (c2x + ex)>>1;
    fixed64 cy = (c2y + ey)>>1;
    fixed64 dx = (ax + bx)>>1;
    fixed64 dy = (ay + by)>>1;
    fixed64 fx = (bx + cx)>>1;
    fixed64 fy = (by + cy)>>1;
    fixed64 gx = (dx + fx)>>1;
    fixed64 gy = (dy + fy)>>1;

    assert(depth >= 0);
    if (depth == 0) {
        *id += 1;
        mark_line_tr_app(cr, (fixed)sx, (fixed)sy, (fixed)ex, (fixed)ey, *id);
    } else {
        depth--;
        mark_curve_big_tr_app(cr, sx, sy, ax, ay, dx, dy, gx, gy, depth, id);
        mark_curve_big_tr_app(cr, gx, gy, fx, fy, cx, cy, ex, ey, depth, id);
    }
}

static void mark_curve_top_tr_app(cursor_tr * gs_restrict cr, fixed sx, fixed sy, fixed c1x, fixed c1y, fixed c2x, fixed c2y, fixed ex, fixed ey, int depth, int * gs_restrict id)
{
    fixed test = (sx^(sx<<1))|(sy^(sy<<1))|(c1x^(c1x<<1))|(c1y^(c1y<<1))|(c2x^(c2x<<1))|(c2y^(c2y<<1))|(ex^(ex<<1))|(ey^(ey<<1));

    if (test < 0)
        mark_curve_big_tr_app(cr, sx, sy, c1x, c1y, c2x, c2y, ex, ey, depth, id);
    else
        mark_curve_tr_app(cr, sx, sy, c1x, c1y, c2x, c2y, ex, ey, depth, id);
}

static int make_table_tr_app(gx_device     * pdev,
                             gx_path       * path,
                             gs_fixed_rect * ibox,
                             int           * scanlines,
                             int          ** index,
                             int          ** table)
{
    return make_table_template(pdev, path, ibox, 4, 0, scanlines, index, table);
}

static void
fill_zero_app_tr(int *row, const fixed *x)
{
    int n = *row = (*row)+2; /* Increment the count */
    row[4*n-7] = x[0];
    row[4*n-6] = 0;
    row[4*n-5] = x[1];
    row[4*n-4] = 0;
    row[4*n-3] = x[1];
    row[4*n-2] = (1<<1)|1;
    row[4*n-1] = x[1];
    row[4*n  ] = 1;
}

int gx_scan_convert_tr_app(gx_device     * gs_restrict pdev,
                           gx_path       * gs_restrict path,
                     const gs_fixed_rect * gs_restrict clip,
                           gx_edgebuffer * gs_restrict edgebuffer,
                           fixed                    fixed_flat)
{
    gs_fixed_rect  ibox;
    gs_fixed_rect  bbox;
    int            scanlines;
    const subpath *psub;
    int           *index;
    int           *table;
    int            i;
    cursor_tr      cr;
    int            code;
    int            id = 0;
    int            zero;

    edgebuffer->index = NULL;
    edgebuffer->table = NULL;

    /* Bale out if no actual path. We see this with the clist */
    if (path->first_subpath == NULL)
        return 0;

    zero = make_bbox(path, clip, &bbox, &ibox, 0);
    if (zero < 0)
        return zero;

    if (ibox.q.y <= ibox.p.y)
        return 0;

    code = make_table_tr_app(pdev, path, &ibox, &scanlines, &index, &table);
    if (code != 0) /* > 0 means "retry with smaller height" */
        return code;

    if (scanlines == 0)
        return 0;

    if (zero) {
        code = zero_case(pdev, path, &ibox, index, table, fixed_flat, fill_zero_app_tr);
    } else {

    /* Step 2 continued: Now we run through the path, filling in the real
     * values. */
    cr.scanlines = scanlines;
    cr.index     = index;
    cr.table     = table;
    cr.base      = ibox.p.y;
    for (psub = path->first_subpath; psub != 0;) {
        const segment *pseg = (const segment *)psub;
        fixed ex = pseg->pt.x;
        fixed ey = pseg->pt.y;
        fixed ix = ex;
        fixed iy = ey;
        fixed sx, sy;

        if ((ey & 0xff) == 0) {
            cr.left  = max_fixed;
            cr.right = min_fixed;
        } else {
            cr.left = cr.right = ex;
        }
        cr.lid = cr.rid = id+1;
        cr.y = ey;
        cr.d = DIRN_UNSET;
        cr.first = 1;
        cr.saved = 0;

        while ((pseg = pseg->next) != 0 &&
               pseg->type != s_start
            ) {
            sx = ex;
            sy = ey;
            ex = pseg->pt.x;
            ey = pseg->pt.y;

            switch (pseg->type) {
                default:
                case s_start: /* Should never happen */
                case s_dash:  /* We should never be seeing a dash here */
                    assert("This should never happen" == NULL);
                    break;
                case s_curve: {
                    const curve_segment *const pcur = (const curve_segment *)pseg;
                    int k = gx_curve_log2_samples(sx, sy, pcur, fixed_flat);

                    mark_curve_top_tr_app(&cr, sx, sy, pcur->p1.x, pcur->p1.y, pcur->p2.x, pcur->p2.y, ex, ey, k, &id);
                    break;
                }
                case s_gap:
                case s_line:
                case s_line_close:
                    mark_line_tr_app(&cr, sx, sy, ex, ey, ++id);
                    break;
            }
        }
        /* And close any open segments */
        mark_line_tr_app(&cr, ex, ey, ix, iy, ++id);
        cursor_flush_tr(&cr, ex, id);
        psub = (const subpath *)pseg;
    }
    }

    /* Step 2 complete: We now have a complete list of intersection data in
     * table, indexed by index. */

    edgebuffer->base   = ibox.p.y;
    edgebuffer->height = scanlines;
    edgebuffer->xmin   = ibox.p.x;
    edgebuffer->xmax   = ibox.q.x;
    edgebuffer->index  = index;
    edgebuffer->table  = table;

#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter) {
        dlprintf("Before sorting\n");
        gx_edgebuffer_print_tr_app(edgebuffer);
    }
#endif

    /* Step 3: Sort the intersects on x */
    for (i=0; i < scanlines; i++) {
        int *row = &table[index[i]];
        int  rowlen = *row++;

        /* Bubblesort short runs, qsort longer ones. */
        /* Figure of '6' comes from testing */
        if (rowlen <= 6) {
            int j, k;
            for (j = 0; j < rowlen-1; j++) {
                int * gs_restrict t = &row[j<<2];
                for (k = j+1; k < rowlen; k++) {
                    int * gs_restrict s = &row[k<<2];
                    int tmp;
                    if (t[0] < s[0])
                        continue;
                    if (t[0] > s[0])
                        goto swap0213;
                    if (t[2] < s[2])
                        continue;
                    if (t[2] > s[2])
                        goto swap213;
                    if (t[1] < s[1])
                        continue;
                    if (t[1] > s[1])
                        goto swap13;
                    if (t[3] <= s[3])
                        continue;
                    if (0) {
swap0213:
                        tmp = t[0], t[0] = s[0], s[0] = tmp;
swap213:
                        tmp = t[2], t[2] = s[2], s[2] = tmp;
swap13:
                        tmp = t[1], t[1] = s[1], s[1] = tmp;
                    }
                    tmp = t[3], t[3] = s[3], s[3] = tmp;
                }
            }
        } else
            qsort(row, rowlen, 4*sizeof(int), edgecmp_tr);
    }

    return 0;
}

/* Step 5: Filter the intersections according to the rules */
int
gx_filter_edgebuffer_tr_app(gx_device       * gs_restrict pdev,
                            gx_edgebuffer   * gs_restrict edgebuffer,
                            int                        rule)
{
    int i;
    int marked_id = 0;

#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter) {
        dlprintf("Before filtering:\n");
        gx_edgebuffer_print_tr_app(edgebuffer);
    }
#endif

    for (i=0; i < edgebuffer->height; i++) {
        int *row      = &edgebuffer->table[edgebuffer->index[i]];
        int  rowlen   = *row++;
        int *rowstart = row;
        int *rowout   = row;
        int  ll, llid, lr, lrid, rlid, rr, rrid, wind, marked_to;

        /* Avoid double setting pixels, by keeping where we have marked to. */
        marked_to = INT_MIN;
        while (rowlen > 0) {
            if (rule == gx_rule_even_odd) {
                /* Even Odd */
                ll   = *row++;
                llid = (*row++)>>1;
                lr   = *row++;
                lrid = *row++;
                rowlen--;

                /* We will fill solidly from ll to at least lr, possibly further */
                assert(rowlen > 0);
                (void)row++; /* rl not needed here */
                (void)row++;
                rr   = *row++;
                rrid = *row++;
                rowlen--;
                if (rr > lr) {
                    lr   = rr;
                    lrid = rrid;
                }
            } else {
                /* Non-Zero */
                int w;

                ll   = *row++;
                llid = *row++;
                lr   = *row++;
                lrid = *row++;
                wind = -(llid&1) | 1;
                llid >>= 1;
                rowlen--;

                assert(rowlen > 0);
                do {
                    (void)row++; /* rl not needed */
                    rlid = *row++;
                    rr   = *row++;
                    rrid = *row++;
                    w = -(rlid&1) | 1;
                    rlid >>= 1;
                    rowlen--;
                    if (rr > lr) {
                        lr   = rr;
                        lrid = rrid;
                    }
                    wind += w;
                    if (wind == 0)
                        break;
                } while (rowlen > 0);
            }

            if (lr < marked_to)
                continue;

            if (marked_to >= ll) {
                if (rowout == rowstart) {
                    ll   = marked_to;
                    llid = --marked_id;
                } else {
                    rowout -= 4;
                    ll   = rowout[0];
                    llid = rowout[1];
                }
            }

            if (lr >= ll) {
                *rowout++ = ll;
                *rowout++ = llid;
                *rowout++ = lr;
                *rowout++ = lrid;
                marked_to = lr;
            }
        }
        rowstart[-1] = (rowout - rowstart)>>2;
    }
    return 0;
}

/* Step 6: Fill */
int
gx_fill_edgebuffer_tr_app(gx_device       * gs_restrict pdev,
                    const gx_device_color * gs_restrict pdevc,
                          gx_edgebuffer   * gs_restrict edgebuffer,
                          int                        log_op)
{
    int i, j, code;
    int mfb = pdev->max_fill_band;

#ifdef DEBUG_SCAN_CONVERTER
    if (debugging_scan_converter) {
        dlprintf("Filling:\n");
        gx_edgebuffer_print_filtered_tr_app(edgebuffer);
    }
#endif

    for (i=0; i < edgebuffer->height; ) {
        int *row    = &edgebuffer->table[edgebuffer->index[i]];
        int  rowlen = *row++;
        int *row2;
        int *rowptr;
        int *row2ptr;
        int y_band_max;

        if (mfb) {
            y_band_max = (i & ~(mfb-1)) + mfb;
            if (y_band_max > edgebuffer->height)
                y_band_max = edgebuffer->height;
        } else {
            y_band_max = edgebuffer->height;
        }

        /* See how many scanlines match i */
        for (j = i+1; j < y_band_max; j++) {
            int row2len;

            row2    = &edgebuffer->table[edgebuffer->index[j]];
            row2len = *row2++;
            row2ptr = row2;
            rowptr  = row;

            if (rowlen != row2len)
                break;
            while (row2len > 0) {
                if (rowptr[1] != row2ptr[1] || rowptr[3] != row2ptr[3])
                    goto rowdifferent;
                rowptr  += 4;
                row2ptr += 4;
                row2len--;
            }
        }
rowdifferent:{}

        /* So j is the first scanline that doesn't match i */

        /* The first scanline is always sent as rectangles */
        while (rowlen > 0) {
            int left  = row[0];
            int right = row[2];
            row += 4;
            left = fixed2int(left);
            right = fixed2int(right + fixed_1 - 1);
            rowlen--;

            right -= left;
            if (right > 0) {
#ifdef DEBUG_OUTPUT_SC_AS_PS
                dlprintf("0.001 setlinewidth 1 0 0 setrgbcolor %%red %%PS\n");
                coord("moveto", int2fixed(left), int2fixed(edgebuffer->base+i));
                coord("lineto", int2fixed(left+right), int2fixed(edgebuffer->base+i));
                coord("lineto", int2fixed(left+right), int2fixed(edgebuffer->base+i+1));
                coord("lineto", int2fixed(left), int2fixed(edgebuffer->base+i+1));
                dlprintf("closepath stroke %%PS\n");
#endif
                if (log_op < 0)
                    code = dev_proc(pdev, fill_rectangle)(pdev, left, edgebuffer->base+i, right, 1, pdevc->colors.pure);
                else
                    code = gx_fill_rectangle_device_rop(left, edgebuffer->base+i, right, 1, pdevc, pdev, (gs_logical_operation_t)log_op);
                if (code < 0)
                    return code;
            }
        }

        /* The middle section (all but the first and last
         * scanlines) can be sent as a trapezoid. */
        if (i + 2 < j) {
            gs_fixed_edge le;
            gs_fixed_edge re;
            fixed ybot = int2fixed(edgebuffer->base+i+1);
            fixed ytop = int2fixed(edgebuffer->base+j-1);
            int *row3, *row4;
            int offset = 1;
            row    = &edgebuffer->table[edgebuffer->index[i]];
            row2    = &edgebuffer->table[edgebuffer->index[i+1]];
            row3    = &edgebuffer->table[edgebuffer->index[j-2]];
            row4    = &edgebuffer->table[edgebuffer->index[j-1]];
            rowlen = *row;
            while (rowlen > 0) {
                /* The fill rules used by fill_trap state that if a
                 * pixel centre is touched by a boundary, the pixel
                 * will be filled iff the boundary is horizontal and
                 * the filled region is above it, or the boundary is
                 * not horizontal, and the filled region is to the
                 * right of it.
                 *
                 * We need to fill "any part of a pixel", not just
                 * "centre covered", so we need to adjust our edges
                 * by half a pixel in both X and Y.
                 *
                 * X is relatively easy. We move the left edge back by
                 * just less than half, so ...00 goes to ...81 and
                 * therefore does not cause an extra pixel to get filled.
                 *
                 * Similarly, we move the right edge forward by half, so
                 *  ...00 goes to ...80 and therefore does not cause an
                 * extra pixel to get filled.
                 *
                 * For y, we can adjust edges up or down as appropriate.
                 * We move up by half, so ...0 goes to ..80 and therefore
                 * does not cause an extra pixel to get filled. We move
                 * down by just less than a half so that ...0 goes to
                 * ...81 and therefore does not cause an extra pixel to
                 * get filled.
                 *
                 * We use ybot = ...80 and ytop = ...81 in the trap call
                 * so that it just covers the pixel centres.
                 */
                if (row[offset] <= row4[offset]) {
                    le.start.x = row2[offset] - (fixed_half-1);
                    le.end.x   = row4[offset] - (fixed_half-1);
                    le.start.y = ybot + fixed_half;
                    le.end.y   = ytop + fixed_half;
                } else {
                    le.start.x = row [offset] - (fixed_half-1);
                    le.end.x   = row3[offset] - (fixed_half-1);
                    le.start.y = ybot - (fixed_half-1);
                    le.end.y   = ytop - (fixed_half-1);
                }
                if (row[offset+2] <= row4[offset+2]) {
                    re.start.x = row [offset+2] + fixed_half;
                    re.end.x   = row3[offset+2] + fixed_half;
                    re.start.y = ybot - (fixed_half-1);
                    re.end.y   = ytop - (fixed_half-1);
                } else {
                    re.start.x = row2[offset+2] + fixed_half;
                    re.end.x   = row4[offset+2] + fixed_half;
                    re.start.y = ybot + fixed_half;
                    re.end.y   = ytop + fixed_half;
                }
                offset += 4;
                rowlen--;

                assert(re.start.x >= le.start.x);
                assert(re.end.x >= le.end.x);
                assert(le.start.y <= ybot + fixed_half);
                assert(re.start.y <= ybot + fixed_half);
                assert(le.end.y >= ytop - (fixed_half - 1));
                assert(re.end.y >= ytop - (fixed_half - 1));

#ifdef DEBUG_OUTPUT_SC_AS_PS
                dlprintf("0.001 setlinewidth 0 1 0 setrgbcolor %% green %%PS\n");
                coord("moveto", le.start.x, le.start.y);
                coord("lineto", le.end.x, le.end.y);
                coord("lineto", re.end.x, re.end.y);
                coord("lineto", re.start.x, re.start.y);
                dlprintf("closepath stroke %%PS\n");
#endif
                code = dev_proc(pdev, fill_trapezoid)(
                                pdev,
                                &le,
                                &re,
                                ybot + fixed_half,
                                ytop - (fixed_half - 1),
                                0, /* bool swap_axes */
                                pdevc, /*const gx_drawing_color *pdcolor */
                                log_op);
                if (code < 0)
                    return code;
            }
        }

        if (i + 1 < j)
        {
            /* The last scanline is always sent as rectangles */
            row    = &edgebuffer->table[edgebuffer->index[j-1]];
            rowlen = *row++;
            while (rowlen > 0) {
                int left  = row[0];
                int right = row[2];
                row += 4;
                left = fixed2int(left);
                right = fixed2int(right + fixed_1 - 1);
                rowlen--;

                right -= left;
                if (right > 0) {
#ifdef DEBUG_OUTPUT_SC_AS_PS
                    dlprintf("0.001 setlinewidth 0 0 1 setrgbcolor %% blue %%PS\n");
                    coord("moveto", int2fixed(left), int2fixed(edgebuffer->base+j-1));
                    coord("lineto", int2fixed(left+right), int2fixed(edgebuffer->base+j-1));
                    coord("lineto", int2fixed(left+right), int2fixed(edgebuffer->base+j));
                    coord("lineto", int2fixed(left), int2fixed(edgebuffer->base+j));
                    dlprintf("closepath stroke %%PS\n");
#endif
                    if (log_op < 0)
                        code = dev_proc(pdev, fill_rectangle)(pdev, left, edgebuffer->base+j-1, right, 1, pdevc->colors.pure);
                    else
                        code = gx_fill_rectangle_device_rop(left, edgebuffer->base+j-1, right, 1, pdevc, pdev, (gs_logical_operation_t)log_op);
                    if (code < 0)
                        return code;
                }
            }
        }
        i = j;
    }
    return 0;
}


void
gx_edgebuffer_init(gx_edgebuffer * edgebuffer)
{
    edgebuffer->base   = 0;
    edgebuffer->height = 0;
    edgebuffer->index  = NULL;
    edgebuffer->table  = NULL;
}

void
gx_edgebuffer_fin(gx_device     * pdev,
                  gx_edgebuffer * edgebuffer)
{
    gs_free_object(pdev->memory, edgebuffer->table, "scanc intersects buffer");
    gs_free_object(pdev->memory, edgebuffer->index, "scanc index buffer");
    edgebuffer->index = NULL;
    edgebuffer->table = NULL;
}

gx_scan_converter_t gx_scan_converter =
{
    gx_scan_convert,
    gx_filter_edgebuffer,
    gx_fill_edgebuffer
};

gx_scan_converter_t gx_scan_converter_app =
{
    gx_scan_convert_app,
    gx_filter_edgebuffer_app,
    gx_fill_edgebuffer_app
};

gx_scan_converter_t gx_scan_converter_tr =
{
    gx_scan_convert_tr,
    gx_filter_edgebuffer_tr,
    gx_fill_edgebuffer_tr
};

gx_scan_converter_t gx_scan_converter_tr_app =
{
    gx_scan_convert_tr_app,
    gx_filter_edgebuffer_tr_app,
    gx_fill_edgebuffer_tr_app
};

int
gx_scan_convert_and_fill(const gx_scan_converter_t *sc,
                               gx_device       *dev,
                               gx_path         *ppath,
                         const gs_fixed_rect   *ibox,
                               fixed            flat,
                               int              rule,
                         const gx_device_color *pdevc,
                               int              lop)
{
    int code;
    gx_edgebuffer eb;
    gs_fixed_rect ibox2 = *ibox;
    int height;
    int mfb = dev->max_fill_band;

    if (mfb != 0) {
        ibox2.p.y &= ~(mfb-1);
        ibox2.q.y = (ibox2.q.y+mfb-1) & ~(mfb-1);
    }
    height = ibox2.q.y - ibox2.p.y;

    do {
        gx_edgebuffer_init(&eb);
        while (1) {
            ibox2.q.y = ibox2.p.y + height;
            if (ibox2.q.y > ibox->q.y)
                ibox2.q.y = ibox->q.y;
            code = sc->scan_convert(dev,
                                    ppath,
                                    &ibox2,
                                    &eb,
                                    flat);
            if (code <= 0)
                break;
            /* Let's shrink the ibox and try again */
            if (mfb && height == mfb) {
                /* Can't shrink the height any more! */
                code = gs_error_rangecheck;
                break;
            }
            height = height/code;
            if (mfb)
                height = (height + mfb-1) & ~(mfb-1);
            if (height < (mfb ? mfb : 1)) {
                code = gs_error_VMerror;
                break;
            }
        }
        if (code >= 0)
            code = sc->filter(dev,
                              &eb,
                              rule);
        if (code >= 0)
            code = sc->fill(dev,
                            pdevc,
                            &eb,
                            lop);
        gx_edgebuffer_fin(dev,&eb);
        ibox2.p.y += height;
    }
    while (ibox2.p.y < ibox->q.y);

    return code;
}
