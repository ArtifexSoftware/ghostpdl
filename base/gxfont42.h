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


/* Type 42 font data definition */

#ifndef gxfont42_INCLUDED
#  define gxfont42_INCLUDED

#include "gxfont.h"
#include "gsgcache.h"
#include "gxmatrix.h"
#include "gxfcache.h"

typedef struct
{
  const int ccode;
  const char * const name;
} mac_glyph_ordering_t;

/* This is the type-specific information for a Type 42 (TrueType) font. */
typedef struct gs_type42_data_s gs_type42_data;

typedef enum gs_type42_metrics_options_s {
    gs_type42_metrics_options_WMODE0 = 0,
    gs_type42_metrics_options_WMODE1 = 1,
    gs_type42_metrics_options_BBOX = 2,
    gs_type42_metrics_options_WMODE0_AND_BBOX = 4,
    gs_type42_metrics_options_WMODE1_AND_BBOX = 5,
} gs_type42_metrics_options_t;
#define gs_type42_metrics_options_wmode(a)         ((a)&gs_type42_metrics_options_WMODE1)
#define gs_type42_metrics_options_sbw_requested(a) (~(a)&gs_type42_metrics_options_BBOX)
#define gs_type42_metrics_options_bbox_requested(a)((a)&6)

/* Export the default get_metrics procedure.
   The length of sbw is >=4 when bbox in not requested,
   and 8 otherwise.
 */
int gs_type42_default_get_metrics(gs_font_type42 *pfont, uint glyph_index,
                                  gs_type42_metrics_options_t options, float *sbw);

typedef struct gs_type42_mtx_s {
    uint numMetrics;		/* num*Metrics from [hv]hea */
    ulong offset;		/* offset to [hv]mtx table */
    uint length;		/* length of [hv]mtx table */
} gs_type42_mtx_t;
struct gs_type42_data_s {
    /* The following are set by the client. */
    int (*string_proc) (gs_font_type42 *, ulong, uint, const byte **);
    void *proc_data;		/* data for procedures */
    /*
     * The following are initialized by ...font_init, but may be reset by
     * the client.
     */
    uint (*get_glyph_index)(gs_font_type42 *pfont, gs_glyph glyph);
    int (*get_outline)(gs_font_type42 *pfont, uint glyph_index,
                       gs_glyph_data_t *pgd);
    int (*get_metrics)(gs_font_type42 *pfont, uint glyph_index,
                        gs_type42_metrics_options_t options,
                        float *sbw_bbox/* See comment for gs_type42_default_get_metrics */);
     uint (*substitute_glyph_index_vertical)(gs_font_type42 *pfont, uint glyph_index,
                                          int WMode, gs_glyph glyph);

    /* The following are cached values. */
    ulong cmap;			/* offset to cmap table (not used by */
                                /* renderer, only here for clients) */
    ulong glyf;			/* offset to glyf table */
    uint unitsPerEm;		/* from head */
    uint indexToLocFormat;	/* from head */
    gs_type42_mtx_t metrics[2];	/* hhea/hmtx, vhea/vmtx (indexed by WMode) */
    ulong loca;			/* offset to loca table */
    ulong name_offset;		/* offset to name table */
    ulong os2_offset;		/* offset to OS/2 table */
    ulong post_offset;          /* Offset to the post table */
    /*
     * TrueType fonts specify the number of glyphs in two different ways:
     * the size of the loca table, and an explicit value in maxp.  Currently
     * the value of numGlyphs in this structure is computed from the size of
     * loca.  This is wrong: incrementally downloaded TrueType (or
     * CIDFontType 2) fonts will have no loca table, but will have a
     * reasonable glyph count in maxp.  Unfortunately, a significant amount
     * of code now depends on the incorrect definition of numGlyphs.
     * Therefore, rather than run the risk of introducing bugs by changing
     * the definition and/or by changing the name of the data member, we add
     * another member trueNumGlyphs to hold the value from maxp.
     */
    uint numGlyphs;		/* from size of loca */
    uint trueNumGlyphs;		/* from maxp */
    uint maxPoints;         /* from maxp (used by pdfwrite) */
    uint maxContours;       /* from maxp (used by pdfwrite) */
    uint maxCPoints;        /* from maxp (used by pdfwrite) */
    uint maxCContours;      /* from maxp (used by pdfwrite) */
    uint *len_glyphs;		/* built from the loca table */
    gs_glyph_cache *gdcache;
    bool warning_patented;
    bool warning_bad_instruction;
    byte *gsub;			/* GSUB */
    ulong gsub_size;
};
#define gs_font_type42_common\
    gs_font_base_common;\
    gs_type42_data data
