/* Copyright (C) 2003 Aladdin Enterprises.  All rights reserved.
  
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
/* State and interface definitions for a spot analyzer device. */

/*
 * A spot analyzer device performs an analyzis while handling an output 
 * of the trapezoid fill algorithm for 2 purposes :
 * a glyph grid fitting, and 
 * a glyph antialiased rendering any number of alpha bits.
 * Currently we only implement a vertical stem recognition for the grid fitting.
 */

#ifndef gzspotan_INCLUDED
#  define gzspotan_INCLUDED

#include "gxdevcli.h"

#ifndef segment_DEFINED
#  define segment_DEFINED
typedef struct segment_s segment; /* gzpath.h */
#endif

#ifndef gx_device_spot_analyzer_DEFINED
#   define gx_device_spot_analyzer_DEFINED
typedef struct gx_device_spot_analyzer_s gx_device_spot_analyzer;
#endif

/* -------------- Structures ----------------------------- */

typedef struct gx_san_trap_s gx_san_trap;
typedef struct gx_san_trap_contact_s gx_san_trap_contact;

/* A trapezoid. */
struct gx_san_trap_s {
    /* The geometry : */
    fixed ybot, ytop;
    fixed xlbot, xrbot, xltop, xrtop;
    /* The spot topology representation : */
    gx_san_trap_contact *lower; /* Neighbours of the upper band. */
    const segment *l; /* Outline pointer : left boundary. */
    const segment *r; /* Outline pointer : right boundary. */
    /* The topology reconstrustor work data : */
    gx_san_trap *next; /* Next with same ytop. */
    gx_san_trap *prev; /* Prev with same ytop. */
    /* The stem recognizer work data : */
};
#define private_st_san_trap()	/* No GC invocations */\
  gs_private_st_simple(st_san_trap, gx_san_trap, "gx_san_trap")

/* A contact of 2 trapezoids. */
/* Represents a neighbourship through a band boundary. */
struct gx_san_trap_contact_s {
    gx_san_trap_contact *next; /* Next element of the same relation, a cyclic list. */
    gx_san_trap_contact *prev; /* Prev element of the same relation, a cyclic list. */
    gx_san_trap *upper, *lower; /* A contacting pair. */
};
#define private_st_san_trap_contact()	/* No GC invocations */\
  gs_private_st_simple(st_san_trap_contact, gx_san_trap_contact, "gx_san_trap")

/* A stem section. */
typedef struct gx_san_sect_s gx_san_sect;
struct gx_san_sect_s {
    fixed y, xl, xr;
    const segment *l, *r;
};

/* A spot analyzer device. */
struct gx_device_spot_analyzer_s {
    gx_device_common;
    int lock;
    gx_san_trap *trap_buffer;
    gx_san_trap_contact *cont_buffer;
    int trap_buffer_count, trap_buffer_max;
    int cont_buffer_count, cont_buffer_max;
    /* The topology reconstrustor work data (no GC invocations) : */
    gx_san_trap *bot_band;
    gx_san_trap *top_band;
    gx_san_trap *bot_current;
    /* The stem recognizer work data (no GC invocations) : */
};

extern_st(st_device_spot_analyzer);
#define public_st_device_spot_analyzer()\
    gs_public_st_suffix_add2_final(st_device_spot_analyzer, gx_device_spot_analyzer,\
	    "gx_device_spot_analyzer", device_spot_analyzer_enum_ptrs,\
	    device_spot_analyzer_reloc_ptrs, gx_device_finalize, st_device,\
	    trap_buffer, cont_buffer)

/* -------------- Interface methods ----------------------------- */

/* Obtain a spot analyzer device. */
int gx_san__obtain(gs_memory_t *mem, gx_device_spot_analyzer **ppadev);

/* Obtain a spot analyzer device. */
void gx_san__release(gx_device_spot_analyzer **ppadev);

/* Start accumulating a path. */
void gx_san_begin(gx_device_spot_analyzer *padev);

/* Store a tarpezoid. */
/* Assumes an Y-band scanning order with increasing X inside a band. */
int gx_san_trap_store(gx_device_spot_analyzer *padev, 
    fixed ybot, fixed ytop, fixed xlbot, fixed xrbot, fixed xltop, fixed xrtop,
    const segment *r, const segment *l);

/* Finish accumulating a path. */
void gx_san_end(const gx_device_spot_analyzer *padev);

/* Generate stems. */
int gx_san_generate_stems(gx_device_spot_analyzer *padev, 
		void *client_data,
		int (*handler)(void *client_data, gx_san_sect *ss));

#endif /* gzspotan_INCLUDED */
