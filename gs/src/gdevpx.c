/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.

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

/* gdevpx.c */
/* H-P PCL XL driver */
#include "math_.h"
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsccolor.h"
#include "gsdcolor.h"
#include "gxcspace.h"		/* for color mapping for images */
#include "gxdevice.h"
#include "gxpath.h"
#include "gdevvec.h"
#include "strimpl.h"
#include "srlx.h"
#include "gdevpxat.h"
#include "gdevpxen.h"
#include "gdevpxop.h"

/* ---------------- Device definition ---------------- */

/* Define the default resolution. */
#ifndef X_DPI
#  define X_DPI 600
#endif
#ifndef Y_DPI
#  define Y_DPI 600
#endif

/* Structure definition */
#define num_points 100		/* must be >= 3 and <= 255 */
typedef enum {
    points_none,
    points_lines,
    points_curves
} point_type_t;
typedef struct gx_device_pclxl_s {
    gx_device_vector_common;
    /* Additional state information */
    pxeMediaSize_t media_size;
    gx_path_type_t fill_rule;	/* ...winding_number or ...even_odd  */
    gx_path_type_t clip_rule;	/* ditto */
    pxeColorSpace_t color_space;
    struct pal_ {
	int size;		/* # of bytes */
	byte data[256 * 3];	/* up to 8-bit samples */
    } palette;
    struct pts_ {		/* buffer for accumulating path points */
	gs_int_point current;	/* current point as of start of data */
	point_type_t type;
	int count;
	gs_int_point data[num_points];
    } points;
    struct ch_ {		/* cache for downloaded characters */
#define max_cached_chars 400
#define max_char_data 500000
#define max_char_size 5000
#define char_hash_factor 247
	ushort table[max_cached_chars * 3 / 2];
	struct cd_ {
	    gs_id id;		/* key */
	    uint size;
	} data[max_cached_chars];
	int next_in;		/* next data element to fill in */
	int next_out;		/* next data element to discard */
	int count;		/* of occupied data elements */
	ulong used;
    } chars;
    bool font_set;
} gx_device_pclxl;

gs_public_st_suffix_add0_final(st_device_pclxl, gx_device_pclxl,
	 "gx_device_pclxl", device_pclxl_enum_ptrs, device_pclxl_reloc_ptrs,
			       gx_device_finalize, st_device_vector);

#define pclxl_device_body(dname, depth)\
  std_device_dci_type_body(gx_device_pclxl, 0, dname, &st_device_pclxl,\
			   DEFAULT_WIDTH_10THS * X_DPI / 10,\
			   DEFAULT_HEIGHT_10THS * Y_DPI / 10,\
			   X_DPI, Y_DPI,\
			   (depth > 8 ? 3 : 1), depth,\
			   (depth > 1 ? 255 : 1), (depth > 8 ? 255 : 0),\
			   (depth > 1 ? 256 : 2), (depth > 8 ? 256 : 1))

/* Driver procedures */
private dev_proc_open_device(pclxl_open_device);
private dev_proc_output_page(pclxl_output_page);
private dev_proc_close_device(pclxl_close_device);
private dev_proc_copy_mono(pclxl_copy_mono);
private dev_proc_copy_color(pclxl_copy_color);
private dev_proc_fill_mask(pclxl_fill_mask);

/*private dev_proc_draw_thin_line(pclxl_draw_thin_line); */
private dev_proc_begin_image(pclxl_begin_image);
private dev_proc_image_data(pclxl_image_data);
private dev_proc_end_image(pclxl_end_image);
private dev_proc_strip_copy_rop(pclxl_strip_copy_rop);

#define pclxl_device_procs(map_rgb_color, map_color_rgb)\
	{	pclxl_open_device,\
		NULL,			/* get_initial_matrix */\
		NULL,			/* sync_output */\
		pclxl_output_page,\
		pclxl_close_device,\
		map_rgb_color,		/* differs */\
		map_color_rgb,		/* differs */\
		gdev_vector_fill_rectangle,\
		NULL,			/* tile_rectangle */\
		pclxl_copy_mono,\
		pclxl_copy_color,\
		NULL,			/* draw_line */\
		NULL,			/* get_bits */\
		gdev_vector_get_params,\
		gdev_vector_put_params,\
		NULL,			/* map_cmyk_color */\
		NULL,			/* get_xfont_procs */\
		NULL,			/* get_xfont_device */\
		NULL,			/* map_rgb_alpha_color */\
		gx_page_device_get_page_device,\
		NULL,			/* get_alpha_bits */\
		NULL,			/* copy_alpha */\
		NULL,			/* get_band */\
		NULL,			/* copy_rop */\
		gdev_vector_fill_path,\
		gdev_vector_stroke_path,\
		pclxl_fill_mask,\
		gdev_vector_fill_trapezoid,\
		gdev_vector_fill_parallelogram,\
		gdev_vector_fill_triangle,\
		NULL /****** WRONG ******/,	/* draw_thin_line */\
		pclxl_begin_image,\
		pclxl_image_data,\
		pclxl_end_image,\
		NULL,			/* strip_tile_rectangle */\
		pclxl_strip_copy_rop\
	}

gx_device_pclxl far_data gs_pxlmono_device =
{
    pclxl_device_body("pxlmono", 8),
    pclxl_device_procs(gx_default_gray_map_rgb_color, gx_default_gray_map_color_rgb)
};

gx_device_pclxl far_data gs_pxlcolor_device =
{
    pclxl_device_body("pxlcolor", 24),
    pclxl_device_procs(gx_default_rgb_map_rgb_color, gx_default_rgb_map_color_rgb)
};

/* ---------------- Output utilities ---------------- */

/* Write a sequence of bytes. */
#define put_lit(s, bytes) put_bytes(s, bytes, sizeof(bytes))
private void
put_bytes(stream * s, const byte * data, uint count)
{
    uint used;

    sputs(s, data, count, &used);
}

/* Utilities for writing data values. */
/* H-P printers only support little-endian data, so that's what we emit. */
#define da(a) pxt_attr_ubyte, (a)
private void
put_a(stream * s, px_attribute_t a)
{
    sputc(s, pxt_attr_ubyte);
    sputc(s, a);
}
#define dub(b) pxt_ubyte, (byte)(b)
private void
put_ub(stream * s, byte b)
{
    sputc(s, pxt_ubyte);
    sputc(s, b);
}
#define put_uba(s, b, a)\
	(put_ub(s, b), put_a(s, a))
#define ds(i) (byte)(i), (byte)((i) >> 8)
private void
put_s(stream * s, uint i)
{
    sputc(s, (byte) i);
    sputc(s, (byte) (i >> 8));
}
#define dus(i) pxt_uint16, ds(i)
private void
put_us(stream * s, uint i)
{
    sputc(s, pxt_uint16);
    put_s(s, i);
}
#define put_usa(s, i, a)\
	(put_us(s, i), put_a(s, a))
