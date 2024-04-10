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


/* PatternType 1 filling algorithms */
#include "string_.h"
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsrop.h"
#include "gsmatrix.h"
#include "gxcspace.h"           /* for gscolor2.h */
#include "gxcolor2.h"
#include "gxdcolor.h"
#include "gxdevcli.h"
#include "gxdevmem.h"
#include "gxclip2.h"
#include "gxpcolor.h"
#include "gxp1impl.h"
#include "gxcldev.h"
#include "gxblend.h"
#include "gsicc_cache.h"
#include "gxdevsop.h"
#include <limits.h>             /* For INT_MAX etc */

#include "gdevp14.h"

#define fastfloor(x) (((int)(x)) - (((x)<0) && ((x) != (float)(int)(x))))

/* Define the state for tile filling. */
typedef struct tile_fill_state_s {

    /* Original arguments */

    const gx_device_color *pdevc;       /* pattern color */
    int x0, y0, w0, h0;
    gs_logical_operation_t lop;
    const gx_rop_source_t *source;

    /* Variables set at initialization */

    gx_device_tile_clip *cdev;
    gx_device *pcdev;           /* original device or cdev */
    const gx_strip_bitmap *tmask;
    gs_int_point phase;
    int num_planes;    /* negative if not planar */


    /* Following are only for uncolored patterns */

    dev_color_proc_fill_rectangle((*fill_rectangle));

    /* Following are only for colored patterns */

    gx_device *orig_dev;
    int xoff, yoff;             /* set dynamically */

} tile_fill_state_t;

/* Define the state for tile filling.
   This is used for when we have
   transparency */
typedef struct tile_fill_trans_state_s {

    /* Original arguments */

    const gx_device_color *pdevc;       /* pattern color */
    int x0, y0, w0, h0;

    /* Variables set at initialization */

    gx_device *pcdev;           /* original device or &cdev */
    gs_int_point phase;

    int xoff, yoff;             /* set dynamically */

} tile_fill_trans_state_t;

/* we need some means of detecting if a forwarding clipping device was
   installed.  If the tile state cdev pointer is non-NULL, then the
   target output device must be the clipping device. */
#define CLIPDEV_INSTALLED (state.cdev != NULL)

/* Initialize the filling state. */
static int
tile_fill_init(tile_fill_state_t * ptfs, const gx_device_color * pdevc,
               gx_device * dev, bool set_mask_phase)
{
    gx_color_tile *m_tile = pdevc->mask.m_tile;
    int px, py;
    int num_planar_planes;

    ptfs->pdevc = pdevc;
    num_planar_planes = dev->num_planar_planes;
    if (num_planar_planes) {
        ptfs->num_planes = dev->num_planar_planes;
    } else {
        ptfs->num_planes = -1;
    }
    if (m_tile == 0) {          /* no clipping */
        ptfs->cdev = NULL;
        ptfs->pcdev = dev;
        ptfs->phase = pdevc->phase;
        return 0;
    }
    if ((ptfs->cdev = (gx_device_tile_clip *)gs_alloc_struct(dev->memory,
                                                 gx_device_tile_clip,
                                                 &st_device_tile_clip,
                                                 "tile_fill_init(cdev)")) == NULL) {
        return_error(gs_error_VMerror);
    }
    ptfs->cdev->finalize = NULL;
    ptfs->pcdev = (gx_device *)ptfs->cdev;
    ptfs->tmask = &m_tile->tmask;
    ptfs->phase.x = pdevc->mask.m_phase.x;
    ptfs->phase.y = pdevc->mask.m_phase.y;
    /*
     * For non-simple tiles, the phase will be reset on each pass of the
     * tile_by_steps loop, but for simple tiles, we must set it now.
     */
    if (set_mask_phase && m_tile->is_simple) {
        px = imod(-(int)fastfloor(m_tile->step_matrix.tx - ptfs->phase.x + 0.5),
                  m_tile->tmask.rep_width);
        py = imod(-(int)fastfloor(m_tile->step_matrix.ty - ptfs->phase.y + 0.5),
                  m_tile->tmask.rep_height);
    } else
        px = py = 0;
    return tile_clip_initialize(ptfs->cdev, ptfs->tmask, dev, px, py);
}

static int
threshold_ceil(float f, float e)
{
    int i = (int)fastfloor(f);

    if (f - i < e)
        return i;
    return i+1;
}
static int
threshold_floor(float f, float e)
{
    int i = (int)fastfloor(f);

    if (f - i > 1-e)
        return i+1;
    return i;
}

/* Return a conservative approximation to the maximum expansion caused by
 * a given matrix. */
static float
matrix_expansion(gs_matrix *m)
{
    float e, f;

    e = fabs(m->xx);
    f = fabs(m->xy);
    if (f > e)
        e = f;
    f = fabs(m->yx);
    if (f > e)
        e = f;
    f = fabs(m->yy);
    if (f > e)
        e = f;

    return e*e;
}
/*
 * Fill with non-standard X and Y stepping.
 * ptile is pdevc->colors.pattern.{m,p}_tile.
 * tbits_or_tmask is whichever of tbits and tmask is supplying
 * the tile size.
 * This implementation could be sped up considerably!
 */
