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


/* pcsymbol.h */
/* Definitions for PCL5 symbol sets */

#ifndef pcsymbol_INCLUDED
#  define pcsymbol_INCLUDED

#include "plsymbol.h"

/* The structure for symbol sets (fig 10-1, p. 10-5 of PCL5 TRM) is in
 * plsymbol.h, as a "symbol map".  A symbol map describes the mapping
 * from one symbol set to one glyph vocabulary.  A symbol set may
 * comprise multiple maps (currently at most two, one each for MSL and
 * Unicode).  We want a separate dictionary of symbol sets because
 * there are various operations performed per symbol set (affecting
 * all mappings).
 */

typedef struct pcl_symbol_set_s
{
    pcl_data_storage_t storage;
    pl_symbol_map_t *maps[plgv_next];   /* (these may be NULL) */
} pcl_symbol_set_t;

/* Check whether a symbol map's character requirements are supported by a
 * font's character complement. */
bool pcl_check_symbol_support(const byte * symset_req, const byte * font_sup);

/* Find a symbol map, given its ID and glyph vocabulary. */
pl_symbol_map_t *pcl_find_symbol_map(const pcl_state_t * pcs,
                                     const byte * id,
                                     pl_glyph_vocabulary_t gv, bool wide16);

#endif /* pcsymbol_INCLUDED */