private void
put_u(stream * s, uint i)
{
    if (i <= 255)
	put_ub(s, i);
    else
	put_us(s, i);
}
#define dusp(ix,iy) pxt_uint16_xy, ds(ix), ds(iy)
private void
put_usp(stream * s, uint ix, uint iy)
{
    sputc(s, pxt_uint16_xy);
    put_s(s, ix);
    put_s(s, iy);
}
private void
put_usq_fixed(stream * s, fixed x0, fixed y0, fixed x1, fixed y1)
{
    sputc(s, pxt_uint16_box);
    put_s(s, fixed2int(x0));
    put_s(s, fixed2int(y0));
    put_s(s, fixed2int(x1));
    put_s(s, fixed2int(y1));
}
#define dss(i) pxt_sint16, ds(i)
private void
put_ss(stream * s, int i)
{
    sputc(s, pxt_sint16);
    put_s(s, (uint) i);
}
#define dssp(ix,iy) pxt_sint16_xy, ds(ix), ds(iy)
private void
put_ssp(stream * s, int ix, int iy)
{
    sputc(s, pxt_sint16_xy);
    put_s(s, (uint) ix);
    put_s(s, (uint) iy);
}
#define dl(l) ds(l), ds((l) >> 16)
private void
put_l(stream * s, ulong l)
{
    put_s(s, (uint) l);
    put_s(s, (uint) (l >> 16));
}
private void
put_r(stream * s, floatp r)
{				/* Convert to single-precision IEEE float. */
    int exp;
    long mantissa = (long)(frexp(r, &exp) * 0x1000000);

    if (exp < -126)
	mantissa = 0, exp = 0;	/* unnormalized */
    if (mantissa < 0)
	exp += 128, mantissa = -mantissa;
    /* All quantities are little-endian. */
    spputc(s, (byte) mantissa);
    spputc(s, (byte) (mantissa >> 8));
    spputc(s, (byte) (((exp + 127) << 7) + ((mantissa >> 16) & 0x7f)));
    spputc(s, (exp + 127) >> 1);
}
private void
put_rl(stream * s, floatp r)
{
    sputc(s, pxt_real32);
    put_r(s, r);
}
private void
put_data_length(stream * s, uint num_bytes)
{
    if (num_bytes > 255) {
	spputc(s, pxt_dataLength);
	put_l(s, (ulong) num_bytes);
    } else {
	spputc(s, pxt_dataLengthByte);
	spputc(s, (byte) num_bytes);
    }
}

#define put_ac(s, a, op)\
do { static const byte ac_[] = { da(a), op }; put_lit(s, ac_); } while ( 0 )
#define return_put_ac(s, a, op)\
do { put_ac(s, a, op); return 0; } while ( 0 )

/* ---------------- Other utilities ---------------- */

#define vxdev ((gx_device_vector *)xdev)

/* Initialize for a page. */
private void
pclxl_page_init(gx_device_pclxl * xdev)
{
    gdev_vector_init(vxdev);
    xdev->in_page = false;
    xdev->fill_rule = gx_path_type_winding_number;
    xdev->clip_rule = gx_path_type_winding_number;
    xdev->color_space = eNoColorSpace;
    xdev->palette.size = 0;
    xdev->font_set = false;
}

/* Test whether a RGB color is actually a gray shade. */
#define rgb_is_gray(ci) ((ci) >> 8 == ((ci) & 0xffff))

/* Set the color space and (optionally) palette. */
private void
pclxl_set_color_space(gx_device_pclxl * xdev, pxeColorSpace_t color_space)
{
    if (xdev->color_space != color_space) {
	stream *s = gdev_vector_stream(vxdev);

	put_ub(s, color_space);
	put_ac(s, pxaColorSpace, pxtSetColorSpace);
	xdev->color_space = color_space;
    }
}
private void
pclxl_set_color_palette(gx_device_pclxl * xdev, pxeColorSpace_t color_space,
			const byte * palette, uint palette_size)
{
    if (xdev->color_space != color_space ||
	xdev->palette.size != palette_size ||
	memcmp(xdev->palette.data, palette, palette_size)
	) {
	stream *s = gdev_vector_stream(vxdev);
	static const byte csp_[] =
	{
	    da(pxaColorSpace),
	    dub(e8Bit), da(pxaPaletteDepth),
	    pxt_ubyte_array
	};

	put_ub(s, color_space);
	put_lit(s, csp_);
	put_u(s, palette_size);
	put_bytes(s, palette, palette_size);
	put_ac(s, pxaPaletteData, pxtSetColorSpace);
	xdev->color_space = color_space;
	xdev->palette.size = palette_size;
	memcpy(xdev->palette.data, palette, palette_size);
    }
}

/* Set a drawing RGB color. */
private int
pclxl_set_color(gx_device_pclxl * xdev, const gx_drawing_color * pdc,
		px_attribute_t null_source, px_tag_t op)
{
    stream *s = gdev_vector_stream(vxdev);

    if (gx_dc_is_pure(pdc)) {
	gx_color_index color = gx_dc_pure_color(pdc);

	if (xdev->color_info.num_components == 1 || rgb_is_gray(color)) {
	    pclxl_set_color_space(xdev, eGray);
	    put_uba(s, (byte) color, pxaGrayLevel);
	} else {
	    pclxl_set_color_space(xdev, eRGB);
	    spputc(s, pxt_ubyte_array);
	    put_ub(s, 3);
	    spputc(s, (byte) (color >> 16));
	    spputc(s, (byte) (color >> 8));
	    spputc(s, (byte) color);
	    put_a(s, pxaRGBColor);
	}
    } else if (gx_dc_is_null(pdc))
	put_uba(s, 0, null_source);
    else
	return_error(gs_error_rangecheck);
    spputc(s, op);
    return 0;
}

/* Test whether we can handle a given color space in an image. */
private bool
pclxl_can_handle_color_space(const gs_color_space * pcs)
{
    gs_color_space_index index = gs_color_space_get_index(pcs);

    if (index == gs_color_space_index_Indexed) {
	if (pcs->params.indexed.use_proc)
	    return false;
	index =
	    gs_color_space_get_index(gs_color_space_indexed_base_space(pcs));
    }
    return !(index == gs_color_space_index_Separation ||
	     index == gs_color_space_index_Pattern);
}

/* Set brush, pen, and mode for painting a path. */
private void
pclxl_set_paints(gx_device_pclxl * xdev, gx_path_type_t type)
{
    stream *s = gdev_vector_stream(vxdev);
    gx_path_type_t rule = type & gx_path_type_rule;

    if (!(type & gx_path_type_fill) &&
	!gx_dc_is_null(&xdev->fill_color)
	) {
	static const byte nac_[] =
	{
	    dub(0), da(pxaNullBrush), pxtSetBrushSource
	};

	put_lit(s, nac_);
	color_set_null(&xdev->fill_color);
	if (rule != xdev->fill_rule) {
	    put_ub(s, (rule == gx_path_type_even_odd ? eEvenOdd :
		       eNonZeroWinding));
	    put_ac(s, pxaFillMode, pxtSetFillMode);
	    xdev->fill_rule = rule;
	}
    }
    if (!(type & gx_path_type_stroke) &&
	!gx_dc_is_null(&xdev->stroke_color)
	) {
	static const byte nac_[] =
	{
	    dub(0), da(pxaNullPen), pxtSetPenSource
	};

	put_lit(s, nac_);
	color_set_null(&xdev->stroke_color);
    }
}

