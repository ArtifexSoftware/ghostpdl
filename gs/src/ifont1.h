/* Copyright (C) 1998, 1999 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Type 1 font utilities shared with Type 2 */

#ifndef ifont1_INCLUDED
#  define ifont1_INCLUDED

/*
 * Define the temporary structure for holding pointers to substructures of a
 * CharString-based font.  This is used for parsing Type 1, 2, and 4 fonts.
 */
typedef struct charstring_font_refs_s {
    ref *Private;
    ref no_subrs;
    ref *OtherSubrs;
    ref *Subrs;
    ref *GlobalSubrs;
} charstring_font_refs_t;

/*
 * Parse the substructures of a CharString-based font.
 */
int charstring_font_get_refs(P2(os_ptr op, charstring_font_refs_t *pfr));

/*
 * Finish building a CharString-based font.  The client has filled in
 * pdata1->interpret, subroutineNumberBias, lenIV, and (if applicable)
 * the Type 2 elements.
 */
int build_charstring_font(P7(i_ctx_t *i_ctx_p, os_ptr op,
			     build_proc_refs * pbuild, font_type ftype,
			     charstring_font_refs_t *pfr,
			     gs_type1_data *pdata1,
			     build_font_options_t options));

#endif /* ifont1_INCLUDED */
