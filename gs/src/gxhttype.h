/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* gxhttype.h */
/* Client halftone type enumeration */

#ifndef gxhttype_INCLUDED
#  define gxhttype_INCLUDED

/* Halftone types */
typedef enum {
	ht_type_none,			/* is this needed? */
	ht_type_screen,			/* set by setscreen */
	ht_type_colorscreen,		/* set by setcolorscreen */
	ht_type_spot,			/* Type 1 halftone dictionary */
	ht_type_threshold,		/* Type 3 halftone dictionary */
	ht_type_multiple,		/* Type 5 halftone dictionary */
	ht_type_multiple_colorscreen	/* Type 5 halftone dictionary */
					/* created from Type 2 or Type 4 */
					/* halftone dictionary  */
} gs_halftone_type;

#endif				/* gxhttype_INCLUDED */
