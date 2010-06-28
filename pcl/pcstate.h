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

/* pcstate.h - Definition of PCL5 state */

#ifndef pcstate_INCLUDED
#  define pcstate_INCLUDED

#include "gx.h"
#include "gxdevice.h"
#include "scommon.h"
#include "gscspace.h"
#include "gscolor2.h"
#include "gscrd.h"
#include "gsdcolor.h"		/* for gx_ht_tile */
#include "gschar.h"
#include "pldict.h"
#include "plfont.h"

/* various components of the state structure */
#include "pccoord.h"            /* centi-point coordinate system */
#include "pcxfmst.h"            /* geometric transformation information */
#include "pcfontst.h"           /* font selection information */
#include "pctpm.h"              /* text parsing method information */
#include "pcpattyp.h"           /* pattern related structures */
#include "pcdict.h"             /* PL dictionary key structure */
#include "rtrstst.h"            /* raster state information */
#include "pcht.h"
#include "pcident.h"
#include "pccsbase.h"           

/*#include "pgstate.h"*/	/* HP-GL/2 state, included below */
#include "pjtop.h"

/* type for string id's */
typedef struct pcl_string_id_s {
    byte *  id;
    int     size;
} alphanumeric_string_id_t;

/* type for current state of id's, string or regular id's macros */
typedef enum id_type_enum {
    string_id,
    numeric_id
} id_type_t;

/* define different language personalities for dynamic configuration */
typedef enum personality_enum {   /* NB  document */
    pcl5c,
    pcl5e,
    rtl,
    hpgl
} pcl_personality_t;

/*
 * Palette stack. This is implemented as a simple linked list.  NB
 * needs to be moved.  
 */
typedef struct pstack_entry_s {
    struct pstack_entry_s * pnext;
    pcl_palette_t *         ppalet;
} pstack_entry_t;

#ifndef pcl_state_DEFINED
#  define pcl_state_DEFINED
typedef struct pcl_state_s  pcl_state_t;
#endif
#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s   gs_state;
#endif

/*
 * The routines pcl_gsave and pcl_grestore should be used instead of
 * gs_gsave and gs_grestore. See the comment in pcdraw.c for details.
 *
 * The routine pcl_init_gstate_stk must be called once a boot time to
 * intialize PCL graphics state stack tracking mechanism.
 */
int pcl_gsave(pcl_state_t * pcs);
int pcl_grestore(pcl_state_t * pcs);
void pcl_init_gstate_stk(pcl_state_t * pcs);
void pcl_free_gstate_stk(pcl_state_t * pcs);
/*
 * "Cold start" initialization for the graphic state.
 */
void pcl_init_state(pcl_state_t * pcs, gs_memory_t * pmem);


#include "pgstate.h"	    /* HP-GL/2 state */

#ifndef pcl_pattern_data_DEFINED
#define pcl_ht_builtin_dither_DEFINED
typedef struct pcl_pattern_t pcl_pattern;
#endif

#ifndef pcl_cs_indexed_DEFINED
#define pcl_cs_indexed_DEFINED
typedef struct pcl_cs_indexed_s pcl_cs_indexed_t;
#endif

/*
 * Define the entire PCL/HPGL state.  The documentation for this is spread
 * out over the PCL reference manuals, and is incomplete, inconsistent, and
 * poorly organized.  In order to have a ghost (:-)) of a chance of being
 * able to think about it as a whole, we've organized it here by
 * documentation chapter, just as we did the .c files.
 *
 * NB: If you modify this structure, be sure to also modify the routine
 *     pcl_init_state in pcommand.c.
 */
struct pcl_state_s {

    gs_memory_t *   memory;

    /* hook back to client data, for callback procedures */
    void *          client_data;

    /* graphics state */
    gs_state *      pgs;

    /* Define an optional procedure for parsing non-ESC data. */
    int             (*parse_other)( void *                  parse_data,
                                       pcl_state_t *           pcs,
			               stream_cursor_read *    pr
                                       );
    void *          parse_data;	        /* closure data for parse_other */

    /* Chapter 4 (pcjob.c) */
    int             num_copies;		/* (also a device parameter) */
    bool            duplex;		/* (also a device parameter) */
    bool            bind_short_edge;	/* (also a device parameter) */
    bool            back_side;		/* (also a device parameter) */
    int             output_bin;		/* (also a device parameter) */
    coord           uom_cp;	        /* centipoints per PCL unit */

    /* Chapter 5 (pcpage.c) */
    int             paper_source;
    int             perforation_skip;
    pcl_margins_t   margins;            /* relative to print_direction */
    pcl_xfm_state_t xfm_state;
    bool            orientation_set;    /* the orientation command can
                                           only be set once per page */
    /* Chapter 6 (pcursor.c) */
    coord           hmi_cp;
    coord           vmi_cp;
    int             line_termination;
    coord_point_t   cap;
    gs_point        cursor_stk[20];
    int             cursor_stk_size;

