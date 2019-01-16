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


/* pgchar.c */
/* HP-GL/2 font and character commands */
#include "math_.h"
#include "stdio_.h"             /* for gdebug.h */
#include "gdebug.h"
#include "pcparse.h"
#include "pgmand.h"
#include "pgdraw.h"
#include "pginit.h"
#include "pggeom.h"
#include "pgmisc.h"
#include "pcfsel.h"
#include "pcpalet.h"

/* ------ Internal procedures ------ */

/* Define font parameters (AD, SD). */
static int
hpgl_font_definition(hpgl_args_t * pargs, hpgl_state_t * pgls, int index)
{
    /*
     * Since these commands take an arbitrary number of arguments, we
     * reset the argument bookkeeping after each group.  We reset
     * phase to 1, 2, or 3 after seeing the first pair, so we can tell
     * whether there were any arguments at all.  (1 means no parameter
     * changed, >1 means some parameter changed.)
     */

    pcl_font_selection_t *pfs = &pgls->g.font_selection[index];
#define pfp (&pfs->params)
    int kind;

    pfs->selected_id = (uint) - 1;
    for (; hpgl_arg_c_int(pgls->memory, pargs, &kind); pargs->phase |= 1)
        switch (kind) {
            case 1:            /* symbol set */
                {
                    int32 sset;

                    if (!hpgl_arg_int(pgls->memory, pargs, &sset))
                        return e_Range;
                    if (pfp->symbol_set != (uint) sset)
                        pfp->symbol_set = (uint) sset, pargs->phase |= 2;
                }
                break;
            case 2:            /* spacing */
                {
                    int spacing;

                    if (!hpgl_arg_c_int(pgls->memory, pargs, &spacing))
                        return e_Range;
                    if (((spacing == 1) || (spacing == 0))
                        && (pfp->proportional_spacing != spacing))
                        pfp->proportional_spacing = spacing, pargs->phase |=
                            2;
                }
                break;
            case 3:            /* pitch */
                {
                    hpgl_real_t pitch;

                    if (!hpgl_arg_c_real(pgls->memory, pargs, &pitch))
                        return e_Range;
                    if ((pl_fp_pitch_per_inch(pfp) != pitch) &&
                        (pitch >= 0) && (pitch < 32768.0)) {
                        pl_fp_set_pitch_per_inch(pfp,
                                                 pitch >
                                                 7200.0 ? 7200.0 : pitch);
                        pargs->phase |= 2;
                    }

                }
                break;
            case 4:            /* height */
                {
                    hpgl_real_t height;

                    if (!hpgl_arg_c_real(pgls->memory, pargs, &height))
                        return e_Range;
                    if ((pfp->height_4ths != (uint) (height * 4))
                        && (height >= 0)) {
                        /* minimum height for practical purposes is one
                           quarter point.  The HP Spec says 0 is legal
                           but does not specify what a height of zero
                           means.  The previous code truncated height,
                           it probably should be rounded as in pcl but
                           doing so would change a lot of files for no
                           compelling reason so for now truncate. */
                        uint trunc_height_4ths = (uint) (height * 4);

                        pfp->height_4ths =
                            (trunc_height_4ths == 0 ? 1 : trunc_height_4ths);
                        pargs->phase |= 2;
                    }
                }
                break;
            case 5:            /* posture */
                {
                    int posture;

                    if (!hpgl_arg_c_int(pgls->memory, pargs, &posture))
                        return e_Range;
                    if (pfp->style != posture)
                        pfp->style = posture, pargs->phase |= 2;

                }
                break;
            case 6:            /* stroke weight */
                {
                    int weight;

                    if (!hpgl_arg_c_int(pgls->memory, pargs, &weight))
                        return e_Range;
                    if (pfp->stroke_weight != weight)
                        if (((weight >= -7) && (weight <= 7))
                            || (weight == 9999))
                            pfp->stroke_weight = weight, pargs->phase |= 2;
                }
                break;
            case 7:            /* typeface */
                {
                    int32 face;

                    if (!hpgl_arg_int(pgls->memory, pargs, &face))
                        return e_Range;
                    if (pfp->typeface_family != (uint) face)
                        pfp->typeface_family = (uint) face, pargs->phase |= 2;
                }
                break;
            default:
                return e_Range;
        }
    /* If there were no arguments at all, default all values. */
    if (!pargs->phase)
        hpgl_default_font_params(pfs);
    if (pargs->phase != 1) {    /* A value changed, or we are defaulting.  Decache the font. */
        pfs->font = 0;
        if (index == pgls->g.font_selected)
            pgls->g.font = 0;
    }
    return 0;
}
/* Define label drawing direction (DI, DR). */
static int
hpgl_label_direction(hpgl_args_t * pargs, hpgl_state_t * pgls, bool relative)
{
    hpgl_real_t run = 1, rise = 0;

    if (hpgl_arg_c_real(pgls->memory, pargs, &run)) {
        if (!hpgl_arg_c_real(pgls->memory, pargs, &rise)
            || (run == 0 && rise == 0))
            return e_Range;
        {
            double hyp = hypot(run, rise);

            run /= hyp;
            rise /= hyp;
        }
    }
    pgls->g.character.direction.x = run;
    pgls->g.character.direction.y = rise;
    pgls->g.character.direction_relative = relative;
    hpgl_call(hpgl_update_carriage_return_pos(pgls));
    return 0;
}

