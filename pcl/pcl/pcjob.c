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


/* pcjob.c -  PCL5 job control commands */
#include "std.h"
#include "gx.h"
#include "gsmemory.h"
#include "gspaint.h"
#include "gsmatrix.h"           /* for gsdevice.h */
#include "gsdevice.h"
#include "pcommand.h"
#include "pcursor.h"            /* for pcl_home_cursor() */
#include "pcstate.h"
#include "pcparam.h"
#include "pcdraw.h"
#include "pcpage.h"
#include "pjtop.h"
#include <stdlib.h>             /* for atof() */
#include "gzstate.h"
#include "plparams.h"

/* Commands */

int
pcl_do_printer_reset(pcl_state_t * pcs)
{
	int code = 0;
    if (pcs->macro_level)
        return e_Range;         /* not allowed inside macro */

    /* reset the other parser in case we have gotten the
       pcl_printer_reset while in gl/2 mode. */
    code = pcl_implicit_gl2_finish(pcs);
    if (code < 0)
        return code;
    /* Print any partial page if not pclxl snippet mode. */
    if (pcs->end_page == pcl_end_page_top) {
        code = pcl_end_page_if_marked(pcs);

        if (code < 0)
            return code;
        /* if duplex start on the front side of the paper */
        if (pcs->duplex)
            code = put_param1_bool(pcs, "FirstSide", true);
        if (code < 0)
            return code;
    }
    /* unload fonts */

    /* Reset to user default state. */
    return pcl_do_resets(pcs, pcl_reset_printer);
}

static int                      /* ESC E */
pcl_printer_reset(pcl_args_t * pargs, pcl_state_t * pcs)
{
    return pcl_do_printer_reset(pcs);
}

static int                      /* ESC % -12345 X */
pcl_exit_language(pcl_args_t * pargs, pcl_state_t * pcs)
{
    if (int_arg(pargs) != -12345)
        return e_Range;
    {
        int code = pcl_printer_reset(pargs, pcs);

        return (code < 0 ? code : e_ExitLanguage);
    }
}

static int                      /* ESC & l <num_copies> X */
pcl_number_of_copies(pcl_args_t * pargs, pcl_state_t * pcs)
{
    int i = int_arg(pargs);

    if (i < 1)
        return 0;
    pcs->num_copies = i;
    return put_param1_int(pcs, "NumCopies", i);
}

static int                      /* ESC & l <sd_enum> S */
pcl_simplex_duplex_print(pcl_args_t * pargs, pcl_state_t * pcs)
{
    int code;
    bool reopen = false;

    /* oddly the command goes to the next page irrespective of
       arguments */
    code = pcl_end_page_if_marked(pcs);
    if (code < 0)
        return code;
    pcl_home_cursor(pcs);
    switch (int_arg(pargs)) {
        case 0:
            pcs->duplex = false;
            pcs->back_side = false;
            break;
        case 1:
            pcs->duplex = true;
            pcs->back_side = false;
            pcs->bind_short_edge = false;
            break;
        case 2:
            pcs->duplex = true;
            pcs->back_side = false;
            pcs->bind_short_edge = true;
            break;
        default:
            return 0;
    }
    code = put_param1_bool(pcs, "Duplex", pcs->duplex);
    switch (code) {
        case 1:                /* reopen device */
            reopen = true;
        case 0:
            break;
        case gs_error_undefined:
            return 0;
        default:               /* error */
            if (code < 0)
                return code;
    }
    code = put_param1_bool(pcs, "FirstSide", !pcs->back_side);
    switch (code) {
        case 1:                /* reopen device */
            reopen = true;
        case 0:
            break;
        case gs_error_undefined:
            return 0;
        default:               /* error */
            if (code < 0)
                return code;
    }
    code = put_param1_bool(pcs, "BindShortEdge", pcs->bind_short_edge);
    switch (code) {
        case 1:                /* reopen device */
            reopen = true;
        case 0:
        case gs_error_undefined:
            break;
        default:               /* error */
            if (code < 0)
                return code;
    }
    return (reopen ? gs_setdevice_no_erase(pcs->pgs,
                                           gs_currentdevice(pcs->pgs)) : 0);
}

