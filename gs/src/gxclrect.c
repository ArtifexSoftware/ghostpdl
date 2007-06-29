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
/* Rectangle-oriented command writing for command list */
#include "gx.h"
#include "gserrors.h"
#include "gsutil.h"		/* for gs_next_ids */
#include "gxdevice.h"
#include "gxdevmem.h"		/* must precede gxcldev.h */
#include "gxcldev.h"

/* ---------------- Writing utilities ---------------- */

#define cmd_set_rect(rect)\
  ((rect).x = x, (rect).y = y,\
   (rect).width = width, (rect).height = height)

/* Write a rectangle. */
private int
cmd_size_rect(register const gx_cmd_rect * prect)
{
    return
	cmd_sizew(prect->x) + cmd_sizew(prect->y) +
	cmd_sizew(prect->width) + cmd_sizew(prect->height);
}
private byte *
cmd_put_rect(register const gx_cmd_rect * prect, register byte * dp)
{
    cmd_putw(prect->x, dp);
    cmd_putw(prect->y, dp);
    cmd_putw(prect->width, dp);
    cmd_putw(prect->height, dp);
    return dp;
}

int
cmd_write_rect_cmd(gx_device_clist_writer * cldev, gx_clist_state * pcls,
		   int op, int x, int y, int width, int height)
{
    int dx = x - pcls->rect.x;
    int dy = y - pcls->rect.y;
    int dwidth = width - pcls->rect.width;
    int dheight = height - pcls->rect.height;
    byte *dp;
    int code;

#define check_range_xy(rmin, rmax)\
  ((unsigned)(dx - rmin) <= (rmax - rmin) &&\
   (unsigned)(dy - rmin) <= (rmax - rmin))
#define check_range_w(rmin, rmax)\
  ((unsigned)(dwidth - rmin) <= (rmax - rmin))
#define check_ranges(rmin, rmax)\
  (check_range_xy(rmin, rmax) && check_range_w(rmin, rmax) &&\
   (unsigned)(dheight - rmin) <= (rmax - rmin))
    cmd_set_rect(pcls->rect);
    if (dheight == 0 && check_range_w(cmd_min_dw_tiny, cmd_max_dw_tiny) &&
	check_range_xy(cmd_min_dxy_tiny, cmd_max_dxy_tiny)
	) {
	byte op_tiny = op + 0x20 + dwidth - cmd_min_dw_tiny;

	if (dx == width - dwidth && dy == 0) {
	    code = set_cmd_put_op(dp, cldev, pcls, op_tiny + 8, 1);
	    if (code < 0)
		return code;
	} else {
	    code = set_cmd_put_op(dp, cldev, pcls, op_tiny, 2);
	    if (code < 0)
		return code;
	    dp[1] = (dx << 4) + dy - (cmd_min_dxy_tiny * 0x11);
	}
    }
#define rmin cmd_min_short
#define rmax cmd_max_short
    else if (check_ranges(rmin, rmax)) {
	int dh = dheight - cmd_min_dxy_tiny;

	if ((unsigned)dh <= cmd_max_dxy_tiny - cmd_min_dxy_tiny &&
	    dh != 0 && dy == 0
	    ) {
	    op += dh;
	    code = set_cmd_put_op(dp, cldev, pcls, op + 0x10, 3);
	    if (code < 0)
		return code;
	    if_debug3('L', "    rs2:%d,%d,0,%d\n",
		      dx, dwidth, dheight);
	} else {
	    code = set_cmd_put_op(dp, cldev, pcls, op + 0x10, 5);
	    if (code < 0)
		return code;
	    if_debug4('L', "    rs4:%d,%d,%d,%d\n",
		      dx, dwidth, dy, dheight);
	    dp[3] = dy - rmin;
	    dp[4] = dheight - rmin;
	}
	dp[1] = dx - rmin;
	dp[2] = dwidth - rmin;
    }
#undef rmin
#undef rmax
    else if (dy >= -2 && dy <= 1 && dheight >= -2 && dheight <= 1 &&
	     (dy + dheight) != -4
	) {
	int rcsize = 1 + cmd_sizew(x) + cmd_sizew(width);

	code = set_cmd_put_op(dp, cldev, pcls,
			      op + ((dy + 2) << 2) + dheight + 2, rcsize);
	if (code < 0)
	    return code;
	++dp;
	cmd_put2w(x, width, dp);
    } else {
	int rcsize = 1 + cmd_size_rect(&pcls->rect);

	code = set_cmd_put_op(dp, cldev, pcls, op, rcsize);
	if (code < 0)
	    return code;
	if_debug5('L', "    r%d:%d,%d,%d,%d\n",
		  rcsize - 1, dx, dwidth, dy, dheight);
	cmd_put_rect(&pcls->rect, dp + 1);
    }
    return 0;
}

