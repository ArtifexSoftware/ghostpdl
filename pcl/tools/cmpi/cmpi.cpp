///////////////////////////////////////////////////////////////////////////////
// Name:        samples/cmpi/cmpi.cpp
// Purpose:     Compare images with 'fuzzy' options
// Author:      Ray Johnston
// Modified by:
// Created:     2006
// RCS-ID:      $Id:$
// Copyright:   (c) 2006-2021 Artifex Software Inc.
// License:     AFPL
///////////////////////////////////////////////////////////////////////////////

#include <math.h>

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "wx/defs.h"
#include "wx/image.h"
#include "wx/file.h"
#include "wx/filename.h"
#include "wx/filefn.h"
#include "wx/mstream.h"
#include "wx/wfstream.h"
#include "wx/pen.h"
#include "wx/progdlg.h"
#include "wx/msgdlg.h"

// derived classes

class MyFrame;
class MyApp;

// MyCanvas

class MyCanvas: public wxScrolledWindow
{
public:
    MyCanvas() {}
    MyCanvas( wxWindow *parent, wxWindowID, const wxPoint &pos, const wxSize &size );
    ~MyCanvas();
    void OnPaint( wxPaintEvent &event );
    bool ProcessArgs(int argc, wxChar **argv, wxStatusBar *S);
    void update_bitmap();
    void goto_diff_rect_focus();
    bool pixel_differs( unsigned char **base_rows, unsigned char **compare_rows, int pixel,
			int i_pix_window, int color_tolerance);
    void prune_pending_list( int row );
    bool load_image_files( wxChar *basefilename, wxChar *comparefilename );

    wxImage base_image, compare_image, diff_image, *current_image, *prev_image;
    wxStatusBar *frame_status_bar;
    wxChar *basefn, *comparefn;
    wxBitmap *current_bitmap;
    float	current_zoom, prev_zoom;
    int i_pix_window;
    int i_rect_separation;
    int *neighbor_pix, *neighbor_row, neighbor_count;
    int color_tolerance;
    int diff_count;
    bool batch_mode;
    unsigned char *alpha;
    bool mask_on, prev_mask;
    bool highlight_on;

    typedef struct diff_rect_s {
	int left, top, right, bottom;
	struct diff_rect_s *next, *prev;
    } diff_rect;

    diff_rect *diff_list_head, *diff_list_tail, *diff_list_pending;
    diff_rect *diff_rect_focus;
    int diff_list_count, i_diff_rect_focus;

protected:

private:
    DECLARE_DYNAMIC_CLASS(MyCanvas)
    DECLARE_EVENT_TABLE()
};


// MyFrame


class MyFrame: public wxFrame
{
public:
	MyFrame();

    bool ProcessArgs(int argc, wxChar **argv);
    void OnAbout( wxCommandEvent &event );
    void OnHelp( wxCommandEvent &event );
    void OnNewImages( wxCommandEvent &event );
    void OnQuit( wxCommandEvent &event );
    void OnHiLiteToggle( wxCommandEvent &event );
    void OnMaskToggle( wxCommandEvent &event );
    void OnView_Image( wxCommandEvent &event );
    void OnGoTo( wxCommandEvent &event );
    void OnZoom( wxCommandEvent &event );
    void OnSettings( wxCommandEvent &event );

    MyCanvas	*m_canvas;
    wxStatusBar	*status_bar;

private:
    DECLARE_DYNAMIC_CLASS(MyFrame)
    DECLARE_EVENT_TABLE()
};


// MyApp

class MyApp: public wxApp
{
public:
    virtual bool OnInit();
};

// main program

IMPLEMENT_APP(MyApp)

// MyCanvas

IMPLEMENT_DYNAMIC_CLASS(MyCanvas, wxScrolledWindow)

BEGIN_EVENT_TABLE(MyCanvas, wxScrolledWindow)
  EVT_PAINT(MyCanvas::OnPaint)
END_EVENT_TABLE()

MyCanvas::MyCanvas( wxWindow *parent, wxWindowID id,
                    const wxPoint &pos, const wxSize &size )
        : wxScrolledWindow( parent, id, pos, size, wxSUNKEN_BORDER )
{
    SetBackgroundColour(* wxWHITE);
    current_bitmap = (wxBitmap *)NULL;
    current_image = (wxImage *)NULL;
    prev_image = current_image;
    frame_status_bar = (wxStatusBar *)NULL;
    neighbor_count = 0;
    neighbor_pix = neighbor_row = NULL;
}

