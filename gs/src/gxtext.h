/* Copyright (C) 1998, 1999 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */


/* Driver text interface implementation support */

#ifndef gxtext_INCLUDED
#  define gxtext_INCLUDED

#include "gstext.h"

/* Define the abstract type for the object procedures. */
typedef struct gs_text_enum_procs_s gs_text_enum_procs_t;

/*
 * Define values returned by text_process to the client.
 */
typedef struct gs_text_returned_s {
    gs_char current_char;	/* INTERVENE */
    gs_glyph current_glyph;	/* INTERVENE */
    gs_point current_width;	/* INTERVENE & RETURN_WIDTH */
    gs_point total_width;	/* RETURN_WIDTH */
} gs_text_returned_t;
/*
 * Define the common part of the structure that tracks the state of text
 * processing.  All implementations of text_begin must allocate one of these
 * using rc_alloc_struct_1; implementations may subclass and extend it.
 * Note that it includes a copy of the text parameters.
 */
#define gs_text_enum_common\
    /*\
     * The following copies of the arguments of text_begin are set at\
     * initialization, and const thereafter.\
     */\
    gs_text_params_t text;	/* must be first for subclassing */\
    gx_device *dev;\
    gs_imager_state *pis;\
    const gs_font *orig_font;\
    gx_path *path;			/* unless DO_NONE & !RETURN_WIDTH */\
    const gx_device_color *pdcolor;	/* if DO_DRAW */\
    const gx_clip_path *pcpath;		/* if DO_DRAW */\
    gs_memory_t *memory;\
    /* The following additional members are set at initialization. */\
    const gs_text_enum_procs_t *procs;\
    /* The following change dynamically. */\
    rc_header rc;\
    const gs_font *current_font; /* changes for composite fonts */\
    gs_log2_scale_point scale;	/* for oversampling */\
    uint index;		/* index within string */\
    /* The following are used to return information to the client. */\
    gs_text_returned_t returned
/* The typedef is in gstext.h. */
/*typedef*/ struct gs_text_enum_s {
    gs_text_enum_common;
} /*gs_text_enum_t*/;

#define st_gs_text_enum_max_ptrs st_gs_text_params_max_ptrs
/*extern_st(st_gs_text_enum); */
#define public_st_gs_text_enum()	/* in gstext.c */\
  gs_public_st_composite(st_gs_text_enum, gs_text_enum_t, "gs_text_enum_t",\
    text_enum_enum_ptrs, text_enum_reloc_ptrs)

/*
 * Define the control parameter for setting text metrics.
 */
typedef enum {
    TEXT_SET_CHAR_WIDTH,
    TEXT_SET_CACHE_DEVICE,
    TEXT_SET_CACHE_DEVICE2
} gs_text_cache_control_t;

/*
 * Define the procedures associated with text processing.
 */
struct gs_text_enum_procs_s {

    /*
     * Process the text.  Then client should call this repeatedly until
     * it returns <= 0.  (> 0 means the client must intervene.)
     */

#define text_enum_proc_process(proc)\
  int proc(P1(gs_text_enum_t *penum))

    text_enum_proc_process((*process));

    /*
     * Set the character width and optionally the bounding box,
     * and optionally enable caching.
     */

#define text_enum_proc_set_cache(proc)\
  int proc(P3(gs_text_enum_t *penum, const double *values,\
    gs_text_cache_control_t control))

    text_enum_proc_set_cache((*set_cache));

};

#endif /* gxtext_INCLUDED */
