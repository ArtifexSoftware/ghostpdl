/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Common definitions for client interface to path enumeration */

#ifndef gspenum_INCLUDED
#  define gspenum_INCLUDED

/* Define the path element types. */
#define gs_pe_moveto 1
#define gs_pe_lineto 2
#define gs_pe_curveto 3
#define gs_pe_closepath 4

/* Define an abstract type for the path enumerator. */
typedef struct gs_path_enum_s gs_path_enum;

#endif /* gspenum_INCLUDED */