// Called when zoom factor changes to create bitmap of correct size
void
MyCanvas::update_bitmap()
{
    int current_scroll_x, current_scroll_y, sx, sy, client_width_pixels, client_height_rows;

    if ((current_image == prev_image) && (current_zoom == prev_zoom) && (mask_on == prev_mask))
	return;

    if (current_bitmap) {
	delete current_bitmap;
    }
    GetClientSize( &client_width_pixels, &client_height_rows );
    GetViewStart( &current_scroll_x, &current_scroll_y );
    /* Keep center of focus constant as we zoom in and out */
    /* These calculations could be chained, but this is easier for debugging */
    sx = current_scroll_x + ( client_width_pixels / 2 );
    sy = current_scroll_y + ( client_height_rows / 2 );
    sx = sx * ( current_zoom / prev_zoom );
    sy = sy * ( current_zoom / prev_zoom );
    sx = sx - ( client_width_pixels / 2 );
    sy = sy - ( client_height_rows / 2 );
    sx = ( sx < 0 ) ? 0 : sx;
    sy = ( sy < 0 ) ? 0 : sy;
    prev_image = current_image;
    prev_zoom = current_zoom;
    prev_mask = mask_on;
    current_bitmap = new wxBitmap( 
	    current_image->Scale( current_image->GetWidth() * current_zoom, 
	    current_image->GetHeight() * current_zoom)
	    );
    SetScrollbars( 1, 1, current_bitmap->GetWidth(), current_bitmap->GetHeight(), sx, sy );
    
    wxChar *fn;
    char *hdr;
    if (current_image == &base_image) {
	hdr = "Base Image: ";
	fn = basefn;
    } else if (current_image == &compare_image) {
	hdr = "Compare Image: ";
	fn = comparefn;
    } else {
	hdr = "Diff Image";
	fn = _T("");
    }
#if wxUSE_STATUSBAR
    frame_status_bar->SetStatusText(wxString(hdr, wxConvUTF8), 0);
    frame_status_bar->SetStatusText(wxString(fn, wxConvUTF8), 1);
#endif // wxUSE_STATUSBAR
    Refresh();
}

void
MyCanvas::goto_diff_rect_focus( )
{
    int scroll_units_pixels, scroll_units_rows, client_width_pixels, client_height_rows;

    /* make the center of the current diff rect be at the center of the window */
    int i_rect_center_pixel = ( diff_rect_focus->left + diff_rect_focus->right ) / 2;
    int i_rect_center_row =  ( diff_rect_focus->top + diff_rect_focus->bottom ) / 2;

    GetScrollPixelsPerUnit( &scroll_units_pixels, &scroll_units_rows );
    GetClientSize( &client_width_pixels, &client_height_rows );

    int scroll_to_pixel = ( (current_zoom * i_rect_center_pixel) - ( client_width_pixels / 2 ) );
    int scroll_to_row = ( current_zoom * i_rect_center_row - ( client_height_rows / 2 ) );

    Scroll(   scroll_to_pixel / scroll_units_pixels, scroll_to_row / scroll_units_rows );
    Refresh();
}


#define MAX_PIXWINDOW 8

bool
MyCanvas::pixel_differs( unsigned char **base_rows, unsigned char **compare_rows, int pixel, int i_pw, int color_tolerance)
{

    /* rows are pointers to the data to be compared */
    /* the current pixel is in ***_rows[i_pw][3*i_pw] */
    /* color_tolerance is an absolute diff in each channel R, G, B */
    int p3 = (pixel * 3);		/* center pixel offset in row */
	int i;
    bool base_differs = true;							/* default to differing */

    /* This row in _rows is i_pw */
    /* Check pixels from the center out */
    unsigned char *cp, *bp = base_rows [i_pw] + p3;		/* reference is base */
    for (i=0; i<neighbor_count; i++) {
	cp = compare_rows [i_pw + neighbor_row[i]] + p3 + (3 * neighbor_pix[i]);
	if (abs(bp[0] - cp[0]) <= color_tolerance &&
	    abs(bp[1] - cp[1]) <= color_tolerance &&
	    abs(bp[2] - cp[2]) <= color_tolerance ) {
	    if (i == 0)
		    return false;	/* matches at current pixel */
	    base_differs = false;
	    break;				/* found a match */
	}
    }
    if (base_differs)
	return true;			/* bail out now */
    /* Need to make sure that this isn't a missing feature */
    cp = compare_rows [i_pw] + p3;		/* reference is compare */
    for (i=0; i<neighbor_count; i++) {
	bp = base_rows [i_pw + neighbor_row[i]] + p3 + (3 * neighbor_pix[i]);
	if (abs(bp[0] - cp[0]) <= color_tolerance &&
	    abs(bp[1] - cp[1]) <= color_tolerance &&
	    abs(bp[2] - cp[2]) <= color_tolerance ) {
	    return false;
	}
    }
    return true;
}

