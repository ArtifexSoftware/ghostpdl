/* Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */


/* RasterOp / transparency implementation for memory devices */
#include "memory_.h"
#include "gx.h"
#include "gsbittab.h"
#include "gserrors.h"
#include "gsropt.h"
#include "gxcindex.h"
#include "gxdcolor.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxdevrop.h"
#include "gdevmrop.h"

/* Define whether we implement transparency correctly, or whether we */
/* implement it as documented in the H-P manuals. */
#define TRANSPARENCY_PER_H_P

/*
 * NOTE: The 16- and 32-bit cases aren't implemented: they just fall back to
 * the default implementation.  This is very slow and will be fixed someday.
 */

#define chunk byte

/* Calculate the X offset for a given Y value, */
/* taking shift into account if necessary. */
#define x_offset(px, ty, textures)\
  ((textures)->shift == 0 ? (px) :\
   (px) + (ty) / (textures)->rep_height * (textures)->rep_shift)

/* ---------------- Monobit RasterOp ---------------- */

int
mem_mono_strip_copy_rop(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
			const gx_color_index * scolors,
	   const gx_strip_bitmap * textures, const gx_color_index * tcolors,
			int x, int y, int width, int height,
			int phase_x, int phase_y, gs_logical_operation_t lop)
{
    gx_device_memory *mdev = (gx_device_memory *) dev;
    gs_rop3_t rop = gs_transparent_rop(lop);	/* handle transparency */
    gx_strip_bitmap no_texture;
    bool invert;
    uint draster = mdev->raster;
    uint traster;
    int line_count;
    byte *drow;
    const byte *srow;
    int ty;

    /* If map_rgb_color isn't the default one for monobit memory */
    /* devices, palette might not be set; set it now if needed. */
    if (mdev->palette.data == 0)
	gdev_mem_mono_set_inverted(mdev,
				   (*dev_proc(dev, map_rgb_color))
				   (dev, (gx_color_value) 0,
				    (gx_color_value) 0, (gx_color_value) 0)
				   != 0);
    invert = mdev->palette.data[0] != 0;

#ifdef DEBUG
    if (gs_debug_c('b'))
	trace_copy_rop("mem_mono_strip_copy_rop",
		       dev, sdata, sourcex, sraster,
		       id, scolors, textures, tcolors,
		       x, y, width, height, phase_x, phase_y, lop);
    if (gs_debug_c('B'))
	debug_dump_bitmap(scan_line_base(mdev, y), mdev->raster,
			  height, "initial dest bits");
#endif

    /*
     * RasterOp is defined as operating in RGB space; in the monobit
     * case, this means black = 0, white = 1.  However, most monobit
     * devices use the opposite convention.  To make this work,
     * we must precondition the Boolean operation by swapping the
     * order of bits end-for-end and then inverting.
     */

    if (invert)
	rop = byte_reverse_bits[rop] ^ 0xff;

    /*
     * From this point on, rop works in terms of device pixel values,
     * not RGB-space values.
     */

    /* Modify the raster operation according to the source palette. */
    if (scolors != 0) {		/* Source with palette. */
	switch ((int)((scolors[1] << 1) + scolors[0])) {
	    case 0:
		rop = rop3_know_S_0(rop);
		break;
	    case 1:
		rop = rop3_invert_S(rop);
		break;
	    case 2:
		break;
	    case 3:
		rop = rop3_know_S_1(rop);
		break;
	}
    }
    /* Modify the raster operation according to the texture palette. */
    if (tcolors != 0) {		/* Texture with palette. */
	switch ((int)((tcolors[1] << 1) + tcolors[0])) {
	    case 0:
		rop = rop3_know_T_0(rop);
		break;
	    case 1:
		rop = rop3_invert_T(rop);
		break;
	    case 2:
		break;
	    case 3:
		rop = rop3_know_T_1(rop);
		break;
	}
    }
    /* Handle constant source and/or texture, and other special cases. */
    {
	gx_color_index color0, color1;

	switch (rop_usage_table[rop]) {
	    case rop_usage_none:
		/* We're just filling with a constant. */
		return (*dev_proc(dev, fill_rectangle))
		    (dev, x, y, width, height, (gx_color_index) (rop & 1));
	    case rop_usage_D:
		/* This is either D (no-op) or ~D. */
		if (rop == rop3_D)
		    return 0;
		/* Code no_S inline, then finish with no_T. */
		fit_fill(dev, x, y, width, height);
		sdata = mdev->base;
		sourcex = x;
		sraster = 0;
		goto no_T;
	    case rop_usage_S:
		/* This is either S or ~S, which copy_mono can handle. */
		if (rop == rop3_S)
		    color0 = 0, color1 = 1;
		else
		    color0 = 1, color1 = 0;
	      do_copy:return (*dev_proc(dev, copy_mono))
		    (dev, sdata, sourcex, sraster, id, x, y, width, height,
		     color0, color1);
	    case rop_usage_DS:
		/* This might be a case that copy_mono can handle. */
#define copy_case(c0, c1) color0 = c0, color1 = c1; goto do_copy;
		switch ((uint) rop) {	/* cast shuts up picky compilers */
		    case rop3_D & rop3_not(rop3_S):
			copy_case(gx_no_color_index, 0);
		    case rop3_D | rop3_S:
			copy_case(gx_no_color_index, 1);
		    case rop3_D & rop3_S:
			copy_case(0, gx_no_color_index);
		    case rop3_D | rop3_not(rop3_S):
			copy_case(1, gx_no_color_index);
		    default:;
		}
#undef copy_case
		fit_copy(dev, sdata, sourcex, sraster, id, x, y, width, height);
	      no_T:		/* Texture is not used; textures may be garbage. */
		no_texture.data = mdev->base;	/* arbitrary */
		no_texture.raster = 0;
		no_texture.size.x = width;
		no_texture.size.y = height;
		no_texture.rep_width = no_texture.rep_height = 1;
		no_texture.rep_shift = no_texture.shift = 0;
		textures = &no_texture;
		break;
	    case rop_usage_T:
		/* This is either T or ~T, which tile_rectangle can handle. */
		if (rop == rop3_T)
		    color0 = 0, color1 = 1;
		else
		    color0 = 1, color1 = 0;
	      do_tile:return (*dev_proc(dev, strip_tile_rectangle))
		    (dev, textures, x, y, width, height, color0, color1,
		     phase_x, phase_y);
	    case rop_usage_DT:
		/* This might be a case that tile_rectangle can handle. */
#define tile_case(c0, c1) color0 = c0, color1 = c1; goto do_tile;
		switch ((uint) rop) {	/* cast shuts up picky compilers */
		    case rop3_D & rop3_not(rop3_T):
			tile_case(gx_no_color_index, 0);
		    case rop3_D | rop3_T:
			tile_case(gx_no_color_index, 1);
		    case rop3_D & rop3_T:
			tile_case(0, gx_no_color_index);
		    case rop3_D | rop3_not(rop3_T):
			tile_case(1, gx_no_color_index);
		    default:;
		}
#undef tile_case
		fit_fill(dev, x, y, width, height);
		/* Source is not used; sdata et al may be garbage. */
		sdata = mdev->base;	/* arbitrary, as long as all */
		/* accesses are valid */
		sourcex = x;	/* guarantee no source skew */
		sraster = 0;
		break;
	    default:		/* rop_usage_[D]ST */
		fit_copy(dev, sdata, sourcex, sraster, id, x, y, width, height);
	}
    }

#ifdef DEBUG
    if_debug1('b', "final rop=0x%x\n", rop);
#endif

    /* Set up transfer parameters. */
    line_count = height;
    srow = sdata;
    drow = scan_line_base(mdev, y);
    traster = textures->raster;
    ty = y + phase_y;

    /* Loop over scan lines. */
    for (; line_count-- > 0; drow += draster, srow += sraster, ++ty) {
	int sx = sourcex;
	int dx = x;
	int w = width;
	const byte *trow =
	textures->data + (ty % textures->rep_height) * traster;
	int xoff = x_offset(phase_x, ty, textures);
	int nw;

	/* Loop over (horizontal) copies of the tile. */
	for (; w > 0; sx += nw, dx += nw, w -= nw) {
	    int dbit = dx & 7;
	    int sbit = sx & 7;
	    int sskew = sbit - dbit;
	    int tx = (dx + xoff) % textures->rep_width;
	    int tbit = tx & 7;
	    int tskew = tbit - dbit;
	    int left = nw = min(w, textures->size.x - tx);
	    byte lmask = 0xff >> dbit;
	    byte rmask = 0xff << (~(dbit + nw - 1) & 7);
	    byte mask = lmask;
	    int nx = 8 - dbit;
	    byte *dptr = drow + (dx >> 3);
	    const byte *sptr = srow + (sx >> 3);
	    const byte *tptr = trow + (tx >> 3);

	    if (sskew < 0)
		--sptr, sskew += 8;
	    if (tskew < 0)
		--tptr, tskew += 8;
	    for (; left > 0;
		 left -= nx, mask = 0xff, nx = 8,
		 ++dptr, ++sptr, ++tptr
		) {
		byte dbyte = *dptr;

#define fetch1(ptr, skew)\
  (skew ? (ptr[0] << skew) + (ptr[1] >> (8 - skew)) : *ptr)
		byte sbyte = fetch1(sptr, sskew);
		byte tbyte = fetch1(tptr, tskew);

#undef fetch1
		byte result =
		(*rop_proc_table[rop]) (dbyte, sbyte, tbyte);

		if (left <= nx)
		    mask &= rmask;
		*dptr = (mask == 0xff ? result :
			 (result & mask) | (dbyte & ~mask));
	    }
	}
    }
#ifdef DEBUG
    if (gs_debug_c('B'))
	debug_dump_bitmap(scan_line_base(mdev, y), mdev->raster,
			  height, "final dest bits");
#endif
    return 0;
}

