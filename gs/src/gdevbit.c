/* Copyright (C) 1991, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

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


/* "Plain bits" devices to measure rendering time. */
#include "gdevprn.h"
#include "gsparam.h"
#include "gscrd.h"
#include "gscrdp.h"
#include "gxlum.h"

/*
 * When debugging problems on large CMYK devices, we have to be able to
 * modify the output at this stage.  These parameters are not set in any
 * normal configuration.
 */
/* Define whether to trim off top and bottom white space. */
/*#define TRIM_TOP_BOTTOM */
/* Define left and right trimming margins. */
/* Note that this is only approximate: we trim to byte boundaries. */
/*#define TRIM_LEFT 400 */
/*#define TRIM_RIGHT 400 */
/* Define whether to expand each bit to a byte. */
/* Also convert black-and-white to proper gray (inverting if monobit). */
/*#define EXPAND_BITS_TO_BYTES */

/* Define the device parameters. */
#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* The device descriptor */
private dev_proc_map_rgb_color(bit_mono_map_rgb_color);
private dev_proc_map_rgb_color(bit_map_rgb_color);
private dev_proc_map_rgb_color(bit_forcemono_map_rgb_color);
private dev_proc_map_color_rgb(bit_map_color_rgb);
private dev_proc_map_cmyk_color(bit_map_cmyk_color);
private dev_proc_get_params(bit_get_params);
private dev_proc_put_params(bit_put_params);
private dev_proc_print_page(bit_print_page);

#define bit_procs(map_rgb_color, map_cmyk_color)\
{	gdev_prn_open,\
	gx_default_get_initial_matrix,\
	NULL,	/* sync_output */\
	gdev_prn_output_page,\
	gdev_prn_close,\
	map_rgb_color,\
	bit_map_color_rgb,\
	NULL,	/* fill_rectangle */\
	NULL,	/* tile_rectangle */\
	NULL,	/* copy_mono */\
	NULL,	/* copy_color */\
	NULL,	/* draw_line */\
	NULL,	/* get_bits */\
	bit_get_params,\
	bit_put_params,\
	map_cmyk_color,\
	NULL,	/* get_xfont_procs */\
	NULL,	/* get_xfont_device */\
	NULL,	/* map_rgb_alpha_color */\
	gx_page_device_get_page_device	/* get_page_device */\
}

/* The following macro is used in get_params and put_params to determine  */
/* the num_components for the current device. It works using the device   */
/* name character after "bit" which is either '\0', 'r', or 'c'. Any new  */
/* devices that are added to this module must modify this macro to return */
/* the correct num_components. This is needed to support the ForceMono    */
/* parameter which alters dev->num_components.				  */
#define REAL_NUM_COMPONENTS(dev) ((dev->dname[3]== 'c') ? 4 : \
				  (dev->dname[3]=='r') ? 3 : 1)

private const gx_device_procs bitmono_procs =
bit_procs(bit_mono_map_rgb_color, NULL);
const gx_device_printer gs_bit_device =
{prn_device_body(gx_device_printer, bitmono_procs, "bit",
		 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
		 X_DPI, Y_DPI,
		 0, 0, 0, 0,    /* margins */
		 1, 1, 1, 0, 2, 1, bit_print_page)
};

private const gx_device_procs bitrgb_procs =
bit_procs(bit_map_rgb_color, NULL);
const gx_device_printer gs_bitrgb_device =
{prn_device_body(gx_device_printer, bitrgb_procs, "bitrgb",
		 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
		 X_DPI, Y_DPI,
		 0, 0, 0, 0,	/* margins */
		 3, 4, 1, 1, 2, 2, bit_print_page)
};

private const gx_device_procs bitcmyk_procs =
bit_procs(bit_forcemono_map_rgb_color, bit_map_cmyk_color);
const gx_device_printer gs_bitcmyk_device =
{prn_device_body(gx_device_printer, bitcmyk_procs, "bitcmyk",
		 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
		 X_DPI, Y_DPI,
		 0, 0, 0, 0,	/* margins */
		 4, 4, 1, 1, 2, 2, bit_print_page)
};

/* Map gray to color. */
/* Note that 1-bit monochrome is a special case. */
private gx_color_index
bit_mono_map_rgb_color(gx_device * dev, gx_color_value red,
		       gx_color_value green, gx_color_value blue)
{
    int bpc = dev->color_info.depth;
    int drop = sizeof(gx_color_value) * 8 - bpc;
    gx_color_value gray =
    (red * (unsigned long)lum_red_weight +
     green * (unsigned long)lum_green_weight +
     blue * (unsigned long)lum_blue_weight +
     (lum_all_weights / 2))
    / lum_all_weights;

    return (bpc == 1 ? gx_max_color_value - gray : gray) >> drop;
}

