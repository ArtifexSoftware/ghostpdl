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


/* Default and device-independent RasterOp algorithms */
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
#include "gxgetbit.h"
#include "gdevmrop.h"

/* ---------------- Debugging aids ---------------- */

#ifdef DEBUG

void
trace_copy_rop(const char *cname, gx_device * dev,
	       const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
	       const gx_color_index * scolors,
	       const gx_strip_bitmap * textures,
	       const gx_color_index * tcolors,
	       int x, int y, int width, int height,
	       int phase_x, int phase_y, gs_logical_operation_t lop)
{
    dlprintf4("%s: dev=0x%lx(%s) depth=%d\n",
	      cname, (ulong) dev, dev->dname, dev->color_info.depth);
    dlprintf4("  source data=0x%lx x=%d raster=%u id=%lu colors=",
	      (ulong) sdata, sourcex, sraster, (ulong) id);
    if (scolors)
	dprintf2("(%lu,%lu);\n", scolors[0], scolors[1]);
    else
	dputs("none;\n");
    if (textures)
	dlprintf8("  textures=0x%lx size=%dx%d(%dx%d) raster=%u shift=%d(%d)",
		  (ulong) textures, textures->size.x, textures->size.y,
		  textures->rep_width, textures->rep_height,
		  textures->raster, textures->shift, textures->rep_shift);
    else
	dlputs("  textures=none");
    if (tcolors)
	dprintf2(" colors=(%lu,%lu)\n", tcolors[0], tcolors[1]);
    else
	dputs(" colors=none\n");
    dlprintf7("  rect=(%d,%d),(%d,%d) phase=(%d,%d) op=0x%x\n",
	      x, y, x + width, y + height, phase_x, phase_y,
	      (uint) lop);
    if (gs_debug_c('B')) {
	if (sdata)
	    debug_dump_bitmap(sdata, sraster, height, "source bits");
	if (textures && textures->data)
	    debug_dump_bitmap(textures->data, textures->raster,
			      textures->size.y, "textures bits");
    }
}

#endif

/* ---------------- Default copy_rop implementations ---------------- */

private void
unpack_to_standard(gx_device *dev, byte *dest, const byte *src, int sourcex,
		   int width, gs_get_bits_options_t options,
		   gs_get_bits_options_t stored)
{
    gs_get_bits_params_t params;

    params.options = options;
    params.data[0] = dest;
    params.x_offset = 0;
    gx_get_bits_copy(dev, sourcex, width, 1, &params, stored,
		     src, 0 /*raster, irrelevant*/);
}

private void
unpack_colors_to_standard(gx_device * dev, gx_color_index real_colors[2],
			  const gx_color_index * colors, int depth)
{
    int i;

    for (i = 0; i < 2; ++i) {
	gx_color_value rgb[3];
	gx_color_index pixel;

	(*dev_proc(dev, map_color_rgb)) (dev, colors[i], rgb);
	pixel = gx_color_value_to_byte(rgb[0]);
	if (depth > 8) {
	    pixel = (pixel << 16) +
		(gx_color_value_to_byte(rgb[1]) << 8) +
		gx_color_value_to_byte(rgb[2]);
	}
	real_colors[i] = pixel;
    }
}

