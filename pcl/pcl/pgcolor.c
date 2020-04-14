/* Copyright (C) 2001-2020 Artifex Software, Inc.
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


/* pgcolor.c - HP-GL/2 color vector graphics commands */

#include "std.h"
#include "pcparse.h"
#include "pgmand.h"
#include "pginit.h"
#include "pgmisc.h"
#include "pgdraw.h"
#include "gsstate.h"            /* for gs_setfilladjust */
#include "pcpalet.h"

/* ------ Commands ------ */

/*
 * PC [pen[,primary1,primary2,primary3];
 */
static int
hpgl_PC(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    int32 pen;
    int32 npen = pcl_palette_get_num_entries(pgls->ppalet);

    if (pgls->personality == pcl5e)
        return 0;

    /* output any current path */
    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));

    if (hpgl_arg_int(pgls->memory, pargs, &pen)) {
        hpgl_real_t primary[3];

        if ((pen < 0) || (pen >= npen))
            return e_Range;

        if (hpgl_arg_c_real(pgls->memory, pargs, &primary[0])) {
            float comps[3];

            if (!hpgl_arg_c_real(pgls->memory, pargs, &primary[1]) ||
                !hpgl_arg_c_real(pgls->memory, pargs, &primary[2]))
                return e_Range;
            comps[0] = primary[0];
            comps[1] = primary[1];
            comps[2] = primary[2];
            return pcl_palette_set_color(pgls, pen, comps);
        } else
            return pcl_palette_set_default_color(pgls, pen);
    } else {
        int i;
        int code;

        for (i = 0; i < npen; ++i) {
            if ((code = pcl_palette_set_default_color(pgls, i)) < 0)
                return code;
        }
    }

    return 0;
}

/*
 * NP [n];
 */
int
hpgl_NP(hpgl_args_t * pargs, hpgl_state_t * pgls)
{
    int32 n = 8;

    if (pgls->personality == pcl5e)
        return 0;

    /* output any current path */
    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));

    if (hpgl_arg_int(pgls->memory, pargs, &n) && ((n < 2) || (n > 32768)))
        return e_Range;
    return pcl_palette_NP(pgls, n);
}

/*
 * CR [black reference point red, white reference point red, ...];
 */
int
hpgl_CR(hpgl_args_t * pargs, hpgl_state_t * pgls)
{

    double b_ref_r, b_ref_g, b_ref_b, w_ref_r, w_ref_g, w_ref_b;

    if (pgls->personality == pcl5e)
        return 0;

    if (hpgl_arg_c_real(pgls->memory, pargs, &b_ref_r))
        if (!hpgl_arg_c_real(pgls->memory, pargs, &w_ref_r) ||
            !hpgl_arg_c_real(pgls->memory, pargs, &b_ref_g) ||
            !hpgl_arg_c_real(pgls->memory, pargs, &w_ref_g) ||
            !hpgl_arg_c_real(pgls->memory, pargs, &b_ref_b) ||
            !hpgl_arg_c_real(pgls->memory, pargs, &w_ref_b))
            return e_Range;
        else
            return pcl_palette_CR(pgls, w_ref_r, w_ref_g, w_ref_b,
                                  b_ref_r, b_ref_g, b_ref_b);
    else                        /* no args - default references */
        return pcl_palette_CR(pgls, 255, 255, 255, 0, 0, 0);
}

/*
 * Initialization. There is no reset or copy command, as those operations are
 * carried out by the palette mechanism.
 */
static int
pgcolor_do_registration(pcl_parser_state_t * pcl_parser_state,
                        gs_memory_t * mem)
{
    /* Register commands */
    DEFINE_HPGL_COMMANDS(mem)
        HPGL_COMMAND('N', 'P', hpgl_NP, hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('P', 'C', hpgl_PC, hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('C', 'R', hpgl_CR, hpgl_cdf_pcl_rtl_both),
    END_HPGL_COMMANDS
    return 0;
}

const pcl_init_t pgcolor_init = { pgcolor_do_registration, 0, 0 };
