/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgfont.h */
/* Definitions for HP-GL/2 stick and arc fonts */

#ifndef pgfont_INCLUDED
#  define pgfont_INCLUDED

/* Compute the escapement of a symbol. */
int hpgl_stick_char_width(P2(uint char_index, floatp *escapement));
int hpgl_arc_char_width(P2(uint char_index, floatp *escapement));

/* Append a symbol to the GL/2 path. */
int hpgl_stick_append_char(P2(hpgl_state_t *pgls, uint char_index));
int hpgl_arc_append_char(P3(hpgl_state_t *pgls, uint char_index,
			    floatp chord_angle));

#endif                       /* pgfont_INCLUDED */
