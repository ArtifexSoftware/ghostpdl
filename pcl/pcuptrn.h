/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pcuptrn.h - interface for PCL and GL/2 user defined patterns */

#ifndef pcuptrn_INCLUDED
#define pcuptrn_INCLUDED

#include "gx.h"
#include "pcommand.h"
#include "pcpatrn.h"

/*
 * Free routine for patterns. This is exported for the benefit of the code
 * that handles PCL built-in patterns.
 */
void pcl_pattern_free_pattern(P3(
    gs_memory_t *   pmem,
    void *          pvptrn,
    client_name_t   cname
));

/*
 * Build a PCL pattern. This is exported for use by the routines supporting
 * built-in patterns.
 */
int pcl_pattern_build_pattern(P6(
    pcl_pattern_t **        pppat_data,
    const gs_depth_bitmap * ppixmap,
    pcl_pattern_type_t      type,
    int                     xres,
    int                     yres,
    gs_memory_t *           pmem
));

/*
 * Get a PCL user-defined pattern. A null return indicates the pattern is
 * not defined.
 */
pcl_pattern_t * pcl_pattern_get_pcl_uptrn(P2(pcl_state_t *pcs, int id));

/*
 * Get a GL/2 user defined pattern. A null return indicates there is no pattern
 * defined for the index.
 */
extern pcl_pattern_t * pcl_pattern_get_gl_uptrn(P2(pcl_state_t *pcs, int indx));

/* pcl_pattern_RF is in pcpatrn.h */

/*
 * External access to the user defined pattern related operators.
 */
extern  const pcl_init_t    pcl_upattern_init;

#endif  	/* pcuptrn_INCLUDED */
