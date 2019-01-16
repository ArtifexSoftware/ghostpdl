/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* pgfont.h */
/* Definitions for HP-GL/2 stick and arc fonts */

#ifndef pgfont_INCLUDED
#  define pgfont_INCLUDED

#include "pgmand.h"

/* Fill in stick/arc font boilerplate. */
void hpgl_fill_in_stick_font(gs_font_base * pfont, long unique_id);

void hpgl_fill_in_arc_font(gs_font_base * pfont, long unique_id);

void hpgl_initialize_stick_fonts(hpgl_state_t * pcs);

void hpgl_fill_in_531_font(gs_font_base * pfont, long unique_id);

#endif /* pgfont_INCLUDED */