void
MyCanvas::prune_pending_list( int row )
{
    diff_rect *diff_pending_scan;
    diff_rect *diff_pending_scan_base = diff_list_pending;

    /* First anneal any rects on the pending list that are close enough */
    /* This is done first since this list is active for every row */
    while ( (diff_pending_scan_base != NULL) && (diff_pending_scan_base->next != NULL) ) {
        diff_pending_scan = diff_pending_scan_base->next;
        while (diff_pending_scan != NULL) {
            if ( ( diff_pending_scan_base->right >= (diff_pending_scan->left - i_rect_separation) ) &&
                 ( diff_pending_scan_base->left <= (diff_pending_scan->right + i_rect_separation) ) ) {
                /* merge the _scan rect into the _base rect and delete the _scan element */
                 diff_pending_scan_base->left = diff_pending_scan_base->left < diff_pending_scan->left ? 
                     diff_pending_scan_base->left : diff_pending_scan->left;
                 diff_pending_scan_base->right = diff_pending_scan_base->right > diff_pending_scan->right ? 
                     diff_pending_scan_base->right : diff_pending_scan->right;
                 diff_pending_scan_base->top = diff_pending_scan_base->top < diff_pending_scan->top ? 
                     diff_pending_scan_base->top : diff_pending_scan->top;
                 diff_pending_scan_base->bottom = diff_pending_scan_base->bottom > diff_pending_scan->bottom ? 
                     diff_pending_scan_base->bottom : diff_pending_scan->bottom;
                 /* delete the diff_pending_scan rectangle from the list */
                 diff_pending_scan->prev->next = diff_pending_scan->next;
                 if (diff_pending_scan->next != NULL)
                     diff_pending_scan->next->prev = diff_pending_scan->prev;
                 diff_rect *prev = diff_pending_scan->prev;
                 free( diff_pending_scan );
                 diff_pending_scan = prev;
            }
            diff_pending_scan = diff_pending_scan->next;
        }
        diff_pending_scan_base = diff_pending_scan_base->next;
    }
    /* Remove any pending differences that will be too far above the current row */
    /* Before adding it, we need to make sure we can't simply anneal to an existing diff */
    diff_pending_scan = diff_list_pending;

    while (diff_pending_scan != NULL) {
        if (diff_pending_scan->bottom <= (row - i_rect_separation)) {
	    bool diff_pending_merged = false;
	    diff_rect *diff_list_scan = diff_list_head;

	    while (diff_list_scan != NULL) {
		if ( ( diff_list_scan->bottom >= diff_pending_scan->top - i_rect_separation ) &&
		     ( ( diff_list_scan->right >= (diff_pending_scan->left - i_rect_separation) ) &&
		       ( diff_list_scan->left <= (diff_pending_scan->right + i_rect_separation) )
		     )
		   ) {
		    /* merge the diff_pending_scan rect into the diff_list_scan rect */
		    diff_list_scan->left = diff_list_scan->left < diff_pending_scan->left ? 
			diff_list_scan->left : diff_pending_scan->left;
		    diff_list_scan->right = diff_list_scan->right > diff_pending_scan->right ? 
			diff_list_scan->right : diff_pending_scan->right;
		    diff_list_scan->top = diff_list_scan->top < diff_pending_scan->top ? 
			diff_list_scan->top : diff_pending_scan->top;
		    diff_list_scan->bottom = diff_list_scan->bottom > diff_pending_scan->bottom ? 
			diff_list_scan->bottom : diff_pending_scan->bottom;

		     /* delete the diff_pending_scan rectangle from the list */
		    if (diff_pending_scan->prev != NULL)
			diff_pending_scan->prev->next = diff_pending_scan->next;
		    else
			diff_list_pending = diff_pending_scan->next;
		    if (diff_pending_scan->next != NULL)
			 diff_pending_scan->next->prev = diff_pending_scan->prev;
		    diff_rect *next = diff_pending_scan->next;
		    free( diff_pending_scan );
		    diff_pending_scan = next;
		    diff_pending_merged = true;
		    break;
	    	}
		diff_list_scan = diff_list_scan->next;
	    }

	    if ( ! diff_pending_merged ) {
		/* move this rect from the pending list to the end of the diff_list */
		if (diff_pending_scan->prev != NULL) 
		    /* not the first on the pending list */
		    diff_pending_scan->prev->next = diff_pending_scan->next;
		else
		    diff_list_pending = diff_pending_scan->next;
		if (diff_pending_scan->next != NULL)
		    /* wasn't the last on the pending list */
		    diff_pending_scan->next->prev = diff_pending_scan->prev;
		/* now put this on the diff_list at the tail */
		if (diff_list_tail == NULL) {
		    diff_list_tail = diff_list_head = diff_pending_scan;
		    diff_pending_scan->prev = NULL;
		} else {
		    diff_list_tail->next = diff_pending_scan;
		    diff_pending_scan->prev = diff_list_tail;
		    diff_list_tail = diff_pending_scan;
		}
		diff_pending_scan = diff_pending_scan->next;
		diff_list_tail->next = NULL;
		diff_list_count++;
		 /* finished moving a rect from 'pending' to the diff_list */    
	    } /* end if ! diff_pending_merged */
	} else {
	    diff_pending_scan = diff_pending_scan->next;
	} /* end if diff_pending rect ends too far above current row  */
    } /* end while diff_pending_scan != NULL */
}

