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


/* X Windows driver initialization for Ghostscript library */
#include "memory_.h"
#include "x_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gdevx.h"

extern char *getenv(P1(const char *));

/* Define constants for orientation from ghostview */
/* Number represents clockwise rotation of the paper in degrees */
typedef enum {
    Portrait = 0,		/* Normal portrait orientation */
    Landscape = 90,		/* Normal landscape orientation */
    Upsidedown = 180,		/* Don't think this will be used much */
    Seascape = 270		/* Landscape rotated the wrong way */
} orientation;

/* Forward/external references */
int gdev_x_setup_colors(P1(gx_device_X *));
private void gdev_x_setup_fontmap(P1(gx_device_X *));

/* Catch the alloc error when there is not enough resources for the
 * backing pixmap.  Automatically shut off backing pixmap and let the
 * user know when this happens.
 *
 * Because the X API was designed without adequate thought to reentrancy,
 * these variables must be allocated statically.  We do not see how this
 * code can work reliably in the presence of multi-threading.
 */
private struct xv_ {
    Boolean alloc_error;
    XErrorHandler orighandler;
    XErrorHandler oldhandler;
} x_error_handler;

private int
x_catch_alloc(Display * dpy, XErrorEvent * err)
{
    if (err->error_code == BadAlloc)
	x_error_handler.alloc_error = True;
    if (x_error_handler.alloc_error)
	return 0;
    return x_error_handler.oldhandler(dpy, err);
}

int
x_catch_free_colors(Display * dpy, XErrorEvent * err)
{
    if (err->request_code == X_FreeColors)
	return 0;
    return x_error_handler.orighandler(dpy, err);
}

