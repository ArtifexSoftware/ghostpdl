/* Copyright (C) 1989, 1995, 1996, 1997, 1998, 1999 Aladdin Enterprises.  All rights reserved.

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


/* X Windows driver for Ghostscript library */
/* The X include files include <sys/types.h>, which, on some machines */
/* at least, define uint, ushort, and ulong, which std.h also defines. */
/* std.h has taken care of this. */
#include "gx.h"			/* for gx_bitmap; includes std.h */
#include "math_.h"
#include "memory_.h"
#include "x_.h"
#include "gserrors.h"
#include "gsmatrix.h"		/* for gscoord.h */
#include "gscoord.h"		/* for gs_currentmatrix */
#include "gsdevice.h"		/* for gs_currentdevice */
#include "gsparam.h"
#include "gxdevice.h"
#include "gxpath.h"
#include "gxgetbit.h"
#include "gxiparam.h"
#include "gsiparm2.h"
#include "gdevx.h"

/* Define whether to update after every write, for debugging. */
bool X_ALWAYS_UPDATE = false;

/* Define the maximum size of the temporary pixmap for copy_mono */
/* that we are willing to leave lying around in the server */
/* between uses.  (Assume 32-bit ints here!) */
private int X_MAX_TEMP_PIXMAP = 20000;

/* Define the maximum size of the temporary image created in memory */
/* for get_bits_rectangle. */
private int X_MAX_TEMP_IMAGE = 5000;

/* Define whether to try to read back exposure events after XGetImage. */
/****** THIS IS USELESS.  XGetImage DOES NOT GENERATE EXPOSURE EVENTS. ******/
#define GET_IMAGE_EXPOSURES 0

/* Forward references */
private int set_tile(P2(gx_device *, const gx_strip_bitmap *));
private void free_cp(P1(gx_device *));
private void x_send_event(P2(gx_device *, Atom));

/* Screen updating machinery */
#define update_init(dev)\
  ((gx_device_X *)(dev))->up_area = 0,\
  ((gx_device_X *)(dev))->up_count = 0
#define update_flush(dev)\
  if ( ((gx_device_X *)(dev))->up_area != 0 ) update_do_flush(dev)
private void update_do_flush(P1(gx_device *));

#define flush_text(dev)\
  if ( IN_TEXT((gx_device_X *)(dev)) ) do_flush_text(dev)
private void do_flush_text(P1(gx_device *));

/* Procedures */

extern int gdev_x_open(P1(gx_device_X *));
private dev_proc_open_device(x_open);
private dev_proc_get_initial_matrix(x_get_initial_matrix);
private dev_proc_sync_output(x_sync);
private dev_proc_output_page(x_output_page);
extern void gdev_x_free_dynamic_colors(P1(gx_device_X *));
extern void gdev_x_free_colors(P1(gx_device_X *));
private dev_proc_close_device(x_close);
extern dev_proc_map_rgb_color(gdev_x_map_rgb_color);
extern dev_proc_map_color_rgb(gdev_x_map_color_rgb);
private dev_proc_fill_rectangle(x_fill_rectangle);
private dev_proc_copy_mono(x_copy_mono);
private dev_proc_copy_color(x_copy_color);
private dev_proc_get_params(x_get_params);
private dev_proc_put_params(x_put_params);
dev_proc_get_xfont_procs(x_get_xfont_procs);
private dev_proc_get_page_device(x_get_page_device);
private dev_proc_strip_tile_rectangle(x_strip_tile_rectangle);
private dev_proc_begin_typed_image(x_begin_typed_image);
private dev_proc_get_bits_rectangle(x_get_bits_rectangle);

/* The device descriptor */
private const gx_device_procs x_procs =
{
    x_open,
    x_get_initial_matrix,
    x_sync,
    x_output_page,
    x_close,
    gdev_x_map_rgb_color,
    gdev_x_map_color_rgb,
    x_fill_rectangle,
    NULL,			/* tile_rectangle */
    x_copy_mono,
    x_copy_color,
    NULL,			/* draw_line */
    NULL,			/* get_bits */
    x_get_params,
    x_put_params,
    NULL,			/* map_cmyk_color */
    x_get_xfont_procs,
    NULL,			/* get_xfont_device */
    NULL,			/* map_rgb_alpha_color */
    x_get_page_device,
    NULL,			/* get_alpha_bits */
    NULL,			/* copy_alpha */
    NULL,			/* get_band */
    NULL,			/* copy_rop */
    NULL,			/* fill_path */
    NULL,			/* stroke_path */
    NULL,			/* fill_mask */
    NULL,			/* fill_trapezoid */
    NULL,			/* fill_parallelogram */
    NULL,			/* fill_triangle */
    NULL,			/* draw_thin_line */
    NULL,			/* begin_image */
    NULL,			/* image_data */
    NULL,			/* end_image */
    x_strip_tile_rectangle,
    NULL,			/* strip_copy_rop */
    NULL,			/* get_clipping_box */
    x_begin_typed_image,
    x_get_bits_rectangle
};

