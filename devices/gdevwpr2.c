/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/*
 * Microsoft Windows 3.n printer driver for Ghostscript.
 * Original version by Russell Lang and
 * L. Peter Deutsch, Aladdin Enterprises.
 * Modified by rjl 1995-03-29 to use BMP printer code
 * Modified by Pierre Arnaud 1999-02-18 (see description below)
 * Modified by lpd 1999-04-03 for compatibility with Borland C++ 4.5.
 * Modified by Pierre Arnaud 1999-10-03 (accept b&w printing on color printers).
 * Modified by Pierre Arnaud 1999-11-20 (accept lower resolution)
 * Bug fixed by Pierre Arnaud 2000-03-09 (win_pr2_put_params error when is_open)
 * Bug fixed by Pierre Arnaud 2000-03-20 (win_pr2_set_bpp did not set anti_alias)
 * Bug fixed by Pierre Arnaud 2000-03-22 (win_pr2_set_bpp depth was wrong)
 * Modified by Pierre Arnaud 2000-12-12 (mainly added support for Tumble)
 * Bug fixed by Pierre Arnaud 2000-12-18 (-dQueryUser now works from cmd line)
 */

/* This driver uses the printer default size and resolution and
 * ignores page size and resolution set using -gWIDTHxHEIGHT and
 * -rXxY.  You must still set the correct PageSize to get the
 * correct clipping path.
 * The code in win_pr2_getdc() does try to set the printer page
 * size from the PostScript PageSize, but it isn't working
 * reliably at the moment.
 *
 * This driver doesn't work with some Windows printer drivers.
 * The reason is unknown.  All printers to which I have access
 * work.
 *
 * rjl 1997-11-20
 */

/* Additions by Pierre Arnaud (Pierre.Arnaud@opac.ch)
 *
 * The driver has been extended in order to provide some run-time
 * feed-back about the default Windows printer and to give the user
 * the opportunity to select the printer's properties before the
 * device gets opened (and any spooling starts).
 *
 * The driver returns an additional property named "UserSettings".
 * This is a dictionary which contents are valid only after setting
 * the QueryUser property (see below). The UserSettings dict contains
 * the following keys:
 *
 *  DocumentRange  [begin end]		(int array, can be set)
 *	Defines the range of pages in the document; [1 10] would
 *	describe a document starting at page 1 and ending at page 10.
 *
 *  SelectedRange  [begin end]		(int array, can be set)
 *	Defines the pages the user wants to print.
 *
 *  MediaSize	   [width height]	(float array, read only)
 *	Current printer's media size.
 *
 *  Copies	   n			(integer, can be set)
 *	User selected number of copies.
 *
 *  PrintCopies    n			(integer, read only)
 *	Number of copies which must be printed by Ghostscript itself.
 *	This is still experimental.
 *
 *  DocumentName   name			(string, can be set)
 *	Name to be associated with the print job.
 *
 *  UserChangedSettings			(bool, read only)
 *	Set to 'true' if the last QueryUser operation succeeded.
 *
 *  Paper	   n			(integer, can be set)
 *	Windows paper selection (0 = automatic).
 *
 *  Orient	   n			(integer, can be set)
 *	Windows paper orientation (0 = automatic).
 *
 *  Color	   n			(integer, can be set)
 *	Windows color (0 = automatic, 1 = monochrome, 2 = color).
 *
 *  MaxResolution  n			(integer, can be set)
 *	Maximum resolution in pixels pet inch (0 = no maximum). If
 *	the printer has a higher resolution than the maximum, trim
 *	the used resolution to the best one (dpi_chosen <= dpi_max,
 *	with dpi_chosen = dpi_printer / ratio).
 */

/* Supported printer parameters are :
 *
 *  -dBitsPerPixel=n
 *     Override what the Window printer driver returns.
 *
 *  -dNoCancel
 *     Don't display cancel dialog box.  Useful for unattended or
 *     console EXE operation.
 *
 *  -dQueryUser=n
 *     Query user interactively for the destination printer, before
 *     the device gets opened. This fills in the UserSettings dict
 *     and the OutputFile name properties. The following values are
 *     supported for n:
 *     1 => show standard Print dialog
 *     2 => show Print Setup dialog instead
 *     3 => select default printer
 *     other, does nothing
 *
 * The /Duplex & /Tumble keys of the setpagedevice dict are supported
 * if the Windows printer supports duplex printing.
 */

#include "windows_.h"
#include "gdevprn.h"
#include "gdevpccm.h"
#include "string_.h"
#include <shellapi.h>
#include "gp_mswin.h"

#include "gp.h"
#include "gpcheck.h"
#include "commdlg.h"
#include "gsicc_manage.h"

/* Make sure we cast to the correct structure type. */
typedef struct gx_device_win_pr2_s gx_device_win_pr2;

/* Device procedures */

/* See gxdevice.h for the definitions of the procedures. */
static dev_proc_open_device(win_pr2_open);
static dev_proc_close_device(win_pr2_close);
static dev_proc_print_page(win_pr2_print_page);
static dev_proc_map_rgb_color(win_pr2_map_rgb_color);
static dev_proc_map_color_rgb(win_pr2_map_color_rgb);
static dev_proc_get_params(win_pr2_get_params);
static dev_proc_put_params(win_pr2_put_params);

static int win_pr2_set_bpp(gx_device * dev, int depth);

static void
win_pr2_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, open_device, win_pr2_open);
    set_dev_proc(dev, close_device, win_pr2_close);
    set_dev_proc(dev, map_rgb_color, win_pr2_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, win_pr2_map_color_rgb);
    set_dev_proc(dev, get_params, win_pr2_get_params);
    set_dev_proc(dev, put_params, win_pr2_put_params);
    set_dev_proc(dev, encode_color, win_pr2_map_rgb_color);
    set_dev_proc(dev, decode_color, win_pr2_map_color_rgb);
}

#define PARENT_WINDOW  HWND_DESKTOP
BOOL CALLBACK CancelDlgProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK AbortProc2(HDC, int);

/* The device descriptor */
typedef struct gx_device_win_pr2_s gx_device_win_pr2;
struct gx_device_win_pr2_s {
    gx_device_common;
    gx_prn_device_common;
    HDC hdcprn;
    bool nocancel;

    int doc_page_begin;		/* first page number in document */
    int doc_page_end;		/* last page number in document */
    int user_page_begin;	/* user's choice: first page to print */
    int user_page_end;		/* user's choice: last page to print */
    int user_copies;		/* user's choice: number of copies */
    int print_copies;		/* number of times GS should print each page */
    float user_media_size[2];	/* width/height of media selected by user */
    char doc_name[200];		/* name of document for the spooler */
    char paper_name[64];	/* name of selected paper format */
    bool user_icc;              /* User specified device icc profile */
    bool user_changed_settings;	/* true if user validated dialog */
    int user_paper;		/* user's choice: paper format */
    int user_orient;		/* user's choice: paper orientation */
    int user_color;		/* user's choice: color format */
    int max_dpi;		/* maximum resolution in DPI */
    int ratio;			/* stretch ratio when printing */
    int selected_bpp;		/* selected bpp, memorised by win_pr2_set_bpp */
    bool tumble;		/* tumble setting (with duplex) */
    int query_user;		/* query user (-dQueryUser) */
    int TextAlpha;
    int GraphicsAlpha;

    HANDLE win32_hdevmode;	/* handle to device mode information */
    HANDLE win32_hdevnames;	/* handle to device names information */

    DLGPROC lpfnAbortProc;
    DLGPROC lpfnCancelProc;
    HWND hDlgModeless;

    bool use_old_spool_name;	/* user prefers old \\spool\ name */
    gx_device_win_pr2* original_device;	/* used to detect copies */
};