static int
tile_by_steps(tile_fill_state_t * ptfs, int x0, int y0, int w0, int h0,
              const gx_color_tile * ptile,
              const gx_strip_bitmap * tbits_or_tmask,
              int (*fill_proc) (const tile_fill_state_t * ptfs,
                                int x, int y, int w, int h))
{
    int x1 = x0 + w0, y1 = y0 + h0;
    int i0, i1, j0, j1, i, j;
    gs_matrix step_matrix;      /* translated by phase */
    int code;
#ifdef DEBUG
    const gs_memory_t *mem = ptfs->pcdev->memory;
#endif

    ptfs->x0 = x0, ptfs->w0 = w0;
    ptfs->y0 = y0, ptfs->h0 = h0;
    step_matrix = ptile->step_matrix;
    step_matrix.tx -= ptfs->phase.x;
    step_matrix.ty -= ptfs->phase.y;
    {
        gs_rect bbox;           /* bounding box in device space */
        gs_rect ibbox;          /* bounding box in stepping space */
        double bbw = ptile->bbox.q.x - ptile->bbox.p.x;
        double bbh = ptile->bbox.q.y - ptile->bbox.p.y;
        double u0, v0, u1, v1;
        /* Any difference smaller than error is guaranteed to result in
         * less than a pixels difference in the output. Use this as a
         * threshold when rounding to allow for inaccuracies in
         * floating point maths. This enables us to avoid doing more
         * repeats than we need to. */
        float error = 1/matrix_expansion(&step_matrix);

        bbox.p.x = x0, bbox.p.y = y0;
        bbox.q.x = x1, bbox.q.y = y1;
        code = gs_bbox_transform_inverse(&bbox, &step_matrix, &ibbox);
        if (code < 0)
            return code;
        if_debug10m('T', mem,
          "[T]x,y=(%d,%d) w,h=(%d,%d) => (%g,%g),(%g,%g), offset=(%g,%g)\n",
                    x0, y0, w0, h0,
                    ibbox.p.x, ibbox.p.y, ibbox.q.x, ibbox.q.y,
                    step_matrix.tx, step_matrix.ty);
        /*
         * If the pattern is partly transparent and XStep/YStep is smaller
         * than the device space BBox, we need to ensure that we cover
         * each pixel of the rectangle being filled with *every* pattern
         * that overlaps it, not just *some* pattern copy.
         */
        u0 = ibbox.p.x - max(ptile->bbox.p.x, 0);
        v0 = ibbox.p.y - max(ptile->bbox.p.y, 0);
        u1 = ibbox.q.x - min(ptile->bbox.q.x, 0);
        v1 = ibbox.q.y - min(ptile->bbox.q.y, 0);
        if (!ptile->is_simple)
            u0 -= bbw, v0 -= bbh, u1 += bbw, v1 += bbh;
        i0 = threshold_floor(u0, error);
        j0 = threshold_floor(v0, error);
        i1 = threshold_ceil(u1, error);
        j1 = threshold_ceil(v1, error);
    }
    if_debug4m('T', mem, "[T]i=(%d,%d) j=(%d,%d)\n", i0, i1, j0, j1);
    for (i = i0; i < i1; i++)
        for (j = j0; j < j1; j++) {
            int x = (int)fastfloor(step_matrix.xx * i +
                          step_matrix.yx * j + step_matrix.tx);
            int y = (int)fastfloor(step_matrix.xy * i +
                          step_matrix.yy * j + step_matrix.ty);
            int w = tbits_or_tmask->size.x;
            int h = tbits_or_tmask->size.y;
            int xoff, yoff;

            if_debug4m('T', mem, "[T]i=%d j=%d x,y=(%d,%d)", i, j, x, y);
            if (x == INT_MIN || y == INT_MIN) {
                if_debug0m('T', mem, " underflow!\n");
                continue;
            }
            if (x < x0)
                xoff = x0 - x, x = x0, w -= xoff;
            else
                xoff = 0;
            if (y < y0)
                yoff = y0 - y, y = y0, h -= yoff;
            else
                yoff = 0;
            /* Check for overflow */
            if (h > 0 && max_int - h < y)
                h = max_int - y;
            if (w > 0 && max_int - w < x)
                w = max_int - x;
            if (x + w > x1)
                w = x1 - x;
            if (y + h > y1)
                h = y1 - y;
            if_debug6m('T', mem, "=>(%d,%d) w,h=(%d,%d) x/yoff=(%d,%d)\n",
                       x, y, w, h, xoff, yoff);
            if (w > 0 && h > 0) {
                if (ptfs->pcdev == (gx_device *)ptfs->cdev)
                    tile_clip_set_phase(ptfs->cdev,
                                imod(xoff - x, ptfs->tmask->rep_width),
                                imod(yoff - y, ptfs->tmask->rep_height));
                /* Set the offsets for colored pattern fills */
                ptfs->xoff = xoff;
                ptfs->yoff = yoff;
                code = (*fill_proc) (ptfs, x, y, w, h);
                if (code < 0)
                    return code;
            }
        }
    return 0;
}

/* Fill a rectangle with a colored Pattern. */
/* Note that we treat this as "texture" for RasterOp. */
static int
tile_colored_fill(const tile_fill_state_t * ptfs,
                  int x, int y, int w, int h)
{
    gx_color_tile *ptile = ptfs->pdevc->colors.pattern.p_tile;
    gs_logical_operation_t lop = ptfs->lop;
    const gx_rop_source_t *source = ptfs->source;
    gx_device *dev = ptfs->orig_dev;
    int xoff = ptfs->xoff, yoff = ptfs->yoff;
    gx_strip_bitmap *bits = &ptile->tbits;
    const byte *data = bits->data;
    bool full_transfer = (w == ptfs->w0 && h == ptfs->h0);
    int code = 0;

    if (source == NULL && lop_no_S_is_T(lop) && dev_proc(dev, copy_planes) != gx_default_copy_planes &&
        ptfs->num_planes > 0) {
            code = (*dev_proc(ptfs->pcdev, copy_planes))
                    (ptfs->pcdev, data + bits->raster * yoff, xoff,
                     bits->raster,
                     (full_transfer ? bits->id : gx_no_bitmap_id),
                     x, y, w, h, ptile->tbits.rep_height);
    } else if (source == NULL && lop_no_S_is_T(lop)) {
            code = (*dev_proc(ptfs->pcdev, copy_color))
                    (ptfs->pcdev, data + bits->raster * yoff, xoff,
                     bits->raster,
                     (full_transfer ? bits->id : gx_no_bitmap_id),
                     x, y, w, h);
    } else {
        gx_strip_bitmap data_tile;
        gx_bitmap_id source_id;
        gx_rop_source_t no_source;

        if (source == NULL)
            set_rop_no_source(source, no_source, dev);
        source_id = (full_transfer ? source->id : gx_no_bitmap_id);
        data_tile.data = (byte *) data;         /* actually const */
        data_tile.raster = bits->raster;
        data_tile.size.x = data_tile.rep_width = ptile->tbits.size.x;
        data_tile.size.y = data_tile.rep_height = ptile->tbits.size.y;
        data_tile.id = bits->id;
        data_tile.shift = data_tile.rep_shift = 0;
        data_tile.num_planes = (ptfs->num_planes > 1 ? ptfs->num_planes : 1);
        code = (*dev_proc(ptfs->pcdev, strip_copy_rop2))
                           (ptfs->pcdev,
                            source->sdata + (y - ptfs->y0) * source->sraster,
                            source->sourcex + (x - ptfs->x0),
                            source->sraster, source_id,
                            (source->use_scolors ? source->scolors : NULL),
                            &data_tile, NULL,
                            x, y, w, h,
                            imod(xoff - x, data_tile.rep_width),
                            imod(yoff - y, data_tile.rep_height),
                            lop,
                            source->planar_height);
    }
    return code;
}

/* Fill a rectangle with a colored Pattern. */
/* Note that we treat this as "texture" for RasterOp. */
static int
tile_pattern_clist(const tile_fill_state_t * ptfs,
                  int x, int y, int w, int h)
{
    gx_color_tile *ptile = ptfs->pdevc->colors.pattern.p_tile;
    gx_device_clist *cdev = ptile->cdev;
    gx_device_clist_reader *crdev = (gx_device_clist_reader *)cdev;
    gx_device *dev = ptfs->orig_dev;
    int code;

    crdev->offset_map = NULL;
    code = crdev->page_info.io_procs->rewind(crdev->page_info.bfile, false, NULL);
    if (code < 0) return code;
    code = crdev->page_info.io_procs->rewind(crdev->page_info.cfile, false, NULL);
    if (code < 0) return code;

    clist_render_init(cdev);
     /* Check for and get ICC profile table */
    if (crdev->icc_table == NULL) {
        code = clist_read_icctable(crdev);
        if (code < 0)
            return code;
    }
    /* Also allocate the icc cache for the clist reader */
    if ( crdev->icc_cache_cl == NULL )
        crdev->icc_cache_cl = gsicc_cache_new(crdev->memory->thread_safe_memory);
    if_debug0m('L', dev->memory, "Pattern clist playback begin\n");
    code = clist_playback_file_bands(playback_action_render,
                crdev, &crdev->page_info, dev, 0, 0, ptfs->xoff - x, ptfs->yoff - y);
    if_debug0m('L', dev->memory, "Pattern clist playback end\n");
    /* FIXME: it would be preferable to have this persist, but as
     * clist_render_init() sets it to NULL, we currently have to
     * cleanup before returning. Set to NULL for safety
     */
    rc_decrement(crdev->icc_cache_cl, "tile_pattern_clist");
    crdev->icc_cache_cl = NULL;
    return code;
}