/* The instance is public. */
const gx_device_X gs_x11_device =
{
    std_device_color_body(gx_device_X, &x_procs, "x11",
  FAKE_RES * DEFAULT_WIDTH_10THS / 10, FAKE_RES * DEFAULT_HEIGHT_10THS / 10,	/* x and y extent (nominal) */
			  FAKE_RES, FAKE_RES,	/* x and y density (nominal) */
			  /*dci_color( */ 24, 255, 256 /*) */ ),
    {0},			/* std_procs */
    1 /*true */ ,		/* IsPageDevice */
    {				/* image */
	0, 0,			/* width, height */
	0, XYBitmap, NULL,	/* xoffset, format, data */
	MSBFirst, 8,		/* byte-order, bitmap-unit */
	MSBFirst, 8, 1,		/* bitmap-bit-order, bitmap-pad, depth */
	0, 1,			/* bytes_per_line, bits_per_pixel */
	0, 0, 0,		/* red_mask, green_mask, blue_mask */
	NULL,			/* *obdata */
	{NULL,			/* *(*create_image)() */
	 NULL,			/* (*destroy_image)() */
	 NULL,			/* (*get_pixel)() */
	 NULL,			/* (*put_pixel)() */
	 NULL,			/* *(*sub_image)() */
	 NULL			/* (*add_pixel)() */
	},
    },
    NULL, NULL,			/* dpy, scr */
				/* (connection not initialized) */
    NULL,			/* vinfo */
    (Colormap) None,		/* cmap */
    (Window) None,		/* win */
    NULL,			/* gc */
    (Window) None,		/* pwin */
    (Pixmap) 0,			/* bpixmap */
    0,				/* ghostview */
    (Window) None,		/* mwin */
    {identity_matrix_body},	/* initial matrix (filled in) */
    (Atom) 0, (Atom) 0, (Atom) 0,	/* Atoms: NEXT, PAGE, DONE */
    {0, 0, 0, 0}, 0, 0,		/* update, up_area, up_count */
    (Pixmap) 0,			/* dest */
    0L, (ulong) ~ 0L,		/* colors_or, colors_and */
    {				/* cp */
	(Pixmap) 0,		/* pixmap */
	NULL,			/* gc */
	-1, -1			/* raster, height */
    },
    {				/* ht */
	(Pixmap) None,		/* pixmap */
	(Pixmap) None,		/* no_pixmap */
	gx_no_bitmap_id,	/* id */
	0, 0, 0,		/* width, height, raster */
	0, 0			/* fore_c, back_c */
    },
    GXcopy,			/* function */
    FillSolid,			/* fill_style */
    0,				/* font */
    0, 0,			/* back_color, fore_color */
    0, 0,			/* background, foreground */
    { 0 },			/* cman */
    0, 0,			/* borderColor, borderWidth */
    NULL,			/* geometry */
    128, 5,			/* maxGrayRamp, maxRGBRamp */
    NULL,			/* palette */
    NULL, NULL, NULL,		/* regularFonts, symbolFonts, dingbatFonts */
    NULL, NULL, NULL,		/* regular_fonts, symbol_fonts, dingbat_fonts */
    1, 1,			/* useXFonts, useFontExtensions */
    1, 0,			/* useScalableFonts, logXFonts */
    0.0, 0.0,			/* xResolution, yResolution */
    1,				/* useBackingPixmap */
    1, 1,			/* useXPutImage, useXSetTile */
    {				/* text */
	0,			/* item_count */
	0,			/* char_count */
	{0, 0},			/* origin */
	0,			/* x */
	{
	    {0}},		/* items */
	{0}			/* chars */
    }
};

/* If XPutImage doesn't work, do it ourselves. */
private int alt_put_image(P11(gx_device * dev, Display * dpy, Drawable win,
GC gc, XImage * pi, int sx, int sy, int dx, int dy, unsigned w, unsigned h));

#define put_image(dpy,win,gc,im,sx,sy,x,y,w,h)\
  BEGIN\
    if ( xdev->useXPutImage ) {\
      if (XInitImage(im) == 0)\
	return_error(gs_error_unknownerror);\
      XPutImage(dpy,win,gc,im,sx,sy,x,y,w,h);\
    } else {\
      int code_ = alt_put_image(dev,dpy,win,gc,im,sx,sy,x,y,w,h);\
      if ( code_ < 0 ) return code_;\
    }\
  END

/* Open the device.  Most of the code is in gdevxini.c. */
private int
x_open(gx_device * dev)
{
    gx_device_X *xdev = (gx_device_X *) dev;
    int code = gdev_x_open(xdev);

    if (code < 0)
	return code;
    update_init(dev);
    return 0;
}

/* Free fonts when closing the device. */
private void
free_x_fontmaps(x11fontmap **pmaps)
{
    while (*pmaps) {
	x11fontmap *font = *pmaps;

	*pmaps = font->next;
	if (font->std.names)
	    XFreeFontNames(font->std.names);
	if (font->iso.names)
	    XFreeFontNames(font->iso.names);
	gs_free(font->x11_name, sizeof(char), strlen(font->x11_name) + 1,
		"x11_font_x11name");
	gs_free(font->ps_name, sizeof(char), strlen(font->ps_name) + 1,
		"x11_font_psname");

	gs_free(font, sizeof(x11fontmap), 1, "x11_fontmap");
    }
}

/* Close the device. */
private int
x_close(gx_device * dev)
{
    gx_device_X *xdev = (gx_device_X *) dev;

    if (xdev->ghostview)
	x_send_event(dev, xdev->DONE);
    if (xdev->vinfo) {
	XFree((char *)xdev->vinfo);
	xdev->vinfo = NULL;
    }
    gdev_x_free_colors(xdev);
    free_x_fontmaps(&xdev->dingbat_fonts);
    free_x_fontmaps(&xdev->symbol_fonts);
    free_x_fontmaps(&xdev->regular_fonts);
    XCloseDisplay(xdev->dpy);
    return 0;
}

/* Get initial matrix for X device. */
/* This conflicts seriously with the code for page devices; */
/* we only do it if Ghostview is active. */
private void
x_get_initial_matrix(register gx_device * dev, register gs_matrix * pmat)
{
    gx_device_X *xdev = (gx_device_X *) dev;

    if (!xdev->ghostview) {
	gx_default_get_initial_matrix(dev, pmat);
	return;
    }
    pmat->xx = xdev->initial_matrix.xx;
    pmat->xy = xdev->initial_matrix.xy;
    pmat->yx = xdev->initial_matrix.yx;
    pmat->yy = xdev->initial_matrix.yy;
    pmat->tx = xdev->initial_matrix.tx;
    pmat->ty = xdev->initial_matrix.ty;
}

/* Synchronize the display with the commands already given */
private int
x_sync(register gx_device * dev)
{
    gx_device_X *xdev = (gx_device_X *) dev;

    flush_text(dev);
    update_flush(dev);
    XFlush(xdev->dpy);
    return 0;
}