/* ---------------- Driver procedures ---------------- */

int
clist_fill_rectangle(gx_device * dev, int rx, int ry, int rwidth, int rheight,
		     gx_color_index color)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int code;
    cmd_rects_enum_t re;

    fit_fill(dev, rx, ry, rwidth, rheight);
    if (cdev->permanent_error < 0)
      return (cdev->permanent_error);
    FOR_RECTS(re, ry, rheight) {
	re.pcls->colors_used.or |= color;
	re.pcls->band_complexity.uses_color |= ((color != 0xffffff) && (color != 0)); 
	do {
	    code = cmd_disable_lop(cdev, re.pcls);
	    if (code >= 0 && color != re.pcls->colors[1])
		code = cmd_put_color(cdev, re.pcls, &clist_select_color1,
				     color, &re.pcls->colors[1]);
	    if (code >= 0)
		code = cmd_write_rect_cmd(cdev, re.pcls, cmd_op_fill_rect, rx, re.y,
					  rwidth, re.height);
	} while (RECT_RECOVER(code));
	/* HANDLE_RECT_UNLESS(code, 0); */
	if (code < 0 && SET_BAND_CODE(code))
	    goto error_in_rect;
    } END_RECTS;
    return 0;
}

int
clist_strip_tile_rectangle(gx_device * dev, const gx_strip_bitmap * tile,
			   int rx, int ry, int rwidth, int rheight,
	       gx_color_index color0, gx_color_index color1, int px, int py)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int depth =
	(color1 == gx_no_color_index && color0 == gx_no_color_index ?
	 dev->color_info.depth : 1);
    gx_color_index colors_used =
	(color1 == gx_no_color_index && color0 == gx_no_color_index ?
	 /* We can't know what colors will be used: assume the worst. */
	 ((gx_color_index)1 << depth) - 1 :
	 (color0 == gx_no_color_index ? 0 : color0) |
	 (color1 == gx_no_color_index ? 0 : color1));
    int code;
    cmd_rects_enum_t re;

    fit_fill(dev, rx, ry, rwidth, rheight);
    if (cdev->permanent_error < 0)
      return (cdev->permanent_error);
    FOR_RECTS(re, ry, rheight) {
	ulong offset_temp;

	re.pcls->colors_used.or |= colors_used;	
	re.pcls->band_complexity.uses_color |= 
	    ((color0 != gx_no_color_index) && (color0 != 0xffffff) && (color0 != 0)) ||
	    ((color1 != gx_no_color_index) && (color1 != 0xffffff) && (color1 != 0));

	do {
	    code = cmd_disable_lop(cdev, re.pcls);
	} while (RECT_RECOVER(code));
	/* HANDLE_RECT_UNLESS(code, 0); */
	if (code < 0 && SET_BAND_CODE(code))
	    goto error_in_rect;
	if (!cls_has_tile_id(cdev, re.pcls, tile->id, offset_temp)) {
	    code = 0;
	    if (tile->id != gx_no_bitmap_id) {
		do {
		    code = clist_change_tile(cdev, re.pcls, tile, depth);
		} while (RECT_RECOVER(code));
		/* HANDLE_RECT_UNLESS(code, (code != gs_error_VMerror || !cdev->error_is_retryable)); */
		if (code < 0 && !(code != gs_error_VMerror || !cdev->error_is_retryable) && SET_BAND_CODE(code))
		    goto error_in_rect;
	    }
	    if (code < 0) {
		/* ok if gx_default... does retries internally: */
		/* it's self-sufficient */
		code = gx_default_strip_tile_rectangle(dev, tile,
						       rx, re.y, rwidth, re.height,
						       color0, color1,
						       px, py);
		if (code < 0 && SET_BAND_CODE(code))
		    goto error_in_rect;
		goto endr;
	    }
	}
	do {
	    code = 0;
	    if (color0 != re.pcls->tile_colors[0] || color1 != re.pcls->tile_colors[1])
		code = cmd_set_tile_colors(cdev, re.pcls, color0, color1);
	    if (px != re.pcls->tile_phase.x || py != re.pcls->tile_phase.y) {
		if (code >= 0)
		    code = cmd_set_tile_phase(cdev, re.pcls, px, py);
	    }
	    if (code >= 0)
		code = cmd_write_rect_cmd(cdev, re.pcls, cmd_op_tile_rect, rx, re.y,
					  rwidth, re.height);
	} while (RECT_RECOVER(code));
	/* HANDLE_RECT_UNLESS(code, 0); */
	if (code < 0 && SET_BAND_CODE(code))
	    goto error_in_rect;
endr:;
    } END_RECTS;
    return 0;
}

