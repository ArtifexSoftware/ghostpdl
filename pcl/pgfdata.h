/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pgfdata.h */
/* Interface to HP-GL/2 stick and arc font data */

/* font type - stick or arc */
typedef enum {
    HPGL_ARC_FONT,
    HPGL_STICK_FONT
} hpgl_font_type_t;

/* Enumerate the segments of a stick/arc character. */
int hpgl_stick_arc_segments(P3(void *data, uint char_index, hpgl_font_type_t font_type));

/* Get the unscaled width of a stick/arc character. */
int hpgl_stick_arc_width(P2(uint char_index, hpgl_font_type_t font_type));
