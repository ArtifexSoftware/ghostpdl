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


/* Definitions for X Windows drivers */
/* Requires gxdevice.h and x_.h */

#ifndef gdevx_INCLUDED
#  define gdevx_INCLUDED

#include "gdevbbox.h"
#include "gdevxcmp.h"

/* Declare the X resource tables compiled separately in gdevxres.c. */
extern XtResource gdev_x_resources[];
extern const int gdev_x_resource_count;
extern String gdev_x_fallback_resources[];

/* Define the X Windows device */
typedef struct gx_device_X_s {
    gx_device_bbox_common;	/* if target != 0, is image buffer */
    /*
     * Normally, an X device has an image buffer iff target != 0.  However,
     * the bbox device sometimes sets target to NULL temporarily, so we need
     * a separate flag to record whether this device is buffered.
     */
    bool is_buffered;
    bool IsPageDevice;
    byte *buffer;		/* full-window image */
    size_t buffer_size;
    gx_device_color_info        orig_color_info;

    /* An XImage object for writing bitmap images to the screen */
    XImage image;

    /* Global X state */
    Display *dpy;
    Screen *scr;
    XVisualInfo *vinfo;
    Colormap cmap;
    Window win;
    GC gc;

    /* An optional Window ID supplied as a device parameter */
    Window pwin;

    /* A backing pixmap so X will handle exposure automatically */
    Pixmap bpixmap;		/* 0 if useBackingPixmap is false, */
                                /* or if it can't be allocated */
    int ghostview;		/* flag to tell if ghostview is in control */
    Window mwin;		/* window to receive ghostview messages */
    gs_matrix initial_matrix;	/* the initial transformation */
    Atom NEXT, PAGE, DONE;	/* Atoms used to talk to ghostview */
    struct {
        gs_int_rect box;	/* region needing updating */
        long area;		/* total area of update */
        long total;		/* total of individual area updates */
        int count;		/* # of updates since flush */
    } update;
    Pixmap dest;		/* bpixmap if non-0, else use win */
    x_pixel colors_or;		/* 'or' of all device colors used so far */
    x_pixel colors_and;		/* 'and' ditto */

    /* An intermediate pixmap for the stencil case of copy_mono */
    struct {
        Pixmap pixmap;
        GC gc;
        int raster, height;
    } cp;

    /* Structure for dealing with the halftone tile. */
    /* Later this might become a multi-element cache. */
    struct {
        Pixmap pixmap;
        Pixmap no_pixmap;	/* kludge to get around X bug */
        gx_bitmap_id id;
        int width, height, raster;
        x_pixel fore_c, back_c;
    } ht;

    /* Cache the function and fill style from the GC */
    int function;
    int fill_style;
    Font fid;

#define X_SET_FILL_STYLE(xdev, style)\
  BEGIN\
    if (xdev->fill_style != (style))\
      XSetFillStyle(xdev->dpy, xdev->gc, (xdev->fill_style = (style)));\
  END
#define X_SET_FUNCTION(xdev, func)\
  BEGIN\
    if (xdev->function != (func))\
      XSetFunction(xdev->dpy, xdev->gc, (xdev->function = (func)));\
  END
#define X_SET_FONT(xdev, font)\
  BEGIN\
    if (xdev->fid != (font))\
      XSetFont(xdev->dpy, xdev->gc, (xdev->fid = (font)));\
  END

    x_pixel back_color, fore_color;

    Pixel background, foreground;

    /*
     * The color management structure is defined in gdevxcmp.h and is
     * managed by the code in gdevxcmp.c.
     */
    x11_cman_t cman;

#define NOTE_COLOR(xdev, pixel)\
  (xdev->colors_or |= (pixel),\
   xdev->colors_and &= (pixel))
#define X_SET_BACK_COLOR(xdev, pixel)\
  BEGIN\
    if (xdev->back_color != (pixel)) {\
      xdev->back_color = (pixel);\
      NOTE_COLOR(xdev, pixel);\
      XSetBackground(xdev->dpy, xdev->gc, (pixel));\
    }\
  END
#define X_SET_FORE_COLOR(xdev, pixel)\
  BEGIN\
    if (xdev->fore_color != (pixel)) {\
      xdev->fore_color = (pixel);\
      NOTE_COLOR(xdev, pixel);\
      XSetForeground(xdev->dpy, xdev->gc, (pixel));\
    }\
  END

    /* Defaults set by resources */
    Pixel borderColor;
    Dimension borderWidth;
    String geometry;
    int maxGrayRamp, maxRGBRamp;
    String palette;
    float xResolution, yResolution;

    /* Flags work around various X server problems. */
    Boolean useBackingPixmap;
    Boolean useXPutImage;
    Boolean useXSetTile;

    /*
     * Parameters for the screen update algorithms.
     */

    /*
     * Define whether to update after every write, for debugging.
     * Note that one can obtain the same effect by setting any of
     */
    bool AlwaysUpdate;
    /*
     * Define the maximum size of the temporary pixmap for copy_mono
     * that we are willing to leave lying around in the server
     * between uses.
     */
    int MaxTempPixmap;
    /*
     * Define the maximum size of the temporary image created in memory
     * for get_bits_rectangle.
     */
    int MaxTempImage;

    /*
     * Buffered text awaiting display.
     */
    struct {
        int item_count;
#define IN_TEXT(xdev) ((xdev)->text.item_count != 0)
        int char_count;
        gs_int_point origin;
        int x;			/* after last buffered char */
#define MAX_TEXT_ITEMS 12
        XTextItem items[MAX_TEXT_ITEMS];
#define MAX_TEXT_CHARS 25
        char chars[MAX_TEXT_CHARS];
    } text;
/*
 * All the GC parameters are set correctly when we buffer the first
 * character: we must call DRAW_TEXT before resetting any of them.
 * DRAW_TEXT assumes xdev->text.{item,char}_count > 0.
 */
#define DRAW_TEXT(xdev)\
   XDrawText(xdev->dpy, xdev->dest, xdev->gc, xdev->text.origin.x,\
             xdev->text.origin.y, xdev->text.items, xdev->text.item_count)

} gx_device_X;