int
gx_dc_pattern_fill_rectangle(const gx_device_color * pdevc, int x, int y,
                             int w, int h, gx_device * dev,
                             gs_logical_operation_t lop,
                             const gx_rop_source_t * source)
{
    gx_color_tile *ptile = pdevc->colors.pattern.p_tile;
    const gx_rop_source_t *rop_source = source;
    gx_rop_source_t no_source;
    gx_strip_bitmap *bits;
    tile_fill_state_t state;
    int code;

    if (ptile == 0)             /* null pattern */
        return 0;
    if (rop_source == NULL)
        set_rop_no_source(rop_source, no_source, dev);
    bits = &ptile->tbits;

    code = tile_fill_init(&state, pdevc, dev, false);	/* This _may_ allocate state.cdev */
    if (code < 0) {
        goto exit;
    }
    if (ptile->is_simple && ptile->cdev == NULL) {
        int px =
            imod(-(int)fastfloor(ptile->step_matrix.tx - state.phase.x + 0.5),
                 bits->rep_width);
        int py =
            imod(-(int)fastfloor(ptile->step_matrix.ty - state.phase.y + 0.5),
                 bits->rep_height);

        if (CLIPDEV_INSTALLED)
            tile_clip_set_phase(state.cdev, px, py);
        if (source == NULL && lop_no_S_is_T(lop))
            code = (*dev_proc(state.pcdev, strip_tile_rectangle))
                (state.pcdev, bits, x, y, w, h,
                 gx_no_color_index, gx_no_color_index, px, py);
        else
            code = (*dev_proc(state.pcdev, strip_copy_rop2))
                        (state.pcdev,
                         rop_source->sdata, rop_source->sourcex,
                         rop_source->sraster, rop_source->id,
                         (rop_source->use_scolors ? rop_source->scolors : NULL),
                         bits, NULL, x, y, w, h, px, py, lop,
                         rop_source->planar_height);
    } else {
        state.lop = lop;
        state.source = source;
        state.orig_dev = dev;
        if (ptile->cdev == NULL) {
            code = tile_by_steps(&state, x, y, w, h, ptile,
                                 &ptile->tbits, tile_colored_fill);
        } else {
            gx_device_clist *cdev = ptile->cdev;
            gx_device_clist_reader *crdev = (gx_device_clist_reader *)cdev;
            gx_strip_bitmap tbits;

            crdev->yplane.depth = 0; /* Don't know what to set here. */
            crdev->yplane.shift = 0;
            crdev->yplane.index = -1;
            crdev->pages = NULL;
            crdev->num_pages = 1;
            state.orig_dev = dev;
            tbits = ptile->tbits;
            tbits.size.x = crdev->width;
            tbits.size.y = crdev->height;
            code = tile_by_steps(&state, x, y, w, h, ptile,
                                 &tbits, tile_pattern_clist);
        }
    }
exit:
    if (CLIPDEV_INSTALLED) {
        tile_clip_free((gx_device_tile_clip *)state.cdev);
        state.cdev = NULL;
    }
    return code;
}

/* Fill a rectangle with an uncolored Pattern. */
/* Note that we treat this as "texture" for RasterOp. */
static int
tile_masked_fill(const tile_fill_state_t * ptfs,
                 int x, int y, int w, int h)
{
    if (ptfs->source == NULL)
        return (*ptfs->fill_rectangle)
            (ptfs->pdevc, x, y, w, h, ptfs->pcdev, ptfs->lop, NULL);
    else {
        const gx_rop_source_t *source = ptfs->source;
        gx_rop_source_t step_source;

        step_source.sdata = source->sdata + (y - ptfs->y0) * source->sraster;
        step_source.sourcex = source->sourcex + (x - ptfs->x0);
        step_source.sraster = source->sraster;
        step_source.id = (w == ptfs->w0 && h == ptfs->h0 ?
                          source->id : gx_no_bitmap_id);
        step_source.scolors[0] = source->scolors[0];
        step_source.scolors[1] = source->scolors[1];
        step_source.planar_height = source->planar_height;
        step_source.use_scolors = source->use_scolors;
        return (*ptfs->fill_rectangle)
            (ptfs->pdevc, x, y, w, h, ptfs->pcdev, ptfs->lop, &step_source);
    }
}
int
gx_dc_pure_masked_fill_rect(const gx_device_color * pdevc,
                            int x, int y, int w, int h, gx_device * dev,
                            gs_logical_operation_t lop,
                            const gx_rop_source_t * source)
{
    gx_color_tile *ptile = pdevc->mask.m_tile;
    tile_fill_state_t state;
    int code;

    /*
     * This routine should never be called if there is no masking,
     * but we leave the checks below just in case.
     */
    code = tile_fill_init(&state, pdevc, dev, true);
    if (code < 0)
        goto exit;
    if (state.pcdev == dev || ptile->is_simple)
        code = (*gx_dc_type_data_pure.fill_rectangle)
            (pdevc, x, y, w, h, state.pcdev, lop, source);
    else {
        state.lop = lop;
        state.source = source;
        state.fill_rectangle = gx_dc_type_data_pure.fill_rectangle;
        code = tile_by_steps(&state, x, y, w, h, ptile, &ptile->tmask,
                             tile_masked_fill);
    }
exit:
    if (CLIPDEV_INSTALLED) {
        tile_clip_free((gx_device_tile_clip *)state.cdev);
        state.cdev = NULL;
    }
    return code;
}

int
gx_dc_devn_masked_fill_rect(const gx_device_color * pdevc,
                            int x, int y, int w, int h, gx_device * dev,
                            gs_logical_operation_t lop,
                            const gx_rop_source_t * source)
{
    gx_color_tile *ptile = pdevc->mask.m_tile;
    tile_fill_state_t state;
    int code;

    /*
     * This routine should never be called if there is no masking,
     * but we leave the checks below just in case.
     */
    code = tile_fill_init(&state, pdevc, dev, true);
    if (code < 0)
        goto exit;
    if (state.pcdev == dev || ptile->is_simple) {
        gx_device_color dcolor = *pdevc;

        if (ptile == NULL) {
        int k;

            /* Have to set the pdevc to a non mask type since the pattern was stored as non-masking */
            dcolor.type = gx_dc_type_devn;
            for (k = 0; k < GS_CLIENT_COLOR_MAX_COMPONENTS; k++) {
                dcolor.colors.devn.values[k] = pdevc->colors.devn.values[k];
            }
        }
        code = (*gx_dc_type_data_devn.fill_rectangle)
            (&dcolor, x, y, w, h, state.pcdev, lop, source);
    } else {
        state.lop = lop;
        state.source = source;
        state.fill_rectangle = gx_dc_type_data_devn.fill_rectangle;
        code = tile_by_steps(&state, x, y, w, h, ptile, &ptile->tmask,
                             tile_masked_fill);
    }
exit:
    if (CLIPDEV_INSTALLED) {
        tile_clip_free((gx_device_tile_clip *)state.cdev);
        state.cdev = NULL;
    }
    return code;
}