private gx_color_index
map_rgb_to_color_via_cmyk(gx_device * dev, gx_color_value r,
			  gx_color_value g, gx_color_value b)
{
    gx_color_value c = gx_max_color_value - r;
    gx_color_value m = gx_max_color_value - g;
    gx_color_value y = gx_max_color_value - b;
    gx_color_value k = (c < m ? min(c, y) : min(m, y));

    return (*dev_proc(dev, map_cmyk_color)) (dev, c - k, m - k, y - k, k);
}
private void
pack_from_standard(gx_device * dev, byte * dest, int destx, const byte * src,
		   int width, int depth, int src_depth)
{
    dev_proc_map_rgb_color((*map)) =
	(dev->color_info.num_components == 4 ?
	 map_rgb_to_color_via_cmyk : dev_proc(dev, map_rgb_color));
    int bit_x = destx * depth;
    byte *dp = dest + (bit_x >> 3);
    int shift = (~bit_x & 7) + 1;
    byte buf = (shift == 8 ? 0 : *dp & (0xff00 >> shift));
    const byte *sp = src;
    int x;

    for (x = width; --x >= 0;) {
	byte vr, vg, vb;
	gx_color_value r, g, b;
	gx_color_index pixel;
	byte chop = 0x1;

	vr = *sp++;
	if (src_depth > 8) {
	    vg = *sp++;
	    vb = *sp++;
	} else
	    vb = vg = vr;
	/*
	 * We have to map back to some pixel value, even if the color
	 * isn't accurate.
	 */
	for (;;) {
	    r = gx_color_value_from_byte(vr);
	    g = gx_color_value_from_byte(vg);
	    b = gx_color_value_from_byte(vb);
	    pixel = (*map) (dev, r, g, b);
	    if (pixel != gx_no_color_index)
		break;
	    /* Reduce the color accuracy and try again. */
	    vr = (vr >= 0x80 ? vr | chop : vr & ~chop);
	    vg = (vg >= 0x80 ? vg | chop : vg & ~chop);
	    vb = (vb >= 0x80 ? vb | chop : vb & ~chop);
	    chop <<= 1;
	}
	if ((shift -= depth) >= 0)
	    buf += (byte)(pixel << shift);
	else {
	    switch (depth) {
	    default:		/* 1, 2, 4, 8 */
		*dp++ = buf;
		shift += 8;
		buf = (byte)(pixel << shift);
		break;
	    case 32:
		*dp++ = (byte)(pixel >> 24);
		*dp++ = (byte)(pixel >> 16);
	    case 16:
		*dp++ = (byte)(pixel >> 8);
		*dp++ = (byte)pixel;
		shift = 0;
	    }
	}
    }
    if (width > 0 && depth <= 8)
	*dp = (shift == 0 ? buf : buf + (*dp & ((1 << shift) - 1)));
}

