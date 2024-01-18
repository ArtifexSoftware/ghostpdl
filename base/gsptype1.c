/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* PatternType 1 pattern implementation */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsrop.h"
#include "gsstruct.h"
#include "gsutil.h"             /* for gs_next_ids */
#include "gxarith.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxcoord.h"            /* for gs_concat, gx_tr'_to_fixed */
#include "gxcspace.h"           /* for gscolor2.h */
#include "gxcolor2.h"
#include "gxdcolor.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxclip2.h"
#include "gspath.h"
#include "gxpath.h"
#include "gxpcolor.h"
#include "gxp1impl.h"           /* requires gxpcolor.h */
#include "gxclist.h"
#include "gzstate.h"
#include "gsimage.h"
#include "gsiparm4.h"
#include "gsovrc.h"
#include "gxdevsop.h"

/* Temporary switches for experimanting with Adobe compatibility. */
#define ADJUST_SCALE_FOR_THIN_LINES 0   /* Old code = 0 */
#define ADJUST_SCALE_BY_GS_TRADITION 0  /* Old code = 1 */
#define ADJUST_AS_ADOBE 1               /* Old code = 0 *//* This one is closer to Adobe. */

#define fastfloor(x) (((int)(x)) - (((x)<0) && ((x) != (float)(int)(x))))
#define fastfloord(x) (((int)(x)) - (((x)<0) && ((x) != (double)(int)(x))))

/* GC descriptors */
private_st_pattern1_template();
public_st_pattern1_instance();

/* GC procedures */
static ENUM_PTRS_BEGIN(pattern1_instance_enum_ptrs) {
    if (index < st_pattern1_template_max_ptrs) {
        gs_ptr_type_t ptype =
            ENUM_SUPER_ELT(gs_pattern1_instance_t, st_pattern1_template,
                           templat, 0);

        if (ptype)
            return ptype;
        return ENUM_OBJ(NULL);  /* don't stop early */
    }
    ENUM_PREFIX(st_pattern_instance, st_pattern1_template_max_ptrs);
} ENUM_PTRS_END
static RELOC_PTRS_BEGIN(pattern1_instance_reloc_ptrs) {
    RELOC_PREFIX(st_pattern_instance);
    RELOC_SUPER(gs_pattern1_instance_t, st_pattern1_template, templat);
} RELOC_PTRS_END

/* Define a PatternType 1 pattern. */
static pattern_proc_uses_base_space(gs_pattern1_uses_base_space);
static pattern_proc_make_pattern(gs_pattern1_make_pattern);
static pattern_proc_get_pattern(gs_pattern1_get_pattern);
static pattern_proc_set_color(gs_pattern1_set_color);
static const gs_pattern_type_t gs_pattern1_type = {
    1, {
        gs_pattern1_uses_base_space, gs_pattern1_make_pattern,
        gs_pattern1_get_pattern, gs_pattern1_remap_color,
        gs_pattern1_set_color
    }
};

/*
 * Build a PatternType 1 Pattern color space.
 */
int
gs_cspace_build_Pattern1(gs_color_space ** ppcspace,
                    gs_color_space * pbase_cspace, gs_memory_t * pmem)
{
    gs_color_space *pcspace = 0;

    if (pbase_cspace != 0) {
        if (gs_color_space_num_components(pbase_cspace) < 0)         /* Pattern space */
            return_error(gs_error_rangecheck);
    }
    pcspace = gs_cspace_alloc(pmem, &gs_color_space_type_Pattern);
    if (pcspace == NULL)
        return_error(gs_error_VMerror);
    if (pbase_cspace != 0) {
        pcspace->params.pattern.has_base_space = true;
        /* reference to base space shifts from pgs to pcs with no net change */
        pcspace->base_space = pbase_cspace;
    } else
        pcspace->params.pattern.has_base_space = false;
    *ppcspace = pcspace;
    return 0;
}

/* Initialize a PatternType 1 pattern template. */
void
gs_pattern1_init(gs_pattern1_template_t * ppat)
{
    gs_pattern_common_init((gs_pattern_template_t *)ppat, &gs_pattern1_type);
    ppat->uses_transparency = 0;        /* false */
    ppat->BM_Not_Normal = 0;            /* false */
}

/* Make an instance of a PatternType 1 pattern. */
static int compute_inst_matrix(gs_pattern1_instance_t *pinst,
                               gs_rect *pbbox, int width, int height,
                               float *bbw, float *bbh);
static int fix_bbox_after_matrix_adjustment(gs_pattern1_instance_t *pinst,
                                            gs_rect *pbbox);
int
gs_makepattern(gs_client_color * pcc, const gs_pattern1_template_t * pcp,
               const gs_matrix * pmat, gs_gstate * pgs, gs_memory_t * mem)
{
    return gs_pattern1_make_pattern(pcc, (const gs_pattern_template_t *)pcp,
                                    pmat, pgs, mem);
}

/* Limited accuracy due to Floating point implementation limits can
 * cause us headaches due to values not being representable.
 * This is particular bad when we start using matrix maths (and
 * inverse matrixes in particular) to map bboxes into device space.
 * We therefore have a set of functions to 'sanely' round stuff.
 * Essentially, any device space location that is sufficiently
 * close to an exact pixel boundary will be clamped to that boundary.
 */
#define SANE_THRESHOLD 0.001f