int
gx_dc_binary_masked_fill_rect(const gx_device_color * pdevc,
                              int x, int y, int w, int h, gx_device * dev,
                              gs_logical_operation_t lop,
                              const gx_rop_source_t * source)
{
    gx_color_tile *ptile = pdevc->mask.m_tile;
    tile_fill_state_t state;
    int code;

    code = tile_fill_init(&state, pdevc, dev, true);
    if (code < 0)
        goto exit;
    if (state.pcdev == dev || ptile->is_simple)
        code = (*gx_dc_type_data_ht_binary.fill_rectangle)
            (pdevc, x, y, w, h, state.pcdev, lop, source);
    else {
        state.lop = lop;
        state.source = source;
        state.fill_rectangle = gx_dc_type_data_ht_binary.fill_rectangle;
        code = tile_by_steps(&state, x, y, w, h, ptile, &ptile->tmask,
                             tile_masked_fill);
    }
exit:
    if (CLIPDEV_INSTALLED) {
        tile_clip_free((gx_device_tile_clip *)state.cdev);
        state.cdev = NULL;
    }
    return code;
}

int
gx_dc_colored_masked_fill_rect(const gx_device_color * pdevc,
                               int x, int y, int w, int h, gx_device * dev,
                               gs_logical_operation_t lop,
                               const gx_rop_source_t * source)
{
    gx_color_tile *ptile = pdevc->mask.m_tile;
    tile_fill_state_t state;
    int code;

    code = tile_fill_init(&state, pdevc, dev, true);
    if (code < 0)
        goto exit;
    if (state.pcdev == dev || ptile->is_simple)
        code = (*gx_dc_type_data_ht_colored.fill_rectangle)
            (pdevc, x, y, w, h, state.pcdev, lop, source);
    else {
        state.lop = lop;
        state.source = source;
        state.fill_rectangle = gx_dc_type_data_ht_colored.fill_rectangle;
        code = tile_by_steps(&state, x, y, w, h, ptile, &ptile->tmask,
                             tile_masked_fill);
    }
exit:
    if (CLIPDEV_INSTALLED) {
        tile_clip_free((gx_device_tile_clip *)state.cdev);
        state.cdev = NULL;
    }
    return code;
}

/*
 * This is somewhat a clone of the tile_by_steps function but one
 * that performs filling from and to pdf14dev (transparency) buffers.
 * At some point it may be desirable to do some optimization here.
 */
static int
tile_by_steps_trans(tile_fill_trans_state_t * ptfs, int x0, int y0, int w0, int h0,
              gx_pattern_trans_t *fill_trans_buffer, const gx_color_tile * ptile,
              int native16)
{
    int x1 = x0 + w0, y1 = y0 + h0;
    int i0, i1, j0, j1, i, j;
    gs_matrix step_matrix;      /* translated by phase */
    gx_pattern_trans_t *ptrans_pat = ptile->ttrans;
#ifdef DEBUG
    const gs_memory_t *mem = ptile->ttrans->mem;
#endif
    int code;

    ptfs->x0 = x0, ptfs->w0 = w0;
    ptfs->y0 = y0, ptfs->h0 = h0;
    step_matrix = ptile->step_matrix;
    step_matrix.tx -= ptfs->phase.x;
    step_matrix.ty -= ptfs->phase.y;
    {
        gs_rect bbox;           /* bounding box in device space */
        gs_rect ibbox;          /* bounding box in stepping space */
        double bbw = ptile->bbox.q.x - ptile->bbox.p.x;
        double bbh = ptile->bbox.q.y - ptile->bbox.p.y;
        double u0, v0, u1, v1;

        bbox.p.x = x0, bbox.p.y = y0;
        bbox.q.x = x1, bbox.q.y = y1;
        code = gs_bbox_transform_inverse(&bbox, &step_matrix, &ibbox);
        if (code < 0)
            return code;
        if_debug10m('T', mem,
          "[T]x,y=(%d,%d) w,h=(%d,%d) => (%g,%g),(%g,%g), offset=(%g,%g)\n",
                    x0, y0, w0, h0,
                    ibbox.p.x, ibbox.p.y, ibbox.q.x, ibbox.q.y,
                    step_matrix.tx, step_matrix.ty);
        /*
         * If the pattern is partly transparent and XStep/YStep is smaller
         * than the device space BBox, we need to ensure that we cover
         * each pixel of the rectangle being filled with *every* pattern
         * that overlaps it, not just *some* pattern copy.
         */
        u0 = ibbox.p.x - max(ptile->bbox.p.x, 0) - 0.000001;
        v0 = ibbox.p.y - max(ptile->bbox.p.y, 0) - 0.000001;
        u1 = ibbox.q.x - min(ptile->bbox.q.x, 0) + 0.000001;
        v1 = ibbox.q.y - min(ptile->bbox.q.y, 0) + 0.000001;
        if (!ptile->is_simple)
            u0 -= bbw, v0 -= bbh, u1 += bbw, v1 += bbh;
        i0 = (int)fastfloor(u0);
        j0 = (int)fastfloor(v0);
        i1 = (int)ceil(u1);
        j1 = (int)ceil(v1);
    }
    if_debug4m('T', mem, "[T]i=(%d,%d) j=(%d,%d)\n", i0, i1, j0, j1);
    for (i = i0; i < i1; i++)
        for (j = j0; j < j1; j++) {
            int x = (int)fastfloor(step_matrix.xx * i +
                          step_matrix.yx * j + step_matrix.tx);
            int y = (int)fastfloor(step_matrix.xy * i +
                          step_matrix.yy * j + step_matrix.ty);
            int w = ptrans_pat->width;
            int h = ptrans_pat->height;
            int xoff, yoff;
            int px, py;

            if_debug4m('T', mem, "[T]i=%d j=%d x,y=(%d,%d)", i, j, x, y);
            if (x < x0)
                xoff = x0 - x, x = x0, w -= xoff;
            else
                xoff = 0;
            if (y < y0)
                yoff = y0 - y, y = y0, h -= yoff;
            else
                yoff = 0;
            if (x + w > x1)
                w = x1 - x;
            if (y + h > y1)
                h = y1 - y;
            if_debug6m('T', mem, "=>(%d,%d) w,h=(%d,%d) x/yoff=(%d,%d)\n",
                       x, y, w, h, xoff, yoff);
            if (w > 0 && h > 0) {

                px = imod(xoff - x, ptile->ttrans->width);
                py = imod(yoff - y, ptile->ttrans->height);

                /* Set the offsets for colored pattern fills */
                ptfs->xoff = xoff;
                ptfs->yoff = yoff;

                /* We only go through blending during tiling, if there was overlap
                   as defined by the step matrix and the bounding box.
                   Ignore if the area is outside the fill_trans_buffer.rect (bug 700719) */
                if (x > fill_trans_buffer->rect.q.x || x+w < 0 ||
                    y > fill_trans_buffer->rect.q.y || y+h < 0)
                    continue;	/* skip the fill (can breakpoint here) */
                ptile->ttrans->pat_trans_fill(x, y, x+w, y+h, px, py, ptile,
                    fill_trans_buffer, native16);
            }
        }
    return 0;
}