/* ---------------- Fake RasterOp for 2- and 4-bit devices ---------------- */

/*
 * Define patched versions of the driver procedures that may be called
 * by mem_mono_strip_copy_rop (see below).  Currently we just punt to
 * the slow, general case; we could do a lot better.
 */
private int
mem_gray_rop_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
			    gx_color_index color)
{
    return -1;
}
private int
mem_gray_rop_copy_mono(gx_device * dev, const byte * data,
		       int dx, int raster, gx_bitmap_id id,
		       int x, int y, int w, int h,
		       gx_color_index zero, gx_color_index one)
{
    return -1;
}
private int
mem_gray_rop_strip_tile_rectangle(gx_device * dev,
				  const gx_strip_bitmap * tiles,
				  int x, int y, int w, int h,
				  gx_color_index color0, gx_color_index color1,
				  int px, int py)
{
    return -1;
}

int
mem_gray_strip_copy_rop(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
			const gx_color_index * scolors,
	   const gx_strip_bitmap * textures, const gx_color_index * tcolors,
			int x, int y, int width, int height,
			int phase_x, int phase_y, gs_logical_operation_t lop)
{
    gx_color_index scolors2[2];
    const gx_color_index *real_scolors = scolors;
    gx_color_index tcolors2[2];
    const gx_color_index *real_tcolors = tcolors;
    gx_strip_bitmap texture2;
    const gx_strip_bitmap *real_texture = textures;
    long tdata;
    int depth = dev->color_info.depth;
    int log2_depth = depth >> 1;	/* works for 2, 4 */
    gx_color_index max_pixel = (1 << depth) - 1;
    int code;

#ifdef DEBUG
    if (gs_debug_c('b'))
	trace_copy_rop("mem_gray_strip_copy_rop",
		       dev, sdata, sourcex, sraster,
		       id, scolors, textures, tcolors,
		       x, y, width, height, phase_x, phase_y, lop);
#endif
    if (gx_device_has_color(dev) ||
	(lop & (lop_S_transparent | lop_T_transparent)) ||
	(scolors &&		/* must be (0,0) or (max,max) */
	 ((scolors[0] | scolors[1]) != 0) &&
	 ((scolors[0] & scolors[1]) != max_pixel)) ||
	(tcolors && (tcolors[0] != tcolors[1]))
	) {
	/* We can't fake it: do it the slow, painful way. */
	return gx_default_strip_copy_rop(dev,
		    sdata, sourcex, sraster, id, scolors, textures, tcolors,
				x, y, width, height, phase_x, phase_y, lop);
    }
    if (scolors) {		/* Must be a solid color: see above. */
	scolors2[0] = scolors2[1] = scolors[0] & 1;
	real_scolors = scolors2;
    }
    if (textures) {
	texture2 = *textures;
	texture2.size.x <<= log2_depth;
	texture2.rep_width <<= log2_depth;
	texture2.shift <<= log2_depth;
	texture2.rep_shift <<= log2_depth;
	real_texture = &texture2;
    }
    if (tcolors) {
	/* For polybit textures with colors other than */
	/* all 0s or all 1s, fabricate the data. */
	if (tcolors[0] != 0 && tcolors[0] != max_pixel) {
	    real_tcolors = 0;
	    *(byte *) & tdata = (byte) tcolors[0] << (8 - depth);
	    texture2.data = (byte *) & tdata;
	    texture2.raster = align_bitmap_mod;
	    texture2.size.x = texture2.rep_width = depth;
	    texture2.size.y = texture2.rep_height = 1;
	    texture2.id = gx_no_bitmap_id;
	    texture2.shift = texture2.rep_shift = 0;
	    real_texture = &texture2;
	} else {
	    tcolors2[0] = tcolors2[1] = tcolors[0] & 1;
	    real_tcolors = tcolors2;
	}
    }
    /*
     * mem_mono_strip_copy_rop may call fill_rectangle, copy_mono, or
     * strip_tile_rectangle for special cases.  Patch those procedures
     * temporarily so they will either do the right thing or return
     * an error.
     */
    {
	dev_proc_fill_rectangle((*fill_rectangle)) =
	    dev_proc(dev, fill_rectangle);
	dev_proc_copy_mono((*copy_mono)) =
	    dev_proc(dev, copy_mono);
	dev_proc_strip_tile_rectangle((*strip_tile_rectangle)) =
	    dev_proc(dev, strip_tile_rectangle);

	set_dev_proc(dev, fill_rectangle, mem_gray_rop_fill_rectangle);
	set_dev_proc(dev, copy_mono, mem_gray_rop_copy_mono);
	set_dev_proc(dev, strip_tile_rectangle,
		     mem_gray_rop_strip_tile_rectangle);
	dev->width <<= log2_depth;
	code = mem_mono_strip_copy_rop(dev, sdata,
				       (real_scolors == NULL ?
					sourcex << log2_depth : sourcex),
				       sraster, id, real_scolors,
				       real_texture, real_tcolors,
				       x << log2_depth, y,
				       width << log2_depth, height,
				       phase_x << log2_depth, phase_y, lop);
	set_dev_proc(dev, fill_rectangle, fill_rectangle);
	set_dev_proc(dev, copy_mono, copy_mono);
	set_dev_proc(dev, strip_tile_rectangle, strip_tile_rectangle);
	dev->width >>= log2_depth;
    }
    /* If we punted, use the general procedure. */
    if (code < 0)
	return gx_default_strip_copy_rop(dev, sdata, sourcex, sraster, id,
					 scolors, textures, tcolors,
					 x, y, width, height,
					 phase_x, phase_y, lop);
    return code;
}

