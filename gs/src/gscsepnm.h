/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

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
