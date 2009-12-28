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

/* plfont.h */
/* Interface to PCL font utilities */

#ifndef plfont_INCLUDED
#  define plfont_INCLUDED

#include "gsccode.h"
#include "plsymbol.h"
#include "pldict.h"
#include "strmio.h"

/* ---------------- PCL-specified definitions ---------------- */

/*
 * Define a types for 16-bit (Unicode/MSL) characters.
 * Note that these are always unsigned.
 */
typedef bits16 char16;

/* Define the Font Type for PCL5 downloaded fonts. */
typedef enum {
  plft_7bit_printable = 0,	/* bound, 32-127 printable */
  plft_8bit_printable = 1,	/* bound, 32-127 & 160-255 printable */
  plft_8bit = 2,		/* bound, 0-255 printable */
  plft_16bit = 3,		/* bound, 0-65534 printable */
  plft_MSL = 10,		/* unbound, MSL codes */
  plft_Unicode = 11		/* unbound, Unicode codes */
} pl_font_type_t;
#define pl_font_is_bound(plfont)\
  ((plfont)->font_type < 10)

/* Define the Font Scaling Technology for PCL downloaded fonts. */
typedef enum {
  plfst_Intellifont = 0,
  plfst_TrueType = 1,
  plfst_MicroType = 63,
  plfst_bitmap = 254
} pl_font_scaling_technology_t;
#define pl_font_is_scalable(plfont)\
  ((plfont)->scaling_technology != plfst_bitmap)

/*
 * Define the matching characteristics for PCL5 fonts.  Note that there
 * are two different values representing pitch: one in centipoints,
 * one in characters/inch * 100.
 */
#define pl_fp_pitch_cp(pfp) ((pfp)->pitch.cp)
#define pl_fp_set_pitch_cp(pfp, cpv)\
  ((pfp)->pitch.cp = (cpv),\
   (pfp)->pitch.per_inch_x100 = ( (cpv) == 0 ? (cpv) : 720000.0 / (cpv)))
#define pl_fp_pitch_per_inch(pfp) ((pfp)->pitch.per_inch_x100 / 100.0)
#define pl_fp_pitch_per_inch_x100(pfp) ((pfp)->pitch.per_inch_x100)
#define pl_fp_set_pitch_per_inch(pfp, cpi)\
  ((pfp)->pitch.cp = 7200.0 / (cpi),\
   (pfp)->pitch.per_inch_x100 = (cpi) * 100.0)
typedef struct pl_font_pitch_s {
  floatp cp;
  floatp per_inch_x100;
} pl_font_pitch_t;
#define fp_pitch_value_cp(cpv)\
  { (cpv), 720000 / (cpv) }
typedef struct pl_font_params_s {
  uint symbol_set;
  bool proportional_spacing;
  pl_font_pitch_t pitch;
  uint height_4ths;		/* unused in scalable fonts */
  uint style;
  int stroke_weight;
  uint typeface_family;
    /**** HACK, unfortunately there is not a better way to do this at
          the current time. ****/
  int pjl_font_number;
} pl_font_params_t;

/* ---------------- Internal structures ---------------- */

/*
 * Define the size values for hash tables.
 * We require used <= limit < size.
 */
#define pl_table_sizes\
  uint used;			/* # of entries in use */\
  uint limit;			/* max # of entries in use */\
  uint size;			/* allocated size of table */\
  uint skip			/* rehashing interval */

/*
 * Define the hash table for mapping glyphs to character data.
 * Empty entries have data == 0 and glyph == 0;
 * deleted entries have data == 0 and glyph != 0.
 */
typedef struct pl_font_glyph_s {
  gs_glyph glyph;
  const byte *data;
} pl_font_glyph_t;
typedef struct pl_glyph_table_s {
  pl_font_glyph_t *table;
  pl_table_sizes;
} pl_glyph_table_t;

/*
 * Define the hash table for mapping character codes to TrueType glyph indices.
 * Empty entries have chr == gs_no_char and glyph == 0;
 * deleted entries have chr == gs_no_char and glyph != 0.
 */
typedef struct pl_tt_char_glyph_s {
  gs_char chr;
  gs_glyph glyph;
} pl_tt_char_glyph_t;
typedef struct pl_tt_char_glyph_table_s {
  pl_tt_char_glyph_t *table;
  pl_table_sizes;
} pl_tt_char_glyph_table_t;

/* Define an abstract type for library fonts. */
#ifndef gs_font_DEFINED
#  define gs_font_DEFINED
typedef struct gs_font_s gs_font;
#endif

/* Define an abstract type for transformation matrices. */
#ifndef gs_matrix_DEFINED
#  define gs_matrix_DEFINED
typedef struct gs_matrix_s gs_matrix;
#endif

/*
 * We 'wrap' gs_fonts in our own structure.
 *
 */