/* Set the cursor. */
private int
pclxl_set_cursor(gx_device_pclxl * xdev, int x, int y)
{
    stream *s = gdev_vector_stream(vxdev);

    put_ssp(s, x, y);
    return_put_ac(s, pxaPoint, pxtSetCursor);
}

/* ------ Paths ------ */

/* Flush any buffered path points. */
private void
put_np(stream * s, int count, pxeDataType_t dtype)
{
    put_uba(s, count, pxaNumberOfPoints);
    put_uba(s, dtype, pxaPointType);
}
private int
pclxl_flush_points(gx_device_pclxl * xdev)
{
    int count = xdev->points.count;

    if (count) {
	stream *s = gdev_vector_stream(vxdev);
	px_tag_t op;
	int x = xdev->points.current.x, y = xdev->points.current.y;
	int uor = 0, sor = 0;
	pxeDataType_t data_type;
	int i, di;
	byte diffs[num_points * 2];

	/*
	 * Writing N lines using a point list requires 11 + 4*N or 11 +
	 * 2*N bytes, as opposed to 8*N bytes using separate commands;
	 * writing N curves requires 11 + 12*N or 11 + 6*N bytes
	 * vs. 22*N.  So it's always shorter to write curves with a
	 * list (except for N = 1 with full-size coordinates, but since
	 * the difference is only 1 byte, we don't bother to ever use
	 * the non-list form), but lines are shorter only if N >= 3
	 * (again, with a 1-byte difference if N = 2 and byte
	 * coordinates).
	 */
	switch (xdev->points.type) {
	    case points_none:
		return 0;
	    case points_lines:
		op = pxtLinePath;
		if (count < 3) {
		    for (i = 0; i < count; ++i) {
			put_ssp(s, xdev->points.data[i].x,
				xdev->points.data[i].y);
			put_a(s, pxaEndPoint);
			spputc(s, op);
		    }
		    goto zap;
		}
		/* See if we can use byte values. */
		for (i = di = 0; i < count; ++i, di += 2) {
		    int dx = xdev->points.data[i].x - x;
		    int dy = xdev->points.data[i].y - y;

		    diffs[di] = (byte) dx;
		    diffs[di + 1] = (byte) dy;
		    uor |= dx | dy;
		    sor |= (dx + 0x80) | (dy + 0x80);
		    x += dx, y += dy;
		}
		if (!(uor & ~0xff))
		    data_type = eUByte;
		else if (!(sor & ~0xff))
		    data_type = eSByte;
		else
		    break;
		op = pxtLineRelPath;
		/* Use byte values. */
	      useb:put_np(s, count, data_type);
		spputc(s, op);
		put_data_length(s, count * 2);	/* 2 bytes per point */
		put_bytes(s, diffs, count * 2);
		goto zap;
	    case points_curves:
		op = pxtBezierPath;
		/* See if we can use byte values. */
		for (i = di = 0; i < count; i += 3, di += 6) {
		    int dx1 = xdev->points.data[i].x - x;
		    int dy1 = xdev->points.data[i].y - y;
		    int dx2 = xdev->points.data[i + 1].x - x;
		    int dy2 = xdev->points.data[i + 1].y - y;
		    int dx = xdev->points.data[i + 2].x - x;
		    int dy = xdev->points.data[i + 2].y - y;

		    diffs[di] = (byte) dx1;
		    diffs[di + 1] = (byte) dy1;
		    diffs[di + 2] = (byte) dx2;
		    diffs[di + 3] = (byte) dy2;
		    diffs[di + 4] = (byte) dx;
		    diffs[di + 5] = (byte) dy;
		    uor |= dx1 | dy1 | dx2 | dy2 | dx | dy;
		    sor |= (dx1 + 0x80) | (dy1 + 0x80) |
			(dx2 + 0x80) | (dy2 + 0x80) |
			(dx + 0x80) | (dy + 0x80);
		    x += dx, y += dy;
		}
		if (!(uor & ~0xff))
		    data_type = eUByte;
		else if (!(sor & ~0xff))
		    data_type = eSByte;
		else
		    break;
		op = pxtBezierRelPath;
		goto useb;
	    default:		/* can't happen */
		return_error(gs_error_unknownerror);
	}
	put_np(s, count, eSInt16);
	spputc(s, op);
	put_data_length(s, count * 4);	/* 2 UInt16s per point */
	for (i = 0; i < count; ++i) {
	    put_s(s, xdev->points.data[i].x);
	    put_s(s, xdev->points.data[i].y);
	}
      zap:xdev->points.type = points_none;
	xdev->points.count = 0;
    }
    return 0;
}

/* ------ Images ------ */

/* Begin an image. */
private void
pclxl_write_begin_image(gx_device_pclxl * xdev, uint width, uint height,
			uint dest_width, uint dest_height)
{
    stream *s = gdev_vector_stream(vxdev);

    put_usa(s, width, pxaSourceWidth);
    put_usa(s, height, pxaSourceHeight);
    put_usp(s, dest_width, dest_height);
    put_ac(s, pxaDestinationSize, pxtBeginImage);
}

/* Write rows of an image. */
/****** IGNORES data_bit ******/
private void
pclxl_write_image_data(gx_device_pclxl * xdev, const byte * data, int data_bit,
		       uint raster, uint width_bits, int y, int height)
{
    stream *s = gdev_vector_stream(vxdev);
    uint width_bytes = (width_bits + 7) >> 3;
    bool compress = width_bytes >= 8;
    uint num_bytes = round_up(width_bytes, 4) * height;
    int i;

    put_usa(s, y, pxaStartLine);
    put_usa(s, height, pxaBlockHeight);
    if (compress) {
	stream_RLE_state rlstate;
	stream_cursor_write w;
	stream_cursor_read r;

	/*
	 * H-P printers required that all the data for an operator be
	 * contained in a single data block.  Thus, we must allocate
	 * a temporary buffer for the compressed data.  Currently we
	 * don't go to the trouble of breaking the data up into scan
	 * lines if we can't allocate a buffer large enough for the
	 * entire transfer.
	 */
	byte *buf = gs_alloc_bytes(xdev->v_memory, num_bytes,
				   "pclxl_write_image_data");

	if (buf == 0)
	    goto nc;
	s_RLE_set_defaults_inline(&rlstate);
	rlstate.EndOfData = false;
	s_RLE_init_inline(&rlstate);
	w.ptr = buf - 1;
	w.limit = w.ptr + num_bytes;
	/*
	 * If we ever overrun the buffer, it means that the compressed
	 * data was larger than the uncompressed.  If this happens,
	 * write the data uncompressed.
	 */
	for (i = 0; i < height; ++i) {
	    r.ptr = data + i * raster - 1;
	    r.limit = r.ptr + width_bytes;
	    if ((*s_RLE_template.process)
		((stream_state *) & rlstate, &r, &w, false) != 0 ||
		r.ptr != r.limit
		)
		goto ncfree;
	    r.ptr = (const byte *)"\000\000\000\000\000";
	    r.limit = r.ptr + (-width_bytes & 3);
	    if ((*s_RLE_template.process)
		((stream_state *) & rlstate, &r, &w, false) != 0 ||
		r.ptr != r.limit
		)
		goto ncfree;
	}
	r.ptr = r.limit;
	if ((*s_RLE_template.process)
	    ((stream_state *) & rlstate, &r, &w, true) != 0
	    )
	    goto ncfree;
	{
	    uint count = w.ptr + 1 - buf;

	    put_ub(s, eRLECompression);
	    put_ac(s, pxaCompressMode, pxtReadImage);
	    put_data_length(s, count);
	    put_bytes(s, buf, count);
	}
	return;
      ncfree:gs_free_object(xdev->v_memory, buf, "pclxl_write_image_data");
      nc:;
    }
    /* Write the data uncompressed. */
    put_ub(s, eNoCompression);
    put_ac(s, pxaCompressMode, pxtReadImage);
    put_data_length(s, num_bytes);
    for (i = 0; i < height; ++i) {
	put_bytes(s, data + i * raster, width_bytes);
	put_bytes(s, (const byte *)"\000\000\000\000", -width_bytes & 3);
    }
}

