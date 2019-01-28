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


/* Line parameter and quality definitions */

#ifndef gsline_INCLUDED
#  define gsline_INCLUDED

#include "stdpre.h"
#include "gsgstate.h"
#include "gslparam.h"

/* Procedures */
int gs_setlinewidth(gs_gstate *, double);
float gs_currentlinewidth(const gs_gstate *);
int gs_setlinecap(gs_gstate *, gs_line_cap);
gs_line_cap gs_currentlinecap(const gs_gstate *);
int gs_setlinestartcap(gs_gstate *, gs_line_cap);
int gs_setlineendcap(gs_gstate *, gs_line_cap);
int gs_setlinedashcap(gs_gstate *, gs_line_cap);
int gs_setlinejoin(gs_gstate *, gs_line_join);
gs_line_join gs_currentlinejoin(const gs_gstate *);
int gs_setmiterlimit(gs_gstate *, double);
float gs_currentmiterlimit(const gs_gstate *);
int gs_setdash(gs_gstate *, const float *, uint, double);
uint gs_currentdash_length(const gs_gstate *);
const float *gs_currentdash_pattern(const gs_gstate *);
float gs_currentdash_offset(const gs_gstate *);
int gs_setflat(gs_gstate *, double);
float gs_currentflat(const gs_gstate *);
int gs_setstrokeadjust(gs_gstate *, bool);
bool gs_currentstrokeadjust(const gs_gstate *);

/* Extensions - device-independent */
void gs_setdashadapt(gs_gstate *, bool);
bool gs_currentdashadapt(const gs_gstate *);
int gs_setcurvejoin(gs_gstate *, int);
int gs_currentcurvejoin(const gs_gstate *);

/* Extensions - device-dependent */
void gs_setaccuratecurves(gs_gstate *, bool);
bool gs_currentaccuratecurves(const gs_gstate *);
int gs_setdotlength(gs_gstate *, double, bool);
float gs_currentdotlength(const gs_gstate *);
bool gs_currentdotlength_absolute(const gs_gstate *);
int gs_setdotorientation(gs_gstate *);
int gs_dotorientation(gs_gstate *);

/* gs_gstate-level procedures */
int gs_gstate_setflat(gs_gstate *, double);
bool gs_gstate_currentdashadapt(const gs_gstate *);
bool gs_gstate_currentaccuratecurves(const gs_gstate *);

#endif /* gsline_INCLUDED */
