/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcstate.h */
/* Definition of PCL5 state */

#include "gsdcolor.h"		/* for gx_ht_tile */
#include "gsmatrix.h"		/* for gsiparam.h */
#include "gsiparam.h"
#include "pldict.h"
#include "plfont.h"
/*#include "pgstate.h"*/	/* HP-GL/2 state, included below */

/*
 * Following the PCL documentation, we represent coordinates internally in
 * centipoints (1/7200").  When we are in PCL mode, we maintain a CTM that
 * puts (0,0) at the origin of the logical page, uses the PCL coordinate
 * orientation (+Y goes down the page, opposite from PostScript), uses
 * centipoints as the user units, and takes both orientation and print
 * direction into account.
 */
#if arch_sizeof_int == 2
typedef long coord;
#else
typedef int coord;
#endif
#define pcl_coord_scale 7200
#define inch2coord(v) ((coord)((v) * (coord)pcl_coord_scale))
#define coord2inch(c) ((c) / (float)pcl_coord_scale)
typedef struct coord_point_s {
  coord x, y;
} coord_point;

/* Define the structure for paper size parameters. */
/* Note that these values are all coords (centipoints). */
typedef struct pcl_paper_size_s {
  coord width, height;		/* physical page size */
  coord_point offset_portrait;	/* offsets of logical page from physical */
				/* page in portrait orientations */
  coord_point offset_landscape; /* ditto for landscape orientations */
} pcl_paper_size_t;

/* Define the parameters for one font set (primary or secondary). */
#ifndef pl_font_t_DEFINED
#  define pl_font_t_DEFINED
typedef struct pl_font_s pl_font_t;
#endif
typedef struct pcl_font_selection_s {
    /* Parameters used for selection, or loaded from font selected by ID. */
  pl_font_params_t params;
    /* Font found by matching */
  pl_font_t *font;
    /* The symbol map that goes with it. */
  pl_symbol_map_t *map;
} pcl_font_selection_t;

/* Define the indices for PCL color spaces (e.g. for the CID command). */
typedef enum {
  pcl_csi_DeviceRGB = 0,
  pcl_csi_DeviceCMY = 1,
  pcl_csi_ColorimetricRGB = 2,
  pcl_csi_CIELab = 3,
  pcl_csi_LuminanceChrominance = 4
} pcl_color_space_index;

/* Define the type for an ID key used in a dictionary. */
typedef struct pcl_id_s {
  uint value;
  byte key[2];			/* key for dictionaries */
} pcl_id_t;
#define id_key(id) ((id).key)
#define id_value(id) ((id).value)
#define id_set_key(id, bytes)\
  ((id).key[0] = (bytes)[0], (id).key[1] = (bytes)[1],\
   (id).value = ((id).key[0] << 8) + (id).key[1])
#define id_set_value(id, val)\
  ((id).value = (val), (id).key[0] = (val) >> 8, (id).key[1] = (byte)(val))

/* Define the various types of pattern for filling. */
typedef enum {
  pcpt_solid_black = 0,
  pcpt_solid_white,
  pcpt_shading,
  pcpt_cross_hatch,
  pcpt_user_defined,
  pcpt_current_pattern	/* for rectangle fill only */
} pcl_pattern_type_t;

/*
 * Define the PCL/HPGL color mapping state.  This is what is saved on the
 * palette stack.
 */
#ifndef gs_halftone_DEFINED
#  define gs_halftone_DEFINED
typedef struct gs_halftone_s gs_halftone;
#endif
typedef struct pcl_palette_state_s pcl_palette_state_t;
typedef struct cid_range_s {
  float vmin, vmax;
} cid_range_t;
typedef struct cid_scaling_s {
  float gamma, gain;
} cid_scaling_t;
typedef struct cid_xy_s {
  float x, y;
} cid_xy_t;
struct pcl_palette_state_s {
  pcl_palette_state_t *next;	/* for palette stack */

	/* Chapter C2 */

  bool readonly;		/* palette can't be changed */
  pcl_color_space_index color_space_index;
  int bits_per_index;
  bool planar;		/* is this needed? */
  int bits_per_primary[3];
  bool indexed;
  union {
    int vi[6];			/* for RGB/CMY reference values */
    float vf[29];		/* for device-independent parameters */
    struct {
      int white_reference[3];
      int black_reference[3];
    } rgb_cmy;
    struct {
      cid_range_t l, a, b;
    } lab;
    struct {
      cid_xy_t chromaticity[3];		/* r, g, b */
      cid_scaling_t scaling[3];		/* ditto */
      cid_range_t range[3];		/* ditto */
    } colorimetric;
    struct {
      struct {
	float rgb[3];
      } primary_encoding[3];
      cid_range_t primary_ranges[3];
      cid_xy_t chromaticity[3];		/* r, g, b */
      cid_xy_t white_point;
      cid_scaling_t scaling[3];		/* ditto */
    } luminance_chrominance;
  } params;