int
gx_default_strip_copy_rop(gx_device * dev,
			  const byte * sdata, int sourcex,
			  uint sraster, gx_bitmap_id id,
			  const gx_color_index * scolors,
			  const gx_strip_bitmap * textures,
			  const gx_color_index * tcolors,
			  int x, int y, int width, int height,
			  int phase_x, int phase_y,
			  gs_logical_operation_t lop)
{
    /*
     * The default implementation uses get_bits_rectangle to read out the
     * pixels, the memory device implementation to do the operation, and
     * copy_color to write the pixels back.  Note that if the device is
     * already a memory device, we need to convert the pixels to standard
     * representation (8-bit gray or 24-bit RGB), do the operation, and
     * convert them back.
     */
    bool expand = gs_device_is_memory(dev);
    int depth = dev->color_info.depth;
    int rop_depth = (!expand ? depth : gx_device_has_color(dev) ? 24 : 8);
    const gx_bitmap_format_t no_expand_options =
	GB_COLORS_NATIVE | GB_ALPHA_NONE | GB_DEPTH_ALL |
	GB_PACKING_CHUNKY | GB_RETURN_ALL | GB_ALIGN_STANDARD |
	GB_OFFSET_0 | GB_OFFSET_ANY | GB_RASTER_STANDARD;
    const gx_bitmap_format_t expand_options =
	(rop_depth > 8 ? GB_COLORS_RGB : GB_COLORS_GRAY) |
	GB_ALPHA_NONE | GB_DEPTH_8 |
	GB_PACKING_CHUNKY | GB_RETURN_COPY | GB_ALIGN_STANDARD |
	GB_OFFSET_0 | GB_RASTER_STANDARD;
    gs_memory_t *mem = (dev->memory ? dev->memory : &gs_memory_default);
    const gx_device_memory *mdproto = gdev_mem_device_for_bits(rop_depth);
    gx_device_memory mdev;
    uint draster = bitmap_raster(width * depth);
    bool uses_d = rop3_uses_D(gs_transparent_rop(lop));
    bool expand_s, expand_t;
    byte *row = 0;
    byte *dest_row = 0;
    byte *source_row = 0;
    byte *texture_row = 0;
    uint traster;
    gx_color_index source_colors[2];
    const gx_color_index *real_scolors = scolors;
    gx_color_index texture_colors[2];
    const gx_color_index *real_tcolors = tcolors;
    gx_strip_bitmap rop_texture;
    const gx_strip_bitmap *real_texture = textures;
    gs_int_rect rect;
    gs_get_bits_params_t bit_params;
    int code;
    int py;

#ifdef DEBUG
    if (gs_debug_c('b'))
	trace_copy_rop("gx_default_strip_copy_rop",
		       dev, sdata, sourcex, sraster,
		       id, scolors, textures, tcolors,
		       x, y, width, height, phase_x, phase_y, lop);
#endif
    if (mdproto == 0)
	return_error(gs_error_rangecheck);
    if (sdata == 0) {
	fit_fill(dev, x, y, width, height);
    } else {
	fit_copy(dev, sdata, sourcex, sraster, id, x, y, width, height);
    }
    gs_make_mem_device(&mdev, mdproto, 0, -1, (expand ? NULL : dev));
    mdev.width = width;
    mdev.height = 1;
    mdev.bitmap_memory = mem;
    if (expand) {
	if (rop_depth == 8) {
	    /* Mark 8-bit standard device as gray-scale only. */
	    mdev.color_info.num_components = 1;
	}
    } else
	mdev.color_info = dev->color_info;
    code = (*dev_proc(&mdev, open_device)) ((gx_device *) & mdev);
    if (code < 0)
	return code;

#define ALLOC_BUF(buf, size, cname)\
	BEGIN\
	  buf = gs_alloc_bytes(mem, size, cname);\
	  if ( buf == 0 ) {\
	    code = gs_note_error(gs_error_VMerror);\
	    goto out;\
	  }\
	END

    ALLOC_BUF(row, draster, "copy_rop row");
    if (expand) {
	/* We may need intermediate buffers for all 3 operands. */
	ALLOC_BUF(dest_row, bitmap_raster(width * rop_depth),
		  "copy_rop dest_row");
	expand_s = scolors == 0 && rop3_uses_S(gs_transparent_rop(lop));
	if (expand_s) {
	    ALLOC_BUF(source_row, bitmap_raster(width * rop_depth),
		      "copy_rop source_row");
	}
	if (scolors) {
	    unpack_colors_to_standard(dev, source_colors, scolors, rop_depth);
	    real_scolors = source_colors;
	}
	expand_t = tcolors == 0 && rop3_uses_T(gs_transparent_rop(lop));
	if (expand_t) {
	    traster = bitmap_raster(textures->rep_width * rop_depth);
	    ALLOC_BUF(texture_row, traster, "copy_rop texture_row");
	    rop_texture = *textures;
	    rop_texture.data = texture_row;
	    rop_texture.raster = traster;
	    rop_texture.size.x = rop_texture.rep_width;
	    rop_texture.id = gs_no_bitmap_id;
	    real_texture = &rop_texture;
	}
	if (tcolors) {
	    unpack_colors_to_standard(dev, texture_colors, tcolors, rop_depth);
	    real_tcolors = texture_colors;
	}
    } else {
	dest_row = row;
    }

#undef ALLOC_BUF

    rect.p.x = x;
    rect.q.x = x + width;

    for (py = y; py < y + height; ++py) {
	byte *data;
	int sx = sourcex;
	const byte *source_data = sdata + (py - y) * sraster;

	rect.p.y = py;
	rect.q.y = py + 1;

	if (uses_d) {
	    bit_params.options =
		(expand ? expand_options : no_expand_options);
	    bit_params.data[0] = dest_row;
	    bit_params.x_offset = 0;
	    code = (*dev_proc(dev, get_bits_rectangle))
		(dev, &rect, &bit_params, NULL);
	    if (code < 0)
		break;
	    code = (*dev_proc(&mdev, copy_color))
		((gx_device *)&mdev, bit_params.data[0], bit_params.x_offset,
		 draster /*irrelevant*/, gx_no_bitmap_id, 0, 0, width, 1);
	    if (code < 0)
		return code;
	}
	if (expand) {
	    /* Convert the source and texture to standard format. */
	    if (expand_s) {
		unpack_to_standard(dev, source_row, source_data, sx, width,
				   expand_options, no_expand_options);
		source_data = source_row;
		sx = 0;
	    }
	    if (expand_t) {
		int rep_y = (phase_y + py) % rop_texture.rep_height;

		unpack_to_standard(dev, texture_row,
				   textures->data +
				   rep_y * textures->raster,
				   0, textures->rep_width,
				   expand_options, no_expand_options);
		/*
		 * Compensate for the addition of rep_y * raster
		 * in the subsidiary strip_copy_rop call.
		 */
		rop_texture.data = texture_row - rep_y * rop_texture.raster;
	    }
	}
	code = (*dev_proc(&mdev, strip_copy_rop)) ((gx_device *) & mdev,
		     source_data, sx, sraster /*ad lib */ , gx_no_bitmap_id,
				   real_scolors, real_texture, real_tcolors,
				  0, 0, width, 1, phase_x + x, phase_y + py,
						   lop);
	if (code < 0)
	    break;
	data = scan_line_base(&mdev, 0);
	if (expand) {
	    /* Convert the result back to the device's format. */
	    pack_from_standard(dev, row, 0, data, width, depth, rop_depth);
	    data = row;
	}
	code = (*dev_proc(dev, copy_color))(dev,
					    data, 0, draster, gx_no_bitmap_id,
					    x, py, width, 1);
	if (code < 0)
	    break;
    }
  out:
    if (dest_row != row) {
	gs_free_object(mem, texture_row, "copy_rop texture_row");
	gs_free_object(mem, source_row, "copy_rop source_row");
	gs_free_object(mem, dest_row, "copy_rop dest_row");
    }
    gs_free_object(mem, row, "copy_rop row");
    (*dev_proc(&mdev, close_device)) ((gx_device *) & mdev);
    return code;
}

