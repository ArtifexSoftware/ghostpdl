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

#define pl_complement_to_vocab(complement)\
    ((complement[7] & 7) == 6 ? plgv_Unicode : plgv_MSL)

/* Define mapping types so that we do not have to implement both
 * unicode and msl symbol tables.  These definitions can be used with
 * the mapping function defined in pl/plvocab.h to map to and from msl
 * and unicode. There are 3 mutually exclusive possibilities: (1) a
 * unicode symbol set that can be mapped to msl, (2) an msl symbol set
 * that can be mapped to unicode and (3) neither (1) or (2).  
 */

#define PLGV_M2U_MAPPING 1
#define PLGV_U2M_MAPPING 2
#define PLGV_NO_MAPPING 3
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
  byte mapping_type;
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

/* lookup symbol in symbol set.  Resident fonts will index implicitly
   if the symbol set in null.  We cheat here and use a ulong instead
   of gs_char to avoid pulling in all the gs_char graphics library
   dependencies. */
ulong pl_map_symbol(const pl_symbol_map_t *psm, uint chr, bool is_resident_font, 
                    bool is_MSL, bool is_590);

/* supported pcl and xl wide encodings - 4 Asian encodings and an an
   undocumented unicode variant which we haven't conclusively
   identified (maybe ucs/2 or utf-16).

    619 Japanese WIN31J
    579 Simplified Chinese GB2312
    616 Korean KSC5601
    596 Traditional Chinese BIG5
    590 unicode
*/

#define pl_wide_encoding(ss) \
    ((ss) == 619 || (ss) == 579 || (ss) == 616 || (ss) == 596 || (ss) == 590)

#endif				/* plsymbol_INCLUDED */
