/* Copyright (C) 1995, 1997 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Line parameter definitions */

#ifndef gslparam_INCLUDED
#  define gslparam_INCLUDED

/* Line cap values */
typedef enum {
    gs_cap_butt = 0,
    gs_cap_round = 1,
    gs_cap_square = 2,
    gs_cap_triangle = 3		/* not supported by PostScript */
} gs_line_cap;

#define gs_line_cap_max 3

/* Line join values */
typedef enum {
    gs_join_miter = 0,
    gs_join_round = 1,
    gs_join_bevel = 2,
    gs_join_none = 3,		/* not supported by PostScript */
    gs_join_triangle = 4	/* not supported by PostScript */
} gs_line_join;

#define gs_line_join_max 4

#endif /* gslparam_INCLUDED */