static void
be_rev_cpy(uint16_t *dst,const uint16_t *src,int n)
{
    for (; n != 0; n--) {
        uint16_t in = *src++;
        ((byte *)dst)[0] = in>>8;
        ((byte *)dst)[1] = in;
        dst++;
    }
}

/* This does the case of tiling with simple tiles.  Since it is not commented
 * anywhere note that simple means that the tile size is the same as the step
 * matrix size and the cross terms in the step matrix are 0.  Hence a simple
 * case of tile replication. This needs to be optimized.  */
/* Our source tile runs (conceptually) from (0,0) to
 * (ptile->ttrans->width, ptile->ttrans->height). In practise, only a limited
 * section of this (ptile->rect) may actually be used. */
void
tile_rect_trans_simple(int xmin, int ymin, int xmax, int ymax,
                       int px, int py, const gx_color_tile *ptile,
                       gx_pattern_trans_t *fill_trans_buffer,
                       int native16)
{
    int kk, jj, ii, h, w;
    int buff_out_y_offset, buff_out_x_offset;
    unsigned char *ptr_out, *ptr_in, *buff_out, *buff_in, *ptr_out_temp;
    unsigned char *row_ptr;
    int in_row_offset;
    int dx, dy;
    int left_rem_end, left_width, num_full_tiles, right_tile_width;
    int left_copy_rem_end, left_copy_width, left_copy_offset, left_copy_start;
    int mid_copy_width, right_copy_width;
    int tile_width  = ptile->ttrans->width;
    int tile_height = ptile->ttrans->height;
    int src_planes = fill_trans_buffer->n_chan + (fill_trans_buffer->has_tags ? 1 : 0);
    pdf14_buf *buf = fill_trans_buffer->buf;
    bool deep = fill_trans_buffer->deep;

    /* Update the bbox in the topmost stack entry to reflect the fact that we
     * have drawn into it. FIXME: This makes the groups too large! */
    if (buf->dirty.p.x > xmin)
        buf->dirty.p.x = xmin;
    if (buf->dirty.p.y > ymin)
        buf->dirty.p.y = ymin;
    if (buf->dirty.q.x < xmax)
        buf->dirty.q.x = xmax;
    if (buf->dirty.q.y < ymax)
        buf->dirty.q.y = ymax;
    buff_out_y_offset = ymin - fill_trans_buffer->rect.p.y;
    buff_out_x_offset = xmin - fill_trans_buffer->rect.p.x;

    buff_out = fill_trans_buffer->transbytes +
        buff_out_y_offset * fill_trans_buffer->rowstride +
        (buff_out_x_offset<<deep);

    buff_in = ptile->ttrans->transbytes;

    h = ymax - ymin;
    w = xmax - xmin;

    if (h <= 0 || w <= 0) return;

    /* Calc dx, dy within the entire (conceptual) input tile. */
    dx = (xmin + px) % tile_width;
    dy = (ymin + py) % tile_height;

    /* To speed this up, the inner loop on the width is implemented with
     * memcpys where we have a left remainder, full tiles and a right
     * remainder. Depending upon the rect that we are filling we may have
     * only one of these three portions, or two or all three.  We compute
     * the parts now outside the loops. */

    /* Left remainder part */
    left_rem_end = min(dx+w,tile_width);
    left_width = left_rem_end - dx;
    left_copy_start = max(dx,ptile->ttrans->rect.p.x);
    left_copy_rem_end = min(dx+w,ptile->ttrans->rect.q.x);
    left_copy_width = left_copy_rem_end - left_copy_start;
    if (left_copy_width < 0)
        left_copy_width = 0;
    left_copy_offset = (left_copy_start-ptile->ttrans->rect.p.x)<<deep;

    /* Now the middle part */
    num_full_tiles = (int)fastfloor((float) (w - left_width)/ (float) tile_width);
    mid_copy_width = ptile->ttrans->rect.q.x - ptile->ttrans->rect.p.x;

    /* Now the right part */
    right_tile_width = w - num_full_tiles*tile_width - left_width;
    right_copy_width = right_tile_width - ptile->ttrans->rect.p.x;
    if (right_copy_width > ptile->ttrans->rect.q.x)
        right_copy_width = ptile->ttrans->rect.q.x;
    right_copy_width -= ptile->ttrans->rect.p.x;
    if (right_copy_width < 0)
        right_copy_width = 0;

    if (deep && native16) {
        /* fill_trans_buffer is in native endian. ptile is in big endian. */
        /* Convert as we copy. */
        for (kk = 0; kk < src_planes; kk++) {

            ptr_out = buff_out + kk * fill_trans_buffer->planestride;
            ptr_in  = buff_in  + kk * ptile->ttrans->planestride;
            if (fill_trans_buffer->has_shape && kk == fill_trans_buffer->n_chan)
                ptr_out += fill_trans_buffer->planestride;	/* tag plane follows shape plane */

            for (jj = 0; jj < h; jj++, ptr_out += fill_trans_buffer->rowstride) {

                in_row_offset = ((jj + dy) % ptile->ttrans->height);
                if (in_row_offset >= ptile->ttrans->rect.q.y)
                    continue;
                in_row_offset -= ptile->ttrans->rect.p.y;
                if (in_row_offset < 0)
                    continue;
                row_ptr = ptr_in + in_row_offset * ptile->ttrans->rowstride;

                 /* This is the case when we have no blending. */
                ptr_out_temp = ptr_out;

                /* Left part */
                be_rev_cpy((uint16_t *)ptr_out_temp, (uint16_t *)(row_ptr + left_copy_offset), left_copy_width);
                ptr_out_temp += left_width<<deep;

                /* Now the full tiles */

                for ( ii = 0; ii < num_full_tiles; ii++){
                    be_rev_cpy((uint16_t *)ptr_out_temp, (uint16_t *)row_ptr, mid_copy_width);
                    ptr_out_temp += tile_width<<deep;
                }

                /* Now the remainder */
                be_rev_cpy((uint16_t *)ptr_out_temp, (uint16_t *)row_ptr, right_copy_width);
            }
        }
    } else {
        for (kk = 0; kk < src_planes; kk++) {

            ptr_out = buff_out + kk * fill_trans_buffer->planestride;
            ptr_in  = buff_in  + kk * ptile->ttrans->planestride;
            if (fill_trans_buffer->has_shape && kk == fill_trans_buffer->n_chan)
                ptr_out += fill_trans_buffer->planestride;	/* tag plane follows shape plane */

            for (jj = 0; jj < h; jj++, ptr_out += fill_trans_buffer->rowstride) {

                in_row_offset = ((jj + dy) % ptile->ttrans->height);
                if (in_row_offset >= ptile->ttrans->rect.q.y)
                    continue;
                in_row_offset -= ptile->ttrans->rect.p.y;
                if (in_row_offset < 0)
                    continue;
                row_ptr = ptr_in + in_row_offset * ptile->ttrans->rowstride;

                 /* This is the case when we have no blending. */
                ptr_out_temp = ptr_out;

                /* Left part */
                memcpy( ptr_out_temp, row_ptr + left_copy_offset, left_copy_width<<deep);
                ptr_out_temp += left_width<<deep;

                /* Now the full tiles */

                for ( ii = 0; ii < num_full_tiles; ii++){
                    memcpy( ptr_out_temp, row_ptr, mid_copy_width<<deep);
                    ptr_out_temp += tile_width<<deep;
                }

                /* Now the remainder */
                memcpy( ptr_out_temp, row_ptr, right_copy_width<<deep);
            }
        }
    }

    /* If the group we are filling has a shape plane fill that now */
    /* Note:  Since this was a virgin group push we can just blast it with
     * 255 */
    if (fill_trans_buffer->has_shape) {
        ptr_out = buff_out + fill_trans_buffer->n_chan * fill_trans_buffer->planestride;
        for (jj = 0; jj < h; jj++,ptr_out += fill_trans_buffer->rowstride) {
            memset(ptr_out, 255, w<<deep);
        }
    }
}