static int
sane_ceil(float f)
{
    int i = (int)f;

    if (f - i < SANE_THRESHOLD)
        return i;
    return i+1;
}
static float
sane_clamp_float(float f)
{
    int i = (int)fastfloor(f);

    if (f - i < SANE_THRESHOLD)
        return (float)i;
    else if (f - i > (1-SANE_THRESHOLD))
        return (float)(i+1);
    return f;
}
static double
sane_clamp_double(double d)
{
    int i = (int)fastfloord(d);

    if (d - i < SANE_THRESHOLD)
        return (double)i;
    else if (d - i > (1-SANE_THRESHOLD))
        return (double)(i+1);
    return d;
}
static void
sane_clamp_rect(gs_rect *r)
{
    double x0 = sane_clamp_double(r->p.x);
    double y0 = sane_clamp_double(r->p.y);
    double x1 = sane_clamp_double(r->q.x);
    double y1 = sane_clamp_double(r->q.y);

    /* Be careful not to round stuff to zero, because this breaks fts_15_1529.pdf */
    if (x0 != x1)
        r->p.x = x0, r->q.x = x1;
    if (y0 != y1)
        r->p.y = y0, r->q.y = y1;
}
static void
sane_clamp_matrix(gs_matrix *m)
{
    m->xx = sane_clamp_float(m->xx);
    m->xy = sane_clamp_float(m->xy);
    m->yx = sane_clamp_float(m->yx);
    m->yy = sane_clamp_float(m->yy);
    m->tx = sane_clamp_float(m->tx);
    m->ty = sane_clamp_float(m->ty);
}
static int
gs_pattern1_make_pattern(gs_client_color * pcc,
                         const gs_pattern_template_t * ptemp,
                         const gs_matrix * pmat, gs_gstate * pgs,
                         gs_memory_t * mem)
{
    const gs_pattern1_template_t *pcp = (const gs_pattern1_template_t *)ptemp;
    gs_pattern1_instance_t inst;
    gs_pattern1_instance_t *pinst;
    gs_gstate *saved;
    gs_rect bbox;
    gs_fixed_rect cbox;
    gx_device * pdev = pgs->device;
    int dev_width = pdev->width;
    int dev_height = pdev->height;
    int code = gs_make_pattern_common(pcc, (const gs_pattern_template_t *)pcp,
                                      pmat, pgs, mem,
                                      &st_pattern1_instance);
    float bbw, bbh;

    if (code < 0)
        return code;
    if (mem == 0)
        mem = gs_gstate_memory(pgs);
    pinst = (gs_pattern1_instance_t *)pcc->pattern;
#ifdef PACIFY_VALGRIND
    /* The following memset is required to avoid a valgrind warning
     * in:
     *   gs -I./gs/lib -sOutputFile=out.pgm -dMaxBitmap=10000
     *      -sDEVICE=pgmraw -r300 -Z: -sDEFAULTPAPERSIZE=letter
     *      -dNOPAUSE -dBATCH -K2000000 -dClusterJob -dJOBSERVER
     *      tests_private/ps/ps3cet/11-14.PS
     * Setting the individual elements of the structure directly is
     * not enough, which leads me to believe that we are writing the
     * entire struct out, padding and all.
     */
    memset(((char *)&inst) + sizeof(gs_pattern_instance_t), 0,
           sizeof(inst) - sizeof(gs_pattern_instance_t));
#endif
    *(gs_pattern_instance_t *)&inst = *(gs_pattern_instance_t *)pinst;
    saved = inst.saved;
    switch (pcp->PaintType) {
        case 1:         /* colored */
            gs_set_logical_op(saved, lop_default);
            break;
        case 2:         /* uncolored */
            code = gx_set_device_color_1(saved);
            if (code < 0)
                goto fsaved;
            break;
        default:
            code = gs_note_error(gs_error_rangecheck);
            goto fsaved;
    }

    inst.templat = *pcp;
    /* Even if the pattern wants to use transparency, don't permit it if there is no device which will support it */
    inst.templat.uses_transparency &= dev_proc( gs_currentdevice_inline(pgs), dev_spec_op)( gs_currentdevice_inline(pgs), gxdso_supports_pattern_transparency, NULL, 0);

    code = compute_inst_matrix(&inst, &bbox, dev_width, dev_height, &bbw, &bbh);
    if (code < 0)
        goto fsaved;

    /* Check if we will have any overlapping tiles.  If we do and there is
       transparency present, then we will need to blend when we tile.  We want
       to detect this since blending is expensive and we would like to avoid it
       if possible.  Note that any skew or rotation matrix will make it
       neccessary to perform blending */
    inst.has_overlap =
        (inst.templat.XStep < inst.templat.BBox.q.x - inst.templat.BBox.p.x ||
         inst.templat.YStep < inst.templat.BBox.q.y - inst.templat.BBox.p.y ||
         ctm_only(saved).xy != 0 ||
         ctm_only(saved).yx != 0 );

#define mat inst.step_matrix
    if_debug6m('t', mem, "[t]step_matrix=[%g %g %g %g %g %g]\n",
               inst.step_matrix.xx, inst.step_matrix.xy, inst.step_matrix.yx,
               inst.step_matrix.yy, inst.step_matrix.tx, inst.step_matrix.ty);
    if_debug5m('t', mem, "[t]bbox=(%g,%g),(%g,%g), uses_transparency=%d\n",
               bbox.p.x, bbox.p.y, bbox.q.x, bbox.q.y, inst.templat.uses_transparency);

    /* If the step and the size agree to within 1/2 pixel, */
    /* make them the same. */
    if (ADJUST_SCALE_BY_GS_TRADITION) {
        inst.size.x = (int)(bbw + 0.8);             /* 0.8 is arbitrary */
        inst.size.y = (int)(bbh + 0.8);
    } else if (inst.templat.TilingType == 2) {
        /* Always round up for TilingType 2, as we don't want any
         * content to be lost. */
        inst.size.x = sane_ceil(bbw);
        inst.size.y = sane_ceil(bbh);
    } else {
        /* For TilingType's other than 2 allow us to round up or down
         * to whatever is nearer. The scale we do later prevents us
         * losing content. */
        inst.size.x = (int)floor(bbw+0.5);
        inst.size.y = (int)floor(bbh+0.5);
    }

    /* Ensure we never round down to 0. Or below zero (bug 705768). */
    if (inst.size.x <= 0)
        inst.size.x = bbw > 0 ? 1 : 0;
    if (inst.size.y <= 0)
        inst.size.y = bbh > 0 ? 1 : 0;

    /* After compute_inst_matrix above, we are guaranteed that
     * inst.step_matrix.xx > 0 and inst.step_matrix.yy > 0.
     * Similarly, we are guaranteed that inst.size.x >= 0 and
     * inst.size.y >= 0. */
    if (inst.size.x == 0 || inst.size.y == 0) {
        /* The pattern is empty: the stepping matrix doesn't matter. */
        gs_make_identity(&inst.step_matrix);
        bbox.p.x = bbox.p.y = bbox.q.x = bbox.q.y = 0;
    } else if (fabs(inst.step_matrix.xx * inst.step_matrix.yy -
                    inst.step_matrix.xy * inst.step_matrix.yx) < 1.0e-9) {
        /* Singular stepping matrix. */
        code = gs_note_error(gs_error_rangecheck);
        goto fsaved;
    } else if (ADJUST_SCALE_BY_GS_TRADITION &&
               inst.step_matrix.xy == 0 && inst.step_matrix.yx == 0 &&
               fabs(inst.step_matrix.xx - bbw) < 0.5 &&
               fabs(inst.step_matrix.yy - bbh) < 0.5) {
        gs_scale(saved, inst.size.x / inst.step_matrix.xx,
                        inst.size.y / inst.step_matrix.yy);
        if (ADJUST_SCALE_FOR_THIN_LINES) {
            /* To allow thin lines at a cell boundary to be painted
             * inside the cell, we adjust the scale so that the scaled
             * width is in fixed_1 smaller. */
            gs_scale(saved, (inst.size.x - 1.0 / fixed_scale) / inst.size.x,
                            (inst.size.y - 1.0 / fixed_scale) / inst.size.y);
        }
        code = compute_inst_matrix(&inst, &bbox,
                                   dev_width, dev_height, &bbw, &bbh);
        if (code < 0)
            goto fsaved;
        if_debug2m('t', mem,
                   "[t]adjusted XStep & YStep to size=(%d,%d)\n",
                   inst.size.x, inst.size.y);
        if_debug4m('t', mem, "[t]bbox=(%g,%g),(%g,%g)\n",
                   bbox.p.x, bbox.p.y, bbox.q.x, bbox.q.y);
    } else if ((ADJUST_AS_ADOBE) && (inst.templat.TilingType != 2)) {
        if (inst.step_matrix.xy == 0 && inst.step_matrix.yx == 0 &&
            fabs(inst.step_matrix.xx - bbw) < 0.5 &&
            fabs(inst.step_matrix.yy - bbh) < 0.5) {
            if (inst.step_matrix.xx <= 2) {
                /* Prevent a degradation - see -r72 mspro.pdf */
                gs_scale(saved, fabs(inst.size.x / inst.step_matrix.xx), 1);
                inst.step_matrix.xx = (float)inst.size.x;
            } else {
                float cx, ox, dx;
                /* We adjust the step matrix to an integer (as we
                 * can't quickly tile non-integer tiles). We bend
                 * the contents of the tile slightly so that they
                 * completely fill the tile (rather than potentially
                 * leaving gaps around the edge).
                 * To allow thin lines at a cell boundary to be painted
                 * inside the cell, we adjust the scale so that the
                 * scaled width is fixed_1 smaller. */
                gs_scale(saved,
                         (inst.size.x - 1.0 / fixed_scale) / inst.step_matrix.xx,
                         1);
                /* We want the point in the centre of the displayed
                   region of this pattern not to move. We don't know
                   where the displayed region of the pattern is, so
                   we take the centre of the pattern bbox as a guess.
                   We call this (cx,cy). Let's suppose that this point
                   is the image of (ox,oy) under transformation.

                              (a      0      0)
                              (0      d      0)
                              (x      y      1)
                     (ox oy 1)(ox.a+x oy.d+y 1)

                   Thus cx = ox.a + x, cy = oy.d + y
                   So ox = (cx - x)/a, oy = (cy - y)/d

                   We want to adjust the matrix to use A and D instead
                   of a and d, and also adjust x and y so that the image
                   of (ox,oy) is the same.

                     i.e.     (A      0      0)
                              (0      D      0)
                              (X      Y      1)
                     (ox oy 1)(ox.A+X oy.D+Y 1)

                     i.e. cx = ox.A+X, cy = oy.D+Y
                     So X   = cx - ox.A
                        x   = cx - ox.a
                        x-X = -ox.a + ox.A
                            = ox.(A-a)

                   BUT we've been at pains already to make sure that the
                   origin of the 0th tile falls on pixel boundaries. So
                   clamp our correction to whole pixels.
                */
                cx = (bbox.p.x + bbox.q.x)/2;
                ox = (cx - inst.step_matrix.tx) / inst.step_matrix.xx;
                dx = ox * (inst.size.x - inst.step_matrix.xx);
                dx = floor(dx+0.5); /* Whole pixels! */
                inst.step_matrix.xx = (float)inst.size.x;
                inst.saved->ctm.tx -= dx;
            }
            if (inst.step_matrix.yy <= 2) {
                gs_scale(saved, 1, inst.size.y / inst.step_matrix.yy);
                inst.step_matrix.yy = (float)inst.size.y;
            } else {
                float cy, oy, dy;
                /* See above comment for explanation */
                gs_scale(saved,
                         1,
                         (inst.size.y - 1.0 / fixed_scale) / inst.step_matrix.yy);
                cy = (bbox.p.y + bbox.q.y)/2;
                oy = (cy - inst.step_matrix.ty) / inst.step_matrix.yy;
                dy = oy * (inst.size.y - inst.step_matrix.yy);
                dy = floor(dy+0.5); /* Whole pixels! */
                inst.step_matrix.yy = (float)inst.size.y;
                inst.saved->ctm.ty -= dy;
            }
            code = fix_bbox_after_matrix_adjustment(&inst, &bbox);
            if (code < 0)
                goto fsaved;
        }
    } else if ((inst.templat.TilingType == 2) &&
               ((pgs->fill_adjust.x | pgs->fill_adjust.y) == 0)) {
        /* RJW: This codes with non-rotated cases (with or without a
         * skew), but won't cope with rotated ones. Find an example. */
        float shiftx = ((inst.step_matrix.yx == 0 &&
                         fabs(inst.step_matrix.xx - bbw) <= 0.5) ?
                        (bbw - inst.size.x)/2 : 0);
        float shifty = ((inst.step_matrix.xy == 0 &&
                         fabs(inst.step_matrix.yy - bbh) <= 0.5) ?
                        (bbh - inst.size.y)/2 : 0);
        gs_translate_untransformed(saved, shiftx, shifty);
        code = fix_bbox_after_matrix_adjustment(&inst, &bbox);
        if (code < 0)
            goto fsaved;
    }
    if ((code = gs_bbox_transform_inverse(&bbox, &inst.step_matrix, &inst.bbox)) < 0)
        goto fsaved;
    if_debug4m('t', mem, "[t]ibbox=(%g,%g),(%g,%g)\n",
               inst.bbox.p.x, inst.bbox.p.y, inst.bbox.q.x, inst.bbox.q.y);
    inst.is_simple = (inst.step_matrix.xx == inst.size.x && inst.step_matrix.xy == 0 &&
                      inst.step_matrix.yx == 0 && inst.step_matrix.yy == inst.size.y);
    if_debug6m('t', mem,
               "[t]is_simple? xstep=(%g,%g) ystep=(%g,%g) size=(%d,%d)\n",
               inst.step_matrix.xx, inst.step_matrix.xy,
               inst.step_matrix.yx, inst.step_matrix.yy,
               inst.size.x, inst.size.y);
    /* Absent other information, instances always require a mask. */
    inst.uses_mask = true;
    inst.is_clist = false;      /* automatically set clist (don't force use) */
    gx_translate_to_fixed(saved, float2fixed_rounded(inst.step_matrix.tx - bbox.p.x),
                                 float2fixed_rounded(inst.step_matrix.ty - bbox.p.y));
    inst.step_matrix.tx = bbox.p.x;
    inst.step_matrix.ty = bbox.p.y;
#undef mat
    cbox.p.x = fixed_0;
    cbox.p.y = fixed_0;
    cbox.q.x = int2fixed(inst.size.x);
    cbox.q.y = int2fixed(inst.size.y);
    code = gx_clip_to_rectangle(saved, &cbox);
    if (code < 0)
        goto fsaved;
    if (!inst.is_simple) {
        code = gs_newpath(saved);
        if (code >= 0)
            code = gs_moveto(saved, inst.templat.BBox.p.x, inst.templat.BBox.p.y);
        if (code >= 0)
            code = gs_lineto(saved, inst.templat.BBox.q.x, inst.templat.BBox.p.y);
        if (code >= 0)
            code = gs_lineto(saved, inst.templat.BBox.q.x, inst.templat.BBox.q.y);
        if (code >= 0)
            code = gs_lineto(saved, inst.templat.BBox.p.x, inst.templat.BBox.q.y);
        if (code >= 0)
            code = gs_clip(saved);
        if (code < 0)
            goto fsaved;
    }
    code = gs_newpath(saved);
    if (code < 0)
        goto fsaved;
    inst.id = gs_next_ids(mem, 1);
    *pinst = inst;
    return 0;
  fsaved:gs_gstate_free(saved);
    gs_free_object(mem, pinst, "gs_makepattern");
    pcc->pattern = NULL; /* We've just freed the memory this points to */
    return code;
}

/*
 * Clamp the bound box for a pattern to the region of the pattern that will
 * actually be on our page.  We need to do this becuase some applications
 * create patterns which specify a bounding box which is much larger than
 * the page.  We allocate a buffer for holding the pattern.  We need to
 * prevent this buffer from getting too large.
 */
static int
clamp_pattern_bbox(gs_pattern1_instance_t * pinst, gs_rect * pbbox,
                    int width, int height, const gs_matrix * pmat)
{
    double xstep = pinst->templat.XStep;
    double ystep = pinst->templat.YStep;
    double xmin = pbbox->q.x;
    double xmax = pbbox->p.x;
    double ymin = pbbox->q.y;
    double ymax = pbbox->p.y;
    int ixpat, iypat, iystart;
    double xpat, ypat;
    double xlower, xupper, ylower, yupper;
    double xdev, ydev;
    gs_rect dev_page, pat_page;
    gs_point dev_pat_origin, dev_step;
    int code;

    double xepsilon = FLT_EPSILON * width;
    double yepsilon = FLT_EPSILON * height;

    /*
     * Scan across the page.  We determine the region to be scanned
     * by working in the pattern coordinate space.  This is logically
     * simpler since XStep and YStep are on axis in the pattern space.
     */
    /* But, since we are starting below bottom left, and 'incrementing' by
     * xstep and ystep, make sure they are not negative, or we will be in
     * a very long loop indeed.
     */
    if (xstep < 0)
        xstep *= -1;
    if (ystep < 0)
        ystep *= -1;
    /*
     * Convert the page dimensions from device coordinates into the
     * pattern coordinate frame.
     */
    dev_page.p.x = dev_page.p.y = 0;
    dev_page.q.x = width;
    dev_page.q.y = height;
    code = gs_bbox_transform_inverse(&dev_page, pmat, &pat_page);
    if (code < 0)
        return code;
    /* So pat_page is the bbox for the region in pattern space that will
     * be mapped forwards to cover the page. We want to find which tiles
     * are required to cover this area. */
    /*
     * Determine the location of the pattern origin in device coordinates.
     */
    gs_point_transform(0.0, 0.0, pmat, &dev_pat_origin);
    /*
     * Determine our starting point.  We start with a postion that puts the
     * pattern below and to the left of the page (in pattern space) and scan
     * until the pattern is above and right of the page.
     *
     * So the right hand edge of each tile is:
     *
     *  xstep * n + pinst->templat.BBox.q.x
     *
     * and we want the largest n s.t. that is <= pat_page.p.x. i.e.
     *
     *  xstep * n <= pat_page.p.x - pinst->templat.BBox.q.x < xstep *n+1
     *  n <= (pat_page.p.x - pinst->templat.BBox.q.x) / xstep < n+1
     */
    ixpat = (int) floor((pat_page.p.x - pinst->templat.BBox.q.x) / xstep);
    iystart = (int) floor((pat_page.p.y - pinst->templat.BBox.q.y) / ystep);

    /* Now do the scan */
    for (; ; ixpat++) {
        xpat = ixpat * xstep;
        for (iypat = iystart; ; iypat++) {
            ypat = iypat * ystep;
            /*
             * Calculate the shift in the pattern's location.
             */
            gs_point_transform(xpat, ypat, pmat, &dev_step);
            xdev = dev_step.x - dev_pat_origin.x;
            ydev = dev_step.y - dev_pat_origin.y;
            /*
             * Check if the pattern bounding box intersects the page.
             */
            xlower = (xdev + pbbox->p.x > 0) ? pbbox->p.x : -xdev;
            xupper = (xdev + pbbox->q.x < width) ? pbbox->q.x : -xdev + width;
            ylower = (ydev + pbbox->p.y > 0) ? pbbox->p.y : -ydev;
            yupper = (ydev + pbbox->q.y < height) ? pbbox->q.y : -ydev + height;

            /* The use of floating point in these calculations causes us
             * problems. Values which go through the calculation without ever
             * being 'large' retain more accuracy in the lower bits than ones
             * which momentarily become large. This is seen in bug 694528
             * where a y value of 0.00017... becomes either 0 when 8000 is
             * first added to it, then subtracted. This can lead to yupper
             * and ylower being different.
             *
             * The "fix" implemented here is to amend the following test to
             * ensure that the region found is larger that 'epsilon'. The
             * epsilon values are calculated to reflect the floating point
             * innacuracies at the appropriate range.
             */
            if (xlower + xepsilon < xupper && ylower + yepsilon < yupper) {
                /*
                 * The pattern intersects the page.  Expand required area if
                 * needed.
                 */
                if (xlower < xmin)
                    xmin = xlower;
                if (xupper > xmax)
                    xmax = xupper;
                if (ylower < ymin)
                    ymin = ylower;
                if (yupper > ymax)
                    ymax = yupper;
            }
            if (ypat > pat_page.q.y - pinst->templat.BBox.p.y)
                break;
        }
        if (xpat > pat_page.q.x - pinst->templat.BBox.p.x)
            break;
    }
    /* Update the bounding box. */
    if (xmin < xmax && ymin < ymax) {
        pbbox->p.x = xmin;
        pbbox->q.x = xmax;
        pbbox->p.y = ymin;
        pbbox->q.y = ymax;
    } else {
        /* The pattern is never on the page.  Set bbox = 1, 1 */
        pbbox->p.x = pbbox->p.y = 0;
        pbbox->q.x = pbbox->q.y = 1;
    }
    return 0;
}

