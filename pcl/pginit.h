/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pginit.h */
/* Interface to initialize/reset procedures in pginit.c */

/* Reset a set of font parameters to their default values. */
void hpgl_default_font_params(P1(pcl_font_selection_t *pfs));

/* Reset a pen to its default color. */
void hpgl_default_pen_color(P2(hpgl_state_t *pgls, int pen));

/* Reset a fill pattern to solid fill.  The index is 1-origin, */
/* as for the RF command.  free = true means free the old pattern if any. */
void hpgl_default_fill_pattern(P3(hpgl_state_t *pgls, int index, bool free));

/* Reset (parts of) the HP-GL/2 state. */
void hpgl_do_reset(P2(pcl_state_t *pcls, pcl_reset_type_t type));