/* Map RGB to color. */
private gx_color_index
bit_map_rgb_color(gx_device * dev, gx_color_value red,
		  gx_color_value green, gx_color_value blue)
{
    int bpc = dev->color_info.depth / 3;
    int drop = sizeof(gx_color_value) * 8 - bpc;

    return ((((red >> drop) << bpc) + (green >> drop)) << bpc) +
	(blue >> drop);
}

/* Map RGB to gray shade. */
/* Only used in CMYK mode when put_params has set ForceMono=1 */
private gx_color_index
bit_forcemono_map_rgb_color(gx_device * dev, gx_color_value red,
		  gx_color_value green, gx_color_value blue)
{
    gx_color_value color;
    int bpc = dev->color_info.depth / 4;	/* This function is used in CMYK mode */
    int drop = sizeof(gx_color_value) * 8 - bpc;
    gx_color_value gray = red;

    if ((red != green) || (green != blue))
	gray = (red * (unsigned long)lum_red_weight +
	     green * (unsigned long)lum_green_weight +
	     blue * (unsigned long)lum_blue_weight +
	     (lum_all_weights / 2))
		/ lum_all_weights;

    color = (gx_max_color_value - gray) >> drop;	/* color is in K channel */
    return color;
}

/* Map color to RGB.  This has 3 separate cases, but since it is rarely */
/* used, we do a case test rather than providing 3 separate routines. */
private int
bit_map_color_rgb(gx_device * dev, gx_color_index color, gx_color_value rgb[3])
{
    int depth = dev->color_info.depth;
    int ncomp = dev->color_info.num_components;
    int bpc = depth / ncomp;
    uint mask = (1 << bpc) - 1;

#define cvalue(c) ((gx_color_value)((ulong)(c) * gx_max_color_value / mask))

    switch (ncomp) {
	case 1:		/* gray */
	    rgb[0] = rgb[1] = rgb[2] =
		(depth == 1 ? (color ? 0 : gx_max_color_value) :
		 cvalue(color));
	    break;
	case 3:		/* RGB */
	    {
		gx_color_index cshift = color;

		rgb[2] = cvalue(cshift & mask);
		cshift >>= bpc;
		rgb[1] = cvalue(cshift & mask);
		rgb[0] = cvalue(cshift >> bpc);
	    }
	    break;
	case 4:		/* CMYK */
	    /* Map CMYK back to RGB. */
	    {
		gx_color_index cshift = color;
		uint c, m, y, k;

		k = cshift & mask;
		cshift >>= bpc;
		y = cshift & mask;
		cshift >>= bpc;
		m = cshift & mask;
		c = cshift >> bpc;
		/* We use our improved conversion rule.... */
		rgb[0] = cvalue((mask - c) * (mask - k) / mask);
		rgb[1] = cvalue((mask - m) * (mask - k) / mask);
		rgb[2] = cvalue((mask - y) * (mask - k) / mask);
	    }
	    break;
    }
    return 0;
#undef cvalue
}

/* Map CMYK to color. */
private gx_color_index
bit_map_cmyk_color(gx_device * dev, gx_color_value cyan,
	gx_color_value magenta, gx_color_value yellow, gx_color_value black)
{
    int bpc = dev->color_info.depth / 4;
    int drop = sizeof(gx_color_value) * 8 - bpc;
    gx_color_index color =
    ((((((cyan >> drop) << bpc) +
	(magenta >> drop)) << bpc) +
      (yellow >> drop)) << bpc) +
    (black >> drop);

    return (color == gx_no_color_index ? color ^ 1 : color);
}

/* Get parameters.  We provide a default CRD. */
private int
my_tpqr(int index, floatp in, const gs_cie_wbsd * pwbsd,
	gs_cie_render * pcrd, float *out)
{
    *out = (in < 0.5 ? in / 2 : in * 3 / 2 - 0.5);
    return 0;
}