static int
adjust_bbox_to_pixel_origin(gs_pattern1_instance_t *pinst, gs_rect *pbbox)
{
    gs_gstate * saved = pinst->saved;
    float dx, dy;
    int code = 0;

    /*
     * Adjust saved.ctm to map the bbox origin to pixels.
     */
    dx = pbbox->p.x - floor(pbbox->p.x + 0.5);
    dy = pbbox->p.y - floor(pbbox->p.y + 0.5);
    if (dx != 0 || dy != 0) {
        pbbox->p.x -= dx;
        pbbox->p.y -= dy;
        pbbox->q.x -= dx;
        pbbox->q.y -= dy;

        if (saved->ctm.txy_fixed_valid) {
            code = gx_translate_to_fixed(saved, float2fixed_rounded(saved->ctm.tx - dx),
                                                float2fixed_rounded(saved->ctm.ty - dy));
        } else {         /* the ctm didn't fit in a fixed. Just adjust the float values */
            saved->ctm.tx -= dx;
            saved->ctm.ty -= dy;
            /* not sure if this is needed for patterns, but lifted from gx_translate_to_fixed */
            code = gx_path_translate(saved->path, float2fixed(-dx), float2fixed(-dy));
        }
    }
    pinst->step_matrix.tx = saved->ctm.tx;
    pinst->step_matrix.ty = saved->ctm.ty;

    return code;
}

/* Compute the stepping matrix and device space instance bounding box */
/* from the step values and the saved matrix. */
static int
compute_inst_matrix(gs_pattern1_instance_t * pinst,
                    gs_rect * pbbox, int width, int height,
                    float *pbbw, float *pbbh)
{
    float xx, xy, yx, yy, temp;
    int code;
    gs_gstate * saved = pinst->saved;
    gs_matrix m = ctm_only(saved);

    /* Bug 702124: Due to the limited precision of floats, we find that
     * transforming (say) small height boxes in the presence of large tx/ty
     * values can cause the box heights to map to 0. So calculate the
     * width/height of the bbox before we roll the offset into it. */
    m.tx = 0; m.ty = 0;

    code = gs_bbox_transform(&pinst->templat.BBox, &m, pbbox);
    if (code < 0)
        return code;

    sane_clamp_rect(pbbox);

    *pbbw = pbbox->q.x - pbbox->p.x;
    *pbbh = pbbox->q.y - pbbox->p.y;

    pbbox->p.x += ctm_only(saved).tx;
    pbbox->p.y += ctm_only(saved).ty;
    pbbox->q.x += ctm_only(saved).tx;
    pbbox->q.y += ctm_only(saved).ty;

    code = adjust_bbox_to_pixel_origin(pinst, pbbox);
    if (code < 0)
        return code;

    /* The stepping matrix : */
    /* We do not want to overflow the maths here. Since xx etc are all floats
     * then the multiplication will definitely fit into a double, and we can
     * check to ensure that the result still fits into a float without
     * overflowing at any point.
     */
    {
        double double_mult = 0.0;

        double_mult = (double)pinst->templat.XStep * (double)saved->ctm.xx;
        if (double_mult < -MAX_FLOAT || double_mult > MAX_FLOAT)
            return_error(gs_error_rangecheck);
        xx = (float)double_mult;

        double_mult = (double)pinst->templat.XStep * (double)saved->ctm.xy;
        if (double_mult < -MAX_FLOAT || double_mult > MAX_FLOAT)
            return_error(gs_error_rangecheck);
        xy = double_mult;

        double_mult = (double)pinst->templat.YStep * (double)saved->ctm.yx;
        if (double_mult < -MAX_FLOAT || double_mult > MAX_FLOAT)
            return_error(gs_error_rangecheck);
        yx = double_mult;

        double_mult = (double)pinst->templat.YStep * (double)saved->ctm.yy;
        if (double_mult < -MAX_FLOAT || double_mult > MAX_FLOAT)
            return_error(gs_error_rangecheck);
        yy = double_mult;
    }

    /* Adjust the stepping matrix so all coefficients are >= 0. */
    if (xx == 0 || yy == 0) { /* We know that both xy and yx are non-zero. */
        temp = xx, xx = yx, yx = temp;
        temp = xy, xy = yy, yy = temp;
    }
    if (xx < 0)
        xx = -xx, xy = -xy;
    if (yy < 0)
        yx = -yx, yy = -yy;
    /* Now xx > 0, yy > 0. */
    pinst->step_matrix.xx = xx;
    pinst->step_matrix.xy = xy;
    pinst->step_matrix.yx = yx;
    pinst->step_matrix.yy = yy;

    sane_clamp_matrix(&pinst->step_matrix);

    /*
     * Some applications produce patterns that are larger than the page.
     * If the bounding box for the pattern is larger than the page. clamp
     * the pattern to the page size.
     */
    if ((pbbox->q.x - pbbox->p.x > width || pbbox->q.y - pbbox->p.y > height))
        code = clamp_pattern_bbox(pinst, pbbox, width,
                                        height, &ctm_only(saved));

    return code;
}

static int
fix_bbox_after_matrix_adjustment(gs_pattern1_instance_t *pinst, gs_rect *pbbox)
{
    int code;
    gs_gstate * saved = pinst->saved;

    code = gs_bbox_transform(&pinst->templat.BBox, &ctm_only(saved), pbbox);
    if (code < 0)
        return code;

    code = adjust_bbox_to_pixel_origin(pinst, pbbox);
    if (code < 0)
        return code;

    return code;
}

/* Test whether a PatternType 1 pattern uses a base space. */
static bool
gs_pattern1_uses_base_space(const gs_pattern_template_t *ptemp)
{
    return ((const gs_pattern1_template_t *)ptemp)->PaintType == 2;
}

/* getpattern for PatternType 1 */
/* This is only intended for the benefit of pattern PaintProcs. */
static const gs_pattern_template_t *
gs_pattern1_get_pattern(const gs_pattern_instance_t *pinst)
{
    return (const gs_pattern_template_t *)
        &((const gs_pattern1_instance_t *)pinst)->templat;
}

/* Get transparency object pointer */
void *
gx_pattern1_get_transptr(const gx_device_color *pdevc)
{
    if (pdevc->colors.pattern.p_tile != NULL)
        return pdevc->colors.pattern.p_tile->ttrans;
    else
        return NULL;
}

/* Check for if the clist in the pattern has transparency */
int
gx_pattern1_clist_has_trans(const gx_device_color *pdevc)
{
    if (pdevc->colors.pattern.p_tile != NULL &&
        pdevc->colors.pattern.p_tile->cdev != NULL) {
        return pdevc->colors.pattern.p_tile->cdev->common.page_uses_transparency;
    } else {
        return 0;
    }
}

/* Check device color for Pattern Type 1. */
bool
gx_dc_is_pattern1_color_clist_based(const gx_device_color *pdevc)
{
    if (!(gx_dc_is_pattern1_color(pdevc)))
        return false;
    return gx_pattern_tile_is_clist(pdevc->colors.pattern.p_tile);
}

/* Get pattern id (type 1 pattern only) */
gs_id
gs_dc_get_pattern_id(const gx_device_color *pdevc)
{
    if (!(gx_dc_is_pattern1_color(pdevc)))
        return gs_no_id;
    if (pdevc->colors.pattern.p_tile == NULL)
        return gs_no_id;
    return pdevc->colors.pattern.p_tile->id;
}

/*
 * Perform actions required at setcolor time. This procedure resets the
 * overprint information (almost) as required by the pattern. The logic
 * behind this operation is a bit convoluted:
 *
 *  1. Both PatternType 1 and 2 "colors" occur within the pattern color
 *     space.
 *
 *  2. Nominally, the set of drawn components is a property of the color
 *     space, and is set at the time setcolorspace is called. This is
 *     not the case for patterns, so overprint information must be set
 *     at setcolor time for them.
 *
 *  3. PatternType 2 color spaces incorporate their own color space, so
 *     the set of drawn components is determined by that color space.
 *     For PatternType 1 color spaces, the PaintType determines the
 *     appropriate color space to use. If PaintType is 2 (uncolored),
 *     the pattern makes use of the base color space of the current
 *     pattern color space, so overprint is set as appropriate for
 *     that color space.
 *
 *  4. For PatternType 1 color spaces with PaintType 1 (colored), the
 *     appropriate color space to use is determined by the pattern's
 *     PaintProc. This cannot be handled by the current graphic
 *     library mechanism, because color space information is lost when
 *     the pattern tile is cached (and the pattern tile is essentially
 *     always cached). We punt in this case and list all components
 *     as drawn components. (This feature could be support by retaining
 *     per-component pattern masks, but complete re-design of the
 *     pattern mechanism is probably more appropriate.)
 *
 *  5. Once overprint information has been set for a particular color,
 *     it must be reset to the proper value when that color is no
 *     longer in use. "Normal" (non-pattern) colors do not have a
 *     "set_color" action, both for performance and logical reasons.
 *     This does not, however, cause significant difficulty, as the
 *     change in color space required to set a normal color will
 *     reset the overprint information as required.
 */
static int
gs_pattern1_set_color(const gs_client_color * pcc, gs_gstate * pgs)
{
    gs_pattern1_instance_t * pinst = (gs_pattern1_instance_t *)pcc->pattern;
    gs_pattern1_template_t * ptmplt = &pinst->templat;

    if (ptmplt->PaintType == 2) {
        const gs_color_space *pcs = gs_currentcolorspace_inline(pgs);

        pcs = pcs->base_space;
        return pcs->type->set_overprint(pcs, pgs);
    } else {
        gs_overprint_params_t   params = {0};

        params.retain_any_comps = false;
        params.effective_opm = pgs->color[0].effective_opm = 0;
        params.op_state = OP_STATE_NONE;
        params.is_fill_color = pgs->is_fill_color;
        params.idle = false;

        return gs_gstate_update_overprint(pgs, &params);
    }
}

const gs_pattern1_template_t *
gs_getpattern(const gs_client_color * pcc)
{
    const gs_pattern_instance_t *pinst = pcc->pattern;

    return (pinst == 0 || pinst->type != &gs_pattern1_type ? 0 :
            &((const gs_pattern1_instance_t *)pinst)->templat);
}

/*
 *  Code for generating patterns from bitmaps and pixmaps.
 */

/*
 *  The following structures are realized here only because this is the
 *  first location in which they were needed. Otherwise, there is nothing
 *  about them that is specific to patterns.
 */
public_st_gs_bitmap();
public_st_gs_tile_bitmap();
public_st_gs_depth_bitmap();
public_st_gs_tile_depth_bitmap();
public_st_gx_strip_bitmap();

/*
 *  Structure for holding a gs_depth_bitmap and the corresponding depth and
 *  colorspace information.
 *
 *  The free_proc pointer is needed to hold the original value of the pattern
 *  instance free structure. This pointer in the pattern instance will be
 *  overwritten with free_pixmap_pattern, which will free the pixmap info
 *  structure when it is freed.
 */