/* This does the case of tiling with non simple tiles.  In this case, the
 * tiles may overlap and so we really need to do blending within the existing
 * buffer.  This needs some serious optimization. */
static void
do_tile_rect_trans_blend(int xmin, int ymin, int xmax, int ymax,
                         int px, int py, const gx_color_tile *ptile,
                         gx_pattern_trans_t *fill_trans_buffer)
{
    int kk, jj, ii, h, w;
    int buff_out_y_offset, buff_out_x_offset;
    unsigned char *buff_out, *buff_in;
    unsigned char *buff_ptr, *row_ptr_in, *row_ptr_out;
    unsigned char *tile_ptr;
    int in_row_offset;
    int dx, dy;
    byte src[PDF14_MAX_PLANES];
    byte dst[PDF14_MAX_PLANES];
    int tile_width  = ptile->ttrans->width;
    int tile_height = ptile->ttrans->height;
    int num_chan    = ptile->ttrans->n_chan;  /* Includes alpha */
    int tag_offset = fill_trans_buffer->n_chan + (fill_trans_buffer->has_shape ? 1 : 0);
    pdf14_device *p14dev = (pdf14_device *) fill_trans_buffer->pdev14;

    if (fill_trans_buffer->has_tags == 0)
        tag_offset = 0;

    buff_out_y_offset = ymin - fill_trans_buffer->rect.p.y;
    buff_out_x_offset = xmin - fill_trans_buffer->rect.p.x;

    h = ymax - ymin;
    w = xmax - xmin;

    if (h <= 0 || w <= 0) return;

    /* Calc dx, dy within the entire (conceptual) input tile. */
    dx = (xmin + px) % tile_width;
    dy = (ymin + py) % tile_height;

    buff_out = fill_trans_buffer->transbytes +
        buff_out_y_offset * fill_trans_buffer->rowstride +
        buff_out_x_offset;

    buff_in = ptile->ttrans->transbytes;

    for (jj = 0; jj < h; jj++){

        in_row_offset = (jj + dy) % ptile->ttrans->height;
        if (in_row_offset >= ptile->ttrans->rect.q.y)
            continue;
        in_row_offset -= ptile->ttrans->rect.p.y;
        if (in_row_offset < 0)
            continue;
        row_ptr_in = buff_in + in_row_offset * ptile->ttrans->rowstride;

        row_ptr_out = buff_out + jj * fill_trans_buffer->rowstride;

        for (ii = 0; ii < w; ii++) {
            int x_in_offset = (dx + ii) % ptile->ttrans->width;

            if (x_in_offset >= ptile->ttrans->rect.q.x)
                continue;
            x_in_offset -= ptile->ttrans->rect.p.x;
            if (x_in_offset < 0)
                continue;
            tile_ptr = row_ptr_in + x_in_offset;
            buff_ptr = row_ptr_out + ii;

            /* We need to blend here.  The blending mode from the current
               imager state is used.
            */

            /* The color values. This needs to be optimized */
            for (kk = 0; kk < num_chan; kk++) {
                dst[kk] = *(buff_ptr + kk * fill_trans_buffer->planestride);
                src[kk] = *(tile_ptr + kk * ptile->ttrans->planestride);
            }

            /* Blend */
            art_pdf_composite_pixel_alpha_8(dst, src, ptile->ttrans->n_chan-1,
                                            ptile->blending_mode, ptile->ttrans->n_chan-1,
                                            ptile->ttrans->blending_procs, p14dev);

            /* Store the color values */
            for (kk = 0; kk < num_chan; kk++) {
                *(buff_ptr + kk * fill_trans_buffer->planestride) = dst[kk];
            }
            /* Now handle the blending of the tag. NB: dst tag_offset follows shape */
            if (tag_offset > 0) {
                int src_tag = *(tile_ptr + num_chan * ptile->ttrans->planestride);
                int dst_tag = *(buff_ptr + tag_offset * fill_trans_buffer->planestride);

                dst_tag |= src_tag;	/* simple blend combines tags */
                *(buff_ptr + tag_offset * fill_trans_buffer->planestride) = dst_tag;
            }
        }
    }

    /* If the group we are filling has a shape plane fill that now */
    /* Note:  Since this was a virgin group push we can just blast it with
     * 255 */
    if (fill_trans_buffer->has_shape) {
        buff_ptr = buff_out + fill_trans_buffer->n_chan * fill_trans_buffer->planestride;

        for (jj = 0; jj < h; jj++) {
            memset(buff_ptr, 255, w);
            buff_ptr += fill_trans_buffer->rowstride;
        }
    }
}