int
clist_copy_mono(gx_device * dev,
		const byte * data, int data_x, int raster, gx_bitmap_id id,
		int rx, int ry, int rwidth, int rheight,
		gx_color_index color0, gx_color_index color1)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int y0;
    gx_bitmap_id orig_id = id;
    gx_color_index colors_used =
	(color0 == gx_no_color_index ? 0 : color0) |
	(color1 == gx_no_color_index ? 0 : color1);
    bool uses_color = 
	((color0 != gx_no_color_index) && (color0 != 0xffffff) && (color0 != 0)) ||
	((color1 != gx_no_color_index) && (color1 != 0xffffff) && (color1 != 0));
    cmd_rects_enum_t re;


    fit_copy(dev, data, data_x, raster, id, rx, ry, rwidth, rheight);
    y0 = ry;
    if (cdev->permanent_error < 0)
      return (cdev->permanent_error);
    FOR_RECTS(re, ry, rheight) {
	int dx = data_x & 7;
	int w1 = dx + rwidth;
	const byte *row = data + (re.y - y0) * raster + (data_x >> 3);
	int code;

	re.pcls->colors_used.or |= colors_used;
	re.pcls->band_complexity.uses_color |= uses_color;

	do {
	    code = cmd_disable_lop(cdev, re.pcls);
	    if (code >= 0)
		code = cmd_disable_clip(cdev, re.pcls);
	    if (color0 != re.pcls->colors[0] && code >= 0)
		code = cmd_set_color0(cdev, re.pcls, color0);
	    if (color1 != re.pcls->colors[1] && code >= 0)
		code = cmd_set_color1(cdev, re.pcls, color1);
	} while (RECT_RECOVER(code));
	/* HANDLE_RECT_UNLESS(code, 0); */
	if (code < 0 && SET_BAND_CODE(code))
	    goto error_in_rect;
	/* Don't bother to check for a possible cache hit: */
	/* tile_rectangle and fill_mask handle those cases. */
copy:{
	gx_cmd_rect rect;
	int rsize;
	byte op = (byte) cmd_op_copy_mono;
	byte *dp;
	uint csize;
	uint compress;
	int code;

	rect.x = rx, rect.y = re.y;
	rect.width = w1, rect.height = re.height;
	rsize = (dx ? 3 : 1) + cmd_size_rect(&rect);
	do {
	    code = cmd_put_bits(cdev, re.pcls, row, w1, re.height, raster,
				rsize, (orig_id == gx_no_bitmap_id ?
					1 << cmd_compress_rle :
					cmd_mask_compress_any),
				&dp, &csize);
	} while (RECT_RECOVER(code));
	/* HANDLE_RECT_UNLESS(code, code == gs_error_limitcheck); */
	if (code < 0 && !(code == gs_error_limitcheck) && SET_BAND_CODE(code))
	    goto error_in_rect;
	compress = (uint)code;
	if (code < 0) {
	    /* The bitmap was too large; split up the transfer. */
	    if (re.height > 1) {
		/*
		 * Split the transfer by reducing the height.
		 * See the comment above FOR_RECTS in gxcldev.h.
		 */
		re.height >>= 1;
		goto copy;
	    } else {
		/* Split a single (very long) row. */
		int w2 = w1 >> 1;

		++cdev->driver_call_nesting; /* NEST_RECT */ 
		{
		    code = clist_copy_mono(dev, row, dx,
					   raster, gx_no_bitmap_id, rx, re.y,
					   w2, 1, color0, color1);
		    if (code >= 0)
			code = clist_copy_mono(dev, row, dx + w2,
					       raster, gx_no_bitmap_id,
					       rx + w2, re.y,
					       w1 - w2, 1, color0, color1);
		} 
		--cdev->driver_call_nesting; /* UNNEST_RECT */
		if (code < 0 && SET_BAND_CODE(code))
		    goto error_in_rect;
		continue;
	    }
	}
	op += compress;
	if (dx) {
	    *dp++ = cmd_count_op(cmd_opv_set_misc, 2);
	    *dp++ = cmd_set_misc_data_x + dx;
	}
	*dp++ = cmd_count_op(op, csize);
	cmd_put2w(rx, re.y, dp);
	cmd_put2w(w1, re.height, dp);
	re.pcls->rect = rect;
	}
    } END_RECTS;
    return 0;
}

