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

/* pcstate.h - Definition of PCL5 state */

#ifndef pcstate_INCLUDED
#  define pcstate_INCLUDED

#include "gx.h"
#include "scommon.h"
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
/*#include "pgstate.h"*/	/* HP-GL/2 state, included below */


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
extern  int     pcl_gsave( pcl_state_t * pcs );
extern  int     pcl_grestore( pcl_state_t * pcs );
extern  void    pcl_init_gstate_stk( pcl_state_t * pcs );

/*
 * "Cold start" initialization for the graphic state.
 */
extern  void    pcl_init_state( pcl_state_t * pcs, gs_memory_t * pmem );


#include "pgstate.h"	    /* HP-GL/2 state */

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
    int             (*parse_other)(P3( void *                  parse_data,
                                       pcl_state_t *           pcls,
			               stream_cursor_read *    pr
                                       ));
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

    /* Internal variables */
    bool            have_page;	        /* true ==> have written on page */

    /* Chapter 6 (pcursor.c) */
    coord           hmi_cp;
    coord           vmi_cp;
    int             line_termination;

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

    int             text_path;		/* 0 or -1 */

    /* Internal variables */
    float           last_width;		/* escapement of last char (for BS) */
    coord_point     underline_start;	/* start point of underline */
    bool            last_was_BS;	/* no printable chars since last BS */
    gs_show_enum *  penum;              /* enumeration structure for
                                         * printing text */

    /* Chapter 10 (pcsymbol.c) */
    pcl_id_t        symbol_set_id;
    pl_dict_t       soft_symbol_sets;
    pl_dict_t       built_in_symbol_sets;

    /* Chapter 9 & 11 (pcsfont.c) */
    pcl_id_t        font_id;
    uint            character_code;
    pl_dict_t       soft_fonts;

    /* PCL comparison guide - alphanumeric string id */
    alphanumeric_string_id_t    alpha_font_id;
    id_type_t                   font_id_type;

#define current_font_id                                                 \
    ( ((pcls->font_id_type == string_id) ? (pcls->alpha_font_id.id)     \
                                         : (id_key(pcls->font_id))) )

#define current_font_id_size                                                  \
    ( ((pcls->font_id_type == string_id) ? (pcls->alpha_font_id.size) : (2)) )

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
    ( ((pcls->macro_id_type == string_id) ? (pcls->alpha_macro_id.id)   \
                                          : (id_key(pcls->macro_id))) )

#define current_macro_id_size                                                   \
    ( ((pcls->macro_id_type == string_id) ? (pcls->alpha_macro_id.size) : (2)) )

    /* Chapter 13 (pcprint.c) */
    gs_point            pat_ref_pt;     /* active pattern reference point,
                                         * in device space */

    int                 pat_orient;     /* current pattern orientation */
    int                 pattern_id;
    int                 current_pattern_id;
    pcl_pattern_source_t pattern_type;  /* current source for PCL patterns */
    gs_point            pcl_pat_ref_pt; /* PCL's pattern reference point */
    bool                rotate_patterns;/* rotate patterns with print
                                         * direction in PCL */

    bool                source_transparent; /* (also in graphics state) */
    bool                pattern_transparent;/* (also in graphics state);
                                             * PCL and GL/2 set this
                                             * independenty; for GL/2 it is
                                             * known as source transparent */
    bool                pcl_pattern_transparent;


    /* Chapter 14 (pcrect.c) */
    coord_point     rectangle;

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
    int             (*configure_appletalk)(P4( const byte * key,
                                               uint         key_length,
				               const byte * value,
                                               uint         value_length
                                               ));

    /* Chapter C2/C3/C4 (pcpalet.c/pcindexed.c/pccsbase.c/etc.) */
    int                 sel_palette_id;
    int                 ctrl_palette_id;
    bool                monochrome_mode;/* true ==> monochrome print mode */
    int                 render_mode;    /* raw (unmapped) render algorithm */
    pcl_palette_t *     ppalet;
    pcl_frgrnd_t *      pfrgrnd;
    pcl_gstate_ids_t *  pids;

    /* Chapter C5 (pccprint.c) */
    byte            logical_op;	    /* (also in graphics state) */
    byte            pp_mode;        /* pixel placement mode */

    /* ---------------- HP-GL/2 state ---------------- */
    pcl_hpgl_state_t    g;	/* see pgstate.h */
};

#endif 						/* pcstate_INCLUDED */
