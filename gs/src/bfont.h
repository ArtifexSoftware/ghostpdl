/* Copyright (C) 1992, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* bfont.h */
/* Interpreter internal routines and data needed for building fonts */
/* Requires gxfont.h */
#include "ifont.h"

/* In zfont.c */
int add_FID(P2(ref *pfdict, gs_font *pfont));
font_proc_make_font(zdefault_make_font);
font_proc_make_font(zbase_make_font);
/* The global font directory */
extern gs_font_dir *ifont_dir;

/* Structure for passing BuildChar and BuildGlyph procedures. */
typedef struct build_proc_refs_s {
	ref BuildChar;
	ref BuildGlyph;
} build_proc_refs;

/* Options for collecting parameters from a font dictionary. */
/* The comment indicates where the option is tested. */
typedef enum {
  bf_options_none = 0,
  bf_Encoding_optional = 1,	/* build_gs_font */
  bf_FontBBox_required = 2,	/* build_gs_simple_font */
  bf_UniqueID_ignored = 4,	/* build_gs_simple_font */
  bf_CharStrings_optional = 8,	/* build_gs_primitive_font */
  bf_notdef_required = 16	/* build_gs_primitive_font */
} build_font_options_t;

/* In zfont2.c */
int build_proc_name_refs(P3(build_proc_refs *pbuild,
			    const char _ds *bcstr,
			    const char _ds *bgstr));
int build_gs_font_procs(P2(os_ptr, build_proc_refs *));
int build_gs_primitive_font(P6(os_ptr, gs_font_base **, font_type,
			       gs_memory_type_ptr_t, const build_proc_refs *,
			       build_font_options_t));
int build_gs_simple_font(P6(os_ptr, gs_font_base **, font_type,
			    gs_memory_type_ptr_t, const build_proc_refs *,
			    build_font_options_t));
void lookup_gs_simple_font_encoding(P1(gs_font_base *));
int build_gs_font(P6(os_ptr, gs_font **, font_type,
		     gs_memory_type_ptr_t, const build_proc_refs *,
		     build_font_options_t));
int define_gs_font(P1(gs_font *));