/* Send event to ghostview process */
private void
x_send_event(gx_device * dev, Atom msg)
{
    gx_device_X *xdev = (gx_device_X *) dev;
    XEvent event;

    event.xclient.type = ClientMessage;
    event.xclient.display = xdev->dpy;
    event.xclient.window = xdev->win;
    event.xclient.message_type = msg;
    event.xclient.format = 32;
    event.xclient.data.l[0] = xdev->mwin;
    event.xclient.data.l[1] = xdev->dest;
    XSendEvent(xdev->dpy, xdev->win, False, 0, &event);
}

/* Output "page" */
private int
x_output_page(gx_device * dev, int num_copies, int flush)
{
    gx_device_X *xdev = (gx_device_X *) dev;

    x_sync(dev);

    /* Send ghostview a "page" client event */
    /* Wait for a "next" client event */
    if (xdev->ghostview) {
	XEvent event;

	x_send_event(dev, xdev->PAGE);
	XNextEvent(xdev->dpy, &event);
	while (event.type != ClientMessage ||
	       event.xclient.message_type != xdev->NEXT) {
	    XNextEvent(xdev->dpy, &event);
	}
    }
    return gx_finish_output_page(dev, num_copies, flush);
}

/* Fill a rectangle with a color. */
private int
x_fill_rectangle(register gx_device * dev,
		 int x, int y, int w, int h, gx_color_index color)
{
    gx_device_X *xdev = (gx_device_X *) dev;

    fit_fill(dev, x, y, w, h);
    flush_text(dev);
    set_fill_style(FillSolid);
    set_fore_color(color);
    set_function(GXcopy);
    XFillRectangle(xdev->dpy, xdev->dest, xdev->gc, x, y, w, h);
    /* If we are filling the entire screen, reset */
    /* colors_or and colors_and.  It's wasteful to test this */
    /* on every operation, but there's no separate driver routine */
    /* for erasepage (yet). */
    if (x == 0 && y == 0 && w == xdev->width && h == xdev->height) {
	if (color == xdev->foreground || color == xdev->background)
	    gdev_x_free_dynamic_colors(xdev);
	xdev->colors_or = xdev->colors_and = color;
    }
    if (xdev->bpixmap != (Pixmap) 0) {
	x_update_add(dev, x, y, w, h);
    }
    if_debug5('F', "[F] fill (%d,%d):(%d,%d) %ld\n",
	      x, y, w, h, (long)color);
    return 0;
}

/* Copy a monochrome bitmap. */
private int
x_copy_mono(register gx_device * dev,
	    const byte * base, int sourcex, int raster, gx_bitmap_id id,
	    int x, int y, int w, int h,
	    gx_color_index zero, gx_color_index one)
/*
 * X doesn't directly support the simple operation of writing a color
 * through a mask specified by an image.  The plot is the following:
 *  If neither color is gx_no_color_index ("transparent"),
 *      use XPutImage with the "copy" function as usual.
 *  If the color either bitwise-includes or is bitwise-included-in
 *      every color written to date
 *      (a special optimization for writing black/white on color displays),
 *      use XPutImage with an appropriate Boolean function.
 *  Otherwise, do the following complicated stuff:
 *      Create pixmap of depth 1 if necessary.
 *      If foreground color is "transparent" then
 *        invert the raster data.
 *      Use XPutImage to copy the raster image to the newly
 *        created Pixmap.
 *      Install the Pixmap as the clip_mask in the X GC and
 *        tweak the clip origin.
 *      Do an XFillRectangle, fill style=solid, specifying a
 *        rectangle the same size as the original raster data.
 *      De-install the clip_mask.
 */
{
    gx_device_X *xdev = (gx_device_X *) dev;
    int function = GXcopy;

    x_pixel
	bc = zero,
	fc = one;

    fit_copy(dev, base, sourcex, raster, id, x, y, w, h);
    flush_text(dev);

    xdev->image.width = sourcex + w;
    xdev->image.height = h;
    xdev->image.data = (char *)base;
    xdev->image.bytes_per_line = raster;
    set_fill_style(FillSolid);

    /* Check for null, easy 1-color, hard 1-color, and 2-color cases. */
    if (zero != gx_no_color_index) {
	if (one != gx_no_color_index) {
	    /* 2-color case. */
	    /* Simply replace existing bits with what's in the image. */
	} else if (!(~xdev->colors_and & bc)) {
	    function = GXand;
	    fc = ~(x_pixel) 0;
	} else if (!(~bc & xdev->colors_or)) {
	    function = GXor;
	    fc = 0;
	} else {
	    goto hard;
	}
    } else {
	if (one == gx_no_color_index) {		/* no-op */
	    return 0;
	} else if (!(~xdev->colors_and & fc)) {
	    function = GXand;
	    bc = ~(x_pixel) 0;
	} else if (!(~fc & xdev->colors_or)) {
	    function = GXor;
	    bc = 0;
	} else {
	    goto hard;
	}
    }
    xdev->image.format = XYBitmap;
    set_function(function);
    if (bc != xdev->back_color) {
	XSetBackground(xdev->dpy, xdev->gc, (xdev->back_color = bc));
    }
    if (fc != xdev->fore_color) {
	XSetForeground(xdev->dpy, xdev->gc, (xdev->fore_color = fc));
    }
    if (zero != gx_no_color_index)
	note_color(zero);
    if (one != gx_no_color_index)
	note_color(one);
    put_image(xdev->dpy, xdev->dest, xdev->gc, &xdev->image,
	      sourcex, 0, x, y, w, h);

    goto out;

  hard:			/* Handle the hard 1-color case. */
    if (raster > xdev->cp.raster || h > xdev->cp.height) {
	/* Must allocate a new pixmap and GC. */
	/* Release the old ones first. */
	free_cp(dev);

	/* Create the clipping pixmap, depth must be 1. */
	xdev->cp.pixmap =
	    XCreatePixmap(xdev->dpy, xdev->win, raster << 3, h, 1);
	if (xdev->cp.pixmap == (Pixmap) 0) {
	    lprintf("x_copy_mono: can't allocate pixmap\n");
	    return_error(gs_error_VMerror);
	}
	xdev->cp.gc = XCreateGC(xdev->dpy, xdev->cp.pixmap, 0, 0);
	if (xdev->cp.gc == (GC) 0) {
	    lprintf("x_copy_mono: can't allocate GC\n");
	    return_error(gs_error_VMerror);
	}
	xdev->cp.raster = raster;
	xdev->cp.height = h;
    }
    /* Initialize static mask image params */
    xdev->image.format = XYBitmap;
    set_function(GXcopy);

    /* Select polarity based on fg/bg transparency. */
    if (one == gx_no_color_index) {	/* invert */
	XSetBackground(xdev->dpy, xdev->cp.gc, (x_pixel) 1);
	XSetForeground(xdev->dpy, xdev->cp.gc, (x_pixel) 0);
	set_fore_color(zero);
    } else {
	XSetBackground(xdev->dpy, xdev->cp.gc, (x_pixel) 0);
	XSetForeground(xdev->dpy, xdev->cp.gc, (x_pixel) 1);
	set_fore_color(one);
    }
    put_image(xdev->dpy, xdev->cp.pixmap, xdev->cp.gc,
	      &xdev->image, sourcex, 0, 0, 0, w, h);

    /* Install as clipmask. */
    XSetClipMask(xdev->dpy, xdev->gc, xdev->cp.pixmap);
    XSetClipOrigin(xdev->dpy, xdev->gc, x, y);

    /*
     * Draw a solid rectangle through the raster clip mask.
     * Note fill style is guaranteed to be solid from above.
     */
    XFillRectangle(xdev->dpy, xdev->dest, xdev->gc, x, y, w, h);

    /* Tidy up.  Free the pixmap if it's big. */
    XSetClipMask(xdev->dpy, xdev->gc, None);
    if (raster * h > X_MAX_TEMP_PIXMAP)
	free_cp(dev);

  out:if (xdev->bpixmap != (Pixmap) 0) {
	/* We wrote to the pixmap, so update the display now. */
	x_update_add(dev, x, y, w, h);
    }
    return 0;
}

