/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* plvocab.h */
/* Maps between glyph vocabularies */

/* Define an entry in a glyph mapping table. */
/* Glyph mapping tables are sorted by key. */
typedef struct pl_glyph_mapping_s {
  ushort key;
  ushort value;
} pl_glyph_mapping_t;

/* Map from MSL to Unicode. */
extern const far_data pl_glyph_mapping_t pl_map_MSL_to_Unicode[];
extern const uint pl_map_MSL_to_Unicode_size;