    /* Chapter 8 (pcfont.c) */
    pcl_font_selection_t    font_selection[2];
    enum {
        primary = 0,
	secondary = 1
    }               font_selected;
    pl_font_t *     font;		/* 0 means recompute from params */
    pl_dict_t       built_in_fonts;	/* "built-in", known at start-up */

    /* Internal variables */
    gs_font_dir *       font_dir;	/* gs-level dictionary of fonts */
    pl_symbol_map_t *   map;		/* map for current font (above) */

    /* more Chapter 8 (pctext.c) */
    bool            underline_enabled;
    bool            underline_floating;
    float           underline_position;	/* distance from baseline */

    const pcl_text_parsing_method_t *   text_parsing_method;

    int             text_path;		/* 0, 1, -1 */

    /* Internal variables */
    float           last_width;		/* escapement of last char (for BS) */
    coord_point_t   underline_start;	/* start point of underline */
    bool            last_was_BS;	/* no printable chars since last BS */

    /* Chapter 10 (pcsymbol.c) */
    pcl_id_t        symbol_set_id;
    pl_dict_t       soft_symbol_sets;
    pl_dict_t       built_in_symbol_sets;
    pl_dict_t       simm_fonts;
    pl_dict_t       cartridge_fonts;
    int             default_symbol_set_value;

    /* Chapter 9 & 11 (pcsfont.c) */
    pcl_id_t        font_id;
    uint            character_code;
    pl_dict_t       soft_fonts;
    uint            soft_font_count;
    byte *          soft_font_char_data;
    /* PCL comparison guide - alphanumeric string id */
    alphanumeric_string_id_t    alpha_font_id;
    id_type_t                   font_id_type;

#define current_font_id                                                 \
    ( ((pcs->font_id_type == string_id) ? (pcs->alpha_font_id.id)     \
                                         : (id_key(pcs->font_id))) )

#define current_font_id_size                                                  \
    ( ((pcs->font_id_type == string_id) ? (pcs->alpha_font_id.size) : (2)) )

    /* Chapter 12 (pcmacros.c) */
    pcl_id_t        macro_id;
    pcl_id_t        overlay_macro_id;
    bool            overlay_enabled;
    int             macro_level;
    pl_dict_t       macros;

    bool            defining_macro;
    pcl_state_t *   saved;	/* saved state during execute/call/overlay */

    /* Internal variables */
    byte *          macro_definition;   /* for macro being defined, if any */
    alphanumeric_string_id_t    alpha_macro_id;
    id_type_t       macro_id_type;

#define current_macro_id                                                \
    ( ((pcs->macro_id_type == string_id) ? (pcs->alpha_macro_id.id)   \
                                          : (id_key(pcs->macro_id))) )

#define current_macro_id_size                                                   \
    ( ((pcs->macro_id_type == string_id) ? (pcs->alpha_macro_id.size) : (2)) )

    /* Chapter 13 (pcprint.c) */
    gs_point            pat_ref_pt;     /* active pattern reference point,
                                         * in device space */

    int                 pat_orient;     /* current pattern orientation */
    int                 pattern_id;
    int                 current_pattern_id;
    pcl_pattern_source_t pattern_type;  /* current source for PCL patterns */
    gs_point            pcl_pat_ref_pt; /* PCL's pattern reference point */
    pl_dict_t           pcl_patterns;   /* dictionaries to hold pcl and gl/2 patterns */
    pl_dict_t           gl_patterns;
#define PCL_NUM_SHADE_PATTERNS (7)         /* pcl support 7 shades of gray */
#define PCL_NUM_CROSSHATCH_PATTERNS (6)    /* and 6 cross hatch patterns */
    pcl_pattern *     bi_pattern_array[PCL_NUM_SHADE_PATTERNS + PCL_NUM_CROSSHATCH_PATTERNS];
    int                 last_pcl_uptrn_id; /* optimizations for recording last patter */
    pcl_pattern *     plast_pcl_uptrn;   /* and pattern id */
    int                 last_gl2_RF_indx;
    pcl_pattern *     plast_gl2_uptrn;
    pcl_pattern *     psolid_pattern;    /* see documentation in pcbiptrn.c for these two */
    pcl_pattern *	punsolid_pattern;
    bool                rotate_patterns;    /* rotate patterns with print direction in PCL */
    bool                source_transparent; /* (also in graphics state) */
    bool                pattern_transparent;/* (also in graphics state);
                                             * PCL and GL/2 set this
                                             * independenty; for GL/2 it is
                                             * known as source transparent */
    bool                pcl_pattern_transparent;


    /* Chapter 14 (pcrect.c) */
    coord_point_t     rectangle;