int
clist_copy_color(gx_device * dev,
		 const byte * data, int data_x, int raster, gx_bitmap_id id,
		 int rx, int ry, int rwidth, int rheight)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int depth = dev->color_info.depth;
    int y0;
    int data_x_bit;
    /* We can't know what colors will be used: assume the worst. */
    gx_color_index colors_used = ((gx_color_index)1 << depth) - 1;
    cmd_rects_enum_t re;

    fit_copy(dev, data, data_x, raster, id, rx, ry, rwidth, rheight);
    y0 = ry;
    data_x_bit = data_x * depth;
    if (cdev->permanent_error < 0)
      return (cdev->permanent_error);
    FOR_RECTS(re, ry, rheight) {
	int dx = (data_x_bit & 7) / depth;
	int w1 = dx + rwidth;
	const byte *row = data + (re.y - y0) * raster + (data_x_bit >> 3);
	int code;

	re.pcls->colors_used.or |= colors_used;
	re.pcls->band_complexity.uses_color = 1;

	do {
	    code = cmd_disable_lop(cdev, re.pcls);
	    if (code >= 0)
		code = cmd_disable_clip(cdev, re.pcls);
	} while (RECT_RECOVER(code));
	/* HANDLE_RECT_UNLESS(code, 0); */
	if (code < 0 && SET_BAND_CODE(code))
	    goto error_in_rect;
	if (re.pcls->color_is_alpha) {
	    byte *dp;

	    do {
		code =
		    set_cmd_put_op(dp, cdev, re.pcls, cmd_opv_set_copy_color, 1);
	    } while (RECT_RECOVER(code));
	    /* HANDLE_RECT_UNLESS(code, 0); */
	    if (code < 0 && SET_BAND_CODE(code))
		goto error_in_rect;
	    re.pcls->color_is_alpha = 0;
	}
copy:{
	    gx_cmd_rect rect;
	    int rsize;
	    byte op = (byte) cmd_op_copy_color_alpha;
	    byte *dp;
	    uint csize;
	    uint compress;

	    rect.x = rx, rect.y = re.y;
	    rect.width = w1, rect.height = re.height;
	    rsize = (dx ? 3 : 1) + cmd_size_rect(&rect);
	    do {
		code = cmd_put_bits(cdev, re.pcls, row, w1 * depth,
				    re.height, raster, rsize,
				    1 << cmd_compress_rle, &dp, &csize);
	    } while (RECT_RECOVER(code));
	    /* HANDLE_RECT_UNLESS(code, code == gs_error_limitcheck); */
	    if (code < 0 && !(code == gs_error_limitcheck) && SET_BAND_CODE(code))
		goto error_in_rect;
	    compress = (uint)code;
	    if (code < 0) {
		/* The bitmap was too large; split up the transfer. */
		if (re.height > 1) {
		    /* Split the transfer by reducing the height.
		     * See the comment above FOR_RECTS in gxcldev.h.
		     */
		    re.height >>= 1;
		    goto copy;
		} else {
		    /* Split a single (very long) row. */
		    int w2 = w1 >> 1;

		    ++cdev->driver_call_nesting; /* NEST_RECT */ 
		    {
			code = clist_copy_color(dev, row, dx,
						raster, gx_no_bitmap_id,
						rx, re.y, w2, 1);
			if (code >= 0)
			    code = clist_copy_color(dev, row, dx + w2,
						    raster, gx_no_bitmap_id,
						    rx + w2, re.y, w1 - w2, 1);
		    } 
		    --cdev->driver_call_nesting; /* UNNEST_RECT */
		    if (code < 0 && SET_BAND_CODE(code))
			goto error_in_rect;
		    continue;
		}
	    }
	    op += compress;
	    if (dx) {
		*dp++ = cmd_count_op(cmd_opv_set_misc, 2);
		*dp++ = cmd_set_misc_data_x + dx;
	    }
	    *dp++ = cmd_count_op(op, csize);
	    cmd_put2w(rx, re.y, dp);
	    cmd_put2w(w1, re.height, dp);
	    re.pcls->rect = rect;
	}
    } END_RECTS;
    return 0;
}