gx_device_win_pr2 far_data gs_mswinpr2_device =
{
    prn_device_std_body(gx_device_win_pr2,
                        win_pr2_initialize_device_procs,
                        "mswinpr2",
                        DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                        72.0, 72.0,
                        0, 0, 0, 0,
                        0, win_pr2_print_page),		/* depth = 0 */
    0,				/* hdcprn */
    0,				/* nocancel */
    0,				/* doc_page_begin */
    0,				/* doc_page_end */
    0,				/* user_page_begin */
    0,				/* user_page_end */
    1,				/* user_copies */
    1,				/* print_copies */
    { 0.0, 0.0 },		/* user_media_size */
    { 0 },			/* doc_name */
    { 0 },			/* paper_name */
    false,                      /* user_icc */
    0,				/* user_changed_settings */
    0,				/* user_paper */
    0,				/* user_orient */
    0,				/* user_color */
    0,				/* max_dpi */
    0,				/* ratio */
    0,				/* selected_bpp */
    false,			/* tumble */
    -1,				/* query_user */
    1,
    1,
    NULL,			/* win32_hdevmode */
    NULL,			/* win32_hdevnames */
    NULL,			/* lpfnAbortProc */
    NULL,			/* lpfnCancelProc */
    NULL,			/* hDlgModeless */
    false,			/* use_old_spool_name */
    NULL			/* original_device */
};

/********************************************************************************/

static int win_pr2_getdc(gx_device_win_pr2 * dev);
static int win_pr2_update_dev(gx_device_win_pr2 * dev, LPDEVMODEW pdevmode);
static int win_pr2_update_win(gx_device_win_pr2 * dev, LPDEVMODEW pdevmode);
static int win_pr2_print_setup_interaction(gx_device_win_pr2 * dev, int mode);
static int win_pr2_write_user_settings(gx_device_win_pr2 * dev, gs_param_list * plist);
static int win_pr2_read_user_settings(gx_device_win_pr2 * dev, gs_param_list * plist);
static void win_pr2_copy_check(gx_device_win_pr2 * dev);

/********************************************************************************/

/* Open the win_pr2 driver */
static int
win_pr2_open(gx_device * dev)
{
    int code, code1;
    int depth;
    PRINTDLGW pd;
    POINT offset;
    POINT size;
    float m[4];
    gp_file *pfile;
    DOCINFO docinfo;
    float ratio = 1.0;
    gx_device_win_pr2 *wdev = (gx_device_win_pr2 *)dev;

    win_pr2_copy_check(wdev);

    /* get a HDC for the printer */
    if ((wdev->win32_hdevmode) &&
        (wdev->win32_hdevnames)) {
        /* The user has already had the opportunity to choose the output */
        /* file interactively. Just use the specified parameters. */

        LPDEVMODEW devmode = (LPDEVMODEW) GlobalLock(wdev->win32_hdevmode);
        LPDEVNAMES devnames = (LPDEVNAMES) GlobalLock(wdev->win32_hdevnames);

        const WCHAR* driver = (WCHAR*)(devnames)+(devnames->wDriverOffset);
        const WCHAR* device = (WCHAR*)(devnames)+(devnames->wDeviceOffset);
        const WCHAR* output = (WCHAR*)(devnames)+(devnames->wOutputOffset);

        wdev->hdcprn = CreateDCW(driver, device, output, devmode);

        GlobalUnlock(wdev->win32_hdevmode);
        GlobalUnlock(wdev->win32_hdevnames);

        if (wdev->hdcprn == NULL) {
	  return_error(gs_error_Fatal);
        }

    } else if (!win_pr2_getdc(wdev)) {
        /* couldn't get a printer from -sOutputFile= */
        /* Prompt with dialog box */

        LPDEVMODEW devmode = NULL;
        memset(&pd, 0, sizeof(pd));

        pd.lStructSize = sizeof(pd);
        pd.hwndOwner = PARENT_WINDOW;
        pd.Flags = PD_RETURNDC;
        pd.nMinPage = wdev->doc_page_begin;
        pd.nMaxPage = wdev->doc_page_end;
        pd.nFromPage = wdev->user_page_begin;
        pd.nToPage = wdev->user_page_end;
        pd.nCopies = wdev->user_copies;

        if (!PrintDlgW(&pd)) {
            /* device not opened - exit ghostscript */
	  return_error(gs_error_Fatal);	/* exit Ghostscript cleanly */
        }

        devmode = GlobalLock(pd.hDevMode);
        win_pr2_update_dev(wdev,devmode);
        GlobalUnlock(pd.hDevMode);

        if (wdev->win32_hdevmode)
            GlobalFree(wdev->win32_hdevmode);
        if (wdev->win32_hdevnames)
            GlobalFree(wdev->win32_hdevnames);

        wdev->hdcprn = pd.hDC;
        wdev->win32_hdevmode = pd.hDevMode;
        wdev->win32_hdevnames = pd.hDevNames;

        pd.hDevMode = NULL;
        pd.hDevNames = NULL;
    }
    if (!(GetDeviceCaps(wdev->hdcprn, RASTERCAPS) != RC_DIBTODEV)) {
        errprintf(dev->memory, "Windows printer does not have RC_DIBTODEV\n");
        DeleteDC(wdev->hdcprn);
        return_error(gs_error_limitcheck);
    }
    /* initialise printer, install abort proc */
    wdev->lpfnAbortProc = (DLGPROC) AbortProc2;
    SetAbortProc(wdev->hdcprn, (ABORTPROC) wdev->lpfnAbortProc);

    /*
     * Some versions of the Windows headers include lpszDatatype and fwType,
     * and some don't.  Since we want to set these fields to zero anyway,
     * we just start by zeroing the whole structure.
     */
    memset(&docinfo, 0, sizeof(docinfo));
    docinfo.cbSize = sizeof(docinfo);
    docinfo.lpszDocName = wdev->doc_name;
    /*docinfo.lpszOutput = NULL;*/
    /*docinfo.lpszDatatype = NULL;*/
    /*docinfo.fwType = 0;*/

    if (docinfo.lpszDocName[0] == 0) {
        docinfo.lpszDocName = "Ghostscript output";
    }

    if (StartDoc(wdev->hdcprn, &docinfo) <= 0) {
        errprintf(dev->memory,
                  "Printer StartDoc failed (error %08x)\n", GetLastError());
        DeleteDC(wdev->hdcprn);
        return_error(gs_error_limitcheck);
    }

    dev->x_pixels_per_inch = (float)GetDeviceCaps(wdev->hdcprn, LOGPIXELSX);
    dev->y_pixels_per_inch = (float)GetDeviceCaps(wdev->hdcprn, LOGPIXELSY);

    wdev->ratio = 1;

    if (wdev->max_dpi > 50) {

        float dpi_x = dev->x_pixels_per_inch;
        float dpi_y = dev->y_pixels_per_inch;

        while ((dev->x_pixels_per_inch > wdev->max_dpi)
            || (dev->y_pixels_per_inch > wdev->max_dpi)) {
            ratio += 1.0;
            wdev->ratio ++;
            dev->x_pixels_per_inch = dpi_x / ratio;
            dev->y_pixels_per_inch = dpi_y / ratio;
        }
    }

    size.x = GetDeviceCaps(wdev->hdcprn, PHYSICALWIDTH);
    size.y = GetDeviceCaps(wdev->hdcprn, PHYSICALHEIGHT);
    gx_device_set_width_height(dev, (int)(size.x / ratio), (int)(size.y / ratio));
    offset.x = GetDeviceCaps(wdev->hdcprn, PHYSICALOFFSETX);
    offset.y = GetDeviceCaps(wdev->hdcprn, PHYSICALOFFSETY);

    /* m[] gives margins in inches */
    m[0] /*left   */ = offset.x / dev->x_pixels_per_inch / ratio;
    m[3] /*top    */ = offset.y / dev->y_pixels_per_inch / ratio;
    m[2] /*right  */ = (size.x - offset.x - GetDeviceCaps(wdev->hdcprn, HORZRES)) / dev->x_pixels_per_inch / ratio;
    m[1] /*bottom */ = (size.y - offset.y - GetDeviceCaps(wdev->hdcprn, VERTRES)) / dev->y_pixels_per_inch / ratio;
    gx_device_set_margins(dev, m, true);

    depth = dev->color_info.depth;
    if (depth == 0) {
        /* Set parameters that were unknown before opening device */
        /* Find out if the device supports color */
        /* We recognize 1, 4 (but use only 3), 8 and 24 bit color devices */
        depth = GetDeviceCaps(wdev->hdcprn, PLANES) * GetDeviceCaps(wdev->hdcprn, BITSPIXEL);
    }
    code1 = win_pr2_set_bpp(dev, depth);

    /* Previous comment was misleading and has been removed. This is an ugly hack!
     * We don't get to the mswinpr2 'print_page' method until we've been through
     * gdev_prn_output_page, which (in gdev_prn_output_page_aux) calls
     * gdev_prn_open_printer_seekable(), which insists on having a filename and
     * opening that file, even though we don't use it. If we don't have a valid
     * filename we can use in that function then we get an error.....
     *
     * So this code simply creates a dummy temporary file which we never use,
     * just to get around that problem.....
     */
    wdev->fname[0] = '\0';
    pfile = gp_open_scratch_file(dev->memory,
                                 gp_scratch_file_name_prefix,
                                 wdev->fname, "wb");
    gp_fclose(pfile);
    code = gdev_prn_open(dev);

    /* If we subclassed the device, with a FirstPage LastPage device,
     * update the stored pointer copy here, if we don't then this whole
     * device stops working, not sure why.
     */
    if (dev->child) {
        gx_device_win_pr2 *windev;

        while (dev->child)
            dev = dev->child;

        windev = (gx_device_win_pr2 *)dev;

        windev->original_device = (gx_device_win_pr2 *)dev;
    }

    if ((code < 0) && wdev->fname[0])
        gp_unlink(wdev->memory, wdev->fname);

    if (!wdev->nocancel) {
        /* inform user of progress with dialog box and allow cancel */
        wdev->lpfnCancelProc = (DLGPROC) CancelDlgProc;
        wdev->hDlgModeless = CreateDialog(phInstance, "CancelDlgBox",
                                    PARENT_WINDOW, wdev->lpfnCancelProc);
        ShowWindow(wdev->hDlgModeless, SW_HIDE);
    }
    if (code1 < 0 && code >= 0) {
        code = code1;
    }

    return code;
};

