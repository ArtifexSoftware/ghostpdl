/* Copyright (C) 1993 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* External font client definitions for Ghostscript library */

#ifndef gsxfont_INCLUDED
#  define gsxfont_INCLUDED

/* Define a character glyph identifier.  This is opaque, probably an index */
/* into the font.  Glyph identifiers are font-specific. */
typedef ulong gx_xglyph;

#define gx_no_xglyph ((gx_xglyph)~0L)

/* Structure for xfont procedures. */
struct gx_xfont_procs_s;
typedef struct gx_xfont_procs_s gx_xfont_procs;

/* A generic xfont. */
struct gx_xfont_s;
typedef struct gx_xfont_s gx_xfont;

#endif /* gsxfont_INCLUDED */