bool
MyCanvas::load_image_files( wxChar *basefilename, wxChar *comparefilename )
{
    char str[256];
    int i, percent_done = 0;
    
    wxProgressDialog *wxPD = new wxProgressDialog(wxT("Loading and Analyzing Images"),
		wxT("________________________________________________________________"),
		100, 0, wxPD_AUTO_HIDE );
    sprintf(str, "Loading baseline image file: %-64s", basefilename);
    wxPD->Update( 0, wxString(str, wxConvUTF8));
    if ( ! base_image.LoadFile( basefilename, wxBITMAP_TYPE_ANY ) ) {
	wxLogError(wxT("Can't load baseline image"));
	return false;
    }
    else {
	sprintf(str, "Loading compare image file: %-64s", basefilename);
	wxPD->Update( 25, wxString(str, wxConvUTF8));
	if ( ! compare_image.LoadFile( comparefilename, wxBITMAP_TYPE_ANY )){
	    wxLogError(wxT("Can't load compare image"));
	    return false;
	}
    }
    if (compare_image.GetWidth() != base_image.GetWidth() ||
	compare_image.GetHeight() != base_image.GetHeight()) {
	wxLogError(wxT("compare image geometry doesn't match base image"));
	return false;
    }


    basefn = basefilename;
    comparefn = comparefilename;
    current_image = &base_image;
    base_image.SetAlpha ();
    compare_image.SetAlpha ();
    memset (base_image.GetAlpha(), 255, base_image.GetWidth() * base_image.GetHeight());
    memset (compare_image.GetAlpha(), 255, base_image.GetWidth() * base_image.GetHeight());

    // Compute 'fit' zoom factor
    int frameW, frameH;
    DoGetClientSize(&frameW, &frameH);
    float WRatio = (float)frameW / (float)(base_image.GetWidth());
    float HRatio = (float)frameH / (float)(base_image.GetHeight());
    current_zoom = WRatio < HRatio ? WRatio : HRatio;
    prev_zoom = current_zoom;		/* save the value for scroll positioning when zooming */

    // Gather differences
    unsigned char *base_data = base_image.GetData();
    unsigned char *compare_data = compare_image.GetData();
    bool differs;
    int pixel, row;
    int image_width = base_image.GetWidth();
    int image_height = base_image.GetHeight();
    unsigned char *diff_data = (unsigned char *)malloc(image_width * image_height *3);
    unsigned char *p_base_rows[MAX_PIXWINDOW];
    unsigned char *p_compare_rows[MAX_PIXWINDOW];

    diff_list_head = diff_list_tail = diff_list_pending = (diff_rect *)NULL;
    diff_rect_focus = NULL;
    i_diff_rect_focus = 0;
    diff_list_count = 0;
    alpha = (unsigned char *)calloc(image_height * image_width, 1);
    mask_on = false;
    highlight_on = false;
    prev_mask = mask_on;
    memset(diff_data, 255, image_width * image_height * 3);
    memset(alpha, 30, image_width * image_height);

    /* generate the list of neighbors dynamically */
    neighbor_count = 1;
    for (i=1; i <= i_pix_window; i++)
	neighbor_count += 8 * i;
    neighbor_pix = (int *)calloc(neighbor_count, sizeof(int));
    neighbor_row = (int *)calloc(neighbor_count, sizeof(int));
    neighbor_count = 1;
    for (i=1; i <= i_pix_window; i++) {
	for (row = -i; row < i; row++, neighbor_count++) {	/* lower right, up */
	    neighbor_pix[neighbor_count] = i;
	    neighbor_row[neighbor_count] = row;
	}
	for (pixel = i; pixel > -i; pixel--, neighbor_count++) {	/* upper right, left */
	    neighbor_pix[neighbor_count] = pixel;
	    neighbor_row[neighbor_count] = i;
	}
	for (row = i; row > -i; row--, neighbor_count++) {	/* lower right, up */
	    neighbor_pix[neighbor_count] = -i;
	    neighbor_row[neighbor_count] = row;
	}
	for (pixel = -i; pixel < i; pixel++, neighbor_count++) {	/* upper right, left */
	    neighbor_pix[neighbor_count] = pixel;
	    neighbor_row[neighbor_count] = -i;
	}
    }

    for (row=0; row < image_height; row++) {
	    
	if ( 25 + ((row * 75)/ image_height) > percent_done ) {
	    /* update progress dialog */
	    percent_done = 25 + ( ( row * 75 )/ image_height );
	    sprintf(str, "      Analyzing Differences: %d %%", percent_done);
	    wxPD->Update( percent_done, wxString(str, wxConvUTF8));
	}

	int i;
	for (i = -i_pix_window; i <= i_pix_window; i++) {
	    int row_offset = (row + i) * image_width * 3;
	    p_base_rows[i+i_pix_window] = base_data + row_offset;
	    p_compare_rows[i+i_pix_window] = compare_data + row_offset;
	}

	for (pixel=0; pixel < image_width; pixel++) {
	    if ( (pixel < i_pix_window) || (pixel >= (image_width - i_pix_window)) || 
		(row < i_pix_window) || (row >= (image_height - i_pix_window)) ) {
		/* Need to copy a range of data to our "window" filling in rows and */
		/* columns outside of the image */
		// ------------------------------------------------------------- //
		//
		//  THIS SECTION T.B.I. BUT IT ONLY AFFECTS DIFFERENCES AT THE
		//	EDGE OF THE IMAGES, SO WE LEAVE IT FOR LATER.
		//
		//  The approach is to copy the data to a 'window' of rows that
		//	is 1+2*i_pix_window square and pass those row pointers to
		//	pixel_differs()

		/* Now call the comparator on the window we just filled in */

		differs = false;
	    }
	    else {
		differs = pixel_differs( p_base_rows, p_compare_rows, pixel, i_pix_window, color_tolerance);
	    }
	    if (differs) {
		unsigned char *d = diff_data + 3 * (pixel + (row * image_width));
		d[0] = d[1] = d[2] = 0;
		alpha[pixel + (row * image_width)] = 255;
		diff_count++;

		/* Now maintain the list of diff_rects */
		bool diff_processed = false;
		diff_rect *diff_rect_scan = diff_list_pending;

		while (diff_rect_scan != NULL) {
		    /* we already know that any rects still on the pending list are */
		    /* within the row i_rect_separation distance -- just check the pixel distance */
		    if ( (pixel >= (diff_rect_scan->left - i_rect_separation)) &&
			(pixel <= (diff_rect_scan->right + i_rect_separation)) ) {
			/* update thebottom to the current row. Set right and left if past current pixel */
			diff_rect_scan->right = (pixel > diff_rect_scan->right) ? pixel : diff_rect_scan->right ;
			diff_rect_scan->left = (pixel < diff_rect_scan->left) ? pixel : diff_rect_scan->left ;
			diff_rect_scan->bottom = row;
			diff_processed = true;
			break;
		    }
		    /* if not at end of list, advance to next */
		    if (diff_rect_scan->next == NULL) {
			break;			/* exit loop with _scan at tail of list */
			/* the above exit at end of list will have diff_processed == false */
		    }
		    diff_rect_scan = diff_rect_scan->next;
		}
		if ( ! diff_processed ) {
		    /* This difference wasn't on the 'pending' list -- add a new rect at the head */
		    diff_rect *diff_rect_new = (diff_rect *)calloc(1, sizeof(diff_rect));
		    diff_rect_new->right = diff_rect_new->left = pixel;
		    diff_rect_new->bottom = diff_rect_new->top = row;
		    diff_rect_new->next = diff_list_pending;
		    if (diff_list_pending != NULL) {
			diff_list_pending->prev = diff_rect_new;	/* link back to new head of list */
		    }
		    diff_list_pending = diff_rect_new;				/* new head of list */
		}
	    } /* finished handling a pixel that differs */
	} /* end of pixels in a row */

	prune_pending_list( row );
    } /* end of rows in image */

    /* Move any rects still pending to the diff_list */
    prune_pending_list( row + i_rect_separation + 1 );	/* far enoungh beyond all pending */

    wxPD->Close();
    wxPD->Destroy();
    diff_image.Create(image_width, image_height, diff_data, NULL, false);
    free(neighbor_pix);
    free(neighbor_row);

    return true;
}

