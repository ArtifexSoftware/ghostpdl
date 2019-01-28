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


#ifndef pccid_INCLUDED
#define  pccid_INCLUDED

#include "gx.h"
#include "gsstruct.h"
#include "pcommand.h"

typedef short int16;

typedef unsigned short uint16;

/*
 * Enumeration of the 5c color spaces.
 *
 * Note that there is no separate monochrome color space; the monochrome
 * simple color space is simply an RGB color space with a two-entry palette
 * (1-bit/index) containing white (0) and black (1).
 *
 * These values are defined by HP.
 */
typedef enum
{
    pcl_cspace_White = -1,
    pcl_cspace_RGB = 0,
    pcl_cspace_CMY = 1,
    pcl_cspace_Colorimetric = 2,
    pcl_cspace_CIELab = 3,
    pcl_cspace_LumChrom = 4,
    pcl_cspace_num
} pcl_cspace_type_t;

/*
 * The pixel encoding enumeration.
 *
 * These values are defined by HP.
 */
typedef enum
{
    pcl_penc_indexed_by_plane = 0,
    pcl_penc_indexed_by_pixel = 1,
    pcl_penc_direct_by_plane = 2,
    pcl_penc_direct_by_pixel = 3,
    pcl_penc_num
} pcl_encoding_type_t;

/*
 * The short form and various long-form configure image data structures.
 * Though the long form structures are similar to those defined by HP,
 * only pcl_cid_shortform is the same (the others must be different to
 * support alignment-sensitive processors).
 *
 * A GC memory descriptor is provided for pcl_cid_data_t object, though
 * it should seldom be used: the configure image data information is needed
 * only fleetingly while an indexed color space is created. Beyond that only
 * the header (short form) is needed, and this is so small it can be copied
 * into any objects that require it.
 *
 * NB: For the long form luminance-chrominance case, rows and columns of
 *     of the RGB ==> LumChrom transformation matrix must be transposed,
 *     to go from the column-vector view of HP to row-vector view used
 *     by the pc_mtx3_t data structure and the graphics library. The column
 *     vector form is stored in the matrix field of the pcl_cid_lum_chrom_t
 *     structure.
 */
#define pcl_cid_hdr_common                                      \
    byte    cspace;         /* actually pcl_cspace_type_t */    \
    byte    encoding;       /* actually pcl_encoding_type_t */  \
    byte    bits_per_index;                                     \
    byte    bits_per_primary[3]

typedef struct pcl_cid_hdr_s
{
    pcl_cid_hdr_common;
} pcl_cid_hdr_t;

typedef struct pcl_cid_dev_long_s
{
    pcl_cid_hdr_common;
    int16 white_ref[3];
    int16 black_ref[3];
} pcl_cid_dev_long_t;

typedef struct pcl_cid_minmax_s
{
    struct
    {
        float min_val, max_val;
    } val_range[3];
} pcl_cid_minmax_t;

typedef struct pcl_cid_col_common_s
{
    struct
    {
        float x, y;
    } chroma[4];
    struct
    {
        float gamma, gain;
    } nonlin[3];
} pcl_cid_col_common_t;

typedef struct pcl_cid_col_long_s
{
    pcl_cid_hdr_common;
    pcl_cid_col_common_t colmet;
    pcl_cid_minmax_t minmax;
} pcl_cid_col_long_t;

typedef struct pcl_cid_Lab_long_s
{
    pcl_cid_hdr_common;
    pcl_cid_minmax_t minmax;
} pcl_cid_Lab_long_t;

typedef struct pcl_cid_lum_long_s
{
    pcl_cid_hdr_common;
    float matrix[9];
    pcl_cid_minmax_t minmax;
    pcl_cid_col_common_t colmet;
} pcl_cid_lum_long_t;

/*
 * The unified PCL configure image data structure.
 */
typedef struct pcl_cid_data_s
{
    uint16 len;
    byte original_cspace;       /* the original color space if a substitution has occurred */
    union
    {
        pcl_cid_hdr_t hdr;
        pcl_cid_dev_long_t dev;
        pcl_cid_col_long_t col;
        pcl_cid_Lab_long_t lab;
        pcl_cid_lum_long_t lum;
    } u;
} pcl_cid_data_t;

/*
 * Functions for getting configuration parameters from the first six bytes of
 * the configure image data structure.
 */
pcl_cspace_type_t pcl_cid_get_cspace(const pcl_cid_data_t * pcid);

pcl_encoding_type_t pcl_cid_get_encoding(const pcl_cid_data_t * pcid);

byte pcl_cid_get_bits_per_index(const pcl_cid_data_t * pcid);

byte pcl_cid_get_bits_per_primary(const pcl_cid_data_t * pcid, int index);

/*
 * Implement the GL/2 IN command. This is probably better done via a reset flag,
 * but currently there are no reset flags that propagate out of GL/2.
 */
int pcl_cid_IN(pcl_state_t * pcs);

/*
 * Special color space for CCITT raster.
 */
int pcl_cid_CCITT_raster(pcl_state_t * pcs);

/*
 * Entry point for the configure image data code.
 */
extern const pcl_init_t pcl_cid_init;

/*
 * Debugging routines.
 */

#ifdef DEBUG
const char *pcl_cid_cspace_get_debug_name(const gs_memory_t * mem, int index);

const char *pcl_cid_enc_get_debug_name(const gs_memory_t * mem, int index);
#endif

#endif /* pccid_INCLUDED */