/* Open the X device */
int
gdev_x_open(register gx_device_X * xdev)
{
    XSizeHints sizehints;
    char *window_id;
    XEvent event;
    XVisualInfo xvinfo;
    int nitems;
    XtAppContext app_con;
    Widget toplevel;
    Display *dpy;
    XColor xc;
    int zero = 0;
    int xid_height, xid_width;
    int code;

#ifdef DEBUG
# ifdef have_Xdebug
    if (gs_debug['X']) {
	extern int _Xdebug;

	_Xdebug = 1;
    }
# endif
#endif
    if (!(xdev->dpy = XOpenDisplay((char *)NULL))) {
	char *dispname = getenv("DISPLAY");

	eprintf1("Cannot open X display `%s'.\n",
		 (dispname == NULL ? "(null)" : dispname));
	return_error(gs_error_ioerror);
    }
    xdev->dest = 0;
    if ((window_id = getenv("GHOSTVIEW"))) {
	if (!(xdev->ghostview = sscanf(window_id, "%ld %ld",
				       &(xdev->win), &(xdev->dest)))) {
	    eprintf("Cannot get Window ID from ghostview.\n");
	    return_error(gs_error_ioerror);
	}
    }
    if (xdev->pwin != (Window) None) {	/* pick up the destination window parameters if specified */
	XWindowAttributes attrib;

	xdev->win = xdev->pwin;
	if (XGetWindowAttributes(xdev->dpy, xdev->win, &attrib)) {
	    xdev->scr = attrib.screen;
	    xvinfo.visual = attrib.visual;
	    xdev->cmap = attrib.colormap;
	    xid_width = attrib.width;
	    xid_height = attrib.height;
	} else {
	    /* No idea why we can't get the attributes, but */
	    /* we shouldn't let it lead to a failure below. */
	    xid_width = xid_height = 0;
	}
    } else if (xdev->ghostview) {
	XWindowAttributes attrib;
	Atom type;
	int format;
	unsigned long nitems, bytes_after;
	char *buf;
	Atom ghostview_atom = XInternAtom(xdev->dpy, "GHOSTVIEW", False);

	if (XGetWindowAttributes(xdev->dpy, xdev->win, &attrib)) {
	    xdev->scr = attrib.screen;
	    xvinfo.visual = attrib.visual;
	    xdev->cmap = attrib.colormap;
	    xdev->width = attrib.width;
	    xdev->height = attrib.height;
	}
	/* Delete property if explicit dest is given */
	if (XGetWindowProperty(xdev->dpy, xdev->win, ghostview_atom, 0,
			       256, (xdev->dest != 0), XA_STRING,
			       &type, &format, &nitems, &bytes_after,
			       (unsigned char **)&buf) == 0 &&
	    type == XA_STRING) {
	    int llx, lly, urx, ury;
	    int left_margin = 0, bottom_margin = 0;
	    int right_margin = 0, top_margin = 0;

	    /* We declare page_orientation as an int so that we can */
	    /* use an int * to reference it for sscanf; compilers */
	    /* might be tempted to use less space to hold it if */
	    /* it were declared as an orientation. */
	    int /*orientation */ page_orientation;
	    float xppp, yppp;	/* pixels per point */

	    nitems = sscanf(buf,
			    "%ld %d %d %d %d %d %f %f %d %d %d %d",
			    &(xdev->bpixmap), &page_orientation,
			    &llx, &lly, &urx, &ury,
			    &(xdev->x_pixels_per_inch),
			    &(xdev->y_pixels_per_inch),
			    &left_margin, &bottom_margin,
			    &right_margin, &top_margin);
	    if (!(nitems == 8 || nitems == 12)) {
		eprintf("Cannot get ghostview property.\n");
		return_error(gs_error_ioerror);
	    }
	    if (xdev->dest && xdev->bpixmap) {
		eprintf("Both destination and backing pixmap specified.\n");
		return_error(gs_error_rangecheck);
	    }
	    if (xdev->dest) {
		Window root;
		int x, y;
		unsigned int width, height;
		unsigned int border_width, depth;

		if (XGetGeometry(xdev->dpy, xdev->dest, &root, &x, &y,
				 &width, &height, &border_width, &depth)) {
		    xdev->width = width;
		    xdev->height = height;
		}
	    }
	    xppp = xdev->x_pixels_per_inch / 72.0;
	    yppp = xdev->y_pixels_per_inch / 72.0;
	    switch (page_orientation) {
		case Portrait:
		    xdev->initial_matrix.xx = xppp;
		    xdev->initial_matrix.xy = 0.0;
		    xdev->initial_matrix.yx = 0.0;
		    xdev->initial_matrix.yy = -yppp;
		    xdev->initial_matrix.tx = -llx * xppp;
		    xdev->initial_matrix.ty = ury * yppp;
		    break;
		case Landscape:
		    xdev->initial_matrix.xx = 0.0;
		    xdev->initial_matrix.xy = yppp;
		    xdev->initial_matrix.yx = xppp;
		    xdev->initial_matrix.yy = 0.0;
		    xdev->initial_matrix.tx = -lly * xppp;
		    xdev->initial_matrix.ty = -llx * yppp;
		    break;
		case Upsidedown:
		    xdev->initial_matrix.xx = -xppp;
		    xdev->initial_matrix.xy = 0.0;
		    xdev->initial_matrix.yx = 0.0;
		    xdev->initial_matrix.yy = yppp;
		    xdev->initial_matrix.tx = urx * xppp;
		    xdev->initial_matrix.ty = -lly * yppp;
		    break;
		case Seascape:
		    xdev->initial_matrix.xx = 0.0;
		    xdev->initial_matrix.xy = -yppp;
		    xdev->initial_matrix.yx = -xppp;
		    xdev->initial_matrix.yy = 0.0;
		    xdev->initial_matrix.tx = ury * xppp;
		    xdev->initial_matrix.ty = urx * yppp;
		    break;
	    }

	    /* The following sets the imageable area according to the */
	    /* bounding box and margins sent by ghostview.            */
	    /* This code has been patched many times; its current state */
	    /* is per a recommendation by Tim Theisen on 4/28/95. */

	    xdev->ImagingBBox[0] = llx - left_margin;
	    xdev->ImagingBBox[1] = lly - bottom_margin;
	    xdev->ImagingBBox[2] = urx + right_margin;
	    xdev->ImagingBBox[3] = ury + top_margin;
	    xdev->ImagingBBox_set = true;

	} else if (xdev->pwin == (Window) None) {
	    eprintf("Cannot get ghostview property.\n");
	    return_error(gs_error_ioerror);
	}
    } else {
	Screen *scr = DefaultScreenOfDisplay(xdev->dpy);

	xdev->scr = scr;
	xvinfo.visual = DefaultVisualOfScreen(scr);
	xdev->cmap = DefaultColormapOfScreen(scr);
    }

    xvinfo.visualid = XVisualIDFromVisual(xvinfo.visual);
    xdev->vinfo = XGetVisualInfo(xdev->dpy, VisualIDMask, &xvinfo, &nitems);
    if (xdev->vinfo == NULL) {
	eprintf("Cannot get XVisualInfo.\n");
	return_error(gs_error_ioerror);
    }
    /* Buggy X servers may cause a Bad Access on XFreeColors. */
    x_error_handler.orighandler = XSetErrorHandler(x_catch_free_colors);

    /* Get X Resources.  Use the toolkit for this. */
    XtToolkitInitialize();
    app_con = XtCreateApplicationContext();
    XtAppSetFallbackResources(app_con, gdev_x_fallback_resources);
    dpy = XtOpenDisplay(app_con, NULL, "ghostscript", "Ghostscript",
			NULL, 0, &zero, NULL);
    toplevel = XtAppCreateShell(NULL, "Ghostscript",
				applicationShellWidgetClass, dpy, NULL, 0);
    XtGetApplicationResources(toplevel, (XtPointer) xdev,
			      gdev_x_resources, gdev_x_resource_count,
			      NULL, 0);

    /* Reserve foreground and background colors under the regular connection. */
    xc.pixel = xdev->foreground;
    XQueryColor(xdev->dpy, xdev->cmap, &xc);
    XAllocColor(xdev->dpy, xdev->cmap, &xc);
    xc.pixel = xdev->background;
    XQueryColor(xdev->dpy, xdev->cmap, &xc);
    XAllocColor(xdev->dpy, xdev->cmap, &xc);

    code = gdev_x_setup_colors(xdev);
    if (code < 0) {
	XCloseDisplay(xdev->dpy);
	return code;
    }
    gdev_x_setup_fontmap(xdev);

    if (!xdev->ghostview) {
	XWMHints wm_hints;
	XSetWindowAttributes xswa;
	gx_device *dev = (gx_device *) xdev;

	/* Take care of resolution and paper size. */
	if (xdev->x_pixels_per_inch == FAKE_RES ||
	    xdev->y_pixels_per_inch == FAKE_RES) {
	    float xsize = (float)xdev->width / xdev->x_pixels_per_inch;
	    float ysize = (float)xdev->height / xdev->y_pixels_per_inch;

	    if (xdev->xResolution == 0.0 && xdev->yResolution == 0.0) {
		float dpi, xdpi, ydpi;

		xdpi = 25.4 * WidthOfScreen(xdev->scr) /
		    WidthMMOfScreen(xdev->scr);
		ydpi = 25.4 * HeightOfScreen(xdev->scr) /
		    HeightMMOfScreen(xdev->scr);
		dpi = min(xdpi, ydpi);
		/*
		 * Some X servers provide very large "virtual screens", and
		 * return the virtual screen size for Width/HeightMM but the
		 * physical size for Width/Height.  Attempt to detect and
		 * correct for this now.  This is a KLUDGE required because
		 * the X server provides no way to read the screen
		 * resolution directly.
		 */
		if (dpi < 30)
		    dpi = 75;	/* arbitrary */
		else {
		    while (xsize * dpi > WidthOfScreen(xdev->scr) - 32 ||
			   ysize * dpi > HeightOfScreen(xdev->scr) - 32)
			dpi *= 0.95;
		}
		xdev->x_pixels_per_inch = dpi;
		xdev->y_pixels_per_inch = dpi;
	    } else {
		xdev->x_pixels_per_inch = xdev->xResolution;
		xdev->y_pixels_per_inch = xdev->yResolution;
	    }
	    if (xdev->width > WidthOfScreen(xdev->scr)) {
		xdev->width = xsize * xdev->x_pixels_per_inch;
	    }
	    if (xdev->height > HeightOfScreen(xdev->scr)) {
		xdev->height = ysize * xdev->y_pixels_per_inch;
	    }
	    xdev->MediaSize[0] =
		(float)xdev->width / xdev->x_pixels_per_inch * 72;
	    xdev->MediaSize[1] =
		(float)xdev->height / xdev->y_pixels_per_inch * 72;
	}
	sizehints.x = 0;
	sizehints.y = 0;
	sizehints.width = xdev->width;
	sizehints.height = xdev->height;
	sizehints.flags = 0;

	if (xdev->geometry != NULL) {
	    /*
	     * Note that border_width must be set first.  We can't use
	     * scr, because that is a Screen*, and XWMGeometry wants
	     * the screen number.
	     */
	    char gstr[40];
	    int bitmask;

	    sprintf(gstr, "%dx%d+%d+%d", sizehints.width,
		    sizehints.height, sizehints.x, sizehints.y);
	    bitmask = XWMGeometry(xdev->dpy, DefaultScreen(xdev->dpy),
				  xdev->geometry, gstr, xdev->borderWidth,
				  &sizehints,
				  &sizehints.x, &sizehints.y,
				  &sizehints.width, &sizehints.height,
				  &sizehints.win_gravity);

	    if (bitmask & (XValue | YValue))
		sizehints.flags |= USPosition;
	}
	gx_default_get_initial_matrix(dev, &(xdev->initial_matrix));

	if (xdev->pwin != (Window) None && xid_width != 0 && xid_height != 0) {
#if 0				/*************** */

	    /*
	     * The user who originally implemented the WindowID feature
	     * provided the following code to scale the displayed output
	     * to fit in the window.  We think this is a bad idea,
	     * since it doesn't track window resizing and is generally
	     * completely at odds with the way Ghostscript treats
	     * window or paper size in all other contexts.  We are
	     * leaving the code here in case someone decides that
	     * this really is the behavior they want.
	     */

	    /* Scale to fit in the window. */
	    xdev->initial_matrix.xx
		= xdev->initial_matrix.xx *
		(float)xid_width / (float)xdev->width;
	    xdev->initial_matrix.yy
		= xdev->initial_matrix.yy *
		(float)xid_height / (float)xdev->height;

#endif /*************** */
	    xdev->width = xid_width;
	    xdev->height = xid_height;
	    xdev->initial_matrix.ty = xdev->height;
	} else {		/* !xdev->pwin */
	    xswa.event_mask = ExposureMask;
	    xswa.background_pixel = xdev->background;
	    xswa.border_pixel = xdev->borderColor;
	    xswa.colormap = xdev->cmap;
	    xdev->win = XCreateWindow(xdev->dpy, RootWindowOfScreen(xdev->scr),
				      sizehints.x, sizehints.y,		/* upper left */
				      xdev->width, xdev->height,
				      xdev->borderWidth,
				      xdev->vinfo->depth,
				      InputOutput,	/* class        */
				      xdev->vinfo->visual,	/* visual */
				      CWEventMask | CWBackPixel |
				      CWBorderPixel | CWColormap,
				      &xswa);
	    XStoreName(xdev->dpy, xdev->win, "ghostscript");
	    XSetWMNormalHints(xdev->dpy, xdev->win, &sizehints);
	    wm_hints.flags = InputHint;
	    wm_hints.input = False;
	    XSetWMHints(xdev->dpy, xdev->win, &wm_hints);	/* avoid input focus */
	}
    }
    /***
     *** According to Ricard Torres (torres@upf.es), we have to wait until here
     *** to close the toolkit connection, which we formerly did
     *** just after the calls on XAllocColor above.  I suspect that
     *** this will cause things to be left dangling if an error occurs
     *** anywhere in the above code, but I'm willing to let users
     *** fight over fixing it, since I have no idea what's right.
     ***/

    /* And close the toolkit connection. */
    XtDestroyWidget(toplevel);
    XtCloseDisplay(dpy);
    XtDestroyApplicationContext(app_con);

    xdev->ht.pixmap = (Pixmap) 0;
    xdev->ht.id = gx_no_bitmap_id;;
    xdev->fill_style = FillSolid;
    xdev->function = GXcopy;
    xdev->fid = (Font) 0;

    /* Set up a graphics context */
    xdev->gc = XCreateGC(xdev->dpy, xdev->win, 0, (XGCValues *) NULL);
    XSetFunction(xdev->dpy, xdev->gc, GXcopy);
    XSetLineAttributes(xdev->dpy, xdev->gc, 0,
		       LineSolid, CapButt, JoinMiter);

    gdev_x_clear_window(xdev);

    if (!xdev->ghostview) {	/* Make the window appear. */
	XMapWindow(xdev->dpy, xdev->win);

	/* Before anything else, do a flush and wait for */
	/* an exposure event. */
	XFlush(xdev->dpy);
	if (xdev->pwin == (Window) None) {	/* there isn't a next event for existing windows */
	    XNextEvent(xdev->dpy, &event);
	}
	/* Now turn off graphics exposure events so they don't queue up */
	/* indefinitely.  Also, since we can't do anything about real */
	/* Expose events, mask them out. */
	XSetGraphicsExposures(xdev->dpy, xdev->gc, False);
	XSelectInput(xdev->dpy, xdev->win, NoEventMask);
    } else {
	/* Create an unmapped window, that the window manager will ignore.
	 * This invisible window will be used to receive "next page"
	 * events from ghostview */
	XSetWindowAttributes attributes;

	attributes.override_redirect = True;
	xdev->mwin = XCreateWindow(xdev->dpy, RootWindowOfScreen(xdev->scr),
				   0, 0, 1, 1, 0, CopyFromParent,
				   CopyFromParent, CopyFromParent,
				   CWOverrideRedirect, &attributes);
	xdev->NEXT = XInternAtom(xdev->dpy, "NEXT", False);
	xdev->PAGE = XInternAtom(xdev->dpy, "PAGE", False);
	xdev->DONE = XInternAtom(xdev->dpy, "DONE", False);
    }

    xdev->ht.no_pixmap = XCreatePixmap(xdev->dpy, xdev->win, 1, 1,
				       xdev->vinfo->depth);

    return 0;
}

