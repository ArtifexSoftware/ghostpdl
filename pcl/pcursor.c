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

/* pcursor.c - PCL5 cursor positioning commands */

#include "std.h"
#include "math_.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcdraw.h"
#include "pcpatxfm.h"
#include "pcfont.h"
#include "pcursor.h"
#include "pcpage.h"
#include "gscoord.h"
#include "pjtop.h"

/*
 * Hoizontal and vertical movement.
 *
 * This is one of the most confusing areas of PCL because the individual
 * movement commands and margins evolved at different times in the history
 * of PCL, and thus have different behavior. In the dicussion below, we
 * divide the various horizontal and vertical motion commands into groups,
 * and identify the interaction of each group with the corresponding horizontal
 * or vertical boundaries. (Note that, if the current print direciton is not
 * zero, what is called the "left" logical page boundary would, from the
 * point of view of the logical page, be given a different label.)
 *
 * Horizontal motion commmands (note: a movement "transitions" a boundary if
 * the current point before and after the movement is on opposite sides of
 * the boundary):
 *
 *     a. Horizontal position by column, decipoint, or PCL unit, in absolute
 *        or relative mode, and horizontal motion due to rasters:
 *
 *         "left" logical page boundary is used as origin for absolute positions
 *         movement to the left of the left logical page boundary is clamped
 *             to that boundary
 *         left text boundary is ignored
 *         right text boundary is ignored
 *         movement beyond the "right" logical page boundary is clamped to
 *             that boundary
 *
 *     b. Tab (always relative)
 *
 *         "left" logical boundary is irrelevant
 *         left text boundary is used as the origin for tab stops
 *         movement that transitions the right text boundary is clamped to
 *             that boundary
 *         movement beyond the "right" logical page boundary is clamped
 *             to that boundary
 *
 *     c. Character or space (code legal in symbol set up not occupied by a
 *        printable character; motion always relative)
 *
 *         "left" logical page boundary is irrelevant
 *         left text boundary is ignored
 *         if the character WOULD transition the right text boundary, ignore
 *             the character or issue a CR/LF sequence BEFORE printing the
 *             character, base on whether or not end-of-line wrapping is
 *             enabled (with one exception; see below)
 *         if the character WOULD transition the "right" logical page  boundary,
 *             ignore the character or issue a CR/LF sequence BEFORE printing
 *             the character, base on whether or not end-of-line wrapping is
 *             enabled
 *
 *        Note that only one of the latter two operations will be preformed if
 *        the logical and text margins are the same.
 *
 *        An exception is made in third case above in the event that a back-
 *        space was received immediately before the character to be rendered.
 *        In that case, the character is rendered regardless of the transition
 *        of the right text boundary.
 *
 *     d. Carriage return (always absolute)
 *
 *         "left" logical page boundary is irrelevant
 *         left text boundary is used as the new horizontal location
 *         right text boundary is irrelevant
 *         "right" logical page boundary is irrelevant
 *
 *     e. Back space
 *
 *         movement beyond the "left" logical page boundary is clamped to
 *             that boundary
 *         movement that would transition the left text boundary is clamped
 *             to that boundary
 *         right text boundary is ignored (this is contrary to HP's
 *             documentation, but empirically verified on several machines)
 *         "right" logical page boundary is irrelevant
 *
 * In addtion, any horizontal movement to the left will "break" the current
 * underline (this is significant for floating underlines).
 *
 * Vertical motion commands:
 *
 *     f. Vertical cursor position by decipoints or PCL units (but NOT by
 *        rows), both absolute and relative
 *
 *         movement beyond the "top" logical page boundary is clamped to
 *             that boundary
 *         top text boundary is used as the origin for absolute moves
 *         bottom text margin is ignored
 *         movement beyond the "bottom" logical page boundary is clamped to
 *             that boundary
 *
 *     g. Absolute (NOT relative) vertical cursor position by rows
 *
 *         "top" logical page boundary is irrelevant (can only be reached by
 *             relative moves)
 *         top text boundary, offset by 75% of the VMI distance, is used as
 *             the origin
 *         bottom text margin is ignored
 *         movement beyond the "bottom" logical page boundary is clamped to
 *             that boundary
 *
 *     h. Relative (NOT absolute) vertical cursor position by rows, and both
 *        line-feed and half line-feed when perforation skip is disabled
 *
 *         movement beyond the "top" logical page boundary is clamped to
 *             that boundary
 *         if and advance of n rows (n == 1 for LF) is requested, and only
 *             m additional rows can be accommodated on the current page,
 *             an implicit page ejection occurs and the cursor is positioned
 *             n - m rows below the "top" logical page boundary on the
 *             subsequent page; if the subsequent page will not accommodate
 *             n - m rows, the process is repeated
 *         after an implicit page eject (see below), the cursor is positioned
 *             75% of the VMI distance below the "top" logical page boundary plus
 *
 *         top text boundary is ignored
 *         bottom text boundary is ignored
 *         movement beyond the "bottom" page boundary causes an implicit
 *             page eject
 *
 *     i. Line-feed and halft line feed, when perforation skip is enabled
 *
 *         "top" logical page boundary is irrelevant
 *         after an implicit page eject (see below), the cursor is set 75% of
 *             the VMI distance below the top text boundary
 *         any movement that ends below the bottom text boundary causes a
 *             page eject (NB: this does not require a transition of the
 *             boundary, as is the case for the right text boundary)
 *         the "bottom" logical page boundary is irrelevant
 *
 *     j. Form feed
 *
 *         "top" logical page boundary is irrelevant
 *         the cursor is positioned 75% of the VMI distance below the top
 *             text boundary
 *         bottom text boundary is irrelevant
 *         "bottom" logical page boundary is irrelevant
 *
 *
 *     k. for a relative vertical motion command relative movement can extend
 *        to the next logical page's lower boundary, where it is clamped.
 *        This should could be grouped with item h.
 *
 * Wow - 11 different forms to accommodate.
 *
 * The special handling required by character and space induced horizontal
 * movement is handled in pctext.c (pcl_show_chars); all other movement is
 * handled in this file.
 */

