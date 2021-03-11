/* Copyright (C) 2001-2021 Artifex Software, Inc.
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


/* rtmisc.c - Miscellanous HP RTL commands */
/* the order of these includes are jumbled because of dependencies */
#include "math_.h"
#include "pgmand.h"
#include "pgdraw.h"             /* for hpgl_add_pcl_point_to_path() */
#include "pgmisc.h"             /* for hpgl_call */
#include "gsmemory.h"
#include "gsrop.h"
#include "gscoord.h"
#include "pcpatxfm.h"
#include "pcpage.h"
#include "pcdraw.h"
#include "rtmisc.h"

/* ---------------- Chapter 4 ---------------- */

/* Import the table of pointers to initialization data. */
extern const pcl_init_t *pcl_init_table[];

/* ---------------- Chapter 18 ---------------- */

/*
 * ESC % <enum> B
 */
int
rtl_enter_hpgl_mode(pcl_args_t * pargs, pcl_state_t * pcs)
{
    int i = int_arg(pargs);

    /* Note: -1..3 for PCL5c, 0..1 for PCL5 */
    if (i < 0)
        i = -1;
    else if (i > 3)
        return 0;
    pcs->hpgl_mode = i;
    hpgl_call_mem(pcs->memory, hpgl_clear_current_path(pcs));
    pcs->parse_other = (int (*)(void *,
                                pcl_state_t *,
                                stream_cursor_read *))hpgl_process;

    /* add the pcl cap to hpgl/2's path */
    if (i == 1) {
        gs_point pcl_pt;

        pcl_pt.x = (hpgl_real_t) pcs->cap.x;
        pcl_pt.y = (hpgl_real_t) pcs->cap.y;
        hpgl_call_mem(pcs->memory, hpgl_add_pcl_point_to_path(pcs, &pcl_pt));
        hpgl_call_mem(pcs->memory, hpgl_update_carriage_return_pos(pcs));
    }
    hpgl_call_mem(pcs->memory, hpgl_set_ctm(pcs));
    return 0;
}

/*
 * We export this so we can call it from HP-GL/2 configurations.
 * Note that it returns 1 iff it changed the PCL CAP.
 *
 * ESC % <enum> A
 */
int
rtl_enter_pcl_mode(pcl_args_t * pargs, pcl_state_t * pcs)
{
    int b = int_arg(pargs) & 1;

    if (pcs->parse_other ==
        (int (*)(void *, pcl_state_t *, stream_cursor_read *))hpgl_process) {
        /*
         * We were in HP-GL/2 mode.  Destroy the gl/2 polygon path
         * and conditionally copy back the cursor position.
         */
        if (b != 0) {
            /* the usual user -> device -> user dance. */
            gs_point pt, dev_pt;

            hpgl_call_mem(pcs->memory, hpgl_set_ctm(pcs));
            hpgl_call_mem(pcs->memory, hpgl_get_current_position(pcs, &pt));
            hpgl_call_mem(pcs->memory,
                          gs_transform(pcs->pgs, pt.x, pt.y, &dev_pt));
            hpgl_call_mem(pcs->memory, pcl_set_ctm(pcs, true));
            hpgl_call_mem(pcs->memory,
                          gs_itransform(pcs->pgs, dev_pt.x, dev_pt.y, &pt));

            /* HPGL/2 uses floats for coordinates */
#define round(x)    (((x) < 0.0) ? (ceil ((x) - 0.5)) : (floor ((x) + 0.5)))
            pcs->cap.x = (coord) round(pt.x);
            pcs->cap.y = (coord) round(pt.y);
#undef round
        }
    } else
        b = 0;

    pcs->parse_other = 0;
    return b;                   /* not 0, see comment above */
}

/* ---------------- Comparison Guide ---------------- */

/* (From PCL5 Comparison Guide, p. 1-30 & 1-92) */

/*
 * ESC & b <count> W
 */