private int
bit_get_params(gx_device * pdev, gs_param_list * plist)
{
    int ecode = gdev_prn_get_params(pdev, plist);
    int code;
    /* The following is a hack to get the original num_components. See comment above */
    int ncomps = REAL_NUM_COMPONENTS(pdev);
    int forcemono = (ncomps == pdev->color_info.num_components) ? 0 : 1;

    if (param_requested(plist, "CRDDefault") > 0) {
	gs_cie_render *pcrd;

	code = gs_cie_render1_build(&pcrd, pdev->memory, "bit_get_params");
	if (code >= 0) {
	    static const gs_vector3 my_white_point = {1, 1, 1};
	    static const gs_cie_transform_proc3 my_TransformPQR = {
		0, "bitTPQRDefault", {0, 0}, 0
	    };
	    gs_cie_transform_proc3 tpqr;

	    tpqr = my_TransformPQR;
	    tpqr.driver_name = pdev->dname;
	    code = gs_cie_render1_initialize(pcrd, NULL,
					     &my_white_point, NULL,
					     NULL, NULL, &tpqr,
					     NULL, NULL, NULL,
					     NULL, NULL, NULL,
					     NULL);
	    if (code >= 0) {
		code = param_write_cie_render1(plist, "CRDDefault", pcrd,
					       pdev->memory);
	    }
	    rc_decrement(pcrd, "bit_get_params"); /* release */
	}
	if (code < 0)
	    ecode = code;
    }

    if (param_requested(plist, "bitTPQRDefault") > 0) {
	gs_cie_transform_proc my_proc = my_tpqr;
	static byte my_addr[sizeof(gs_cie_transform_proc)];
	gs_param_string as;

	memcpy(my_addr, &my_proc, sizeof(gs_cie_transform_proc));
	as.data = my_addr;
	as.size = sizeof(gs_cie_transform_proc);
	as.persistent = true;
	code = param_write_string(plist, "bitTPQRDefault", &as);
	if (code < 0)
	    ecode = code;
    }

    if ((code = param_write_int(plist, "ForceMono", &forcemono)) < 0) {
	ecode = code;
    }

    return ecode;
}

/* Set parameters.  We allow setting the number of bits per component. */
/* Also, ForceMono=1 forces monochrome output from RGB/CMYK devices. */
private int
bit_put_params(gx_device * pdev, gs_param_list * plist)
{
    gx_device_color_info save_info;
    int ncomps = REAL_NUM_COMPONENTS(pdev);
    int new_ncomps = ncomps;
    int bpc = pdev->color_info.depth / ncomps;
    int v;
    int ecode = 0;
    int code, ccode;
    static const byte depths[4][8] = {
	{1, 2, 0, 4, 8, 0, 0, 8},
	{0},
	{4, 8, 0, 16, 16, 0, 0, 24},
	{4, 8, 0, 16, 32, 0, 0, 32}
    };
    const char *vname;

    if ((code = param_read_int(plist, (vname = "GrayValues"), &v)) != 1 ||
	(code = param_read_int(plist, (vname = "RedValues"), &v)) != 1 ||
	(code = param_read_int(plist, (vname = "GreenValues"), &v)) != 1 ||
	(code = param_read_int(plist, (vname = "BlueValues"), &v)) != 1
	) {
	if (code < 0)
	    ecode = code;
	else
	    switch (v) {
		case 2:
		    bpc = 1;
		    break;
		case 4:
		    bpc = 2;
		    break;
		case 16:
		    bpc = 4;
		    break;
		case 32:
		    bpc = 5;
		    break;
		case 256:
		    bpc = 8;
		    break;
		default:
		    param_signal_error(plist, vname,
				       ecode = gs_error_rangecheck);
	    }
    }

    switch (ccode = param_read_int(plist, (vname = "ForceMono"), &v)) {
    case 0:
	if (v == 1) {
	    new_ncomps = 1;
	    break;
	}
	else if (v == 0)
	    break;
	code = gs_error_rangecheck;
    default:
	ecode = code;
	param_signal_error(plist, vname, ecode);
    case 1:
	break;
    }
    if (ecode < 0)
	return ecode;

    /* Tuck away the color_info in case gdev_prn_put_params fails */
    save_info = pdev->color_info;
    if (code != 1 || ccode != 1) {
	pdev->color_info.depth = depths[ncomps - 1][bpc - 1];
	pdev->color_info.max_gray = pdev->color_info.max_color =
	    (pdev->color_info.dither_grays =
	     pdev->color_info.dither_colors =
	     (1 << bpc)) - 1;
    }
    ecode = gdev_prn_put_params(pdev, plist);
    if (ecode < 0) {
	pdev->color_info = save_info;
	return ecode;
    }
    /* Now change num_components -- couldn't above because	*/
    /*  '1' is special in gx_default_put_params			*/
    pdev->color_info.num_components = new_ncomps;
    if (code != 1) {
	if (pdev->is_open)
	    gs_closedevice(pdev);
    }
    /* Reset the map_cmyk_color procedure if appropriate. */
    if (dev_proc(pdev, map_cmyk_color) == cmyk_1bit_map_cmyk_color ||
	dev_proc(pdev, map_cmyk_color) == cmyk_8bit_map_cmyk_color ||
	dev_proc(pdev, map_cmyk_color) == bit_map_cmyk_color) {
	set_dev_proc(pdev, map_cmyk_color,
		     pdev->color_info.depth == 4 ? cmyk_1bit_map_cmyk_color :
		     pdev->color_info.depth == 32 ? cmyk_8bit_map_cmyk_color :
		     bit_map_cmyk_color);
    }
    return 0;
}

