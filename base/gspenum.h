/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Common definitions for client interface to path enumeration */

#ifndef gspenum_INCLUDED
#  define gspenum_INCLUDED

/* Opaque type for a path */
typedef struct gx_path_s gx_path;

/* Define the path element types. */
#define gs_pe_moveto 1
#define gs_pe_lineto 2
#define gs_pe_curveto 3
#define gs_pe_closepath 4
#define gs_pe_gapto 5

/* Define an abstract type for the path enumerator. */
typedef struct gs_path_enum_s gs_path_enum;

#endif /* gspenum_INCLUDED */
