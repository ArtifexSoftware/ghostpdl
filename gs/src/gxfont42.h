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
/* Type 42 font data definition */

#ifndef gxfont42_INCLUDED
#  define gxfont42_INCLUDED

/* This is the type-specific information for a Type 42 (TrueType) font. */
typedef struct gs_type42_data_s gs_type42_data;
#ifndef gs_font_type42_DEFINED
#  define gs_font_type42_DEFINED
typedef struct gs_font_type42_s gs_font_type42;
#endif
typedef struct gs_type42_mtx_s {
    uint numMetrics;		/* num*Metrics from [hv]hea */
    ulong offset;		/* offset to [hv]mtx table */
    uint length;		/* length of [hv]mtx table */
} gs_type42_mtx_t;
struct gs_type42_data_s {
    /* The following are set by the client. */
    int (*string_proc) (P4(gs_font_type42 *, ulong, uint, const byte **));
    void *proc_data;		/* data for procedures */
    /*
     * The following are initialized by ...font_init, but may be reset by
     * the client.  get_outline returns 1 if the string is newly allocated
     * (using the font's allocator) and can be freed by the client.
     */
    int (*get_outline)(P3(gs_font_type42 *pfont, uint glyph_index,
			  gs_const_string *pgstr));
    int (*get_metrics)(P4(gs_font_type42 *pfont, uint glyph_index, int wmode,
			  float sbw[4]));
    /* The following are cached values. */
    ulong glyf;			/* offset to glyf table */
    uint unitsPerEm;		/* from head */
    uint indexToLocFormat;	/* from head */
    gs_type42_mtx_t metrics[2];	/* hhea/hmtx, vhea/vmtx (indexed by WMode) */
    ulong loca;			/* offset to loca table */
    uint numGlyphs;		/* from size of loca */
};
#define gs_font_type42_common\
    gs_font_base_common;\
    gs_type42_data data
struct gs_font_type42_s {
    gs_font_type42_common;
};

extern_st(st_gs_font_type42);
#define public_st_gs_font_type42()	/* in gstype42.c */\
  gs_public_st_suffix_add1_final(st_gs_font_type42, gs_font_type42,\
    "gs_font_type42", font_type42_enum_ptrs, font_type42_reloc_ptrs,\
    gs_font_finalize, st_gs_font_base, data.proc_data)

/*
 * Because a Type 42 font contains so many cached values,
 * we provide a procedure to initialize them from the font data.
 * Note that this initializes get_outline and the font procedures as well.
 */
int gs_type42_font_init(P1(gs_font_type42 *));

/* Append the outline of a TrueType character to a path. */
int gs_type42_append(P7(uint glyph_index, gs_imager_state * pis,
			gx_path * ppath, const gs_log2_scale_point * pscale,
			bool charpath_flag, int paint_type,
			gs_font_type42 * pfont));

/* Get the metrics of a TrueType character. */
int gs_type42_get_metrics(P3(gs_font_type42 * pfont, uint glyph_index,
			     float psbw[4]));
int gs_type42_wmode_metrics(P4(gs_font_type42 * pfont, uint glyph_index,
			       int wmode, float psbw[4]));

/* Get the metrics of a TrueType glyph. 
 * default for overrideable function pfont->data.get_metrics()
 */
int gs_type42_default_get_metrics(P4(gs_font_type42 * pfont, uint glyph_index,
				     int wmode, float sbw[4]));

/* Export the font procedures so they can be called from the interpreter. */
font_proc_enumerate_glyph(gs_type42_enumerate_glyph);
font_proc_glyph_info(gs_type42_glyph_info);
font_proc_glyph_outline(gs_type42_glyph_outline);

#endif /* gxfont42_INCLUDED */