struct gs_font_type42_s {
    gs_font_type42_common;
};

extern_st(st_gs_font_type42);
#define public_st_gs_font_type42()	/* in gstype42.c */\
  gs_public_st_suffix_add4_final(st_gs_font_type42, gs_font_type42,\
    "gs_font_type42", font_type42_enum_ptrs, font_type42_reloc_ptrs,\
    gs_font_finalize, st_gs_font_base, data.proc_data, data.len_glyphs, \
    data.gdcache, data.gsub)

/*
 * Because a Type 42 font contains so many cached values,
 * we provide a procedure to initialize them from the font data.
 * Note that this initializes the type42_data procedures other than
 * string_proc, and the font procedures as well.
 * USE_ttfReader subclasses gs_font_type42 with ttfReader or without.
 * FAPI will disable ttfReader as well.
 */
int gs_type42_font_init(gs_font_type42 *pfont, int subfontid);

/* Read data from sfnts. */
int gs_type42_read_data(gs_font_type42 * pfont, ulong pos, uint length, byte *buf);

/* Use the name table to find a glyph name */
int gs_type42_find_post_name(gs_font_type42 * pfont, gs_glyph glyph, gs_string *gname);

/* Read data from sfnts. */
/* A temporary macro for simplifying the old code change. */
#define READ_SFNTS(pfont, pos, length, buf)\
  BEGIN\
    if (length > sizeof(buf))\
        return_error(gs_error_unregistered);/* Must not happen. */\
    code = gs_type42_read_data(pfont, (ulong)(pos), length, buf);\
    if ( code < 0 ) return code;\
  END

#define MAX_NUM_TT_TABLES 40

/* Append the outline of a TrueType character to a path. */
int gs_type42_append(uint glyph_index, gs_gstate * pgs,
                 gx_path * ppath, gs_text_enum_t *penum, gs_font *pfont,
                 bool charpath_flag);

/* Get the metrics of a TrueType character. */
int gs_type42_get_metrics(gs_font_type42 * pfont, uint glyph_index,
                          float psbw[4]);

int gs_type42_get_outline_from_TT_file(gs_font_type42 * pfont, stream *s, uint glyph_index,
                gs_glyph_data_t *pgd);

/* Export the font procedures so they can be called from the interpreter. */
font_proc_enumerate_glyph(gs_type42_enumerate_glyph);
font_proc_glyph_info(gs_type42_glyph_info);
font_proc_glyph_outline(gs_type42_glyph_outline);

/* Get glyph info by glyph index. */
int gs_type42_glyph_info_by_gid(gs_font *font, gs_glyph glyph, const gs_matrix *pmat,
                     int members, gs_glyph_info_t *info, uint glyph_index);

int gs_type42_font_info(gs_font *font, const gs_point *pscale, int members,
           gs_font_info_t *info);
int gs_truetype_font_info(gs_font *font, const gs_point *pscale, int members,
           gs_font_info_t *info);

uint gs_type42_substitute_glyph_index_vertical(gs_font_type42 *pfont, uint glyph_index,
                                          int WMode, gs_glyph glyph);

/* Get next component in a composite TrueType glyph. */
void gs_type42_parse_component(const byte **pdata, uint *pflags, gs_matrix_fixed *psmat,
                               int *pmp /*[2], may be null*/, const gs_font_type42 *pfont,
                               const gs_matrix_fixed *pmat);

int
gs_woff2sfnt_stream(gs_memory_t *mem, stream *s, byte *outbuf, int *outbuflen);

int
gs_woff2sfnt_buffer(gs_memory_t *mem, byte *inbuf, int inbuflen, byte *outbuf, int *outbuflen);

#endif /* gxfont42_INCLUDED */