typedef struct pixmap_info_s {
    gs_depth_bitmap bitmap;     /* must be first */
    gs_color_space *pcspace;
    uint white_index;
    void (*free_proc)(gs_memory_t *, void *, client_name_t);
} pixmap_info;

void *
gs_get_pattern_client_data(const gs_client_color * pcc)
{
    const gs_pattern_instance_t *pinst = pcc->pattern;

    return (pinst == 0 || pinst->type != &gs_pattern1_type ? 0 :
            (void *)pinst->client_data);
}

gs_private_st_suffix_add1(st_pixmap_info,
                          pixmap_info,
                          "pixmap info. struct",
                          pixmap_enum_ptr,
                          pixmap_reloc_ptr,
                          st_gs_depth_bitmap,
                          pcspace
);

#define st_pixmap_info_max_ptrs (1 + st_tile_bitmap_max_ptrs)

/*
 *  Free routine for pattern instances created from pixmaps.
 *
 *  Note that this routine does NOT release the data in the original pixmap;
 *  that remains the responsibility of the client.
 */
static void pixmap_free_notify (gs_memory_t * mem, void *vpinst)
{
    gs_pattern1_instance_t *pinst = (gs_pattern1_instance_t *)vpinst;

    gs_free_object(mem, pinst->client_data, "pixmap_free_notify");
}

/*
 *  PaintProcs for bitmap and pixmap patterns.
 */
static int bitmap_paint(gs_image_enum * pen, gs_data_image_t * pim,
                         const gs_depth_bitmap * pbitmap, gs_gstate * pgs);
static int
mask_PaintProc(const gs_client_color * pcolor, gs_gstate * pgs)
{
    int code;
    const pixmap_info *ppmap = (pixmap_info *)gs_get_pattern_client_data(pcolor);
    const gs_depth_bitmap *pbitmap = &(ppmap->bitmap);
    gs_image_enum *pen = gs_image_enum_alloc(gs_gstate_memory(pgs), "mask_PaintProc");
    gs_image1_t mask;

    if (pen == 0)
        return_error(gs_error_VMerror);
    gs_image_t_init_mask(&mask, true);
    mask.Width = pbitmap->size.x;
    mask.Height = pbitmap->size.y;
    code = gs_image_init(pen, &mask, false, false, pgs);
    if (code >= 0)
        code = bitmap_paint(pen, (gs_data_image_t *) & mask, pbitmap, pgs);
    gs_free_object(gs_gstate_memory(pgs), pen, "mask_PaintProc");
    return code;
}
static int
image_PaintProc(const gs_client_color * pcolor, gs_gstate * pgs)
{
    const pixmap_info *ppmap = gs_get_pattern_client_data(pcolor);
    const gs_depth_bitmap *pbitmap = &(ppmap->bitmap);
    gs_image_enum *pen =
        gs_image_enum_alloc(gs_gstate_memory(pgs), "image_PaintProc");
    gs_color_space *pcspace;
    gx_image_enum_common_t *pie;
    /*
     * If the image is transparent then we want to do image type4 processing.
     * Otherwise we want to use image type 1 processing.
     */
    int transparent = ppmap->white_index < (1 << (pbitmap->num_comps * pbitmap->pix_depth));

    /*
     * Note: gs_image1_t and gs_image4_t sre nearly identical structure
     * definitions.  From our point of view, the only significant difference
     * is MaskColor in gs_image4_t.  The fields are generally loaded using
     * the gs_image1_t version of the union and then used for either type
     * of image processing.
     */
    union {
        gs_image1_t i1;
        gs_image4_t i4;
    } image;
    int code;

    if (pen == 0)
        return_error(gs_error_VMerror);

    if (ppmap->pcspace == 0) {
        pcspace = gs_cspace_new_DeviceGray(pgs->memory);
        if (pcspace == NULL)
            return_error(gs_error_VMerror);
    } else
        pcspace = ppmap->pcspace;
    code = gs_gsave(pgs);
    if (code < 0)
        goto fail;
    code = gs_setcolorspace(pgs, pcspace);
    if (code < 0) {
        gs_grestore(pgs);
        goto fail;
    }
    if (transparent)
        gs_image4_t_init( (gs_image4_t *) &image, pcspace);
    else
        gs_image_t_init_adjust( (gs_image_t *) &image, pcspace, 0);
    image.i1.Width = pbitmap->size.x;
    image.i1.Height = pbitmap->size.y;
    if (transparent) {
        image.i4.MaskColor_is_range = false;
        image.i4.MaskColor[0] = ppmap->white_index;
    }
    image.i1.Decode[0] = 0.0;
    image.i1.Decode[1] = (float)((1 << pbitmap->pix_depth) - 1);
    image.i1.BitsPerComponent = pbitmap->pix_depth;
    /* backwards compatibility */
    if (ppmap->pcspace == 0) {
        image.i1.Decode[0] = 1.0;
        image.i1.Decode[1] = 0.0;
    }

    if ( (code = gs_image_begin_typed( (const gs_image_common_t *)&image,
                                       pgs,
                                       false,
                                       false,
                                       &pie )) >= 0 &&
         (code = gs_image_enum_init( pen,
                                     pie,
                                     (gs_data_image_t *)&image,
                                     pgs )) >= 0 &&
        (code = bitmap_paint(pen, (gs_data_image_t *) & image, pbitmap, pgs)) >= 0) {
        gs_free_object(gs_gstate_memory(pgs), pen, "image_PaintProc");
        return gs_grestore(pgs);
    }
    /* Failed above, need to undo the gsave */
    gs_grestore(pgs);

fail:
    gs_free_object(gs_gstate_memory(pgs), pen, "image_PaintProc");
    return code;
}
/* Finish painting any kind of bitmap pattern. */
static int
bitmap_paint(gs_image_enum * pen, gs_data_image_t * pim,
             const gs_depth_bitmap * pbitmap, gs_gstate * pgs)
{
    uint raster = pbitmap->raster;
    uint nbytes = (pim->Width * pbitmap->pix_depth + 7) >> 3;
    uint used;
    const byte *dp = pbitmap->data;
    int n;
    int code = 0, code1;

    if (nbytes == raster)
        code = gs_image_next(pen, dp, nbytes * pim->Height, &used);
    else
        for (n = pim->Height; n > 0 && code >= 0; dp += raster, --n)
            code = gs_image_next(pen, dp, nbytes, &used);
    code1 = gs_image_cleanup(pen, pgs);
    if (code >= 0 && code1 < 0)
        code = code1;
    return code;
}

int pixmap_high_level_pattern(gs_gstate * pgs)
{
    gs_matrix m;
    gs_rect bbox;
    gs_fixed_rect clip_box;
    int code;
    gx_device_color *pdc = gs_currentdevicecolor_inline(pgs);
    const gs_client_pattern *ppat = gs_getpattern(&pdc->ccolor);
    gs_color_space *pcs;
    gs_pattern1_instance_t *pinst =
        (gs_pattern1_instance_t *)gs_currentcolor(pgs)->pattern;
    const pixmap_info *ppmap = (const pixmap_info *)gs_get_pattern_client_data((const gs_client_color *)&pdc->ccolor);

    code = gx_pattern_cache_add_dummy_entry(pgs, pinst, pgs->device->color_info.depth);
    if (code < 0)
        return code;

    code = gs_gsave(pgs);
    if (code < 0)
        return code;

    dev_proc(pgs->device, get_initial_matrix)(pgs->device, &m);
    gs_setmatrix(pgs, &m);
    code = gs_bbox_transform(&ppat->BBox, &ctm_only(pgs), &bbox);
    if (code < 0) {
        gs_grestore(pgs);
            return code;
    }
    clip_box.p.x = float2fixed(bbox.p.x);
    clip_box.p.y = float2fixed(bbox.p.y);
    clip_box.q.x = float2fixed(bbox.q.x);
    clip_box.q.y = float2fixed(bbox.q.y);
    code = gx_clip_to_rectangle(pgs, &clip_box);
    if (code < 0) {
        gs_grestore(pgs);
        return code;
    }

    {
        pattern_accum_param_s param;
        param.pinst = (void *)pinst;
        param.graphics_state = (void *)pgs;
        param.pinst_id = pinst->id;

        code = dev_proc(pgs->device, dev_spec_op)(pgs->device,
                                gxdso_pattern_start_accum, &param, sizeof(pattern_accum_param_s));
    }

    if (code < 0) {
        gs_grestore(pgs);
        return code;
    }

    if (ppmap->pcspace != 0)
        code = image_PaintProc(&pdc->ccolor, pgs);
    else {
        pcs = gs_cspace_new_DeviceGray(pgs->memory);
        if (pcs == NULL) {
            gs_grestore(pgs);
            return_error(gs_error_VMerror);
        }
        gs_setcolorspace(pgs, pcs);
        code = mask_PaintProc(&pdc->ccolor, pgs);
    }
    if (code < 0) {
        gs_grestore(pgs);
        return code;
    }

    code = gs_grestore(pgs);
    if (code < 0)
        return code;

    {
        pattern_accum_param_s param;
        param.pinst = (void *)pinst;
        param.graphics_state = (void *)pgs;
        param.pinst_id = pinst->id;

        code = dev_proc(pgs->device, dev_spec_op)(pgs->device,
                          gxdso_pattern_finish_accum, &param, sizeof(pattern_accum_param_s));
    }

    return code;
}

static int pixmap_remap_mask_pattern(const gs_client_color *pcc, gs_gstate *pgs)
{
    const gs_client_pattern *ppat = gs_getpattern(pcc);
    int code = 0;

    /* pgs->device is the newly created pattern accumulator, but we want to test the device
     * that is 'behind' that, the actual output device, so we use the one from
     * the saved graphics state.
     */
    if (pgs->have_pattern_streams)
        code = dev_proc(pcc->pattern->saved->device, dev_spec_op)(pcc->pattern->saved->device,
                                gxdso_pattern_can_accum, (void *)ppat, ppat->uid.id);

    if (code == 1) {
        /* Device handles high-level patterns, so return 'remap'.
         * This closes the internal accumulator device, as we no longer need
         * it, and the error trickles back up to the PDL client. The client
         * must then take action to start the device's accumulator, draw the
         * pattern, close the device's accumulator and generate a cache entry.
         * See px_high_level_pattern above.
         */
        return_error(gs_error_Remap_Color);
    } else {
        mask_PaintProc(pcc, pgs);
        return 0;
    }
}

static int pixmap_remap_image_pattern(const gs_client_color *pcc, gs_gstate *pgs)
{
    const gs_client_pattern *ppat = gs_getpattern(pcc);
    int code = 0;

    /* pgs->device is the newly created pattern accumulator, but we want to test the device
     * that is 'behind' that, the actual output device, so we use the one from
     * the saved graphics state.
     */
    if (pgs->have_pattern_streams)
        code = dev_proc(pcc->pattern->saved->device, dev_spec_op)(pcc->pattern->saved->device,
                                gxdso_pattern_can_accum, (void *)ppat, ppat->uid.id);

    if (code == 1) {
        /* Device handles high-level patterns, so return 'remap'.
         * This closes the internal accumulator device, as we no longer need
         * it, and the error trickles back up to the PDL client. The client
         * must then take action to start the device's accumulator, draw the
         * pattern, close the device's accumulator and generate a cache entry.
         * See px_high_level_pattern above.
         */
        return_error(gs_error_Remap_Color);
    } else {
        return image_PaintProc(pcc, pgs);
    }
}

/*
 * Make a pattern from a bitmap or pixmap. The pattern may be colored or
 * uncolored, as determined by the mask operand. This code is intended
 * primarily for use by PCL.
 *
 * See the comment prior to the declaration of this function in gscolor2.h
 * for further information.
 */
