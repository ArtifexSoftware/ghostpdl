/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* rtmisc.c - Miscellanous HP RTL commands */
/* the order of these includes are jumbled because of dependencies */
#include "math_.h"
#include "pgmand.h"
#include "pgdraw.h" /* for hpgl_add_pcl_point_to_path() */
#include "pgmisc.h" /* for hpgl_call */
#include "gsmemory.h"
#include "gsrop.h" 
#include "gscoord.h"
#include "pcpatxfm.h"
#include "pcpage.h"
#include "pcdraw.h"

/* ---------------- Chapter 4 ---------------- */

/* Import the table of pointers to initialization data. */
extern  const pcl_init_t *  pcl_init_table[];

/*
 * ESC E
 */
  private int
rtl_printer_reset(
    pcl_args_t *    pargs,
    pcl_state_t *   pcls
)
{
    /* Print any partial page. */
    int     code = pcl_end_page_if_marked(pcls);

    if (code < 0)
	return code;

    /* Reset to user default state. */
    return pcl_do_resets(pcls, pcl_reset_printer);
}

/*
 * ESC % -12345 X
 */
  private int
rtl_exit_language(
    pcl_args_t *    pargs,
    pcl_state_t *   pcls
)
{
    int             code;

    if (int_arg(pargs) != -12345)
	return e_Range;
    code = rtl_printer_reset(pargs, pcls);
    return (code < 0 ? code : e_ExitLanguage);
}

/* ---------------- Chapter 18 ---------------- */

/*
 * ESC % <enum> B
 */ 
  private int
rtl_enter_hpgl_mode(
    pcl_args_t *    pargs,
    pcl_state_t *   pcls
)
{
    int             i = int_arg(pargs);

    /* Note: -1..3 for PCL5c, 0..1 for PCL5 */
    if (i < 0)
	i = -1;
    else if (i > 3)
	return 0;

    /**** PARTIAL IMPLEMENTATION ****/
    if (pcls->parse_data != 0)
	return 0;		/* already in HP-GL/2 mode */

    pcls->parse_data = gs_alloc_bytes( pcls->memory,
                                       sizeof(hpgl_parser_state_t),
			               "hpgl parser data(enter hpgl mode)"
                                       );
    if (pcls->parse_data == 0)
	return_error(e_Memory);
    hpgl_process_init(pcls->parse_data);
    pcls->parse_other = ( int (*)( void *,
                                   pcl_state_t *,
                                   stream_cursor_read *
                                   ) ) hpgl_process;

    /* add the pcl cap to hpgl/2's path */
    if (i == 1) {
	gs_point    pcl_pt;

	pcl_pt.x = (hpgl_real_t)pcl_cap.x;
	pcl_pt.y = (hpgl_real_t)pcl_cap.y;
        hpgl_add_pcl_point_to_path(pcls, &pcl_pt);
	hpgl_update_carriage_return_pos(pcls);
    }
    return 0;
}

/*
 * We export this so we can call it from HP-GL/2 configurations.
 * Note that it returns 1 iff it changed the PCL CAP.
 *
 * ESC % <enum> A
 */ 
  int
rtl_enter_pcl_mode(
    pcl_args_t *    pargs,
    pcl_state_t *   pcls
)
{
    int             b = int_arg(pargs) & 1;

    if (pcls->parse_data != 0) {
        /* 
         * We were in HP-GL/2 mode.  Destroy the gl/2 polygon path
	 * and conditionally copy back the cursor position.
         */
	if (b != 0) {
            /* the usual user -> device -> user dance. */
	    gs_point    pt, dev_pt;

	    hpgl_call(hpgl_set_ctm(pcls));
	    hpgl_call(hpgl_get_current_position(pcls, &pt));
	    hpgl_call(gs_transform(pcls->pgs, pt.x, pt.y, &dev_pt));
	    hpgl_call(pcl_set_ctm(pcls, false));
	    hpgl_call(gs_itransform(pcls->pgs, dev_pt.x, dev_pt.y, &pt));

	    /* HPGL/2 uses floats for coordinates */
#define round(x)    (((x) < 0.0) ? (ceil ((x) - 0.5)) : (floor ((x) + 0.5)))
	    pcl_cap.x = round(pt.x);
	    pcl_cap.y = round(pt.y);
#undef round
	}
        gs_free_object( pcls->memory,
                        pcls->parse_data,
			"hpgl parser data(enter pcl mode)"
                        );
        pcls->parse_data = 0;

    } else
	  b = 0;

    pcls->parse_other = 0;
    return b;		/* not 0, see comment above */
}