/* Internal routine to free the GC and pixmap used for copying. */
private void
free_cp(register gx_device * dev)
{
    gx_device_X *xdev = (gx_device_X *) dev;

    if (xdev->cp.gc != NULL) {
	XFreeGC(xdev->dpy, xdev->cp.gc);
	xdev->cp.gc = NULL;
    }
    if (xdev->cp.pixmap != (Pixmap) 0) {
	XFreePixmap(xdev->dpy, xdev->cp.pixmap);
	xdev->cp.pixmap = (Pixmap) 0;
    }
    xdev->cp.raster = -1;	/* mark as unallocated */
}

/* Copy a color bitmap. */
private int
x_copy_color(register gx_device * dev,
	     const byte * base, int sourcex, int raster, gx_bitmap_id id,
	     int x, int y, int w, int h)
{
    gx_device_X *xdev = (gx_device_X *) dev;
    int depth = dev->color_info.depth;

    fit_copy(dev, base, sourcex, raster, id, x, y, w, h);
    flush_text(dev);
    set_fill_style(FillSolid);
    set_function(GXcopy);

    /* Filling with a colored halftone often gives rise to */
    /* copy_color calls for a single pixel.  Check for this now. */

    if (h == 1 && w == 1) {
	uint sbit = sourcex * depth;
	const byte *ptr = base + (sbit >> 3);
	x_pixel pixel;

	if (depth < 8)
	    pixel = (byte) (*ptr << (sbit & 7)) >> (8 - depth);
	else {
	    pixel = *ptr++;
	    while ((depth -= 8) > 0)
		pixel = (pixel << 8) + *ptr++;
	}
	set_fore_color(pixel);
	XDrawPoint(xdev->dpy, xdev->dest, xdev->gc, x, y);
    } else {
	xdev->image.width = sourcex + w;
	xdev->image.height = h;
	xdev->image.format = ZPixmap;
	xdev->image.data = (char *)base;
	xdev->image.depth = depth;
	xdev->image.bytes_per_line = raster;
	xdev->image.bits_per_pixel = depth;
	if (XInitImage(&xdev->image) == 0)
	    return_error(gs_error_unknownerror);
	XPutImage(xdev->dpy, xdev->dest, xdev->gc, &xdev->image,
		  sourcex, 0, x, y, w, h);
	xdev->image.depth = xdev->image.bits_per_pixel = 1;
    }
    if (xdev->bpixmap != (Pixmap) 0)
	x_update_add(dev, x, y, w, h);
    if_debug4('F', "[F] copy_color (%d,%d):(%d,%d)\n",
	      x, y, w, h);
    return 0;
}

/* Get the device parameters.  See below. */
private int
x_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_X *xdev = (gx_device_X *) dev;
    int code = gx_default_get_params(dev, plist);
    long id = (long)xdev->pwin;

    if (code < 0 ||
	(code = param_write_long(plist, "WindowID", &id)) < 0 ||
	(code = param_write_bool(plist, ".IsPageDevice", &xdev->IsPageDevice)) < 0
	)
	DO_NOTHING;
    return code;
}