#define HOME_X(pcs) (pcs->margins.left)
#define DEFAULT_Y_START(pcs) ((3L * pcs->vmi_cp) / 4L)
#define HOME_Y(pcs) (pcs->margins.top + DEFAULT_Y_START(pcs))

  void
pcl_set_cap_x(
    pcl_state_t *   pcs,
    coord           x,
    bool            relative,
    bool            use_margins
)
{
    coord               old_x = pcs->cap.x;

    if (relative)
        x += pcs->cap.x;

    /* the horizontal text margins are only interesting in transition */
    if (use_margins) {
        coord   min_x =  pcs->margins.left;
        coord   max_x = pcs->margins.right;

        if ((old_x >= min_x) && (x < min_x))
            x = min_x;
        else if ((old_x <= max_x) && (x > max_x))
            x = max_x;
    }

    /* the logical page bounds always apply */
    x = ( x > pcs->xfm_state.pd_size.x ? pcs->xfm_state.pd_size.x
                                       : (x < 0L ? 0L : x) );

    /* leftward motion "breaks" an underline */
    if (x < old_x) {
        pcl_break_underline(pcs);
        pcs->cap.x = x;
        pcl_continue_underline(pcs);
    } else
        pcs->cap.x = x;
}

  int
pcl_set_cap_y(
    pcl_state_t *   pcs,
    coord           y,
    bool            relative,
    bool            use_margins,
    bool            by_row,
    bool            by_row_command
)
{
    coord           lim_y = pcs->xfm_state.pd_size.y;
    coord           max_y = pcs->margins.top + pcs->margins.length;
    bool            page_eject = by_row && relative;

    /* this corresponds to rule 'k' above. */
    if (relative && by_row_command) {
        /* calculate the advance to the next logical page bound.  Note
           margins are false if by_row_command is true. */
        coord advance_max = 2 * lim_y - pcs->cap.y;
        /* clamp */
        y = (y < advance_max ? y : advance_max + HOME_Y(pcs));
    }

    /* adjust the vertical position provided */
    if (relative)
        y += pcs->cap.y;
    else
        y += (by_row ? HOME_Y(pcs) : pcs->margins.top);

    /* vertical moves always "break" underlines */
    pcl_break_underline(pcs);

    max_y = (use_margins ? max_y : lim_y);
    if (y < 0L)
        pcs->cap.y = 0L;
    else if (y <= max_y)
        pcs->cap.y = y;
    else if (!page_eject)
        pcs->cap.y = (y <= lim_y ? y : lim_y);
    else {
        coord   vmi_cp = pcs->vmi_cp;
        coord   y0 = pcs->cap.y;

        while (y > max_y) {
            int    code = pcl_end_page_always(pcs);

            if (code < 0)
                return code;
            y -= (y0 <= max_y ? max_y : y0);
            y0 = (use_margins ? HOME_Y(pcs) : DEFAULT_Y_START(pcs));

            /* if one VMI distance or less remains, always exit */
            if ((vmi_cp == 0) || (y <= vmi_cp)) {
                y = y0;
                break;
            }

            /* otherwise, round to a multiple of VMI distance */
            y += y0 - 1 - ((y - 1) % vmi_cp);
        }
        pcs->cap.y = y;
    }

    pcl_continue_underline(pcs);
    return 0;
}