    /* Chapter 15 &  Chapter C6 (pcgmode.c) */
    pcl_raster_state_t  raster_state;

    /* Chapter 16 (pcstatus.c) */
    int             location_type;
    int             location_unit;
    struct _sb {
	byte            internal_buffer[80];    /* enough for an error message */
	byte *          buffer;
	uint            write_pos;
	uint            read_pos;
    }               status;

    /* Chapter 24 (pcmisc.c) */
    bool            end_of_line_wrap;
    bool            display_functions;
    int             (*configure_appletalk)( const byte * key,
                                               uint         key_length,
				               const byte * value,
                                               uint         value_length
                                               );

    /* Chapter C2/C3/C4 (pcpalet.c/pcindexed.c/pccsbase.c/etc.) */
    int                 sel_palette_id;
    int                 ctrl_palette_id;
    int                 monochrome_mode;/* 0=off */
    int                 render_mode;    /* raw (unmapped) render algorithm */
    pcl_palette_t *     ppalet;
    pcl_gsid_t          next_id; /* id's for palette's and foreground
                                    colors see pcident.h */
    pcl_frgrnd_t *      pfrgrnd;
    pcl_gsid_t          frgnd_cache_id;
    
    pcl_gstate_ids_t *  pids;
    /*
     * Unlike other elements of the PCL "palette", color rendering dictionaries
     * are for the most part not a feature that can be controlled from the language.
     * Except for the white point, the parameters of a color rendering dictionary
     * are determined by the output device rather than the language.
     */
    pcl_crd_t *         pcl_default_crd;
    
    /* internal dithers */
    pcl_ht_builtin_dither_t  ordered_dither;
    pcl_ht_builtin_dither_t  clustered_dither;
    pcl_ht_builtin_dither_t  noise_dither;
    /*
     * The forwarding devices to preform any necessary color mapping. There are
     * four of these: identity mapping, snap to primaries, map black to white and
     * all other colors to black, and monochrome mapping. The devices are all
     * identical except for the mapping method used.
     *
     * Several devices are required because the rendering method used by the
     * foreground may not be the same as that used by the current palette.
     */
    gs_cie_transform_proc3 dflt_TransformPQR;  /* default transform PQR */
    pcl_rend_info_t rendering_info[20];       /* rendering methods */
    byte            dflt_rendering_remap[20]; /* NB not convinced this is necessary (review) */
    byte            rendering_remap[20];      /* remap the table of rendering methods */
    pcl_ht_t *      pdflt_ht;                 /* default halftone */
    pcl_cs_indexed_t *pdflt_cs_indexed;
    pcl_cid_data_t  dflt_cid_data;
    pcl_cs_base_t * pwhite_cs;
    pcl_frgrnd_t *  pdflt_frgrnd;
    pstack_entry_t *palette_stack;
    pcl_palette_t  *pdflt_palette;           /* default palette */
    pl_dict_t       palette_store;           /* dictionary to hold the palette store */
    float           color_comps[3];
    /* Chapter C5 (pccprint.c) */
    byte            logical_op;	    /* (also in graphics state) */
    byte            pp_mode;        /* pixel placement mode */
    /* ---------------- HP-GL/2 state ---------------- */
    pcl_hpgl_state_t    g;	/* see pgstate.h */
    /* ---------------- PJL state -------------------- */
    pl_interp_instance_t *pjls;
    /* yet another poorly documented pjl variable - this should widen
       the margins on A4 paper to support 80 10 pitch characters but
       it appears to affect letter paper as well */
    bool              wide_a4;

    /* the current language personality */
    pcl_personality_t personality;
    
    /* enable image interpolation */
    bool interpolate;

    /* store a pointer to the command definitions for use by macros */
    void *pcl_commands;
    
    /* indicates page has been written to with a drawing command */
    bool page_marked;

    /* end page procedure to use */
    int (*end_page)( pcl_state_t * pcs, int num_copies, int flush );
    /* map device color spaces to srgb */
    bool useciecolor;
    bool halftone_set;
};

/* accessor functions for the pcl target device.  These live in
   pctop.c for now */
int pcl_load_cartridge_fonts(pcl_state_t *pcs, const char *pathname);
int pcl_load_simm_fonts(pcl_state_t *pcs, const char *pathname);
int pcl_load_built_in_fonts(pcl_state_t *pcs, const char *pathname);
/* implicitly exit gl/2 whenever ESC E is found */
int pcl_implicit_gl2_finish(pcl_state_t *pcs);
int pcl_do_printer_reset(pcl_state_t *pcs);
int pcl_end_page_top(pcl_state_t *pcs, int num_copies, int flush);

/* exported from pcl to support PCL XL pass through mode */
bool pcl_downloaded_and_bound(pl_font_t *plfont);
void pcl_font_scale(pcl_state_t *, gs_point *psz);

#endif 						/* pcstate_INCLUDED */