/* Send an event to the Ghostview process */
void gdev_x_send_event(gx_device_X *xdev, Atom msg);

/* function to keep track of screen updates */
void x_update_add(gx_device_X *, int, int, int, int);
void gdev_x_clear_window(gx_device_X *);
int x_catch_free_colors(Display *, XErrorEvent *);

/* Number used to distinguish when resolution was set from the command line */
#define FAKE_RES (16*72)

/* ------ Inter-module procedures ------ */

/* Exported by gdevxcmp.c for gdevxini.c */
int gdev_x_setup_colors(gx_device_X *);
void gdev_x_free_colors(gx_device_X *);
void gdev_x_free_dynamic_colors(gx_device_X *);

/* Exported by gdevxini.c for gdevx.c */
int gdev_x_open(gx_device_X *);
int gdev_x_close(gx_device_X *);

/* Driver procedures exported for gdevx.c */
dev_proc_map_rgb_color(gdev_x_map_rgb_color);  /* gdevxcmp.c */
dev_proc_map_color_rgb(gdev_x_map_color_rgb);  /* gdevxcmp.c */
dev_proc_get_params(gdev_x_get_params);  /* gdevxini.c */
dev_proc_put_params(gdev_x_put_params);  /* gdevxini.c */
dev_proc_initialize_device_procs(gdev_x_initialize_device_procs);  /* gdevxini.c */
dev_proc_open_device(x_open);
dev_proc_get_initial_matrix(x_get_initial_matrix);
dev_proc_sync_output(x_sync);
dev_proc_output_page(x_output_page);
dev_proc_close_device(x_close);
dev_proc_fill_rectangle(x_fill_rectangle);
dev_proc_copy_mono(x_copy_mono);
dev_proc_copy_color(x_copy_color);
dev_proc_get_page_device(x_get_page_device);
dev_proc_strip_tile_rectangle(x_strip_tile_rectangle);
dev_proc_get_bits_rectangle(x_get_bits_rectangle);
dev_proc_fillpage(x_fillpage);


#endif /* gdevx_INCLUDED */
