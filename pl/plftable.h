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
/*$Id$*/

#ifndef plftable_INCLUDED
#  define plftable_INCLUDED

/* plftable.h */
/* resident font table */
typedef struct font_resident {
    const char full_font_name[40];    /* name entry 4 in truetype fonts */
    const short unicode_fontname[16]; /* pxl name */
    pl_font_params_t params;
    byte character_complement[8];
    pl_font_type_t font_type;
} font_resident_t;

extern const font_resident_t resident_table[];

extern const int pl_built_in_resident_font_table_count;

#endif                       /* plftable_INCLUDED */
