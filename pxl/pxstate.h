/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pxstate.h */
/* State definitions for PCL XL interpreter */

#ifndef pxstate_INCLUDED
#  define pxstate_INCLUDED

#include "gsmemory.h"
#include "pxgstate.h"
#include "pjparse.h"
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
	/* Pattern dictionary */
  px_dict_t session_pattern_dict;

	/* Page state */

  pxeOrientation_t orientation;
  pxeMediaSource_t media_source;
  gs_point media_size;		/* in 1/72" units */
  bool duplex;
  pxeDuplexPageMode_t duplex_page_mode;
  bool duplex_back_side;
  pxeMediaDestination_t media_destination;
  pxeMediaType_t media_type;
  int (*end_page)(P3(px_state_t *pxs, int num_copies, int flush));
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
  struct sd2_ {
    byte *data;
    uint size;
  } stream_def;

	/* Font dictionary */
  px_dict_t font_dict;
	/* Font/character downloading state */
  px_font_t *download_font;
  int font_format;
	/* Global structures */
  gs_font_dir *font_dir;
  px_font_t *error_page_font;
  gs_show_enum *error_page_show_enum;

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
  struct sp_ {
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
  pjl_parser_state *pjls;
};
#define private_st_px_state()		/* in pxstate.c */\
  gs_private_st_composite(st_px_state, px_state_t, "px_state",\
    px_state_enum_ptrs, px_state_reloc_ptrs)
/* Enumerate the various kinds of pointers in a px_state_t. */
#define px_state_do_ptrs(m)\
  m(0,client_data) m(1,stream_def.data)\
  m(2,download_font) m(3,font_dir) m(4,error_page_font)\
  m(5,error_page_show_enum) m(6,pgs) m(7,pxgs) m(8,image_enum)\
  m(9,pattern_enum) m(10,download_bytes.data)
#define px_state_num_ptrs 11
#define px_state_do_string_ptrs(m)\
  m(0,stream_name) m(1,download_string)
#define px_state_num_string_ptrs 2
#define px_state_do_dicts(m)\
  m(0,session_pattern_dict) m(1,page_pattern_dict) m(2,stream_dict)\
  m(4,font_dict)
#define px_state_num_dicts 4

/* Allocate a px_state_t. */
px_state_t *px_state_alloc(P1(gs_memory_t *));

/* Do one-time state initialization. */
void px_state_init(P2(px_state_t *, gs_state *));

/* Define the default end-of-page procedure. */
int px_default_end_page(P3(px_state_t *, int, int));

/* Clean up after an error or UEL. */
void px_state_cleanup(P1(px_state_t *));

/* Do one-time finalization. */
void px_state_finit(P1(px_state_t *));

#endif				/* pxstate_INCLUDED */