/* Set the device parameters.  We reimplement this so we can resize */
/* the window and avoid closing and reopening the device, and to add */
/* .IsPageDevice. */
private int
x_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_X *xdev = (gx_device_X *) dev;
    bool is_open = dev->is_open;
    int width = dev->width;
    int height = dev->height;
    float xres = dev->HWResolution[0];
    float yres = dev->HWResolution[1];
    long pwin = (long)xdev->pwin;
    bool is_page = xdev->IsPageDevice;
    bool save_is_page = xdev->IsPageDevice;
    int ecode = 0, code;

    /* Handle extra parameters */
    switch (code = param_read_long(plist, "WindowID", &pwin)) {
	case 0:
	case 1:
	    break;
	default:
	    ecode = code;
	    param_signal_error(plist, "GSVIEW", ecode);
    }

    switch (code = param_read_bool(plist, ".IsPageDevice", &is_page)) {
	case 0:
	case 1:
	    break;
	default:
	    ecode = code;
	    param_signal_error(plist, "GSVIEW", ecode);
    }

    if (ecode < 0)
	return ecode;

    /* Unless we specified a new window ID, */
    /* prevent gx_default_put_params from closing the device. */
    if (pwin == (long)xdev->pwin)
	dev->is_open = false;
    xdev->IsPageDevice = is_page;
    code = gx_default_put_params(dev, plist);
    dev->is_open = is_open;
    if (code < 0) {		/* Undo setting of .IsPageDevice */
	xdev->IsPageDevice = save_is_page;
	return code;
    }
    if (pwin != (long)xdev->pwin) {
	if (xdev->is_open)
	    gs_closedevice(dev);
	xdev->pwin = (Window) pwin;
    }
    /* If the device is open, resize the window. */
    /* Don't do this if Ghostview is active. */
    if (xdev->is_open && !xdev->ghostview &&
	(dev->width != width || dev->height != height ||
	 dev->HWResolution[0] != xres || dev->HWResolution[1] != yres)
	) {
	int dw = dev->width - width;
	int dh = dev->height - height;
	double qx = dev->HWResolution[0] / xres;
	double qy = dev->HWResolution[1] / xres;

	if (dw != 0 || dh != 0) {
	    XResizeWindow(xdev->dpy, xdev->win,
			  dev->width, dev->height);
	    if (xdev->bpixmap != (Pixmap) 0) {
		XFreePixmap(xdev->dpy, xdev->bpixmap);
		xdev->bpixmap = (Pixmap) 0;
	    }
	    xdev->dest = 0;
	    gdev_x_clear_window(xdev);
	}
	/* Attempt to update the initial matrix in a sensible way. */
	/* The whole handling of the initial matrix is a hack! */
	if (xdev->initial_matrix.xy == 0) {
	    if (xdev->initial_matrix.xx < 0) {	/* 180 degree rotation */
		xdev->initial_matrix.tx += dw;
	    } else {		/* no rotation */
		xdev->initial_matrix.ty += dh;
	    }
	} else {
	    if (xdev->initial_matrix.xy < 0) {	/* 90 degree rotation */
		xdev->initial_matrix.tx += dh;
		xdev->initial_matrix.ty += dw;
	    } else {		/* 270 degree rotation */
	    }
	}
	xdev->initial_matrix.xx *= qx;
	xdev->initial_matrix.xy *= qx;
	xdev->initial_matrix.yx *= qy;
	xdev->initial_matrix.yy *= qy;
    }
    return 0;
}

/* Get the page device.  We reimplement this so that we can make this */
/* device be a page device conditionally. */
private gx_device *
x_get_page_device(gx_device * dev)
{
    return (((gx_device_X *) dev)->IsPageDevice ? dev : (gx_device *) 0);
}

/* Tile a rectangle. */
private int
x_strip_tile_rectangle(register gx_device * dev, const gx_strip_bitmap * tiles,
		       int x, int y, int w, int h,
		       gx_color_index zero, gx_color_index one,
		       int px, int py)
{
    gx_device_X *xdev = (gx_device_X *) dev;

    /* Give up if one color is transparent, or if the tile is colored. */
    /* We should implement the latter someday, since X can handle it. */

    if (one == gx_no_color_index || zero == gx_no_color_index)
	return gx_default_strip_tile_rectangle(dev, tiles, x, y, w, h,
					       zero, one, px, py);

    /* For the moment, give up if the phase or shift is non-zero. */
    if (tiles->shift | px | py)
	return gx_default_strip_tile_rectangle(dev, tiles, x, y, w, h,
					       zero, one, px, py);

    fit_fill(dev, x, y, w, h);
    flush_text(dev);

    /* Imaging with a halftone often gives rise to very small */
    /* tile_rectangle calls.  Check for this now. */

    if (h <= 2 && w <= 2) {
	int j;

	set_fill_style(FillSolid);
	set_function(GXcopy);
	for (j = y + h; --j >= y;) {
	    const byte *ptr =
	    tiles->data + (j % tiles->rep_height) * tiles->raster;
	    int i;

	    for (i = x + w; --i >= x;) {
		uint tx = i % tiles->rep_width;
		byte mask = 0x80 >> (tx & 7);
		x_pixel pixel = (ptr[tx >> 3] & mask ? one : zero);

		set_fore_color(pixel);
		XDrawPoint(xdev->dpy, xdev->dest, xdev->gc, i, j);
	    }
	}
	if (xdev->bpixmap != (Pixmap) 0) {
	    x_update_add(dev, x, y, w, h);
	}
	return 0;
    }
    /*
     * Remember, an X tile is already filled with particular
     * pixel values (i.e., colors).  Therefore if we are changing
     * fore/background color, we must invalidate the tile (using
     * the same technique as in set_tile).  This problem only
     * bites when using grayscale -- you may want to change
     * fg/bg but use the same halftone screen.
     */
    if ((zero != xdev->ht.back_c) || (one != xdev->ht.fore_c))
	xdev->ht.id = ~tiles->id;	/* force reload */

    set_back_color(zero);
    set_fore_color(one);
    if (!set_tile(dev, tiles)) {	/* Bad news.  Fall back to the default algorithm. */
	return gx_default_strip_tile_rectangle(dev, tiles, x, y, w, h,
					       zero, one, px, py);
    }
    /* Use the tile to fill the rectangle */
    set_fill_style(FillTiled);
    set_function(GXcopy);
    XFillRectangle(xdev->dpy, xdev->dest, xdev->gc, x, y, w, h);
    if (xdev->bpixmap != (Pixmap) 0) {
	x_update_add(dev, x, y, w, h);
    }
    if_debug6('F', "[F] tile (%d,%d):(%d,%d) %ld,%ld\n",
	      x, y, w, h, (long)zero, (long)one);
    return 0;
}

