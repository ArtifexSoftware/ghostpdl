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

/* pxgstate.h */
/* Graphics state extension for PCL XL interpreter */

#ifndef pxgstate_INCLUDED
#  define pxgstate_INCLUDED

#include "gsccolor.h"		/* for gs_client_color */
#include "gsmatrix.h"		/* must precede gsiparam.h */
#include "gsiparam.h"		/* for px_image_color_space */
#include "gsrefct.h"
#include "gxbitmap.h"
#include "gxfixed.h"
#include "plsymbol.h"
#include "pxdict.h"
#include "pxenum.h"

/* Define an abstract type for the PostScript graphics state. */
#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s gs_state;
#endif

/* Define an abstract type for a font. */
#ifndef px_font_t_DEFINED
#  define px_font_t_DEFINED
typedef struct px_font_s px_font_t;
#endif

/* Define the type of the PCL XL state. */
#ifndef px_state_DEFINED
#  define px_state_DEFINED
typedef struct px_state_s px_state_t;
#endif

/* Define the parameters for a downloaded bitmap (image or raster pattern). */
typedef struct px_bitmap_params_s {
  uint width, height;
  int depth;
  pxeColorSpace_t color_space;
  bool indexed;
  real dest_width, dest_height;
} px_bitmap_params_t;

/* Define the structure for downloaded raster patterns. */
typedef struct px_pattern_s {
  rc_header rc;			/* counts refs from gstates, dicts */
	/* Original parameters */
  px_bitmap_params_t params;
  gs_string palette;		/* copy of palette if indexed color */
  byte *data;			/* raster data */
	/* Internal values */
  gx_bitmap_id id;		/* PCL XL ID * #persistence + persistence */
} px_pattern_t;
#define private_st_px_pattern()		/* in pximage.c */\
  gs_private_st_composite(st_px_pattern, px_pattern_t, "px_pattern_t",\
    px_pattern_enum_ptrs, px_pattern_reloc_ptrs);
/* Define the freeing procedure for patterns in a dictionary. */
void px_free_pattern(gs_memory_t *, void *, client_name_t);
/* Purge the pattern cache up to a given persistence level. */
void px_purge_pattern_cache(px_state_t *, pxePatternPersistence_t);
/* purge font cache - all characters */
void px_purge_character_cache(px_state_t *pxs);

/* Define a structure for a brush or pen.  These only exist */
/* within a px_gstate_t; they are never allocated separately. */
typedef enum {
  pxpNull,
  pxpGray,
  pxpRGB,
  pxpSRGB,
  pxpPattern
} px_paint_type_t;
typedef struct px_paint_s {
  px_paint_type_t type;
  union pv_ {
    float gray;
    float rgb[3];
    struct p_ {
      px_pattern_t *pattern;
      gs_client_color color;
    } pattern;
  } value;
} px_paint_t;
#define private_st_px_paint()		/* in pxgstate.c */\
  gs_private_st_composite(st_px_paint, px_paint_t, "px_paint_t",\
    px_paint_enum_ptrs, px_paint_reloc_ptrs)
#define st_px_paint_max_ptrs (st_client_color_max_ptrs + 1)

/* Define the types of character transformation, for remembering the order */
/* in which to apply them. */
typedef enum {
  pxct_rotate = 0,
  pxct_shear,
  pxct_scale
} px_char_transform_t;

/* Define the PCL XL extension of the PostScript graphics state. */
typedef struct px_gstate_s {
  gs_memory_t *memory;
	/* Since this is what the 'client data' of the gs_state points to, */
	/* we need a pointer back to the px_state_t. */
  px_state_t *pxs;
	/* State information */
  px_paint_t brush;
  float char_angle;
  float char_bold_value;
  gs_point char_scale;
  gs_point char_shear;
  /*
   * The strange H-P rules for character rotation, scaling, and
   * shearing require us to keep track of the order in which they should
   * be applied when we finally construct the text matrix.
   * This array is always a permutation of the 3 possible values.
   */
  px_char_transform_t char_transforms[3];
  pxeCharSubModeArray_t char_sub_mode;
  pxeWritingMode_t writing_mode;
  pxeClipMode_t clip_mode;
  pxeColorSpace_t color_space;
  gs_const_string palette;
  bool palette_is_shared;	/* with next higher gstate */
  float char_size;
  uint symbol_set;
  px_font_t *base_font;		/* 0 if no font set */
  struct ht_ {
    pxeDitherMatrix_t method;
    bool set;			/* true if we have done gs_sethalftone */
				/* with these parameters */
    uint width;
    uint height;
    gs_point origin;
    gs_string thresholds;
  } halftone;
  gs_string dither_matrix;	/* dither matrix downloaded at this level */
  pxeFillMode_t fill_mode;
  bool dashed;
  gs_matrix dash_matrix;
  px_paint_t pen;
	/* Pattern dictionary */
  px_dict_t temp_pattern_dict;
	/* Cached values */
  gs_matrix text_ctm;		/* scale/rotate transformations applied in */
				/* the reverse order */
  gs_matrix char_matrix;	/* char_size+angle+scale+shear */
  bool char_matrix_set;
  int stack_depth;		/* # of unmatched PushGS */
  const pl_symbol_map_t *symbol_map; /* symbol mapping */
} px_gstate_t;
#define private_st_px_gstate()	/* in pxgstate.c */\
  gs_private_st_composite(st_px_gstate, px_gstate_t, "px_gstate_t",\
    px_gstate_enum_ptrs, px_gstate_reloc_ptrs)
#define px_gstate_do_ptrs(m)\
  m(0,pxs) m(1,base_font)
#define px_gstate_num_ptrs 2
#define px_gstate_do_string_ptrs(m)\
  m(0,halftone.thresholds) m(1,dither_matrix)
#define px_gstate_num_string_ptrs 2

/* Allocate a px_gstate_t. */
px_gstate_t *px_gstate_alloc(gs_memory_t *);

/* Initialize a px_gstate_t. */
void px_gstate_init(px_gstate_t *, gs_state *);

/* Initialize the graphics state for a page. */
/* Note that this takes a px_state_t, not a px_gstate_t. */
int px_initgraphics(px_state_t *);

/* initialize the clipping region */
int px_initclip(px_state_t *pxs);

/* Reset a px_gstate_t, initially or at the beginning of a page. */
void px_gstate_reset(px_gstate_t *);

/* Set up the color space information for a bitmap image or pattern. */
int px_image_color_space(gs_image_t *pim,
                         const px_bitmap_params_t *params,
                         const gs_string *palette,
                         const gs_state *pgs);

/* Set the color in the graphics state to the pen or brush. */
int px_set_paint(const px_paint_t *ppt, px_state_t *pxs);

/* Set the halftone in the graphics state to the most recently selected one. */
int px_set_halftone(px_state_t *pxs);

/* Adjust the paint reference counts in the graphics state. */
/* These are exported by pxgstate.c for pxink.c. */
void px_paint_rc_adjust(px_paint_t *ppt, int delta, gs_memory_t *mem);
void px_gstate_rc_adjust(px_gstate_t *pxgs, int delta, gs_memory_t *mem);

#endif				/* pxstate_INCLUDED */
