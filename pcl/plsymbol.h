/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* plsymbol.h */
/* PCL symbol set mapping definitions */

#ifndef plsymbol_INCLUDED
#  define plsymbol_INCLUDED

/*
 * Define the glyph vocabularies -- glyph numbering systems that are the
 * targets for symbol sets.  Currently we only support Unicode and MSL,
 * but we might add PostScript symbolic names or CIDs in the future.
 *
 * We use the same numbering as for the low 3 bits of the PCL character
 * requirements.
 */
typedef enum {
  plgv_MSL = 0,
  plgv_Unicode = 1,
  plgv_next
} pl_glyph_vocabulary_t;

/*
 * The following structure is defined by PCL5.  See Figure 10-1 on p. 10-5
 * of the PCL5 Technical Reference Manual.  Note that a symbol set may have
 * two maps, one each for Unicode and MSL.  A symbol map is uniquely iden-
 * tified by its id and format.
 */
typedef struct pl_symbol_map_s {
  byte header_size[2];
  byte id[2];
  byte format;
  byte type;
  byte first_code[2];
  byte last_code[2];
  byte character_requirements[8];
  /*
   * Note that the codes are stored with native byte order.
   * If necessary, we byte-swap them after downloading.
   */
  ushort codes[256];		/* may be more or less for downloaded maps */
} pl_symbol_map_t;

#define pl_symbol_map_vocabulary(map)\
  ((pl_glyph_vocabulary_t)((map)->character_requirements[7] & 7))

/*
 * Define the built-in symbol set mappings.  The list is terminated by
 * a NULL.
 */
extern const pl_symbol_map_t *pl_built_in_symbol_maps[];
extern const int pl_built_in_symbol_map_count;

#endif				/* plsymbol_INCLUDED */