/* ---------------- RasterOp with 8-bit gray / 24-bit RGB ---------------- */

int
mem_gray8_rgb24_strip_copy_rop(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
			       const gx_color_index * scolors,
	   const gx_strip_bitmap * textures, const gx_color_index * tcolors,
			       int x, int y, int width, int height,
		       int phase_x, int phase_y, gs_logical_operation_t lop)
{
    gx_device_memory *mdev = (gx_device_memory *) dev;
    gs_rop3_t rop = lop_rop(lop);
    gx_color_index const_source = gx_no_color_index;
    gx_color_index const_texture = gx_no_color_index;
    uint draster = mdev->raster;
    int line_count;
    byte *drow;
    int depth = dev->color_info.depth;
    int bpp = depth >> 3;	/* bytes per pixel, 1 or 3 */
    gx_color_index all_ones = ((gx_color_index) 1 << depth) - 1;
    gx_color_index strans =
	(lop & lop_S_transparent ? all_ones : gx_no_color_index);
    gx_color_index ttrans =
	(lop & lop_T_transparent ? all_ones : gx_no_color_index);

    /* Check for constant source. */
    if (scolors != 0 && scolors[0] == scolors[1]) {	/* Constant source */
	const_source = scolors[0];
	if (const_source == 0)
	    rop = rop3_know_S_0(rop);
	else if (const_source == all_ones)
	    rop = rop3_know_S_1(rop);
    } else if (!rop3_uses_S(rop))
	const_source = 0;	/* arbitrary */

    /* Check for constant texture. */
    if (tcolors != 0 && tcolors[0] == tcolors[1]) {	/* Constant texture */
	const_texture = tcolors[0];
	if (const_texture == 0)
	    rop = rop3_know_T_0(rop);
	else if (const_texture == all_ones)
	    rop = rop3_know_T_1(rop);
    } else if (!rop3_uses_T(rop))
	const_texture = 0;	/* arbitrary */
    if (bpp == 1 && gx_device_has_color(dev)) {
	/*
	 * This is an 8-bit device but not gray-scale.  Except in a few
	 * simple cases, we have to use the slow algorithm that converts
	 * values to and from RGB.
	 */
	gx_color_index bw_pixel;

	switch (rop) {
	case rop3_0:
	    bw_pixel = gx_device_black(dev);
	    goto bw;
	case rop3_1:
	    bw_pixel = gx_device_white(dev);
bw:	    switch (bw_pixel) {
	    case 0x00:
		rop = rop3_0;
		break;
	    case 0xff:
		rop = rop3_1;
		break;
	    default:
		goto df;
	    }
	    break;
	case rop3_D:
	    break;
	case rop3_S:
	    if (lop & lop_S_transparent)
		goto df;
	    break;
	case rop3_T:
	    if (lop & lop_T_transparent)
		goto df;
	    break;
	default:
df:	    return gx_default_strip_copy_rop(dev,
				sdata, sourcex, sraster, id, scolors,
				textures, tcolors, x, y, width, height,
				phase_x, phase_y, lop);
	}
    }
    /* Adjust coordinates to be in bounds. */
    if (const_source == gx_no_color_index) {
	fit_copy(dev, sdata, sourcex, sraster, id,
		 x, y, width, height);
    } else {
	fit_fill(dev, x, y, width, height);
    }

    /* Set up transfer parameters. */
    line_count = height;
    drow = scan_line_base(mdev, y) + x * bpp;

    /*
     * There are 18 cases depending on whether each of the source and
     * texture is constant, 1-bit, or multi-bit, and on whether the
     * depth is 8 or 24 bits.  We divide first according to constant
     * vs. non-constant, and then according to 1- vs. multi-bit, and
     * finally according to pixel depth.  This minimizes source code,
     * but not necessarily time, since we do some of the divisions
     * within 1 or 2 levels of loop.
     */

#define dbit(base, i) ((base)[(i) >> 3] & (0x80 >> ((i) & 7)))
/* 8-bit */
#define cbit8(base, i, colors)\
  (dbit(base, i) ? (byte)colors[1] : (byte)colors[0])
#define rop_body_8(s_pixel, t_pixel)\
  if ( (s_pixel) == strans ||	/* So = 0, s_tr = 1 */\
       (t_pixel) == ttrans	/* Po = 0, p_tr = 1 */\
     )\
    continue;\
  *dptr = (*rop_proc_table[rop])(*dptr, s_pixel, t_pixel)
/* 24-bit */
#define get24(ptr)\
  (((gx_color_index)(ptr)[0] << 16) | ((gx_color_index)(ptr)[1] << 8) | (ptr)[2])
#define put24(ptr, pixel)\
  (ptr)[0] = (byte)((pixel) >> 16),\
  (ptr)[1] = (byte)((uint)(pixel) >> 8),\
  (ptr)[2] = (byte)(pixel)
#define cbit24(base, i, colors)\
  (dbit(base, i) ? colors[1] : colors[0])
#define rop_body_24(s_pixel, t_pixel)\
  if ( (s_pixel) == strans ||	/* So = 0, s_tr = 1 */\
       (t_pixel) == ttrans	/* Po = 0, p_tr = 1 */\
     )\
    continue;\
  { gx_color_index d_pixel = get24(dptr);\
    d_pixel = (*rop_proc_table[rop])(d_pixel, s_pixel, t_pixel);\
    put24(dptr, d_pixel);\
  }

    if (const_texture != gx_no_color_index) {
/**** Constant texture ****/
	if (const_source != gx_no_color_index) {
/**** Constant source & texture ****/
	    for (; line_count-- > 0; drow += draster) {
		byte *dptr = drow;
		int left = width;

		if (bpp == 1)
/**** 8-bit destination ****/
		    for (; left > 0; ++dptr, --left) {
			rop_body_8((byte)const_source, (byte)const_texture);
		    }
		else
/**** 24-bit destination ****/
		    for (; left > 0; dptr += 3, --left) {
			rop_body_24(const_source, const_texture);
		    }
	    }
	} else {
/**** Data source, const texture ****/
	    const byte *srow = sdata;

	    for (; line_count-- > 0; drow += draster, srow += sraster) {
		byte *dptr = drow;
		int left = width;

		if (scolors) {
/**** 1-bit source ****/
		    int sx = sourcex;

		    if (bpp == 1)
/**** 8-bit destination ****/
			for (; left > 0; ++dptr, ++sx, --left) {
			    byte s_pixel = cbit8(srow, sx, scolors);

			    rop_body_8(s_pixel, (byte)const_texture);
			}
		    else
/**** 24-bit destination ****/
			for (; left > 0; dptr += 3, ++sx, --left) {
			    bits32 s_pixel = cbit24(srow, sx, scolors);

			    rop_body_24(s_pixel, const_texture);
			}
		} else if (bpp == 1) {
/**** 8-bit source & dest ****/
		    const byte *sptr = srow + sourcex;

		    for (; left > 0; ++dptr, ++sptr, --left) {
			byte s_pixel = *sptr;

			rop_body_8(s_pixel, (byte)const_texture);
		    }
		} else {
/**** 24-bit source & dest ****/
		    const byte *sptr = srow + sourcex * 3;

		    for (; left > 0; dptr += 3, sptr += 3, --left) {
			bits32 s_pixel = get24(sptr);

			rop_body_24(s_pixel, const_texture);
		    }
		}
	    }
	}
    } else if (const_source != gx_no_color_index) {
/**** Const source, data texture ****/
	uint traster = textures->raster;
	int ty = y + phase_y;

	for (; line_count-- > 0; drow += draster, ++ty) {	/* Loop over copies of the tile. */
	    int dx = x, w = width, nw;
	    byte *dptr = drow;
	    const byte *trow =
	    textures->data + (ty % textures->size.y) * traster;
	    int xoff = x_offset(phase_x, ty, textures);

	    for (; w > 0; dx += nw, w -= nw) {
		int tx = (dx + xoff) % textures->rep_width;
		int left = nw = min(w, textures->size.x - tx);
		const byte *tptr = trow;

		if (tcolors) {
/**** 1-bit texture ****/
		    if (bpp == 1)
/**** 8-bit dest ****/
			for (; left > 0; ++dptr, ++tx, --left) {
			    byte t_pixel = cbit8(tptr, tx, tcolors);

			    rop_body_8((byte)const_source, t_pixel);
			}
		    else
/**** 24-bit dest ****/
			for (; left > 0; dptr += 3, ++tx, --left) {
			    bits32 t_pixel = cbit24(tptr, tx, tcolors);

			    rop_body_24(const_source, t_pixel);
			}
		} else if (bpp == 1) {
/**** 8-bit T & D ****/
		    tptr += tx;
		    for (; left > 0; ++dptr, ++tptr, --left) {
			byte t_pixel = *tptr;

			rop_body_8((byte)const_source, t_pixel);
		    }
		} else {
/**** 24-bit T & D ****/
		    tptr += tx * 3;
		    for (; left > 0; dptr += 3, tptr += 3, --left) {
			bits32 t_pixel = get24(tptr);

			rop_body_24(const_source, t_pixel);
		    }
		}
	    }
	}
    } else {
/**** Data source & texture ****/
	uint traster = textures->raster;
	int ty = y + phase_y;
	const byte *srow = sdata;

	/* Loop over scan lines. */
	for (; line_count-- > 0; drow += draster, srow += sraster, ++ty) {	/* Loop over copies of the tile. */
	    int sx = sourcex;
	    int dx = x;
	    int w = width;
	    int nw;
	    byte *dptr = drow;
	    const byte *trow =
	    textures->data + (ty % textures->size.y) * traster;
	    int xoff = x_offset(phase_x, ty, textures);

	    for (; w > 0; dx += nw, w -= nw) {	/* Loop over individual pixels. */
		int tx = (dx + xoff) % textures->rep_width;
		int left = nw = min(w, textures->size.x - tx);
		const byte *tptr = trow;

		/*
		 * For maximum speed, we should split this loop
		 * into 7 cases depending on source & texture
		 * depth: (1,1), (1,8), (1,24), (8,1), (8,8),
		 * (24,1), (24,24).  But since we expect these
		 * cases to be relatively uncommon, we just
		 * divide on the destination depth.
		 */
		if (bpp == 1) {
/**** 8-bit destination ****/
		    const byte *sptr = srow + sx;

		    tptr += tx;
		    for (; left > 0; ++dptr, ++sptr, ++tptr, ++sx, ++tx, --left) {
			byte s_pixel =
			    (scolors ? cbit8(srow, sx, scolors) : *sptr);
			byte t_pixel =
			    (tcolors ? cbit8(tptr, tx, tcolors) : *tptr);

			rop_body_8(s_pixel, t_pixel);
		    }
		} else {
/**** 24-bit destination ****/
		    const byte *sptr = srow + sx * 3;

		    tptr += tx * 3;
		    for (; left > 0; dptr += 3, sptr += 3, tptr += 3, ++sx, ++tx, --left) {
			bits32 s_pixel =
			    (scolors ? cbit24(srow, sx, scolors) :
			     get24(sptr));
			bits32 t_pixel =
			    (tcolors ? cbit24(tptr, tx, tcolors) :
			     get24(tptr));

			rop_body_24(s_pixel, t_pixel);
		    }
		}
	    }
	}
    }
#undef rop_body_8
#undef rop_body_24
#undef dbit
#undef cbit8
#undef cbit24
    return 0;
}