/* Allocate the backing pixmap, if any, and clear the window. */
void
gdev_x_clear_window(gx_device_X * xdev)
{
    if (!xdev->ghostview) {
	if (xdev->useBackingPixmap) {
	    x_error_handler.oldhandler = XSetErrorHandler(x_catch_alloc);
	    x_error_handler.alloc_error = False;
	    xdev->bpixmap =
		XCreatePixmap(xdev->dpy, xdev->win,
			      xdev->width, xdev->height,
			      xdev->vinfo->depth);
	    XSync(xdev->dpy, False);	/* Force the error */
	    if (x_error_handler.alloc_error) {
		xdev->useBackingPixmap = False;
#ifdef DEBUG
		eprintf("Warning: Failed to allocated backing pixmap.\n");
#endif
		if (xdev->bpixmap) {
		    XFreePixmap(xdev->dpy, xdev->bpixmap);
		    xdev->bpixmap = None;
		    XSync(xdev->dpy, False);	/* Force the error */
		}
	    }
	    x_error_handler.oldhandler =
		XSetErrorHandler(x_error_handler.oldhandler);
	} else
	    xdev->bpixmap = (Pixmap) 0;
    }
    /* Clear the destination pixmap to avoid initializing with garbage. */
    if (xdev->dest != (Pixmap) 0) {
	XSetForeground(xdev->dpy, xdev->gc, xdev->background);
	XFillRectangle(xdev->dpy, xdev->dest, xdev->gc,
		       0, 0, xdev->width, xdev->height);
    } else {
	xdev->dest = (xdev->bpixmap != (Pixmap) 0 ?
		      xdev->bpixmap : (Pixmap) xdev->win);
    }

    /* Clear the background pixmap to avoid initializing with garbage. */
    if (xdev->bpixmap != (Pixmap) 0) {
	if (!xdev->ghostview)
	    XSetWindowBackgroundPixmap(xdev->dpy, xdev->win, xdev->bpixmap);
	XSetForeground(xdev->dpy, xdev->gc, xdev->background);
	XFillRectangle(xdev->dpy, xdev->bpixmap, xdev->gc,
		       0, 0, xdev->width, xdev->height);
    }
    /* Initialize foreground and background colors */
    xdev->back_color = xdev->background;
    XSetBackground(xdev->dpy, xdev->gc, xdev->background);
    xdev->fore_color = xdev->background;
    XSetForeground(xdev->dpy, xdev->gc, xdev->background);
    xdev->colors_or = xdev->colors_and = xdev->background;
}