/* ------ Implementation of related functions ------ */

int
gx_default_copy_rop(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
		    const gx_color_index * scolors,
	     const gx_tile_bitmap * texture, const gx_color_index * tcolors,
		    int x, int y, int width, int height,
		    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    const gx_strip_bitmap *textures;
    gx_strip_bitmap tiles;

    if (texture == 0)
	textures = 0;
    else {
	*(gx_tile_bitmap *) & tiles = *texture;
	tiles.rep_shift = tiles.shift = 0;
	textures = &tiles;
    }
    return (*dev_proc(dev, strip_copy_rop))
	(dev, sdata, sourcex, sraster, id, scolors, textures, tcolors,
	 x, y, width, height, phase_x, phase_y, lop);
}

int
gx_copy_rop_unaligned(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
		      const gx_color_index * scolors,
	     const gx_tile_bitmap * texture, const gx_color_index * tcolors,
		      int x, int y, int width, int height,
		      int phase_x, int phase_y, gs_logical_operation_t lop)
{
    const gx_strip_bitmap *textures;
    gx_strip_bitmap tiles;

    if (texture == 0)
	textures = 0;
    else {
	*(gx_tile_bitmap *) & tiles = *texture;
	tiles.rep_shift = tiles.shift = 0;
	textures = &tiles;
    }
    return gx_strip_copy_rop_unaligned
	(dev, sdata, sourcex, sraster, id, scolors, textures, tcolors,
	 x, y, width, height, phase_x, phase_y, lop);
}