/* forward declaration */
static int hpgl_select_font(hpgl_state_t * pgls, int index);

/* Select font by ID (FI, FN). */
static int
hpgl_select_font_by_id(hpgl_args_t * pargs, hpgl_state_t * pgls, int index)
{
    pcl_font_selection_t *pfs = &pgls->g.font_selection[index];
    int32 id;
    int code;

    if (!hpgl_arg_c_int(pgls->memory, pargs, &id) || id < 0)
        return e_Range;
    code = pcl_select_font_by_id(pfs, id, pgls /****** NOTA BENE ******/ );
    switch (code) {
        default:               /* error */
            return code;
        case 1:                /* ID not found, no effect */
            return 0;
        case 0:                /* ID found */
            break;
    }
    pgls->g.font_selection[index].font = pfs->font;
    pgls->g.font_selection[index].map = pfs->map;
    /*
     * If we just selected a bitmap font, force the equivalent of SB1.
     * See TRM 23-65 and 23-81.
     */
    if (pfs->font->scaling_technology == plfst_bitmap) {
        hpgl_args_t args;

        hpgl_args_setup(&args);
        hpgl_args_add_int(&args, 1);
        hpgl_call(hpgl_SB(&args, pgls));
    }
    /* note pcltrm 23-54 - only select if the table (primary or
       secondary) matches the currently selected table. */
    if (index == pgls->g.font_selected) {
        hpgl_select_font(pgls, index);
    }
    return 0;
}

/* Select font (SA, SS). */
static int
hpgl_select_font(hpgl_state_t * pgls, int index)
{
    pgls->g.font_selected = index;
    pgls->g.font = pgls->g.font_selection[index].font;
    pgls->g.map = pgls->g.font_selection[index].map;
    return 0;
}

/* ------ Commands ------ */

/* AD [kind,value...]; */
int
hpgl_AD(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    return hpgl_font_definition(pargs, pgls, 1);
}

#define CHAR_EDGE_PEN_UNSET -1

/* return the current edge pen based on whethere or not the interpreter specifically set the pen */
int32
hpgl_get_character_edge_pen(hpgl_state_t * pgls)
{
    /* if the character edge pen has not been set then we return the
       current pen number, otherwise the state value as set in the CF
       command is used.  (see hpgl_CF) */
    return (pgls->g.character.edge_pen == CHAR_EDGE_PEN_UNSET ?
            hpgl_get_selected_pen(pgls) : pgls->g.character.edge_pen);

}

/*
 * CF [mode[,pen]];
 */
int
hpgl_CF(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    int mode = 0;
    int npen = pcl_palette_get_num_entries(pgls->ppalet);
    int32 pen = 0;

    if (hpgl_arg_c_int(pgls->memory, pargs, &mode)) {
        if ((mode & ~3) != 0)
            return e_Range;
        /* With only 1 argument, we "unset" the current pen.  This
           causes the drawing machinery to use the current pen when
           the stroke is rendered (i.e. a subsequent SP will change
           the character edge pen */
        if (hpgl_arg_int(pgls->memory, pargs, &pen)) {
            if ((pen < 0) || (pen >= npen))
                return e_Range;
        } else
            pen = CHAR_EDGE_PEN_UNSET;
    }
    pgls->g.character.fill_mode = mode;
    pgls->g.character.edge_pen = pen;
    return 0;
}

#undef CHAR_EDGE_PEN_UNSET

/* DI [run,rise]; */
int
hpgl_DI(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    return hpgl_label_direction(pargs, pgls, false);
}

/* DR [run,rise]; */
int
hpgl_DR(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    return hpgl_label_direction(pargs, pgls, true);
}

