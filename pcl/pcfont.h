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
/*$Id$ */

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
void pcl_decache_font(pcl_state_t * pcs, int set);

/*
 * Recompute the font if necessary.
 */
int pcl_recompute_font(pcl_state_t * pcs, bool internal_only);

/*
 * Do any underlining just before a break in motion (vertical motion or
 * negative horizontal motion)...
 */
#define	pcl_break_underline(pcs)   \
    BEGIN                           \
    if (pcs->underline_enabled)    \
	pcl_do_underline(pcs);     \
    END

/* ...and then, after repositioning, restart underlining if necessary... */
#define	pcl_continue_underline(pcs)        \
    BEGIN                                   \
    if (pcs->underline_enabled)            \
        pcs->underline_start = pcs->cap;    \
    END

void pcl_do_underline(pcl_state_t *pcs);

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

/* set the default font environment based on setting from the pjl
   interpreter */
int pcl_set_current_font_environment(pcl_state_t *pcs);

/* debugging to to print font parameters */
#ifdef DEBUG
void dprint_font_params_t(const pl_font_params_t *pfp);
#else
#define dprint_font_params_t(p) DO_NOTHING
#endif /* DEBUG */
#endif		/* pcfont_INCLUDED */
