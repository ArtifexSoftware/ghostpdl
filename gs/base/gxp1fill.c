/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* PatternType 1 filling algorithms */
#include "string_.h"
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsrop.h"
#include "gsmatrix.h"
#include "gxcspace.h"		/* for gscolor2.h */
#include "gxcolor2.h"
#include "gxdcolor.h"
#include "gxdevcli.h"
#include "gxdevmem.h"
#include "gxclip2.h"
#include "gxpcolor.h"
#include "gxp1impl.h"
#include "gxcldev.h"
#include "gxblend.h"

/* Define the state for tile filling. */
typedef struct tile_fill_state_s {

    /* Original arguments */

    const gx_device_color *pdevc;	/* pattern color */
    int x0, y0, w0, h0;
    gs_logical_operation_t lop;
    const gx_rop_source_t *source;

    /* Variables set at initialization */

    gx_device_tile_clip cdev;
    gx_device *pcdev;		/* original device or &cdev */
    const gx_strip_bitmap *tmask;
    gs_int_point phase;

    /* Following are only for uncolored patterns */

    dev_color_proc_fill_rectangle((*fill_rectangle));

    /* Following are only for colored patterns */

    const gx_rop_source_t *rop_source;
    gx_device *orig_dev;
    int xoff, yoff;		/* set dynamically */

} tile_fill_state_t;


/* Define the state for tile filling.
   This is used for when we have
   transparency */
typedef struct tile_fill_trans_state_s {

    /* Original arguments */

    const gx_device_color *pdevc;	/* pattern color */
    int x0, y0, w0, h0;

    /* Variables set at initialization */

    gx_device *pcdev;		/* original device or &cdev */
    gs_int_point phase;

    int xoff, yoff;		/* set dynamically */

} tile_fill_trans_state_t;