bool
MyCanvas::ProcessArgs(int argc=0, wxChar **argv=NULL, wxStatusBar *status_bar=NULL)
{

    int atarg = 1;
    char *logfilename = "cmpi.log";

    frame_status_bar = status_bar;
    i_pix_window = 3;		/* Default is sort of loose */
    color_tolerance = 5;	/* also for color variations */
    diff_count = 0;
    i_rect_separation = 3 * i_pix_window;

    batch_mode = false;
    while ((atarg < argc) && (argv[atarg][0] == '-')) {
	/* process options */
	switch(argv[atarg][1]) {
	    case 'b':
		batch_mode = true;
		break;
	    case 'c':
		sscanf((char *)argv[atarg]+2, "%d", &color_tolerance);
		if (color_tolerance < 0)
		    color_tolerance = 0;
		break;
	    case 'l':
		if (argv[atarg]+2 != 0)
			logfilename = (char *)argv[atarg]+2;
		else {
			atarg++;
			logfilename = (char *)argv[atarg];
		}
		break;
	    case 'r':
		sscanf((char *)argv[atarg]+2, "%d", &i_rect_separation);
		if (i_rect_separation < 0)
		    i_rect_separation = 0;
		break;
	    case 'w':
		sscanf((char *)argv[atarg]+2, "%d", &i_pix_window);
		if (i_pix_window < 0)
		    i_pix_window = 0;
		break;
	    default:
		wxLogError(wxT("Unknown option"));
	}
	atarg++;
    }
    if (argc - atarg < 2) {
	wxLogError(wxT("Two filename arguments are required"));
	return false;
    }
    if ( ! load_image_files( argv[atarg], argv[atarg+1] ) )
	return false;

    if (batch_mode) {
	char str[256], basename[256];
	wxFile logFile;

	if (wxFileExists(logfilename))
		logFile.Open(logfilename, wxFile::write_append);
	else
	    logFile.Create(logfilename);

	logFile.SeekEnd();

	/* find the "basename .p?m" from the base image name */
	int end = strlen(argv[atarg]) - 1;
	while ( argv[atarg][end--] != '.' && end > 0 )
	    ;
	if ( end == 0 )
		end = strlen(argv[atarg]) - 1;
	int start = end++;
	while ( argv[atarg][start] != '/' && argv[atarg][start] != '\\' &&
		argv[atarg][start] != ':' && start > 0 )
	    start--;
	strncpy(basename, argv[atarg]+start, end-start);
	basename[end-start] = 0;
	sprintf(str, "%s\tDifference count: %d\n", basename, diff_count);
	logFile.Write(wxString(str, wxConvUTF8));
	logFile.Close();
	return false;
    }

#if wxUSE_STATUSBAR
    char str[32];
    sprintf( str, "Rect Count: %-10d", diff_list_count);
    frame_status_bar->SetStatusText(wxString(str, wxConvUTF8), 2);
    sprintf( str, "Diff Count: %-10d", diff_count);
    frame_status_bar->SetStatusText(wxString(str, wxConvUTF8), 3);
#endif // wxUSE_STATUSBAR

    update_bitmap();

    return true;
}

