/* Copyright (C) 1989, 1992, 1993 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Erase/fill/stroke procedures */
/* Requires gsstate.h */

#ifndef gspaint_INCLUDED
#  define gspaint_INCLUDED

/* Painting */
int gs_erasepage(P1(gs_state *)),
    gs_fillpage(P1(gs_state *)),
    gs_fill(P1(gs_state *)),
    gs_eofill(P1(gs_state *)),
    gs_stroke(P1(gs_state *));

/* Image tracing */
int gs_imagepath(P4(gs_state *, int, int, const byte *));

#endif /* gspaint_INCLUDED */
