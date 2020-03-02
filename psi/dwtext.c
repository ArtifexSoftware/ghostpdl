/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/



/* Microsoft Windows text window for Ghostscript. */

#include <stdlib.h>
#include <string.h> 	/* use only far items */
#include <wchar.h>
#include <ctype.h>

#define STRICT
#include <windowsx.h>
#include "dwtext.h"
#include <commdlg.h>
#include <shellapi.h>

#define CHARSIZE 2
#ifndef WM_UNICHAR
#define WM_UNICHAR 0x109
#endif

/* Define  min and max, but make sure to use the identical definition */
/* to the one that all the compilers seem to have.... */
#ifndef min
#  define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#  define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef EOF
#define EOF (-1)
#endif

/* sysmenu */
#define M_COPY_CLIP 1
#define M_PASTE_CLIP 2

/* These two are defined in dwmain.c/dwmainc.c because they need access to the gsdll and instance */
int dwmain_add_file_control_path(const TCHAR *pathfile);
void dwmain_remove_file_control_path(const TCHAR *pathfile);

LRESULT CALLBACK WndTextProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
static void text_error(char *message);
static void text_new_line(TW *tw);
static void text_update_text(TW *tw, int count);
static void text_drag_drop(TW *tw, HDROP hdrop);
static void text_copy_to_clipboard(TW *tw);
static void text_paste_from_clipboard(TW *tw);

static const wchar_t* TextWinClassName = L"rjlTextWinClass";
static const POINT TextWinMinSize = {16, 4};

#if defined(_MSC_VER) && _MSC_VER < 1400
/*
 * 'wmemset()' is documented for both Visual Studio 6.0 and .NET 2003, but
 * a bug in the shipped "wctype.h" makes it available only for C++ programs.
 * As this is an inline function, it's not found in the libs.
 */
static wchar_t *wmemset(wchar_t *buf, wchar_t w, int count) {
    while (count > 0)
        buf[--count] = w;
    return buf;
}
#endif

static void
text_error(char *message)
{
    MessageBoxA((HWND)NULL,message,(LPSTR)NULL, MB_ICONHAND | MB_SYSTEMMODAL);
}

static void
UpdateScrollBarX(TW *tw)
{
    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
    si.nMin = 0;
    si.nTrackPos = 0;
    si.nPage = tw->ClientSize.x;
    si.nMax = tw->CharSize.x*tw->ScreenSize.x-1;
    si.nPos = tw->ScrollPos.x;
    SetScrollInfo(tw->hwnd, SB_HORZ, &si, TRUE);
}

static void
UpdateScrollBarY(TW *tw)
{
    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
    si.nMin = 0;
    si.nTrackPos = 0;
    si.nPage = tw->ClientSize.y;
    si.nMax = tw->CharSize.y*tw->ScreenSize.y-1;
    si.nPos = tw->ScrollPos.y;
    SetScrollInfo(tw->hwnd, SB_VERT, &si, TRUE);
}

/* Bring Cursor into text window */
void
text_to_cursor(TW *tw)
{
int nXinc=0;
int nYinc=0;
int cxCursor;
int cyCursor;
        cyCursor = tw->CursorPos.y * tw->CharSize.y;
        if ( (cyCursor + tw->CharSize.y > tw->ScrollPos.y + tw->ClientSize.y)
/*	  || (cyCursor < tw->ScrollPos.y) ) { */
/* change to scroll to end of window instead of just making visible */
/* so that ALL of error message can be seen */
          || (cyCursor < tw->ScrollPos.y+tw->ClientSize.y) ) {
                nYinc = max(0, cyCursor + tw->CharSize.y
                        - tw->ClientSize.y) - tw->ScrollPos.y;
                nYinc = min(nYinc, tw->ScrollMax.y - tw->ScrollPos.y);
        }
        cxCursor = tw->CursorPos.x * tw->CharSize.x;
        if ( (cxCursor + tw->CharSize.x > tw->ScrollPos.x + tw->ClientSize.x)
          || (cxCursor < tw->ScrollPos.x) ) {
                nXinc = max(0, cxCursor + tw->CharSize.x
                        - tw->ClientSize.x/2) - tw->ScrollPos.x;
                nXinc = min(nXinc, tw->ScrollMax.x - tw->ScrollPos.x);
        }
        if (nYinc || nXinc) {
                tw->ScrollPos.y += nYinc;
                tw->ScrollPos.x += nXinc;
                ScrollWindow(tw->hwnd,-nXinc,-nYinc,NULL,NULL);
                UpdateScrollBarX(tw);
                UpdateScrollBarY(tw);
                UpdateWindow(tw->hwnd);
        }
}

static void
text_new_line(TW *tw)
{
        tw->CursorPos.x = 0;
        tw->CursorPos.y++;
        if (tw->CursorPos.y >= tw->ScreenSize.y) {
            int i =  tw->ScreenSize.x * (tw->ScreenSize.y - 1);
                memmove(tw->ScreenBuffer, tw->ScreenBuffer+tw->ScreenSize.x,
                        i * CHARSIZE);
                wmemset(tw->ScreenBuffer + i, ' ', tw->ScreenSize.x);
                tw->CursorPos.y--;
                ScrollWindow(tw->hwnd,0,-tw->CharSize.y,NULL,NULL);
                UpdateWindow(tw->hwnd);
        }
        if (tw->CursorFlag)
                text_to_cursor(tw);

/*	TextMessage(); */
}

