/* Copyright (C) 2001-2020 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


#ifndef plftable_INCLUDED
#  define plftable_INCLUDED

/* plftable.h */
/* resident font table */
typedef struct font_resident
{
    const char full_font_name[3][40];      /* name entry 4 in truetype fonts */
    const short unicode_fontname[16];   /* pxl name */
    pl_font_params_t params;
    byte character_complement[8];
    pl_font_type_t font_type;
} font_resident_t;

enum {
        AGFASCREENNAME = 0,
        AGFANAME = 1,
        URWNAME = 2
};

extern const font_resident_t pl_built_in_resident_font_table[];
extern const int pl_built_in_resident_font_table_count;

#endif /* plftable_INCLUDED */
