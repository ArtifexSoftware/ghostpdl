/* Copyright (C) 1989, 1993, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Generic font and font cache interface */
/* Requires gsmatrix.h */

#ifndef gsfont_INCLUDED
#  define gsfont_INCLUDED

/* A 'font directory' object (to avoid making fonts global). */
/* 'directory' is something of a misnomer: this structure */
/* just keeps track of the defined fonts, and the scaled font and */
/* rendered character caches. */
#ifndef gs_font_dir_DEFINED
#  define gs_font_dir_DEFINED
typedef struct gs_font_dir_s gs_font_dir;
#endif

/* Font objects */
#ifndef gs_font_DEFINED
#  define gs_font_DEFINED
typedef struct gs_font_s gs_font;
#endif

/* Initialization */
/* These procedures return 0 if they fail. */
gs_font_dir *gs_font_dir_alloc2(P2(gs_memory_t * struct_mem,
				   gs_memory_t * bits_mem));
gs_font_dir *gs_font_dir_alloc2_limits(P7(gs_memory_t * struct_mem,
					  gs_memory_t * bits_mem,
					  uint smax, uint bmax, uint mmax,
					  uint cmax, uint upper));

/* Backward compatibility */
#define gs_font_dir_alloc(mem) gs_font_dir_alloc2(mem, mem)
#define gs_font_dir_alloc_limits(mem, smax, bmax, mmax, cmax, upper)\
  gs_font_dir_alloc2_limits(mem, mem, smax, bmax, mmax, cmax, upper)

/* Font manipulations */
/* Use gs_definefont only with original (unscaled) fonts! */
int gs_definefont(P2(gs_font_dir *, gs_font *));

/* gs_scalefont and gs_makefont return 0 if the scaled font */
/* was already in the cache, 1 if a new font was created. */
int gs_scalefont(P4(gs_font_dir *, const gs_font *, floatp, gs_font **));
int gs_makefont(P4(gs_font_dir *, const gs_font *, const gs_matrix *, gs_font **));
int gs_setfont(P2(gs_state *, gs_font *));
gs_font *gs_currentfont(P1(const gs_state *));
gs_font *gs_rootfont(P1(const gs_state *));
void gs_set_currentfont(P2(gs_state *, gs_font *));
void gs_purge_font(P1(gs_font *));

/* Font cache parameter operations */
void gs_cachestatus(P2(const gs_font_dir *, uint[7]));

#define gs_setcachelimit(pdir,limit) gs_setcacheupper(pdir,limit)
uint gs_currentcachesize(P1(const gs_font_dir *));
int gs_setcachesize(P2(gs_font_dir *, uint));
uint gs_currentcachelower(P1(const gs_font_dir *));
int gs_setcachelower(P2(gs_font_dir *, uint));
uint gs_currentcacheupper(P1(const gs_font_dir *));
int gs_setcacheupper(P2(gs_font_dir *, uint));

#endif /* gsfont_INCLUDED */