/* Update count characters in window at cursor position */
/* Updates cursor position */
static void
text_update_text(TW *tw, int count)
{
HDC hdc;
int xpos, ypos;
        xpos = tw->CursorPos.x*tw->CharSize.x - tw->ScrollPos.x;
        ypos = tw->CursorPos.y*tw->CharSize.y - tw->ScrollPos.y;
        hdc = GetDC(tw->hwnd);
        SelectFont(hdc, tw->hfont);
        TextOutW(hdc,xpos,ypos,
                 (LPCWSTR)(tw->ScreenBuffer +
                           tw->CursorPos.y*tw->ScreenSize.x +
                           tw->CursorPos.x),
                 count);
        (void)ReleaseDC(tw->hwnd,hdc);
        tw->CursorPos.x += count;
        if (tw->CursorPos.x >= tw->ScreenSize.x)
                text_new_line(tw);
}

void
text_size(TW *tw, int width, int height)
{
    tw->ScreenSize.x =  max(width, TextWinMinSize.x);
    tw->ScreenSize.y =  max(height, TextWinMinSize.y);
}

void
text_font(TW *tw, const char *name, int size)
{
    /* make a new font */
    LOGFONTA lf;
    TEXTMETRIC tm;
    LPSTR p;
    HDC hdc;

    /* reject inappropriate arguments */
    if (name == NULL)
        return;
    if (size < 4)
        return;

    /* set new name and size */
    free(tw->fontname);
    tw->fontname = (char *)malloc(strlen(name)+1);
    if (tw->fontname == NULL)
        return;
    strcpy(tw->fontname, name);
    tw->fontsize = size;

    /* if window not open, hwnd == 0 == HWND_DESKTOP */
    hdc = GetDC(tw->hwnd);
    memset(&lf, 0, sizeof(LOGFONTA));
    strncpy(lf.lfFaceName,tw->fontname,LF_FACESIZE);
    lf.lfHeight = -MulDiv(tw->fontsize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    lf.lfPitchAndFamily = FIXED_PITCH;
    lf.lfCharSet = DEFAULT_CHARSET;
    if ( (p = strstr(tw->fontname," Italic")) != (LPSTR)NULL ) {
        lf.lfFaceName[ (unsigned int)(p-tw->fontname) ] = '\0';
        lf.lfItalic = TRUE;
    }
    if ( (p = strstr(tw->fontname," Bold")) != (LPSTR)NULL ) {
        lf.lfFaceName[ (unsigned int)(p-tw->fontname) ] = '\0';
        lf.lfWeight = FW_BOLD;
    }
    if (tw->hfont)
        DeleteFont(tw->hfont);

    tw->hfont = CreateFontIndirectA((LOGFONTA FAR *)&lf);

    /* get text size */
    SelectFont(hdc, tw->hfont);
    GetTextMetrics(hdc,(TEXTMETRIC FAR *)&tm);
    tw->CharSize.y = tm.tmHeight;
    tw->CharSize.x = tm.tmAveCharWidth;
    tw->CharAscent = tm.tmAscent;
    if (tw->bFocus)
        CreateCaret(tw->hwnd, 0, tw->CharSize.x, 2+tw->CaretHeight);
    ReleaseDC(tw->hwnd, hdc);

    /* redraw window if necessary */
    if (tw->hwnd != HWND_DESKTOP) {
        /* INCOMPLETE */
    }
}

/* Set drag strings */
void
text_drag(TW *tw, const char *pre, const char *post)
{
    /* remove old strings */
    free((char *)tw->DragPre);
    tw->DragPre = NULL;
    free((char *)tw->DragPost);
    tw->DragPost = NULL;

    /* add new strings */
    tw->DragPre = malloc(strlen(pre)+1);
    if (tw->DragPre)
        strcpy(tw->DragPre, pre);
    tw->DragPost = malloc(strlen(post)+1);
    if (tw->DragPost)
        strcpy(tw->DragPost, post);
}

/* Set the window position and size */
void
text_setpos(TW *tw, int x, int y, int cx, int cy)
{
    tw->x = x;
    tw->y = y;
    tw->cx = cx;
    tw->cy = cy;
}

/* Get the window position and size */
int text_getpos(TW *tw, int *px, int *py, int *pcx, int *pcy)
{
    *px = tw->x;
    *py = tw->y;
    *pcx = tw->cx;
    *pcy = tw->cy;
    return 0;
}

/* Allocate new text window */
TW *
text_new(void)
{
    TW *tw;
    tw = (TW *)malloc(sizeof(TW));
    if (tw == NULL)
        return NULL;
    /* make sure everything is null */
    memset(tw, 0, sizeof(TW));

    /* set some defaults */
    text_font(tw, "Courier New", 10);
    text_size(tw, 80, 24);
    tw->KeyBufSize = 2048;
    tw->CursorFlag = 1;	/* scroll to cursor after \n or \r */
    tw->hwnd = HWND_DESKTOP;

    tw->line_end = 0;
    tw->line_start = 0;
    tw->line_complete = FALSE;
    tw->line_eof = FALSE;

    tw->x = CW_USEDEFAULT;
    tw->y = CW_USEDEFAULT;
    tw->cx = CW_USEDEFAULT;
    tw->cy = CW_USEDEFAULT;
    tw->utf8shift = 0;
    return tw;
}

/* Destroy window and deallocate text window structure */
void
text_destroy(TW *tw)
{
    if (tw->hwnd)
        DestroyWindow(tw->hwnd);
    tw->hwnd = HWND_DESKTOP;

    if (tw->hfont)
        DeleteFont(tw->hfont);
    tw->hfont = NULL;

    free((char *)tw->KeyBuf);
    tw->KeyBuf = NULL;

    free((char *)tw->ScreenBuffer);
    tw->ScreenBuffer = NULL;

    free((char *)tw->DragPre);
    tw->DragPre = NULL;

    free((char *)tw->DragPost);
    tw->DragPost = NULL;

    free((char *)tw->fontname);
    tw->fontname = NULL;

    free((char *)tw->TitleW);
    tw->TitleW = NULL;
}

/* register the window class */
int
text_register_class(TW *tw, HICON hicon)
{
    WNDCLASSW wndclass;
    HINSTANCE hInstance = GetModuleHandle(NULL);
    tw->hIcon = hicon;

    /* register window class */
    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = WndTextProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = sizeof(void *);
    wndclass.hInstance = hInstance;
    wndclass.hIcon = tw->hIcon ? tw->hIcon : LoadIcon(NULL, IDI_APPLICATION);
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.hbrBackground = GetStockBrush(WHITE_BRUSH);
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = TextWinClassName;
    return RegisterClassW(&wndclass);
}

/* Show the window */
int text_create(TW *tw, const char *app_name, int show_cmd)
{
    HMENU sysmenu;
    HINSTANCE hInstance = GetModuleHandle(NULL);
    wchar_t *app_nameW, *d;
    const char *s;

    app_nameW = malloc(strlen(app_name)*2+2);
    if (app_nameW == NULL) {
        text_error("Out of memory");
        return 1;
    }
    d = app_nameW;
    s = app_name;
    while ((*d++ = (wchar_t)(unsigned char)(*s++)) != 0);
    tw->TitleW = app_nameW;

    tw->Title = app_name;
    tw->nCmdShow = show_cmd;
    tw->quitnow = FALSE;

    /* make sure we have some sensible defaults */
    if (tw->KeyBufSize < 256)
        tw->KeyBufSize = 256;

    tw->CursorPos.x = tw->CursorPos.y = 0;
    tw->bFocus = FALSE;
    tw->bGetCh = FALSE;
    tw->CaretHeight = 0;

    /* allocate buffers */
    tw->KeyBufIn = tw->KeyBufOut = tw->KeyBuf = malloc(tw->KeyBufSize);
    if (tw->KeyBuf == NULL) {
        text_error("Out of memory");
        return 1;
    }
    tw->ScreenBuffer = malloc(tw->ScreenSize.x * tw->ScreenSize.y * CHARSIZE);
    if (tw->ScreenBuffer == NULL) {
        text_error("Out of memory");
        return 1;
    }
    wmemset(tw->ScreenBuffer, ' ', tw->ScreenSize.x * tw->ScreenSize.y);

    tw->hwnd = CreateWindowW(TextWinClassName, app_nameW,
                  WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL,
                  tw->x, tw->y, tw->cx, tw->cy,
                  NULL, NULL, hInstance, tw);

    if (tw->hwnd == NULL) {
        MessageBoxA((HWND)NULL,"Couldn't open text window",(LPSTR)NULL, MB_ICONHAND | MB_SYSTEMMODAL);
        return 1;
    }

    ShowWindow(tw->hwnd, tw->nCmdShow);
    sysmenu = GetSystemMenu(tw->hwnd,0);	/* get the sysmenu */
    AppendMenu(sysmenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(sysmenu, MF_STRING, M_COPY_CLIP, "Copy to Clip&board");
    AppendMenuA(sysmenu, MF_STRING, M_PASTE_CLIP, "&Paste");

    return 0;
}

int
text_putch(TW *tw, int ch)
{
int pos;
int n;
int shift = tw->utf8shift;
    tw->utf8shift=0;
    if (tw->quitnow)
        return ch;	/* don't write error message as we shut down */
    switch(ch) {
        case '\r':
                tw->CursorPos.x = 0;
                if (tw->CursorFlag)
                    text_to_cursor(tw);
                break;
        case '\n':
                text_new_line(tw);
                break;
        case 7:
                MessageBeep(-1);
                if (tw->CursorFlag)
                    text_to_cursor(tw);
                break;
        case '\t':
                {
                    for (n = 8 - (tw->CursorPos.x % 8); n>0; n-- )
                            text_putch(tw, ' ');
                }
                break;
        case 0x08:
        case 0x7f:
                tw->CursorPos.x--;
                if (tw->CursorPos.x < 0) {
                    tw->CursorPos.x = tw->ScreenSize.x - 1;
                    tw->CursorPos.y--;
                }
                if (tw->CursorPos.y < 0)
                    tw->CursorPos.y = 0;
                break;
        default:
                pos = tw->CursorPos.y*tw->ScreenSize.x + tw->CursorPos.x;
                /* Are we continuing a unicode char? */
                if ((ch & 0xC0) == 0x80) {
                    tw->ScreenBuffer[pos] |= (ch & 0x3F)<<shift;
                    if (shift > 0)
                        tw->utf8shift = shift-6;
                    else
                        text_update_text(tw, 1); /* Only update when complete */
                } else if (ch >= 0xe0) { /* 2 more to come */
                    tw->ScreenBuffer[pos] = (ch & 0x0f)<<12;
                    tw->utf8shift = 6;
                } else if (ch >= 0xC0) { /* 1 more to come */
                    tw->ScreenBuffer[pos] = (ch & 0x01f)<<6;
                    tw->utf8shift = 0;
                }
                else
                {
                    tw->ScreenBuffer[pos] = ch;
                    text_update_text(tw, 1);
                }
    }
    return ch;
}

void
text_write_buf(TW *tw, const char *str, int cnt)
{
wchar_t *p;
int count, limit;
int n;
    if (tw->quitnow)
        return;		/* don't write error message as we shut down */
    while (cnt>0) {
        p = tw->ScreenBuffer + tw->CursorPos.y*tw->ScreenSize.x + tw->CursorPos.x;
        limit = tw->ScreenSize.x - tw->CursorPos.x;
        for (count=0;
             (count < limit) && (cnt>0) && (((*str >= 32) && (*str <= 0x7F)) || *str=='\t');
             count++) {
            if (*str=='\t') {
                for (n = 8 - ((tw->CursorPos.x+count) % 8); (count < limit) & (n>0); n--, count++ )
                    *p++ = ' ';
                str++;
                count--;
            }
            else {
                *p++ = *str++;
            }
            cnt--;
        }
        if (count>0) {
            text_update_text(tw, count);
        }
        if (cnt > 0) {
            if (*str=='\n') {
                text_new_line(tw);
                str++;
                cnt--;
            }
            else if (!((*str >= 32) && (*str <= 0x7f)) && *str!='\t') {
                text_putch(tw, *(const unsigned char *)str++);
                cnt--;
            }
        }
    }
}

/* Put string to window */
void
text_puts(TW *tw, const char *str)
{
    text_write_buf(tw, str, strlen(str));
}

/* TRUE if key hit, FALSE if no key */
int
text_kbhit(TW *tw)
{
    return (tw->KeyBufIn != tw->KeyBufOut);
}

/* get character from keyboard, no echo */
/* need to add extended codes */
int
text_getch(TW *tw)
{
    MSG msg;
    int ch;
    text_to_cursor(tw);
    tw->bGetCh = TRUE;
    if (tw->bFocus) {
        SetCaretPos(tw->CursorPos.x*tw->CharSize.x - tw->ScrollPos.x,
            tw->CursorPos.y*tw->CharSize.y + tw->CharAscent
            - tw->CaretHeight - tw->ScrollPos.y);
        ShowCaret(tw->hwnd);
    }

    while (PeekMessageW(&msg, (HWND)NULL, 0, 0, PM_NOREMOVE)) {
        if (GetMessageW(&msg, (HWND)NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (tw->quitnow)
       return EOF;	/* window closed */

    while (!text_kbhit(tw)) {
        if (!tw->quitnow) {
            if (GetMessageW(&msg, (HWND)NULL, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        else
           return EOF;	/* window closed */
    }

    ch = *tw->KeyBufOut++;
    if (ch=='\r')
        ch = '\n';
    if (tw->KeyBufOut - tw->KeyBuf >= tw->KeyBufSize)
        tw->KeyBufOut = tw->KeyBuf;		/* wrap around */
    if (tw->bFocus)
        HideCaret(tw->hwnd);
    tw->bGetCh = FALSE;
    return ch;
}

/* Read line from keyboard using buffered input
 * Return at most 'len' characters
 * Does NOT add null terminating character
 * This is NOT the same as fgets()
 * Do not mix this with calls to text_getch()
 */
int
text_read_line(TW *tw, char *line, int len)
{
int ch;
    if (tw->line_eof)
        return 0;

    while (!tw->line_complete) {
        /* we have not yet collected a full line */
        ch = text_getch(tw);
        switch(ch) {
            case EOF:
            case 26:	/* ^Z == EOF */
                tw->line_eof = TRUE;
                tw->line_complete = TRUE;
                break;
            case '\b':	/* ^H */
            case 0x7f:  /* DEL */
                if (tw->line_end) {
                    text_putch(tw, '\b');
                    text_putch(tw, ' ');
                    text_putch(tw, '\b');
                    while ((tw->line_end) &&
                           ((tw->line_buf[tw->line_end-1] & 0xC0) == 0x80)) {
                        /* It's a UTF-8 continuation char. */
                        --(tw->line_end);
                    }
                    if (tw->line_end == 0)
                        break;
                    --(tw->line_end);
                }
                break;
            case 21:	/* ^U */
                while (tw->line_end) {
                    text_putch(tw, '\b');
                    text_putch(tw, ' ');
                    text_putch(tw, '\b');
                    --(tw->line_end);
                }
                break;
            case '\r':
            case '\n':
                tw->line_complete = TRUE;
                /* fall through */
            default:
                tw->line_buf[tw->line_end++] = ch;
                text_putch(tw, ch);
                break;
        }
        if (tw->line_end >= sizeof(tw->line_buf))
            tw->line_complete = TRUE;
    }

    if (tw->quitnow)
        return -1;

    if (tw->line_complete) {
        /* We either filled the buffer or got CR, LF or EOF */
        int count = min(len, tw->line_end - tw->line_start);
        memcpy(line, tw->line_buf + tw->line_start, count);
        tw->line_start += count;
        if (tw->line_start == tw->line_end) {
            tw->line_start = tw->line_end = 0;
            tw->line_complete = FALSE;
        }
        return count;
    }

    return 0;
}

/* Read a string from the keyboard, of up to len characters */
/* (not including trailing NULL) */
int
text_gets(TW *tw, char *line, int len)
{
LPSTR dest = line;
LPSTR limit = dest + len;  /* don't leave room for '\0' */
int ch;
    do {
        if (dest >= limit)
            break;
        ch = text_getch(tw);
        switch(ch) {
            case 26:	/* ^Z == EOF */
                return 0;
            case '\b':	/* ^H */
            case 0x7f:  /* DEL */
                if (dest > line) {
                    text_putch(tw, '\b');
                    text_putch(tw, ' ');
                    text_putch(tw, '\b');
                    while ((dest > line) &&
                           ((dest[-1] & 0xC0) == 0x80)) {
                        /* It's a UTF-8 continuation char. */
                        --(dest);
                    }
                    if (dest == line)
                        break;
                    --dest;
                }
                break;
            case 21:	/* ^U */
                while (dest > line) {
                    text_putch(tw, '\b');
                    text_putch(tw, ' ');
                    text_putch(tw, '\b');
                    --dest;
                }
                break;
            default:
                *dest++ = ch;
                text_putch(tw, ch);
                break;
        }
    } while (ch != '\n');

    *dest = '\0';
    return (dest-line);
}

void
text_clear_drag_and_drop_list(TW* tw, int freelist)
{
    int i;
    for (i = 0; tw->szFiles && i < tw->cFiles; i++) {
        if (tw->szFiles[i] != NULL) {
            dwmain_remove_file_control_path(tw->szFiles[i]);
            free(tw->szFiles[i]);
            tw->szFiles[i] = NULL;
        }
    }
    if (freelist != 0) {
        free(tw->szFiles);
        tw->szFiles = NULL;
        tw->cFiles = 0;
    }
}

/* Windows 3.1 drag-drop feature */
static void
text_drag_drop(TW *tw, HDROP hdrop)
{
    int i, cFiles, code;
    unsigned int Len, error;
    const char *p;
    const TCHAR *t;
    if ( (tw->DragPre==NULL) || (tw->DragPost==NULL) )
            return;
    text_clear_drag_and_drop_list(tw, 0);
    cFiles = DragQueryFile(hdrop, (UINT)(-1), (LPTSTR)NULL, 0);
    if (tw->cFiles < cFiles) {
        free(tw->szFiles);
        tw->szFiles = malloc(cFiles * sizeof(char*));
        if (tw->szFiles == NULL) {
            tw->cFiles = 0;
            return;
        }
        memset(tw->szFiles, 0x00, cFiles * sizeof(char*));
        tw->cFiles = cFiles;
    }
    for (i=0; i<cFiles; i++) {
        Len = DragQueryFile(hdrop, i, NULL, 0);
        tw->szFiles[i] = (TCHAR *)malloc((Len+1)*sizeof(TCHAR));
        if (tw->szFiles[i] != 0) {
            error = DragQueryFile(hdrop, i, tw->szFiles[i], Len+1);
            if (error != 0) {
                code = dwmain_add_file_control_path(tw->szFiles[i]);
                if (code >= 0) {
                    for (p=tw->DragPre; *p; p++)
                        SendMessage(tw->hwnd,WM_CHAR,*p,1L);
                    for (t=tw->szFiles[i]; *t; t++) {
                        if (*t == '\\')
                            SendMessage(tw->hwnd,WM_CHAR,'/',1L);
                        else
                            SendMessage(tw->hwnd,WM_CHAR,*t,1L);
                    }
                    for (p=tw->DragPost; *p; p++)
                        SendMessage(tw->hwnd,WM_CHAR,*p,1L);
                }
            }
        }
    }
    DragFinish(hdrop);
}

void
text_copy_to_clipboard(TW *tw)
{
    int size, count;
    HGLOBAL hGMem;
    LPWSTR cbuf, cp;
    UINT type;
    int i;

    size = tw->ScreenSize.y * (tw->ScreenSize.x + 2) + 1;
    size *= CHARSIZE;
    hGMem = GlobalAlloc(GHND | GMEM_SHARE, (DWORD)size);
    cbuf = cp = (LPWSTR)GlobalLock(hGMem);
    if (cp == NULL)
        return;

    for (i=0; i<tw->ScreenSize.y; i++) {
        count = tw->ScreenSize.x;
        memcpy(cp, tw->ScreenBuffer + tw->ScreenSize.x*i, count*CHARSIZE);
        /* remove trailing spaces */
        for (count=count-1; count>=0; count--) {
                if (cp[count]!=' ')
                        break;
                cp[count] = '\0';
        }
        cp[++count] = '\r';
        cp[++count] = '\n';
        cp[++count] = '\0';
        cp += count;
    }
    /* Now remove completely empty trailing lines */
    while (cp >= cbuf+4) {
        if ((cp[-3] != '\n') || (cp[-4] != '\r'))
            break;
        cp -= 2;
        *cp = '\0';
    }
    size = CHARSIZE*(wcslen(cbuf) + 1);
    GlobalUnlock(hGMem);
    hGMem = GlobalReAlloc(hGMem, (DWORD)size, GHND | GMEM_SHARE);
    type = CF_UNICODETEXT;
    /* give buffer to clipboard */
    OpenClipboard(tw->hwnd);
    EmptyClipboard();
    SetClipboardData(type, hGMem);
    CloseClipboard();
}

void
text_paste_from_clipboard(TW *tw)
{
    HGLOBAL hClipMemory;
    wchar_t *p;
    long count;
    OpenClipboard(tw->hwnd);
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        hClipMemory = GetClipboardData(CF_UNICODETEXT);
        p = GlobalLock(hClipMemory);
        while (*p) {
            /* transfer to keyboard circular buffer */
            count = tw->KeyBufIn - tw->KeyBufOut;
            if (count < 0)
                count += tw->KeyBufSize;
            /* The clipboard contains unicode, but we put it into the key
             * buffer as if it was typing utf8 */
            if (*p >= 0x800) {
                if (count < tw->KeyBufSize-3) {
                    *tw->KeyBufIn++ = 0xE0 | (*p>>12);
                    if (tw->KeyBufIn - tw->KeyBuf >= tw->KeyBufSize)
                        tw->KeyBufIn = tw->KeyBuf; /* wrap around */
                    *tw->KeyBufIn++ = 0x80 | ((*p>>6) & 0x3F);
                    if (tw->KeyBufIn - tw->KeyBuf >= tw->KeyBufSize)
                        tw->KeyBufIn = tw->KeyBuf; /* wrap around */
                    *tw->KeyBufIn++ = 0x80 | (*p & 0x3f);
                    if (tw->KeyBufIn - tw->KeyBuf >= tw->KeyBufSize)
                        tw->KeyBufIn = tw->KeyBuf; /* wrap around */
                }
            } else if (*p >= 0x80) {
                if (count < tw->KeyBufSize-2) {
                    *tw->KeyBufIn++ = 0xC0 | (*p>>6);
                    if (tw->KeyBufIn - tw->KeyBuf >= tw->KeyBufSize)
                        tw->KeyBufIn = tw->KeyBuf; /* wrap around */
                    *tw->KeyBufIn++ = 0x80 | (*p & 0x3f);
                    if (tw->KeyBufIn - tw->KeyBuf >= tw->KeyBufSize)
                        tw->KeyBufIn = tw->KeyBuf; /* wrap around */
                }
            } else if (count < tw->KeyBufSize-1) {
                *tw->KeyBufIn++ = *p;
                if (tw->KeyBufIn - tw->KeyBuf >= tw->KeyBufSize)
                    tw->KeyBufIn = tw->KeyBuf; /* wrap around */
            }
            p++;
        }
        GlobalUnlock(hClipMemory);
    }
    CloseClipboard();
}

/* text window */
LRESULT CALLBACK
WndTextProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HDC hdc;
    PAINTSTRUCT ps;
    RECT rect;
    int nYinc, nXinc;
    TW *tw;
    if (message == WM_CREATE) {
        /* Object is stored in window extra data.
         * Nothing must try to use the object before WM_CREATE
         * initializes it here.
         */
        tw = (TW *)(((CREATESTRUCT FAR *)lParam)->lpCreateParams);
        SetWindowLongPtr(hwnd, 0, (LONG_PTR)tw);
    }
    tw = (TW *)GetWindowLongPtr(hwnd, 0);

    switch(message) {
        case WM_SYSCOMMAND:
            switch(LOWORD(wParam)) {
                case M_COPY_CLIP:
                    text_copy_to_clipboard(tw);
                    return 0;
                case M_PASTE_CLIP:
                    text_paste_from_clipboard(tw);
                    return 0;
            }
            break;
        case WM_SETFOCUS:
            tw->bFocus = TRUE;
            CreateCaret(hwnd, 0, tw->CharSize.x, 2+tw->CaretHeight);
            SetCaretPos(tw->CursorPos.x*tw->CharSize.x - tw->ScrollPos.x,
                    tw->CursorPos.y*tw->CharSize.y + tw->CharAscent
                     - tw->CaretHeight - tw->ScrollPos.y);
            if (tw->bGetCh)
                    ShowCaret(hwnd);
            break;
        case WM_KILLFOCUS:
            DestroyCaret();
            tw->bFocus = FALSE;
            break;
        case WM_MOVE:
            if (!IsIconic(hwnd) && !IsZoomed(hwnd)) {
                GetWindowRect(hwnd, &rect);
                tw->x = rect.left;
                tw->y = rect.top;
            }
            break;
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED)
                    return(0);

            /* remember current window size */
            if (wParam != SIZE_MAXIMIZED) {
                GetWindowRect(hwnd, &rect);
                tw->cx = rect.right - rect.left;
                tw->cy = rect.bottom - rect.top;
                tw->x = rect.left;
                tw->y = rect.top;
            }

            tw->ClientSize.y = HIWORD(lParam);
            tw->ClientSize.x = LOWORD(lParam);

            tw->ScrollMax.y = max(0, tw->CharSize.y*tw->ScreenSize.y - tw->ClientSize.y);
            tw->ScrollPos.y = min(tw->ScrollPos.y, tw->ScrollMax.y);

            UpdateScrollBarY(tw);

            tw->ScrollMax.x = max(0, tw->CharSize.x*tw->ScreenSize.x - tw->ClientSize.x);
            tw->ScrollPos.x = min(tw->ScrollPos.x, tw->ScrollMax.x);

            UpdateScrollBarX(tw);

            if (tw->bFocus && tw->bGetCh) {
                SetCaretPos(tw->CursorPos.x*tw->CharSize.x - tw->ScrollPos.x,
                            tw->CursorPos.y*tw->CharSize.y + tw->CharAscent
                            - tw->CaretHeight - tw->ScrollPos.y);
                ShowCaret(hwnd);
            }
            return(0);
        case WM_VSCROLL:
            switch(LOWORD(wParam)) {
                case SB_TOP:
                    nYinc = -tw->ScrollPos.y;
                    break;
                case SB_BOTTOM:
                    nYinc = tw->ScrollMax.y - tw->ScrollPos.y;
                    break;
                case SB_LINEUP:
                    nYinc = -tw->CharSize.y;
                    break;
                case SB_LINEDOWN:
                    nYinc = tw->CharSize.y;
                    break;
                case SB_PAGEUP:
                    nYinc = min(-1,-tw->ClientSize.y);
                    break;
                case SB_PAGEDOWN:
                    nYinc = max(1,tw->ClientSize.y);
                    break;
                case SB_THUMBPOSITION:
                    nYinc = HIWORD(wParam) - tw->ScrollPos.y;
                    break;
                default:
                    nYinc = 0;
            }
            if ( (nYinc = max(-tw->ScrollPos.y,
                    min(nYinc, tw->ScrollMax.y - tw->ScrollPos.y)))
                    != 0 ) {
                    tw->ScrollPos.y += nYinc;
                    ScrollWindow(hwnd,0,-nYinc,NULL,NULL);
                    UpdateScrollBarY(tw);
                    UpdateWindow(hwnd);
            }
            return(0);
        case WM_HSCROLL:
            switch(LOWORD(wParam)) {
                case SB_LINEUP:
                    nXinc = -tw->CharSize.x;
                    break;
                case SB_LINEDOWN:
                    nXinc = tw->CharSize.x;
                    break;
                case SB_PAGEUP:
                    nXinc = min(-1,-tw->ClientSize.x);
                    break;
                case SB_PAGEDOWN:
                    nXinc = max(1,tw->ClientSize.x);
                    break;
                case SB_THUMBPOSITION:
                    nXinc = HIWORD(wParam) - tw->ScrollPos.x;
                    break;
                default:
                    nXinc = 0;
            }
            if ( (nXinc = max(-tw->ScrollPos.x,
                    min(nXinc, tw->ScrollMax.x - tw->ScrollPos.x)))
                    != 0 ) {
                    tw->ScrollPos.x += nXinc;
                    ScrollWindow(hwnd,-nXinc,0,NULL,NULL);
                    UpdateScrollBarX(tw);
                    UpdateWindow(hwnd);
            }
            return(0);
        case WM_KEYDOWN:
            switch(wParam) {
              case VK_HOME:
                      SendMessage(hwnd, WM_VSCROLL, SB_TOP, (LPARAM)0);
                      break;
              case VK_END:
                      SendMessage(hwnd, WM_VSCROLL, SB_BOTTOM, (LPARAM)0);
                      break;
              case VK_PRIOR:
                      SendMessage(hwnd, WM_VSCROLL, SB_PAGEUP, (LPARAM)0);
                      break;
              case VK_NEXT:
                      SendMessage(hwnd, WM_VSCROLL, SB_PAGEDOWN, (LPARAM)0);
                      break;
              case VK_UP:
                      SendMessage(hwnd, WM_VSCROLL, SB_LINEUP, (LPARAM)0);
                      break;
              case VK_DOWN:
                      SendMessage(hwnd, WM_VSCROLL, SB_LINEDOWN, (LPARAM)0);
                      break;
              case VK_LEFT:
                      SendMessage(hwnd, WM_HSCROLL, SB_LINEUP, (LPARAM)0);
                      break;
              case VK_RIGHT:
                      SendMessage(hwnd, WM_HSCROLL, SB_LINEDOWN, (LPARAM)0);
                      break;
            }
            break;
#ifdef WM_UNICHAR
        case WM_UNICHAR: /* Belt and braces */
#endif
        case WM_CHAR:
            { /* We get unicode in, but we put it into the buffer as if the
               * user was typing UTF8.
               */
                long count = tw->KeyBufIn - tw->KeyBufOut;
                if (count < 0) count += tw->KeyBufSize;
                if (wParam >= 0x800) {
                    if (count >= tw->KeyBufSize-3)
                        return 0; /* Silently drop the chars! */
                    *tw->KeyBufIn++ = 0xE0 | (wParam>>12);
                    if (tw->KeyBufIn - tw->KeyBuf >= tw->KeyBufSize)
                        tw->KeyBufIn = tw->KeyBuf; /* wrap around */
                    *tw->KeyBufIn++ = 0x80 | ((wParam>>6) & 0x3F);
                    if (tw->KeyBufIn - tw->KeyBuf >= tw->KeyBufSize)
                        tw->KeyBufIn = tw->KeyBuf; /* wrap around */
                    *tw->KeyBufIn++ = 0x80 | (wParam & 0x3f);
                    if (tw->KeyBufIn - tw->KeyBuf >= tw->KeyBufSize)
                        tw->KeyBufIn = tw->KeyBuf; /* wrap around */
                } else if (wParam >= 0x80) {
                    if (count >= tw->KeyBufSize-2)
                        return 0; /* Silently drop the chars! */
                    *tw->KeyBufIn++ = 0xC0 | (wParam>>6);
                    if (tw->KeyBufIn - tw->KeyBuf >= tw->KeyBufSize)
                        tw->KeyBufIn = tw->KeyBuf; /* wrap around */
                    *tw->KeyBufIn++ = 0x80 | (wParam & 0x3f);
                    if (tw->KeyBufIn - tw->KeyBuf >= tw->KeyBufSize)
                        tw->KeyBufIn = tw->KeyBuf; /* wrap around */
                } else {
                    if (count >= tw->KeyBufSize-1)
                        return 0; /* Silently drop the char! */
                    *tw->KeyBufIn++ = wParam;
                    if (tw->KeyBufIn - tw->KeyBuf >= tw->KeyBufSize)
                        tw->KeyBufIn = tw->KeyBuf; /* wrap around */
                }
            }
            return(0);
        case WM_PAINT:
            {
            POINT source, width, dest;
            hdc = BeginPaint(hwnd, &ps);
            SelectFont(hdc, tw->hfont);
            SetMapMode(hdc, MM_TEXT);
            SetBkMode(hdc,OPAQUE);
            GetClientRect(hwnd, &rect);
            source.x = (rect.left + tw->ScrollPos.x) / tw->CharSize.x; /* source */
            source.y = (rect.top + tw->ScrollPos.y) / tw->CharSize.y;
            dest.x = source.x * tw->CharSize.x - tw->ScrollPos.x; /* destination */
            dest.y = source.y * tw->CharSize.y - tw->ScrollPos.y;
            width.x = ((rect.right  + tw->ScrollPos.x + tw->CharSize.x - 1) / tw->CharSize.x) - source.x; /* width */
            width.y = ((rect.bottom + tw->ScrollPos.y + tw->CharSize.y - 1) / tw->CharSize.y) - source.y;
            if (source.x < 0)
                    source.x = 0;
            if (source.y < 0)
                    source.y = 0;
            if (source.x+width.x > tw->ScreenSize.x)
                    width.x = tw->ScreenSize.x - source.x;
            if (source.y+width.y > tw->ScreenSize.y)
                    width.y = tw->ScreenSize.y - source.y;
            /* for each line */
            while (width.y>0) {
                    TextOutW(hdc,dest.x,dest.y,
                        (LPCWSTR)(tw->ScreenBuffer + source.y*tw->ScreenSize.x + source.x),
                        width.x);
                    dest.y += tw->CharSize.y;
                    source.y++;
                    width.y--;
            }
            EndPaint(hwnd, &ps);
            return 0;
            }
        case WM_DROPFILES:
            text_drag_drop(tw, (HDROP)wParam);
            break;
        case WM_CREATE:
            {
            RECT crect, wrect;
            int cx, cy;

            tw->hwnd = hwnd;

            /* make window no larger than screen buffer */
            GetWindowRect(hwnd, &wrect);
            GetClientRect(hwnd, &crect);
            cx = min(tw->CharSize.x*tw->ScreenSize.x, crect.right);
            cy = min(tw->CharSize.y*tw->ScreenSize.y, crect.bottom);
            MoveWindow(hwnd, wrect.left, wrect.top,
                 wrect.right-wrect.left + (cx - crect.right),
                 wrect.bottom-wrect.top + (cy - crect.bottom),
                     TRUE);

            if ( (tw->DragPre!=(LPSTR)NULL) && (tw->DragPost!=(LPSTR)NULL) )
                DragAcceptFiles(hwnd, TRUE);
            }
            break;
        case WM_CLOSE:
            /* Tell user that we heard them */
            if (!tw->quitnow) {
                TCHAR title[256];
                int count = GetWindowText(hwnd, title,
                                          sizeof(title)/sizeof(TCHAR)-11);
                lstrcpyW(title+count, L" - closing");
                SetWindowText(hwnd, title);
            }
            tw->quitnow = TRUE;
            /* wait until Ghostscript exits before destroying window */
            return 0;
        case WM_DESTROY:
            DragAcceptFiles(hwnd, FALSE);
            if (tw->hfont)
                DeleteFont(tw->hfont);
            tw->hfont = (HFONT)0;
            tw->quitnow = TRUE;
            PostQuitMessage(0);
            break;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

HWND text_get_handle(TW *tw)
{
    return tw->hwnd;
}

#ifdef NOTUSED
/* test program */

int PASCAL
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
        TW *tw;

        /* make a test window */
        tw = text_new();

        if (!hPrevInstance) {
            HICON hicon = LoadIcon(NULL, IDI_APPLICATION);
            text_register_class(tw, hicon);
        }
        text_font(tw, "Courier New", 10);
        text_size(tw, 80, 80);
        text_drag(tw, "(", ") run\r");

        /* show the text window */
        if (!text_create(tw, "Application Name", nCmdShow)) {
            /* do the real work here */
            /* TESTING */
            int ch;
            int len;
            char line[256];
            while ( (len = text_read_line(tw, line, 256-1)) != 0 ) {
                text_write_buf(tw, line, len);
            }
/*
            while ( text_gets(tw, line, 256-1) ) {
                text_puts(tw, line);
            }
*/
/*
            while ( (ch = text_getch(tw, )) != 4 )
                text_putch(tw, ch);
*/
        }
        else {

        }

        /* clean up */
        text_destroy(tw);

        /* end program */
        return 0;
}
#endif