static inline float
motion_args(pcl_args_t *pargs, bool truncate)
{
    float arg = float_arg(pargs);
    if (truncate)
        arg = floor(arg);
    return arg;
}

/* some convenient short-hand for the cursor movement commands */

static inline void
do_horiz_motion(
   pcl_args_t  *pargs,
   pcl_state_t *pcs,
   coord        mul,
   bool          truncate_arg
)
{
    pcl_set_cap_x(pcs, motion_args(pargs, truncate_arg) * mul, arg_is_signed(pargs), false);
    return;
}


static inline int
do_vertical_move(pcl_state_t *pcs, pcl_args_t *pargs, float mul,
                 bool use_margins, bool by_row, bool by_row_command, bool truncate_arg)
{
    return pcl_set_cap_y(pcs, motion_args(pargs, truncate_arg) * mul,
                         arg_is_signed(pargs), use_margins, by_row,
                         by_row_command);
}

/*
 * Control character action implementation.
 *
 * These routines perform just the individual actions. The control character
 * routines may invoke several of these, based on the selected line termination
 * setting.
 *
 * do_CR and do_LF are exported for use by the text manipulation routines and
 * the display functions.
 *
 * Note: CR always "breaks" an underline, even if it is a movement to the right.
 */
  void
pcl_do_CR(
    pcl_state_t *   pcs
)
{
    pcl_break_underline(pcs);
    pcl_set_cap_x(pcs, pcs->margins.left, false, false);
    pcl_continue_underline(pcs);
}

  int
pcl_do_LF(
    pcl_state_t *   pcs
)
{
    return pcl_set_cap_y( pcs,
                          pcs->vmi_cp,
                          true,
                          (pcs->perforation_skip == 1),
                          true,
                          false
                          );
}

/*
 * Unconditionally feed a page, and move the the "home" verical position on
 * the followin page.
 */
  int
pcl_do_FF(
    pcl_state_t *   pcs
)
{
    int             code = pcl_end_page_always(pcs);

    if (code >= 0) {
        code = pcl_set_cap_y(pcs, 0L, false, false, true, false);
        pcl_continue_underline(pcs);    /* (after adjusting y!) */
    }
    return code;
}

/*
 * Return the cursor to its "home" position
 */
  void
pcl_home_cursor(
    pcl_state_t *   pcs
)
{
    pcl_set_cap_x(pcs, pcs->margins.left, false, false);
    pcl_set_cap_y(pcs, 0L, false, false, true, false);
}

/*
 * Update the HMI by recomputing it from the font.
 */
  coord