/* End an image. */
private void
pclxl_write_end_image(gx_device_pclxl * xdev)
{
    spputc(xdev->strm, pxtEndImage);
}

/* ------ Fonts ------ */

/* Write a string (single- or double-byte). */
private void
put_string(stream * s, const byte * data, uint len, bool wide)
{
    if (wide) {
	spputc(s, pxt_uint16_array);
	put_u(s, len);
	put_bytes(s, data, len * 2);
    } else {
	spputc(s, pxt_ubyte_array);
	put_u(s, len);
	put_bytes(s, data, len);
    }
}

/* Write a 16-bit big-endian value. */
private void
put_us_be(stream * s, uint i)
{
    spputc(s, (byte) (i >> 8));
    spputc(s, (byte) i);
}

/* Define a bitmap font.  The client must call put_string */
/* with the font name immediately before calling this procedure. */
private void
pclxl_define_bitmap_font(gx_device_pclxl * xdev)
{
    stream *s = gdev_vector_stream(vxdev);
    static const byte bfh_[] =
    {
	da(pxaFontName), dub(0), da(pxaFontFormat),
	pxtBeginFontHeader,
	dus(8 + 6 + 4 + 6), da(pxaFontHeaderLength),
	pxtReadFontHeader,
	pxt_dataLengthByte, 8 + 6 + 4 + 6,
	0, 0, 0, 0,
	254, 0, (max_cached_chars + 255) >> 8, 0,
	'B', 'R', 0, 0, 0, 4
    };
    static const byte efh_[] =
    {
	0xff, 0xff, 0, 0, 0, 0,
	pxtEndFontHeader
    };

    put_lit(s, bfh_);
    put_us_be(s, (uint) (xdev->HWResolution[0] + 0.5));
    put_us_be(s, (uint) (xdev->HWResolution[1] + 0.5));
    put_lit(s, efh_);
}

/* Set the font.  The client must call put_string */
/* with the font name immediately before calling this procedure. */
private void
pclxl_set_font(gx_device_pclxl * xdev)
{
    stream *s = gdev_vector_stream(vxdev);
    static const byte sf_[] =
    {
	da(pxaFontName), dub(1), da(pxaCharSize), dus(0), da(pxaSymbolSet),
	pxtSetFont
    };

    put_lit(s, sf_);
}

/* Define a character in a bitmap font.  The client must call put_string */
/* with the font name immediately before calling this procedure. */
private void
pclxl_define_bitmap_char(gx_device_pclxl * xdev, uint ccode,
	       const byte * data, uint raster, uint width_bits, uint height)
{
    stream *s = gdev_vector_stream(vxdev);
    uint width_bytes = (width_bits + 7) >> 3;
    uint size = 10 + width_bytes * height;
    uint i;

    put_ac(s, pxaFontName, pxtBeginChar);
    put_u(s, ccode);
    put_a(s, pxaCharCode);
    if (size > 0xffff) {
	spputc(s, pxt_uint32);
	put_l(s, (ulong) size);
    } else
	put_us(s, size);
    put_ac(s, pxaCharDataSize, pxtReadChar);
    put_data_length(s, size);
    put_bytes(s, (const byte *)"\000\000\000\000\000\000", 6);
    put_us_be(s, width_bits);
    put_us_be(s, height);
    for (i = 0; i < height; ++i)
	put_bytes(s, data + i * raster, width_bytes);
    spputc(s, pxtEndChar);
}

/* Write the name of the only font we define. */
private void
pclxl_write_font_name(gx_device_pclxl * xdev)
{
    stream *s = gdev_vector_stream(vxdev);

    put_string(s, (const byte *)"@", 1, false);
}

/* Look up a bitmap id, return the index in the character table. */
/* If the id is missing, return an index for inserting. */
private int
pclxl_char_index(gx_device_pclxl * xdev, gs_id id)
{
    int i, i_empty = -1;
    uint ccode;

    for (i = (id * char_hash_factor) % countof(xdev->chars.table);;
	 i = (i == 0 ? countof(xdev->chars.table) : i) - 1
	) {
	ccode = xdev->chars.table[i];
	if (ccode == 0)
	    return (i_empty >= 0 ? i_empty : i);
	else if (ccode == 1) {
	    if (i_empty < 0)
		i_empty = i;
	    else if (i == i_empty)	/* full table */
		return i;
	} else if (xdev->chars.data[ccode].id == id)
	    return i;
    }
}

/* Remove the character table entry at a given index. */
private void
pclxl_remove_char(gx_device_pclxl * xdev, int index)
{
    uint ccode = xdev->chars.table[index];
    int i;

    if (ccode < 2)
	return;
    xdev->chars.count--;
    xdev->chars.used -= xdev->chars.data[ccode].size;
    xdev->chars.table[index] = 1;	/* mark as deleted */
    i = (index == 0 ? countof(xdev->chars.table) : index) - 1;
    if (xdev->chars.table[i] == 0) {
	/* The next slot in probe order is empty. */
	/* Mark this slot and any deleted predecessors as empty. */
	for (i = index; xdev->chars.table[i] == 1;
	     i = (i == countof(xdev->chars.table) - 1 ? 0 : i + 1)
	    )
	    xdev->chars.table[i] = 0;
    }
}