/* Close the win_pr2 driver */
static int
win_pr2_close(gx_device * dev)
{
    int code;
    int aborted = FALSE;
    gx_device_win_pr2 *wdev = (gx_device_win_pr2 *)dev;

    win_pr2_copy_check(wdev);

    /* Free resources */

    if (!wdev->nocancel) {
        if (!wdev->hDlgModeless)
            aborted = TRUE;
        else
            DestroyWindow(wdev->hDlgModeless);
        wdev->hDlgModeless = 0;
    }
    if (aborted)
        AbortDoc(wdev->hdcprn);
    else
        EndDoc(wdev->hdcprn);

    DeleteDC(wdev->hdcprn);

    if (wdev->win32_hdevmode != NULL) {
        GlobalFree(wdev->win32_hdevmode);
        wdev->win32_hdevmode = NULL;
    }
    if (wdev->win32_hdevnames != NULL) {
        GlobalFree(wdev->win32_hdevnames);
        wdev->win32_hdevnames = NULL;
    }

    /* delete unwanted temporary file */
    if (wdev->fname[0])
        gp_unlink(wdev->memory, wdev->fname);

    code = gdev_prn_close(dev);

    return code;
}

/* ------ Internal routines ------ */

/********************************************************************************/

/* ------ Private definitions ------ */

/* new win_pr2_print_page routine */