int
gx_strip_copy_rop_unaligned(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
			    const gx_color_index * scolors,
	   const gx_strip_bitmap * textures, const gx_color_index * tcolors,
			    int x, int y, int width, int height,
		       int phase_x, int phase_y, gs_logical_operation_t lop)
{
    dev_proc_strip_copy_rop((*copy_rop)) = dev_proc(dev, strip_copy_rop);
    int depth = (scolors == 0 ? dev->color_info.depth : 1);
    int step = sraster & (align_bitmap_mod - 1);

    /* Adjust the origin. */
    if (sdata != 0) {
	uint offset =
	(uint) (sdata - (const byte *)0) & (align_bitmap_mod - 1);

	/* See copy_color above re the following statement. */
	if (depth == 24)
	    offset += (offset % 3) *
		(align_bitmap_mod * (3 - (align_bitmap_mod % 3)));
	sdata -= offset;
	sourcex += (offset << 3) / depth;
    }
    /* Adjust the raster. */
    if (!step || sdata == 0 ||
	(scolors != 0 && scolors[0] == scolors[1])
	) {			/* No adjustment needed. */
	return (*copy_rop) (dev, sdata, sourcex, sraster, id, scolors,
			    textures, tcolors, x, y, width, height,
			    phase_x, phase_y, lop);
    }
    /* Do the transfer one scan line at a time. */
    {
	const byte *p = sdata;
	int d = sourcex;
	int dstep = (step << 3) / depth;
	int code = 0;
	int i;

	for (i = 0; i < height && code >= 0;
	     ++i, p += sraster - step, d += dstep
	    )
	    code = (*copy_rop) (dev, p, d, sraster, gx_no_bitmap_id, scolors,
				textures, tcolors, x, y + i, width, 1,
				phase_x, phase_y, lop);
	return code;
    }
}

/* ---------------- Internal routines ---------------- */

/* Compute the effective RasterOp for the 1-bit case, */
/* taking transparency into account. */
gs_rop3_t
gs_transparent_rop(gs_logical_operation_t lop)
{
    gs_rop3_t rop = lop_rop(lop);

    /*
     * The algorithm for computing an effective RasterOp is presented,
     * albeit obfuscated, in the H-P PCL5 technical documentation.
     * Define So ("source opaque") and Po ("pattern opaque") as masks
     * that have 1-bits precisely where the source or pattern
     * respectively are not white (transparent).
     * One applies the original RasterOp to compute an intermediate
     * result R, and then computes the final result as
     * (R & M) | (D & ~M) where M depends on transparencies as follows:
     *      s_tr    p_tr    M
     *       0       0      1
     *       0       1      ~So | Po (? Po ?)
     *       1       0      So
     *       1       1      So & Po
     * The s_tr = 0, p_tr = 1 case seems wrong, but it's clearly
     * specified that way in the "PCL 5 Color Technical Reference
     * Manual."
     *
     * In the 1-bit case, So = ~S and Po = ~P, so we can apply the
     * above table directly.
     */
#define So rop3_not(rop3_S)
#define Po rop3_not(rop3_T)
#ifdef TRANSPARENCY_PER_H_P
#  define MPo (rop3_uses_S(rop) ? rop3_not(So) | Po : Po)
#else
#  define MPo Po
#endif
    /*
     * If the operation doesn't use S or T, we must disregard the
     * corresponding transparency flag.
     */
#define source_transparent ((lop & lop_S_transparent) && rop3_uses_S(rop))
#define pattern_transparent ((lop & lop_T_transparent) && rop3_uses_T(rop))
    gs_rop3_t mask =
    (source_transparent ?
     (pattern_transparent ? So & Po : So) :
     (pattern_transparent ? MPo : rop3_1));

#undef MPo
    return (rop & mask) | (rop3_D & ~mask);
}