MyCanvas::~MyCanvas()
{
    base_image.Destroy();
	compare_image.Destroy();
	delete current_bitmap;
}

void MyCanvas::OnPaint( wxPaintEvent &WXUNUSED(event) )
{
    wxPaintDC dc( this );
    PrepareDC( dc );

    if (current_bitmap && current_bitmap->Ok()) {
	diff_rect *diff_rect_scan;

	dc.DrawBitmap( *current_bitmap, 0, 0 );
	
	/* now outline the difference rects */
	if ( highlight_on ) {
	    dc.SetBrush( *wxTRANSPARENT_BRUSH );

	    for (diff_rect_scan = diff_list_head; diff_rect_scan != NULL; diff_rect_scan = diff_rect_scan->next) {
		int w = current_zoom * (diff_rect_scan->right - diff_rect_scan->left + 1);
		int h = current_zoom * (diff_rect_scan->bottom - diff_rect_scan->top + 1);
		bool focus = diff_rect_scan == diff_rect_focus;

		dc.SetPen( wxPen( focus ? *wxBLUE : *wxRED, focus ? 4 : 2, wxSOLID ) );
		dc.DrawRectangle( current_zoom * diff_rect_scan->left, current_zoom * diff_rect_scan->top,
		    (w <= 0) ? 1 : w, (h <= 0) ? 1 : h );
	    } /* end for diff_rect_scan */
	} /* end if highlight_on */
    }
}

// MyFrame

enum
{
    ID_QUIT  = wxID_EXIT,
    ID_ABOUT = wxID_ABOUT,
    ID_HELP = wxID_HELP,
    ID_NEW = 100,
    ID_BASE = 101,
    ID_COMPARE = 102,
    ID_DIFF = 103,
    ID_HILITE = 104,
    ID_MASK = 105,
    ID_GOTO_NEXT = 106,
    ID_GOTO_PREV = 107,
    ID_ZOOM_FIT = 108,
    ID_ZOOM_1 = 109,
    ID_ZOOM_IN = 110,
    ID_ZOOM_OUT = 111,
    ID_PIXEL_WINDOW = 112,
    ID_COLOR_TOLERANCE = 113,
};

IMPLEMENT_DYNAMIC_CLASS( MyFrame, wxFrame )

BEGIN_EVENT_TABLE(MyFrame,wxFrame)
    EVT_MENU    (ID_ABOUT, MyFrame::OnAbout)
    EVT_MENU    (ID_HELP, MyFrame::OnHelp)
    EVT_MENU    (ID_QUIT,  MyFrame::OnQuit)
    EVT_MENU    (ID_NEW,  MyFrame::OnNewImages)
    EVT_MENU    (ID_BASE,  MyFrame::OnView_Image)
    EVT_MENU    (ID_COMPARE,  MyFrame::OnView_Image)
    EVT_MENU    (ID_DIFF,  MyFrame::OnView_Image)
    EVT_MENU    (ID_HILITE,  MyFrame::OnHiLiteToggle)
    EVT_MENU    (ID_MASK,  MyFrame::OnMaskToggle)
    EVT_MENU    (ID_GOTO_NEXT,  MyFrame::OnGoTo)
    EVT_MENU    (ID_GOTO_PREV,  MyFrame::OnGoTo)
    EVT_MENU    (ID_ZOOM_FIT,  MyFrame::OnZoom)
    EVT_MENU    (ID_ZOOM_1,  MyFrame::OnZoom)
    EVT_MENU    (ID_ZOOM_IN,  MyFrame::OnZoom)
    EVT_MENU    (ID_ZOOM_OUT,  MyFrame::OnZoom)
    EVT_MENU    (ID_PIXEL_WINDOW,  MyFrame::OnSettings)
    EVT_MENU    (ID_COLOR_TOLERANCE,  MyFrame::OnSettings)
END_EVENT_TABLE()

