/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Public graphics state API */

#ifndef gsstate_INCLUDED
#  define gsstate_INCLUDED

#include "std.h"
#include "gsgstate.h"
#include "gsovrc.h"

/* Initial allocation and freeing */
gs_gstate *gs_gstate_alloc(gs_memory_t *);	/* 0 if fails */
void gs_gstate_free(gs_gstate *);
void gs_gstate_free_chain(gs_gstate *);

/* Initialization, saving, restoring, and copying */
int gs_gsave(gs_gstate *), gs_grestore(gs_gstate *), gs_grestoreall(gs_gstate *);
int gs_grestore_only(gs_gstate *);
int gs_gsave_for_save(gs_gstate *, gs_gstate **), gs_grestoreall_for_restore(gs_gstate *, gs_gstate *);

gs_gstate *gs_gstate_copy(const gs_gstate *, gs_memory_t *);
int gs_copygstate(gs_gstate * /*to */ , const gs_gstate * /*from */ ),
      gs_currentgstate(gs_gstate * /*to */ , const gs_gstate * /*from */ ),
      gs_setgstate(gs_gstate * /*to */ , const gs_gstate * /*from */ );

int gs_gstate_update_overprint(gs_gstate *, const gs_overprint_params_t *);
bool gs_currentoverprint(const gs_gstate *);
void gs_setoverprint(gs_gstate *, bool);
bool gs_currentstrokeoverprint(const gs_gstate *);
void gs_setstrokeoverprint(gs_gstate *, bool);
bool gs_currentfilloverprint(const gs_gstate *);
void gs_setfilloverprint(gs_gstate *, bool);

int gs_currentoverprintmode(const gs_gstate *);
int gs_setoverprintmode(gs_gstate *, int);

int gs_do_set_overprint(gs_gstate *);

int gs_currentrenderingintent(const gs_gstate *);
int gs_setrenderingintent(gs_gstate *, int);

int gs_currentblackptcomp(const gs_gstate *);
int gs_setblackptcomp(gs_gstate *, int);

int gs_initgraphics(gs_gstate *);
int gs_initgraphics_no_cspace(gs_gstate *);

bool gs_currentcpsimode(const gs_memory_t *);
void gs_setcpsimode(gs_memory_t *, bool);

int gs_getscanconverter(const gs_memory_t *);
void gs_setscanconverter(gs_gstate *, int);

/* Device control */
#include "gsdevice.h"

/* Line parameters and quality */
#include "gsline.h"

/* Color and gray */
#include "gscolor.h"

/* Halftone screen */
#include "gsht.h"
#include "gscsel.h"
int gs_setscreenphase(gs_gstate *, int, int, gs_color_select_t);
int gs_currentscreenphase(const gs_gstate *, gs_int_point *, gs_color_select_t);

#define gs_sethalftonephase(pgs, px, py)\
  gs_setscreenphase(pgs, px, py, gs_color_select_all)
#define gs_currenthalftonephase(pgs, ppt)\
  gs_currentscreenphase(pgs, ppt, 0)
int gx_gstate_setscreenphase(gs_gstate *, int, int, gs_color_select_t);

/* Miscellaneous */
int gs_setfilladjust(gs_gstate *, double, double);
int gs_currentfilladjust(const gs_gstate *, gs_point *);
void gs_setlimitclamp(gs_gstate *, bool);
bool gs_currentlimitclamp(const gs_gstate *);
void gs_settextrenderingmode(gs_gstate * pgs, uint trm);
uint gs_currenttextrenderingmode(const gs_gstate * pgs);
int gs_settextspacing(gs_gstate *pgs, double Tc);
double gs_currenttextspacing(const gs_gstate *pgs);
int gs_settextleading(gs_gstate *pgs, double TL);
double gs_currenttextleading(const gs_gstate *pgs);
int gs_settextrise(gs_gstate *pgs, double Ts);
double gs_currenttextrise(const gs_gstate *pgs);
int gs_setwordspacing(gs_gstate *pgs, double Tz);
double gs_currentwordspacing(const gs_gstate *pgs);
int gs_settexthscaling(gs_gstate *pgs, double Tw);
double gs_currenttexthscaling(const gs_gstate *pgs);
int gs_settextlinematrix(gs_gstate *pgs, gs_matrix *m);
int gs_gettextlinematrix(gs_gstate *pgs, gs_matrix *m);
int gs_settextmatrix(gs_gstate *pgs, gs_matrix *m);
int gs_gettextmatrix(gs_gstate *pgs, gs_matrix *m);
int gs_setPDFfontsize(gs_gstate *pgs, double Tw);
double gs_currentPDFfontsize(const gs_gstate *pgs);

#include "gscpm.h"
gs_in_cache_device_t gs_incachedevice(const gs_gstate *);
void gs_sethpglpathmode(gs_gstate *, bool);
bool gs_currenthpglpathmode(const gs_gstate *);

#endif /* gsstate_INCLUDED */