/* ------ Initialize font mapping ------ */

/* Extract the PostScript font name from the font map resource. */
private const char *
get_ps_name(const char **cpp, int *len)
{
    const char *ret;

    *len = 0;
    /* skip over whitespace and newlines */
    while (**cpp == ' ' || **cpp == '\t' || **cpp == '\n') {
	(*cpp)++;
    }
    /* return font name up to ":", whitespace, or end of string */
    if (**cpp == ':' || **cpp == '\0') {
	return NULL;
    }
    ret = *cpp;
    while (**cpp != ':' &&
	   **cpp != ' ' && **cpp != '\t' && **cpp != '\n' &&
	   **cpp != '\0') {
	(*cpp)++;
	(*len)++;
    }
    return ret;
}

/* Extract the X11 font name from the font map resource. */
private const char *
get_x11_name(const char **cpp, int *len)
{
    const char *ret;
    int dashes = 0;

    *len = 0;
    /* skip over whitespace and the colon */
    while (**cpp == ' ' || **cpp == '\t' ||
	   **cpp == ':') {
	(*cpp)++;
    }
    /* return font name up to end of line or string */
    if (**cpp == '\0' || **cpp == '\n') {
	return NULL;
    }
    ret = *cpp;
    while (dashes != 7 &&
	   **cpp != '\0' && **cpp != '\n') {
	if (**cpp == '-')
	    dashes++;
	(*cpp)++;
	(*len)++;
    }
    while (**cpp != '\0' && **cpp != '\n') {
	(*cpp)++;
    }
    if (dashes != 7)
	return NULL;
    return ret;
}

