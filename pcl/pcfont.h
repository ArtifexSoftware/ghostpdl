/*
 * Copyright (C) 1998 Aladdin Enterprises.
 * All rights reserved.
 *
 * This file is part of Aladdin Ghostscript.
 *
 * Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
 * or distributor accepts any responsibility for the consequences of using it,
 * or for whether it serves any particular purpose or works at all, unless he
 * or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
 * License (the "License") for full details.
 *
 * Every copy of Aladdin Ghostscript must include a copy of the License,
 * normally in a plain ASCII text file named PUBLIC.  The License grants you
 * the right to copy, modify and redistribute Aladdin Ghostscript, but only
 * under certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved on
 * all copies.
 */

/* pcfont.h - Definitions for PCL5 fonts */

#ifndef	pcfont_INCLUDED
#define	pcfont_INCLUDED

#include "pcstate.h"
#include "plfont.h"

/*
 * Clear the font pointer cache.  Some non-font operations--removing a
 * downloaded symbol set, or changing orientations--can cause this.
 * set == -1 means all.
 */
extern  void    pcl_decache_font( pcl_state_t * pcls, int set );

/*
 * Recompute the font if necessary. This is exported for resetting HMI.
 */
extern  int     pcl_recompute_font( pcl_state_t * pcls );

/*
 * Recompute the font if the glyph is not found at the time of rendering
 */
extern  int     pcl_recompute_substitute_font(
    pcl_state_t *   pcls,
    const uint      chr
);

/*
 * Do any underlining just before a break in motion (vertical motion or
 * negative horizontal motion)...
 */
#define	pcl_break_underline(pcls)   \
    BEGIN                           \
    if (pcls->underline_enabled)    \
	pcl_do_underline(pcls);     \
    END

/* ...and then, after repositioning, restart underlining if necessary... */
#define	pcl_continue_underline(pcls)        \
    BEGIN                                   \
    if (pcls->underline_enabled)            \
        pcls->underline_start = pcl_cap;    \
    END

extern  void    pcl_do_underline(P1(pcl_state_t *pcls));

/* Define the common structure of downloaded font headers. */
typedef struct pcl_font_header_s {
    byte    FontDescriptorSize[2];  /* must be >=64 */
    byte    HeaderFormat;
    byte    FontType;
    byte    StyleMSB;
    byte    Reserved;		    /* must be 0 */
    byte    BaselinePosition[2];
    byte    CellWidth[2];
    byte    CellHeight[2];
    byte    Orientation;
    byte    Spacing;
    byte    SymbolSet[2];
    byte    Pitch[2];
    byte    Height[2];
    byte    xHeight[2];
    byte    WidthType;
    byte    StyleLSB;
    byte    StrokeWeight;
    byte    TypefaceLSB;
    byte    TypefaceMSB;
    byte    SerifStyle;
    byte    Quality;
    byte    Placement;
    byte    UnderlinePosition;
    byte    UnderlineThickness;
    byte    TextHeight[2];
    byte    TextWidth[2];
    byte    FirstCode[2];
    byte    LastCode[2];
    byte    PitchExtended;
    byte    HeightExtended;
    byte    CapHeight[2];
    byte    FontNumber[4];
    char    FontName[16];
} pcl_font_header_t;

/* Define the downloaded font header formats. */
typedef enum {
    pcfh_bitmap = 0,
    pcfh_resolution_bitmap = 20,
    pcfh_intellifont_bound = 10,
    pcfh_intellifont_unbound = 11,
    pcfh_truetype = 15,
    pcfh_truetype_large = 16
} pcl_font_header_format_t;

/* Define the extended of resolution-specified bitmap font header (type 20). */
typedef struct pcl_resolution_bitmap_header_s {
    pcl_font_header_t   common;
    byte                XResolution[2];
    byte                YResolution[2];
    char                Copyright[1];
} pcl_resolution_bitmap_header_t;

#endif		/* pcfont_INCLUDED */