int
gs_makepixmappattern(
                        gs_client_color * pcc,
                        const gs_depth_bitmap * pbitmap,
                        bool mask,
                        const gs_matrix * pmat,
                        long id,
                        gs_color_space * pcspace,
                        uint white_index,
                        gs_gstate * pgs,
                        gs_memory_t * mem
)
{

    gs_pattern1_template_t pat;
    pixmap_info *ppmap;
    gs_matrix mat, smat;
    int code;

    /* check that the data is legitimate */
    if ((mask) || (pcspace == 0)) {
        if (pbitmap->pix_depth != 1)
            return_error(gs_error_rangecheck);
        pcspace = 0;
    } else if (gs_color_space_get_index(pcspace) != gs_color_space_index_Indexed)
        return_error(gs_error_rangecheck);
    if (pbitmap->num_comps != 1)
        return_error(gs_error_rangecheck);

    /* allocate and initialize a pixmap_info structure for the paint proc */
    if (mem == 0)
        mem = gs_gstate_memory(pgs);
    ppmap = gs_alloc_struct(mem,
                            pixmap_info,
                            &st_pixmap_info,
                            "makepximappattern"
        );
    if (ppmap == 0)
        return_error(gs_error_VMerror);
    ppmap->bitmap = *pbitmap;
    ppmap->pcspace = pcspace;
    ppmap->white_index = white_index;

    /* set up the client pattern structure */
    gs_pattern1_init(&pat);
    uid_set_UniqueID(&pat.uid, (id == no_UniqueID) ? gs_next_ids(mem, 1) : id);
    pat.PaintType = (mask ? 2 : 1);
    pat.TilingType = 1;
    pat.BBox.p.x = 0;
    pat.BBox.p.y = 0;
    pat.BBox.q.x = pbitmap->size.x;
    pat.BBox.q.y = pbitmap->size.y;
    pat.XStep = (float)pbitmap->size.x;
    pat.YStep = (float)pbitmap->size.y;
    pat.PaintProc = (mask ? pixmap_remap_mask_pattern : pixmap_remap_image_pattern);

    /* set the ctm to be the identity */
    gs_currentmatrix(pgs, &smat);
    gs_make_identity(&mat);
    gs_setmatrix(pgs, &mat);

    /* build the pattern, restore the previous matrix */
    if (pmat == NULL)
        pmat = &mat;
    if ((code = gs_makepattern(pcc, &pat, pmat, pgs, mem)) != 0)
        gs_free_object(mem, ppmap, "makebitmappattern_xform");
    else {
        /*
         * If this is not a masked pattern and if the white pixel index
         * is outside of the representable range, we don't need to go to
         * the trouble of accumulating a mask that will just be all 1s.
         * Also, patterns that use transparency don't need a mask since
         * the alpha plane of the transparency buffers will be used.
         */
        gs_pattern1_instance_t *pinst =
            (gs_pattern1_instance_t *)pcc->pattern;

        if (!mask && (white_index >= (1 << pbitmap->pix_depth)))
            pinst->uses_mask = false;

        pinst->client_data = ppmap;
        pinst->notify_free = pixmap_free_notify;

        /*
         * Since the PaintProcs don't reference the saved color space or
         * color, clear these so that there isn't an extra retained
         * reference to the Pattern object.
         */
        code = gs_setgray(pinst->saved, 0.0);

    }
    gs_setmatrix(pgs, &smat);
    return code;
}

/*
 *  Backwards compatibility.
 */
int
gs_makebitmappattern_xform(
                              gs_client_color * pcc,
                              const gx_tile_bitmap * ptile,
                              bool mask,
                              const gs_matrix * pmat,
                              long id,
                              gs_gstate * pgs,
                              gs_memory_t * mem
)
{
    gs_depth_bitmap bitmap;

    /* build the bitmap the size of one repetition */
    bitmap.data = ptile->data;
    bitmap.raster = ptile->raster;
    bitmap.size.x = ptile->rep_width;
    bitmap.size.y = ptile->rep_height;
    bitmap.id = ptile->id;      /* shouldn't matter */
    bitmap.pix_depth = 1;
    bitmap.num_comps = 1;

    return gs_makepixmappattern(pcc, &bitmap, mask, pmat, id, 0, 0, pgs, mem);
}

/* ------ Color space implementation ------ */

/*
 * Defined the Pattern device color types.  We need a masked analogue of
 * each of the non-pattern types, to handle uncolored patterns.  We use
 * 'masked_fill_rect' instead of 'masked_fill_rectangle' in order to limit
 * identifier lengths to 32 characters.
 */
static dev_color_proc_get_dev_halftone(gx_dc_pattern_get_dev_halftone);
static dev_color_proc_load(gx_dc_pattern_load);
/*dev_color_proc_fill_rectangle(gx_dc_pattern_fill_rectangle); *//*gxp1fill.h */
static dev_color_proc_equal(gx_dc_pattern_equal);
static dev_color_proc_load(gx_dc_pure_masked_load);
static dev_color_proc_load(gx_dc_devn_masked_load);
static dev_color_proc_equal(gx_dc_devn_masked_equal);

static dev_color_proc_get_dev_halftone(gx_dc_pure_masked_get_dev_halftone);
/*dev_color_proc_fill_rectangle(gx_dc_pure_masked_fill_rect); *//*gxp1fill.h */
static dev_color_proc_equal(gx_dc_pure_masked_equal);
static dev_color_proc_load(gx_dc_binary_masked_load);

static dev_color_proc_get_dev_halftone(gx_dc_binary_masked_get_dev_halftone);
/*dev_color_proc_fill_rectangle(gx_dc_binary_masked_fill_rect); *//*gxp1fill.h */
static dev_color_proc_equal(gx_dc_binary_masked_equal);
static dev_color_proc_load(gx_dc_colored_masked_load);

static dev_color_proc_get_dev_halftone(gx_dc_colored_masked_get_dev_halftone);
/*dev_color_proc_fill_rectangle(gx_dc_colored_masked_fill_rect); *//*gxp1fill.h */
static dev_color_proc_equal(gx_dc_colored_masked_equal);

/* The device color types are exported for gxpcmap.c. */
gs_private_st_composite(st_dc_pattern, gx_device_color, "dc_pattern",
                        dc_pattern_enum_ptrs, dc_pattern_reloc_ptrs);
const gx_device_color_type_t gx_dc_pattern = {
    &st_dc_pattern,
    gx_dc_pattern_save_dc, gx_dc_pattern_get_dev_halftone,
    gx_dc_ht_get_phase,
    gx_dc_pattern_load, gx_dc_pattern_fill_rectangle,
    gx_dc_default_fill_masked, gx_dc_pattern_equal,
    gx_dc_pattern_write, gx_dc_pattern_read,
    gx_dc_pattern_get_nonzero_comps
};

const gx_device_color_type_t gx_dc_pattern_trans = {
    &st_dc_pattern,
    gx_dc_pattern_save_dc, gx_dc_pattern_get_dev_halftone,
    gx_dc_ht_get_phase,
    gx_dc_pattern_load, gx_dc_pat_trans_fill_rectangle,
    gx_dc_default_fill_masked, gx_dc_pattern_equal,
    gx_dc_pattern_write, gx_dc_pattern_read,
    gx_dc_pattern_get_nonzero_comps
};

extern_st(st_dc_ht_binary);
gs_private_st_composite(st_dc_pure_masked, gx_device_color, "dc_pure_masked",
                        dc_masked_enum_ptrs, dc_masked_reloc_ptrs);
const gx_device_color_type_t gx_dc_pure_masked = {
    &st_dc_pure_masked,
    gx_dc_pattern_save_dc, gx_dc_pure_masked_get_dev_halftone,
    gx_dc_no_get_phase,
    gx_dc_pure_masked_load, gx_dc_pure_masked_fill_rect,
    gx_dc_default_fill_masked, gx_dc_pure_masked_equal,
    gx_dc_cannot_write, gx_dc_cannot_read,
    gx_dc_pure_get_nonzero_comps
};

gs_private_st_composite(st_dc_binary_masked, gx_device_color,
                        "dc_binary_masked", dc_binary_masked_enum_ptrs,
                        dc_binary_masked_reloc_ptrs);
const gx_device_color_type_t gx_dc_binary_masked = {
    &st_dc_binary_masked,
    gx_dc_pattern_save_dc, gx_dc_binary_masked_get_dev_halftone,
    gx_dc_ht_get_phase,
    gx_dc_binary_masked_load, gx_dc_binary_masked_fill_rect,
    gx_dc_default_fill_masked, gx_dc_binary_masked_equal,
    gx_dc_cannot_write, gx_dc_cannot_read,
    gx_dc_ht_binary_get_nonzero_comps
};

gs_private_st_composite(st_dc_colored_masked, gx_device_color,
                        "dc_colored_masked",
                        dc_colored_masked_enum_ptrs, dc_colored_masked_reloc_ptrs);
const gx_device_color_type_t gx_dc_colored_masked = {
    &st_dc_colored_masked,
    gx_dc_pattern_save_dc, gx_dc_colored_masked_get_dev_halftone,
    gx_dc_ht_get_phase,
    gx_dc_colored_masked_load, gx_dc_colored_masked_fill_rect,
    gx_dc_default_fill_masked, gx_dc_colored_masked_equal,
    gx_dc_cannot_write, gx_dc_cannot_read,
    gx_dc_ht_colored_get_nonzero_comps
};

gs_private_st_composite(st_dc_devn_masked, gx_device_color,
                        "dc_devn_masked",
                        dc_devn_masked_enum_ptrs, dc_devn_masked_reloc_ptrs);
const gx_device_color_type_t gx_dc_devn_masked = {
    &st_dc_devn_masked,
    gx_dc_pattern_save_dc, gx_dc_pure_masked_get_dev_halftone,
    gx_dc_no_get_phase,
    gx_dc_devn_masked_load, gx_dc_devn_masked_fill_rect,
    gx_dc_devn_fill_masked, gx_dc_devn_masked_equal,
    gx_dc_cannot_write, gx_dc_cannot_read,
    gx_dc_devn_get_nonzero_comps
};

#undef gx_dc_type_pattern
const gx_device_color_type_t *const gx_dc_type_pattern = &gx_dc_pattern;
#define gx_dc_type_pattern (&gx_dc_pattern)

/* GC procedures */
static
ENUM_PTRS_WITH(dc_pattern_enum_ptrs, gx_device_color *cptr)
{
    return ENUM_USING(st_dc_pure_masked, vptr, size, index - 1);
}
case 0:
{
    gx_color_tile *tile = cptr->colors.pattern.p_tile;

    ENUM_RETURN((tile == 0 ? tile : tile - tile->index));
}
ENUM_PTRS_END
static RELOC_PTRS_WITH(dc_pattern_reloc_ptrs, gx_device_color *cptr)
{
    gx_color_tile *tile = cptr->colors.pattern.p_tile;

    if (tile != 0) {
        uint index = tile->index;

        RELOC_TYPED_OFFSET_PTR(gx_device_color, colors.pattern.p_tile, index);
    }
    RELOC_USING(st_dc_pure_masked, vptr, size);
}
RELOC_PTRS_END
static ENUM_PTRS_WITH(dc_masked_enum_ptrs, gx_device_color *cptr)
ENUM_SUPER(gx_device_color, st_client_color, ccolor, 1);
case 0:
{
    gx_color_tile *mask = cptr->mask.m_tile;

    ENUM_RETURN((mask == 0 ? mask : mask - mask->index));
}
ENUM_PTRS_END
static RELOC_PTRS_WITH(dc_masked_reloc_ptrs, gx_device_color *cptr)
{
    gx_color_tile *mask = cptr->mask.m_tile;

    RELOC_SUPER(gx_device_color, st_client_color, ccolor);
    if (mask != 0) {
        uint index = mask->index;

        RELOC_TYPED_OFFSET_PTR(gx_device_color, mask.m_tile, index);
    }
}
RELOC_PTRS_END
static ENUM_PTRS_WITH(dc_colored_masked_enum_ptrs, gx_device_color *cptr)
ENUM_SUPER(gx_device_color, st_client_color, ccolor, 1);
case 0:
{
    ENUM_RETURN(cptr->colors.colored.c_ht);
}
ENUM_PTRS_END
static RELOC_PTRS_WITH(dc_colored_masked_reloc_ptrs, gx_device_color *cptr)
{
    RELOC_SUPER(gx_device_color, st_client_color, ccolor);
    if (cptr->colors.colored.c_ht != 0) {
        RELOC_PTR(gx_device_color, colors.colored.c_ht);
    }
}
RELOC_PTRS_END
static ENUM_PTRS_WITH(dc_devn_masked_enum_ptrs, gx_device_color *cptr)
ENUM_SUPER(gx_device_color, st_client_color, ccolor, 0);
(void)cptr; /* Avoid unused warning */
ENUM_PTRS_END
static RELOC_PTRS_WITH(dc_devn_masked_reloc_ptrs, gx_device_color *cptr)
{
    RELOC_SUPER(gx_device_color, st_client_color, ccolor);
    (void)cptr; /* Avoid unused warning */
}
RELOC_PTRS_END
static ENUM_PTRS_BEGIN(dc_binary_masked_enum_ptrs)
{
    return ENUM_USING(st_dc_ht_binary, vptr, size, index - 2);
}
case 0:
case 1:
return ENUM_USING(st_dc_pure_masked, vptr, size, index);
ENUM_PTRS_END
static RELOC_PTRS_BEGIN(dc_binary_masked_reloc_ptrs)
{
    RELOC_USING(st_dc_pure_masked, vptr, size);
    RELOC_USING(st_dc_ht_binary, vptr, size);
}
RELOC_PTRS_END