MyFrame::MyFrame()
       : wxFrame( (wxFrame *)NULL, wxID_ANY, _T("cmpi - fuzzy image comparator"),
                  wxPoint(20,20), wxSize(1200,900) )
{
    status_bar = (wxStatusBar *)NULL;
    wxMenuBar *menu_bar = new wxMenuBar();
    wxMenu *menuFile = new wxMenu;
    menuFile->Append( ID_NEW, _T("&Open Image Files...\tCtrl-O"));

    menuFile->AppendSeparator();
    menuFile->Append( ID_QUIT, _T("E&xit\tCtrl-Q"));
    menu_bar->Append(menuFile, _T("&File"));

    wxMenu *menuView = new wxMenu;
    menuView->Append( ID_BASE, _T("&Base Image\tb"));
    menuView->Append( ID_COMPARE, _T("&Compare Image\tc"));
    menuView->Append( ID_DIFF, _T("&Diff Image\td"));
    menuView->AppendSeparator();
    menuView->Append( ID_HILITE, _T("&Highlight Toggle\th"));
    menuView->Append( ID_MASK, _T("&Mask Toggle Image\tm"));
    menuView->AppendSeparator();
    menuView->Append( ID_GOTO_NEXT, _T("&Next Difference\tn"));
    menuView->Append( ID_GOTO_PREV, _T("&Previous Difference\tp"));
    menuView->AppendSeparator();
    menuView->Append( ID_ZOOM_FIT, _T("Zoom &Fit in Window\tf"));
    menuView->Append( ID_ZOOM_1, _T("Zoom &1:1\t1"));
    menuView->Append( ID_ZOOM_IN, _T("Zoom &In\t+"));
    menuView->Append( ID_ZOOM_OUT, _T("Zoom &Out\t-"));
    menu_bar->Append(menuView, _T("&View"));

    wxMenu *menuSettings = new wxMenu;
    menuSettings->Append( ID_PIXEL_WINDOW, _T("Pixel &Window ...\tCtrl-W"));
    menuSettings->Append( ID_COLOR_TOLERANCE, _T("Color &Tolerance...\tCtrl-T"));
    menu_bar->Append(menuSettings, _T("&Settings"));

    wxMenu *menuHelp = new wxMenu;
    menuHelp->Append( ID_ABOUT, _T("&About..."));
    menuHelp->Append( ID_HELP, _T("&Help\tF1"));
    menu_bar->Append(menuHelp, _T("&Help"));

    SetMenuBar( menu_bar );

#if wxUSE_STATUSBAR
    status_bar = CreateStatusBar(4);
    int widths[] = { 150, -1, 200, 200 };
    SetStatusWidths( 4, widths );
#endif // wxUSE_STATUSBAR

    m_canvas = new MyCanvas( this, wxID_ANY, wxPoint(0,0), wxSize(10,10) );
}

bool MyFrame::ProcessArgs(int argc, wxChar **argv)
{
    if (!m_canvas->ProcessArgs(argc, argv, status_bar))
	Close();
    return true;
}

void MyFrame::OnQuit( wxCommandEvent &WXUNUSED(event) )
{
  Close( true );
}

void MyFrame::OnAbout( wxCommandEvent &WXUNUSED(event) )
{
    (void)wxMessageBox( _T("cmpi - Image Comparison tool with fuzzy compare -- Version 0.9\n")
                      _T("Artifex Software (c) 2006"),
		      _T("Written by Ray Johnston"),
                      wxICON_INFORMATION | wxOK );
}

void MyFrame::OnHelp( wxCommandEvent &WXUNUSED(event) )
{
  (void)wxMessageBox( _T(
	"usage: cmpi [-w#] [-c#] [-r#] [-l logfilename] baseline.ppm compare.ppm\n"
	"\n"
	"    w# is pixel distance from the reference location 0 to 3\n"
	"    c# is the color tolerance (same for all components)\n"
	"    r# is the max rect separation used to collect adjacent\n"
	"       or nearby pixels into rectangular regions\n"
	"\n"
	"Hotkeys are:\n"
	"\n"
	"    b    display baseline image\n"
	"    c    display compare image\n"
	"    d    display diff image\n"
	"    f    fit page to window\n"
	"    h    highlight on/off toggle\n"
	"    m    mask on/off toggle (only for base or compare images)\n"
	"    n    next difference rectangle - center on screen, highlight blue\n"
	"    p    previous difference rectangle - center on screen, highlight blue\n"
	"    1    set zoom to 1:1\n"
	"    -    zoom out\n"
	"    +    zoom in\n"
	"\n"
	"    ctrl-q    quit\n"
					),
	  _T("Usage:"),
	  wxICON_INFORMATION | wxOK );
}

void MyFrame::OnNewImages( wxCommandEvent &WXUNUSED(event) )
{
#if wxUSE_FILEDLG
    unsigned int i;
    wxString filename = wxFileSelector(_T("Select base image file"));
    if ( !filename )
        return;

    wxChar *basefilename = (wxChar *)calloc(filename.Len(), sizeof(wxChar));
    for ( i=0; i < filename.Len(); i++)
	basefilename[i] = filename.GetChar(i);
    basefilename[i] = 0;

    filename = wxFileSelector(_T("Select compare image file"));
    if ( !filename )
        return;

    wxChar *comparefilename = (wxChar *)calloc(filename.Len(), sizeof(wxChar));
    for ( i=0; i < filename.Len(); i++)
	comparefilename[i] = filename.GetChar(i);
    comparefilename[i] = 0;

    m_canvas->load_image_files(basefilename, comparefilename);

#endif // wxUSE_FILEDLG
}

void MyFrame::OnView_Image( wxCommandEvent &event )
{
    MyCanvas *m = m_canvas;

    switch (event.GetId()) {
	case ID_COMPARE:
	    m->current_image = &m->compare_image;
	    break;
	case ID_DIFF:
	    m->current_image = &m->diff_image;
	    break;
	case ID_BASE:
	default:
	    m->current_image = &m->base_image;
	    break;
    }
    m->update_bitmap();
} 