static int                      /* ESC & a <side_enum> G */
pcl_duplex_page_side_select(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint i = uint_arg(pargs);
    int code = 0;

    /* oddly the command goes to the next page irrespective of
       arguments */
    code = pcl_end_page_if_marked(pcs);
    if (i > 2)
        return 0;

    /* home the cursor even if the command has no effect */
    pcl_home_cursor(pcs);
    
    /* if there is an error (code < 0) or the page is unmarked (code
       == 0) then nothing to update */
    if (code <= 0)
        return code;


    if (pcs->duplex) {
        switch (i) {
        case 0:
            /* do nothing, back_side was updated by
               pcl_end_page_if_marked() above */
            break;
        case 1:
            pcs->back_side = false;
            break;
        case 2:
            pcs->back_side = true;
            break;
        default:
            /* can't happen */
            break;
        }
        code = put_param1_bool(pcs, "FirstSide", !pcs->back_side);
    }
    return code;
}

static int                      /* ESC & l 1 T */
pcl_job_separation(pcl_args_t * pargs, pcl_state_t * pcs)
{
    int i = int_arg(pargs);

    if (i != 1)
        return 0;
        /**** NEED A DRIVER PROCEDURE FOR END-OF-JOB ****/
    return 0;
}

static int                      /* ESC & l <bin_enum> G */
pcl_output_bin_selection(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint i = uint_arg(pargs);

    if (i < 1 || i > 2)
        return e_Range;
    return put_param1_int(pcs, "OutputBin", i);
}

static int                      /* ESC & u <upi> B */
pcl_set_unit_of_measure(pcl_args_t * pargs, pcl_state_t * pcs)
{
    int num = int_arg(pargs);

    if (num <= 96)
        num = 96;
    else if (num >= 7200)
        num = 7200;
    else if (7200 % num != 0) { /* Pick the exact divisor of 7200 with the smallest */
        /* relative error. */
        static const int values[] = {
            96, 100, 120, 144, 150, 160, 180, 200, 225, 240, 288,
            300, 360, 400, 450, 480, 600, 720, 800, 900,
            1200, 1440, 1800, 2400, 3600, 7200
        };
        const int *p = values;

        while (num > p[1])
            p++;
        /* Now *p < num < p[1]. */
        if ((p[1] - (float)num) / p[1] < ((float)num - *p) / *p)
            p++;
        num = *p;
    }
    pcs->uom_cp = pcl_coord_scale / num;
    return 0;
}

/* Initialization */
static int
pcjob_do_registration(pcl_parser_state_t * pcl_parser_state,
                      gs_memory_t * mem)
{                               /* Register commands */
    DEFINE_ESCAPE_ARGS('E', "Printer Reset", pcl_printer_reset, pca_in_rtl)
        DEFINE_CLASS('%') {
        0, 'X', {
    pcl_exit_language, pca_neg_ok | pca_big_error | pca_in_rtl}},
        END_CLASS DEFINE_CLASS('&') {
    'l', 'X',
            PCL_COMMAND("Number of Copies", pcl_number_of_copies,
                            pca_neg_ignore | pca_big_clamp)}, {
    'l', 'S',
            PCL_COMMAND("Simplex/Duplex Print", pcl_simplex_duplex_print,
                            pca_neg_ignore | pca_big_ignore)}, {
    'a', 'G',
            PCL_COMMAND("Duplex Page Side Select",
                            pcl_duplex_page_side_select,
                            pca_neg_ignore | pca_big_ignore)}, {
    'l', 'T',
            PCL_COMMAND("Job Separation", pcl_job_separation,
                            pca_neg_error | pca_big_error)}, {
    'l', 'G',
            PCL_COMMAND("Output Bin Selection", pcl_output_bin_selection,
                            pca_neg_error | pca_big_error)}, {
    'u', 'D',
            PCL_COMMAND("Set Unit of Measure", pcl_set_unit_of_measure,
                            pca_neg_error | pca_big_error | pca_in_rtl)},
        END_CLASS return 0;
}

