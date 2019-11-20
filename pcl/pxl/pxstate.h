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


/* pxstate.h */
/* State definitions for PCL XL interpreter */

#ifndef pxstate_INCLUDED
#  define pxstate_INCLUDED

#include "gsmemory.h"
#include "pxgstate.h"
#include "pltop.h"
#include "gslibctx.h"
#include "gxtext.h"

#include "pcparse.h"
#include "pgmand.h"

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
typedef struct px_media_s
{
    pxeMediaSize_t ms_enum;
    const char *mname;
    short width, height;
    short m_left, m_top, m_right, m_bottom;
} px_media_t;

/* This structure captures the entire state of the PCL XL "virtual */
/* machine", except for graphics state parameters in the gs_gstate. */
struct px_state_s
{

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

    int (*end_page) (px_state_t * pxs, int num_copies, int flush);
    /* Pattern dictionary */
    px_dict_t page_pattern_dict;
    /* Internal variables */
    bool have_page;             /* true if anything has been written on page */
    /* Cached values */
    gs_matrix initial_matrix;

    /* Data source */

    bool data_source_open;
    bool data_source_big_endian;
    /* Stream dictionary */
    px_dict_t stream_dict;
    /* Stream reading state */
    gs_string stream_name;
    int stream_level;           /* recursion depth */
    struct sd2_
    {
        byte *data;
        uint size;
    } stream_def;

    /* Font dictionary */
    px_dict_t font_dict;
    px_dict_t builtin_font_dict;
    /* Font/character downloading state */
    pl_font_t *download_font;
    int font_format;
    /* Global structures */
    gs_font_dir *font_dir;
    pl_font_t *error_page_font;

    /* Graphics state */

    gs_gstate *pgs;              /* PostScript graphics state */
    px_gstate_t *pxgs;
    /* Image/pattern reading state */
    px_image_enum_t *image_enum;
    px_pattern_enum_t *pattern_enum;

    /* Miscellaneous */
    bool interpolate;           /* image interpolation */
    bool nocache;               /* disable the cache */

    struct db_
    {
        byte *data;
        uint size;
    } download_bytes;           /* font/character/halftone data */
    gs_string download_string;  /* ditto */
    struct sp_n
    {
        int x;
        double y0, y1;
    } scan_point;               /* position when reading scan lines */

    /* We put the warning table and error line buffer at the end */
    /* so that the offsets of the scalars will stay small. */
#define px_max_error_line 120
    char error_line[px_max_error_line + 1];     /* for errors with their own msg */
#define px_max_warning_message 500
    uint warning_length;
    char warnings[px_max_warning_message + 1];
    /* ---------------- PJL state -------------------- */
    pl_interp_implementation_t *pjls;
    /* ---------------- PCL state -------------------- */
    pl_interp_implementation_t *pcls;

    struct pcl_state_s *pcs;

    pcl_parser_state_t  pcl_parser_state;

    hpgl_parser_state_t gl_parser_state;

    /* if this is a contiguous passthrough meaning that 2 passtrough
       operators have been given back to back and pxl should not regain
       control. */
    bool                this_pass_contiguous;

    bool                pass_first;
};

/* Allocate a px_state_t. */
px_state_t *px_state_alloc(gs_memory_t *);

/* Release a px_state_t */
void px_state_release(px_state_t * pxs);

/* Do one-time state initialization. */
void px_state_init(px_state_t *, gs_gstate *);

/* Define the default end-of-page procedure. */
int px_default_end_page(px_state_t *, int, int);

/* Clean up after an error or UEL. */
void px_state_cleanup(px_state_t *);

/* Do one-time finalization. */
void px_state_finit(px_state_t *);

/* get media size */
void px_get_default_media_size(px_state_t * pxs, gs_point * pt);

/* special begin page operator for passthrough mode. */
int pxBeginPageFromPassthrough(px_state_t * pxs);

#endif /* pxstate_INCLUDED */