/* Implement ImageType 2 using CopyArea if possible. */
/* Note that since ImageType 2 images don't have any source data, */
/* this procedure does all the work. */
private int
x_begin_typed_image(gx_device * dev,
		    const gs_imager_state * pis, const gs_matrix * pmat,
		    const gs_image_common_t * pic, const gs_int_rect * prect,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
		    gs_memory_t * mem, gx_image_enum_common_t ** pinfo)
{
    gx_device_X *xdev = (gx_device_X *) dev;
    const gs_image2_t *pim;
    gs_state *pgs;
    gx_device *sdev;
    gs_matrix smat, dmat;

    if (pic->type->index != 2)
	goto punt;
    pim = (const gs_image2_t *)pic;
    if (!pim->PixelCopy)
	goto punt;
    pgs = pim->DataSource;
    sdev = gs_currentdevice(pgs);
    if (dev->dname != sdev->dname ||
	memcmp(&dev->color_info, &sdev->color_info,
	       sizeof(dev->color_info))
	)
	goto punt;
    flush_text(dev);
    gs_currentmatrix(pgs, &smat);
    /*
     * Figure 7.2 of the Adobe 3010 Supplement says that we should
     * compute CTM x ImageMatrix here, but I'm almost certain it
     * should be the other way around.  Also see gximage2.c.
     */
    gs_matrix_multiply(&pim->ImageMatrix, &smat, &smat);
    if (pis == 0)
	dmat = *pmat;
    else
	gs_currentmatrix((const gs_state *)pis, &dmat);
    if (!((is_xxyy(&dmat) || is_xyyx(&dmat)) &&
#define eqe(e) smat.e == dmat.e
	  eqe(xx) && eqe(xy) && eqe(yx) && eqe(yy))
#undef eqe
	)
	goto punt;
    {
	gs_rect rect, src, dest;
	gs_int_point size;
	int srcx, srcy, destx, desty;

	rect.p.x = rect.p.y = 0;
	rect.q.x = pim->Width, rect.q.y = pim->Height;
	gs_bbox_transform(&rect, &dmat, &dest);
	if (pcpath != NULL &&
	    !gx_cpath_includes_rectangle(pcpath,
			       float2fixed(dest.p.x), float2fixed(dest.p.y),
			       float2fixed(dest.q.x), float2fixed(dest.q.y))
	    )
	    goto punt;
	rect.q.x += (rect.p.x = pim->XOrigin);
	rect.q.y += (rect.p.y = pim->YOrigin);
	gs_bbox_transform(&rect, &smat, &src);
	(*pic->type->source_size) (pis, pic, &size);
	set_fill_style(FillSolid);
	set_function(GXcopy);
	srcx = (int)(src.p.x + 0.5);
	srcy = (int)(src.p.y + 0.5);
	destx = (int)(dest.p.x + 0.5);
	desty = (int)(dest.p.y + 0.5);
	XCopyArea(xdev->dpy, xdev->bpixmap, xdev->bpixmap, xdev->gc,
		  srcx, srcy, size.x, size.y, destx, desty);
	x_update_add(dev, destx, desty, size.x, size.y);
    }
    return 0;
  punt:return gx_default_begin_typed_image(dev, pis, pmat, pic, prect,
					pdcolor, pcpath, mem, pinfo);
}