/* Write a bitmap as a text character if possible. */
/* The caller must set the color, cursor, and RasterOp. */
/* We know id != gs_no_id. */
private int
pclxl_copy_text_char(gx_device_pclxl * xdev, const byte * data,
		     int raster, gx_bitmap_id id, int w, int h)
{
    uint width_bytes = (w + 7) >> 3;
    uint size = width_bytes * h;
    int index;
    uint ccode;
    stream *s = gdev_vector_stream(vxdev);

    if (size > max_char_size)
	return -1;
    index = pclxl_char_index(xdev, id);
    if ((ccode = xdev->chars.table[index]) < 2) {
	/* Enter the character in the table. */
	while (xdev->chars.used + size > max_char_data ||
	       xdev->chars.count >= max_cached_chars - 2
	    ) {
	    ccode = xdev->chars.next_out;
	    index = pclxl_char_index(xdev, xdev->chars.data[ccode].id);
	    pclxl_remove_char(xdev, index);
	    xdev->chars.next_out =
		(ccode == max_cached_chars - 1 ? 2 : ccode + 1);
	}
	index = pclxl_char_index(xdev, id);
	ccode = xdev->chars.next_in;
	xdev->chars.data[ccode].id = id;
	xdev->chars.data[ccode].size = size;
	xdev->chars.table[index] = ccode;
	xdev->chars.next_in =
	    (ccode == max_cached_chars - 1 ? 2 : ccode + 1);
	if (!xdev->chars.count++) {
	    /* This is the very first character. */
	    pclxl_write_font_name(xdev);
	    pclxl_define_bitmap_font(xdev);
	}
	xdev->chars.used += size;
	pclxl_write_font_name(xdev);
	pclxl_define_bitmap_char(xdev, ccode, data, raster, w, h);
    }
    if (!xdev->font_set) {
	pclxl_write_font_name(xdev);
	pclxl_set_font(xdev);
	xdev->font_set = true;
    } {
	byte cc_bytes[2];

	cc_bytes[0] = (byte) ccode;
	cc_bytes[1] = ccode >> 8;
	put_string(s, cc_bytes, 1, cc_bytes[1] != 0);
    }
    put_ac(s, pxaTextData, pxtText);
    return 0;
}

/* ---------------- Vector implementation procedures ---------------- */

#define xvdev ((gx_device_pclxl *)vdev)

private int
pclxl_beginpage(gx_device_vector * vdev)
{				/*
				 * We can't use gdev_vector_stream here, because this may be called
				 * from there before in_page is set.
				 */
    stream *s = vdev->strm;

    {
	static const byte page_header_1[] =
	{
	    dub(ePortraitOrientation), da(pxaOrientation),
	};

	put_lit(s, page_header_1);
    }
    {
#define msd(ms, res, w, h)\
  { ms, (w) * 1.0 / (res), (h) * 1.0 / res },
	static const struct {
	    pxeMediaSize_t ms;
	    float width, height;
	} media_sizes[] = {
	    px_enumerate_media(msd) {
		pxeMediaSize_next
	    }
	};
	float w = vdev->width / vdev->HWResolution[0], h = vdev->height / vdev->HWResolution[1];
	int i;
	pxeMediaSize_t size;

	/* The default is eLetterPaper, media size 0. */
	for (i = countof(media_sizes) - 2; i > 0; --i)
	    if (fabs(media_sizes[i].width - w) < 5.0 / 72 &&
		fabs(media_sizes[i].height - h) < 5.0 / 72
		)
		break;
	size = media_sizes[i].ms;
	/*
	 * According to the PCL XL documentation, MediaSize must always
	 * be specified, but MediaSource is optional.
	 */
	put_uba(s, size, pxaMediaSize);
	if (size != xvdev->media_size) {
	    static const byte page_header_2[] =
	    {
		dub(eAutoSelect), da(pxaMediaSource)
	    };

	    put_lit(s, page_header_2);
	    xvdev->media_size = size;
	}
    }
    spputc(s, pxtBeginPage);
    return 0;
}

private int
pclxl_setlinewidth(gx_device_vector * vdev, floatp width)
{
    stream *s = gdev_vector_stream(vdev);

    put_us(s, (uint) width);
    return_put_ac(s, pxaPenWidth, pxtSetPenWidth);
}

private int
pclxl_setlinecap(gx_device_vector * vdev, gs_line_cap cap)
{
    stream *s = gdev_vector_stream(vdev);

    /* The PCL XL cap styles just happen to be identical to PostScript. */
    put_ub(s, (byte) cap);
    return_put_ac(s, pxaLineCapStyle, pxtSetLineCap);
}

private int
pclxl_setlinejoin(gx_device_vector * vdev, gs_line_join join)
{
    stream *s = gdev_vector_stream(vdev);

    /* The PCL XL join styles just happen to be identical to PostScript. */
    put_ub(s, (byte) join);
    return_put_ac(s, pxaLineJoinStyle, pxtSetLineJoin);
}

private int
pclxl_setmiterlimit(gx_device_vector * vdev, floatp limit)
{
    stream *s = gdev_vector_stream(vdev);

    /*
     * Amazingly enough, the PCL XL specification doesn't allow real
     * numbers for the miter limit.
     */
    int i_limit = (int)(limit + 0.5);

    put_u(s, max(i_limit, 1));
    return_put_ac(s, pxaMiterLength, pxtSetMiterLimit);
}

private int
pclxl_setdash(gx_device_vector * vdev, const float *pattern, uint count,
	      floatp offset)
{
    stream *s = gdev_vector_stream(vdev);

    if (count == 0) {
	static const byte nac_[] =
	{dub(0), da(pxaSolidLine)};

	put_lit(s, nac_);
    } else if (count > 255)
	return_error(gs_error_limitcheck);
    else {
	uint i;

	spputc(s, pxt_real32_array);
	put_ub(s, count);
	for (i = 0; i < count; ++i)
	    put_r(s, pattern[i]);
	put_a(s, pxaLineDashStyle);
	if (offset != 0) {
	    put_rl(s, offset);
	    put_a(s, pxaDashOffset);
	}
    }
    spputc(s, pxtSetLineDash);
    return 0;
}

private int
pclxl_setlogop(gx_device_vector * vdev, gs_logical_operation_t lop,
	       gs_logical_operation_t diff)
{
    stream *s = gdev_vector_stream(vdev);

    if (diff & lop_S_transparent) {
	put_ub(s, (lop & lop_S_transparent ? 1 : 0));
	put_ac(s, pxaTxMode, pxtSetSourceTxMode);
    }
    if (diff & lop_T_transparent) {
	put_ub(s, (lop & lop_T_transparent ? 1 : 0));
	put_ac(s, pxaTxMode, pxtSetPaintTxMode);
    }
    if (lop_rop(diff)) {
	put_ub(s, lop_rop(lop));
	put_ac(s, pxaROP3, pxtSetROP);
    }
    return 0;
}

private int
pclxl_setfillcolor(gx_device_vector * vdev, const gx_drawing_color * pdc)
{
    return pclxl_set_color(xvdev, pdc, pxaNullBrush, pxtSetBrushSource);
}

private int
pclxl_setstrokecolor(gx_device_vector * vdev, const gx_drawing_color * pdc)
{
    return pclxl_set_color(xvdev, pdc, pxaNullPen, pxtSetPenSource);
}