pcl_updated_hmi(
    pcl_state_t *                   pcs
)
{
    coord                           hmi;
    const pcl_font_selection_t *    pfs =
                                     &(pcs->font_selection[pcs->font_selected]);
    int                             code = pcl_recompute_font(pcs, false);
    const pl_font_t *               plfont = pcs->font;

    if (code < 0)
        return pcs->hmi_cp;     /* bad news; don't mark the HMI as valid. */

    /* we check for typeface == 0 here (lineprinter) because we
       frequently simulate lineprinter with a scalable truetype
       font */
    if (pl_font_is_scalable(plfont) && plfont->params.typeface_family != 0) {
        if (plfont->params.proportional_spacing)
            /* Scale the font's pitch by the requested height. */
            hmi = pl_fp_pitch_cp(&plfont->params) / 10.0 * pfs->params.height_4ths / 4;
        else
            hmi = pl_fp_pitch_cp(&(pfs->params));
    } else
        hmi = pl_fp_pitch_cp(&(plfont->params));

    /*
     * Round to a multiple of the unit of measure (see the "PCL 5 Printer
     * LanguageTechnical Reference Manual", October 1992 ed., page 5-22.
     */
    hmi = hmi + pcs->uom_cp / 2;
    return pcs->hmi_cp = hmi - (hmi % pcs->uom_cp);
}


/* Commands */

/*
 * ESC & k <x> H
 *
 * Set horizontal motion index.
 */
  static int
set_horiz_motion_index(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    /* HMI in 120 units converted to 7200 units with roundup */
    pcs->hmi_cp = (coord)((fabs(float_arg(pargs)) * 60.0) + 0.5);
    return 0;
}

/*
 * ESC & l <y> C
 *
 * Set vertical motion index.
 *
 * Contrary to HP's documentation ("PCL 5 Printer Language Technical Reference
 * Manual", October 1992 ed., p. 5-24), this command is NOT ignored if the
 * requested VMI is greater than the page length.
 *
 * Apparently this problem has been fixed in the Color Laserjet 4600.
 * For the old behavior undefine the next definition.
 */

#define HP_VERT_MOTION_NEW
  static int
set_vert_motion_index(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    /* LMI :== 48.0 / lpi;  ie 0.16 = 48/300;
     * convert to pcl_coord_scale (7200), roundup the float prior to truncation.
     */
    coord vcp = ((fabs(float_arg(pargs)) * 7200.0 / 48.0) + 0.5);
#ifdef HP_VERT_MOTION_NEW
    if (vcp <= pcs->xfm_state.pd_size.y)
#endif
        pcs->vmi_cp = vcp;
    return 0;
}

#undef HP_VERT_MOTION_NEW

/*
 * ESC & l <lpi> D
 *
 * Set line spacing. Though it is not documented anywhere, various HP devices
 * agree that a zero operand specifies 12 lines per inch (NOT the default).
 */
  static int
set_line_spacing(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            lpi = uint_arg(pargs);

    if (lpi == 0)                   /* 0 ==> 12 lines per inch */
        lpi = 12;
    if ((48 % lpi) == 0)            /* lpi must divide 48 */
        pcs->vmi_cp = inch2coord(1.0 / lpi);
    return 0;
}

/*
 * ESC & k G
 */
  static int
set_line_termination(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            ui = uint_arg(pargs);

    if (ui <= 3)
        pcs->line_termination = ui;
    return 0;
}


/*
 * ESC & a <cols> C
 */
  static int
horiz_cursor_pos_columns(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    do_horiz_motion(pargs, pcs, pcl_hmi(pcs), false);
    return 0;
}

/*
 * ESC & a <dp> H
 */
  static int
horiz_cursor_pos_decipoints(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    do_horiz_motion(pargs, pcs, 10.0, false);
    return 0;
}

/*
 * ESC * p <units> X
 */
  static int
horiz_cursor_pos_units(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    if ( pcs->personality == rtl )
        dprintf("Warning: device/resolution dependent units used\n" );
    do_horiz_motion(pargs, pcs, pcs->uom_cp, true);
    return 0;
}

/*
 * CR
 */
  static int
cmd_CR(
    pcl_args_t *    pargs,  /* ignored */
    pcl_state_t *   pcs
)
{
    pcl_do_CR(pcs);
    return ((pcs->line_termination & 1) != 0 ? pcl_do_LF(pcs) : 0);
}

/*
 * BS
 */
  static int
cmd_BS(
    pcl_args_t *    pargs,  /* ignored */
    pcl_state_t *   pcs
)
{
    pcl_set_cap_x(pcs, -pcs->last_width, true, true);
    pcs->last_was_BS = true;
    return 0;
}

/*
 * HT
 *
 * Tabs occur at ever 8 columns, measure from the left text margin.
 */
  static int
