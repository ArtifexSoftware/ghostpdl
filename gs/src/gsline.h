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

/*$RCSfile$ $Revision$ */
/* Line parameter and quality definitions */

#ifndef gsline_INCLUDED
#  define gsline_INCLUDED

#include "gslparam.h"

/* Procedures */
int gs_setlinewidth(P2(gs_state *, floatp));
float gs_currentlinewidth(P1(const gs_state *));
int gs_setlinecap(P2(gs_state *, gs_line_cap));
gs_line_cap gs_currentlinecap(P1(const gs_state *));
int gs_setlinejoin(P2(gs_state *, gs_line_join));
gs_line_join gs_currentlinejoin(P1(const gs_state *));
int gs_setmiterlimit(P2(gs_state *, floatp));
float gs_currentmiterlimit(P1(const gs_state *));
int gs_setdash(P4(gs_state *, const float *, uint, floatp));
uint gs_currentdash_length(P1(const gs_state *));
const float *gs_currentdash_pattern(P1(const gs_state *));
float gs_currentdash_offset(P1(const gs_state *));
int gs_setflat(P2(gs_state *, floatp));
float gs_currentflat(P1(const gs_state *));
int gs_setstrokeadjust(P2(gs_state *, bool));
bool gs_currentstrokeadjust(P1(const gs_state *));

/* Extensions - device-independent */
void gs_setdashadapt(P2(gs_state *, bool));
bool gs_currentdashadapt(P1(const gs_state *));
int gs_setcurvejoin(P2(gs_state *, int));
int gs_currentcurvejoin(P1(const gs_state *));

/* Extensions - device-dependent */
void gs_setaccuratecurves(P2(gs_state *, bool));
bool gs_currentaccuratecurves(P1(const gs_state *));
int gs_setdotlength(P3(gs_state *, floatp, bool));
float gs_currentdotlength(P1(const gs_state *));
bool gs_currentdotlength_absolute(P1(const gs_state *));
int gs_setdotorientation(P1(gs_state *));
int gs_dotorientation(P1(gs_state *));

/* Imager-level procedures */
#ifndef gs_imager_state_DEFINED
#  define gs_imager_state_DEFINED
typedef struct gs_imager_state_s gs_imager_state;
#endif
int gs_imager_setflat(P2(gs_imager_state *, floatp));
bool gs_imager_currentdashadapt(P1(const gs_imager_state *));
bool gs_imager_currentaccuratecurves(P1(const gs_imager_state *));

#endif /* gsline_INCLUDED */
