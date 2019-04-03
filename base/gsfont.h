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


/* Generic font and font cache interface */

#ifndef gsfont_INCLUDED
#  define gsfont_INCLUDED

#include "std.h"
#include "gsgstate.h"
#include "gstypes.h"
#include "gsmatrix.h"
#include "gslibctx.h"

/* Font objects */
typedef struct gs_font_s gs_font;

/* A font object as seen by clients. */
/* See the PostScript Language Reference Manual for details. */

typedef struct gs_show_enum_s gs_show_enum;

/* Initialization */
/* These procedures return 0 if they fail. */
/* The returned gs_font_dir can be cleanly deallocated by
 * simply calling gs_free_object on it. */
gs_font_dir *gs_font_dir_alloc2(gs_memory_t * struct_mem,
                                gs_memory_t * bits_mem);
gs_font_dir *gs_font_dir_alloc2_limits(gs_memory_t * struct_mem,
                                       gs_memory_t * bits_mem,
                                       uint smax, uint bmax, uint mmax,
                                       uint cmax, uint upper);

/* Backward compatibility */
#define gs_font_dir_alloc(mem) gs_font_dir_alloc2(mem, mem)
#define gs_font_dir_alloc_limits(mem, smax, bmax, mmax, cmax, upper)\
  gs_font_dir_alloc2_limits(mem, mem, smax, bmax, mmax, cmax, upper)

/* Font manipulations */
/* Use gs_definefont only with original (unscaled) fonts! */
int gs_definefont(gs_font_dir *, gs_font *);

/* Find a sililar registered font of same type. */
int gs_font_find_similar(const gs_font_dir * pdir, const gs_font **ppfont,
                           int (*similar)(const gs_font *, const gs_font *));

/* gs_scalefont and gs_makefont return 0 if the scaled font */
/* was already in the cache, 1 if a new font was created. */
int gs_scalefont(gs_font_dir *, const gs_font *, double, gs_font **);
int gs_makefont(gs_font_dir *, const gs_font *, const gs_matrix *, gs_font **);
int gs_setfont(gs_gstate *, gs_font *);
gs_font *gs_currentfont(const gs_gstate *);
gs_font *gs_rootfont(const gs_gstate *);
void gs_set_currentfont(gs_gstate *, gs_font *);
int  gs_purge_font(gs_font *);
/* Locate a gs_font by gs_id. */
gs_font *gs_find_font_by_id(gs_font_dir *pdir, gs_id id, gs_matrix *FontMatrix);

/* Font cache parameter operations */
void gs_cachestatus(const gs_font_dir *, uint[7]);

#define gs_setcachelimit(pdir,limit) gs_setcacheupper(pdir,limit)
uint gs_currentcachesize(const gs_font_dir *);
int gs_setcachesize(gs_gstate * pgs, gs_font_dir *, uint);
uint gs_currentcachelower(const gs_font_dir *);
int gs_setcachelower(gs_font_dir *, uint);
uint gs_currentcacheupper(const gs_font_dir *);
int gs_setcacheupper(gs_font_dir *, uint);
uint gs_currentaligntopixels(const gs_font_dir *);
int gs_setaligntopixels(gs_font_dir *, uint);
uint gs_currentgridfittt(const gs_font_dir *);
int gs_setgridfittt(gs_font_dir *, uint);

#endif /* gsfont_INCLUDED */
