/* Copyright (C) 2001-2016 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* Path stroking procedures for Ghostscript library */
#include "math_.h"
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

#ifdef DEBUG_SCAN_CONVERTER
static void
gx_edgebuffer_print(gx_edgebuffer * edgebuffer)
{
    int i;

    dlprintf1("Edgebuffer %x\n", edgebuffer);
    dlprintf4("xmin=%d xmax=%d base=%d height=%d\n",
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

static void mark_line(fixed sx, fixed sy, fixed ex, fixed ey, int base_y, int height, int *table, int *index)
{
    int delta;
    int isy = fixed2int(sy + fixed_half);
    int iey = fixed2int(ey + fixed_half);
    int dirn = DIRN_UP;

#ifdef DEBUG_SCAN_CONVERTER
    dlprintf6("Marking line from %x,%x to %x,%x (%x,%x)\n", sx, sy, ex, ey, isy, iey);
#endif
#ifdef DEBUG_OUTPUT_SC_AS_PS
    dlprintf("0.001 setlinewidth 0 0 0 setrgbcolor %%PS\n");
    dlprintf2("16#%x 16#%x moveto %%PS\n", sx, sy);
    dlprintf2("16#%x 16#%x lineto %%PS\n", ex, ey);
    dlprintf("stroke %%PS\n");
#endif

    if (isy == iey)
        return;
    if (isy > iey) {
        int t;
        t = isy; isy = iey; iey = t;
        t = sx; sx = ex; ex = t;
        dirn = DIRN_DOWN;
    }
    /* So we now have to mark a line of intersects from (sx,sy) to (ex,ey) */
    iey -= isy;
    ex -= sx;
    isy -= base_y;
#ifdef DEBUG_SCAN_CONVERTER
    dlprintf2("    sy=%d ey=%d\n", isy, iey);
#endif
    if (ex >= 0) {
        int x_inc, n_inc, f;

        /* We want to change sx by ex in iey steps. So each step, we add
         * ex/iey to sx. That's x_inc + n_inc/iey.
         */
       x_inc = ex/iey;
       n_inc = ex-(x_inc*iey);
       f     = iey>>1;
       /* Do a half step to start us off */
       sx += x_inc>>1;
       f  -= n_inc>>1;
       if (f < 0) {
           f += iey;
           sx++;
       }
       if (x_inc & 1) {
           f -= n_inc>>2;
           if (f < 0) {
               f += iey;
               sx++;
           }
       }
       delta = iey;
       do {
           int *row;

           if (isy >= 0 && isy < height) {
               row = &table[index[isy]];
               *row = (*row)+1; /* Increment the count */
               row[*row] = (sx&~1) | dirn;
           }
           isy++;
           sx += x_inc;
           f  -= n_inc;
           if (f < 0) {
               f += iey;
               sx++;
           }
        } while (--delta);
    } else {
        int x_dec, n_dec, f;

        ex = -ex;
        /* We want to change sx by ex in iey steps. So each step, we subtract
         * ex/iey from sx. That's x_dec + n_dec/iey.
         */
        x_dec = ex/iey;
        n_dec = ex-(x_dec*iey);
        f     = iey>>1;
        /* Do a half step to start us off */
        sx -= x_dec>>1;
        f  -= n_dec>>1;
        if (f < 0) {
            f += iey;
            sx--;
        }
        if (x_dec & 1) {
            f -= n_dec>>2;
            if (f < 0) {
                f += iey;
                sx--;
            }
        }
        delta = iey;
        do {
            int *row;

            if (isy >= 0 && isy < height) {
                row = &table[index[isy]];
                (*row)++; /* Increment the count */
                row[*row] = (sx&~1) | dirn;
            }
            isy++;
            sx -= x_dec;
            f  -= n_dec;
            if (f < 0) {
                f += iey;
                sx--;
            }
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

static int make_bbox(gx_path       * path,
               const gs_fixed_rect * clip,
                     gs_fixed_rect * ibox,
                     fixed           adjust)
{
    gs_fixed_rect bbox;
    int           code;

    /* Find the bbox - fixed */
    code = gx_path_bbox(path, &bbox);
    if (code < 0)
        return code;

    if (clip) {
        if (bbox.p.y < clip->p.y)
            bbox.p.y = clip->p.y;
        if (bbox.q.y > clip->q.y)
            bbox.q.y = clip->q.y;
    }

    /* Convert to bbox - int */
    ibox->p.x = fixed2int(bbox.p.x-adjust);
    ibox->p.y = fixed2int(bbox.p.y-adjust);
    ibox->q.x = fixed2int(bbox.q.x-adjust+fixed_1);
    ibox->q.y = fixed2int(bbox.q.y-adjust+fixed_1);

    return 0;
}

static int make_table(gx_device     * pdev,
                      gx_path       * path,
                const gs_fixed_rect * ibox,
                      int             intersection_size,
                      fixed           adjust,
                      int           * scanlinesp,
                      int          ** indexp,
                      int          ** tablep)
{
    int            scanlines;
    const subpath *psub;
    int           *index;
    int           *table;
    int            i;
    int            offset, delta;

    *scanlinesp = 0;
    *indexp     = NULL;
    *tablep     = NULL;

    /* Step 1: Make us a table */
    scanlines = ibox->q.y-ibox->p.y;
    /* +1 simplifies the loop below */
    index     = (int *)gs_alloc_bytes(pdev->memory,
                                      (scanlines+1) * sizeof(*index),
                                      "scanc index buffer");
    if (index == NULL)
        return_error(gs_error_VMerror);

    /* Step 1 continued: Blank the index */
    for (i=0; i < scanlines+1; i++) {
        index[i] = 0;
    }

    /* Step 1 continued: Run through the path, filling in the index */
    for (psub = path->first_subpath; psub != 0;) {
        const segment *pseg = (const segment *)psub;
        fixed          ey = pseg->pt.y + adjust;
        fixed          iy = ey;

        while ((pseg = pseg->next) != 0 &&
               pseg->type != s_start
            ) {
            fixed sy = ey;
            ey = pseg->pt.y + adjust;

#ifdef DEBUG_SCAN_CONVERTER
            dlprintf1("%d ", pseg->type);
#endif
            switch (pseg->type) {
                default:
                case s_start: /* Should never happen */
                case s_dash:  /* We should never be seeing a dash here */
                    assert("This should never happen" == NULL);
                    break;
                case s_curve: {
                    const curve_segment *const pcur = (const curve_segment *)pseg;
                    fixed c1y = pcur->p1.y + adjust;
                    fixed c2y = pcur->p2.y + adjust;
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
                    iminy = fixed2int(miny) - ibox->p.y;
                    if (iminy < 0)
                        iminy = 0;
                    if (iminy < scanlines) {
                        imaxy = fixed2int(maxy) - ibox->p.y;
                        if (imaxy >= 0) {
                            index[iminy]+=3;
                            if (imaxy < scanlines)
                                index[imaxy+1]-=3;
                        }
                    }
                    break;
                }
                case s_gap:
                case s_line:
                case s_line_close: {
                    fixed miny, maxy;
                    int imaxy, iminy;
                    if (sy == ey)
                        break;
                    if (sy < ey)
                        miny = sy, maxy = ey;
                    else
                        miny = ey, maxy = sy;
                    iminy = fixed2int(miny) - ibox->p.y;
                    if (iminy < 0)
                        iminy = 0;
                    if (iminy < scanlines) {
                        imaxy = fixed2int(maxy) - ibox->p.y;
                        if (imaxy >= 0) {
                            index[iminy]++;
                            if (imaxy < scanlines) {
                                index[imaxy+1]--;
                            }
                        }
                    }
                    break;
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
                iminy = fixed2int(miny) - ibox->p.y;
                if (iminy < 0)
                    iminy = 0;
                if (iminy < scanlines) {
                    imaxy = fixed2int(maxy) - ibox->p.y;
                    if (imaxy >= 0) {
                        index[iminy]++;
                        if (imaxy < scanlines) {
                            index[imaxy+1]--;
                        }
                    }
                }
            }
        }
#ifdef DEBUG_SCAN_CONVERTER
        dlprintf("\n");
#endif
        psub = (const subpath *)pseg;
    }

    /* Step 1 continued: index now contains a list of deltas (how the
     * number of intersects on line x differs from the number on line x-1).
     * First convert them to be the real number of intersects on that line.
     * Sum these values to get us the total nunber of intersects. Then
     * convert the table to be a list of offsets into the real intersect
     * buffer. */
    offset = 0;
    delta  = 0;
    for (i=0; i < scanlines; i++) {
        delta    += intersection_size*index[i];  /* delta = Num ints on this scanline. */
        index[i]  = offset;                      /* Offset into table for this lines data. */
        offset   += delta+1;                     /* Adjust offset for next line. */
    }

    /* End of step 1: index[i] = offset into table 2 for scanline i's
     * intersection data. offset = Total number of int entries required for
     * table. */

    /* Step 2: Collect the real intersections */
    table = (int *)gs_alloc_bytes(pdev->memory, offset * sizeof(*table),
                                  "scanc intersects buffer");
    if (table == NULL) {
        gs_free_object(pdev->memory, table, "scanc index buffer");
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

int gx_scan_convert(gx_device     * pdev,
                    gx_path       * path,
              const gs_fixed_rect * clip,
                    gx_edgebuffer * edgebuffer,
                    fixed           fixed_flat)
{
    gs_fixed_rect  ibox;
    int            scanlines;
    const subpath *psub;
    int           *index;
    int           *table;
    int            i;
    int            code;

    edgebuffer->index = NULL;
    edgebuffer->table = NULL;

    /* Bale out if no actual path. We see this with the clist */
    if (path->first_subpath == NULL)
        return 0;

    code = make_bbox(path, clip, &ibox, fixed_half);
    if (code < 0)
        return code;

    if (ibox.q.y <= ibox.p.y)
        return 0;

    code = make_table(pdev, path, &ibox, 1, fixed_half, &scanlines, &index, &table);
    if (code < 0)
        return code;

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

                    mark_curve(sx, sy, pcur->p1.x, pcur->p1.y, pcur->p2.x, pcur->p2.y, ex, ey, ibox.p.y, scanlines, table, index, k);
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

    /* Step 2 complete: We now have a complete list of intersection data in
     * table, indexed by index. */

    edgebuffer->base   = ibox.p.y;
    edgebuffer->height = scanlines;
    edgebuffer->xmin   = ibox.p.x;
    edgebuffer->xmax   = ibox.q.x;
    edgebuffer->index  = index;
    edgebuffer->table  = table;

#ifdef DEBUG_SCAN_CONVERTER
    dlprintf("Before sorting:\n");
    gx_edgebuffer_print(edgebuffer);
#endif

    /* Step 3: Sort the intersects on x */
    for (i=0; i < scanlines; i++) {
        int *row = &table[index[i]];
        int  rowlen = *row++;

        /* Sort the 'rowlen' entries - *must* be faster to do this with
         * custom code, as the function call overhead on the comparisons
         * will be a killer */
        qsort(row, rowlen, sizeof(int), intcmp);
    }

    return 0;
}

/* Step 5: Filter the intersections according to the rules */
int
gx_filter_edgebuffer(gx_device       * pdev,
                     gx_edgebuffer   * edgebuffer,
                     int               rule)
{
    int i;

#ifdef DEBUG_SCAN_CONVERTER
    dlprintf("Before filtering:\n");
    gx_edgebuffer_print(edgebuffer);
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
gx_fill_edgebuffer(gx_device       * pdev,
             const gx_device_color * pdevc,
                   gx_edgebuffer   * edgebuffer,
                   int               log_op)
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
    int left  = ((int*)a)[0]&~1;
    int right = ((int*)b)[0]&~1;
    if (left != right)
        return left - right;
    return ((int*)a)[1] - ((int*)b)[1];
}

#ifdef DEBUG_SCAN_CONVERTER
static void
gx_edgebuffer_print_app(gx_edgebuffer * edgebuffer)
{
    int i;
    int borked = 0;

    dlprintf1("Edgebuffer %x\n", edgebuffer);
    dlprintf4("xmin=%d xmax=%d base=%d height=%d\n",
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
    int    d; /* 0 up (or horiz), 1 down, -1 uninited */

    int    first;
    fixed  save_left;
    fixed  save_right;
    fixed  save_y;
    int    save_d;

    int    scanlines;
    int   *table;
    int   *index;
    int    base;
} cursor;

static void
output_cursor(cursor *cr, fixed x)
{
    int iy = fixed2int(cr->y) - cr->base;
    int *row;
    int count;

    if (iy < 0 || iy >= cr->scanlines) {
        /* Out of range, nothing to do */
    } else if (cr->first) {
        /* Store this one for later, for when we match up */
        cr->save_left  = cr->left;
        cr->save_right = cr->right;
        cr->save_y     = cr->y;
        cr->save_d     = cr->d;
        cr->first      = 0;
    } else {
        /* Put it in the table */
        assert(cr->d != DIRN_UNSET);

        row = &cr->table[cr->index[iy]];
        *row = count = (*row)+1; /* Increment the count */
        row[2 * count-1] = (cr->left&~1) | cr->d;
        row[2 * count  ] = cr->right;
    }
    cr->left  = x;
    cr->right = x;
}

static void
flush_cursor(cursor *cr, fixed x)
{
    /* This should only happen if we were entirely out of bounds */
    if (cr->first)
        return;

    /* Merge save into current if we can */
    if (fixed2int(cr->y) == fixed2int(cr->save_y) &&
        (cr->d == cr->save_d || cr->save_d == DIRN_UNSET)) {
        if (cr->left > cr->save_left)
            cr->left = cr->save_left;
        if (cr->right < cr->save_right)
            cr->right = cr->save_right;
        output_cursor(cr, x);
        return;
    }

    /* Merge not possible */
    output_cursor(cr, x);
    cr->left  = cr->save_left;
    cr->right = cr->save_right;
    cr->y     = cr->save_y;
    if (cr->save_d != -1)
        cr->d = cr->save_d;
    output_cursor(cr, x);
}

static void mark_line_app(cursor *cr, fixed sx, fixed sy, fixed ex, fixed ey)
{
    int isy = fixed2int(sy) - cr->base;
    int iey = fixed2int(ey) - cr->base;

    if (sx == ex && sy == ey)
        return;
#ifdef DEBUG_SCAN_CONVERTER
    dlprintf6("Marking line from %x,%x to %x,%x (%x,%x)\n", sx, sy, ex, ey, isy, iey);
#endif
#ifdef DEBUG_OUTPUT_SC_AS_PS
    dlprintf("0.001 setlinewidth 0 0 0 setrgbcolor %%PS\n");
    dlprintf2("16#%x 16#%x moveto %%PS\n", sx, sy);
    dlprintf2("16#%x 16#%x lineto %%PS\n", ex, ey);
    dlprintf("stroke %%PS\n");
#endif

    assert(cr->y == sy && cr->left <= sx && cr->right >= sx && cr->d >= DIRN_UNSET && cr->d <= DIRN_DOWN);

    /* First, deal with lines that don't change scanline.
     * This accommodates horizontal lines. */
    if (isy == iey) {
        if (sy == ey) {
            /* Horzizontal line. Don't change cr->d, don't flush. */
        } else if (sy > ey) {
            /* Falling line, flush if previous was rising */
            if (cr->d == DIRN_UP)
                output_cursor(cr, sx);
            cr->d = DIRN_DOWN;
        } else {
            /* Rising line, flush if previous was falling */
            if (cr->d == DIRN_DOWN)
                output_cursor(cr, sx);
            cr->d = DIRN_UP;
        }
        if (sx <= ex) {
            if (sx < cr->left)
                cr->left = sx;
            if (ex > cr->right)
                cr->right = ex;
        } else {
            if (ex < cr->left)
                cr->left = ex;
            if (sx > cr->right)
                cr->right = sx;
        }
        cr->y = ey;
    } else if (sy < ey) {
        /* So lines increasing in y. */
        fixed y_steps = ey - sy;
        /* We want to change from sy to ey, which are guaranteed to be on
         * different scanlines. We do this in 3 phases.
         * Phase 1 gets us from sy to the next scanline boundary.
         * Phase 2 gets us all the way to the last scanline boundary.
         * Phase 3 gets us from the last scanline boundary to ey.
         */
        int phase1_y_steps = (fixed_1 - sy) & (fixed_1 - 1);
        int phase3_y_steps = ey & (fixed_1 - 1);

        if (cr->d == DIRN_DOWN)
            output_cursor(cr, sx);
        cr->d = DIRN_UP;

        if (sx <= ex) {
            /* Lines increasing in x. */
            int phase1_x_steps, phase3_x_steps;
            fixed x_steps = ex - sx;

            assert(cr->left <= sx);
            /* Phase 1: */
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                sx += phase1_x_steps;
                if (cr->right < sx)
                    cr->right = sx;
                x_steps -= phase1_x_steps;
                /* If phase 1 will move us into a new scanline, then we must
                 * flush it before we move. */
                if (fixed2int(cr->y) != fixed2int(cr->y + phase1_y_steps))
                    output_cursor(cr, sx);
                cr->y += phase1_y_steps;
                sy += phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0)
                    return;
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;

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
                while (y_steps) {
                    cr->left = sx;
                    sx += x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx++;
                    cr->right = sx;
                    y_steps--;
                    output_cursor(cr, sx);
                    cr->y += fixed_1;
                }
            }

            /* Phase 3 */
            cr->left  = sx;
            cr->right = ex;
            cr->y += phase3_y_steps;
        } else {
            /* Lines decreasing in x. */
            int phase1_x_steps, phase3_x_steps;
            fixed x_steps = sx - ex;

            assert(cr->right >= sx);
            /* Phase 1: */
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                sx -= phase1_x_steps;
                if (cr->left > sx)
                    cr->left = sx;
                x_steps -= phase1_x_steps;
                /* If phase 1 will move us into a new scanline, then we must
                 * flush it before we move. */
                if (fixed2int(cr->y) != fixed2int(cr->y + phase1_y_steps))
                    output_cursor(cr, sx);
                cr->y += phase1_y_steps;
                sy += phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0)
                    return;
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;

            /* Phase 2: */
            assert((y_steps & (fixed_1 - 1)) == 0);
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps) {
                /* We want to change sx by x_steps in y_steps steps.
                 * So each step, we sub x_steps/y_steps from sx. That's x_inc + n_inc/ey. */
                int x_inc = x_steps/y_steps;
                int n_inc = x_steps - (x_inc * y_steps);
                int f = y_steps/2;
                int d = y_steps;
                while (y_steps) {
                    cr->right = sx;
                    sx -= x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx--;
                    cr->left = sx;
                    y_steps--;
                    output_cursor(cr, sx);
                    cr->y += fixed_1;
                }
            }

            /* Phase 3 */
            cr->right = sx;
            cr->left  = ex;
            cr->y += phase3_y_steps;
        }
    } else {
        /* So lines decreasing in y. */
        fixed y_steps = sy - ey;
        /* We want to change from sy to ey, which are guaranteed to be on
         * different scanlines. We do this in 3 phases.
         * Phase 1 gets us from sy to the next scanline boundary.
         * Phase 2 gets us all the way to the last scanline boundary.
         * Phase 3 gets us from the last scanline boundary to ey.
         */
        int phase1_y_steps = sy & (fixed_1 - 1);
        int phase3_y_steps = (fixed_1 - ey) & (fixed_1 - 1);

        if (cr->d == DIRN_UP)
            output_cursor(cr, sx);
        cr->d = DIRN_DOWN;

        if (sx <= ex) {
            /* Lines increasing in x. */
            int phase1_x_steps, phase3_x_steps;
            fixed x_steps = ex - sx;

            /* Phase 1: */
            assert(cr->left <= sx);
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                sx += phase1_x_steps;
                if (cr->right < sx)
                    cr->right = sx;
                x_steps -= phase1_x_steps;
                /* Phase 1 in a falling line never moves us into a new scanline. */
                sy -= phase1_y_steps;
                cr->y -= phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0)
                    return;
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;

            /* Phase 2: */
            assert((y_steps & (fixed_1 - 1)) == 0);
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps) {
                /* We want to change sx by x_steps in y_steps steps.
                 * So each step, we add x_steps/y_steps to sx. That's x_inc + n_inc/ey. */
                int x_inc = x_steps/y_steps;
                int n_inc = x_steps - (x_inc * y_steps);
                int f = y_steps/2;
                int d = y_steps;
                while (y_steps) {
                    output_cursor(cr, sx);
                    sx += x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx++;
                    cr->right = sx;
                    y_steps--;
                    cr->y -= fixed_1;
                }
            }

            /* Phase 3 */
            if (phase3_y_steps > 0)
                output_cursor(cr, sx);
            cr->left  = sx;
            cr->right = ex;
            cr->y -= phase3_y_steps;
        } else {
            /* Lines decreasing in x. */
            int phase1_x_steps, phase3_x_steps;
            fixed x_steps = sx - ex;

            /* Phase 1: */
            assert(cr->right >= sx);
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                sx -= phase1_x_steps;
                if (cr->left > sx)
                    cr->left = sx;
                x_steps -= phase1_x_steps;
                /* Phase 1 in a falling line never moves us into a new scanline. */
                sy -= phase1_y_steps;
                cr->y -= phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0)
                    return;
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;

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
                while (y_steps) {
                    output_cursor(cr, sx);
                    sx -= x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx--;
                    cr->left = sx;
                    y_steps--;
                    cr->y -= fixed_1;
                }
            }

            /* Phase 3 */
            if (phase3_y_steps > 0)
                output_cursor(cr, sx);
            cr->right = sx;
            cr->left  = ex;
            cr->y -= phase3_y_steps;
        }
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

int gx_scan_convert_app(gx_device     * pdev,
                        gx_path       * path,
                  const gs_fixed_rect * clip,
                        gx_edgebuffer * edgebuffer,
                        fixed           fixed_flat)
{
    gs_fixed_rect  ibox;
    int            scanlines;
    const subpath *psub;
    int           *index;
    int           *table;
    int            i;
    cursor         cr;
    int            code;

    edgebuffer->index = NULL;
    edgebuffer->table = NULL;

    /* Bale out if no actual path. We see this with the clist */
    if (path->first_subpath == NULL)
        return 0;

    code = make_bbox(path, clip, &ibox, 0);
    if (code < 0)
        return code;

    if (ibox.q.y <= ibox.p.y)
        return 0;

    code = make_table(pdev, path, &ibox, 2, 0, &scanlines, &index, &table);
    if (code < 0)
        return code;

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

        cr.left = cr.right = ex;
        cr.y = ey;
        cr.d = -1;
        cr.first = 1;

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

                    mark_curve_app(&cr, sx, sy, pcur->p1.x, pcur->p1.y, pcur->p2.x, pcur->p2.y, ex, ey, k);
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
        flush_cursor(&cr, ex);
        psub = (const subpath *)pseg;
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
    dlprintf("Before sorting:\n");
    gx_edgebuffer_print_app(edgebuffer);
#endif

    /* Step 3: Sort the intersects on x */
    for (i=0; i < scanlines; i++) {
        int *row = &table[index[i]];
        int  rowlen = *row++;

        /* Sort the 'rowlen' entries - *must* be faster to do this with
         * custom code, as the function call overhead on the comparisons
         * will be a killer */
        qsort(row, rowlen, 2*sizeof(int), edgecmp);
    }

    return 0;
}

/* Step 5: Filter the intersections according to the rules */
int
gx_filter_edgebuffer_app(gx_device       * pdev,
                         gx_edgebuffer   * edgebuffer,
                         int               rule)
{
    int i;

#ifdef DEBUG_SCAN_CONVERTER
    dlprintf("Before filtering:\n");
    gx_edgebuffer_print_app(edgebuffer);
#endif

    for (i=0; i < edgebuffer->height; i++) {
        int *row      = &edgebuffer->table[edgebuffer->index[i]];
        int  rowlen   = *row++;
        int *rowstart = row;
        int *rowout   = row;
        int  ll, lr, rl, rr, wind, marked_to;

        /* Avoid double setting pixels, by keeping where we have marked to. */
        marked_to = 0;
        while (rowlen > 0) {
            if (rule == gx_rule_even_odd) {
                /* Even Odd */
                ll = (*row++)&~1;
                lr = (*row++);
                rowlen--;
                wind = 1;

                /* We will fill solidly from ll to at least lr, possibly further */
                assert(rowlen > 0);
                do {
                    rl = (*row++)&~1;
                    rr = (*row++);
                    rowlen--;
                    if (rr > lr)
                        lr = rr;
                    wind ^= 1;
                    if (wind == 0)
                        break;
                } while (rowlen > 0);
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
                    rl = (*row++);
                    rr = (*row++);
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

            if (marked_to > ll) {
                if (rowout == rowstart)
                    ll = marked_to;
                else {
                    rowout -= 2;
                    ll = *rowout;
                }
            }

            if (lr > ll) {
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
gx_fill_edgebuffer_app(gx_device       * pdev,
                 const gx_device_color * pdevc,
                       gx_edgebuffer   * edgebuffer,
                       int               log_op)
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

    dlprintf1("Edgebuffer %x\n", edgebuffer);
    dlprintf4("xmin=%d xmax=%d base=%d height=%d\n",
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
    int delta;
    int isy = fixed2int(sy + fixed_half);
    int iey = fixed2int(ey + fixed_half);
    int dirn = DIRN_UP;

#ifdef DEBUG_SCAN_CONVERTER
    dlprintf6("Marking line from %x,%x to %x,%x (%x,%x)\n", sx, sy, ex, ey, isy, iey);
#endif
#ifdef DEBUG_OUTPUT_SC_AS_PS
    dlprintf("0.001 setlinewidth 0 0 0 setrgbcolor %%PS\n");
    dlprintf2("16#%x 16#%x moveto %%PS\n", sx, sy);
    dlprintf2("16#%x 16#%x lineto %%PS\n", ex, ey);
    dlprintf("stroke %%PS\n");
#endif

    if (isy == iey)
        return;
    if (isy > iey) {
        int t;
        t = isy; isy = iey; iey = t;
        t = sx; sx = ex; ex = t;
        dirn = DIRN_DOWN;
    }
    id = (id<<1) | dirn;
    /* So we now have to mark a line of intersects from (sx,sy) to (ex,ey) */
    iey -= isy;
    ex -= sx;
    isy -= base_y;
#ifdef DEBUG_SCAN_CONVERTER
    dlprintf2("    sy=%d ey=%d\n", isy, iey);
#endif
    if (ex >= 0) {
        int x_inc, n_inc, f;

        /* We want to change sx by ex in iey steps. So each step, we add
         * ex/iey to sx. That's x_inc + n_inc/iey.
         */
       x_inc = ex/iey;
       n_inc = ex-(x_inc*iey);
       f     = iey>>1;
       /* Do a half step to start us off */
       sx += x_inc>>1;
       f  -= n_inc>>1;
       if (f < 0) {
           f += iey;
           sx++;
       }
       if (x_inc & 1) {
           f -= n_inc>>2;
           if (f < 0) {
               f += iey;
               sx++;
           }
       }
       delta = iey;
       do {
           int *row;

           if (isy >= 0 && isy < height) {
               row = &table[index[isy]];
               *row = (*row)+1; /* Increment the count */
               row[*row * 2 - 1] = sx;
               row[*row * 2    ] = id;
           }
           isy++;
           sx += x_inc;
           f  -= n_inc;
           if (f < 0) {
               f += iey;
               sx++;
           }
        }
        while (--delta);
    } else {
        int x_dec, n_dec, f;

        ex = -ex;
        /* We want to change sx by ex in iey steps. So each step, we subtract
         * ex/iey from sx. That's x_dec + n_dec/iey.
         */
        x_dec = ex/iey;
        n_dec = ex-(x_dec*iey);
        f     = iey>>1;
        /* Do a half step to start us off */
        sx -= x_dec>>1;
        f  -= n_dec>>1;
        if (f < 0) {
            f += iey;
            sx--;
        }
        if (x_dec & 1) {
            f -= n_dec>>2;
            if (f < 0)
            {
                f += iey;
                sx--;
            }
        }
        delta = iey;
        do {
            int *row;

            if (isy >= 0 && isy < height) {
                row = &table[index[isy]];
                (*row)++; /* Increment the count */
                row[*row * 2 - 1] = sx;
                row[*row * 2    ] = id;
            }
            isy++;
            sx -= x_dec;
            f  -= n_dec;
            if (f < 0) {
                f += iey;
                sx--;
            }
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

int gx_scan_convert_tr(gx_device     * pdev,
                       gx_path       * path,
                 const gs_fixed_rect * clip,
                       gx_edgebuffer * edgebuffer,
                       fixed           fixed_flat)
{
    gs_fixed_rect  ibox;
    int            scanlines;
    const subpath *psub;
    int           *index;
    int           *table;
    int            i;
    int            code;
    int            id = 0;

    edgebuffer->index = NULL;
    edgebuffer->table = NULL;

    /* Bale out if no actual path. We see this with the clist */
    if (path->first_subpath == NULL)
        return 0;

    code = make_bbox(path, clip, &ibox, fixed_half);
    if (code < 0)
        return code;

    if (ibox.q.y <= ibox.p.y)
        return 0;

    code = make_table(pdev, path, &ibox, 2, fixed_half, &scanlines, &index, &table);
    if (code < 0)
        return code;

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

                    mark_curve_tr(sx, sy, pcur->p1.x, pcur->p1.y, pcur->p2.x, pcur->p2.y, ex, ey, ibox.p.y, scanlines, table, index, &id, k);
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

    /* Step 2 complete: We now have a complete list of intersection data in
     * table, indexed by index. */

    edgebuffer->base   = ibox.p.y;
    edgebuffer->height = scanlines;
    edgebuffer->xmin   = ibox.p.x;
    edgebuffer->xmax   = ibox.q.x;
    edgebuffer->index  = index;
    edgebuffer->table  = table;

#ifdef DEBUG_SCAN_CONVERTER
    dlprintf("Before sorting:\n");
    gx_edgebuffer_print_tr(edgebuffer);
#endif

    /* Step 4: Sort the intersects on x */
    for (i=0; i < scanlines; i++) {
        int *row = &table[index[i]];
        int  rowlen = *row++;

        /* Sort the 'rowlen' entries - *must* be faster to do this with
         * custom code, as the function call overhead on the comparisons
         * will be a killer */
        qsort(row, rowlen, 2*sizeof(int), intcmp_tr);
    }

    return 0;
}

/* Step 5: Filter the intersections according to the rules */
int
gx_filter_edgebuffer_tr(gx_device       * pdev,
                        gx_edgebuffer   * edgebuffer,
                        int               rule)
{
    int i;

#ifdef DEBUG_SCAN_CONVERTER
    dlprintf("Before filtering\n");
    gx_edgebuffer_print_tr(edgebuffer);
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
gx_fill_edgebuffer_tr(gx_device       * pdev,
                const gx_device_color * pdevc,
                      gx_edgebuffer   * edgebuffer,
                      int               log_op)
{
    int i, j, code;

    for (i=0; i < edgebuffer->height; ) {
        int *row    = &edgebuffer->table[edgebuffer->index[i]];
        int  rowlen = *row++;
        int *row2;
        int *rowptr;
        int *row2ptr;

        /* See how many scanlines match i */
        for (j = i+1; j < edgebuffer->height; j++) {
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
                    dlprintf("0.001 setlinewidth 1 0 0 setrgbcolor %%PS\n");
                    dlprintf2("16#%x 16#%x moveto %%PS\n", int2fixed(left), int2fixed(edgebuffer->base+i));
                    dlprintf2("16#%x 16#%x lineto %%PS\n", int2fixed(left+right), int2fixed(edgebuffer->base+i));
                    dlprintf2("16#%x 16#%x lineto %%PS\n", int2fixed(left+right), int2fixed(edgebuffer->base+i+1));
                    dlprintf2("16#%x 16#%x lineto %%PS\n", int2fixed(left), int2fixed(edgebuffer->base+i+1));
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
            le.start.y = re.start.y = int2fixed(edgebuffer->base+i);
            le.end.y   = re.end.y   = int2fixed(edgebuffer->base+j)-1;
            row2    = &edgebuffer->table[edgebuffer->index[j-1]+1];
            while (rowlen > 0) {
                le.start.x = row[0] - fixed_half;
                re.start.x = row[2] + fixed_half;
                le.end.x   = row2[0] - fixed_half;
                re.end.x   = row2[2] + fixed_half;
                row += 4;
                row2 += 4;
                rowlen -= 2;

                assert(le.start.x >= 0);
                assert(le.end.x >= 0);
                assert(re.start.x >= le.start.x);
                assert(re.end.x >= le.end.x);

#ifdef DEBUG_OUTPUT_SC_AS_PS
                dlprintf("0.001 setlinewidth 0 1 0 setrgbcolor %%PS\n");
                dlprintf2("16#%x 16#%x moveto %%PS\n", le.start.x, le.start.y);
                dlprintf2("16#%x 16#%x lineto %%PS\n", le.end.x, le.end.y);
                dlprintf2("16#%x 16#%x lineto %%PS\n", re.end.x, re.end.y);
                dlprintf2("16#%x 16#%x lineto %%PS\n", re.start.x, re.start.y);
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

    dlprintf1("Edgebuffer %x\n", edgebuffer);
    dlprintf4("xmin=%d xmax=%d base=%d height=%d\n",
              edgebuffer->xmin, edgebuffer->xmax, edgebuffer->base, edgebuffer->height);
    for (i=0; i < edgebuffer->height; i++)
    {
        int  offset = edgebuffer->index[i];
        int *row    = &edgebuffer->table[offset];
        int count   = *row++;
        int c       = count;
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

    dlprintf1("Edgebuffer %x\n", edgebuffer);
    dlprintf4("xmin=%d xmax=%d base=%d height=%d\n",
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
    int    d; /* 0 up (or horiz), 1 down, -1 uninited */

    int    first;
    fixed  save_left;
    int    save_lid;
    fixed  save_right;
    int    save_rid;
    fixed  save_y;
    int    save_d;

    int    scanlines;
    int   *table;
    int   *index;
    int    base;
} cursor_tr;

static void
output_cursor_tr(cursor_tr *cr, fixed x, int id)
{
    int iy = fixed2int(cr->y) - cr->base;
    int *row;
    int count;

    if (iy < 0 || iy >= cr->scanlines) {
        /* Nothing to do */
    }
    else if (cr->first) {
        /* Save it for later in case we join up */
        cr->save_left  = cr->left;
        cr->save_lid   = cr->lid;
        cr->save_right = cr->right;
        cr->save_rid   = cr->rid;
        cr->save_y     = cr->y;
        cr->save_d     = cr->d;
        cr->first      = 0;
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
    cr->left  = x;
    cr->lid   = id;
    cr->right = x;
    cr->rid   = id;
}

static void
flush_cursor_tr(cursor_tr *cr, fixed x, int id)
{
    /* This should only happen if we were entirely out of bounds */
    if (cr->first)
        return;

    /* Merge save into current if we can */
    if (fixed2int(cr->y) == fixed2int(cr->save_y) &&
        (cr->d == cr->save_d || cr->save_d == DIRN_UNSET)) {
        if (cr->left > cr->save_left) {
            cr->left = cr->save_left;
            cr->lid  = cr->save_lid;
        }
        if (cr->right < cr->save_right) {
            cr->right = cr->save_right;
            cr->rid = cr->save_rid;
        }
        output_cursor_tr(cr, x, id);
        return;
    }

    /* Merge not possible */
    output_cursor_tr(cr, x, id);
    cr->left  = cr->save_left;
    cr->lid   = cr->save_lid;
    cr->right = cr->save_right;
    cr->rid   = cr->save_rid;
    cr->y     = cr->save_y;
    if (cr->save_d != -1)
        cr->d = cr->save_d;
    output_cursor_tr(cr, x, id);
}

static void mark_line_tr_app(cursor_tr *cr, fixed sx, fixed sy, fixed ex, fixed ey, int id)
{
    int isy = fixed2int(sy) - cr->base;
    int iey = fixed2int(ey) - cr->base;
    fixed y_steps;

    if (sx == ex && sy == ey)
        return;
#ifdef DEBUG_SCAN_CONVERTER
    dlprintf6("Marking line from %x,%x to %x,%x (%x,%x)\n", sx, sy, ex, ey, isy, iey);
#endif
#ifdef DEBUG_OUTPUT_SC_AS_PS
    dlprintf("0.001 setlinewidth 0 0 0 setrgbcolor %%PS\n");
    dlprintf2("16#%x 16#%x moveto %%PS\n", sx, sy);
    dlprintf2("16#%x 16#%x lineto %%PS\n", ex, ey);
    dlprintf("stroke %%PS\n");
#endif

    assert(cr->y == sy && cr->left <= sx && cr->right >= sx && cr->d >= DIRN_UNSET && cr->d <= DIRN_DOWN);

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
        if (sy == ey) {
            /* Horzizontal line. Don't change cr->d, don't flush. */
        } else if (sy > ey) {
            /* Falling line, flush if previous was rising */
            if (cr->d == DIRN_UP)
                output_cursor_tr(cr, sx, id);
            cr->d = DIRN_DOWN;
        } else {
            /* Rising line, flush if previous was falling */
            if (cr->d == DIRN_DOWN)
                output_cursor_tr(cr, sx, id);
            cr->d = DIRN_UP;
        }
        if (sx <= ex) {
            if (sx < cr->left) {
                cr->left = sx;
                cr->lid  = id;
            }
            if (ex > cr->right) {
                cr->right = ex;
                cr->rid   = id;
            }
        } else {
            if (ex < cr->left) {
                cr->left = ex;
                cr->lid  = id;
            }
            if (sx > cr->right) {
                cr->right = sx;
                cr->rid   = id;
            }
        }
        cr->y = ey;
    } else if ((y_steps = ey - sy) > 0) {
        /* So lines increasing in y. */
        /* We want to change from sy to ey, which are guaranteed to be on
         * different scanlines. We do this in 3 phases.
         * Phase 1 gets us from sy to the next scanline boundary.
         * Phase 2 gets us all the way to the last scanline boundary.
         * Phase 3 gets us from the last scanline boundary to ey.
         */
        int phase1_y_steps = (fixed_1 - sy) & (fixed_1 - 1);
        int phase3_y_steps = ey & (fixed_1 - 1);

        if (cr->d == DIRN_DOWN)
            output_cursor_tr(cr, sx, id);
        cr->d = DIRN_UP;

        if (sx <= ex) {
            /* Lines increasing in x. */
            int phase1_x_steps, phase3_x_steps;
            fixed x_steps = ex - sx;

            assert(cr->left <= sx);
            /* Phase 1: */
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                sx += phase1_x_steps;
                if (cr->right <= sx) {
                    cr->right = sx;
                    cr->rid   = id;
                }
                x_steps -= phase1_x_steps;
                /* If phase 1 will move us into a new scanline, then we must
                 * flush it before we move. */
                if (fixed2int(cr->y) != fixed2int(cr->y + phase1_y_steps))
                    output_cursor_tr(cr, sx, id);
                cr->y += phase1_y_steps;
                sy += phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0)
                    return;
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;

            /* Phase 2: */
            cr->lid = id;
            cr->rid = id;
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps) {
                /* We want to change sx by x_steps in y_steps steps.
                 * So each step, we add x_steps/y_steps to sx. That's x_inc + n_inc/y_steps. */
                int x_inc = x_steps/y_steps;
                int n_inc = x_steps - (x_inc * y_steps);
                int f = y_steps/2;
                int d = y_steps;
                while (y_steps) {
                    cr->left = sx;
                    sx += x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx++;
                    cr->right = sx;
                    y_steps--;
                    output_cursor_tr(cr, sx, id);
                    cr->y += fixed_1;
                }
            }

            /* Phase 3 */
            cr->left  = sx;
            cr->right = ex;
            cr->y += phase3_y_steps;
        } else {
            /* Lines decreasing in x. */
            int phase1_x_steps, phase3_x_steps;
            fixed x_steps = sx - ex;

            assert(cr->right >= sx);
            /* Phase 1: */
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                sx -= phase1_x_steps;
                if (cr->left > sx) {
                    cr->left = sx;
                    cr->lid  = id;
                }
                x_steps -= phase1_x_steps;
                /* If phase 1 will move us into a new scanline, then we must
                 * flush it before we move. */
                if (fixed2int(cr->y) != fixed2int(cr->y + phase1_y_steps))
                    output_cursor_tr(cr, sx, id);
                cr->y += phase1_y_steps;
                sy += phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0)
                    return;
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;

            /* Phase 2: */
            cr->lid = id;
            cr->rid = id;
            assert((y_steps & (fixed_1 - 1)) == 0);
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps) {
                /* We want to change sx by x_steps in y_steps steps.
                 * So each step, we sub x_steps/y_steps from sx. That's x_inc + n_inc/ey. */
                int x_inc = x_steps/y_steps;
                int n_inc = x_steps - (x_inc * y_steps);
                int f = y_steps/2;
                int d = y_steps;
                while (y_steps) {
                    cr->right = sx;
                    sx -= x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx--;
                    cr->left = sx;
                    y_steps--;
                    output_cursor_tr(cr, sx, id);
                    cr->y += fixed_1;
                }
            }

            /* Phase 3 */
            cr->right = sx;
            cr->left  = ex;
            cr->y += phase3_y_steps;
        }
    } else {
        /* So lines decreasing in y. */
        /* We want to change from sy to ey, which are guaranteed to be on
         * different scanlines. We do this in 3 phases.
         * Phase 1 gets us from sy to the next scanline boundary.
         * Phase 2 gets us all the way to the last scanline boundary.
         * Phase 3 gets us from the last scanline boundary to ey.
         */
        int phase1_y_steps = sy & (fixed_1 - 1);
        int phase3_y_steps = (fixed_1 - ey) & (fixed_1 - 1);

        y_steps = -y_steps;
        /* Cope with the awkward 0x80000000 case. */
        if (y_steps < 0)
        {
          int mx, my;
          mx = sx + ((ex-sx)>>1);
          my = sy + ((ey-sy)>>1);
          mark_line_tr_app(cr, sx, sy, mx, my, id);
          mark_line_tr_app(cr, mx, my, ex, ey, id);
          return;
        }

        if (cr->d == DIRN_UP)
            output_cursor_tr(cr, sx, id);
        cr->d = DIRN_DOWN;

        if (sx <= ex) {
            /* Lines increasing in x. */
            int phase1_x_steps, phase3_x_steps;
            fixed x_steps = ex - sx;

            /* Phase 1: */
            assert(cr->left <= sx);
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                sx += phase1_x_steps;
                if (cr->right < sx) {
                    cr->right = sx;
                    cr->rid   = id;
                }
                x_steps -= phase1_x_steps;
                /* Phase 1 in a falling line never moves us into a new scanline. */
                sy -= phase1_y_steps;
                cr->y -= phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0)
                    return;
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;

            /* Phase 2: */
            assert((y_steps & (fixed_1 - 1)) == 0);
            y_steps = fixed2int(y_steps);
            assert(y_steps >= 0);
            if (y_steps) {
                /* We want to change sx by x_steps in y_steps steps.
                 * So each step, we add x_steps/y_steps to sx. That's x_inc + n_inc/ey. */
                int x_inc = x_steps/y_steps;
                int n_inc = x_steps - (x_inc * y_steps);
                int f = y_steps/2;
                int d = y_steps;
                while (y_steps) {
                    output_cursor_tr(cr, sx, id);
                    sx += x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx++;
                    cr->right = sx;
                    y_steps--;
                    cr->y -= fixed_1;
                }
            }

            /* Phase 3 */
            if (phase3_y_steps > 0)
                output_cursor_tr(cr, sx, id);
            cr->left  = sx;
            cr->right = ex;
            cr->y -= phase3_y_steps;
        } else {
            /* Lines decreasing in x. */
            int phase1_x_steps, phase3_x_steps;
            fixed x_steps = sx - ex;

            /* Phase 1: */
            assert(cr->right >= sx);
            if (phase1_y_steps) {
                phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
                sx -= phase1_x_steps;
                if (cr->left > sx) {
                    cr->left = sx;
                    cr->lid  = id;
                }
                x_steps -= phase1_x_steps;
                /* Phase 1 in a falling line never moves us into a new scanline. */
                sy -= phase1_y_steps;
                cr->y -= phase1_y_steps;
                y_steps -= phase1_y_steps;
                if (y_steps == 0)
                    return;
            }

            /* Phase 3: precalculation */
            phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
            x_steps -= phase3_x_steps;
            y_steps -= phase3_y_steps;

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
                while (y_steps) {
                    output_cursor_tr(cr, sx, id);
                    sx -= x_inc;
                    f -= n_inc;
                    if (f < 0)
                        f += d, sx--;
                    cr->left = sx;
                    y_steps--;
                    cr->y -= fixed_1;
                }
            }

            /* Phase 3 */
            if (phase3_y_steps > 0)
                output_cursor_tr(cr, sx, id);
            cr->right = sx;
            cr->left  = ex;
            cr->y -= phase3_y_steps;
        }
    }
}

static void mark_curve_tr_app(cursor_tr *cr, fixed sx, fixed sy, fixed c1x, fixed c1y, fixed c2x, fixed c2y, fixed ex, fixed ey, int depth, int *id)
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

int gx_scan_convert_tr_app(gx_device     * pdev,
                           gx_path       * path,
                     const gs_fixed_rect * clip,
                           gx_edgebuffer * edgebuffer,
                           fixed           fixed_flat)
{
    gs_fixed_rect  ibox;
    int            scanlines;
    const subpath *psub;
    int           *index;
    int           *table;
    int            i;
    cursor_tr      cr;
    int            code;
    int            id = 0;

    edgebuffer->index = NULL;
    edgebuffer->table = NULL;

    /* Bale out if no actual path. We see this with the clist */
    if (path->first_subpath == NULL)
        return 0;

    code = make_bbox(path, clip, &ibox, 0);
    if (code < 0)
        return code;

    if (ibox.q.y <= ibox.p.y)
        return 0;

    code = make_table(pdev, path, &ibox, 4, 0, &scanlines, &index, &table);
    if (code < 0)
        return code;

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

        cr.left = cr.right = ex;
        cr.lid = cr.rid = id+1;
        cr.y = ey;
        cr.d = -1;
        cr.first = 1;

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

                    mark_curve_tr_app(&cr, sx, sy, pcur->p1.x, pcur->p1.y, pcur->p2.x, pcur->p2.y, ex, ey, k, &id);
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
        flush_cursor_tr(&cr, ex, id);
        psub = (const subpath *)pseg;
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
    dlprintf("Before sorting\n");
    gx_edgebuffer_print_tr_app(edgebuffer);
#endif

    /* Step 3: Sort the intersects on x */
    for (i=0; i < scanlines; i++) {
        int *row = &table[index[i]];
        int  rowlen = *row++;

        /* Sort the 'rowlen' entries - *must* be faster to do this with
         * custom code, as the function call overhead on the comparisons
         * will be a killer */
        qsort(row, rowlen, 4*sizeof(int), edgecmp_tr);
    }

    return 0;
}

/* Step 5: Filter the intersections according to the rules */
int
gx_filter_edgebuffer_tr_app(gx_device       * pdev,
                            gx_edgebuffer   * edgebuffer,
                            int               rule)
{
    int i;
    int marked_id = 0;

#ifdef DEBUG_SCAN_CONVERTER
    dlprintf("Before filtering:\n");
    gx_edgebuffer_print_tr_app(edgebuffer);
#endif

    for (i=0; i < edgebuffer->height; i++) {
        int *row      = &edgebuffer->table[edgebuffer->index[i]];
        int  rowlen   = *row++;
        int *rowstart = row;
        int *rowout   = row;
        int  ll, llid, lr, lrid, rlid, rr, rrid, wind, marked_to;

        /* Avoid double setting pixels, by keeping where we have marked to. */
        marked_to = 0;
        while (rowlen > 0) {
            if (rule == gx_rule_even_odd) {
                /* Even Odd */
                ll   = *row++;
                llid = (*row++)>>1;
                lr   = *row++;
                lrid = *row++;
                rowlen--;
                wind = 1;

                /* We will fill solidly from ll to at least lr, possibly further */
                assert(rowlen > 0);
                do {
                    (void)row++; /* rl not needed here */
                    rlid = *row++>>1;
                    rr   = *row++;
                    rrid = *row++;
                    rowlen--;
                    if (rr > lr) {
                        lr   = rr;
                        lrid = rrid;
                    }
                    wind ^= 1;
                    if (wind == 0)
                        break;
                } while (rowlen > 0);
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

            if (marked_to > ll) {
                if (rowout == rowstart) {
                    ll   = marked_to;
                    llid = --marked_id;
                } else {
                    rowout -= 4;
                    ll   = rowout[0];
                    llid = rowout[1];
                }
            }

            if (lr > ll) {
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
gx_fill_edgebuffer_tr_app(gx_device       * pdev,
                    const gx_device_color * pdevc,
                          gx_edgebuffer   * edgebuffer,
                          int               log_op)
{
    int i, j, code;

#ifdef DEBUG_SCAN_CONVERTER
    dlprintf("Filling:\n");
    gx_edgebuffer_print_filtered_tr_app(edgebuffer);
#endif

    for (i=0; i < edgebuffer->height; ) {
        int *row    = &edgebuffer->table[edgebuffer->index[i]];
        int  rowlen = *row++;
        int *row2;
        int *rowptr;
        int *row2ptr;

        /* See how many scanlines match i */
        for (j = i+1; j < edgebuffer->height; j++) {
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
                dlprintf("0.001 setlinewidth 1 0 0 setrgbcolor %%PS\n");
                dlprintf2("16#%x 16#%x moveto %%PS\n", int2fixed(left), int2fixed(edgebuffer->base+i));
                dlprintf2("16#%x 16#%x lineto %%PS\n", int2fixed(left+right), int2fixed(edgebuffer->base+i));
                dlprintf2("16#%x 16#%x lineto %%PS\n", int2fixed(left+right), int2fixed(edgebuffer->base+i+1));
                dlprintf2("16#%x 16#%x lineto %%PS\n", int2fixed(left), int2fixed(edgebuffer->base+i+1));
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

                assert(le.start.x >= -fixed_half);
                assert(le.end.x >= -fixed_half);
                assert(re.start.x >= le.start.x);
                assert(re.end.x >= le.end.x);
                assert(le.start.y <= ybot + fixed_half);
                assert(re.start.y <= ybot + fixed_half);
                assert(le.end.y >= ytop - (fixed_half - 1));
                assert(re.end.y >= ytop - (fixed_half - 1));

#ifdef DEBUG_OUTPUT_SC_AS_PS
                dlprintf("0.001 setlinewidth 0 1 0 setrgbcolor %%PS\n");
                dlprintf2("16#%x 16#%x moveto %%PS\n", le.start.x, le.start.y);
                dlprintf2("16#%x 16#%x lineto %%PS\n", le.end.x, le.end.y);
                dlprintf2("16#%x 16#%x lineto %%PS\n", re.end.x, re.end.y);
                dlprintf2("16#%x 16#%x lineto %%PS\n", re.start.x, re.start.y);
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
                    dlprintf("0.001 setlinewidth 0 0 1 setrgbcolor %%PS\n");
                    dlprintf2("16#%x 16#%x moveto %%PS\n", int2fixed(left), int2fixed(edgebuffer->base+j-1));
                    dlprintf2("16#%x 16#%x lineto %%PS\n", int2fixed(left+right), int2fixed(edgebuffer->base+j-1));
                    dlprintf2("16#%x 16#%x lineto %%PS\n", int2fixed(left+right), int2fixed(edgebuffer->base+j));
                    dlprintf2("16#%x 16#%x lineto %%PS\n", int2fixed(left), int2fixed(edgebuffer->base+j));
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