/* Send the page to the printer. */
private int
bit_print_page(gx_device_printer * pdev, FILE * prn_stream)
{				/* Just dump the bits on the file. */
    /* If the file is 'nul', don't even do the writes. */
    int line_size = gdev_mem_bytes_per_scan_line((gx_device *) pdev);
    byte *in = (byte *) gs_malloc(line_size, 1, "bit_print_page(in)");
    byte *data;
    int nul = !strcmp(pdev->fname, "nul");
    int lnum = 0, bottom = pdev->height;

    if (in == 0)
	return_error(gs_error_VMerror);
#ifdef TRIM_TOP_BOTTOM
    {
	gx_color_index white =
	(pdev->color_info.num_components == 4 ? 0 :
	 (*dev_proc(pdev, map_rgb_color))
	 ((gx_device *) pdev, gx_max_color_value, gx_max_color_value,
	  gx_max_color_value));

#define color_index_bits (sizeof(gx_color_index) * 8)
	const gx_color_index *p;
	const gx_color_index *end;
	int depth = pdev->color_info.depth;
	int end_bits = ((long)line_size * depth) % color_index_bits;
	gx_color_index end_mask =
	(end_bits == 0 ? 0 : -1 << (color_index_bits - end_bits));
	int i;

	for (i = depth; i < color_index_bits; i += depth)
	    white |= white << depth;
	/* Remove bottom white space. */
	for (; bottom - lnum > 1; --bottom) {
	    gdev_prn_get_bits(pdev, bottom - 1, in, &data);
	    p = (const gx_color_index *)data;
	    end = p + line_size / sizeof(gx_color_index);
	    for (; p < end; ++p)
		if (*p != white)
		    goto bx;
	    if (end_mask != 0 && ((*end ^ white) & end_mask) != 0)
		goto bx;
	}
	/* Remove top white space. */
      bx:for (; lnum < bottom; ++lnum) {
	    gdev_prn_get_bits(pdev, lnum, in, &data);
	    p = (const gx_color_index *)data;
	    end = p + line_size / sizeof(gx_color_index);
	    for (; p < end; ++p)
		if (*p != white)
		    goto tx;
	    if (end_mask != 0 && ((*end ^ white) & end_mask) != 0)
		goto tx;
	}
      tx:;
    }
#endif
    for (; lnum < bottom; ++lnum) {
	gdev_prn_get_bits(pdev, lnum, in, &data);
	if (!nul) {
	    const byte *row = data;
	    uint len = line_size;

#ifdef TRIM_LEFT
	    row += (TRIM_LEFT * pdev->color_info.depth) >> 3;
#endif
#ifdef TRIM_RIGHT
	    len = data + ((TRIM_RIGHT * pdev->color_info.depth + 7) >> 3) - row;
#endif
#ifdef EXPAND_BITS_TO_BYTES
	    {
		uint i;
		byte invert = (pdev->color_info.depth == 1 ? 0xff : 0);

		for (i = 0; i < len; ++i) {
		    byte b = row[i] ^ invert;

		    putc(-(b >> 7) & 0xff, prn_stream);
		    putc(-((b >> 6) & 1) & 0xff, prn_stream);
		    putc(-((b >> 5) & 1) & 0xff, prn_stream);
		    putc(-((b >> 4) & 1) & 0xff, prn_stream);
		    putc(-((b >> 3) & 1) & 0xff, prn_stream);
		    putc(-((b >> 2) & 1) & 0xff, prn_stream);
		    putc(-((b >> 1) & 1) & 0xff, prn_stream);
		    putc(-(b & 1) & 0xff, prn_stream);
		}
	    }
#else
	    fwrite(row, 1, len, prn_stream);
#endif
	}
    }
    gs_free((char *)in, line_size, 1, "bit_print_page(in)");
    return 0;
}