/*
 * Currently patterns cannot be passed through the command list,
 * however vector devices need to save a color for comparing
 * it with another color, which appears later.
 * We provide a minimal support, which is necessary
 * for the current implementation of pdfwrite.
 * It is not sufficient for restoring the pattern from the saved color.
 */
void
gx_dc_pattern_save_dc(
    const gx_device_color * pdevc,
    gx_device_color_saved * psdc )
{
    psdc->type = pdevc->type;
    if (pdevc->ccolor_valid) {
        psdc->colors.pattern.id = pdevc->ccolor.pattern->pattern_id;
        psdc->phase = pdevc->phase;
    } else {
        /* The client color has been changed to a non-pattern color,
           but device color has not been created yet.
         */
        psdc->colors.pattern.id = gs_no_id;
        psdc->phase.x = psdc->phase.y = 0;
    }
}

/*
 * Colored Type 1 patterns cannot provide a halftone, as multiple
 * halftones may be used by the PaintProc procedure. Hence, we can only
 * hope this is a contone device.
 */
static const gx_device_halftone *
gx_dc_pattern_get_dev_halftone(const gx_device_color * pdevc)
{
    return 0;
}

/*
 * Uncolored Type 1 halftones make use of the halftone impplied by their
 * base color. Ideally this would be returned via an inhereted method,
 * but the device color structure does not support such an arrangement.
 */
static const gx_device_halftone *
gx_dc_pure_masked_get_dev_halftone(const gx_device_color * pdevc)
{
    return 0;
}

static const gx_device_halftone *
gx_dc_binary_masked_get_dev_halftone(const gx_device_color * pdevc)
{
    return pdevc->colors.binary.b_ht;
}

static const gx_device_halftone *
gx_dc_colored_masked_get_dev_halftone(const gx_device_color * pdevc)
{
    return pdevc->colors.colored.c_ht;
}

/* Macros for pattern loading */
#define FINISH_PATTERN_LOAD\
        while ( !gx_pattern_cache_lookup(pdevc, pgs, dev, select) )\
         { code = gx_pattern_load(pdevc, pgs, dev, select);\
           if ( code < 0 ) break;\
         }\
        return code;

/* Ensure that a colored Pattern is loaded in the cache. */
static int
gx_dc_pattern_load(gx_device_color * pdevc, const gs_gstate * pgs,
                   gx_device * dev, gs_color_select_t select)
{
    int code = 0;

    FINISH_PATTERN_LOAD
}
/* Ensure that an uncolored Pattern is loaded in the cache. */
static int
gx_dc_pure_masked_load(gx_device_color * pdevc, const gs_gstate * pgs,
                       gx_device * dev, gs_color_select_t select)
{
    int code = (*gx_dc_type_data_pure.load) (pdevc, pgs, dev, select);

    if (code < 0)
        return code;
    FINISH_PATTERN_LOAD
}
static int
gx_dc_devn_masked_load(gx_device_color * pdevc, const gs_gstate * pgs,
                       gx_device * dev, gs_color_select_t select)
{
    int code = (*gx_dc_type_data_devn.load) (pdevc, pgs, dev, select);

    if (code < 0)
        return code;
    FINISH_PATTERN_LOAD
}
static int
gx_dc_binary_masked_load(gx_device_color * pdevc, const gs_gstate * pgs,
                         gx_device * dev, gs_color_select_t select)
{
    int code = (*gx_dc_type_data_ht_binary.load) (pdevc, pgs, dev, select);

    if (code < 0)
        return code;
    FINISH_PATTERN_LOAD
}
static int
gx_dc_colored_masked_load(gx_device_color * pdevc, const gs_gstate * pgs,
                          gx_device * dev, gs_color_select_t select)
{
    int code = (*gx_dc_type_data_ht_colored.load) (pdevc, pgs, dev, select);

    if (code < 0)
        return code;
    FINISH_PATTERN_LOAD
}

/* Look up a pattern color in the cache. */
bool
gx_pattern_cache_lookup(gx_device_color * pdevc, const gs_gstate * pgs,
                        gx_device * dev, gs_color_select_t select)
{
    gx_pattern_cache *pcache = pgs->pattern_cache;
    gx_bitmap_id id = pdevc->mask.id;

    if (id == gx_no_bitmap_id) {
        color_set_null_pattern(pdevc);
        return true;
    }
    if (pcache != 0) {
        gx_color_tile *ctile = gx_pattern_cache_find_tile_for_id(pcache, id);
        bool internal_accum = true;
        if (pgs->have_pattern_streams) {
            int code = dev_proc(dev, dev_spec_op)(dev, gxdso_pattern_load, &id, sizeof(gx_bitmap_id));
            internal_accum = (code == 0);
            if (code < 0)
                return false;
        }
        if (ctile->id == id &&
            ctile->is_dummy == !internal_accum
            ) {
            int px = pgs->screen_phase[select].x;
            int py = pgs->screen_phase[select].y;

            if (gx_dc_is_pattern1_color(pdevc)) {       /* colored */
                pdevc->colors.pattern.p_tile = ctile;
#           if 0 /* Debugged with Bug688308.ps and applying patterns after clist.
                    Bug688308.ps has a step_matrix much bigger than pattern bbox;
                    rep_width, rep_height can't be used as mod.
                    Would like to use step_matrix instead. */
                color_set_phase_mod(pdevc, px, py,
                                    ctile->tbits.rep_width,
                                    ctile->tbits.rep_height);
#               else
                color_set_phase(pdevc, -px, -py);
#               endif
            }
            if (ctile->tmask.rep_width == 0 || ctile->tmask.rep_height == 0 || ctile->tmask.data == 0 || ctile->tmask.num_planes <= 0)
                pdevc->mask.m_tile = (gx_color_tile *)NULL;
            else
                pdevc->mask.m_tile = ctile;
            pdevc->mask.m_phase.x = -px;
            pdevc->mask.m_phase.y = -py;
            return true;
        }
    }
    return false;
}

#undef FINISH_PATTERN_LOAD

/* Compare two Pattern colors for equality. */
static bool
gx_dc_pattern_equal(const gx_device_color * pdevc1,
                    const gx_device_color * pdevc2)
{
    return pdevc2->type == pdevc1->type &&
        pdevc1->phase.x == pdevc2->phase.x &&
        pdevc1->phase.y == pdevc2->phase.y &&
        pdevc1->mask.id == pdevc2->mask.id;
}

/*
 * For shading and colored tiling patterns, it is not possible to say
 * which color components have non-zero values. The following routine
 * indicates this by just returning 1. The procedure is exported for
 * the benefit of gsptype2.c.
 */
int
gx_dc_pattern_get_nonzero_comps(
    const gx_device_color * pdevc_ignored,
    const gx_device *       dev_ignored,
    gx_color_index *        pcomp_bits_ignored )
{
    return 1;
}

static bool
gx_dc_devn_masked_equal(const gx_device_color * pdevc1,
                        const gx_device_color * pdevc2)
{
    return (*gx_dc_type_devn->equal) (pdevc1, pdevc2) &&
        pdevc1->mask.id == pdevc2->mask.id;
}
static bool
gx_dc_pure_masked_equal(const gx_device_color * pdevc1,
                        const gx_device_color * pdevc2)
{
    return (*gx_dc_type_pure->equal) (pdevc1, pdevc2) &&
        pdevc1->mask.id == pdevc2->mask.id;
}
static bool
gx_dc_binary_masked_equal(const gx_device_color * pdevc1,
                          const gx_device_color * pdevc2)
{
    return (*gx_dc_type_ht_binary->equal) (pdevc1, pdevc2) &&
        pdevc1->mask.id == pdevc2->mask.id;
}
static bool
gx_dc_colored_masked_equal(const gx_device_color * pdevc1,
                           const gx_device_color * pdevc2)
{
    return (*gx_dc_type_ht_colored->equal) (pdevc1, pdevc2) &&
        pdevc1->mask.id == pdevc2->mask.id;
}

typedef struct tile_trans_clist_info_s {
    gs_int_rect rect;
    int rowstride;
    int planestride;
    int n_chan; /* number of pixel planes including alpha */
    bool has_tags;	/* extra plane for tags */
    int width;
    int height;
} tile_trans_clist_info_t;

#define serialized_tile_common \
    gs_id id;\
    int size_b, size_c;\
    gs_matrix step_matrix;\
    gs_rect bbox;\
    int flags

typedef struct gx_dc_serialized_tile_s {
    serialized_tile_common;
} gx_dc_serialized_tile_t;

#define serialized_tile_trans \
    serialized_tile_common;\
    gs_blend_mode_t blending_mode

typedef struct gx_dc_serialized_trans_tile_s {
    serialized_tile_trans;
} gx_dc_serialized_trans_tile_t;

typedef struct gx_dc_serialized_pattern_tile_s {
    serialized_tile_trans;
    gs_int_point size;
} gx_dc_serialized_pattern_tile_t;

enum {
    TILE_IS_LOCKED   = (int)0x80000000,
    TILE_HAS_OVERLAP = 0x40000000,
    TILE_IS_SIMPLE   = 0x20000000,
    TILE_USES_TRANSP = 0x10000000,
    TILE_IS_CLIST    = 0x08000000,
    TILE_TYPE_MASK   = 0x07000000,	/* TilingType values are 1, 2, 3 */
    TILE_TYPE_SHIFT  = 24,
    TILE_DEPTH_MASK  = 0x00FFFFFF
};

static int
gx_dc_pattern_write_raster(gx_color_tile *ptile, int64_t offset, byte *data,
                           uint *psize, const gx_device *dev)
{
    int size_b, size_c;
    byte *dp = data;
    int left = *psize;
    int64_t offset1 = offset;

    size_b = (int)sizeof(gx_strip_bitmap) +
         ptile->tbits.size.y * ptile->tbits.raster * ptile->tbits.num_planes;
    size_c = ptile->tmask.data ? (int)sizeof(gx_strip_bitmap) + ptile->tmask.size.y * ptile->tmask.raster : 0;
    if (data == NULL) {
        *psize = sizeof(gx_dc_serialized_tile_t) + size_b + size_c;
        return 0;
    }
    if (offset1 == 0) { /* Serialize tile parameters: */
#if defined(DEBUG) || defined(PACIFY_VALGRIND) || defined(MEMENTO)
        gx_dc_serialized_tile_t buf = { 0 };
        gx_strip_bitmap buf1 = { 0 };
#else
        gx_dc_serialized_tile_t buf;
        gx_strip_bitmap buf1;
#endif

        buf.id = ptile->id;
        buf.size_b = size_b;
        buf.size_c = size_c;
        buf.step_matrix = ptile->step_matrix;
        buf.bbox = ptile->bbox;
        buf.flags = ptile->depth
                  | (ptile->tiling_type<<TILE_TYPE_SHIFT)
                  | (ptile->is_simple ? TILE_IS_SIMPLE : 0)
                  | (ptile->has_overlap ? TILE_HAS_OVERLAP : 0)
                  | (ptile->is_locked ? TILE_IS_LOCKED : 0);
        if (sizeof(buf) > left) {
            /* For a while we require the client to provide enough buffer size. */
            return_error(gs_error_unregistered); /* Must not happen. */
        }
        memcpy(dp, &buf, sizeof(buf));
        left -= sizeof(buf);
        dp += sizeof(buf);
        offset1 += sizeof(buf);

        buf1 = ptile->tbits;
        buf1.data = NULL; /* fixme: we don't need to write it actually. */
        if (sizeof(buf1) > left) {
            /* For a while we require the client to provide enough buffer size. */
            return_error(gs_error_unregistered); /* Must not happen. */
        }
        memcpy(dp, &buf1, sizeof(buf1));
        left -= sizeof(buf1);
        dp += sizeof(buf1);
        offset1 += sizeof(buf1);
    }
    if (offset1 < sizeof(gx_dc_serialized_tile_t) + size_b) {
        int l = min((size_b - sizeof(gx_strip_bitmap)) - (offset1 - sizeof(gx_dc_serialized_tile_t) -  sizeof(gx_strip_bitmap)), left);

        memcpy(dp, ptile->tbits.data + (offset1 - sizeof(gx_dc_serialized_tile_t) -  sizeof(gx_strip_bitmap)), l);
        left -= l;
        dp += l;
        offset1 += l;
    }
    if (left == 0)
        return 0;
    if (size_c == 0)
        return 0;
    if (offset1 < sizeof(gx_dc_serialized_tile_t) + size_b + sizeof(gx_strip_bitmap)) {
        gx_strip_bitmap buf;

        if (left < sizeof(buf))
            return_error(gs_error_unregistered); /* Not implemented yet because cmd_put_drawing_color provides a big buffer. */
        buf = ptile->tmask;
        buf.data = NULL; /* fixme: we don't need to write it actually. */
        memcpy(dp, &buf, sizeof(buf));
        left -= sizeof(buf);
        dp += sizeof(buf);
        offset1 += sizeof(buf);
    }
    if (offset1 < sizeof(gx_dc_serialized_tile_t) + size_b + size_c) {
        int l = min(size_c - sizeof(gx_strip_bitmap), left);

        memcpy(dp, ptile->tmask.data + (offset1 - sizeof(gx_dc_serialized_tile_t) - size_b - sizeof(gx_strip_bitmap)), l);
    }
    return 0;
}

