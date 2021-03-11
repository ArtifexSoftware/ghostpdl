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
 *     4 - compression mode 4 (no compression blocks), param is size in bytes
 *     5 - compression mode 5 (adaptive), param is size in bytes
 *     6 - compression mode 5 (ccitt group 3 1d), param is size in bytes
 *     7 - compression mode 5 (ccitt group 3 2d), param is size in bytes
 *     8 - compression mode 5 (ccitt group 4 2d), param is size in bytes
 *     9 - compression mode 9 (modified delta row), param is size in bytes
 *
 * There is no separate format for repeated rows. The desired effect can be
 * achieve by create a buffer of type 3 with a size of 0 bytes.
 */
typedef enum
{
    NO_COMPRESS = 0,
    RUN_LEN_COMPRESS = 1,
    PACKBITS_COMPRESS = 2,
    DELTA_ROW_COMPRESS = 3,
    NO_COMPRESS_BLOCK = 4,
    ADAPTIVE_COMPRESS = 5,
    CCITT_GR3_1D_COMPRESS = 6,
    CCITT_GR3_2D_COMPRESS = 7,
    CCITT_GR4_COMPRESS = 8,
    MOD_DELTA_ROW_COMPRESS = 9
} pcl_rast_buff_type_t;

/*
 * Identify the compression schemes that are block (not scan line
 * oriented)
 */

#define PCL_BLOCK_COMP(comp) ((comp) >= NO_COMPRESS_BLOCK && (comp) <= CCITT_GR4_COMPRESS)

/*
 * A seed-row structure. These buffers are used both to pass data to the
 * graphic library image routines, and to retain information on the last row
 * sent to support "delta-row" compression.
 *
 * The is_blank flag is intended as a hint, not as an absolute indication. If
 * it is set, the seed row is known to be blank. However, even if it is not
 * set the seed row may still be blank.
 */
typedef struct pcl_seed_row_s
{
    ushort size;
    bool is_blank;
    byte *pdata;
} pcl_seed_row_t;

/*
 * The array of decompression functions.
 */
extern void (*const pcl_decomp_proc[9 + 1]) (pcl_seed_row_t * pout,
                                             const byte * pin, int in_size);

#endif /* rtrstcmp_INCLUDED */
