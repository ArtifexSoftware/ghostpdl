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

/* rtrstcmp.h - interface to the raster decompression code */

#ifndef rtrstcmp_INCLUDED
#define rtrstcmp_INCLUDED

#include "gx.h"
#include "gsstruct.h"

/*
 * Types:
 *
 *     0 - compression mode 0 (no compression), param is size in bytes
 *     1 - compression mode 1 (run length compression), param is size in bytes
 *     2 - compression mode 2 ("Packbits" compression), param is size in bytes
 *     3 - compression mode 3 (delta row compression), param is size in bytes
 *     4 - not used
 *     5 - compression mode 5 (adaptive), param is size in bytes
 *     9 - compression mode 9 (modified delta row), param is size in bytes
 *
 * There is no separate format for repeated rows. The desired effect can be
 * achieve by create a buffer of type 3 with a size of 0 bytes.
 */
typedef enum {
    NO_COMPRESS = 0,
    RUN_LEN_COMPRESS = 1,
    PACKBITS_COMPRESS = 2,
    DELTA_ROW_COMPRESS = 3,
    /* 4 is not used, and indicated as reserved by HP */
    ADAPTIVE_COMPRESS = 5,
    /* 6 - 8 unused */
    MOD_DELTA_ROW_COMPRESS = 9
} pcl_rast_buff_type_t;

/*
 * A seed-row structure. These buffers are used both to pass data to the
 * graphic library image routines, and to retain information on the last row
 * sent to support "delta-row" compression.
 *
 * The is_blank flag is intended as a hint, not as an absolute indication. If
 * it is set, the seed row is known to be blank. However, even if it is not
 * set the seed row may still be blank.
 */
typedef struct  pcl_seed_row_s {
    ushort  size;
    bool    is_blank;
    byte *  pdata;
} pcl_seed_row_t;

/* in rtraster.c */
#define private_st_seed_row_t()                 \
    gs_private_st_ptrs1( st_seed_row_t,         \
                         pcl_seed_row_t,        \
                         "PCL raster seed row", \
                         seed_row_enum_ptrs,    \
                         seed_row_reloc_ptrs,   \
                         pdata                  \
                         )

#define private_st_seed_row_t_element()                 \
    gs_private_st_element( st_seed_row_t_element,       \
                           pcl_seed_row_t,              \
                           "PCL seed row array",        \
                           seed_row_element_enum_ptrs,  \
                           seed_row_element_reloc_ptrs, \
                           st_seed_row_t                \
                           )

/*
 * The array of decompression functions.
 */
extern  void    (*const pcl_decomp_proc[9 + 1])( pcl_seed_row_t *   pout,
                                                 const byte *       pin,
                                                 int                in_size 
                                                 );

#endif			/* rtrstcmp_INCLUDED */