/* This is for the case of writing into the clist the pattern that includes transparency.
   Transparency with patterns is handled a bit differently since the data is coming from
   a pdf14 device that includes planar data with an alpha channel */

static int
gx_dc_pattern_trans_write_raster(gx_color_tile *ptile, int64_t offset, byte *data, uint *psize)
{
    int size, size_h;
    byte *dp = data;
    int left = *psize;
    int64_t offset1 = offset;
    unsigned char *ptr;

    size_h = sizeof(gx_dc_serialized_trans_tile_t) + sizeof(tile_trans_clist_info_t);

    /* Everything that we need to handle the transparent tile */

    size = size_h + ptile->ttrans->n_chan * ptile->ttrans->planestride;
    if (ptile->ttrans->has_tags)
        size += ptile->ttrans->planestride;

    /* data is sent with NULL if the clist writer just wanted the size */
    if (data == NULL) {
        *psize = size;
        return 0;
    }
    if (offset1 == 0) { /* Serialize tile parameters: */
        gx_dc_serialized_trans_tile_t buf;
        tile_trans_clist_info_t trans_info;

        buf.id = ptile->id;
        buf.size_b = size - size_h;
        buf.size_c = 0;
        buf.flags = ptile->depth
                  | TILE_USES_TRANSP
                  | (ptile->tiling_type<<TILE_TYPE_SHIFT)
                  | (ptile->is_simple ? TILE_IS_SIMPLE : 0)
                  | (ptile->has_overlap ? TILE_HAS_OVERLAP : 0)
                  | (ptile->is_locked ? TILE_IS_LOCKED : 0);
        buf.step_matrix = ptile->step_matrix;
        buf.bbox = ptile->bbox;
        buf.blending_mode = ptile->blending_mode;
        if (sizeof(buf) > left) {
            /* For a while we require the client to provide enough buffer size. */
            return_error(gs_error_unregistered); /* Must not happen. */
        }
        memcpy(dp, &buf, sizeof(buf));
        left -= sizeof(buf);
        dp += sizeof(buf);
        offset1 += sizeof(buf);

        /* Do the transparency information now */
        trans_info.height = ptile->ttrans->height;
        trans_info.n_chan = ptile->ttrans->n_chan;
        trans_info.has_tags = ptile->ttrans->has_tags;
        trans_info.planestride = ptile->ttrans->planestride;
        trans_info.rect.p.x = ptile->ttrans->rect.p.x;
        trans_info.rect.p.y = ptile->ttrans->rect.p.y;
        trans_info.rect.q.x = ptile->ttrans->rect.q.x;
        trans_info.rect.q.y = ptile->ttrans->rect.q.y;
        trans_info.rowstride = ptile->ttrans->rowstride;
        trans_info.width = ptile->ttrans->width;

        if (sizeof(trans_info) > left) {
            return_error(gs_error_unregistered); /* Must not happen. */
        }
        memcpy(dp, &trans_info, sizeof(trans_info));
        left -= sizeof(trans_info);
        dp += sizeof(trans_info);
        offset1 += sizeof(trans_info);
    }

    /* Now do the transparency tile data itself.  Note that it may be split up
     * in the writing stage if it is large. The size include n_chan + the tag
     * plane if this buffer has_tags. */

    /* check if we have written it all */
    if (offset1 < size) {
        /* Get the most that we can write */
        int u = min(size - offset1, left);

        /* copy that amount */
        ptr = ptile->ttrans->transbytes;
        memcpy(dp, ptr + (offset1 - size_h), u);
    }
    return 0;
}

/* Write a pattern into command list, possibly dividing into portions. */
int
gx_dc_pattern_write(
    const gx_device_color *         pdevc,
    const gx_device_color_saved *   psdc,
    const gx_device *               dev,
    int64_t                         offset,
    byte *                          data,
    uint *                          psize )
{
    gx_color_tile *ptile = pdevc->colors.pattern.p_tile;
    int size_b, size_c;
    byte *dp = data;
    int left = *psize;
    int64_t offset1 = offset;
    int code, l;

    if (ptile == NULL)
        return 0;
    if (psdc->type == pdevc->type) {
        if (psdc->colors.pattern.id == ptile->id) {
            /* fixme : Do we need to check phase ? How ? */
            return 1; /* Same as saved one, don't write. */
        }
    }
    if (offset1 == 0 && left == sizeof(gs_id)) {
        /* A special case for writing a known pattern :
           Just write the tile id. */
        gs_id id = ptile->id; /* Ensure sizeof(gs_id). */
        if_debug2m('v', dev->memory,
                   "[v*] Writing trans tile ID into clist, uid = %ld id = %ld \n",
                   ptile->uid.id, ptile->id);
        memcpy(dp, &ptile->id, sizeof(id));
        *psize = sizeof(gs_id);
        return 0;
    }

    /* Check if pattern has transparency object
       If so then that is what we will stuff in
       the clist */
        if (ptile->ttrans != NULL) {
            if_debug2m('v', dev->memory,
                       "[v*] Writing trans tile into clist, uid = %ld id = %ld \n",
                       ptile->uid.id, ptile->id);
            return gx_dc_pattern_trans_write_raster(ptile, offset, data, psize);
        }

    if (ptile->cdev == NULL)
        return gx_dc_pattern_write_raster(ptile, offset, data, psize, dev);
    /* Here is where we write pattern-clist data */
    size_b = clist_data_size(ptile->cdev, 0);
    if (size_b < 0)
        return_error(gs_error_unregistered);
    size_c = clist_data_size(ptile->cdev, 1);
    if (size_c < 0)
        return_error(gs_error_unregistered);
    if (data == NULL) {
        *psize = sizeof(gx_dc_serialized_pattern_tile_t) + size_b + size_c;
        return 0;
    }
    if (offset1 == 0) { /* Serialize tile parameters: */
        gx_dc_serialized_pattern_tile_t buf;

        buf.id = ptile->id;
        buf.size.x = ptile->cdev->common.width;
        buf.size.y = ptile->cdev->common.height;
        buf.size_b = size_b;
        buf.size_c = size_c;
        buf.step_matrix = ptile->step_matrix;
        buf.bbox = ptile->bbox;
        buf.flags = ptile->depth
                  | TILE_IS_CLIST
                  | (ptile->tiling_type<<TILE_TYPE_SHIFT)
                  | (ptile->is_simple ? TILE_IS_SIMPLE : 0)
                  | (ptile->has_overlap ? TILE_HAS_OVERLAP : 0)
                  | (ptile->is_locked ? TILE_IS_LOCKED : 0)
                  | (ptile->cdev->common.page_uses_transparency ? TILE_USES_TRANSP : 0);
        buf.blending_mode = ptile->blending_mode;    /* in case tile has transparency */
        if (sizeof(buf) > left) {
            /* For a while we require the client to provide enough buffer size. */
            return_error(gs_error_unregistered); /* Must not happen. */
        }
        memcpy(dp, &buf, sizeof(gx_dc_serialized_pattern_tile_t));
        left -= sizeof(buf);
        dp += sizeof(buf);
        offset1 += sizeof(buf);
    }
    if (offset1 < sizeof(gx_dc_serialized_pattern_tile_t) + size_b) {
        l = min(left, size_b - (offset1 - sizeof(gx_dc_serialized_pattern_tile_t)));
        code = clist_get_data(ptile->cdev, 0, offset1 - sizeof(gx_dc_serialized_pattern_tile_t), dp, l);
        if (code < 0)
            return code;
        left -= l;
        offset1 += l;
        dp += l;
    }
    if (left > 0) {
        l = min(left, size_c - (offset1 - sizeof(gx_dc_serialized_pattern_tile_t) - size_b));
        code = clist_get_data(ptile->cdev, 1, offset1 - sizeof(gx_dc_serialized_pattern_tile_t) - size_b, dp, l);
        if (code < 0)
            return code;
    }
    return 0;
}

static int
gx_dc_pattern_read_raster(gx_color_tile *ptile, const gx_dc_serialized_tile_t *buf,
                          int64_t offset, const byte *data, uint size, gs_memory_t *mem)
{
    const byte *dp = data;
    int left = size;
    int64_t offset1 = offset;
    int size_b, size_c;

    if (buf != NULL) {
        size_b = buf->size_b;
        size_c = buf->size_c;
        ptile->tbits.data = gs_alloc_bytes(mem, size_b - sizeof(gx_strip_bitmap), "gx_dc_pattern_read_raster");
        if (ptile->tbits.data == NULL)
            return_error(gs_error_VMerror);
        if (size_c) {
            ptile->tmask.data = gs_alloc_bytes(mem, size_c - sizeof(gx_strip_bitmap), "gx_dc_pattern_read_raster");
            if (ptile->tmask.data == NULL)
                return_error(gs_error_VMerror);
        } else
            ptile->tmask.data = NULL;
        ptile->cdev = NULL;
    } else {
        size_b = gs_object_size(mem, ptile->tbits.data) + sizeof(gx_strip_bitmap);
        size_c = ptile->tmask.data != NULL ? gs_object_size(mem, ptile->tmask.data) + sizeof(gx_strip_bitmap) : 0;
    }
    /* Read tbits : */
    if (offset1 < sizeof(gx_dc_serialized_tile_t) + sizeof(gx_strip_bitmap)) {
        int l = min(sizeof(gx_strip_bitmap), left);
        byte *save = ptile->tbits.data;

        memcpy((byte*)&ptile->tbits + (offset1 - sizeof(gx_dc_serialized_tile_t)), dp, l);
        ptile->tbits.data = save;
        left -= l;
        offset1 += l;
        dp += l;
    }
    if (left == 0)
        return size;    /* we've consumed it all */
    if (offset1 < sizeof(gx_dc_serialized_tile_t) + size_b) {
        int l = min(sizeof(gx_dc_serialized_tile_t) + size_b - offset1, left);

        memcpy(ptile->tbits.data +
                (offset1 - sizeof(gx_dc_serialized_tile_t) - sizeof(gx_strip_bitmap)), dp, l);
        left -= l;
        offset1 += l;
        dp += l;
    }
    if (left == 0 || size_c == 0)
        return size - left;
    /* Read tmask : */
    if (offset1 < sizeof(gx_dc_serialized_tile_t) + size_b + sizeof(gx_strip_bitmap)) {
        int l = min(sizeof(gx_dc_serialized_tile_t) + size_b + sizeof(gx_strip_bitmap) - offset1, left);
        byte *save = ptile->tmask.data;

        memcpy((byte*)&ptile->tmask + (offset1 - sizeof(gx_dc_serialized_tile_t) - size_b), dp, l);
        ptile->tmask.data = save;
        left -= l;
        offset1 += l;
        dp += l;
    }
    if (left == 0)
        return size;
    if (offset1 < sizeof(gx_dc_serialized_tile_t) + size_b + size_c) {
        int l = min(sizeof(gx_dc_serialized_tile_t) + size_b + size_c - offset1, left);

        memcpy(ptile->tmask.data +
                (offset1 - sizeof(gx_dc_serialized_tile_t) - size_b - sizeof(gx_strip_bitmap)), dp, l);
        left -= l;
    }
    return size - left;
}