static inline float
pcl_pjl_res(pcl_state_t * pcs)
{
    pjl_envvar_t *pres = pjl_proc_get_envvar(pcs->pjls, "resolution");

    return atof(pres);
}

static int
pcjob_do_copy(pcl_state_t *psaved, const pcl_state_t *pcs,
              pcl_copy_operation_t operation)
{
    if (operation & pcl_copy_after_overlay) {
        psaved->back_side = pcs->back_side;
    }
    return 0;
}

static int
pcjob_do_reset(pcl_state_t * pcs, pcl_reset_type_t type)
{
    int code;

    if (type & (pcl_reset_initial | pcl_reset_printer)) {
        /* check for a resolution setting from PJL and set it right
           away, there is no pcl command to set the resolution so a
           state variable is not needed.  Don't override -r from the command line. */
        if (!pcs->res_set_on_command_line) {
            gx_device *pdev = gs_currentdevice(pcs->pgs);
            float res[2];
            gs_point device_res;

            device_res.x = pdev->HWResolution[0];
            device_res.y = pdev->HWResolution[1];

            /* resolution is always a single number in PJL */
            res[0] = res[1] = pcl_pjl_res(pcs);

            /* 
             * again we only have a single number from PJL so just
             *  check the width
             */

            if (res[0] != 0 && res[0] != device_res.x) {
                code = put_param1_float_array(pcs, "HWResolution", res);
                if (code < 0)
                    return code;
                code = gs_erasepage(pcs->pgs);
                if (code < 0)
                    return code;
            }
        }

        pcs->num_copies = pjl_proc_vartoi(pcs->pjls,
                                          pjl_proc_get_envvar(pcs->pjls,
                                                              "copies"));
        pcs->duplex =
            !pjl_proc_compare(pcs->pjls,
                              pjl_proc_get_envvar(pcs->pjls, "duplex"),
                              "off") ? false : true;
        pcs->bind_short_edge =
            !pjl_proc_compare(pcs->pjls,
                              pjl_proc_get_envvar(pcs->pjls, "binding"),
                              "longedge") ? false : true;
        pcs->back_side = false;
        pcs->output_bin = 1;
        code = put_param1_bool(pcs, "Duplex", pcs->duplex);
        if (code < 0)
            return code;
        code = put_param1_bool(pcs, "FirstSide", !pcs->back_side);
        if (code < 0)
            return code;
        code = put_param1_bool(pcs, "BindShortEdge", pcs->bind_short_edge);
        if (code < 0)
            return code;
    }

    if (type & (pcl_reset_initial ^ pcl_reset_cold))
    {
        pjl_envvar_t *pres;

        pres = pjl_proc_get_envvar(pcs->pjls, "pdfmark");
        if (strlen(pres) > 0) {
            code = pcl_pjl_pdfmark(pcs->memory, pcs->pgs->device, pres);
            if (code < 0)
                return code;
        }

        pres = pjl_proc_get_envvar(pcs->pjls, "setdistillerparams");
        if (strlen(pres) > 0) {
            code = pcl_pjl_setdistillerparams(pcs->memory, pcs->pgs->device, pres);
            if (code < 0)
                return code;
        }
    }

    if (type & (pcl_reset_initial | pcl_reset_printer | pcl_reset_overlay)) {
        /* rtl always uses native units for user units.  The hp
           documentation does not say what to do if the resolution is
           assymetric... */
        pcl_args_t args;

        if (pcs->personality == rtl) {
            float res[2];

            /* resolution is always a single number in PJL */
            res[0] = res[1] = pcl_pjl_res(pcs);

            if (res[0] != 0)
                arg_set_uint(&args, (uint)res[0]);
            else
                arg_set_uint(&args,
                             (uint) gs_currentdevice(pcs->pgs)->
                             HWResolution[0]);
        } else
            arg_set_uint(&args, 300);
        code = pcl_set_unit_of_measure(&args, pcs);
        if (code < 0)
            return code;
    }
    return 0;
}
const pcl_init_t pcjob_init = {
    pcjob_do_registration, pcjob_do_reset, pcjob_do_copy
};