/* DT terminator[,mode]; */
int
hpgl_DT(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    const byte *p = pargs->source.ptr;
    const byte *rlimit = pargs->source.limit;
    byte ch = (byte) pargs->phase;
    int mode = 1;

    /* We use phase to remember the terminator character */
    /* in case we had to restart execution. */
    if (p >= rlimit)
        return gs_error_NeedInput;
    if (!ch)
        switch ((ch = *++p)) {
            case ';':
                pargs->source.ptr = p;
                pgls->g.label.terminator = 3;
                pgls->g.label.print_terminator = false;
                return 0;
            case 0:
            case 5:
            case 27:
                return e_Range;
            default:
                if (p >= rlimit)
                    return gs_error_NeedInput;
                if (*++p == ',') {
                    pargs->source.ptr = p;
                    pargs->phase = ch;
                    if_debug1m('i', pgls->memory, "%c", ch);
                }
        }
    if (hpgl_arg_c_int(pgls->memory, pargs, &mode) && (mode & ~1))
        return e_Range;
    pgls->g.label.terminator = ch;
    pgls->g.label.print_terminator = !mode;
    return 0;
}

/* DV [path[,line]]; */
int
hpgl_DV(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    int path;
    int line;

    if (!hpgl_arg_c_int(pgls->memory, pargs, &path))
        path = 0;
    
    if (!hpgl_arg_c_int(pgls->memory, pargs, &line))
        line = 0;

    if ((path & ~3) | (line & ~1))
        return e_Range;
    pgls->g.character.text_path = path;
    pgls->g.character.line_feed_direction = (line ? -1 : 1);
    hpgl_call(hpgl_update_carriage_return_pos(pgls));
    return 0;
}

/* ES [width[,height]]; */
int
hpgl_ES(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    hpgl_real_t width = 0, height = 0;

    hpgl_arg_c_real(pgls->memory, pargs, &width);
    hpgl_arg_c_real(pgls->memory, pargs, &height);
    pgls->g.character.extra_space.x = width;
    pgls->g.character.extra_space.y = height;
    return 0;
}

/* FI fontid; */
int
hpgl_FI(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    return hpgl_select_font_by_id(pargs, pgls, 0);
}

/* FN fontid; */
int
hpgl_FN(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    return hpgl_select_font_by_id(pargs, pgls, 1);
}

/* The following is an extension documented in the Comparison Guide. */
/* LM [mode[, row number]]; */
int
hpgl_LM(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    int mode;
    int row_number;
    int old_mode =
        (pgls->g.label.double_byte ? 1 : 0) +
        (pgls->g.label.write_vertical ? 2 : 0);

    if (!hpgl_arg_c_int(pgls->memory, pargs, &mode))
        mode = 0;

    if (!hpgl_arg_c_int(pgls->memory, pargs, &row_number))
        row_number = 0;

    pgls->g.label.row_offset =
        (row_number < 0 ? 0 : row_number > 255 ? 255 : row_number) << 8;
    mode &= 3;
    pgls->g.label.double_byte = (mode & 1) != 0;
    pgls->g.label.write_vertical = (mode & 2) != 0;
    /*
     * The documentation says "When LM switches modes, it turns off
     * symbol mode."  We take this literally: LM only turns off
     * symbol mode if the new label mode differs from the old one.
     */
    if (mode != old_mode)
        pgls->g.symbol_mode = 0;
    return 0;
}

/* LO [origin]; */
int
hpgl_LO(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    int origin;

    if (!hpgl_arg_c_int(pgls->memory, pargs, &origin))
        origin = 1;

    if (origin < 1 || origin == 10 || origin == 20 || origin > 21)
        return e_Range;
    pgls->g.label.origin = origin;
    hpgl_call(hpgl_update_carriage_return_pos(pgls));
    return 0;
}

/* SA; */
int
hpgl_SA(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    return hpgl_select_font(pgls, 1);
}

/* SB [mode]; */
int
hpgl_SB(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    int mode = 0;

    if (hpgl_arg_c_int(pgls->memory, pargs, &mode) && (mode & ~1))
        return e_Range;
    {
        int i;

        pgls->g.bitmap_fonts_allowed = mode;
        /*
         * A different set of fonts is now available for consideration.
         * Decache any affected font(s): those selected by parameter,
         * and bitmap fonts selected by ID if bitmap fonts are now
         * disallowed.
         */
        for (i = 0; i < countof(pgls->g.font_selection); ++i) {
            pcl_font_selection_t *pfs = &pgls->g.font_selection[i];

            if (((int)pfs->selected_id < 0) ||
                (!mode && pfs->font != 0 &&
                 pfs->font->scaling_technology == plfst_bitmap)
                ) {
                pfs->font = 0;
            }
        }
        pgls->g.font = 0;
        pgls->g.map = 0;
    }
    return 0;
}