void MyFrame::OnMaskToggle( wxCommandEvent &WXUNUSED(event) )
{
    MyCanvas *m = m_canvas;

    if ((m->current_image == &m->base_image) || (m->current_image == &m->compare_image)) {
	if ( ! m->mask_on ) {
	    memcpy(m->base_image.GetAlpha(), m->alpha, m->base_image.GetWidth() * m->base_image.GetHeight());
	    memcpy(m->compare_image.GetAlpha(), m->alpha, m->base_image.GetWidth() * m->base_image.GetHeight());
	    m->mask_on = true;
	} else {
	    memset (m->base_image.GetAlpha(), 255, m->base_image.GetWidth() * m->base_image.GetHeight());
	    memset (m->compare_image.GetAlpha(), 255, m->base_image.GetWidth() * m->base_image.GetHeight());
	    m->mask_on = false;
	}
	m->update_bitmap();
    }
}

void MyFrame::OnHiLiteToggle( wxCommandEvent &WXUNUSED(event) )
{
	m_canvas->highlight_on = ! m_canvas->highlight_on;
	m_canvas->Refresh();
}

void MyFrame::OnZoom( wxCommandEvent &event )
{
    MyCanvas *m = m_canvas;
    double log_2, log2_zoom;
    float WRatio, HRatio;
    int frameW, frameH;

    switch (event.GetId()) {
	case ID_ZOOM_IN:
	    log_2 = log(2.0);
	    log2_zoom = log(m->current_zoom) / log_2;

	    log2_zoom = ceil(log2_zoom + 0.00001);
	    m->current_zoom = exp(log2_zoom * log_2);
	    break;
	case ID_ZOOM_OUT:
	    log_2 = log(2.0);
	    log2_zoom = log(m->current_zoom) / log_2;

	    log2_zoom = floor(log2_zoom - 0.00001);
	    m->current_zoom = exp(log2_zoom * log_2);
	    break;
	case ID_ZOOM_FIT:
	    // Compute 'fit' zoom factor
	    DoGetClientSize(&frameW, &frameH);
	    WRatio = (float)frameW / (float)(m->base_image.GetWidth());
	    HRatio = (float)frameH / (float)(m->base_image.GetHeight());
	    m->current_zoom = WRatio < HRatio ? WRatio : HRatio;
	    break;
	case ID_ZOOM_1:
	default:
	    m->current_zoom = 1.0;
    }
    // Now scale the bitmap from the image
    m->update_bitmap();
}

void MyFrame::OnGoTo( wxCommandEvent &event )
{
    MyCanvas *m = m_canvas;

    if (m->diff_rect_focus == NULL) {
	m->diff_rect_focus = m->diff_list_head;
	m->i_diff_rect_focus = 1;        /* first rect */
    }
    else {
	switch (event.GetId()) {
	    case ID_GOTO_NEXT:
		if (m->diff_rect_focus->next != NULL) {
		    m->diff_rect_focus = m->diff_rect_focus->next;
		    m->i_diff_rect_focus++;
		}
		break;
	    case ID_GOTO_PREV:
		if (m->diff_rect_focus->prev != NULL) {
		    m->diff_rect_focus = m->diff_rect_focus->prev;
		    m->i_diff_rect_focus--;
		}
		break;
	    default:	/* 't' Center on current focus rect */
		break;
	}
    }
#if wxUSE_STATUSBAR
    char str[32];
	sprintf( str, "At: %d / %-10d", m->i_diff_rect_focus, m->diff_list_count);
    status_bar->SetStatusText(wxString(str, wxConvUTF8), 2);
#endif // wxUSE_STATUSBAR
    m->goto_diff_rect_focus();		/* Make this one the center */

}

void MyFrame::OnSettings( wxCommandEvent &event )
{
    MyCanvas *m = m_canvas;

#if 0000	/* TBI */

	wxMessageDialog *settings_dlg;

    settings_dlg = new WxMessageDialog(this, _T("Change Difference Settings\n"),
			_T("Pixel Window    Color Tolerance\n"),
                      wxCANCEL | wxOK );

    m_spin_pix_win = new wxSpinButton( settings_dlg, ID_SPIN_PIX_WIN, wxPoint( 20, 40 ) );
    m_spin_pix_win->SetRange( 0, MAX_PIXWINDOW );
    m_spin_pix_win->SetValue( m->i_pix_window );

    m_spin_col_tol = new wxSpinButton( settings_dlg, ID_SPIN_COL_TOL, wxPoint( 20, 80 ) );
    m_spin_col_tol->SetRange( 0, 255 );
    m_spin_col_tol->SetValue( m->color_tolerance );


    switch (event.GetId()) {
	case ID_PIXEL_WINDOW:
	    settings_dlg.SetFocus( m_spin_pix_win );
	    break;
	case ID_COLOR_TOLERANCE:
	    settings_dlg.SetFocus( m_spin_col_tol );
	    break;
	default:
	    break;
    }

    /* on "Find Diffs"  BUtton */
    if ( m_spin_window.GetValue != m->i_pix_window ||
	 m_spin_tolerance.GetValue != m->color_tolerance )

#endif // 0000
}

//-----------------------------------------------------------------------------
// MyApp
//-----------------------------------------------------------------------------

bool MyApp::OnInit()
{

#if wxUSE_PNM
    wxImage::AddHandler( new wxPNMHandler );
#endif

    MyFrame *frame = new MyFrame();
    frame->Show( true );

    return frame->ProcessArgs(MyApp::argc, MyApp::argv);
}
