/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* No shortage of gslib headers to include... */

#ifndef GSLITE
#define GSLITE
#endif

#include <stdlib.h>
#include "stdio_.h"
#include "math_.h"
#include "string_.h"

#include "gp.h"

#include "gscdefs.h"
#include "gserrors.h"
#include "gslib.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gscoord.h"
#include "gspaint.h"
#include "gspath.h"
#include "gspath2.h"
#include "gsutil.h"
#include "gsgdata.h"

#include "gx.h"
#include "gxdevice.h"
#include "gxmatrix.h"
#include "gxpath.h"

#include "gxfont.h"
#include "gxchar.h"
#include "gxtype1.h"
#include "gxfont1.h"
#include "gxfont42.h"
#include "gxfcache.h"

#include "gzstate.h"
#include "gzpath.h"

/*
 * Font API internals.
 */

struct gslt_font_s
{
    byte *data;
    int length;
    gs_font *font;

    int subfontid;
    int cmaptable;
    int cmapsubcount;
    int cmapsubtable;
    int usepua;

    /* these are for CFF opentypes only */
    byte *cffdata;
    byte *cffend;
    byte *gsubrs;
    byte *subrs;
    byte *charstrings;
};

int gslt_find_sfnt_table(struct gslt_font_s *xf, char *name, int *lengthp);
int gslt_load_sfnt_cmap(struct gslt_font_s *xf);
int gslt_init_truetype_font(gs_memory_t *mem, gs_font_dir *xfc, struct gslt_font_s *xf);
int gslt_init_postscript_font(gs_memory_t *mem, gs_font_dir *xfc, struct gslt_font_s *xf);

/*
 * Big-endian memory accessor functions
 */

static inline int s16(byte *p)
{
    return (signed short)( (p[0] << 8) | p[1] );
}

static inline int u16(byte *p)
{
    return (p[0] << 8) | p[1];
}

static inline int u24(byte *p)
{
    return (p[0] << 16) | (p[1] << 8) | p[2];
}

static inline int u32(byte *p)
{
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}