int
clist_copy_alpha(gx_device * dev, const byte * data, int data_x,
	   int raster, gx_bitmap_id id, int rx, int ry, int rwidth, int rheight,
		 gx_color_index color, int depth)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    /* I don't like copying the entire body of clist_copy_color */
    /* just to change 2 arguments and 1 opcode, */
    /* but I don't see any alternative that doesn't require */
    /* another level of procedure call even in the common case. */
    int log2_depth = ilog2(depth);
    int y0;
    int data_x_bit;
    cmd_rects_enum_t re;

    /* If the target can't perform copy_alpha, exit now */
    if (depth > 1 && (cdev->disable_mask & clist_disable_copy_alpha) != 0)
	return_error(gs_error_unknownerror);

    fit_copy(dev, data, data_x, raster, id, rx, ry, rwidth, rheight);
    y0 = ry;
    data_x_bit = data_x << log2_depth;
    if (cdev->permanent_error < 0)
      return (cdev->permanent_error);
    FOR_RECTS(re, ry, rheight) {
	int dx = (data_x_bit & 7) >> log2_depth;
	int w1 = dx + rwidth;
	const byte *row = data + (re.y - y0) * raster + (data_x_bit >> 3);
	int code;

	re.pcls->colors_used.or |= color;
	do {
	    code = cmd_disable_lop(cdev, re.pcls);
	    if (code >= 0)
		code = cmd_disable_clip(cdev, re.pcls);
	} while (RECT_RECOVER(code));
	/* HANDLE_RECT_UNLESS(code, 0); */
	if (code < 0 && SET_BAND_CODE(code))
	    goto error_in_rect;
	if (!re.pcls->color_is_alpha) {
	    byte *dp;

	    do {
		code =
		    set_cmd_put_op(dp, cdev, re.pcls, cmd_opv_set_copy_alpha, 1);
	    } while (RECT_RECOVER(code));
	    /* HANDLE_RECT_UNLESS(code, 0); */
	    if (code < 0 && SET_BAND_CODE(code))
		goto error_in_rect;
	    re.pcls->color_is_alpha = 1;
	}
	if (color != re.pcls->colors[1]) {
	    do {
		code = cmd_set_color1(cdev, re.pcls, color);
	    } while (RECT_RECOVER(code));
	    /* HANDLE_RECT_UNLESS(code, 0); */
	    if (code < 0 && SET_BAND_CODE(code))
		goto error_in_rect;
	}
copy:{
	    gx_cmd_rect rect;
	    int rsize;
	    byte op = (byte) cmd_op_copy_color_alpha;
	    byte *dp;
	    uint csize;
	    uint compress;

	    rect.x = rx, rect.y = re.y;
	    rect.width = w1, rect.height = re.height;
	    rsize = (dx ? 4 : 2) + cmd_size_rect(&rect);
	    do {
		code = cmd_put_bits(cdev, re.pcls, row, w1 << log2_depth,
				    re.height, raster, rsize,
				    1 << cmd_compress_rle, &dp, &csize);
	    } while (RECT_RECOVER(code));
	    /* HANDLE_RECT_UNLESS(code, code == gs_error_limitcheck); */
	    if (code < 0 && !(code == gs_error_limitcheck) && SET_BAND_CODE(code))
		goto error_in_rect;
	    compress = (uint)code;
	    if (code < 0) {
		/* The bitmap was too large; split up the transfer. */
		if (re.height > 1) {
		    /* Split the transfer by reducing the height.
		     * See the comment above FOR_RECTS in gxcldev.h.
		     */
		    re.height >>= 1;
		    goto copy;
		} else {
		    /* Split a single (very long) row. */
		    int w2 = w1 >> 1;

		    ++cdev->driver_call_nesting; /* NEST_RECT */ 
		    {
			code = clist_copy_alpha(dev, row, dx,
						raster, gx_no_bitmap_id, rx, re.y,
						w2, 1, color, depth);
			if (code >= 0)
			    code = clist_copy_alpha(dev, row, dx + w2,
						    raster, gx_no_bitmap_id,
						    rx + w2, re.y, w1 - w2, 1,
						    color, depth);
		    } 
		    --cdev->driver_call_nesting; /* UNNEST_RECT */
		    if (code < 0 && SET_BAND_CODE(code))
			goto error_in_rect;
		    continue;
		}
	    }
	    op += compress;
	    if (dx) {
		*dp++ = cmd_count_op(cmd_opv_set_misc, 2);
		*dp++ = cmd_set_misc_data_x + dx;
	    }
	    *dp++ = cmd_count_op(op, csize);
	    *dp++ = depth;
	    cmd_put2w(rx, re.y, dp);
	    cmd_put2w(w1, re.height, dp);
	    re.pcls->rect = rect;
	}
    } END_RECTS;
    return 0;
}