	/* Chapter C3 */

  pcl_fixed *data;		/* actual color mapping palette */
  uint size;			/* # of data elements */

	/* Chapter C4 */

  int render_algorithm;		/* (also in graphics state) */
  gs_halftone *halftone;	/* downloaded dither matrix/ces */
  float gamma;
  union cu_ {
    const byte *i[5];		/* indexed by color space */
    struct {
      const byte *rgb;
      const byte *cmy;		/* identical to rgb */
      const byte *colorimetric;
      const byte *lab;
      const byte *luminance_chrominance;
    } v;
  } clut;
  enum {
    rgb_use_none,
    rgb_use_clut,
    rgb_use_gamma
  } rgb_correction;
  union wu_ {
    float i[2];
    struct { float x, y; } v;
  } white_point;

	/* Other */

  /**** HPGL pen widths ****/

};

/*
 * Define the state of a single raster data row.  This is complicated
 * because it has to keep track of the repetition count for compression mode
 * 5.  (Actually, it's probably only one plane that has to track this, but
 * we aren't sure about this yet.)
 */
typedef struct pcl_raster_row_s {
  byte *data;
  uint size;			/* allocated size of data */
  uint count;			/* amount of data currently valid */
  bool is_zero;			/* if true, data[0..count-1] are zero */
  uint repeat_count;		/* # of repetitions for compression mode 5 */
} pcl_raster_row_t;

/*
 * Define the entire PCL/HPGL state.  The documentation for this is spread
 * out over the PCL reference manuals, and is incomplete, inconsistent, and
 * poorly organized.  In order to have a ghost (:-)) of a chance of being
 * able to think about it as a whole, we've organized it here by
 * documentation chapter, just as we did the .c files.
 */
#ifndef pcl_state_DEFINED
#  define pcl_state_DEFINED
typedef struct pcl_state_s pcl_state_t;
#endif
#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s gs_state;
#endif
#include "pgstate.h"		/* HP-GL/2 state */
struct pcl_state_s {

	gs_memory_t *memory;
		/* Hook back to client data, for callback procedures */
	void *client_data;
		/* Reference to graphics state */
	gs_state *pgs;
	gs_point resolution;		/* device resolution, a copy of */
					/* pgs->device->HWResolution */

		/* Parsing */
	  /* Define an optional procedure for parsing non-ESC data. */
	int (*parse_other)(P3(void *parse_data, pcl_state_t *pcls,
			      stream_cursor_read *pr));
	void *parse_data;	/* closure data for parse_other */

		/* Chapter 4 (pcjob.c) */

	int num_copies;			/* (also a device parameter) */
	bool duplex;			/* (also a device parameter) */
	bool bind_short_edge;		/* (also a device parameter) */
	float left_offset_cp, top_offset_cp;
	bool back_side;			/* (also a device parameter) */
	int output_bin;			/* (also a device parameter) */
	int uom_cp;		/* Unit of Measure, centipoints per PCL Unit */
	float uom_dots;		/* dots per PCL Unit */

		/* Chapter 5 (pcpage.c) */

	const pcl_paper_size_t *paper_size;
	bool manual_feed;	/* ? */
	int paper_source;
	int print_direction;	/* 0..3 */
	int orientation;	/* 0..3 */
	coord left_margin, right_margin;
	coord top_margin;
	int text_length;
	bool perforation_skip;
	coord hmi, hmi_unrounded, vmi;
	enum {
	  hmi_not_set,		/* must be recomputed from font */
	  hmi_set_from_font,
	  hmi_set_explicitly
	} hmi_set;
	int (*end_page)(P3(pcl_state_t *pcls, int num_copies, int flush));
		/* Internal variables */
	bool have_page;		/* true if anything has been written on page */
	coord_point logical_page_size;	/* size of logical page */
#define logical_page_width logical_page_size.x
#define logical_page_height logical_page_size.y

		/* Chapter 6 (pcursor.c) */

	coord_point cap;  /* in user coordinates relative to standard CTM */
	struct {
	  coord_point values[20];
	  int depth;
	} cursor_stack;
	int line_termination;

		/* Chapter 8 (pcfont.c) */

	pcl_font_selection_t font_selection[2];
	int font_selected;		/* 0 or 1 */
	pl_font_t *font;		/* 0 means recompute from params */
	pl_dict_t built_in_fonts;	/* "built-in", known at start-up */
	bool underline_enabled;
	bool underline_floating;
	float underline_position;	/* distance from baseline */
		/* Internal variables */
	gs_font_dir *font_dir;		/* gs-level dictionary of fonts */
	coord_point last_width;		/* escapement of last char (for BS) */
	coord_point underline_start;	/* start point of underline */
	bool last_was_BS;		/* no printable chars since last BS */
	pl_symbol_map_t *map;		/* map for current font (above) */

