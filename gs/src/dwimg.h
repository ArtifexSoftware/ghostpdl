/* Copyright (C) 1996, 1998, Russell Lang.
   Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

// $RCSfile$ $Revision$

// Image Window class

class ImageWindow {
    static ImageWindow *first;
    ImageWindow *next;

    HWND hwnd;
    char FAR *device;		// handle to Ghostscript device

    int width, height;

    // Window scrolling stuff
    int cxClient, cyClient;
    int cxAdjust, cyAdjust;
    int nVscrollPos, nVscrollMax;
    int nHscrollPos, nHscrollMax;

    void register_class(void);

         public:
    static HINSTANCE hInstance;	// instance of EXE

    static HWND hwndtext;	// handle to text window

    friend ImageWindow *FindImageWindow(char FAR * dev);
    void open(char FAR * dev);
    void close(void);
    void sync(void);
    void page(void);
    void size(int x, int y);
    void create_window(void);
    LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);
};