cmd_HT(
    pcl_args_t *    pargs,  /* ignored */
    pcl_state_t *   pcs
)
{
    coord           x = pcs->cap.x - pcs->margins.left;
    coord           tab;

    if (x < 0)
        x = -x;
    else if ((tab = 8 * pcl_hmi(pcs)) > 0)
        x = tab - (x % tab);
    else
        x = 0L;
    pcl_set_cap_x(pcs, x, true, true);
    return 0;
}


/*
 * ESC & a <rows> R
 */
  static int
vert_cursor_pos_rows(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    return do_vertical_move(pcs, pargs, pcs->vmi_cp, false, true, true, false);
}

/*
 * ESC & a <dp> V
 */
  static int
vert_cursor_pos_decipoints(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    return do_vertical_move(pcs, pargs, 10.0, false, false, false, false);
}

/*
 * ESC * p <units> Y
 */
  static int
vert_cursor_pos_units(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    if ( pcs->personality == rtl )
        dprintf("Warning: device/resolution dependent units used\n" );
    return do_vertical_move(pcs, pargs, pcs->uom_cp, false, false, false, true);
}

/*
 * ESC =
 */
  static int
half_line_feed(
    pcl_args_t *    pargs,  /* ignored */
    pcl_state_t *   pcs
)
{
    return pcl_set_cap_y( pcs,
                          pcs->vmi_cp / 2,
                          true,
                          (pcs->perforation_skip == 1),
                          true,
                          false
                          );
}

/*
 * LF
 */
  static int
cmd_LF(
    pcl_args_t *    pargs,  /* ignored */
    pcl_state_t *   pcs
)
{
    if ((pcs->line_termination & 2) != 0)
        pcl_do_CR(pcs);
    return pcl_do_LF(pcs);
}

/*
 * FF
 */
  static int
cmd_FF(
    pcl_args_t *    pargs,  /* ignored */
    pcl_state_t *   pcs
)
{
    if ((pcs->line_termination & 2) != 0)
        pcl_do_CR(pcs);
    return pcl_do_FF(pcs);
}


/*
 * ESC & f <pp_enum> S
 *
 * Contrary to what is indicated in the "PCL 5 Printer Language Technical
 * Reference Manual", October 1992 ed., p. 6-16, pushd cursors are stored
 * in logical page space, not device space.
 */
  static int
push_pop_cursor(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    int             type = uint_arg(pargs);

    if ((type == 0) && (pcs->cursor_stk_size < countof(pcs->cursor_stk))) {
        gs_point *  ppt = &(pcs->cursor_stk[pcs->cursor_stk_size++]);

        ppt->x = (double)pcs->cap.x;
        ppt->y = (double)pcs->cap.y;
        gs_point_transform( ppt->x, ppt->y, &(pcs->xfm_state.pd2lp_mtx), ppt);

    } else if ((type == 1) && (pcs->cursor_stk_size > 0)) {
        gs_point *  ppt = &(pcs->cursor_stk[--pcs->cursor_stk_size]);
        gs_matrix   lp2pd;

        pcl_invert_mtx(&(pcs->xfm_state.pd2lp_mtx), &lp2pd);
        gs_point_transform(ppt->x, ppt->y, &lp2pd, ppt);
        pcl_set_cap_x(pcs, (coord)ppt->x, false, false);
        pcl_set_cap_y( pcs,
                       (coord)ppt->y - pcs->margins.top,
                       false,
                       false,
                       false,
                       false
                       );
    }

    return 0;
}

static int
pcursor_do_copy(pcl_state_t *psaved,
                const pcl_state_t *pcs, pcl_copy_operation_t operation)
{
    int i;

    /* don't restore the current cap.  The cap is not part of the
       state */
    if ( operation & pcl_copy_after ) {
        psaved->cap = pcs->cap;

        /* cursor stack isn't part of the state, either */
        for (i = 0; i < pcs->cursor_stk_size; ++i) {
            psaved->cursor_stk[i] = pcs->cursor_stk[i];
        }
        psaved->cursor_stk_size = pcs->cursor_stk_size;

        /* NB doesn't belong here */
        psaved->page_marked = pcs->page_marked;
    }
    return 0;
}