/* In this version, source data is big endian, dest is native endian */
static void
do_tile_rect_trans_blend_16(int xmin, int ymin, int xmax, int ymax,
                            int px, int py, const gx_color_tile *ptile,
                            gx_pattern_trans_t *fill_trans_buffer)
{
    int kk, jj, ii, h, w;
    int buff_out_y_offset, buff_out_x_offset;
    uint16_t *buff_out, *buff_in;
    uint16_t *buff_ptr, *row_ptr_in, *row_ptr_out;
    uint16_t *tile_ptr;
    int in_row_offset;
    int dx, dy;
    uint16_t src[PDF14_MAX_PLANES];
    uint16_t dst[PDF14_MAX_PLANES];
    int tile_width  = ptile->ttrans->width;
    int tile_height = ptile->ttrans->height;
    int num_chan    = ptile->ttrans->n_chan;  /* Includes alpha */
    int tag_offset = fill_trans_buffer->n_chan + (fill_trans_buffer->has_shape ? 1 : 0);
    pdf14_device *p14dev = (pdf14_device *) fill_trans_buffer->pdev14;

    if (fill_trans_buffer->has_tags == 0)
        tag_offset = 0;

    buff_out_y_offset = ymin - fill_trans_buffer->rect.p.y;
    buff_out_x_offset = xmin - fill_trans_buffer->rect.p.x;

    h = ymax - ymin;
    w = xmax - xmin;

    if (h <= 0 || w <= 0) return;

    /* Calc dx, dy within the entire (conceptual) input tile. */
    dx = (xmin + px) % tile_width;
    dy = (ymin + py) % tile_height;

    buff_out = (uint16_t *)(void *)(fill_trans_buffer->transbytes +
                                    buff_out_y_offset * fill_trans_buffer->rowstride +
                                    buff_out_x_offset*2);

    buff_in = (uint16_t *)(void *)ptile->ttrans->transbytes;

    for (jj = 0; jj < h; jj++){

        in_row_offset = (jj + dy) % ptile->ttrans->height;
        if (in_row_offset >= ptile->ttrans->rect.q.y)
            continue;
        in_row_offset -= ptile->ttrans->rect.p.y;
        if (in_row_offset < 0)
            continue;
        row_ptr_in = buff_in + in_row_offset * (ptile->ttrans->rowstride>>1);

        row_ptr_out = buff_out + jj * (fill_trans_buffer->rowstride>>1);

        for (ii = 0; ii < w; ii++) {
            int x_in_offset = (dx + ii) % ptile->ttrans->width;

            if (x_in_offset >= ptile->ttrans->rect.q.x)
                continue;
            x_in_offset -= ptile->ttrans->rect.p.x;
            if (x_in_offset < 0)
                continue;
            tile_ptr = row_ptr_in + x_in_offset;
            buff_ptr = row_ptr_out + ii;

            /* We need to blend here.  The blending mode from the current
               imager state is used.
            */

            /* Data is stored in big endian, but must be processed in native */
#define GET16_BE2NATIVE(v) \
    ((((byte *)(v))[0]<<8) | (((byte *)(v))[1]))

            /* The color values. This needs to be optimized */
            for (kk = 0; kk < num_chan; kk++) {
                dst[kk] = *(buff_ptr + kk * (fill_trans_buffer->planestride>>1));
                src[kk] = GET16_BE2NATIVE(tile_ptr + kk * (ptile->ttrans->planestride>>1));
            }

            /* Blend */
            art_pdf_composite_pixel_alpha_16(dst, src, ptile->ttrans->n_chan-1,
                                             ptile->blending_mode, ptile->ttrans->n_chan-1,
                                             ptile->ttrans->blending_procs, p14dev);

            /* Store the color values */
            for (kk = 0; kk < num_chan; kk++) {
                *(buff_ptr + kk * (fill_trans_buffer->planestride>>1)) = dst[kk];
            }
            /* Now handle the blending of the tag. NB: dst tag_offset follows shape */
            if (tag_offset > 0) {
                int src_tag = GET16_BE2NATIVE(tile_ptr + (num_chan * ptile->ttrans->planestride>>1));
                int dst_tag = *(buff_ptr + (tag_offset * fill_trans_buffer->planestride>>1));

                dst_tag |= src_tag;	/* simple blend combines tags */
                *(buff_ptr + (tag_offset * fill_trans_buffer->planestride>>1)) = dst_tag;
            }
        }
    }
#undef GET16_BE2NATIVE

    /* If the group we are filling has a shape plane fill that now */
    /* Note:  Since this was a virgin group push we can just blast it with
     * 255 */
    if (fill_trans_buffer->has_shape) {
        buff_ptr = buff_out + fill_trans_buffer->n_chan * (fill_trans_buffer->planestride>>1);

        for (jj = 0; jj < h; jj++) {
            memset(buff_ptr, 255, w*2);
            buff_ptr += fill_trans_buffer->rowstride>>1;
        }
    }
}

/* In this version, both source and dest data is big endian */
static void
do_tile_rect_trans_blend_16be(int xmin, int ymin, int xmax, int ymax,
                              int px, int py, const gx_color_tile *ptile,
                              gx_pattern_trans_t *fill_trans_buffer)
{
    int kk, jj, ii, h, w;
    int buff_out_y_offset, buff_out_x_offset;
    uint16_t *buff_out, *buff_in;
    uint16_t *buff_ptr, *row_ptr_in, *row_ptr_out;
    uint16_t *tile_ptr;
    int in_row_offset;
    int dx, dy;
    uint16_t src[PDF14_MAX_PLANES];
    uint16_t dst[PDF14_MAX_PLANES];
    int tile_width  = ptile->ttrans->width;
    int tile_height = ptile->ttrans->height;
    int num_chan    = ptile->ttrans->n_chan;  /* Includes alpha */
    int tag_offset = fill_trans_buffer->n_chan + (fill_trans_buffer->has_shape ? 1 : 0);
    pdf14_device *p14dev = (pdf14_device *) fill_trans_buffer->pdev14;

    if (fill_trans_buffer->has_tags == 0)
        tag_offset = 0;

    buff_out_y_offset = ymin - fill_trans_buffer->rect.p.y;
    buff_out_x_offset = xmin - fill_trans_buffer->rect.p.x;

    h = ymax - ymin;
    w = xmax - xmin;

    if (h <= 0 || w <= 0) return;

    /* Calc dx, dy within the entire (conceptual) input tile. */
    dx = (xmin + px) % tile_width;
    dy = (ymin + py) % tile_height;

    buff_out = (uint16_t *)(void *)(fill_trans_buffer->transbytes +
                                    buff_out_y_offset * fill_trans_buffer->rowstride +
                                    buff_out_x_offset*2);

    buff_in = (uint16_t *)(void *)ptile->ttrans->transbytes;

    for (jj = 0; jj < h; jj++){

        in_row_offset = (jj + dy) % ptile->ttrans->height;
        if (in_row_offset >= ptile->ttrans->rect.q.y)
            continue;
        in_row_offset -= ptile->ttrans->rect.p.y;
        if (in_row_offset < 0)
            continue;
        row_ptr_in = buff_in + in_row_offset * (ptile->ttrans->rowstride>>1);

        row_ptr_out = buff_out + jj * (fill_trans_buffer->rowstride>>1);

        for (ii = 0; ii < w; ii++) {
            int x_in_offset = (dx + ii) % ptile->ttrans->width;

            if (x_in_offset >= ptile->ttrans->rect.q.x)
                continue;
            x_in_offset -= ptile->ttrans->rect.p.x;
            if (x_in_offset < 0)
                continue;
            tile_ptr = row_ptr_in + x_in_offset;
            buff_ptr = row_ptr_out + ii;

            /* We need to blend here.  The blending mode from the current
               imager state is used.
            */

            /* Data is stored in big endian, but must be processed in native */
#define GET16_BE2NATIVE(v) \
    ((((byte *)(v))[0]<<8) | (((byte *)(v))[1]))
#define PUT16_NATIVE2BE(p,v) \
    ((((byte *)(p))[0] = v>>8), (((byte *)(p))[1] = v))

            /* The color values. This needs to be optimized */
            for (kk = 0; kk < num_chan; kk++) {
                dst[kk] = GET16_BE2NATIVE(buff_ptr + kk * (fill_trans_buffer->planestride>>1));
                src[kk] = GET16_BE2NATIVE(tile_ptr + kk * (ptile->ttrans->planestride>>1));
            }

            /* Blend */
            art_pdf_composite_pixel_alpha_16(dst, src, ptile->ttrans->n_chan-1,
                                             ptile->blending_mode, ptile->ttrans->n_chan-1,
                                             ptile->ttrans->blending_procs, p14dev);

            /* Store the color values */
            for (kk = 0; kk < num_chan; kk++) {
                PUT16_NATIVE2BE(buff_ptr + kk * (fill_trans_buffer->planestride>>1), dst[kk]);
            }
            /* Now handle the blending of the tag. NB: dst tag_offset follows shape */
            if (tag_offset > 0) {
                int src_tag = GET16_BE2NATIVE(tile_ptr + (num_chan * ptile->ttrans->planestride>>1));
                int dst_tag = GET16_BE2NATIVE(buff_ptr + (tag_offset * fill_trans_buffer->planestride>>1));

                dst_tag |= src_tag;	/* simple blend combines tags */
                PUT16_NATIVE2BE(buff_ptr + (tag_offset * fill_trans_buffer->planestride>>1), dst_tag);
            }
        }
    }

    /* If the group we are filling has a shape plane fill that now */
    /* Note:  Since this was a virgin group push we can just blast it with
     * 255 */
    if (fill_trans_buffer->has_shape) {
        buff_ptr = buff_out + fill_trans_buffer->n_chan * (fill_trans_buffer->planestride>>1);

        for (jj = 0; jj < h; jj++) {
            memset(buff_ptr, 255, w*2);
            buff_ptr += fill_trans_buffer->rowstride>>1;
        }
    }
}

