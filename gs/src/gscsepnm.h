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

/*$RCSfile$ $Revision$ */
/* Standard color space separation names */

#ifndef gscsepnm_INCLUDED
#  define gscsepnm_INCLUDED

/*
 * Define enumeration indices for the standard separation names, and the
 * corresponding name strings. These are only used internally: in all
 * externally accessible APIs, separations are defined either by a string
 * name or by an opaque identifier.
 *
 * NB: the enumeration and the list of strings must be synchronized.  */
typedef enum {
    gs_ht_separation_Default,	/* must be first */
    gs_ht_separation_Gray,
    gs_ht_separation_Red,
    gs_ht_separation_Green,
    gs_ht_separation_Blue,
    gs_ht_separation_Cyan,
    gs_ht_separation_Magenta,
    gs_ht_separation_Yellow,
    gs_ht_separation_Black
} gs_ht_separation_name;

#define gs_ht_separation_name_strings            \
    "Default", "Gray", "Red", "Green", "Blue",   \
    "Cyan", "Magenta", "Yellow", "Black"

#endif /* gscsepnm_INCLUDED */