/* Scan one resource and build font map records. */
private void
scan_font_resource(const char *resource, x11fontmap ** pmaps)
{
    const char *ps_name;
    const char *x11_name;
    int ps_name_len;
    int x11_name_len;
    x11fontmap *font;
    const char *cp = resource;

    while ((ps_name = get_ps_name(&cp, &ps_name_len)) != 0) {
	x11_name = get_x11_name(&cp, &x11_name_len);
	if (x11_name) {
	    font = (x11fontmap *) gs_malloc(sizeof(x11fontmap), 1,
					    "x11_setup_fontmap");
	    if (font == NULL)
		continue;
	    font->ps_name = (char *)gs_malloc(sizeof(char), ps_name_len + 1,
					      "x11_setup_fontmap");

	    if (font->ps_name == NULL) {
		gs_free((char *)font, sizeof(x11fontmap), 1, "x11_fontmap");
		continue;
	    }
	    strncpy(font->ps_name, ps_name, ps_name_len);
	    font->ps_name[ps_name_len] = '\0';
	    font->x11_name = (char *)gs_malloc(sizeof(char), x11_name_len,
					       "x11_setup_fontmap");

	    if (font->x11_name == NULL) {
		gs_free(font->ps_name, sizeof(char), strlen(font->ps_name) + 1,
			"x11_font_psname");

		gs_free((char *)font, sizeof(x11fontmap), 1, "x11_fontmap");
		continue;
	    }
	    strncpy(font->x11_name, x11_name, x11_name_len - 1);
	    font->x11_name[x11_name_len - 1] = '\0';
	    font->std.names = NULL;
	    font->std.count = -1;
	    font->iso.names = NULL;
	    font->iso.count = -1;
	    font->next = *pmaps;
	    *pmaps = font;
	}
    }
}

/* Scan all the font resources and set up the maps. */
private void
gdev_x_setup_fontmap(gx_device_X * xdev)
{
    /*
     * If this device is a copy of another one, the *_fonts lists
     * might be dangling references.  Clear them before scanning.
     */
    xdev->regular_fonts = 0;
    xdev->symbol_fonts = 0;
    xdev->dingbat_fonts = 0;

    if (!xdev->useXFonts)
	return;			/* If no external fonts, don't bother */

    scan_font_resource(xdev->regularFonts, &xdev->regular_fonts);
    scan_font_resource(xdev->symbolFonts, &xdev->symbol_fonts);
    scan_font_resource(xdev->dingbatFonts, &xdev->dingbat_fonts);
}
