/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* plfont.h */
/* Interface to PCL font utilities */

#ifndef plfont_INCLUDED
#  define plfont_INCLUDED

#include "gsccode.h"
#include "plsymbol.h"

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
  plfst_bitmap = 254
} pl_font_scaling_technology_t;
#define pl_font_is_scalable(plfont)\
  ((plfont)->scaling_technology != plfst_bitmap)

/* Define the matching characteristics for PCL5 fonts. */
typedef struct pl_font_params_s {
  uint symbol_set;
  bool proportional_spacing;
  uint pitch_100ths;
  uint height_4ths;
  uint style;
  int stroke_weight;
  uint typeface_family;
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

/*
 * We 'wrap' gs_fonts in our own structure.
 *
 * Note that we use the WMode of fonts in a slightly unusual way:
 * the low-order bit indicates that vertical substitutions should be used,
 * and the other 7 bits are a fraction for synthetic bolding.
 */
#ifndef pl_font_t_DEFINED
#  define pl_font_t_DEFINED
typedef struct pl_font_s pl_font_t;
#endif
struct pl_font_s {
  gs_font *pfont;	/* Type 42 if TrueType, Type 3 if bitmap. */
  int storage;		/* where the font is stored */
  const byte *header;	/* downloaded header, or built-in font data */
  ulong header_size;
	/* Information extracted from the font or supplied by the client. */
  pl_font_scaling_technology_t scaling_technology;
  pl_font_type_t font_type;
  /* Implementation of pl_font_char_width, see below */
  int (*char_width)(P4(const pl_font_t *plfont,
		       const pl_symbol_map_collection_t *maps,
		       uint char_code, gs_point *pwidth));
  bool large_sizes;	/* segment sizes are 32 bits if true, 16 if false */
			/* (for segmented fonts only) */
  struct { uint x, y; } resolution;	/* resolution (for bitmap fonts) */
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
};
#define private_st_pl_font()	/* in plfont.c */\
  gs_private_st_ptrs4(st_pl_font, pl_font_t, "pl_font_t",\
    pl_font_enum_ptrs, pl_font_reloc_ptrs, pfont, header, glyphs.table,\
      char_glyphs.table)

/* ---------------- Procedural interface ---------------- */

/* Allocate and minimally initialize a font. */
pl_font_t *pl_alloc_font(P2(gs_memory_t *mem, client_name_t cname));

/* Allocate the glyph table.  num_glyphs is just an estimate -- the table */
/* expands automatically as needed. */
int pl_font_alloc_glyph_table(P4(pl_font_t *plfont, uint num_glyphs,
				 gs_memory_t *mem, client_name_t cname));

/* Allocate the glyph-to-character map for a downloaded TrueType font. */
int pl_tt_alloc_char_glyphs(P4(pl_font_t *plfont, uint num_chars,
			       gs_memory_t *mem, client_name_t cname));

/* Fill in generic gs_font boilerplate. */
#ifndef gs_font_dir_DEFINED
#  define gs_font_dir_DEFINED	
typedef struct gs_font_dir_s gs_font_dir;
#endif
int pl_fill_in_font(P4(gs_font *pfont, pl_font_t *plfont, gs_font_dir *pdir,
		       gs_memory_t *mem));

/* Fill in bitmap gs_font boilerplate. */
#ifndef gs_font_base_DEFINED
#  define gs_font_base_DEFINED
typedef struct gs_font_base_s gs_font_base;
#endif
void pl_fill_in_bitmap_font(P2(gs_font_base *pfont, long unique_id));
/* Initialize the callback procedures for a bitmap font. */
void pl_bitmap_init_procs(P1(gs_font_base *pfont));

/* Fill in TrueType gs_font boilerplate. */
/* data = NULL for downloaded fonts, the TT data for complete fonts. */
#ifndef gs_font_type42_DEFINED
#  define gs_font_type42_DEFINED
typedef struct gs_font_type42_s gs_font_type42;
#endif
void pl_fill_in_tt_font(P3(gs_font_type42 *pfont, void *data,
			   long unique_id));
/* Initialize the callback procedures for a TrueType font. */
void pl_tt_init_procs(P1(gs_font_type42 *pfont));
/* Finish initializing a TrueType font. */
void pl_tt_finish_init(P2(gs_font_type42 *pfont, bool downloaded));

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
int pl_font_scan_segments(P6(pl_font_t *plfont, int fst_offset,
			     int start_offset, long end_offset,
			     bool large_sizes,
			     const pl_font_offset_errors_t *pfoe));

/* Load a built-in (TrueType) font from external storage. */
/* Attempt to define this only if we have stdio included. */
#ifdef getc
int pl_load_tt_font(P5(FILE *in, gs_font_dir *pdir, gs_memory_t *mem,
		       long unique_id, pl_font_t **pplfont));
#endif

/* Add a glyph to a font.  Return -1 if the table is full. */
int pl_font_add_glyph(P3(pl_font_t *plfont, gs_glyph glyph, const byte *data));

/* Determine the escapement of a character in a font / symbol set. */
/* If the font is bound, the symbol set is ignored. */
/* If the character is undefined, set the escapement to (0,0) and return 1. */
/* If pwidth is NULL, don't store the escapement. */
#define pl_font_char_width(plfont, maps, char_code, pwidth)\
  (*(plfont)->char_width)(plfont, maps, char_code, pwidth)

/* Determine whether a font, with a given symbol set, includes a given */
/* character.  If the font is bound, the symbol set is ignored. */
#define pl_font_includes_char(plfont, maps, char_code)\
  (pl_font_char_width(plfont, maps, char_code, NULL) == 0)

/* Free a font.  This is the freeing procedure in the font dictionary. */
void pl_free_font(P3(gs_memory_t *mem, void *plf, client_name_t cname));

#endif				/* plfont_INCLUDED */