private int
pclxl_dorect(gx_device_vector * vdev, fixed x0, fixed y0, fixed x1,
	     fixed y1, gx_path_type_t type)
{
    stream *s = gdev_vector_stream(vdev);

    if (type & (gx_path_type_fill | gx_path_type_stroke)) {
	pclxl_set_paints(xvdev, type);
	put_usq_fixed(s, x0, y0, x1, y1);
	put_ac(s, pxaBoundingBox, pxtRectangle);
    }
    if (type & gx_path_type_clip) {
	static const byte cr_[] =
	{
	    da(pxaBoundingBox),
	    dub(eInterior), da(pxaClipRegion),
	    pxtSetClipRectangle
	};

	put_usq_fixed(s, x0, y0, x1, y1);
	put_lit(s, cr_);
    }
    return 0;
}

private int
pclxl_beginpath(gx_device_vector * vdev, gx_path_type_t type)
{
    stream *s = gdev_vector_stream(vdev);

    spputc(s, pxtNewPath);
    xvdev->points.type = points_none;
    xvdev->points.count = 0;
    return 0;
}

private int
pclxl_moveto(gx_device_vector * vdev, floatp x0, floatp y0, floatp x, floatp y,
	     bool first)
{
    int code = pclxl_flush_points(xvdev);

    if (code < 0)
	return code;
    return pclxl_set_cursor(xvdev,
			    xvdev->points.current.x = (int)x,
			    xvdev->points.current.y = (int)y);
}

private int
pclxl_lineto(gx_device_vector * vdev, floatp x0, floatp y0, floatp x, floatp y)
{
    if (xvdev->points.type != points_lines ||
	xvdev->points.count >= num_points
	) {
	if (xvdev->points.type != points_none) {
	    int code = pclxl_flush_points(xvdev);

	    if (code < 0)
		return code;
	}
	xvdev->points.current.x = (int)x0;
	xvdev->points.current.y = (int)y0;
	xvdev->points.type = points_lines;
    } {
	gs_int_point *ppt = &xvdev->points.data[xvdev->points.count++];

	ppt->x = (int)x, ppt->y = (int)y;
    }
    return 0;
}

private int
pclxl_curveto(gx_device_vector * vdev, floatp x0, floatp y0,
	   floatp x1, floatp y1, floatp x2, floatp y2, floatp x3, floatp y3)
{
    if (xvdev->points.type != points_curves ||
	xvdev->points.count >= num_points - 2
	) {
	if (xvdev->points.type != points_none) {
	    int code = pclxl_flush_points(xvdev);

	    if (code < 0)
		return code;
	}
	xvdev->points.current.x = (int)x0;
	xvdev->points.current.y = (int)y0;
	xvdev->points.type = points_curves;
    } {
	gs_int_point *ppt = &xvdev->points.data[xvdev->points.count];

	ppt->x = (int)x1, ppt->y = (int)y1, ++ppt;
	ppt->x = (int)x2, ppt->y = (int)y2, ++ppt;
	ppt->x = (int)x3, ppt->y = (int)y3;
    }
    xvdev->points.count += 3;
    return 0;
}

private int
pclxl_closepath(gx_device_vector * vdev, floatp x, floatp y,
		floatp x_start, floatp y_start)
{
    stream *s = gdev_vector_stream(vdev);
    int code = pclxl_flush_points(xvdev);

    if (code < 0)
	return code;
    spputc(s, pxtCloseSubPath);
    xvdev->points.current.x = (int)x_start;
    xvdev->points.current.y = (int)y_start;
    return 0;
}

private int
pclxl_endpath(gx_device_vector * vdev, gx_path_type_t type)
{
    stream *s = gdev_vector_stream(vdev);
    int code = pclxl_flush_points(xvdev);
    gx_path_type_t rule = type & gx_path_type_rule;

    if (code < 0)
	return code;
    if (type & (gx_path_type_fill | gx_path_type_stroke)) {
	pclxl_set_paints(xvdev, type);
	spputc(s, pxtPaintPath);
    }
    if (type & gx_path_type_clip) {
	static const byte scr_[] =
	{
	    dub(eInterior), da(pxaClipRegion), pxtSetClipReplace
	};

	if (rule != xvdev->clip_rule) {
	    put_ub(s, (rule == gx_path_type_even_odd ? eEvenOdd :
		       eNonZeroWinding));
	    put_ac(s, pxaClipMode, pxtSetClipMode);
	    xvdev->clip_rule = rule;
	}
	put_lit(s, scr_);
    }
    return 0;
}

/* Vector implementation procedures */

private const gx_device_vector_procs pclxl_vector_procs =
{
	/* Page management */
    pclxl_beginpage,
	/* Imager state */
    pclxl_setlinewidth,
    pclxl_setlinecap,
    pclxl_setlinejoin,
    pclxl_setmiterlimit,
    pclxl_setdash,
    gdev_vector_setflat,
    pclxl_setlogop,
	/* Other state */
    pclxl_setfillcolor,
    pclxl_setstrokecolor,
	/* Paths */
    gdev_vector_dopath,
    pclxl_dorect,
    pclxl_beginpath,
    pclxl_moveto,
    pclxl_lineto,
    pclxl_curveto,
    pclxl_closepath,
    pclxl_endpath
};

/* ---------------- Driver procedures ---------------- */

#define vdev ((gx_device_vector *)dev)
#define xdev ((gx_device_pclxl *)dev)

/* ------ Open/close/page ------ */

/* Open the device. */
private int
pclxl_open_device(gx_device * dev)
{
    int code;
    static const char *file_header =
    "\033%-12345X@PJL ENTER LANGUAGE = PCLXL\n\
) HP-PCL XL;1;1;Comment Copyright Aladdin Enterprises 1996\000\n";
    static const byte stream_header[] =
    {
	da(pxaUnitsPerMeasure),
	dub(0), da(pxaMeasure),
	dub(eBackChAndErrPage), da(pxaErrorReport),
	pxtBeginSession,
	dub(0), da(pxaSourceType),
	dub(eBinaryLowByteFirst), da(pxaDataOrg),
	pxtOpenDataSource
    };

    vdev->v_memory = dev->memory;
/****** WRONG ******/
    vdev->vec_procs = &pclxl_vector_procs;
    code = gdev_vector_open_file(vdev, 512);
    if (code < 0)
	return code;
    pclxl_page_init(xdev);
    {
	stream *s = vdev->strm;

	/* We have to add 2 to the strlen because the next-to-last */
	/* character is a null. */
	put_bytes(s, (const byte *)file_header,
		  strlen(file_header) + 2);
	put_usp(s, (uint) (dev->HWResolution[0] + 0.5),
		(uint) (dev->HWResolution[1] + 0.5));
	put_lit(s, stream_header);
    }
    xdev->media_size = pxeMediaSize_next;	/* no size selected */
    memset(&xdev->chars, 0, sizeof(xdev->chars));
    xdev->chars.next_in = xdev->chars.next_out = 2;
    return 0;
}

/* Wrap up ("output") a page. */
/* We only support flush = true, and we don't support num_copies != 1. */
private int
pclxl_output_page(gx_device * dev, int num_copies, int flush)
{
    if (xdev->in_page) {
	stream *s = vdev->strm;

	spputc(s, pxtEndPage);
	sflush(s);
	pclxl_page_init(xdev);
    }
    return 0;
}