/*
 * Initialization
 */
  static int
pcursor_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *   pmem
)
{

    DEFINE_CLASS('&')
    {
        'k', 'H',
        PCL_COMMAND( "Horizontal Motion Index",
                     set_horiz_motion_index,
                     pca_neg_ok | pca_big_clamp
                     )
    },
    {
        'l', 'C',
        PCL_COMMAND( "Vertical Motion Index",
                     set_vert_motion_index,
                     pca_neg_ok | pca_big_ignore
                     )
    },
    {
        'l', 'D',
        PCL_COMMAND( "Line Spacing",
                     set_line_spacing,
                     pca_neg_ok | pca_big_ignore
                     )
    },
    {
        'k', 'G',
         PCL_COMMAND( "Line Termination",
                      set_line_termination,
                      pca_neg_ok | pca_big_ignore
                      )
    },
    {
        'a', 'C',
         PCL_COMMAND( "Horizontal Cursor Position Columns",
                      horiz_cursor_pos_columns,
                      pca_neg_ok | pca_big_ok
                      )
    },
    {
        'a', 'H',
        PCL_COMMAND( "Horizontal Cursor Position Decipoints",
                     horiz_cursor_pos_decipoints,
                     pca_neg_ok | pca_big_ok
                     )
    },
    {
        'a', 'R',
        PCL_COMMAND( "Vertical Cursor Position Rows",
                     vert_cursor_pos_rows,
                     pca_neg_ok | pca_big_clamp
                     )
    },
    {
        'a', 'V',
        PCL_COMMAND( "Vertical Cursor Position Decipoints",
                     vert_cursor_pos_decipoints,
                     pca_neg_ok | pca_big_ok
                     )
    },
    {
        'f', 'S',
        PCL_COMMAND( "Push/Pop Cursor",
                     push_pop_cursor,
                     pca_neg_ok | pca_big_ignore
                     )
    },
    END_CLASS

    DEFINE_CLASS('*')
    {
        'p', 'X',
        PCL_COMMAND( "Horizontal Cursor Position Units",
                     horiz_cursor_pos_units,
                     pca_neg_ok | pca_big_ok | pca_in_rtl
                     )
    },
    {
        'p', 'Y',
        PCL_COMMAND( "Vertical Cursor Position Units",
                     vert_cursor_pos_units,
                     pca_neg_ok | pca_big_ok  | pca_in_rtl
                     )
    },
    END_CLASS

    DEFINE_CONTROL(CR, "CR", cmd_CR)
    DEFINE_CONTROL(BS, "BS", cmd_BS)
    DEFINE_CONTROL(HT, "HT", cmd_HT)
    DEFINE_ESCAPE('=', "Half Line Feed", half_line_feed)
    DEFINE_CONTROL(LF, "LF", cmd_LF)
    DEFINE_CONTROL(FF, "FF", cmd_FF)

    return 0;
}

  static void
pcursor_do_reset(
    pcl_state_t *       pcs,
    pcl_reset_type_t    type
)
{
    static  const uint  mask = (  pcl_reset_initial
                                | pcl_reset_printer
                                | pcl_reset_overlay );

    if ((type & mask) == 0)
        return;

    pcs->line_termination = 0;
    pcs->hmi_cp = HMI_DEFAULT;
    pcs->vmi_cp = pcs->margins.length
          / pjl_proc_vartoi(pcs->pjls, pjl_proc_get_envvar(pcs->pjls, "formlines"));
    if ( (type & pcl_reset_overlay) == 0 ) {
        pcs->cursor_stk_size = 0;

        /*
         * If this is an initial reset, make sure underlining is
         * disabled (homing the cursor may cause an underline to be
         * put out.  And provide reasonable initial values for the
         * cap.
         */
        if ((type & pcl_reset_initial) != 0) {
            pcs->underline_enabled = false;
            /* WRONG why is the cap set to 0 and then the
               pcl_home_cursor(pcs) */
            pcs->cap.x = pcs->cap.y = 0;
        }
    }
    pcl_home_cursor(pcs);
}

const pcl_init_t    pcursor_init = { pcursor_do_registration, pcursor_do_reset, pcursor_do_copy };