void
tile_rect_trans_blend(int xmin, int ymin, int xmax, int ymax,
                      int px, int py, const gx_color_tile *ptile,
                      gx_pattern_trans_t *fill_trans_buffer,
                      int native16)
{
    pdf14_buf *buf = fill_trans_buffer->buf;

    /* Update the bbox in the topmost stack entry to reflect the fact that we
     * have drawn into it. FIXME: This makes the groups too large! */
    if (buf->dirty.p.x > xmin)
        buf->dirty.p.x = xmin;
    if (buf->dirty.p.y > ymin)
        buf->dirty.p.y = ymin;
    if (buf->dirty.q.x < xmax)
        buf->dirty.q.x = xmax;
    if (buf->dirty.q.y < ymax)
        buf->dirty.q.y = ymax;

    if (!ptile->ttrans->deep)
        do_tile_rect_trans_blend(xmin, ymin, xmax, ymax,
                                 px, py, ptile, fill_trans_buffer);
    else if (native16)
        do_tile_rect_trans_blend_16(xmin, ymin, xmax, ymax,
                                    px, py, ptile, fill_trans_buffer);
    else
        do_tile_rect_trans_blend_16be(xmin, ymin, xmax, ymax,
                                      px, py, ptile, fill_trans_buffer);
}

/* This version does a rect fill with the transparency object */
int
gx_dc_pat_trans_fill_rectangle(const gx_device_color * pdevc, int x, int y,
                               int w, int h, gx_device * dev,
                               gs_logical_operation_t lop,
                               const gx_rop_source_t * source)
{
    gx_color_tile *ptile = pdevc->colors.pattern.p_tile;
    int code;
    gs_int_point phase;
    const gx_rop_source_t *rop_source = source;
    gx_rop_source_t no_source;

    if (ptile == 0)             /* null pattern */
        return 0;
    if (rop_source == NULL)
        set_rop_no_source(rop_source, no_source, dev);

    phase.x = pdevc->phase.x;
    phase.y = pdevc->phase.y;

#if 0
    if_debug8m('v', ptile->ttrans->mem,
            "[v]gx_dc_pat_trans_fill_rectangle, Fill: (%d, %d), %d x %d To Buffer: (%d, %d), %d x %d \n",
            x, y, w, h,  ptile->ttrans->fill_trans_buffer->rect.p.x,
            ptile->ttrans->fill_trans_buffer->rect.p.y,
            ptile->ttrans->fill_trans_buffer->rect.q.x -
            ptile->ttrans->fill_trans_buffer->rect.p.x,
            ptile->ttrans->fill_trans_buffer->rect.q.y -
            ptile->ttrans->fill_trans_buffer->rect.p.y);
#endif
    code = gx_trans_pattern_fill_rect(x, y, x+w, y+h, ptile,
                                      ptile->ttrans->fill_trans_buffer, phase,
                                      dev, pdevc, 0);
    return code;
}

/* This fills the transparency buffer rectangles with a pattern buffer
   that includes transparency */
int
gx_trans_pattern_fill_rect(int xmin, int ymin, int xmax, int ymax,
                           gx_color_tile *ptile,
                           gx_pattern_trans_t *fill_trans_buffer,
                           gs_int_point phase, gx_device *dev,
                           const gx_device_color * pdevc,
                           int native16)
{
    tile_fill_trans_state_t state_trans;
    tile_fill_state_t state_clist_trans;
    int code = 0;
    int w = xmax - xmin;
    int h = ymax - ymin;

    if (ptile == 0)             /* null pattern */
        return 0;

    fit_fill_xywh(dev, xmin, ymin, w, h);
    if (w < 0 || h < 0)
        return 0;
    xmax = w + xmin;
    ymax = h + ymin;

    /* Initialize the fill state */
    state_trans.phase.x = phase.x;
    state_trans.phase.y = phase.y;

    if (ptile->is_simple && ptile->cdev == NULL) {
        /* A simple case.  Tile is not clist and simple. */
        int px =
            imod(-(int)fastfloor(ptile->step_matrix.tx - phase.x + 0.5),
                  ptile->ttrans->width);
        int py =
            imod(-(int)fastfloor(ptile->step_matrix.ty - phase.y + 0.5),
                 ptile->ttrans->height);

        tile_rect_trans_simple(xmin, ymin, xmax, ymax, px, py, ptile,
            fill_trans_buffer, native16);
    } else {
        if (ptile->cdev == NULL) {
            /* No clist for the pattern, but a complex case
               This portion transforms the bounding box by the step matrix
               and does partial rect fills with tiles that fall into this
               transformed bbox */
            code = tile_by_steps_trans(&state_trans, xmin, ymin, xmax-xmin,
                                        ymax-ymin, fill_trans_buffer, ptile,
                                        native16);

        } else {
            /* clist for the trans tile.  This uses the pdf14 device as a target
               and should blend directly into the buffer.  Note that the
               pattern can not have a push pdf14 device or a pop pdf14 device
               compositor action.  Those are removed during the compositor
               clist writing operation where we check for the case of a pattern
               with a transparency */
            gx_device_clist *cdev = ptile->cdev;
            gx_device_clist_reader *crdev = (gx_device_clist_reader *)cdev;
            gx_strip_bitmap tbits;

            code = tile_fill_init(&state_clist_trans, pdevc, dev, false);
            if (code < 0)
            {
                /* Can't use CLIPDEV_INSTALLED macro because it assumes a tile_fill_state_t structure, called 'state' */
                if (state_clist_trans.cdev != NULL) {
                    tile_clip_free((gx_device_tile_clip *)state_clist_trans.cdev);
                    state_clist_trans.cdev = NULL;
                }
                return code;
            }

            state_clist_trans.phase.x = phase.x;
            state_clist_trans.phase.y = phase.y;
            crdev->yplane.depth = 0;
            crdev->yplane.shift = 0;
            crdev->yplane.index = -1;
            crdev->pages = NULL;
            crdev->num_pages = 1;
            state_clist_trans.orig_dev = dev;
            state_clist_trans.pdevc = pdevc;
            tbits = ptile->tbits;
            tbits.size.x = crdev->width;
            tbits.size.y = crdev->height;
            if (code >= 0)
                code = tile_by_steps(&state_clist_trans, xmin, ymin, xmax-xmin,
                                     ymax-ymin, ptile, &tbits, tile_pattern_clist);

            if (code >= 0 && (state_clist_trans.cdev != NULL)) {
                tile_clip_free((gx_device_tile_clip *)state_clist_trans.cdev);
                state_clist_trans.cdev = NULL;
            }

        }
    }
    return code;
}