int
clist_strip_copy_rop(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
		     const gx_color_index * scolors,
	   const gx_strip_bitmap * textures, const gx_color_index * tcolors,
		     int rx, int ry, int rwidth, int rheight,
		     int phase_x, int phase_y, gs_logical_operation_t lop)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    gs_rop3_t rop = lop_rop(lop);
    gx_strip_bitmap tile_with_id;
    const gx_strip_bitmap *tiles = textures;
    int y0;
    /* Compute the set of possible colors that this operation can generate. */
    gx_color_index all = ((gx_color_index)1 << dev->color_info.depth) - 1;
    bool subtractive = dev->color_info.num_components == 4; /****** HACK ******/
    gx_color_index S =
	(scolors ? scolors[0] | scolors[1] : sdata ? all : 0);
    gx_color_index T =
	(tcolors ? tcolors[0] | tcolors[1] : textures ? all : 0);
    gs_rop3_t color_rop =
	(subtractive ? byte_reverse_bits[rop ^ 0xff] : rop);
    bool slow_rop;
    cmd_rects_enum_t re;

    if (scolors != 0 && scolors[0] != scolors[1]) {
	fit_fill(dev, rx, ry, rwidth, rheight);
    } else {
	fit_copy(dev, sdata, sourcex, sraster, id, rx, ry, rwidth, rheight);
    }
    /*
     * On CMYK devices, RasterOps must be executed with complete pixels
     * if the operation involves the destination.
     * This is because the black plane interacts with the other planes
     * in the conversion between RGB and CMYK.  Check for this now.
     */
    {
	gs_rop3_t rop_used = rop;

	if (scolors && (scolors[0] == scolors[1]))
	    rop_used = (scolors[0] == gx_device_black(dev) ?
			rop3_know_S_0(rop_used) :
			scolors[0] == gx_device_white(dev) ?
			rop3_know_S_1(rop_used) : rop_used);
	if (tcolors && (tcolors[0] == tcolors[1]))
	    rop_used = (tcolors[0] == gx_device_black(dev) ?
			rop3_know_T_0(rop_used) :
			tcolors[0] == gx_device_white(dev) ?
			rop3_know_T_1(rop_used) : rop_used);
	slow_rop = !(rop == rop3_0 || rop == rop3_1 || 
                     rop == rop3_S || rop == rop3_T);
    }
    y0 = ry;
    /*
     * We shouldn't need to put the logic below inside FOR/END_RECTS,
     * but the lop_enabled flags are per-band.
     */
    if (cdev->permanent_error < 0)
      return (cdev->permanent_error);
    FOR_RECTS(re, ry, rheight) {
	const byte *row = sdata + (re.y - y0) * sraster;
	gx_color_index D = re.pcls->colors_used.or;
	int code;

	/* Reducing D, S, T to rop_operand (which apparently is 32 bit) appears safe
	   due to 'all' a has smaller snumber of significant bits. */
	re.pcls->colors_used.or =
	    ((rop_proc_table[color_rop])((rop_operand)D, (rop_operand)S, (rop_operand)T) & all) | D;
	re.pcls->colors_used.slow_rop |= slow_rop;
	re.pcls->band_complexity.nontrivial_rops |= slow_rop;
	if (rop3_uses_T(rop)) {
	    if (tcolors == 0 || tcolors[0] != tcolors[1]) {
		ulong offset_temp;

		if (!cls_has_tile_id(cdev, re.pcls, tiles->id, offset_temp)) {
		    /* Change tile.  If there is no id, generate one. */
		    if (tiles->id == gx_no_bitmap_id) {
			tile_with_id = *tiles;
			tile_with_id.id = gs_next_ids(dev->memory, 1);
			tiles = &tile_with_id;
		    }
		    do {
			code = clist_change_tile(cdev, re.pcls, tiles,
						 (tcolors != 0 ? 1 :
						  dev->color_info.depth));
		    } while (RECT_RECOVER(code));
		    /* HANDLE_RECT_UNLESS(code, code == gs_error_limitcheck); */
		    if (code < 0 && !(code == gs_error_limitcheck) && SET_BAND_CODE(code))
			goto error_in_rect;
		    if (code < 0) {
			/*
			 * The error is a limitcheck: we have a tile that
			 * is too big to fit in the command reading buffer.
			 * For now, just divide up the transfer into scan
			 * lines.  (If a single scan line won't fit, punt.)
			 * Eventually, we'll need a way to transfer the tile
			 * in pieces.
			 */
			uint rep_height = tiles->rep_height;
			gs_id ids;
			gx_strip_bitmap line_tile;
			int iy;

			if (rep_height == 1 ||
			    /****** CAN'T HANDLE SHIFT YET ******/
			    tiles->rep_shift != 0
			    )
			    return code;
			/*
			 * Allocate enough fake IDs, since the inner call on
			 * clist_strip_copy_rop will need them anyway.
			 */
			ids = gs_next_ids(dev->memory, min(re.height, rep_height));
			line_tile = *tiles;
			line_tile.size.y = 1;
			line_tile.rep_height = 1;
			for (iy = 0; iy < re.height; ++iy) {
			    line_tile.data = tiles->data + line_tile.raster *
				((re.y + iy + phase_y) % rep_height);
			    line_tile.id = ids + (iy % rep_height);
			    /*
			     * Note that since we're only transferring
			     * a single scan line, phase_y is irrelevant;
			     * we may as well use the current tile phase
			     * so we don't have to write extra commands.
			     */
			    ++cdev->driver_call_nesting; /* NEST_RECT */ 
			    {
				code = clist_strip_copy_rop(dev,
					(sdata == 0 ? 0 : row + iy * sraster),
					sourcex, sraster,
					gx_no_bitmap_id, scolors,
					&line_tile, tcolors,
					rx, re.y + iy, rwidth, 1,
					phase_x, re.pcls->tile_phase.y, lop);
			    } 
			    --cdev->driver_call_nesting; /* UNNEST_RECT */
			    if (code < 0 && SET_BAND_CODE(code))
				goto error_in_rect;
			}
			continue;
		    }
		    if (phase_x != re.pcls->tile_phase.x ||
			phase_y != re.pcls->tile_phase.y
			) {
			do {
			    code = cmd_set_tile_phase(cdev, re.pcls, phase_x,
						      phase_y);
			} while (RECT_RECOVER(code));
			/* HANDLE_RECT_UNLESS(code, 0); */
			if (code < 0 && SET_BAND_CODE(code))
			    goto error_in_rect;
		    }
		}
	    }
	    /* Set the tile colors. */
	    do {
		code =
		    (tcolors != 0 ?
		     cmd_set_tile_colors(cdev, re.pcls, tcolors[0], tcolors[1]) :
		     cmd_set_tile_colors(cdev, re.pcls, gx_no_color_index,
					 gx_no_color_index));
	    } while (RECT_RECOVER(code));
	    /* HANDLE_RECT_UNLESS(code, 0); */
	    if (code < 0 && SET_BAND_CODE(code))
		goto error_in_rect;
	}
	do {
	    code = 0;
	    if (lop != re.pcls->lop)
		code = cmd_set_lop(cdev, re.pcls, lop);
	    if (code >= 0)
		code = cmd_enable_lop(cdev, re.pcls);
	} while (RECT_RECOVER(code));
	/* HANDLE_RECT_UNLESS(code, 0); */
	if (code < 0 && SET_BAND_CODE(code))
	    goto error_in_rect;
	/* Set lop_enabled to -1 so that fill_rectangle / copy_* */
	/* won't attempt to set it to 0. */
	re.pcls->lop_enabled = -1;
	++cdev->driver_call_nesting; /* NEST_RECT */ 
	{
	    if (scolors != 0) {
		if (scolors[0] == scolors[1])
		    code = clist_fill_rectangle(dev, rx, re.y, rwidth, re.height,
						scolors[1]);
		else
		    code = clist_copy_mono(dev, row, sourcex, sraster, id,
					   rx, re.y, rwidth, re.height,
					   scolors[0], scolors[1]);
	    } else
		code = clist_copy_color(dev, row, sourcex, sraster, id,
					rx, re.y, rwidth, re.height);
	} 
	--cdev->driver_call_nesting; /* UNNEST_RECT */
	re.pcls->lop_enabled = 1;
	if (code < 0 && SET_BAND_CODE(code))
	    goto error_in_rect;
    } END_RECTS;
    return 0;
}
