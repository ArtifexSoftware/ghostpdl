/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pgfont.h */
/* Definitions for HP-GL/2 stick and arc fonts */

#ifndef pgfont_INCLUDED
#  define pgfont_INCLUDED

/* Fill in stick/arc font boilerplate. */
void hpgl_fill_in_stick_font(P2(gs_font_base *pfont, long unique_id));
void hpgl_fill_in_arc_font(P2(gs_font_base *pfont, long unique_id));

#endif                       /* pgfont_INCLUDED */
