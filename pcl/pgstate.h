/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgstate.h */
/* Definition of HP-GL/2 portion of PCL5 state */
#include "gslparam.h"
#include "gsuid.h"		/* for gxbitmap.h */
#include "gstypes.h"		/* for gxbitmap.h */
#include "gxbitmap.h"

/* Define a type for HP-GL/2 floating point values. */
typedef double hpgl_real_t;

/* Define a line type (a.k.a. dash pattern). */
typedef struct hpgl_line_type_s {
  int count;
  hpgl_real_t gap[20];
} hpgl_line_type_t;

/* Define the current rendering mode - character, polygon, or vector.
   This will affect the line attributes chosen see
   hpgl_set_graphics_line_attribute_state */
typedef enum {
	vector_mode,
	character_mode,
	polygon_mode
} hpgl_rendering_mode_t;

typedef struct pcl_hpgl_state_s {

		/* Chapter 18 (pgframe.c) */

	struct pf_ {
	  float width;			/* in decipoints */
	  float height;			/* ditto */
	  gs_int_point anchor_point;
	} picture_frame;
	float plot_width, plot_height;	/* in inches */

		/* Chapter 19 (pgconfig.c) */

  	enum {
	  hpgl_scaling_none = -1,
	  hpgl_scaling_anisotropic = 0,
	  hpgl_scaling_isotropic = 1,
	  hpgl_scaling_point_factor = 2
	} scaling_type;
	struct sc_ {
	  gs_point pmin, pmax, factor;
	  float left, bottom;
	} scaling_params;
	gs_rect window;          /* clipping window (IW) */
	int rotation;
	gs_point P1, P2;	/* in plotter units */

		/* Chapter 20 (pgvector.c) */

	bool pen_down;
	bool relative;		/* true if relative coordinates */
        gs_point pos;

        /* used to track the line drawing state in hpgl */
        gs_point first_point;
        bool have_path;

		/* Chapter 21 (pgpoly.c) */

	/**** polygon buffer ****/
	bool polygon_mode;

		/* Chapter 22 (pglfill.c) */

	struct lp_ {
	  int type;
	  float pattern_length;
	  bool pattern_length_relative;
	  int cap;
	  int join;
	  bool is_solid;
	} line;
	float miter_limit;
	/**** FOLLOWING SHOULD BE [number_of_pens] ****/
	float pen_width[2];	/* millimeters or % */
	bool pen_width_relative;
	int pen;
	byte symbol_mode;	/* 0 if not in symbol mode */
	struct ft_ {
	  enum {
	    hpgl_fill_solid = 1,
	    hpgl_fill_solid2 = 2,
	    hpgl_fill_hatch = 3,
	    hpgl_fill_crosshatch = 4,
	    hpgl_fill_shaded = 10,
	    hpgl_fill_hpgl_user_defined = 11,
	    hpgl_fill_pcl_crosshatch = 21,
	    hpgl_fill_pcl_user_defined = 22
	  } type;
	  union fp_ {
	    struct { float spacing, angle; } hatch;
	    float shading;
	    int pattern_index;
	    int pattern_type;
	    uint pattern_id;
	  } param;
	} fill;
	hpgl_line_type_t fixed_line_type[8];
	hpgl_line_type_t adaptive_line_type[8];
	gs_point anchor_corner;
	gx_bitmap fill_pattern[8];
	bool source_transparent;
	struct scr_ {
	  enum {
	    hpgl_screen_none = 0,
	    hpgl_screen_shaded_fill = 1,
	    hpgl_screen_hpgl_user_defined = 2,
	    hpgl_screen_crosshatch = 21,
	    hpgl_screen_pcl_user_defined = 22
	  } type;
	  union su_ {
	    float shading;
	    struct { int pattern_index; bool use_current_pen; } user_defined;
	    int pattern_type;
	    uint pattern_id;
	  } param;
	} screen;
	    /* Temporary while downloading raster fill pattern */
	struct rf_ {
	  int index, width, height;
	  uint raster;
	  byte *data;
	} raster_fill;

		/* Chapter 23 (pgchar.c) */

	pcl_font_selection_t font_selection[2];
	int font_selected;	/* 0 or 1 */
	pl_font_t *font;	/* 0 means recompute from params */
	struct ch_ {
	  gs_point direction;
	  bool direction_relative;
	  int text_path;
	  bool reverse_line_feed;
	  gs_point extra_space;
	  gs_point size;
	  bool size_relative;
	  bool size_set;
	  hpgl_real_t slant;
	  enum {
	    hpgl_char_solid = -1,
	    hpgl_char_solid_edge = 0,
	    hpgl_char_edge = 1,
	    hpgl_char_fill = 2,
	    hpgl_char_fill_edge = 3
	  } fill_mode;
	  int edge_pen;
	} character;
	struct lb_ {
	  int origin;
	  uint terminator;
	  bool print_terminator;
	} label;
	bool transparent_data;
	uint font_id[2];
	bool bitmap_fonts_allowed;

		/* Chapter C7 (pgcolor.c) */

	/**** FOLLOWING SHOULD BE [number_of_pens] ****/
	struct { hpgl_real_t rgb[3]; } pen_color[2];
	uint number_of_pens;
	struct { hpgl_real_t cmin, cmax; } color_range[3];

	hpgl_rendering_mode_t render_mode;

} pcl_hpgl_state_t;