/* Close the device. */
/* Note that if this is being called as a result of finalization, */
/* the stream may no longer exist. */
private int
pclxl_close_device(gx_device * dev)
{
    FILE *file = vdev->file;

    if (xdev->in_page)
	fputc(pxtEndPage, file);
    {
	static const byte file_trailer[] =
	{
	    pxtCloseDataSource,
	    pxtEndSession,
	    033, '%', '-', '1', '2', '3', '4', '5', 'X'
	};

	/* The stream may no longer exist: see above. */
	fwrite(file_trailer, 1, sizeof(file_trailer), file);
    }
    gdev_vector_close_file(vdev);
    return 0;
}

/* ------ One-for-one images ------ */

private const byte eBit_values[] =
{
    0, e1Bit, 0, 0, e4Bit, 0, 0, 0, e8Bit
};

/* Copy a monochrome bitmap. */
private int
pclxl_copy_mono(gx_device * dev, const byte * data,
	int data_x, int raster, gx_bitmap_id id, int x, int y, int w, int h,
		gx_color_index zero, gx_color_index one)
{
    int code;
    stream *s;
    gx_color_index color0 = zero, color1 = one;
    gs_logical_operation_t lop;
    byte palette[2 * 3];
    int palette_size;
    pxeColorSpace_t color_space;

    fit_copy(dev, data, data_x, raster, id, x, y, w, h);
    code = gdev_vector_update_clip_path(vdev, NULL);
    if (code < 0)
	return code;
    pclxl_set_cursor(xdev, x, y);
    if (id != gs_no_id && zero == gx_no_color_index &&
	one != gx_no_color_index && data_x == 0
	) {
	gx_drawing_color dcolor;

	color_set_pure(&dcolor, one);
	pclxl_setfillcolor(vxdev, &dcolor);
	if (pclxl_copy_text_char(xdev, data, raster, id, w, h) >= 0)
	    return 0;
    }
    /*
     * The following doesn't work if we're writing white with a mask.
     * We'll fix it eventually.
     */
    if (zero == gx_no_color_index) {
	if (one == gx_no_color_index)
	    return 0;
	lop = rop3_S | lop_S_transparent;
	color0 = (1 << dev->color_info.depth) - 1;
    } else if (one == gx_no_color_index) {
	lop = rop3_S | lop_S_transparent;
	color1 = (1 << dev->color_info.depth) - 1;
    } else {
	lop = rop3_S;
    }
    if (dev->color_info.num_components == 1 ||
	(rgb_is_gray(color0) && rgb_is_gray(color1))
	) {
	palette[0] = (byte) color0;
	palette[1] = (byte) color1;
	palette_size = 2;
	color_space = eGray;
    } else {
	palette[0] = (byte) (color0 >> 16);
	palette[1] = (byte) (color0 >> 8);
	palette[2] = (byte) color0;
	palette[3] = (byte) (color1 >> 16);
	palette[4] = (byte) (color1 >> 8);
	palette[5] = (byte) color1;
	palette_size = 6;
	color_space = eRGB;
    }
    code = gdev_vector_update_log_op(vdev, lop);
    if (code < 0)
	return 0;
    pclxl_set_color_palette(xdev, color_space, palette, palette_size);
    s = gdev_vector_stream(vxdev);
    {
	static const byte mi_[] =
	{
	    dub(e1Bit), da(pxaColorDepth),
	    dub(eIndexedPixel), da(pxaColorMapping)
	};

	put_lit(s, mi_);
    }
    pclxl_write_begin_image(xdev, w, h, w, h);
    pclxl_write_image_data(xdev, data, data_x, raster, w, 0, h);
    pclxl_write_end_image(xdev);
    return 0;
}

/* Copy a color bitmap. */
private int
pclxl_copy_color(gx_device * dev,
		 const byte * base, int sourcex, int raster, gx_bitmap_id id,
		 int x, int y, int w, int h)
{
    stream *s;
    uint source_bit;
    int code;

    fit_copy(dev, base, sourcex, raster, id, x, y, w, h);
    code = gdev_vector_update_clip_path(vdev, NULL);
    if (code < 0)
	return code;
    source_bit = sourcex * dev->color_info.depth;
    if ((source_bit & 7) != 0)
	return gx_default_copy_color(dev, base, sourcex, raster, id,
				     x, y, w, h);
    gdev_vector_update_log_op(vdev, rop3_S);
    pclxl_set_cursor(xdev, x, y);
    s = gdev_vector_stream(vxdev);
    {
	static const byte ci_[] =
	{
	    da(pxaColorDepth),
	    dub(eDirectPixel), da(pxaColorMapping)
	};

	put_ub(s, eBit_values[dev->color_info.depth / dev->color_info.num_components]);
	put_lit(s, ci_);
    }
    pclxl_write_begin_image(xdev, w, h, w, h);
    pclxl_write_image_data(xdev, base, source_bit, raster,
			   w * dev->color_info.depth, 0, h);
    pclxl_write_end_image(xdev);
    return 0;
}

/* Fill a mask. */
private int
pclxl_fill_mask(gx_device * dev,
		const byte * data, int data_x, int raster, gx_bitmap_id id,
		int x, int y, int w, int h,
		const gx_drawing_color * pdcolor, int depth,
		gs_logical_operation_t lop, const gx_clip_path * pcpath)
{
    int code;
    stream *s;

    fit_copy(dev, data, data_x, raster, id, x, y, w, h);
    if ((data_x & 7) != 0 || !gx_dc_is_pure(pdcolor) || depth > 1)
	return gx_default_fill_mask(dev, data, data_x, raster, id,
				    x, y, w, h, pdcolor, depth,
				    lop, pcpath);
    code = gdev_vector_update_clip_path(vdev, pcpath);
    if (code < 0)
	return code;
    code = gdev_vector_update_fill_color(vdev, pdcolor);
    if (code < 0)
	return 0;
    pclxl_set_cursor(xdev, x, y);
    if (id != gs_no_id && data_x == 0) {
	code = gdev_vector_update_log_op(vdev, lop);
	if (code < 0)
	    return 0;
	if (pclxl_copy_text_char(xdev, data, raster, id, w, h) >= 0)
	    return 0;
    }
    code = gdev_vector_update_log_op(vdev,
				     lop | rop3_S | lop_S_transparent);
    if (code < 0)
	return 0;
    pclxl_set_color_palette(xdev, eGray, (const byte *)"\377\000", 2);
    s = gdev_vector_stream(vxdev);
    {
	static const byte mi_[] =
	{
	    dub(e1Bit), da(pxaColorDepth),
	    dub(eIndexedPixel), da(pxaColorMapping)
	};

	put_lit(s, mi_);
    }
    pclxl_write_begin_image(xdev, w, h, w, h);
    pclxl_write_image_data(xdev, data, data_x, raster, w, 0, h);
    pclxl_write_end_image(xdev);
    return 0;
}

