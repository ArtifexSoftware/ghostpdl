/* Copyright (C) 1989, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* gzstate.h */
/* Private graphics state definition for Ghostscript library */
#include "gscpm.h"
#ifdef DPNEXT
#include "gsrefct.h"
#endif
#include "gxdcolor.h"
#include "gxistate.h"
#include "gsstate.h"
#include "gxstate.h"

/* Opaque types referenced by the graphics state. */
#ifndef gs_font_DEFINED
#  define gs_font_DEFINED
typedef struct gs_font_s gs_font;
#endif

/*
 * We allocate a subset of the graphics state as a single object.
 * Its type is opaque here, defined in gsstate.c.
 * The components marked with @ must be allocated in the contents.
 * (Consult gsstate.c for more details about gstate storage management.)
 */
typedef struct gs_state_contents_s gs_state_contents;

/* Graphics state structure. */

#ifdef DPNEXT
typedef struct gx_clip_state_s {
  rc_header rc;
  struct gx_clip_path_s *path;
} gx_clip_state;
#define private_st_gx_clip_state()	/* in gsstate.c */\
  gs_private_st_ptrs1(st_gx_clip_state, gx_clip_state, "gx_clip_state",\
    clip_state_enum_ptrs, clip_state_reloc_ptrs, path)
#endif

struct gs_state_s {
	gs_imager_state_common;		/* imager state, must be first */
	gs_state *saved;		/* previous state from gsave */
	gs_state_contents *contents;

		/* Transformation: */

	gs_matrix ctm_inverse;
	bool ctm_inverse_valid;		/* true if ctm_inverse = ctm^-1 */
	gs_matrix ctm_default;
	bool ctm_default_set;		/* if true, use ctm_default; */
					/* if false, ask device */

		/* Paths: */

	struct gx_path_s *path;
	struct gx_clip_path_s *clip_path;	/* @ */
#ifdef DPNEXT
	gx_clip_state *view_clip;
	struct gx_clip_path_s *saved_view_clip;	/* for save/restore only */
#endif
	bool clamp_coordinates;		/* if true, clamp out-of-range */
					/* coordinates; if false, */
					/* report a limitcheck */

		/* Color (device-independent): */

	struct gs_color_space_s *color_space;
	struct gs_client_color_s *ccolor;

		/* Color caches: */

	gx_device_color *dev_color;

		/* Font: */

	gs_font *font;
	gs_font *root_font;
	gs_matrix_fixed char_tm;	/* font matrix * ctm */
#define char_tm_only(pgs) *(gs_matrix *)&(pgs)->char_tm
	bool char_tm_valid;		/* true if char_tm is valid */
	byte in_cachedevice;		/* 0 if not in setcachedevice, */
					/* 1 if in setcachedevice but not */
					/* actually caching, */
					/* 2 if in setcachedevice and */
					/* actually caching */
	gs_char_path_mode in_charpath;	/* (see gscpm.h) */
	gs_state *show_gstate;		/* gstate when show was invoked */
					/* (so charpath can append to path) */

		/* Other stuff: */

	int level;			/* incremented by 1 per gsave */
	gx_device *device;
#undef gs_currentdevice_inline
#define gs_currentdevice_inline(pgs) ((pgs)->device)

		/* Client data: */

	void *client_data;
#define gs_state_client_data(pgs) ((pgs)->client_data)
	gs_state_client_procs client_procs;
};
#define private_st_gs_state()	/* in gsstate.c */\
  gs_private_st_composite(st_gs_state, gs_state, "gs_state",\
    gs_state_enum_ptrs, gs_state_reloc_ptrs)