#ifndef pl_font_t_DEFINED
#  define pl_font_t_DEFINED
typedef struct pl_font_s pl_font_t;
#endif
struct pl_font_s {
  gs_font *pfont;	      /* Type 42 if TrueType, Type 3 if bitmap. */
  int storage;                /* where the font is stored */
  bool data_are_permanent;    /* glyph data stored in rom */
  char *font_file;            /* non null only if data is stored in a
				 file only relevant to pcl resident
				 fonts. NB this should be done
				 dynamically */
  bool font_file_loaded;      /* contents of the font file have be read into memory */
  byte *header;	              /* downloaded header, or built-in font data */
  ulong header_size;
	/* Information extracted from the font or supplied by the client. */
  pl_font_scaling_technology_t scaling_technology;
  bool is_xl_format;          /* this is required for the agfa ufst scaler */
  pl_font_type_t font_type;
  bool allow_vertical_substitutes;
  /* Implementation of pl_font_char_width, see below */
  int (*char_width)(const pl_font_t *plfont, const void *pgs, uint char_code, gs_point *pwidth);
  int (*char_metrics)(const pl_font_t *plfont, const void *pgs, uint char_code, float metrics[4]);
  bool large_sizes;	/* segment sizes are 32 bits if true, 16 if false */
			/* (for segmented fonts only) */
  struct { uint x, y; } resolution; /* resolution (for bitmap fonts) */
  float bold_fraction;              /* for PXL algorithmic bolding */
  int orient;                       /* true if pcl bitmap font designed in landscape */
  pl_font_params_t params;
  byte character_complement[8];	/* character complement (for unbound fonts) */
  struct o_ {
    long GC;		/* Galley Character (optional) */
    long GT;		/* Global TrueType data (required, for TT fonts) */
    long VT;		/* VerTical substitution (optional) */
  } offsets;		/* segment offsets, -1 if segment missing */
	/* Glyph table for downloaded fonts. */
  pl_glyph_table_t glyphs;
	/* Character to glyph map for downloaded TrueType fonts. */
  pl_tt_char_glyph_table_t char_glyphs;

  float pts_per_inch;   /* either 72 or 72.307 (for Intellifont) */
};
#define private_st_pl_font()	/* in plfont.c */\
  gs_private_st_ptrs4(st_pl_font, pl_font_t, "pl_font_t",\
    pl_font_enum_ptrs, pl_font_reloc_ptrs, pfont, header, glyphs.table,\
      char_glyphs.table)

/* ---------------- Procedural interface ---------------- */

/* Allocate and minimally initialize a font. */
pl_font_t *pl_alloc_font(gs_memory_t *mem, client_name_t cname);

/* copy a font */
pl_font_t *pl_clone_font(const pl_font_t *src, gs_memory_t *mem, client_name_t cname);

/* Allocate the glyph table.  num_glyphs is just an estimate -- the table */
/* expands automatically as needed. */
int pl_font_alloc_glyph_table(pl_font_t *plfont, uint num_glyphs,
                              gs_memory_t *mem, client_name_t cname);

/* Allocate the glyph-to-character map for a downloaded TrueType font. */
int pl_tt_alloc_char_glyphs(pl_font_t *plfont, uint num_chars,
                            gs_memory_t *mem, client_name_t cname);

/* Fill in generic gs_font boilerplate. */
#ifndef gs_font_dir_DEFINED
#  define gs_font_dir_DEFINED	
typedef struct gs_font_dir_s gs_font_dir;
#endif
int pl_fill_in_font(gs_font *pfont, pl_font_t *plfont, gs_font_dir *pdir,
                    gs_memory_t *mem, const char *font_name);

/* Fill in bitmap and intellifont gs_font boilerplate. */
#ifndef gs_font_base_DEFINED
#  define gs_font_base_DEFINED
typedef struct gs_font_base_s gs_font_base;
#endif
void pl_fill_in_bitmap_font(gs_font_base *pfont, long unique_id);
void pl_fill_in_intelli_font(gs_font_base *pfont, long unique_id);
/* Initialize the callback procedures for a bitmap and intellifont
   fonts. */
void pl_bitmap_init_procs(gs_font_base *pfont);
void pl_intelli_init_procs(gs_font_base *pfont);
/* Fill in TrueType gs_font boilerplate. */
/* data = NULL for downloaded fonts, the TT data for complete fonts. */
#ifndef gs_font_type42_DEFINED
#  define gs_font_type42_DEFINED
typedef struct gs_font_type42_s gs_font_type42;
#endif
int pl_fill_in_tt_font(gs_font_type42 *pfont, void *data,
                       long unique_id);

/* Initialize the callback procedures for a TrueType font. */
void pl_tt_init_procs(gs_font_type42 *pfont);
/* Finish initializing a TrueType font. */
void pl_tt_finish_init(gs_font_type42 *pfont, bool downloaded);

