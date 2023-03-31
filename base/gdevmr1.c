/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/

/* RasterOp implementation for monobit memory devices */
#include "memory_.h"
#include "gx.h"
#include "gsbittab.h"
#include "gserrors.h"
#include "gsropt.h"
#include "gxcindex.h"
#include "gxdcolor.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gdevmem.h"
#include "gdevmrop.h"

/* ---------------- Monobit RasterOp ---------------- */

/* The guts of this function have been moved to gdevm1.c as
 * mem_mono_strip_copy_rop_dev. This function takes care of the color
 * mapping and then passes control over to the subsidiary function that
 * does everything in device space.
 */
int
mem_mono_strip_copy_rop2(gx_device * dev, const byte * sdata,
                         int sourcex,uint sraster, gx_bitmap_id id,
                         const gx_color_index * scolors,
                         const gx_strip_bitmap * textures,
                         const gx_color_index * tcolors,
                         int x, int y, int width, int height,
                         int phase_x, int phase_y,
                         gs_logical_operation_t lop,
                         uint planar_height)
{
    gx_device_memory *mdev = (gx_device_memory *) dev;
    gs_rop3_t rop = lop_sanitize(lop);	/* handle transparency */
    bool invert;

    /* assert(planar_height == 0); */

    /* If map_rgb_color isn't the default one for monobit memory */
    /* devices, palette might not be set; set it now if needed. */
    if (mdev->palette.data == 0) {
        gx_color_value cv[3];
        cv[0] = cv[1] = cv[2] = 0;
        gdev_mem_mono_set_inverted(mdev,
                                   (*dev_proc(dev, encode_color)) (dev, cv) != 0);
    }
    invert = mdev->palette.data[0] != 0;

#ifdef DEBUG
    if (gs_debug_c('b'))
        trace_copy_rop("mem_mono_strip_copy_rop",
                       dev, sdata, sourcex, sraster,
                       id, scolors, textures, tcolors,
                       x, y, width, height, phase_x, phase_y, lop);
    if (gs_debug_c('B'))
        debug_dump_bitmap(mdev->memory, scan_line_base(mdev, y), mdev->raster,
                          height, "initial dest bits");
#endif

    /*
     * RasterOp is defined as operating in RGB space; in the monobit
     * case, this means black = 0, white = 1.  However, most monobit
     * devices use the opposite convention.  To make this work,
     * we must precondition the Boolean operation by swapping the
     * order of bits end-for-end and then inverting.
     */

    if (invert)
        rop = byte_reverse_bits[rop & 0xff] ^ 0xff;

    return mem_mono_strip_copy_rop2_dev(dev, sdata, sourcex, sraster, id,
                                        scolors, textures, tcolors, x, y,
                                        width, height, phase_x, phase_y,
                                        (gs_logical_operation_t)rop, 0);
}
