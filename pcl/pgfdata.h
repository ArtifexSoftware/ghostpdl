/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgfdata.h */
/* Interface to HP-GL/2 stick and arc font data */

/* We use 'stick' instead of 'stick_or_arc' below just because it's shorter. */

/* Define the number of symbols in the stick/arc font. */
#define hpgl_stick_num_symbols (256+20)

/* The arc font is just a condensed version of the stick font, */
/* with minimal inter-character spacing. */
#define hpgl_arc_font_condensation 0.70

/* Define the procedures for processing segments. */
typedef int (*hpgl_stick_move_proc_t)(P3(void *data, int dx, int dy));
typedef int (*hpgl_stick_line_proc_t)(P3(void *data, int dx, int dy));
typedef int (*hpgl_stick_arc_proc_t)(P6(void *data, int cx, int cy,
  const gs_int_point *start, int sweep_angle, floatp chord_angle));
typedef struct hpgl_stick_segment_procs_s {
  hpgl_stick_move_proc_t move;
  hpgl_stick_line_proc_t line;
  hpgl_stick_arc_proc_t arc;
} hpgl_stick_segment_procs_t;

/* Enumerate the segments of a stick/arc character. */
/* We break this out so that we can use it for calculating escapements. */
int hpgl_stick_arc_segments(P4(const hpgl_stick_segment_procs_t *procs,
			       void *data, uint char_index,
			       floatp chord_angle));

/* Get the unscaled width of a stick/arc character. */
int hpgl_stick_arc_width(P1(uint char_index));