static int
pcl_appletalk_configuration(pcl_args_t * pargs, pcl_state_t * pcs)
{
    const byte *data = arg_data(pargs);
    uint count = uint_arg(pargs);
    uint i;

#ifdef DEBUG
    if (gs_debug_c('i')) {
        pcl_debug_dump_data(pcs->memory, arg_data(pargs), uint_arg(pargs));
    }
#endif

    if ((count < 2) || (data[0] == ' '))
        return e_Range;

    /* split the string at the first space */
    for (i = 1; data[i] != ' '; ++i) {
        if (i == count - 1)
            return e_Range;
    }
    if (pcs->configure_appletalk == 0)
        return 0;
    return (*pcs->configure_appletalk) (data, i, data + i + 1,
                                        count - (i + 1));
}

/* (From PCL5 Comparison Guide, p. 1-100) */

/*
 * ESC & a <bool> N
 */
static int
pcl_negative_motion(pcl_args_t * pargs, pcl_state_t * pcs)
{
    int motion = int_arg(pargs);

    if (motion > 1)
        return e_Range;

    /* Currently we can't take any advantage of this.... */
    return 0;
}

/* ---------------- Initialization ---------------- */
static int
rtmisc_do_registration(pcl_parser_state_t * pcl_parser_state,
                       gs_memory_t * mem)
{
    /* Register commands */
    /* Chapter 4 */
    DEFINE_CLASS('%')
    /* Chapter 18 */
    {
        0, 'B',
            PCL_COMMAND("Enter HP-GL/2 Mode",
                        rtl_enter_hpgl_mode,
                        pca_neg_ok | pca_big_ok | pca_in_rtl)
    },
    {
        0, 'A',
            PCL_COMMAND("Enter PCL Mode",
                        rtl_enter_pcl_mode,
                        pca_neg_ok | pca_big_ok | pca_in_rtl)
    },
    END_CLASS

    /* Comparison Guide */
    DEFINE_CLASS('&')
    {
        'b', 'W',
        PCL_COMMAND("Appletalk Configuration",
        pcl_appletalk_configuration, pca_bytes)
    },
    {
        'a', 'N',
        PCL_COMMAND("Negative Motion",
        pcl_negative_motion, pca_neg_error | pca_big_error)
    },
    END_CLASS
    return 0;
}

/*
 * The default for printer name should be set by the output device, but
 * we will ignore that for now.
 *
 * Note that printer and device names are NOT reset by a pcl_reset_printer
 * (ESC E); if they were, AppleTalk network communication would be in
 * serious trouble. In fact, these parameters are normally stored in NVRAM,
 * so they should not even be reset for pcl_reset_initial. Hence, we have
 * consigned them to the currently unused pcl_reset_cold category
 *
 * The value of the device type field is as specified by the PCL 5 Comparison
 * Guide (October, 1996 ed.), but it is not clear this is the correct value
 * for a color device. This field is generally used to select a driver on
 * the host system, and it is not clear an "HP LaserJet 4" device will
 * generate color output (this may vary from host to host).
 */
static int
rtmisc_do_reset(pcl_state_t * pcs, pcl_reset_type_t type)
{
    static const uint mask = (pcl_reset_initial
                              | pcl_reset_cold | pcl_reset_printer);

    if (pcs->configure_appletalk == 0)
        return 0;

    if ((type & mask) != 0)
        pcs->configure_appletalk((const byte *)"JOB", 3, (const byte *)"", 0);
    if ((type & pcl_reset_cold) != 0) {
        static const byte prntr_name[] = "HP Color LaserJet 5M";
        static const byte dev_type[] = "HP LaserJet 4";

        pcs->configure_appletalk((const byte *)"RENAME",
                                 6, prntr_name, sizeof(prntr_name) - 1);
        pcs->configure_appletalk((const byte *)"TYPE", 4, dev_type,
                                 sizeof(dev_type) - 1);
    }
    return 0;
}

const pcl_init_t rtmisc_init = { rtmisc_do_registration, rtmisc_do_reset, 0 };