/* Read bits back from the screen. */
private int
x_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
		     gs_get_bits_params_t * params, gs_int_rect ** unread)
{
    gx_device_X *xdev = (gx_device_X *) dev;
    int depth = dev->color_info.depth;
    int x0 = prect->p.x, y0 = prect->p.y, x1 = prect->q.x, y1 = prect->q.y;
    uint width_bytes = ((x1 - x0) * depth + 7) >> 3;
    uint band = X_MAX_TEMP_IMAGE / width_bytes;
    uint default_raster = bitmap_raster((x1 - x0) * depth);
    gs_get_bits_options_t options = params->options;
    uint raster =
    (options & GB_RASTER_SPECIFIED ? params->raster :
     (params->raster = default_raster));
    long plane_mask = (1L << depth) - 1;
    int y, h;
    XImage *image;
    int code = 0;
#if GET_IMAGE_EXPOSURES
    XWindowAttributes attributes;
#endif /* GET_IMAGE_EXPOSURES */

    if (x0 < 0 || y0 < 0 || x1 > dev->width || y1 > dev->height)
	return_error(gs_error_rangecheck);
    /* XGetImage can only handle x_offset = 0. */
    if ((options & GB_OFFSET_SPECIFIED) && params->x_offset == 0)
	options = (options & ~GB_OFFSET_SPECIFIED) | GB_OFFSET_0;
    if (~options &
	(GB_RETURN_COPY | GB_OFFSET_0 | GB_PACKING_CHUNKY |
	 GB_COLORS_NATIVE) ||
	!(options & GB_ALIGN_ALL) ||
	!(options & GB_RASTER_ALL)
	)
	return
	    gx_default_get_bits_rectangle(dev, prect, params, unread);
    params->options =
	GB_COLORS_NATIVE | GB_ALPHA_NONE | GB_PACKING_CHUNKY |
	GB_RETURN_COPY | GB_OFFSET_0 |
	(options & GB_ALIGN_ALL) |
	(options & GB_RASTER_SPECIFIED ? GB_RASTER_SPECIFIED :
	 GB_RASTER_STANDARD);
    if (x0 >= x1 || y0 >= y1)
	return 0;
    flush_text(dev);
    /*
     * If we want a list of unread rectangles, turn on graphics
     * exposures, and accept exposure events.
     */
	/******
	 ****** FOLLOWING IS WRONG.  XGetImage DOES NOT GENERATE
	 ****** EXPOSURE EVENTS.
	 ******/
#if GET_IMAGE_EXPOSURES
    if (unread) {
	XSetGraphicsExposures(xdev->dpy, xdev->gc, True);
	XGetWindowAttributes(xdev->dpy, xdev->win, &attributes);
	XSelectInput(xdev->dpy, xdev->win,
		     attributes.your_event_mask | ExposureMask);
    }
#endif /* GET_IMAGE_EXPOSURES */
    /*
     * The X library doesn't provide any way to specify the desired
     * bit or byte ordering for the result, so we may have to swap the
     * bit or byte order.
     */
    if (band == 0)
	band = 1;
    for (y = y0; y < y1; y += h) {
	int cy;

	h = min(band, y1 - y);
	image = XGetImage(xdev->dpy, xdev->dest, x0, y, x1 - x0, h,
			  plane_mask, ZPixmap);
	for (cy = y; cy < y + h; ++cy) {
	    const byte *source =
		(const byte *)image->data + (cy - y) * image->bytes_per_line;
	    byte *dest = params->data[0] + (cy - y0) * raster;

	    /*
	     * XGetImage can return an image with any bit order, byte order,
	     * unit size (for bitmaps), and bits_per_pixel it wants: it's up
	     * to us to convert the results.  (It's a major botch in the X
	     * design that even though the server has to have the ability to
	     * convert images from any format to any format, there's no way
	     * to specify a requested format for XGetImage.)
	     */
	    if (image->bits_per_pixel == image->depth &&
		(image->depth > 1 || image->bitmap_bit_order == MSBFirst) &&
		(image->byte_order == MSBFirst || image->depth <= 8)
		) {
		/*
		 * The server has been nice enough to return an image in the
		 * format we use.  
		 */
		memcpy(dest, source, width_bytes);
	    } else {
		/*
		 * We need to swap byte order and/or bit order.  What a
		 * totally unnecessary nuisance!  For the moment, the only
		 * cases we deal with are 16- and 24-bit images with padding
		 * and/or byte swapping.
		 */
		if (image->depth == 24) {
		    int cx;
		    const byte *p = source;
		    byte *q = dest;
		    int step = image->bits_per_pixel >> 3;

		    if (image->byte_order == MSBFirst) {
			p += step - 3;
			for (cx = x0; cx < x1; p += step, q += 3, ++cx)
			    q[0] = p[0], q[1] = p[1], q[2] = p[2];
		    } else {
			for (cx = x0; cx < x1; p += step, q += 3, ++cx)
			    q[0] = p[2], q[1] = p[1], q[2] = p[0];
		    }
		} else if (image->depth == 16) {
		    int cx;
		    const byte *p = source;
		    byte *q = dest;
		    int step = image->bits_per_pixel >> 3;

		    if (image->byte_order == MSBFirst) {
			p += step - 2;
			for (cx = x0; cx < x1; p += step, q += 2, ++cx)
			    q[0] = p[0], q[1] = p[1];
		    } else {
			for (cx = x0; cx < x1; p += step, q += 2, ++cx)
			    q[0] = p[1], q[1] = p[0];
		    }
		} else
		    code = gs_note_error(gs_error_rangecheck);
	    }
	}
	XDestroyImage(image);
    }
    if (unread) {
#if GET_IMAGE_EXPOSURES
	XEvent event;
#endif /* GET_IMAGE_EXPOSURES */

	*unread = 0;
#if GET_IMAGE_EXPOSURES
	/* Read any exposure events. */
	XWindowEvent(xdev->dpy, xdev->win, ExposureMask, &event);
	if (event.type == GraphicsExpose) {
	    gs_int_rect *rects = (gs_int_rect *)
		gs_alloc_bytes(dev->memory, sizeof(gs_int_rect),
			       "x_get_bits_rectangle");
	    int num_rects = 0;

	    for (;;) {
		if (rects == 0) {
		    code = gs_note_error(gs_error_VMerror);
		    break;
		}
#define xevent (*(XGraphicsExposeEvent *)&event)
		rects[num_rects].q.x = xevent.width +
		    (rects[num_rects].p.x = xevent.x);
		rects[num_rects].q.y = xevent.height +
		    (rects[num_rects].p.y = xevent.y);
		++num_rects;
		if (!xevent.count)
		    break;
#undef xevent
		rects = gs_resize_object(dev->memory, rects,
					 (num_rects + 1) * sizeof(gs_int_rect),
					 "x_get_bits_rectangle");
	    }
	    if (code >= 0) {
		*unread = rects;
		code = num_rects;
	    }
	}
	/* Restore the window state. */
	XSetGraphicsExposures(xdev->dpy, xdev->gc, False);
	XSelectInput(xdev->dpy, xdev->win, attributes.your_event_mask);
#endif /* GET_IMAGE_EXPOSURES */
    }
    return code;
}

/* Set up with a specified tile. */
/* Return false if we can't do it for some reason. */
private int
set_tile(register gx_device * dev, register const gx_strip_bitmap * tile)
{
    gx_device_X *xdev = (gx_device_X *) dev;

#ifdef DEBUG
    if (gs_debug['T'])
	return 0;
#endif
    if (tile->id == xdev->ht.id && tile->id != gx_no_bitmap_id)
	return xdev->useXSetTile;
    /* Set up the tile Pixmap */
    if (tile->size.x != xdev->ht.width ||
	tile->size.y != xdev->ht.height ||
	xdev->ht.pixmap == (Pixmap) 0) {
	if (xdev->ht.pixmap != (Pixmap) 0)
	    XFreePixmap(xdev->dpy, xdev->ht.pixmap);
	xdev->ht.pixmap = XCreatePixmap(xdev->dpy, xdev->win,
					tile->size.x, tile->size.y,
					xdev->vinfo->depth);
	if (xdev->ht.pixmap == (Pixmap) 0)
	    return 0;
	xdev->ht.width = tile->size.x, xdev->ht.height = tile->size.y;
	xdev->ht.raster = tile->raster;
    }
    xdev->ht.fore_c = xdev->fore_color;
    xdev->ht.back_c = xdev->back_color;
    /* Copy the tile into the Pixmap */
    xdev->image.data = (char *)tile->data;
    xdev->image.width = tile->size.x;
    xdev->image.height = tile->size.y;
    xdev->image.bytes_per_line = tile->raster;
    xdev->image.format = XYBitmap;
    set_fill_style(FillSolid);
#ifdef DEBUG
    if (gs_debug['H']) {
	int i;

	dlprintf4("[H] 0x%lx: width=%d height=%d raster=%d\n",
	      (ulong) tile->data, tile->size.x, tile->size.y, tile->raster);
	dlputs("");
	for (i = 0; i < tile->raster * tile->size.y; i++)
	    dprintf1(" %02x", tile->data[i]);
	dputc('\n');
    }
#endif
    XSetTile(xdev->dpy, xdev->gc, xdev->ht.no_pixmap);	/* *** X bug *** */
    set_function(GXcopy);
    put_image(xdev->dpy, xdev->ht.pixmap, xdev->gc, &xdev->image,
	      0, 0, 0, 0, tile->size.x, tile->size.y);
    XSetTile(xdev->dpy, xdev->gc, xdev->ht.pixmap);
    xdev->ht.id = tile->id;
    return xdev->useXSetTile;
}