/* SD [kind,value...]; */
int
hpgl_SD(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    return hpgl_font_definition(pargs, pgls, 0);
}

/* SI [width,height]; */
int
hpgl_SI(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    hpgl_real_t width_cm, height_cm;

    if (hpgl_arg_c_real(pgls->memory, pargs, &width_cm)) {
        if (!hpgl_arg_c_real(pgls->memory, pargs, &height_cm))
            return e_Range;
        /* this isn't documented but HP seems to ignore the
           command (retains previous value) if either parameter is
           zero. NB probably should use epsilon have not tested. */
        if (width_cm == 0.0 || height_cm == 0.0)
            return e_Range;
        pgls->g.character.size.x = mm_2_plu(width_cm * 10);
        pgls->g.character.size.y = mm_2_plu(height_cm * 10);
        pgls->g.character.size_mode = hpgl_size_absolute;
    } else
        pgls->g.character.size_mode = hpgl_size_not_set;
    return 0;
}

#define MAX_SL_TANGENT 114.5887
/* SL [slant]; */
int
hpgl_SL(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    hpgl_real_t slant = 0;

    hpgl_arg_c_real(pgls->memory, pargs, &slant);
    /* clamp to 89.5 degrees of char slant, avoids math issues around
     * tan 90degrees == infinity.  Visually close to HP,
     * performance decrease as slant approaches tan(90).
     */

    pgls->g.character.slant = slant > MAX_SL_TANGENT ?
        MAX_SL_TANGENT : slant < -MAX_SL_TANGENT ? -MAX_SL_TANGENT : slant;
    return 0;
}
#undef MAX_SL_TANGENT

/* SR [width,height]; */
int
hpgl_SR(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    hpgl_real_t width_pct, height_pct;

    if (hpgl_arg_c_real(pgls->memory, pargs, &width_pct)) {
        if (!hpgl_arg_c_real(pgls->memory, pargs, &height_pct))
            return e_Range;
        /* this isn't documented but HP seems to ignore the
           command (retains previous value) if either parameter is
           zero. NB probably should use epsilon have not tested. */
        if (width_pct == 0.0 || height_pct == 0.0)
            return e_Range;
        pgls->g.character.size.x = width_pct / 100;
        pgls->g.character.size.y = height_pct / 100;
    } else {
        pgls->g.character.size.x = 0.0075;
        pgls->g.character.size.y = 0.015;
    }
    pgls->g.character.size_mode = hpgl_size_relative;
    return 0;
}

/* SS; */
int
hpgl_SS(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    return hpgl_select_font(pgls, 0);
}

/* TD [mode]; */
int
hpgl_TD(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    int mode = 0;

    if (hpgl_arg_c_int(pgls->memory, pargs, &mode) && (mode & ~1))
        return e_Range;
    pgls->g.transparent_data = mode;
    return 0;
}

/* Add a command to the DL table - pen up or pen down coordinates.
   Resizing the table as appropriate. */
static int
hpgl_add_dl_char_data(hpgl_state_t * pgls, hpgl_dl_cdata_t * pcdata,
                      short cc_data)
{
    /* characters stored as short */
    int csz = sizeof(short);

    if (pcdata->index == -1) {
        /* first element - allocate a small array which will be resized as needed */
        pcdata->data =
            (short *)gs_alloc_bytes(pgls->memory, 2 * csz, "DL data");
        if (pcdata->data == 0)
            return e_Memory;
    } else if (gs_object_size(pgls->memory, pcdata->data) ==
               (pcdata->index + 1) * csz) {
        /* new character doesn't fit - double the size of the array */
        short *new_cdata =
            (short *)gs_resize_object(pgls->memory, pcdata->data,
                                      (pcdata->index + 1) * csz * 2,
                                      "DL data resize");

        if (new_cdata == 0) {
            return e_Memory;
        }
        pcdata->data = new_cdata;
    }
    pcdata->data[++pcdata->index] = cc_data;
    return 0;
}

