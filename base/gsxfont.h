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


/* External font client definitions for Ghostscript library */

#ifndef gsxfont_INCLUDED
#  define gsxfont_INCLUDED

#include "stdpre.h"

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
