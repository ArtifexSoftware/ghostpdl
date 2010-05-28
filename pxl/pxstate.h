/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* pxstate.h */
/* State definitions for PCL XL interpreter */

#ifndef pxstate_INCLUDED
#  define pxstate_INCLUDED

#include "gsmemory.h"
#include "pxgstate.h"
#include "pltop.h"

/* Define an abstract type for a font directory. */
#ifndef gs_font_dir_DEFINED
#  define gs_font_dir_DEFINED	
typedef struct gs_font_dir_s gs_font_dir;
#endif

/* Define an abstract type for a text enumerator. */
/* We only need this for the preallocated enumerator for the error page. */
#ifndef gs_show_enum_DEFINED
#  define gs_show_enum_DEFINED
typedef struct gs_show_enum_s gs_show_enum;
#endif

/* Define an abstract type for an image enumerator. */
#ifndef px_image_enum_DEFINED
#  define px_image_enum_DEFINED
typedef struct px_image_enum_s px_image_enum_t;
#endif

/* Define an abstract type for an pattern enumerator. */
#ifndef px_pattern_enum_DEFINED
#  define px_pattern_enum_DEFINED
typedef struct px_pattern_enum_s px_pattern_enum_t;
#endif

/* Define the type of the PCL XL state. */
#ifndef px_state_DEFINED
#  define px_state_DEFINED
typedef struct px_state_s px_state_t;
#endif

/* NB need a separate header file for media */
typedef struct px_media_s {
  pxeMediaSize_t ms_enum;
  const char *mname;
  short width, height;
  short m_left, m_top, m_right, m_bottom;
} px_media_t;

/* This structure captures the entire state of the PCL XL "virtual */
/* machine", except for graphics state parameters in the gs_state. */
struct px_state_s {

  gs_memory_t *memory;
	/* Hook back to client data, for callback procedures */
  void *client_data;
  gs_id known_fonts_base_id;

	/* Session state */

  pxeMeasure_t measure;
  gs_point units_per_measure;
  pxeErrorReport_t error_report;
  bool useciecolor;

	/* Pattern dictionary */
  px_dict_t session_pattern_dict;

	/* Page state */

  pxeOrientation_t orientation;
  pxeMediaSource_t media_source;
  gs_point media_dims;
  bool duplex;
  int copies;
  pxeDuplexPageMode_t duplex_page_mode;
  bool duplex_back_side;
  pxeMediaDestination_t media_destination;

  /* NB media needs reorganization. */
  pxeMediaType_t media_type;
  px_media_t media_with_dims;
  px_media_t *pm;
  pxeMediaSize_t media_size;
  short media_height;
  short media_width;

  int (*end_page)(px_state_t *pxs, int num_copies, int flush);
	/* Pattern dictionary */
  px_dict_t page_pattern_dict;
	/* Internal variables */
  bool have_page;		/* true if anything has been written on page */
	/* Cached values */
  gs_matrix initial_matrix;

	/* Data source */

  bool data_source_open;
  bool data_source_big_endian;
	/* Stream dictionary */
  px_dict_t stream_dict;
	/* Stream reading state */
  gs_string stream_name;
  int stream_level; /* recursion depth */
  struct sd2_ {
    byte *data;
    uint size;
  } stream_def;

	/* Font dictionary */
  px_dict_t font_dict;
  px_dict_t builtin_font_dict;
	/* Font/character downloading state */
  px_font_t *download_font;
  int font_format;
	/* Global structures */
  gs_font_dir *font_dir;
  px_font_t *error_page_font;

	/* Graphics state */

  gs_state *pgs;		/* PostScript graphics state */
  px_gstate_t *pxgs;
	/* Image/pattern reading state */
  px_image_enum_t *image_enum;
  px_pattern_enum_t *pattern_enum;

	/* Miscellaneous */
  struct db_ {
    byte *data;
    uint size;
  } download_bytes;		/* font/character/halftone data */
  gs_string download_string;	/* ditto */
  struct sp_n {
    int x;
    double y0, y1;
  } scan_point;			/* position when reading scan lines */

	/* We put the warning table and error line buffer at the end */
	/* so that the offsets of the scalars will stay small. */
#define px_max_error_line 120
  char error_line[px_max_error_line + 1]; /* for errors with their own msg */
#define px_max_warning_message 500
  uint warning_length;
  char warnings[px_max_warning_message + 1];
  /* ---------------- PJL state -------------------- */
  pl_interp_instance_t *pjls;
  /* ---------------- PCL state -------------------- */
  pl_interp_instance_t *pcls;
};

/* Allocate a px_state_t. */
px_state_t *px_state_alloc(gs_memory_t *);

/* Release a px_state_t */
void px_state_release(px_state_t *pxs);

/* Do one-time state initialization. */
void px_state_init(px_state_t *, gs_state *);

/* Define the default end-of-page procedure. */
int px_default_end_page(px_state_t *, int, int);

/* Clean up after an error or UEL. */
void px_state_cleanup(px_state_t *);

/* Do one-time finalization. */
void px_state_finit(px_state_t *);

/* get media size */
void px_get_default_media_size(px_state_t *pxs, gs_point *pt);

/* special begin page operator for passthrough mode. */
int pxBeginPageFromPassthrough(px_state_t *pxs);

#endif				/* pxstate_INCLUDED */