/* This reads in the transparency buffer from the clist */
static int
gx_dc_pattern_read_trans_buff(gx_color_tile *ptile, int64_t offset,
                              const byte *data, uint size, gs_memory_t *mem)
{
    const byte *dp = data;
    int left = size;
    int64_t offset1 = offset;
    gx_pattern_trans_t *trans_pat = ptile->ttrans;
    int data_size;

    data_size = trans_pat->planestride * trans_pat->n_chan;
    if (trans_pat->has_tags)
        data_size += trans_pat->planestride;

    /* Allocate the bytes */
    if (trans_pat->transbytes == NULL){
        trans_pat->transbytes = gs_alloc_bytes(mem, data_size, "gx_dc_pattern_read_raster");
        trans_pat->mem = mem;
        if (trans_pat->transbytes == NULL)
                return_error(gs_error_VMerror);
    }
    /* Read transparency buffer */
    if (offset1 < sizeof(gx_dc_serialized_trans_tile_t) + sizeof(tile_trans_clist_info_t) + data_size ) {

        int u = min(data_size - (offset1 - sizeof(gx_dc_serialized_trans_tile_t) - sizeof(tile_trans_clist_info_t)), left);
        byte *save = trans_pat->transbytes;

        memcpy( trans_pat->transbytes + offset1 - sizeof(gx_dc_serialized_trans_tile_t) -
                                    sizeof(tile_trans_clist_info_t), dp, u);
        trans_pat->transbytes = save;
        left -= u;
    }
     return size - left;
}

int
gx_dc_pattern_read(
    gx_device_color *       pdevc,
    const gs_gstate * pgs,
    const gx_device_color * prior_devc,
    const gx_device *       dev,
    int64_t                    offset,
    const byte *            data,
    uint                    size,
    gs_memory_t *           mem,
    int                     x0,
    int                     y0)
{
    gx_dc_serialized_pattern_tile_t buf;
    int size_b, size_c = -1;
    const byte *dp = data;
    int left = size;
    int64_t offset1 = offset;
    gx_color_tile *ptile;
    int code, l;
    tile_trans_clist_info_t trans_info = { { { 0 } } };
    int cache_space_needed;
    bool deep = device_is_deep(dev);
    size_t buf_read;

    if (offset == 0) {
        pdevc->mask.id = gx_no_bitmap_id;
        pdevc->mask.m_tile = NULL;
        if (size == 0) {
            /* Null pattern. */
            pdevc->type = &gx_dc_pattern;
            pdevc->colors.pattern.p_tile = NULL;
            pdevc->mask.id = gs_no_id;
            return 0;
        }
        if (size == sizeof(gs_id)) {
            /* A special case for restoring a known (cached) pattern :
               read the tile id only. */
            gs_id id; /* Ensure data size == sizeof(gs_id). */

            memcpy(&id, dp, sizeof(id));
            pdevc->type = &gx_dc_pattern;
            pdevc->mask.id = id; /* See gx_dc_pattern_load, gx_pattern_cache_lookup. */
            return size;
        }
        if (sizeof(buf) > size) {
            /* For a while we require the client to provide enough buffer size. */
            return_error(gs_error_unregistered); /* Must not happen. */
        }
        memcpy(&buf, dp, sizeof(gx_dc_serialized_tile_t));
        dp += sizeof(gx_dc_serialized_tile_t);
        buf_read = sizeof(gx_dc_serialized_tile_t);
        if (buf.flags & TILE_USES_TRANSP) {
            memcpy(((char *)&buf)+sizeof(gx_dc_serialized_tile_t), dp, sizeof(gx_dc_serialized_trans_tile_t) - sizeof(gx_dc_serialized_tile_t));
            dp += sizeof(gx_dc_serialized_trans_tile_t) - sizeof(gx_dc_serialized_tile_t);
            buf_read = sizeof(gx_dc_serialized_trans_tile_t);
        }
        if (buf.flags & TILE_IS_CLIST) {
            memcpy(((char *)&buf) + sizeof(gx_dc_serialized_trans_tile_t), dp, sizeof(gx_dc_serialized_pattern_tile_t) - sizeof(gx_dc_serialized_trans_tile_t));
            dp += sizeof(gx_dc_serialized_pattern_tile_t) - sizeof(gx_dc_serialized_trans_tile_t);
            buf_read = sizeof(gx_dc_serialized_pattern_tile_t);
        }
        left -= buf_read;
        offset1 += buf_read;

        if ((buf.flags & TILE_USES_TRANSP) && !(buf.flags & TILE_IS_CLIST)){

            if (buf_read + sizeof(tile_trans_clist_info_t) > size) {
                return_error(gs_error_unregistered); /* Must not happen. */
            }

            memcpy(&trans_info, dp, sizeof(trans_info));
            dp += sizeof(trans_info);
            left -= sizeof(trans_info);
            offset1 += sizeof(trans_info);

                /* limit our upper bound to avoid int overflow */
            cache_space_needed = trans_info.planestride > (0x7fffffff / 6) ? 0x7fff0000 :
                        trans_info.planestride * trans_info.n_chan;
        } else {
            /* the following works for raster or clist patterns */
            cache_space_needed = buf.size_b + buf.size_c;
        }

        /* Free up any unlocked patterns if needed */
        gx_pattern_cache_ensure_space((gs_gstate *)pgs, cache_space_needed);

        /* If the pattern tile is already in the cache, make sure it isn't locked */
        /* The lock will be reset below, but the read logic needs to finish loading the pattern. */
        ptile = &(pgs->pattern_cache->tiles[buf.id % pgs->pattern_cache->num_tiles]);
        if (ptile->id != gs_no_id && ptile->is_locked) {
            /* we shouldn't have miltiple tiles locked, but check if OK before unlocking */
            if (ptile->id != buf.id)
                return_error(gs_error_unregistered);	/* can't unlock some other tile in this slot */
            code = gx_pattern_cache_entry_set_lock((gs_gstate *)pgs, buf.id, false);        /* make sure not locked */
            if (code < 0)
                return code;	/* can't happen since we call ensure_space above, but Coverity doesn't know that */
        }
        /* get_entry will free the tile in the cache slot if it isn't empty */
        code = gx_pattern_cache_get_entry((gs_gstate *)pgs, /* Break 'const'. */
                        buf.id, &ptile);
        if (code < 0)
            return code;
        gx_pattern_cache_update_used((gs_gstate *)pgs, cache_space_needed);
        ptile->bits_used = cache_space_needed;
        pdevc->type = &gx_dc_pattern;
        pdevc->colors.pattern.p_tile = ptile;
        ptile->id = buf.id;
        pdevc->mask.id = buf.id;
        ptile->step_matrix = buf.step_matrix;
        ptile->bbox = buf.bbox;
        ptile->depth = buf.flags & TILE_DEPTH_MASK;
        ptile->tiling_type = (buf.flags & TILE_TYPE_MASK)>>TILE_TYPE_SHIFT;
        ptile->is_simple = !!(buf.flags & TILE_IS_SIMPLE);
        ptile->has_overlap = !!(buf.flags & TILE_HAS_OVERLAP);
        ptile->is_locked = !!(buf.flags & TILE_IS_LOCKED);
        ptile->blending_mode = buf.blending_mode;
        ptile->is_dummy = false;

        if (!(buf.flags & TILE_IS_CLIST)) {

            if (buf.flags & TILE_USES_TRANSP){

                /* Make a new ttrans object */

                ptile->ttrans = new_pattern_trans_buff(mem);
                /* trans_info was loaded above */

                ptile->ttrans->height = trans_info.height;
                ptile->ttrans->n_chan = trans_info.n_chan;
                ptile->ttrans->has_tags = trans_info.has_tags;
                ptile->ttrans->pdev14 = NULL;
                ptile->ttrans->planestride = trans_info.planestride;
                ptile->ttrans->rect.p.x = trans_info.rect.p.x;
                ptile->ttrans->rect.p.y = trans_info.rect.p.y;
                ptile->ttrans->rect.q.x = trans_info.rect.q.x;
                ptile->ttrans->rect.q.y = trans_info.rect.q.y;
                ptile->ttrans->rowstride = trans_info.rowstride;
                ptile->ttrans->width = trans_info.width;
                ptile->ttrans->deep = deep;
                pdevc->type = &gx_dc_pattern_trans;
                if_debug2m('v', pgs->memory,
                           "[v*] Reading trans tile from clist into cache, uid = %ld id = %ld \n",
                           ptile->uid.id, ptile->id);

                code = gx_dc_pattern_read_trans_buff(ptile, offset1, dp, left, mem);
                if (code < 0)
                    return code;
                return code + buf_read + sizeof(trans_info);

            } else {
                code = gx_dc_pattern_read_raster(ptile, (gx_dc_serialized_tile_t *)&buf, offset1, dp, left, mem);
                if (code < 0)
                    return code;
                return code + buf_read;
            }

        }

        /* Here is where we read back from the clist */
        size_b = buf.size_b;
        size_c = buf.size_c;
        ptile->tbits.size.x = size_b; /* HACK: Use unrelated field for saving size_b between calls. */
        ptile->tbits.size.y = size_c; /* HACK: Use unrelated field for saving size_c between calls. */
        {
            gs_gstate state;
            gs_pattern1_instance_t inst;

            memset(&state, 0, sizeof(state));
            memset(&inst, 0, sizeof(inst));
            /* NB: Currently PaintType 2 can't pass here. */
            state.device = (gx_device *)dev; /* Break 'const'. */
            inst.templat.PaintType = 1;
            inst.size.x = buf.size.x;
            inst.size.y = buf.size.y;
            inst.saved = &state;
            inst.is_clist = !!(buf.flags & TILE_IS_CLIST);	/* tell gx_pattern_accum_alloc to use clist */
            ptile->cdev = (gx_device_clist *)gx_pattern_accum_alloc(mem, mem,
                               &inst, "gx_dc_pattern_read");
            if (ptile->cdev == NULL)
                return_error(gs_error_VMerror);
            ptile->cdev->common.page_uses_transparency = !!(buf.flags & TILE_USES_TRANSP);
            code = dev_proc(&ptile->cdev->writer, open_device)((gx_device *)&ptile->cdev->writer);
            if (code < 0)
                return code;
        }
    } else {
        ptile = pdevc->colors.pattern.p_tile;

        if (ptile->ttrans != NULL)
            return gx_dc_pattern_read_trans_buff(ptile, offset1, dp, left, mem);

        if (ptile->cdev == NULL)
            return gx_dc_pattern_read_raster(ptile, NULL, offset1, dp, left, mem);

        size_b = ptile->tbits.size.x;
    }
    if (offset1 < sizeof(buf) + size_b) {
        l = min(left, size_b - (offset1 - sizeof(buf)));
        code = clist_put_data(ptile->cdev, 0, offset1 - sizeof(buf), dp, l);
        if (code < 0)
            return code;
        l = code;
        left -= l;
        offset1 += l;
        dp += l;
        ptile->cdev->common.page_info.bfile_end_pos = offset1 - sizeof(buf);
    }
    if (left > 0) {
        l = left;
        code = clist_put_data(ptile->cdev, 1, offset1 - sizeof(buf) - size_b, dp, l);
        if (code < 0)
            return code;
        l = code;
        left -= l;
    }
    return size - left;
}

/* Set the transparency pattern procs for filling rects.  */
void
gx_set_pattern_procs_trans(gx_device_color *pdevc)
{
    pdevc->type = &gx_dc_pattern_trans;
    return;
}

/* Set the standard pattern procs for filling rects.  */
void
gx_set_pattern_procs_standard(gx_device_color *pdevc)
{
    pdevc->type = &gx_dc_pattern;
    return;
}

/* Check if transparency pattern procs for filling rects.  */
bool
gx_pattern_procs_istrans(gx_device_color *pdevc)
{
    return(pdevc->type == &gx_dc_pattern_trans);
}

/* Check device color for Pattern Type 1. */
bool
gx_dc_is_pattern1_color(const gx_device_color *pdevc)
{
    return (pdevc->type == &gx_dc_pattern || pdevc->type == &gx_dc_pattern_trans);
}

/* Check device color for Pattern Type 1 with transparency involved */
bool
gx_dc_is_pattern1_color_with_trans(const gx_device_color *pdevc)
{
    if (!(pdevc->type == &gx_dc_pattern || pdevc->type == &gx_dc_pattern_trans)) {
        return(false);
    }
    return(gx_pattern1_get_transptr(pdevc) != NULL);
}