/* ---------------- Comparison Guide ---------------- */

/* (From PCL5 Comparison Guide, p. 1-30 & 1-92) */

/*
 * ESC & b <count> W
 */
  private int
pcl_appletalk_configuration(
    pcl_args_t *    pargs,
    pcl_state_t *   pcls
)
{
    const byte *    data = arg_data(pargs);
    uint            count = uint_arg(pargs);
    uint            i;

    if ((count < 2) || (data[0] == ' '))
	return e_Range;

    /* split the string at the first space */
    for (i = 1; data[i] != ' '; ++i) {
	if (i == count - 1)
	    return e_Range;
    }
    if (pcls->configure_appletalk == 0)
	return 0;
    return (*pcls->configure_appletalk)(data, i, data + i + 1, count - (i + 1));
}

/* (From PCL5 Comparison Guide, p. 1-100) */

/*
 * ESC & a <bool> N
 */
  private int
pcl_negative_motion(
    pcl_args_t *    pargs,
    pcl_state_t *   pcls
)
{
    int             motion = int_arg(pargs);

    if (motion > 1)
	return e_Range;

    /* Currently we can't take any advantage of this.... */
    return 0;
}

/* ---------------- Initialization ---------------- */
  private int
rtmisc_do_init(
    gs_memory_t *   mem
)
{
    /* Register commands */
    /* Chapter 4 */
    DEFINE_ESCAPE('E', "Printer Reset", rtl_printer_reset)
    DEFINE_CLASS('%')
    {
        0, 'X',
	 PCL_COMMAND( "Exit Language",
                      rtl_exit_language,
		      pca_neg_ok | pca_big_error
                      )
    },

    /* Chapter 18 */
    {
        0, 'B',
        PCL_COMMAND( "Enter HP-GL/2 Mode",
                     rtl_enter_hpgl_mode,
		     pca_neg_ok | pca_big_ok | pca_in_macro
                     )
    },
    {
        0, 'A',
	PCL_COMMAND( "Enter PCL Mode",
                     rtl_enter_pcl_mode,
		     pca_neg_ok | pca_big_ok | pca_in_macro
                     )
    },
    END_CLASS

    /* Comparison Guide */
    DEFINE_CLASS('&')
    {
        'b', 'W',
	PCL_COMMAND( "Appletalk Configuration",
		     pcl_appletalk_configuration,
		     pca_bytes
                     )
    },
    {
        'a', 'N',
	PCL_COMMAND( "Negative Motion",
                     pcl_negative_motion,
		     pca_neg_error | pca_big_error
                     )
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
  private void
rtmisc_do_reset(
    pcl_state_t *       pcls,
    pcl_reset_type_t    type
)
{
    static const uint   mask = (  pcl_reset_initial
                                | pcl_reset_cold
                                | pcl_reset_printer );

    if (pcls->configure_appletalk == 0)
        return;

    if ((type & mask) != 0)
        pcls->configure_appletalk("JOB", 3, "", 0);
    if ((type & pcl_reset_cold) != 0) {
        static const byte   prntr_name[] = "HP Color LaserJet 5M";
        static const byte   dev_type[] = "HP LaserJet 4";

        pcls->configure_appletalk( "RENAME",
                                   6,
                                   prntr_name,
                                   sizeof(prntr_name) - 1
                                   );
        pcls->configure_appletalk("TYPE", 4, dev_type, sizeof(dev_type) - 1);
    }
}

const pcl_init_t    rtmisc_init = { rtmisc_do_init, rtmisc_do_reset, 0 };