/* Do a RasterOp. */
private int
pclxl_strip_copy_rop(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
		     const gx_color_index * scolors,
	   const gx_strip_bitmap * textures, const gx_color_index * tcolors,
		     int x, int y, int width, int height,
		     int phase_x, int phase_y, gs_logical_operation_t lop)
{				/* We can't do general RasterOps yet. */
/****** WORK IN PROGRESS ******/
    return 0;
}

/* ------ High-level images ------ */

typedef struct pclxl_image_enum_s {
    gs_memory_t *memory;
    void *default_info;
    uint bits_per_row;
    int bits_per_pixel;
    int y, h;
} pclxl_image_enum_t;

gs_private_st_ptrs1(st_pclxl_image_enum, pclxl_image_enum_t,
		    "pclxl_image_enum_t", pclxl_image_enum_enum_ptrs,
		    pclxl_image_enum_reloc_ptrs, default_info);

/* Start processing an image. */
private int
pclxl_begin_image(gx_device * dev,
		  const gs_imager_state * pis, const gs_image_t * pim,
		  gs_image_format_t format, const gs_int_rect * prect,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
		  gs_memory_t * mem, void **pinfo)
{
    const gs_color_space *pcs = pim->ColorSpace;
    pclxl_image_enum_t *pie;
    gs_matrix mat;
    int num_components;
    int bits_per_pixel;

    pie = gs_alloc_struct(mem, pclxl_image_enum_t, &st_pclxl_image_enum,
			  "pclxl_begin_image");
    if (pie == 0)
	return_error(gs_error_VMerror);
    pie->memory = mem;
    *pinfo = pie;
    if (pim->ImageMask)
	bits_per_pixel = num_components = 1;
    else
	num_components = gs_color_space_num_components(pcs),
	    bits_per_pixel = pim->BitsPerComponent * num_components;
    gs_matrix_invert(&pim->ImageMatrix, &mat);
    gs_matrix_multiply(&mat, &ctm_only(pis), &mat);
    /* Currently we only handle portrait transformations. */
    if (mat.xx <= 0 || mat.xy != 0 || mat.yx != 0 || mat.yy <= 0 ||
	(pim->ImageMask ?
	 (!gx_dc_is_pure(pdcolor) || pim->CombineWithColor) :
	 (!pclxl_can_handle_color_space(pim->ColorSpace) ||
	  (bits_per_pixel != 1 && bits_per_pixel != 4 &&
	   bits_per_pixel != 8))) ||
	format != gs_image_format_chunky ||
	prect
	) {
	int code = gx_default_begin_image(dev, pis, pim, format, prect,
					  pdcolor, pcpath, mem,
					  &pie->default_info);

	if (code < 0)
	    gs_free_object(mem, pie, "pclxl_begin_image");
	return code;
    }
    pie->default_info = 0;
    {
	stream *s = gdev_vector_stream(vxdev);
	gs_logical_operation_t lop = pis->log_op;
	int code = gdev_vector_update_log_op
	(vdev, (pim->ImageMask || pim->CombineWithColor ? lop :
		rop3_know_T_0(lop)));
	int bpc = pim->BitsPerComponent;
	int sample_max = (1 << bpc) - 1;
	byte palette[256 * 3];
	int i;

	if (code < 0)
	    return code;
	pie->bits_per_pixel = bits_per_pixel;
	pie->bits_per_row = bits_per_pixel * pim->Width;
	pie->y = 0;
	pie->h = pim->Height;
	pclxl_set_cursor(xdev, (int)((mat.tx + 0.5) / xdev->scale.x),
			 (int)((mat.ty + 0.5) / xdev->scale.y));
	for (i = 0; i < 1 << bits_per_pixel; ++i) {
	    gs_client_color cc;
	    gx_device_color devc;
	    int cv = i, j;

	    for (j = num_components - 1; j >= 0; cv >>= bpc, --j)
		cc.paint.values[j] = pim->Decode[j * 2] +
		    (cv & sample_max) *
		    (pim->Decode[j * 2 + 1] - pim->Decode[j * 2]) /
		    sample_max;
	    (*pcs->type->remap_color)
		(&cc, pcs, &devc, pis, dev, gs_color_select_source);
	    if (!gx_dc_is_pure(&devc))
		return_error(gs_error_Fatal);
	    if (dev->color_info.num_components == 1)
		palette[i] = (byte) gx_dc_pure_color(&devc);
	    else {
		gx_color_index ci = gx_dc_pure_color(&devc);
		byte *ppal = &palette[i * 3];

		ppal[0] = (byte) (ci >> 16);
		ppal[1] = (byte) (ci >> 8);
		ppal[2] = (byte) ci;
	    }
	}
	if (dev->color_info.num_components == 1)
	    pclxl_set_color_palette(xdev, eGray, palette,
				    1 << bits_per_pixel);
	else
	    pclxl_set_color_palette(xdev, eRGB, palette,
				    3 << bits_per_pixel);
	{
	    static const byte ii_[] =
	    {
		da(pxaColorDepth),
		dub(eIndexedPixel), da(pxaColorMapping)
	    };

	    put_ub(s, eBit_values[bits_per_pixel]);
	    put_lit(s, ii_);
	}
	pclxl_write_begin_image(xdev, pim->Width, pim->Height,
				(uint) (pim->Width * mat.xx),
				(uint) (pim->Height * mat.yy));
    }
    return 0;
}

/* Process the next piece of an image. */
private int
pclxl_image_data(gx_device * dev,
      void *info, const byte ** planes, int data_x, uint raster, int height)
{
    pclxl_image_enum_t *pie = info;

    if (pie->default_info)
	return gx_default_image_data(dev, pie->default_info, planes, data_x,
				     raster, height);
    if (height > pie->h - pie->y)
	height = pie->h - pie->y;
    pclxl_write_image_data(xdev, planes[0], data_x * pie->bits_per_pixel,
			   raster, pie->bits_per_row, pie->y, height);
    return (pie->y += height) >= pie->h;
}

/* Clean up by releasing the buffers. */
private int
pclxl_end_image(gx_device * dev, void *info, bool draw_last)
{
    pclxl_image_enum_t *pie = info;
    int code = 0;

    if (pie->default_info)
	code = gx_default_end_image(dev, pie->default_info, draw_last);
    else {			/* Fill out to the full image height. */
/****** WRONG -- REST OF IMAGE SHOULD BE TRANSPARENT ******/
	if (pie->h > pie->y) {
	    uint bytes_per_row = (pie->bits_per_row + 7) >> 3;
	    byte *row = gs_alloc_bytes(pie->memory, bytes_per_row,
				       "pclxl_end_image(fill)");

	    if (row == 0)
		return_error(gs_error_VMerror);
	    memset(row, 0, bytes_per_row);
	    for (; pie->y < pie->h; pie->y++)
		pclxl_write_image_data(xdev, row, 0, bytes_per_row,
				       pie->bits_per_row, pie->y, 1);
	    gs_free_object(pie->memory, row, "pclxl_end_image(fill)");
	}
	pclxl_write_end_image(xdev);
    }
    gs_free_object(pie->memory, pie, "pclxl_end_image");
    return code;
}

#undef vdev