/*
 * Set large_sizes, scaling_technology, character_complement, offsets
 * (for TrueType fonts), and resolution (for bitmap fonts) by scanning
 * the segments of a segmented downloaded font.
 * This is used for PCL5 Format 15 and 16 fonts and for PCL XL fonts.
 * fst_offset is the offset of the Font Scaling Technology and Variety bytes;
 * the segment data runs from start_offset up to end_offset.
 * large_sizes = false indicates 2-byte segment sizes, true indicates 4-byte.
 */
typedef struct pl_font_offset_errors_s {
  int illegal_font_data;
  int illegal_font_segment;	/* 0 means ignore unknown segments */
  /* 0 in any of the remaining values means return illegal_font_data */
  int illegal_font_header_fields;
  int illegal_null_segment_size;
  int missing_required_segment;
  int illegal_GT_segment;
  int illegal_GC_segment;
  int illegal_VT_segment;
  int illegal_BR_segment;
} pl_font_offset_errors_t;
int pl_font_scan_segments(const gs_memory_t *mem, 
			  pl_font_t *plfont, int fst_offset,
                          int start_offset, long end_offset,
                          bool large_sizes,
                          const pl_font_offset_errors_t *pfoe);

/* Load a built-in (TrueType) font from external storage. */
int pl_load_tt_font(stream *in, gs_font_dir *pdir, gs_memory_t *mem,
                    long unique_id, pl_font_t **pplfont, char *font_name);

/* allocate, read in and free tt font files to and from memory */
int pl_alloc_tt_fontfile_buffer(stream *in, gs_memory_t *mem, byte **pptt_font_data, ulong *size);
int pl_free_tt_fontfile_buffer(gs_memory_t *mem, byte *ptt_font_data);


/* Add a glyph to a font.  Return -1 if the table is full. */
int pl_font_add_glyph(pl_font_t *plfont, gs_glyph glyph, const byte *data);

/* Determine the escapement of a character in a font / symbol set. */
/* If the font is bound, the symbol set is ignored. */
/* If the character is undefined, set the escapement to (0,0) and return 1. */
/* If pwidth is NULL, don't store the escapement. */
int pl_font_char_width(const pl_font_t *plfont, const void *pgs, uint char_code, gs_point *pwidth);

/* Determine the character metrics.  If vertical substitution is in
   effect metrics[1] = lsb, metrics[3] = width otherwise metrics[0] =
   lsb and metrics 2 = width.   The same rules for character width apply */
int pl_font_char_metrics(const pl_font_t *plfont, const void *pgs, uint char_code, float metrics[4]);


/* Look up a glyph in a font.  Return a pointer to the glyph's slot */
/* (data != 0) or where it should be added (data == 0). */
pl_font_glyph_t *pl_font_lookup_glyph(const pl_font_t *plfont,
                                      gs_glyph glyph);

/* Look up a truetype character */
pl_tt_char_glyph_t *pl_tt_lookup_char(const pl_font_t *plfont, gs_glyph glyph);

/* Determine whether a font, with a given symbol set, includes a given */
/* character.  If the font is bound, the symbol set is ignored. */
#define pl_font_includes_char(plfont, maps, matrix, char_code)\
  (pl_font_char_width(plfont, maps, matrix, char_code, (gs_point *)0) == 0)

/* Remove a glyph from a font.  Return 1 if the glyph was present. */
int pl_font_remove_glyph(pl_font_t *plfont, gs_glyph glyph);

/* Free a font.  This is the freeing procedure in the font dictionary. */
void pl_free_font(gs_memory_t *mem, void *plf, client_name_t cname);

/* load resident font data to ram */
int pl_load_resident_font_data_from_file(gs_memory_t *mem, pl_font_t *plfont);

/* keep resident font data in its original file */
int pl_store_resident_font_data_in_file(char *font_file, gs_memory_t *mem, pl_font_t *plfont);

/* check if the font is zlib deflated.  This test is sufficient
   because we are only checking if the header is a TT header or a zlib
   compressed header, exclusively. */
#define pl_is_tt_zlibC(header)\
    ((header[0] & 0xf) == 8)
/* agfa requires prepending a dummy header to xl fonts */
int pl_prepend_xl_dummy_header(gs_memory_t *mem, byte **ppheader);
/* agfa requires swapping downloaded intellifont headers */
int pl_swap_header(byte *header, uint gifct);

/* UFST callbacks if needed */
void plu_set_callbacks(void);

/* disable component metrics in a TrueType glyph - emulate an HP printer bug */
int pl_font_disable_composite_metrics(pl_font_t *plfont, gs_glyph glyph);

pl_font_t *pl_lookup_font_by_pjl_number(pl_dict_t *pfontdict, int pjl_font_number);
#endif				/* plfont_INCLUDED */