/* Write BMP header to memory, then send bitmap to printer */
/* one scan line at a time */
static int
win_pr2_print_page(gx_device_printer * pdev, gp_file * file)
{
    int raster = gdev_prn_raster(pdev);

    /* BMP scan lines are padded to 32 bits. */
    ulong bmp_raster = raster + (-raster & 3);
    ulong bmp_raster_multi;
    int scan_lines, yslice, lines, i;
    int width;
    int depth = pdev->color_info.depth;
    byte *row;
    int y;
    int code = 0;		/* return code */
    MSG msg;
    char dlgtext[32];
    HGLOBAL hrow;
    int ratio = ((gx_device_win_pr2 *)pdev)->ratio;
    gx_device_win_pr2 *wdev = (gx_device_win_pr2 *)pdev;

    struct bmi_s {
        BITMAPINFOHEADER h;
        RGBQUAD pal[256];
    } bmi;

    scan_lines = dev_print_scan_lines(pdev);
    width = (int)(pdev->width - ((dev_l_margin(pdev) + dev_r_margin(pdev) -
                                  dev_x_offset(pdev)) * pdev->x_pixels_per_inch));

    yslice = 65535 / bmp_raster;	/* max lines in 64k */
    bmp_raster_multi = bmp_raster * yslice;
    hrow = GlobalAlloc(0, bmp_raster_multi);
    row = GlobalLock(hrow);
    if (row == 0)		/* can't allocate row buffer */
        return_error(gs_error_VMerror);

    /* Write the info header. */

    bmi.h.biSize = sizeof(bmi.h);
    bmi.h.biWidth = pdev->width;	/* wdev->mdev.width; */
    bmi.h.biHeight = yslice;
    bmi.h.biPlanes = 1;
    bmi.h.biBitCount = pdev->color_info.depth;
    bmi.h.biCompression = 0;
    bmi.h.biSizeImage = 0;	/* default */
    bmi.h.biXPelsPerMeter = 0;	/* default */
    bmi.h.biYPelsPerMeter = 0;	/* default */

    StartPage(wdev->hdcprn);

    /* Write the palette. */

    if (depth <= 8) {
        int i;
        gx_color_value rgb[3];
        LPRGBQUAD pq;

        bmi.h.biClrUsed = 1 << depth;
        bmi.h.biClrImportant = 1 << depth;
        for (i = 0; i != 1 << depth; i++) {
            (*dev_proc(pdev, map_color_rgb)) ((gx_device *) pdev,
                                              (gx_color_index) i, rgb);
            pq = &bmi.pal[i];
            pq->rgbRed = gx_color_value_to_byte(rgb[0]);
            pq->rgbGreen = gx_color_value_to_byte(rgb[1]);
            pq->rgbBlue = gx_color_value_to_byte(rgb[2]);
            pq->rgbReserved = 0;
        }
    } else {
        bmi.h.biClrUsed = 0;
        bmi.h.biClrImportant = 0;
    }

    if (!wdev->nocancel) {
        gs_snprintf(dlgtext, sizeof(dlgtext), "Printing page %d", (int)(pdev->PageCount) + 1);
        SetWindowText(GetDlgItem(wdev->hDlgModeless, CANCEL_PRINTING), dlgtext);
        ShowWindow(wdev->hDlgModeless, SW_SHOW);
    }
    for (y = 0; y < scan_lines;) {
        /* copy slice to row buffer */
        if (y > scan_lines - yslice)
            lines = scan_lines - y;
        else
            lines = yslice;
        for (i = 0; i < lines; i++) {
            code = gdev_prn_copy_scan_lines(pdev, y + i,
                              row + (bmp_raster * (lines - 1 - i)), raster);
            if (code < 0)
                goto xit;
        }

        if (ratio > 1) {
            StretchDIBits(wdev->hdcprn, 0, y*ratio, pdev->width*ratio, lines*ratio,
                          0, 0, pdev->width, lines,
                          row,
                          (BITMAPINFO FAR *) & bmi, DIB_RGB_COLORS, SRCCOPY);
        } else {
            SetDIBitsToDevice(wdev->hdcprn, 0, y, pdev->width, lines,
                              0, 0, 0, lines,
                              row,
                              (BITMAPINFO FAR *) & bmi, DIB_RGB_COLORS);
        }
        y += lines;

        if (!wdev->nocancel) {
            /* inform user of progress */
            gs_snprintf(dlgtext, sizeof(dlgtext), "%d%% done", (int)(y * 100L / scan_lines));
            SetWindowText(GetDlgItem(wdev->hDlgModeless, CANCEL_PCDONE), dlgtext);
        }
        /* process message loop */
        while (PeekMessage(&msg, wdev->hDlgModeless, 0, 0, PM_REMOVE)) {
            if ((wdev->hDlgModeless == 0) || !IsDialogMessage(wdev->hDlgModeless, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        if ((!wdev->nocancel) && (wdev->hDlgModeless == 0)) {
            /* user pressed cancel button */
            break;
        }
    }

    if ((!wdev->nocancel) && (wdev->hDlgModeless == 0))
        code = gs_error_Fatal;	/* exit Ghostscript cleanly */
    else {
        /* push out the page */
        if (!wdev->nocancel)
            SetWindowText(GetDlgItem(wdev->hDlgModeless, CANCEL_PCDONE),
                          "Ejecting page...");
        EndPage(wdev->hdcprn);
        if (!wdev->nocancel)
            ShowWindow(wdev->hDlgModeless, SW_HIDE);
    }

xit:
    GlobalUnlock(hrow);
    GlobalFree(hrow);

    return code;
}

/* combined color mappers */

/* 24-bit color mappers (taken from gdevmem2.c). */
/* Note that Windows expects RGB values in the order B,G,R. */

/* Map a r-g-b color to a color index. */
static gx_color_index
win_pr2_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{
    gx_color_value r = cv[0];
    gx_color_value g = cv[1];
    gx_color_value b = cv[2];
    switch (dev->color_info.depth) {
        case 1:
            return gdev_prn_map_rgb_color(dev, cv);
        case 4:
            /* use only 8 colors */
            return (r > (gx_max_color_value / 2 + 1) ? 4 : 0) +
                (g > (gx_max_color_value / 2 + 1) ? 2 : 0) +
                (b > (gx_max_color_value / 2 + 1) ? 1 : 0);
        case 8:
            return pc_8bit_map_rgb_color(dev, cv);
        case 24:
            return gx_color_value_to_byte(r) +
                ((uint) gx_color_value_to_byte(g) << 8) +
                ((ulong) gx_color_value_to_byte(b) << 16);
    }
    return 0;			/* error */
}

/* Map a color index to a r-g-b color. */
static int
win_pr2_map_color_rgb(gx_device * dev, gx_color_index color,
                      gx_color_value prgb[3])
{
    switch (dev->color_info.depth) {
        case 1:
            gdev_prn_map_color_rgb(dev, color, prgb);
            break;
        case 4:
            /* use only 8 colors */
            prgb[0] = (color & 4) ? gx_max_color_value : 0;
            prgb[1] = (color & 2) ? gx_max_color_value : 0;
            prgb[2] = (color & 1) ? gx_max_color_value : 0;
            break;
        case 8:
            pc_8bit_map_color_rgb(dev, color, prgb);
            break;
        case 24:
            prgb[2] = gx_color_value_from_byte(color >> 16);
            prgb[1] = gx_color_value_from_byte((color >> 8) & 0xff);
            prgb[0] = gx_color_value_from_byte(color & 0xff);
            break;
    }
    return 0;
}

static int
param_normalize_anti_alias_bits( uint max_gray, int bits )
{
        int	max_bits = ilog2( max_gray + 1);

        return  (bits > max_bits ? max_bits : bits);
}

static int
win_pr2_set_bpp(gx_device * dev, int depth)
{
    int code = 0;
    gx_device_printer *pdev = (gx_device_printer *)dev;
    gx_device_win_pr2 *wdev = (gx_device_win_pr2 *)pdev;

    if (depth > 8) {
        static const gx_device_color_info win_pr2_24color = dci_std_color(24);

        dev->color_info = win_pr2_24color;
        depth = 24;

        if (!wdev->user_icc && (!wdev->icc_struct || (wdev->icc_struct &&
             wdev->icc_struct->device_profile[gsDEFAULTPROFILE]->data_cs != gsRGB))) {

            if (wdev->icc_struct) {
                rc_decrement(wdev->icc_struct, "win_pr2_set_bpp");
            }
            wdev->icc_struct = gsicc_new_device_profile_array((gx_device *)wdev);

            if (wdev->icc_struct) {
                code = gsicc_set_device_profile(dev, dev->memory,
                  (char *)DEFAULT_RGB_ICC, gsDEFAULTPROFILE);
            }
            else {
                code = gs_error_VMerror;
            }
        }

    } else if (depth >= 8) {
        /* 8-bit (SuperVGA-style) color. */
        /* (Uses a fixed palette of 3,3,2 bits.) */
        static const gx_device_color_info win_pr2_8color = dci_pc_8bit;

        dev->color_info = win_pr2_8color;
        depth = 8;

        if (!wdev->user_icc && (!wdev->icc_struct || (wdev->icc_struct &&
             wdev->icc_struct->device_profile[gsDEFAULTPROFILE]->data_cs != gsRGB))) {

            if (wdev->icc_struct) {
                rc_decrement(wdev->icc_struct, "win_pr2_set_bpp");
            }
            wdev->icc_struct = gsicc_new_device_profile_array((gx_device *)wdev);

            if (wdev->icc_struct) {
                code = gsicc_set_device_profile(dev, dev->memory,
                  (char *)DEFAULT_RGB_ICC, gsDEFAULTPROFILE);
            }
            else {
                code = gs_error_VMerror;
            }
        }
    } else if (depth >= 3) {
        /* 3 plane printer */
        /* suitable for impact dot matrix CMYK printers */
        /* create 4-bit bitmap, but only use 8 colors */
        static const gx_device_color_info win_pr2_4color = dci_values(3, 4, 1, 1, 2, 2);

        dev->color_info = win_pr2_4color;
        depth = 4;

        if (!wdev->user_icc && (!wdev->icc_struct || (wdev->icc_struct &&
             wdev->icc_struct->device_profile[gsDEFAULTPROFILE]->data_cs != gsCMYK))) {

            if (wdev->icc_struct) {
                rc_decrement(wdev->icc_struct, "win_pr2_set_bpp");
            }
            wdev->icc_struct = gsicc_new_device_profile_array((gx_device *)wdev);

            if (wdev->icc_struct) {
                code = gsicc_set_device_profile(dev, dev->memory,
                  (char *)DEFAULT_CMYK_ICC, gsDEFAULTPROFILE);
            }
            else {
                code = gs_error_VMerror;
            }
        }
    } else {			/* default is black_and_white */
        static const gx_device_color_info win_pr2_1color = dci_std_color(1);

        dev->color_info = win_pr2_1color;
        depth = 1;

        if (!wdev->user_icc && (!wdev->icc_struct || (wdev->icc_struct &&
             wdev->icc_struct->device_profile[gsDEFAULTPROFILE]->data_cs != gsGRAY))) {

            if (wdev->icc_struct) {
                rc_decrement(wdev->icc_struct, "win_pr2_set_bpp");
            }
            wdev->icc_struct = gsicc_new_device_profile_array((gx_device *)wdev);

            if (wdev->icc_struct) {
                code = gsicc_set_device_profile(dev, dev->memory,
                  (char *)DEFAULT_GRAY_ICC, gsDEFAULTPROFILE);
            }
            else {
                code = gs_error_VMerror;
            }
        }
    }

    wdev->color_info.anti_alias.text_bits =
            param_normalize_anti_alias_bits(max(wdev->color_info.max_gray,
            wdev->color_info.max_color), wdev->TextAlpha);
    wdev->color_info.anti_alias.graphics_bits =
            param_normalize_anti_alias_bits(max(wdev->color_info.max_gray,
            wdev->color_info.max_color), wdev->GraphicsAlpha);
    wdev->selected_bpp = depth;

    /* copy encode/decode procedures */
    set_dev_proc(dev, encode_color, dev_proc(dev, map_rgb_color));
    set_dev_proc(dev, decode_color, dev_proc(dev, map_color_rgb));
    if (depth == 1) {
        set_dev_proc(dev, get_color_mapping_procs,
            gx_default_DevGray_get_color_mapping_procs);
        set_dev_proc(dev, get_color_comp_index,
            gx_default_DevGray_get_color_comp_index);
    }
    else {
        set_dev_proc(dev, get_color_mapping_procs,
            gx_default_DevRGB_get_color_mapping_procs);
        set_dev_proc(dev, get_color_comp_index,
            gx_default_DevRGB_get_color_comp_index);
    }
    return(code);
}

/********************************************************************************/

/* Get device parameters */
int
win_pr2_get_params(gx_device * pdev, gs_param_list * plist)
{
    int code = gdev_prn_get_params(pdev, plist);
    gx_device_win_pr2 *wdev = (gx_device_win_pr2 *)pdev;

    win_pr2_copy_check(wdev);

    if (code >= 0)
        code = param_write_bool(plist, "NoCancel",
                                &(wdev->nocancel));
    if (code >= 0)
        code = param_write_int(plist, "QueryUser",
                                &(wdev->query_user));
    if (code >= 0)
        code = win_pr2_write_user_settings(wdev, plist);

    if ((code >= 0) && (wdev->Duplex_set > 0))
        code = param_write_bool(plist, "Tumble",
                                &(wdev->tumble));

    return code;
}

/* We implement this ourselves so that we can change BitsPerPixel */
/* before the device is opened */
int
win_pr2_put_params(gx_device * pdev, gs_param_list * plist)
{
    int ecode = 0, code;
    int old_bpp = pdev->color_info.depth;
    int bpp = old_bpp, tab = 1, gab = 1;
    gx_device_win_pr2 *wdev = (gx_device_win_pr2 *)pdev;
    bool tumble   = wdev->tumble;
    bool nocancel = wdev->nocancel;
    int queryuser = 0;
    bool old_duplex = wdev->Duplex;
    bool old_tumble = wdev->tumble;
    int  old_orient = wdev->user_orient;
    int  old_color  = wdev->user_color;
    int  old_paper  = wdev->user_paper;
    int  old_mx_dpi = wdev->max_dpi;

    if (wdev->Duplex_set < 0) {
        wdev->Duplex_set = 0;
        wdev->Duplex = false;
        wdev->tumble = false;
    }

    win_pr2_copy_check(wdev);

    code = win_pr2_read_user_settings(wdev, plist);

    switch (code = param_read_int(plist, "BitsPerPixel", &bpp)) {
        case 0:
            if (pdev->is_open) {
                if (wdev->selected_bpp == bpp) {
                    break;
                }
                ecode = gs_error_rangecheck;
            } else {		/* change dev->color_info is valid before device is opened */
                code = win_pr2_set_bpp(pdev, bpp);
                if (code < 0) {
                    ecode = code;
                }
                break;
            }
            goto bppe;
        default:
            ecode = code;
          bppe:param_signal_error(plist, "BitsPerPixel", ecode);
        case 1:
            break;
    }

    switch (code = param_read_bool(plist, "NoCancel", &nocancel)) {
        case 0:
            if (pdev->is_open) {
                if (wdev->nocancel == nocancel) {
                    break;
                }
                ecode = gs_error_rangecheck;
            } else {
                wdev->nocancel = nocancel;
                break;
            }
            goto nocancele;
        default:
            ecode = code;
          nocancele:param_signal_error(plist, "NoCancel", ecode);
        case 1:
            break;
    }

    switch (code = param_read_bool(plist, "Tumble", &tumble)) {
        case 0:
            wdev->tumble = tumble;
            break;
        default:
            ecode = code;
            param_signal_error(plist, "Tumble", ecode);
        case 1:
            break;
    }

    switch (code = param_read_int(plist, "QueryUser", &queryuser)) {
        case 0:
            if ((queryuser > 0) &&
                (queryuser < 4)) {
                win_pr2_print_setup_interaction(wdev, queryuser);
            }
            break;
        default:
            ecode = code;
            param_signal_error(plist, "QueryUser", ecode);
        case 1:
            break;
    }

    switch (code = param_read_int(plist, "TextAlphaBits", &tab))
    {
        case 0:
            switch(tab) {
                case 1:
                case 2:
                case 4:
                    wdev->TextAlpha = tab;
                    break;
                default:
                    ecode = gs_error_rangecheck;
                    param_signal_error(plist, "TextAlphaBits", ecode);
                    break;
            }
            break;
        default:
            ecode = code;
            param_signal_error(plist, "TextAlphaBits", ecode);
        case 1:
            break;
    }

    switch (code = param_read_int(plist, "GraphicsAlphaBits", &gab))
    {
        case 0:
            switch(gab) {
                case 1:
                case 2:
                case 4:
                    wdev->GraphicsAlpha = gab;
                    break;
                default:
                    ecode = gs_error_rangecheck;
                    param_signal_error(plist, "GraphicsAlphaBits", ecode);
                    break;
            }
            break;
        default:
            ecode = code;
            param_signal_error(plist, "GraphicsAlphaBits", ecode);
        case 1:
            break;
    }

    /* Record if the user has specified a custom profile, so we don't replace it
       with a default when we change color space - see win_pr2_set_bpp()
       Here, we only want to know *if* we have a user specced profile, actually
       setting it will be handled by gdev_prn_put_params()
     */
    if (!wdev->user_icc) {
        gs_param_string icc_pro_dummy;

        wdev->user_icc = param_read_string(plist, "OutputICCProfile", &icc_pro_dummy) == 0;
    }

    if (ecode >= 0)
        ecode = gdev_prn_put_params(pdev, plist);

    if (wdev->win32_hdevmode && wdev->hdcprn) {
        if ( (old_duplex != wdev->Duplex)
          || (old_tumble != wdev->tumble)
          || (old_orient != wdev->user_orient)
          || (old_color  != wdev->user_color)
          || (old_paper  != wdev->user_paper)
          || (old_mx_dpi != wdev->max_dpi) ) {

            LPDEVMODEW pdevmode = GlobalLock(wdev->win32_hdevmode);

            if (pdevmode) {
                win_pr2_update_win(wdev, pdevmode);
                ResetDCW(wdev->hdcprn, pdevmode);
                GlobalUnlock(pdevmode);
            }
        }
    }

    return ecode;
}

/********************************************************************************/

/* Get Device Context for printer */
static int
win_pr2_getdc(gx_device_win_pr2 * wdev)
{
    char *device;
    char driverbuf[512], *dbuflast = NULL;
    char *driver;
    char *output;
    LPWSTR devcap;
    int devcapsize;
    int devmode_size;

    int i, n;
    POINT *pp;
    int paperindex;
    int paperwidth, paperheight;
    int orientation;
    int papersize;
    WCHAR papername[64];
    LPDEVMODEW podevmode = NULL, pidevmode = NULL;
    HANDLE hprinter = NULL;

    /* fall back to prompting user */
    int result = FALSE;

    WCHAR* unidev = NULL;
    WCHAR* unidriver = NULL;
    WCHAR* unioutput = NULL;

    /* first try to derive the printer name from -sOutputFile= */
    /* is printer if name prefixed by \\spool\ or by %printer% */
    if (is_spool(wdev->fname)) {
        device = wdev->fname + 8;	/* skip over \\spool\ */
        wdev->use_old_spool_name = true;
    } else if (strncmp("%printer%",wdev->fname,9) == 0) {
        device = wdev->fname + 9;	/* skip over %printer% */
        wdev->use_old_spool_name = false;
    } else {
        goto cleanup;
    }

    /* now try to match the printer name against the [Devices] section */
    {
        wchar_t unidrvbuf[sizeof(driverbuf)];
        wchar_t *devices;
        wchar_t *p;
        int devices_size = 128, returned_length = 0;
        unidev = malloc(gp_utf8_to_uint16(NULL, device)*sizeof(wchar_t));
        if (unidev == NULL)
            goto cleanup;
        gp_utf8_to_uint16(unidev, device);
        do {
            devices = gs_malloc(wdev->memory, devices_size, 1, "win_pr2_getdc");
            if (devices == (wchar_t *)NULL) {
                goto cleanup;
            }
            returned_length = GetProfileStringW(L"Devices", NULL, L"", devices, devices_size / sizeof(wchar_t));
            returned_length *= sizeof(wchar_t);
            if (returned_length >= devices_size - 2 * sizeof(wchar_t)) {
                gs_free(wdev->memory, devices, devices_size, 1, "win_pr2_getdc");
                devices_size += 4096;
            }
            else
                break;
        } while (1);
        p = devices;
        while (*p) {
            if (wcsicmp(p, unidev) == 0)
                break;
            p += wcslen(p) + 1;
        }
        if (*p == '\0')
            p = NULL;
        gs_free(wdev->memory, devices, devices_size, 1, "win_pr2_getdc");
        if (p == NULL) {
            goto cleanup;  /* doesn't match an available printer */
        }

        /* the printer exists, get the remaining information from win.ini */
        GetProfileStringW(L"Devices", unidev, L"", unidrvbuf, sizeof(unidrvbuf) / sizeof(unidrvbuf[0]));
        i = gp_uint16_to_utf8(NULL, unidrvbuf);
        if (i < 0 || i > sizeof(driverbuf))
            goto cleanup;
        gp_uint16_to_utf8(driverbuf, unidrvbuf);
    }
    driver = gs_strtok(driverbuf, ",", &dbuflast);
    output = gs_strtok(NULL, ",", &dbuflast);

    unidriver = malloc(gp_utf8_to_uint16(NULL, driver) * sizeof(wchar_t));
    if (unidriver == NULL)
        goto cleanup;
    gp_utf8_to_uint16(unidriver, driver);

    unioutput = malloc(gp_utf8_to_uint16(NULL, output) * sizeof(wchar_t));
    if (unioutput == NULL)
        goto cleanup;
    gp_utf8_to_uint16(unioutput, output);

    if (!gp_OpenPrinter(device, &hprinter))
        goto cleanup;
    devmode_size = DocumentPropertiesW(NULL, hprinter, unidev, NULL, NULL, 0);
    if ((podevmode = gs_malloc(wdev->memory, devmode_size, 1, "win_pr2_getdc"))
        == (LPDEVMODEW) NULL) {
        goto cleanup;
    }
    if ((pidevmode = gs_malloc(wdev->memory, devmode_size, 1, "win_pr2_getdc"))		== (LPDEVMODEW) NULL) {
        goto cleanup;
    }
    DocumentPropertiesW(NULL, hprinter, unidev, podevmode, NULL, DM_OUT_BUFFER);

    /* now find out what paper sizes are available */
    devcapsize = DeviceCapabilitiesW(unidev, unioutput, DC_PAPERSIZE, NULL, NULL);
    devcapsize *= sizeof(POINT);
    if ((devcap = gs_malloc(wdev->memory, devcapsize, 1, "win_pr2_getdc")) == (LPWSTR) NULL)
        goto cleanup;
    n = DeviceCapabilitiesW(unidev, unioutput, DC_PAPERSIZE, devcap, NULL);
    paperwidth = (int)(wdev->MediaSize[0] * 254 / 72);
    paperheight = (int)(wdev->MediaSize[1] * 254 / 72);
    papername[0] = L'\0';
    papersize = 0;
    paperindex = -1;
    orientation = 0;
    pp = (POINT *) devcap;
    for (i = 0; i < n; i++, pp++) {
        if ((pp->x < paperwidth + 20) && (pp->x > paperwidth - 20) &&
            (pp->y < paperheight + 20) && (pp->y > paperheight - 20)) {
            paperindex = i;
            paperwidth = pp->x;
            paperheight = pp->y;
            orientation = DMORIENT_PORTRAIT;
            break;
        }
    }
    if (paperindex < 0) {
        /* try again in landscape */
        pp = (POINT *) devcap;
        for (i = 0; i < n; i++, pp++) {
            if ((pp->x < paperheight + 20) && (pp->x > paperheight - 20) &&
                (pp->y < paperwidth + 20) && (pp->y > paperwidth - 20)) {
                paperindex = i;
                paperwidth = pp->x;
                paperheight = pp->y;
                orientation = DMORIENT_LANDSCAPE;
                break;
            }
        }
    }
    gs_free(wdev->memory, devcap, devcapsize, 1, "win_pr2_getdc");

    /* get the dmPaperSize */
    devcapsize = DeviceCapabilitiesW(unidev, unioutput, DC_PAPERS, NULL, NULL);
    devcapsize *= sizeof(WORD);
    if ((devcap = gs_malloc(wdev->memory, devcapsize, 1, "win_pr2_getdc")) == (LPWSTR) NULL)
        goto cleanup;
    n = DeviceCapabilitiesW(unidev, unioutput, DC_PAPERS, devcap, NULL);
    if ((paperindex >= 0) && (paperindex < n))
        papersize = ((WORD *) devcap)[paperindex];
    gs_free(wdev->memory, devcap, devcapsize, 1, "win_pr2_getdc");

    /* get the paper name */
    devcapsize = DeviceCapabilitiesW(unidev, unioutput, DC_PAPERNAMES, NULL, NULL);
    devcapsize *= 64 * sizeof(WCHAR);
    if ((devcap = gs_malloc(wdev->memory, devcapsize, 1, "win_pr2_getdc")) == (LPWSTR) NULL)
        goto cleanup;
    n = DeviceCapabilitiesW(unidev, unioutput, DC_PAPERNAMES, devcap, NULL);
    if ((paperindex >= 0) && (paperindex < n))
        wcscpy(papername, devcap + paperindex * 64);
    gs_free(wdev->memory, devcap, devcapsize, 1, "win_pr2_getdc");

    memcpy(pidevmode, podevmode, devmode_size);

    pidevmode->dmFields = 0;

    wdev->paper_name[0] = 0;

    if ( (wdev->user_paper)
      && (wdev->user_paper != papersize) ) {
        papersize = wdev->user_paper;
        paperheight = 0;
        paperwidth = 0;
        papername[0] = 0;
    }
    if (wdev->user_orient) {
        orientation = wdev->user_orient;
    }

    pidevmode->dmFields &= ~(DM_PAPERSIZE | DM_ORIENTATION | DM_COLOR | DM_PAPERLENGTH | DM_PAPERWIDTH | DM_DUPLEX);
    pidevmode->dmFields |= DM_DEFAULTSOURCE;
    pidevmode->dmDefaultSource = 0;

    if (orientation) {
        wdev->user_orient = orientation;
    }
    if (papersize) {
        wdev->user_paper = papersize;
        i = gp_uint16_to_utf8(NULL, papername);
        if (i < 0 || i > sizeof(wdev->paper_name))
            goto cleanup;
        gp_uint16_to_utf8(wdev->paper_name, papername);
    }

    if (paperheight && paperwidth) {
        pidevmode->dmFields |= (DM_PAPERLENGTH | DM_PAPERWIDTH);
        pidevmode->dmPaperWidth = paperwidth;
        pidevmode->dmPaperLength = paperheight;
        wdev->user_media_size[0] = paperwidth / 254.0 * 72.0;
        wdev->user_media_size[1] = paperheight / 254.0 * 72.0;
    }

    if (DeviceCapabilitiesW(unidev, unioutput, DC_DUPLEX, NULL, NULL) > 0) {
        wdev->Duplex_set = 1;
    }

    win_pr2_update_win(wdev, pidevmode);

    /* merge the entries */
    DocumentPropertiesW(NULL, hprinter, unidev, podevmode, pidevmode, DM_IN_BUFFER | DM_OUT_BUFFER);
    ClosePrinter(hprinter);
    hprinter = NULL;

    /* now get a DC */
    wdev->hdcprn = CreateDCW(unidriver, unidev, NULL, podevmode);

    if (wdev->win32_hdevmode == NULL)
        wdev->win32_hdevmode = GlobalAlloc(0, devmode_size);

    if (wdev->win32_hdevmode) {
        LPDEVMODEW pdevmode = (LPDEVMODEW) GlobalLock(wdev->win32_hdevmode);
        if (pdevmode) {
            memcpy(pdevmode, podevmode, devmode_size);
            GlobalUnlock(wdev->win32_hdevmode);
        }
    }

    if (wdev->hdcprn != (HDC) NULL)
        result = TRUE;		/* success */

  cleanup:
    if (pidevmode) gs_free(wdev->memory, pidevmode, devmode_size, 1, "win_pr2_getdc");
    if (podevmode) gs_free(wdev->memory, podevmode, devmode_size, 1, "win_pr2_getdc");

    if (unidev) free(unidev);
    if (unidriver) free(unidriver);
    if (unioutput) free(unioutput);

    if (hprinter) ClosePrinter(hprinter);
    return result;
}

/*
 *  Minimalist update of the wdev parameters (mainly for the
 *  UserSettings parameters).
 */

static int
win_pr2_update_dev(gx_device_win_pr2 * dev, LPDEVMODEW pdevmode)
{
    if (pdevmode == 0)
        return FALSE;

    if (pdevmode->dmFields & DM_COLOR) {
        dev->user_color = pdevmode->dmColor;
    }
    if (pdevmode->dmFields & DM_ORIENTATION) {
        dev->user_orient = pdevmode->dmOrientation;
    }
    if (pdevmode->dmFields & DM_PAPERSIZE) {
        dev->user_paper = pdevmode->dmPaperSize;
        dev->user_media_size[0] = pdevmode->dmPaperWidth / 254.0 * 72.0;
        dev->user_media_size[1] = pdevmode->dmPaperLength / 254.0 * 72.0;
        dev->paper_name[0] = 0;	    /* unknown paper size */
    }
    if (pdevmode->dmFields & DM_DUPLEX) {
        dev->Duplex_set = 1;
        dev->Duplex = pdevmode->dmDuplex == DMDUP_SIMPLEX ? false : true;
        dev->tumble = pdevmode->dmDuplex == DMDUP_HORIZONTAL ? true : false;
    }

    return TRUE;
}

static int
win_pr2_update_win(gx_device_win_pr2 * dev, LPDEVMODEW pdevmode)
{
    if (dev->Duplex_set > 0) {
        pdevmode->dmFields |= DM_DUPLEX;
        pdevmode->dmDuplex = DMDUP_SIMPLEX;
        if (dev->Duplex) {
            if (dev->tumble == false) {
                pdevmode->dmDuplex = DMDUP_VERTICAL;
            } else {
                pdevmode->dmDuplex = DMDUP_HORIZONTAL;
            }
        }
    }

    if (dev->user_color) {
        pdevmode->dmColor = dev->user_color;
        pdevmode->dmFields |= DM_COLOR;
    }

    if (dev->user_orient) {
        pdevmode->dmFields |= DM_ORIENTATION;
        pdevmode->dmOrientation = dev->user_orient;
    }

    if (dev->user_paper) {
        pdevmode->dmFields |= DM_PAPERSIZE;
        pdevmode->dmPaperSize = dev->user_paper;
    }
    return 0;
}

/********************************************************************************/

#define BEGIN_ARRAY_PARAM(pread, pname, pa, psize, e)\
  switch ( code = pread(dict.list, (param_name = pname), &(pa)) )\
  {\
  case 0:\
        if ( (pa).size != psize )\
          ecode = gs_note_error(gs_error_rangecheck);\
        else {
/* The body of the processing code goes here. */
/* If it succeeds, it should do a 'break'; */
/* if it fails, it should set ecode and fall through. */
#define END_ARRAY_PARAM(pa, e)\
        }\
        goto e;\
  default:\
        ecode = code;\
e:	param_signal_error(dict.list, param_name, ecode);\
  case 1:\
        (pa).data = 0;		/* mark as not filled */\
  }

/* Put the user params from UserSettings into our */
/* internal variables. */
static int
win_pr2_read_user_settings(gx_device_win_pr2 * wdev, gs_param_list * plist)
{
    gs_param_dict dict;
    gs_param_string docn = { 0 };
    const char* dict_name = "UserSettings";
    const char* param_name = "";
    int code = 0;
    int ecode = 0;

    switch (code = param_begin_read_dict(plist, dict_name, &dict, false)) {
        default:
            param_signal_error(plist, dict_name, code);
            return code;
        case 1:
            break;
        case 0:
            {
                gs_param_int_array ia;

                BEGIN_ARRAY_PARAM(param_read_int_array, "DocumentRange", ia, 2, ia)
                if ((ia.data[0] < 0) ||
                    (ia.data[1] < 0) ||
                    (ia.data[0] > ia.data[1]))
                    ecode = gs_note_error(gs_error_rangecheck);
                wdev->doc_page_begin = ia.data[0];
                wdev->doc_page_end = ia.data[1];
                END_ARRAY_PARAM(ia, doc_range_error)

                BEGIN_ARRAY_PARAM(param_read_int_array, "SelectedRange", ia, 2, ia)
                if ((ia.data[0] < 0) ||
                    (ia.data[1] < 0) ||
                    (ia.data[0] > ia.data[1]))
                    ecode = gs_note_error(gs_error_rangecheck);
                wdev->user_page_begin = ia.data[0];
                wdev->user_page_end = ia.data[1];
                END_ARRAY_PARAM(ia, sel_range_error)

                param_read_int(dict.list, "Copies", &wdev->user_copies);
                param_read_int(dict.list, "Paper", &wdev->user_paper);
                param_read_int(dict.list, "Orientation", &wdev->user_orient);
                param_read_int(dict.list, "Color", &wdev->user_color);
                param_read_int(dict.list, "MaxResolution", &wdev->max_dpi);

                switch (code = param_read_string(dict.list, (param_name = "DocumentName"), &docn)) {
                    case 0:
                        if (docn.size < sizeof(wdev->doc_name))
                            break;
                        code = gs_error_rangecheck;
                        /* fall through */
                    default:
                        ecode = code;
                        param_signal_error(plist, param_name, ecode);
                        /* fall through */
                    case 1:
                        docn.data = 0;
                        break;
                }

                param_end_read_dict(plist, dict_name, &dict);

                if (docn.data) {
                    memcpy(wdev->doc_name, docn.data, docn.size);
                    wdev->doc_name[docn.size] = 0;
                }

                wdev->print_copies = 1;

                if (wdev->win32_hdevmode) {
                    LPDEVMODEW devmode = (LPDEVMODEW) GlobalLock(wdev->win32_hdevmode);
                    if (devmode) {
                        devmode->dmCopies = wdev->user_copies;
                        devmode->dmPaperSize = wdev->user_paper;
                        devmode->dmOrientation = wdev->user_orient;
                        devmode->dmColor = wdev->user_color;
                        GlobalUnlock(wdev->win32_hdevmode);
                    }
                }
            }
            break;
    }

    return code;
}

static int
win_pr2_write_user_settings(gx_device_win_pr2 * wdev, gs_param_list * plist)
{
    gs_param_dict dict;
    gs_param_int_array range;
    gs_param_float_array box;
    gs_param_string docn;
    gs_param_string papn;
    int array[2];
    const char* pname = "UserSettings";
    int code;

    dict.size = 12;
    code = param_begin_write_dict(plist, pname, &dict, false);
    if (code < 0) return code;

    array[0] = wdev->doc_page_begin;
    array[1] = wdev->doc_page_end;
    range.data = array;
    range.size = 2;
    range.persistent = false;
    code = param_write_int_array(dict.list, "DocumentRange", &range);
    if (code < 0) goto error;

    array[0] = wdev->user_page_begin;
    array[1] = wdev->user_page_end;
    range.data = array;
    range.size = 2;
    range.persistent = false;
    code = param_write_int_array(dict.list, "SelectedRange", &range);
    if (code < 0) goto error;

    box.data = wdev->user_media_size;
    box.size = 2;
    box.persistent = false;
    code = param_write_float_array(dict.list, "MediaSize", &box);
    if (code < 0) goto error;

    code = param_write_int(dict.list, "Copies", &wdev->user_copies);
    if (code < 0) goto error;

    code = param_write_int(dict.list, "Paper", &wdev->user_paper);
    if (code < 0) goto error;

    code = param_write_int(dict.list, "Orientation", &wdev->user_orient);
    if (code < 0) goto error;

    code = param_write_int(dict.list, "Color", &wdev->user_color);
    if (code < 0) goto error;

    code = param_write_int(dict.list, "MaxResolution", &wdev->max_dpi);
    if (code < 0) goto error;

    code = param_write_int(dict.list, "PrintCopies", &wdev->print_copies);
    if (code < 0) goto error;

    docn.data = (const byte*)wdev->doc_name;
    docn.size = strlen(wdev->doc_name);
    docn.persistent = false;

    code = param_write_string(dict.list, "DocumentName", &docn);
    if (code < 0) goto error;

    papn.data = (const byte*)wdev->paper_name;
    papn.size = strlen(wdev->paper_name);
    papn.persistent = false;

    code = param_write_string(dict.list, "PaperName", &papn);
    if (code < 0) goto error;

    code = param_write_bool(dict.list, "UserChangedSettings", &wdev->user_changed_settings);

error:
    param_end_write_dict(plist, pname, &dict);
    return code;
}

/********************************************************************************/

/*  Show up a dialog for the user to choose a printer and a paper size.
 *  If mode == 3, then automatically select the default Windows printer
 *  instead of asking the user.
 */

static int
win_pr2_print_setup_interaction(gx_device_win_pr2 * wdev, int mode)
{
    PRINTDLGW pd;
    LPDEVMODEW  devmode;
    LPDEVNAMES devnames;
    WCHAR* unidevname;
    char* devname;
    int devname_size;

    wdev->user_changed_settings = FALSE;
    wdev->query_user = mode;

    memset(&pd, 0, sizeof(pd));
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = PARENT_WINDOW;

    switch (mode) {
        case 2:	pd.Flags = PD_PRINTSETUP; break;
        case 3:	pd.Flags = PD_RETURNDEFAULT; break;
        default: pd.Flags = 0; break;
    }

    pd.Flags |= PD_USEDEVMODECOPIES;

    pd.nMinPage = wdev->doc_page_begin;
    pd.nMaxPage = wdev->doc_page_end;
    pd.nFromPage = wdev->user_page_begin;
    pd.nToPage = wdev->user_page_end;
    pd.nCopies = wdev->user_copies;

    /* Show the Print Setup dialog and let the user choose a printer
     * and a paper size/orientation.
     */

    if (!PrintDlgW(&pd)) return FALSE;

    devnames = (LPDEVNAMES) GlobalLock(pd.hDevNames);

    unidevname = (WCHAR*)(devnames)+(devnames->wDeviceOffset);
    devname_size = gp_uint16_to_utf8(NULL, unidevname);
    devname = malloc(devname_size);
    if (devname == NULL) {
        GlobalUnlock(pd.hDevNames);
        return FALSE;
    }
    gp_uint16_to_utf8(devname, unidevname);

    devmode = (LPDEVMODEW)GlobalLock(pd.hDevMode);

    wdev->user_changed_settings = TRUE;
    if (wdev->use_old_spool_name) {
        gs_snprintf(wdev->fname, prn_fname_sizeof, "\\\\spool\\%s", devname);
    } else {
        gs_snprintf(wdev->fname, prn_fname_sizeof, "%%printer%%%s", devname);
    }
    free(devname);

    if (mode == 3) {
		devmode->dmFields |= DM_COPIES;
        devmode->dmCopies = wdev->user_copies * wdev->print_copies;
        pd.nCopies = 1;

		if(wdev->user_orient){
			devmode->dmFields |= DM_ORIENTATION;
			devmode->dmOrientation = wdev->user_orient;
		}

        if (wdev->user_color) {
		    devmode->dmFields |= DM_COLOR;
		    devmode->dmColor = wdev->user_color;
        }

		if(wdev->user_paper){
			devmode->dmFields |= DM_PAPERSIZE;
			devmode->dmPaperSize = wdev->user_paper;
		}
    }

    wdev->user_page_begin = pd.nFromPage;
    wdev->user_page_end = pd.nToPage;
    wdev->user_copies = devmode->dmCopies;
    wdev->print_copies = pd.nCopies;
    wdev->user_media_size[0] = devmode->dmPaperWidth / 254.0 * 72.0;
    wdev->user_media_size[1] = devmode->dmPaperLength / 254.0 * 72.0;
    wdev->user_paper = devmode->dmPaperSize;
    wdev->user_orient = devmode->dmOrientation;
    wdev->user_color = devmode->dmColor;

    if (devmode->dmFields & DM_DUPLEX) {
        wdev->Duplex_set = 1;
        wdev->Duplex = devmode->dmDuplex == DMDUP_SIMPLEX ? false : true;
        wdev->tumble = devmode->dmDuplex == DMDUP_HORIZONTAL ? true : false;
    }

    {
        float xppinch = 0;
        float yppinch = 0;
        const WCHAR* driver = (WCHAR*)(devnames)+(devnames->wDriverOffset);
        const WCHAR* device = (WCHAR*)(devnames)+(devnames->wDeviceOffset);
        const WCHAR* output = (WCHAR*)(devnames)+(devnames->wOutputOffset);

        HDC hic = CreateICW(driver, device, output, devmode);

        if (hic) {
            xppinch = (float)GetDeviceCaps(hic, LOGPIXELSX);
            yppinch = (float)GetDeviceCaps(hic, LOGPIXELSY);
            wdev->user_media_size[0] = GetDeviceCaps(hic, PHYSICALWIDTH) * 72.0 / xppinch;
            wdev->user_media_size[1] = GetDeviceCaps(hic, PHYSICALHEIGHT) * 72.0 / yppinch;
            DeleteDC(hic);
        }
    }

    devmode = NULL;
    devnames = NULL;

    GlobalUnlock(pd.hDevMode);
    GlobalUnlock(pd.hDevNames);

    if (wdev->win32_hdevmode != NULL) {
        GlobalFree(wdev->win32_hdevmode);
    }
    if (wdev->win32_hdevnames != NULL) {
        GlobalFree(wdev->win32_hdevnames);
    }

    wdev->win32_hdevmode = pd.hDevMode;
    wdev->win32_hdevnames = pd.hDevNames;

    return TRUE;
}

/*  Check that we are dealing with an original device. If this
 *  happens to be a copy made by "copydevice", we will have to
 *  copy the original's handles to the associated Win32 params.
 */

static void
win_pr2_copy_check(gx_device_win_pr2 * wdev)
{
    HGLOBAL hdevmode = wdev->win32_hdevmode;
    HGLOBAL hdevnames = wdev->win32_hdevnames;
    DWORD devmode_len = (hdevmode) ? GlobalSize(hdevmode) : 0;
    DWORD devnames_len = (hdevnames) ? GlobalSize(hdevnames) : 0;

    if (wdev->original_device == wdev)
        return;

    /* I have, I'm afraid, no real idea what this routine is for. Its been present
     * since the earliest incarnation of the device I can find (in 1994). It seems
     * to me that if this check ever fails, then things will always go badly wrong,
     * since the Windows-specific structures (eg hdcprn) will be reset. I'm not
     * sure if these are somehow expected to be set by calling 'open' again or
     * something. In any event this check is completely incompatible with the
     * device subclassing, because the device is now capable of moving around in
     * memory. So add this check to see if the device has been subclassed. If it
     * has, don't throw away the Windows-specific structures, we need them and
     * they certainly won't be recreated.
     */
    {
        gx_device *dev = wdev->parent;

        while(dev) {
            if ((gx_device *)wdev->original_device == dev)
                return;

            dev = dev->parent;
        }
    }

    wdev->hdcprn = NULL;
    wdev->win32_hdevmode = NULL;
    wdev->win32_hdevnames = NULL;

    wdev->original_device = wdev;

    if (devmode_len) {
        wdev->win32_hdevmode = GlobalAlloc(0, devmode_len);
        if (wdev->win32_hdevmode) {
            memcpy(GlobalLock(wdev->win32_hdevmode), GlobalLock(hdevmode), devmode_len);
            GlobalUnlock(wdev->win32_hdevmode);
            GlobalUnlock(hdevmode);
        }
    }

    if (devnames_len) {
        wdev->win32_hdevnames = GlobalAlloc(0, devnames_len);
        if (wdev->win32_hdevnames) {
            memcpy(GlobalLock(wdev->win32_hdevnames), GlobalLock(hdevnames), devnames_len);
            GlobalUnlock(wdev->win32_hdevnames);
            GlobalUnlock(hdevnames);
        }
    }
}

/* Modeless dialog box - Cancel printing */
BOOL CALLBACK
CancelDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_INITDIALOG:
            SetWindowText(hDlg, szAppName);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDCANCEL:
                    DestroyWindow(hDlg);
                    EndDialog(hDlg, 0);
                    return TRUE;
            }
    }
    return FALSE;
}

BOOL CALLBACK
AbortProc2(HDC hdcPrn, int code)
{
    process_interrupts(NULL);
    if (code == SP_OUTOFDISK)
        return (FALSE);		/* cancel job */
    return (TRUE);
}