		/* Chapter 10 (pcsymbol.c) */

	pcl_id_t symbol_set_id;
	pl_dict_t soft_symbol_sets;
	pl_dict_t built_in_symbol_sets;

		/* Chapter 9 & 11 (pcsfont.c) */

	pcl_id_t font_id;
	uint character_code;
	pl_dict_t soft_fonts;

		/* Chapter 12 (pcmacros.c) */

	pcl_id_t macro_id;
	pcl_id_t overlay_macro_id;
	bool overlay_enabled;
	int macro_level;
	pl_dict_t macros;
	bool defining_macro;
	pcl_state_t *saved;	/* saved state during execute/call/overlay */
		/* Internal variables */
	byte *macro_definition;	/* for macro being defined, if any */

		/* Chapter 13 (pcprint.c) */

	pcl_id_t pattern_id;
	pcl_pattern_type_t pattern_type;
	bool source_transparent;	/* (also in graphics state) */
	bool pattern_transparent;	/* (also in graphics state) */
	bool shift_patterns;
	gs_int_point pattern_reference_point;	/* (also in graphics state) */
	bool rotate_patterns;
	pl_dict_t patterns;
		/* Internal variables */
	pcl_id_t current_pattern_id;	/* at last select_pattern */
	bool pattern_set;	/* true if pattern set in graphics state */
	gx_ht_tile pattern_tile;	/* set by pcl_set_drawing_color */
	gs_pattern_instance *cached_patterns[7+6];  /* create as needed */

		/* Chapter 14 (pcrect.c) */

	coord_point rectangle;
	/* uint pattern_id; */	/* in Chapter 13 */

		/* Chapter 15 (pcraster.c) && */
		/* Chapter C6 (pccrastr.c) */

	struct r_ {
		/* 15 */
	  bool graphics_mode;
	  int (*end_graphics)(P1(pcl_state_t *)); /* for implicit end raster graphics */
	  uint left_margin;
	  int resolution;
	  bool across_physical_page;
	  uint height;
	  bool height_set;
	  uint width;
	  bool width_set;
	  int compression;
	  int depth;		/* # of bits per pixel */
	  bool planar;
	  gs_image_t image;	/* preset for depth, color space */
		/* (Seed) row buffers */
	  pcl_raster_row_t row[8];	/* only [0] used for non-color */
		/* HP RTL */
	  int y_direction;	/* +1 or -1, "line path" */
		/* Working storage while in raster graphics mode */
	  int y;
	  uint last_width;	/* width for begin_image */
	  int first_y;		/* y at begin_image */
	  void *image_info;	/* from begin_image */
		/* C6 */
	  bool scaling;
	  float dest_width_dp;
	  float dest_height_dp;
	  bool enhance_dark;
		/* Working storage while in raster graphics mode */
	  int plane;
	  pcl_raster_row_t color_row;	/* for assembling chunky pixels from planes */
	} raster;

		/* Chapter 16 (pcstatus.c) */

	int location_type;
	int location_unit;
	struct _sb {
	  byte internal_buffer[80];	/* enough for an error message */
	  byte *buffer;
	  uint write_pos;
	  uint read_pos;
	} status;

		/* Chapter 24 (pcmisc.c) */

	bool end_of_line_wrap;
	bool display_functions;
	int (*configure_appletalk)(P4(const byte *key, uint key_length,
				      const byte *value, uint value_length));

		/* Chapter C2 (pccmodes.c) */

	/* (see pcl_palette_state_t above) */
	/**** more stuff from pp. C2-21 thru -24 ****/

		/* Chapter C3 (pccpalet.c) */

	pcl_palette_state_t palette;	/* see above */
	/**** palette stack ****/
	pcl_id_t palette_id;
	pcl_id_t palette_control_id;
	pl_dict_t palettes;
	uint foreground_color;
	pcl_fixed color_components[3];

		/* Chapter C4 (pccrendr.c) */

	/* (see pcl_palette_state_t above) */
	bool monochrome_print;
	int lightness;
	int saturation;
	int scaling_algorithm;
	int color_treatment;
	/**** downloaded color map? ****/

		/* Chapter C5 (pccprint.c) */

	byte logical_op;	/* (also in graphics state) */
	bool grid_centered;	/* (also fill adjust in graphics state) */

		/* Chapter C6 (pccrastr.c) */

	/* (see Chapter 15 above) */

		/* ---------------- HP-GL/2 state ---------------- */

	pcl_hpgl_state_t g;	/* see pgstate.h */

};