/* ------ Screen update procedures ------ */

/* Flush updates to the screen if needed. */
private void
update_do_flush(register gx_device * dev)
{
    gx_device_X *xdev = (gx_device_X *) dev;

    flush_text(dev);
    if (xdev->up_area != 0) {
	int xo = xdev->update.xo, yo = xdev->update.yo;

	set_function(GXcopy);
	XCopyArea(xdev->dpy, xdev->bpixmap, xdev->win, xdev->gc,
		  xo, yo, xdev->update.xe - xo, xdev->update.ye - yo,
		  xo, yo);
	update_init(dev);
    }
}

/* Add a region to be updated. */
/* This is only called if xdev->bpixmap != 0. */
void
x_update_add(gx_device * dev, int xo, int yo, int w, int h)
{
    register gx_device_X *xdev = (gx_device_X *) dev;
    int xe = xo + w, ye = yo + h;
    long new_area;

    if (X_ALWAYS_UPDATE) {	/* Update the screen now. */
	update_do_flush(dev);
	new_area = (long)w *h;
    } else {			/* Only update the screen if it's worthwhile. */
	if ((++xdev->up_count >= 200 && xdev->up_area > 1000) ||
	    xdev->up_area == 0
	    ) {
	    if (xdev->up_area != 0)
		update_do_flush(dev);
	    new_area = (long)w *h;
	} else {		/* See whether adding this rectangle */
	    /* would result in too much being copied unnecessarily. */
	    long old_area = xdev->up_area;
	    long new_up_area;
	    x_rect u;

	    u.xo = min(xo, xdev->update.xo);
	    u.yo = min(yo, xdev->update.yo);
	    u.xe = max(xe, xdev->update.xe);
	    u.ye = max(ye, xdev->update.ye);
	    new_up_area = (long)(u.xe - u.xo) * (u.ye - u.yo);
	    /* The fraction of new_up_area used in the following test */
	    /* is not particularly critical; using a denominator */
	    /* that is a power of 2 eliminates a divide. */
	    if (u.xe - u.xo >= 10 && u.ye - u.yo >= 10 &&
		old_area + (new_area = (long)w * h) <
		new_up_area - (new_up_area >> 2)
		)
		update_do_flush(dev);
	    else {
		xdev->update = u;
		xdev->up_area = new_up_area;
		return;
	    }
	}
    }

    xdev->update.xo = xo;
    xdev->update.yo = yo;
    xdev->update.xe = xe;
    xdev->update.ye = ye;
    xdev->up_area = new_area;
}

/* Flush buffered text to the screen. */
private void
do_flush_text(gx_device * dev)
{
    gx_device_X *xdev = (gx_device_X *) dev;

    if (!IN_TEXT(xdev))
	return;
    DRAW_TEXT(xdev);
    xdev->text.item_count = xdev->text.char_count = 0;
}

/* ------ Internal procedures ------ */

/* Substitute for XPutImage using XFillRectangle. */
/* This is a total hack to get around an apparent bug */
/* in some X servers.  It only works with the specific */
/* parameters (bit/byte order, padding) used above. */
private int
alt_put_image(gx_device * dev, Display * dpy, Drawable win, GC gc,
	XImage * pi, int sx, int sy, int dx, int dy, unsigned w, unsigned h)
{
    int raster = pi->bytes_per_line;
    byte *data = (byte *) pi->data + sy * raster + (sx >> 3);
    int init_mask = 0x80 >> (sx & 7);
    int invert = 0;
    int yi;

#define nrects 40
    XRectangle rects[nrects];
    XRectangle *rp = rects;

    XGCValues gcv;

    XGetGCValues(dpy, gc, (GCFunction | GCForeground | GCBackground), &gcv);

    if (gcv.function == GXcopy) {
	XSetForeground(dpy, gc, gcv.background);
	XFillRectangle(dpy, win, gc, dx, dy, w, h);
	XSetForeground(dpy, gc, gcv.foreground);
    } else if (gcv.function == GXand) {
	if (gcv.background != ~(x_pixel) 0) {
	    XSetForeground(dpy, gc, gcv.background);
	    invert = 0xff;
	}
    } else if (gcv.function == GXor) {
	if (gcv.background != 0) {
	    XSetForeground(dpy, gc, gcv.background);
	    invert = 0xff;
	}
    } else {
	lprintf("alt_put_image: unimplemented function.\n");
	return_error(gs_error_rangecheck);
    }

    for (yi = 0; yi < h; yi++, data += raster) {
	register int mask = init_mask;
	register byte *dp = data;
	register int xi = 0;

	while (xi < w) {
	    if ((*dp ^ invert) & mask) {
		int xleft = xi;

		if (rp == &rects[nrects]) {
		    XFillRectangles(dpy, win, gc, rects, nrects);
		    rp = rects;
		}
		/* Scan over a run of 1-bits */
		rp->x = dx + xi, rp->y = dy + yi;
		do {
		    if (!(mask >>= 1))
			mask = 0x80, dp++;
		    xi++;
		} while (xi < w && (*dp & mask));
		rp->width = xi - xleft, rp->height = 1;
		rp++;
	    } else {
		if (!(mask >>= 1))
		    mask = 0x80, dp++;
		xi++;
	    }
	}
    }
    XFillRectangles(dpy, win, gc, rects, rp - rects);
    if (invert)
	XSetForeground(dpy, gc, gcv.foreground);
    return 0;
}
