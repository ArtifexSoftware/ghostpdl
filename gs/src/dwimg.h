/* Copyright (C) 1996, Russell Lang.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/


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
