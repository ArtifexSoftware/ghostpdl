/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pgcolor.c - HP-GL/2 color vector graphics commands */

#include "std.h"
#include "pgmand.h"
#include "pginit.h"
#include "pgmisc.h"
#include "pgdraw.h"
#include "gsstate.h"		/* for gs_setfilladjust */
#include "pcpalet.h"

/* ------ Commands ------ */

/*
 * PC [pen[,primary1,primary2,primary3];
 */
  private int
hpgl_PC(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{
    int32           pen;
    int32           npen = pcl_palette_get_num_entries(pgls->ppalet);

    /* output any current path */
    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));

    if (hpgl_arg_int(pargs, &pen)) {
        hpgl_real_t primary[3];

        if ((pen < 0) || (pen >= npen))
	    return e_Range;

	if (hpgl_arg_c_real(pargs, &primary[0])) {
            float       comps[3];

	    if ( !hpgl_arg_c_real(pargs, &primary[1]) ||
		 !hpgl_arg_c_real(pargs, &primary[2])   )
		return e_Range;
            comps[0] = primary[0];
            comps[1] = primary[1];
            comps[2] = primary[2];
            return pcl_palette_set_color(pgls, pen, comps);
        } else
	    return pcl_palette_set_default_color(pgls, pen);
    } else {
        int     i;
        int     code;

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
hpgl_NP(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{
    int32           n = 8;

    /* output any current path */
    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));

    if ( hpgl_arg_int(pargs, &n) && ((n < 2) || (n > 32768)) )
	return e_Range;
    return pcl_palette_NP(pgls, n);
}

/*
 * CR [b_red, w_red, b_green, w_green, b_blue, w_blue];
 */
  private int
hpgl_CR(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{
    hpgl_real_t     range[6];
    int             i;

    /* output any current path */
    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));

    range[0] = range[2] = range[4] = 0;
    range[1] = range[3] = range[5] = 255;
    for (i = 0; (i < 6) && hpgl_arg_c_real(pargs, &range[i]); ++i)
	;
    if ( (range[0] == range[1]) ||
         (range[2] == range[3]) ||
         (range[4] == range[5])   )
	return e_Range;
    return pcl_palette_CR( pgls,
                           (floatp)range[1],
                           (floatp)range[3],
                           (floatp)range[5],
                           (floatp)range[0],
                           (floatp)range[2],
                           (floatp)range[4]
                           );
}

/*
 * Initialization. There is no reset or copy command, as those operations are
 * carried out by the palette mechanism.
 */
  private int
pgcolor_do_init(
    gs_memory_t *   mem
)
{
    /* Register commands */
    DEFINE_HPGL_COMMANDS
    HPGL_COMMAND('C', 'R', hpgl_CR, 0),
    HPGL_COMMAND('N', 'P', hpgl_NP, 0),
    HPGL_COMMAND('P', 'C', hpgl_PC, 0),
    END_HPGL_COMMANDS
    return 0;
}

const pcl_init_t    pgcolor_init = { pgcolor_do_init, 0, 0 };

