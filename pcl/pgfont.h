/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgfont.h */
/* Definitions for HP-GL/2 stick and arc fonts */

#ifndef pgfont_INCLUDED
#  define pgfont_INCLUDED

#include "stdpre.h"
#include "gstypes.h"

/* Data structures to support stick fonts follow.  HAS -- For now
   these are defined as constant structures in the code.  It may be
   desirable to place the data in files in the future. */

/* define operations to be used in the font data definitions. */

typedef enum {
  hpgl_char_pen_up,
  hpgl_char_pen_down,
  hpgl_char_pen_down_no_args,
  hpgl_char_pen_relative,
  hpgl_char_arc_relative,
  hpgl_char_end
} hpgl_character_operation;


/* structure of coordinates and operation.  At each vertex of a
   stick font we can either raise the pen and move to the desired
   location or lower the pen and move to the next coordinate.  
   desired location or pen to the desired coordinate or next
   coordinate or by default "line to" the next coordinate.  The new  */

typedef struct {
  hpgl_character_operation operation;
  gs_point vertex;
} hpgl_character_point;
    

/* HAS -- for now define all the other character sets as aliases to
   the ascii character set */

extern const hpgl_character_point *hpgl_ascii_char_set[];

#endif                       /* pgfont_INCLUDED */