static void
hpgl_dl_dict_free_value(gs_memory_t * mem, void *value, client_name_t cname)
{
    hpgl_dl_cdata_t *cdata = (hpgl_dl_cdata_t *) value;

    if (cdata->data)
        gs_free_object(mem, cdata->data, cname);
    gs_free_object(mem, cdata, cname);
}


static void
hpgl_clear_dl_chars(hpgl_state_t * pgls)
{
    /* NB get rid of the 531 dictionary */
    return;
}

/* DL [Character Code, Coords] */
int
hpgl_DL(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    int code;
    hpgl_dl_cdata_t *cdata;

    /* first call */
    if (pargs->phase == 0) {
        int32 cc;

        /* double byte characters are not yet supported */
        if (pgls->g.label.double_byte)
            return e_Unimplemented;
        /* parse the character code, no arguments clears all defined
           characters */
        if (!hpgl_arg_c_int(pgls->memory, pargs, &cc)) {
            hpgl_clear_dl_chars(pgls);
            return 0;
        }

        if (cc < 0 || cc > 255)
            return e_Range;

        cdata =
            (hpgl_dl_cdata_t *) gs_alloc_bytes(pgls->memory,
                                               sizeof(hpgl_dl_cdata_t),
                                               "DL header");
        if (cdata == 0)
            return e_Memory;
        cdata->index = -1;
        cdata->data = NULL;
        id_set_value(pgls->g.current_dl_char_id, (ushort) cc);
        code = pl_dict_put(&pgls->g.dl_531_fontdict,
                    id_key(pgls->g.current_dl_char_id), 2, cdata);
        if (code < 0)
            return code;
        hpgl_args_init(pargs);
        pargs->phase = 1;
    }

    if (!pl_dict_find
        (&pgls->g.dl_531_fontdict, id_key(pgls->g.current_dl_char_id), 2,
         (void **)&cdata))
        /* this should be a failed assertion */
        return -1;
    do {
        int c;

        if (!hpgl_arg_c_int(pgls->memory, pargs, &c))
            break;
        code = hpgl_add_dl_char_data(pgls, cdata, c);
        if (code < 0) {
            pl_dict_undef(&pgls->g.dl_531_fontdict,
                          id_key(pgls->g.current_dl_char_id), 2);
            return code;
        }
        hpgl_args_init(pargs);
    } while (1);
    return 0;
}

/* Initialization */
static int
pgchar_do_registration(pcl_parser_state_t * pcl_parser_state,
                       gs_memory_t * mem)
{                               /* Register commands */
    DEFINE_HPGL_COMMANDS(mem)
        HPGL_COMMAND('A', 'D', hpgl_AD, hpgl_cdf_pcl_rtl_both), /* kind/value pairs */
        HPGL_COMMAND('C', 'F', hpgl_CF, hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('D', 'I', hpgl_DI, hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('D', 'L', hpgl_DL, hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('D', 'R', hpgl_DR, hpgl_cdf_pcl_rtl_both),
        /* DT has special argument parsing, so it must handle skipping */
        /* in polygon mode itself. */
        HPGL_COMMAND('D', 'T', hpgl_DT, hpgl_cdf_polygon | hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('D', 'V', hpgl_DV, hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('E', 'S', hpgl_ES, hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('F', 'I', hpgl_FI, hpgl_cdf_pcl),
        HPGL_COMMAND('F', 'N', hpgl_FN, hpgl_cdf_pcl),
        HPGL_COMMAND('L', 'M', hpgl_LM, hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('L', 'O', hpgl_LO, hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('S', 'A', hpgl_SA, hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('S', 'B', hpgl_SB, hpgl_cdf_pcl),
        HPGL_COMMAND('S', 'D', hpgl_SD, hpgl_cdf_pcl_rtl_both), /* kind/value pairs */
        HPGL_COMMAND('S', 'I', hpgl_SI, hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('S', 'L', hpgl_SL, hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('S', 'R', hpgl_SR, hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('S', 'S', hpgl_SS, hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('T', 'D', hpgl_TD, hpgl_cdf_pcl_rtl_both),
    END_HPGL_COMMANDS
    return 0;
}

static int
pgchar_do_reset(pcl_state_t * pcs, pcl_reset_type_t type)
{
    if (type & pcl_reset_initial)
        pl_dict_init(&pcs->g.dl_531_fontdict, pcs->memory,
                     hpgl_dl_dict_free_value);
    if (type & pcl_reset_permanent)
        pl_dict_release(&pcs->g.dl_531_fontdict);
    return 0;
}

const pcl_init_t pgchar_init = {
    pgchar_do_registration, pgchar_do_reset, 0
};