/* Initialize the filling state. */
static int
tile_fill_init(tile_fill_state_t * ptfs, const gx_device_color * pdevc,
	       gx_device * dev, bool set_mask_phase)
{
    gx_color_tile *m_tile = pdevc->mask.m_tile;
    int px, py;

    ptfs->pdevc = pdevc;
    if (m_tile == 0) {		/* no clipping */
	ptfs->pcdev = dev;
	ptfs->phase = pdevc->phase;
	return 0;
    }
    ptfs->pcdev = (gx_device *) & ptfs->cdev;
    ptfs->tmask = &m_tile->tmask;
    ptfs->phase.x = pdevc->mask.m_phase.x;
    ptfs->phase.y = pdevc->mask.m_phase.y;
    /*
     * For non-simple tiles, the phase will be reset on each pass of the
     * tile_by_steps loop, but for simple tiles, we must set it now.
     */
    if (set_mask_phase && m_tile->is_simple) {
	px = imod(-(int)floor(m_tile->step_matrix.tx - ptfs->phase.x + 0.5),
		  m_tile->tmask.rep_width);
	py = imod(-(int)floor(m_tile->step_matrix.ty - ptfs->phase.y + 0.5),
		  m_tile->tmask.rep_height);
    } else
	px = py = 0;
    return tile_clip_initialize(&ptfs->cdev, ptfs->tmask, dev, px, py, NULL); 
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
    gs_matrix step_matrix;	/* translated by phase */
    int code;

    ptfs->x0 = x0, ptfs->w0 = w0;
    ptfs->y0 = y0, ptfs->h0 = h0;
    step_matrix = ptile->step_matrix;
    step_matrix.tx -= ptfs->phase.x;
    step_matrix.ty -= ptfs->phase.y;
    {
	gs_rect bbox;		/* bounding box in device space */
	gs_rect ibbox;		/* bounding box in stepping space */
	double bbw = ptile->bbox.q.x - ptile->bbox.p.x;
	double bbh = ptile->bbox.q.y - ptile->bbox.p.y;
	double u0, v0, u1, v1;

	bbox.p.x = x0, bbox.p.y = y0;
	bbox.q.x = x1, bbox.q.y = y1;
	gs_bbox_transform_inverse(&bbox, &step_matrix, &ibbox);
	if_debug10('T',
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
	i0 = (int)floor(u0);
	j0 = (int)floor(v0);
	i1 = (int)ceil(u1);
	j1 = (int)ceil(v1);
    }
    if_debug4('T', "[T]i=(%d,%d) j=(%d,%d)\n", i0, i1, j0, j1);
    for (i = i0; i < i1; i++)
	for (j = j0; j < j1; j++) {
	    int x = (int)floor(step_matrix.xx * i +
			  step_matrix.yx * j + step_matrix.tx);
	    int y = (int)floor(step_matrix.xy * i +
			  step_matrix.yy * j + step_matrix.ty);
	    int w = tbits_or_tmask->size.x;
	    int h = tbits_or_tmask->size.y;
	    int xoff, yoff;

	    if_debug4('T', "[T]i=%d j=%d x,y=(%d,%d)", i, j, x, y);
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
	    if_debug6('T', "=>(%d,%d) w,h=(%d,%d) x/yoff=(%d,%d)\n",
		      x, y, w, h, xoff, yoff);
	    if (w > 0 && h > 0) {
		if (ptfs->pcdev == (gx_device *) & ptfs->cdev)
		    tile_clip_set_phase(&ptfs->cdev,
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
    const gx_rop_source_t *rop_source = ptfs->rop_source;
    gx_device *dev = ptfs->orig_dev;
    int xoff = ptfs->xoff, yoff = ptfs->yoff;
    gx_strip_bitmap *bits = &ptile->tbits;
    const byte *data = bits->data;
    bool full_transfer = (w == ptfs->w0 && h == ptfs->h0);
    gx_bitmap_id source_id =
    (full_transfer ? rop_source->id : gx_no_bitmap_id);
    int code;

    if (source == NULL && lop_no_S_is_T(lop))
	code = (*dev_proc(ptfs->pcdev, copy_color))
	    (ptfs->pcdev, data + bits->raster * yoff, xoff,
	     bits->raster,
	     (full_transfer ? bits->id : gx_no_bitmap_id),
	     x, y, w, h);
    else {
	gx_strip_bitmap data_tile;

	data_tile.data = (byte *) data;		/* actually const */
	data_tile.raster = bits->raster;
	data_tile.size.x = data_tile.rep_width = ptile->tbits.size.x;
	data_tile.size.y = data_tile.rep_height = ptile->tbits.size.y;
	data_tile.id = bits->id;
	data_tile.shift = data_tile.rep_shift = 0;
	code = (*dev_proc(dev, strip_copy_rop))
	    (dev,
	     rop_source->sdata + (y - ptfs->y0) * rop_source->sraster,
	     rop_source->sourcex + (x - ptfs->x0),
	     rop_source->sraster, source_id,
	     (rop_source->use_scolors ? rop_source->scolors : NULL),
	     &data_tile, NULL,
	     x, y, w, h,
	     imod(xoff - x, data_tile.rep_width),
	     imod(yoff - y, data_tile.rep_height),
	     lop);
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
    crdev->page_info.io_procs->rewind(crdev->page_info.bfile, false, NULL);
    crdev->page_info.io_procs->rewind(crdev->page_info.cfile, false, NULL);

    if_debug0('L', "Pattern clist playback begin\n");
    code = clist_playback_file_bands(playback_action_render,
		crdev, &crdev->page_info, dev, 0, 0, ptfs->xoff - x, ptfs->yoff - y);
    if_debug0('L', "Pattern clist playback end\n");
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

    if (ptile == 0)		/* null pattern */
	return 0;
    if (rop_source == NULL)
	set_rop_no_source(rop_source, no_source, dev);
    bits = &ptile->tbits;

    state.cdev.finalize = 0;

    code = tile_fill_init(&state, pdevc, dev, false);
    if (code < 0)
	return code;
    if (ptile->is_simple && ptile->cdev == NULL) {
	int px =
	    imod(-(int)floor(ptile->step_matrix.tx - state.phase.x + 0.5),
		 bits->rep_width);
	int py =
	    imod(-(int)floor(ptile->step_matrix.ty - state.phase.y + 0.5),
		 bits->rep_height);

	if (state.pcdev != dev)
	    tile_clip_set_phase(&state.cdev, px, py);
	if (source == NULL && lop_no_S_is_T(lop))
	    code = (*dev_proc(state.pcdev, strip_tile_rectangle))
		(state.pcdev, bits, x, y, w, h,
		 gx_no_color_index, gx_no_color_index, px, py);
	else
	    code = (*dev_proc(state.pcdev, strip_copy_rop))
		(state.pcdev,
		 rop_source->sdata, rop_source->sourcex,
		 rop_source->sraster, rop_source->id,
		 (rop_source->use_scolors ? rop_source->scolors : NULL),
		 bits, NULL, x, y, w, h, px, py, lop);
    } else {
	state.lop = lop;
	state.source = source;
	state.rop_source = rop_source;
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
    if(state.cdev.finalize)
	state.cdev.finalize((gx_device *)&state.cdev);
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
	return code;
    if (state.pcdev == dev || ptile->is_simple)
	return (*gx_dc_type_data_pure.fill_rectangle)
	    (pdevc, x, y, w, h, state.pcdev, lop, source);
    else {
	state.lop = lop;
	state.source = source;
	state.fill_rectangle = gx_dc_type_data_pure.fill_rectangle;
	return tile_by_steps(&state, x, y, w, h, ptile, &ptile->tmask,
			     tile_masked_fill);
    }
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
	return code;
    if (state.pcdev == dev || ptile->is_simple)
	return (*gx_dc_type_data_ht_binary.fill_rectangle)
	    (pdevc, x, y, w, h, state.pcdev, lop, source);
    else {
	state.lop = lop;
	state.source = source;
	state.fill_rectangle = gx_dc_type_data_ht_binary.fill_rectangle;
	return tile_by_steps(&state, x, y, w, h, ptile, &ptile->tmask,
			     tile_masked_fill);
    }
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
	return code;
    if (state.pcdev == dev || ptile->is_simple)
	return (*gx_dc_type_data_ht_colored.fill_rectangle)
	    (pdevc, x, y, w, h, state.pcdev, lop, source);
    else {
	state.lop = lop;
	state.source = source;
	state.fill_rectangle = gx_dc_type_data_ht_colored.fill_rectangle;
	return tile_by_steps(&state, x, y, w, h, ptile, &ptile->tmask,
			     tile_masked_fill);
    }
}


/*
 * This is somewhat a clone of the tile_by_steps function but one
 * that performs filling from and to pdf14dev (transparency) buffers.
 * At some point it may be desirable to do some optimization here.
 */
static int
tile_by_steps_trans(tile_fill_trans_state_t * ptfs, int x0, int y0, int w0, int h0,
	      gx_pattern_trans_t *fill_trans_buffer, const gx_color_tile * ptile)
{
    int x1 = x0 + w0, y1 = y0 + h0;
    int i0, i1, j0, j1, i, j;
    gs_matrix step_matrix;	/* translated by phase */
    gx_pattern_trans_t *ptrans_pat = ptile->ttrans;

    ptfs->x0 = x0, ptfs->w0 = w0;
    ptfs->y0 = y0, ptfs->h0 = h0;
    step_matrix = ptile->step_matrix;
    step_matrix.tx -= ptfs->phase.x;
    step_matrix.ty -= ptfs->phase.y;
    {
	gs_rect bbox;		/* bounding box in device space */
	gs_rect ibbox;		/* bounding box in stepping space */
	double bbw = ptile->bbox.q.x - ptile->bbox.p.x;
	double bbh = ptile->bbox.q.y - ptile->bbox.p.y;
	double u0, v0, u1, v1;

	bbox.p.x = x0, bbox.p.y = y0;
	bbox.q.x = x1, bbox.q.y = y1;
	gs_bbox_transform_inverse(&bbox, &step_matrix, &ibbox);
	if_debug10('T',
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
	i0 = (int)floor(u0);
	j0 = (int)floor(v0);
	i1 = (int)ceil(u1);
	j1 = (int)ceil(v1);
    }
    if_debug4('T', "[T]i=(%d,%d) j=(%d,%d)\n", i0, i1, j0, j1);
    for (i = i0; i < i1; i++)
	for (j = j0; j < j1; j++) {
	    int x = (int)floor(step_matrix.xx * i +
			  step_matrix.yx * j + step_matrix.tx);
	    int y = (int)floor(step_matrix.xy * i +
			  step_matrix.yy * j + step_matrix.ty);
	    int w = ptrans_pat->width;
	    int h = ptrans_pat->height;
	    int xoff, yoff;
            int px, py;

	    if_debug4('T', "[T]i=%d j=%d x,y=(%d,%d)", i, j, x, y);
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
	    if_debug6('T', "=>(%d,%d) w,h=(%d,%d) x/yoff=(%d,%d)\n",
		      x, y, w, h, xoff, yoff);
	    if (w > 0 && h > 0) {

                px = imod(xoff - x, ptile->ttrans->width);
                py = imod(yoff - y, ptile->ttrans->height);

		/* Set the offsets for colored pattern fills */		
                ptfs->xoff = xoff;		
                ptfs->yoff = yoff;                
                
                /* We only go through blending during tiling, if
                   there was overlap as defined by the step matrix
                   and the bounding box */

                ptile->ttrans->pat_trans_fill(x, y, x+w, y+h, px, py, ptile,
                        fill_trans_buffer);	                
            }
	}
    return 0;
}

/* This does the case of tiling with simple tiles.  Since it is not commented anywhere note 
   that simple means that the tile size is the same as the step matrix size and the cross
   terms in the step matrix are 0.  Hence a simple case of tile replication. This needs to be optimized.  */
void 
tile_rect_trans_simple(int xmin, int ymin, int xmax, int ymax, int px, int py, const gx_color_tile *ptile,
            gx_pattern_trans_t *fill_trans_buffer)
{
    int kk, jj, ii, h, w, buff_y_offset, buff_x_offset;
    unsigned char *ptr_out, *ptr_in, *buff_out, *buff_in, *ptr_out_temp;
    unsigned char *row_ptr;
    int in_row_offset;
    int tile_width = ptile->ttrans->width;
    int tile_height = ptile->ttrans->height;
    int dx, dy;
    int left_rem_end, left_width, num_full_tiles, right_tile_width;  

    buff_y_offset = ymin - fill_trans_buffer->rect.p.y;
    buff_x_offset = xmin - fill_trans_buffer->rect.p.x;

    buff_out = fill_trans_buffer->transbytes + 
        buff_y_offset * fill_trans_buffer->rowstride + 
        buff_x_offset;

    buff_in = ptile->ttrans->transbytes;

    h = ymax - ymin;
    w = xmax - xmin;

    dx = (xmin + px) % tile_width;
    dy = (ymin + py) % tile_height;

    /* To speed this up, the inner loop on the width is implemented with
       mem copys where we have a left remainder, full tiles and a right remainder.
       Depending upon the rect that we are filling we may have only one of these
       three portions, or two or all three.  We compute the parts now outside the loops. */

    /* Left remainder part */
    
    left_rem_end = min(dx+w,tile_width);
    left_width = left_rem_end - dx;

    /* Now the middle part */

    num_full_tiles = (int) floor((float) (w - left_width)/ (float) tile_width);

    /* Now the right part */

    right_tile_width = w - num_full_tiles*tile_width - left_width;


    for (kk = 0; kk < fill_trans_buffer->n_chan; kk++){

        ptr_out = buff_out + kk * fill_trans_buffer->planestride;
        ptr_in = buff_in + kk * ptile->ttrans->planestride;

        for (jj = 0; jj < h; jj++){   

            in_row_offset = (jj + dy) % ptile->ttrans->height;
            row_ptr = ptr_in + in_row_offset * ptile->ttrans->rowstride;

             /* This is the case when we have no blending. */

            ptr_out_temp = ptr_out;

            /* Left part */

            memcpy( ptr_out_temp, row_ptr + dx, left_width);
            ptr_out_temp += left_width;

            /* Now the full tiles */

            for ( ii = 0; ii < num_full_tiles; ii++){

                memcpy( ptr_out_temp, row_ptr, tile_width);
                ptr_out_temp += tile_width;

            }

            /* Now the remainder */

            memcpy( ptr_out_temp, row_ptr, right_tile_width);

            ptr_out += fill_trans_buffer->rowstride;
          
        }

    }

    /* If the group we are filling has a shape plane fill that now */
    /* Note:  Since this was a virgin group push we can just blast it with 255 */

    if (fill_trans_buffer->has_shape) {

        ptr_out = buff_out + fill_trans_buffer->n_chan * fill_trans_buffer->planestride;

        for (jj = 0; jj < h; jj++){   

            memset(ptr_out, 255, w);
            ptr_out += fill_trans_buffer->rowstride;

        }

    }

}


/* This does the case of tiling with non simple tiles.  In this case, the tiles may overlap and
   so we really need to do blending within the existing buffer.  This needs some serious optimization. */
void 
tile_rect_trans_blend(int xmin, int ymin, int xmax, int ymax, int px, int py, const gx_color_tile *ptile,
            gx_pattern_trans_t *fill_trans_buffer)
{
    int kk, jj, ii, h, w, buff_y_offset, buff_x_offset;
    unsigned char *buff_out, *buff_in;
    unsigned char *buff_ptr, *row_ptr_in, *row_ptr_out;
    unsigned char *tile_ptr;
    int in_row_offset;
    int tile_width = ptile->ttrans->width;
    int tile_height = ptile->ttrans->height;
    int dx, dy;
    byte src[PDF14_MAX_PLANES];
    byte dst[PDF14_MAX_PLANES];
    int num_chan = ptile->ttrans->n_chan;  /* Includes alpha */

    buff_y_offset = ymin - fill_trans_buffer->rect.p.y;
    buff_x_offset = xmin - fill_trans_buffer->rect.p.x;

    buff_out = fill_trans_buffer->transbytes + 
        buff_y_offset * fill_trans_buffer->rowstride + 
        buff_x_offset;

    buff_in = ptile->ttrans->transbytes;

    h = ymax - ymin;
    w = xmax - xmin;

    dx = (xmin + px) % tile_width;
    dy = (ymin + py) % tile_height;

    for (jj = 0; jj < h; jj++){   

        in_row_offset = (jj + dy) % ptile->ttrans->height;
        row_ptr_in = buff_in + in_row_offset * ptile->ttrans->rowstride;

        row_ptr_out = buff_out + jj * fill_trans_buffer->rowstride;

        for (ii = 0; ii < w; ii++) {
           
            tile_ptr = row_ptr_in + (dx + ii) % ptile->ttrans->width;
            buff_ptr = row_ptr_out + ii; 

            /* We need to blend here.  The blending mode from the current
               imager state is used. 
            */
            
            /* The color values. This needs to be optimized */

            for (kk = 0; kk < num_chan; kk++){

                dst[kk] = *(buff_ptr + kk * fill_trans_buffer->planestride);
                src[kk] = *(tile_ptr + kk * ptile->ttrans->planestride);

            } 

            /* Blend */

           art_pdf_composite_pixel_alpha_8(dst, src, ptile->ttrans->n_chan-1,
			   		 ptile->ttrans->blending_mode, ptile->ttrans->blending_procs);

            /* Store the color values */

            for (kk = 0; kk < num_chan; kk++){

                *(buff_ptr + kk * fill_trans_buffer->planestride) = dst[kk];

            } 
  
        }
     
    }

    /* If the group we are filling has a shape plane fill that now */
    /* Note:  Since this was a virgin group push we can just blast it with 255 */

    if (fill_trans_buffer->has_shape) {

        buff_ptr = buff_out + fill_trans_buffer->n_chan * fill_trans_buffer->planestride;

        for (jj = 0; jj < h; jj++){   

            memset(buff_ptr, 255, w);
            buff_ptr += fill_trans_buffer->rowstride;

        }

    }

}



/* This fills the transparency buffer rectangles with a pattern 
   buffer that includes transparency */

int 
gx_trans_pattern_fill_rect(int xmin, int ymin, int xmax, int ymax, gx_color_tile *ptile, 
                               gx_pattern_trans_t *fill_trans_buffer, gs_int_point phase)
{

    tile_fill_trans_state_t state;
    int code;

    if (ptile == 0)		/* null pattern */
	return 0;

    /* Initialize the fill state */

    state.phase.x = phase.x;
    state.phase.y = phase.y;

    if (ptile->is_simple && ptile->cdev == NULL) {
        
        /* A simple case.  Tile is not clist and simple. */

	int px =
	    imod(-(int)floor(ptile->step_matrix.tx - state.phase.x + 0.5),
		  ptile->ttrans->width);
	int py =
	    imod(-(int)floor(ptile->step_matrix.ty - state.phase.y + 0.5),
		 ptile->ttrans->height);

        tile_rect_trans_simple(xmin, ymin, xmax, ymax, px, py, ptile,
            fill_trans_buffer);

    } else {

        if (ptile->cdev == NULL) {

            /* No clist for the pattern, but a complex case
               This portion transforms the bounding box by the step matrix
               and does partial rect fills with tiles that fall into this 
               transformed bbox */
            
            code = tile_by_steps_trans(&state, xmin, ymin, xmax-xmin, ymax-ymin, 
                fill_trans_buffer, ptile);

	} else {

            /* clist for the tile.  Currently this is not implemented
               for the case when the tile has transparency.  This is
               on the to do list.  Right now, all tiles with transparency
               are rendered into the pattern cache or into the clist
               */
	    return_error(gs_error_unregistered);

	}
    }

return(0);

}
