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

/* Device to set monochrome mode in PCL */
/* This device is one of the 'subclassing' devices, part of a chain or pipeline
 * of devices, each of which can process some aspect of the graphics methods
 * before passing them on to the next device in the chain.
 * In this case, the device simply returns monochrome color_mapping procs
 * instead of color ones. When we want to go back to color, we just
 * remove this device.
 */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gxdevice.h"
#include "gsdevice.h"		/* requires gsmatrix.h */
#include "gxdcolor.h"		/* for gx_device_black/white */
#include "gxiparam.h"		/* for image source size */
#include "gxgstate.h"
#include "gxpaint.h"
#include "gxpath.h"
#include "gxcpath.h"
#include "gxcmap.h"         /* color mapping procs */
#include "gsstype.h"
#include "gdevprn.h"
#include "gdevp14.h"        /* Needed to patch up the procs after compositor creation */
#include "gdevsclass.h"
#include "gdevmplt.h"
#include "gxdcconv.h"       /* for color_rgb_to_gray and color_cmyk_to_gray */

/* Device procedures, we only need one */
static dev_proc_get_color_mapping_procs(pcl_mono_palette_get_color_mapping_procs);

/* The device prototype */
#define MAX_COORD (max_int_in_fixed - 1000)
#define MAX_RESOLUTION 4000

/* GC descriptor */
#define public_st_pcl_mono_palette_device()	/* in gsdevice.c */\
  gs_public_st_complex_only(st_pcl_mono_palette_device, gx_device, "PCL_Mono_Palette",\
    0, pcl_mono_palette_enum_ptrs, pcl_mono_palette_reloc_ptrs, default_subclass_finalize)

static
ENUM_PTRS_WITH(pcl_mono_palette_enum_ptrs, gx_device *dev);
return 0; /* default case */
case 0:ENUM_RETURN(gx_device_enum_ptr(dev->parent));
case 1:ENUM_RETURN(gx_device_enum_ptr(dev->child));
ENUM_PTRS_END
static RELOC_PTRS_WITH(pcl_mono_palette_reloc_ptrs, gx_device *dev)
{
    dev->parent = gx_device_reloc_ptr(dev->parent, gcst);
    dev->child = gx_device_reloc_ptr(dev->child, gcst);
}
RELOC_PTRS_END

public_st_pcl_mono_palette_device();

static int
pcl_mono_palette_initialize(gx_device *dev)
{
    int code = default_subclass_initialize(dev);

    if (code < 0)
        return code;

    set_dev_proc(dev, get_color_mapping_procs, pcl_mono_palette_get_color_mapping_procs);

    return 0;
}

const
gx_device_mplt gs_pcl_mono_palette_device =
{
    /*
     * Define the device as 8-bit gray scale to avoid computing halftones.
     */
    std_device_dci_type_body(gx_device_mplt, pcl_mono_palette_initialize,
                        "PCL_Mono_Palette", &st_pcl_mono_palette_device,
                        MAX_COORD, MAX_COORD,
                        MAX_RESOLUTION, MAX_RESOLUTION,
                        1, 8, 255, 0, 256, 1)
};

#undef MAX_COORD
#undef MAX_RESOLUTION

/* The justification for this device, these 3 procedures map colour values
 * to gray values
 */
static void
pcl_gray_cs_to_cm(gx_device * dev, frac gray, frac out[])
{
    pcl_mono_palette_subclass_data *psubclass_data;

    while(dev && dev->child) {
        if (strncmp(dev->dname, "PCL_Mono_Palette", 16) == 0)
            break;
        dev = dev->child;
    };

    if (dev && dev->child) {
        psubclass_data = dev->subclass_data;
        /* just pass it along */
        psubclass_data->device_cm_procs->map_gray(dev, gray, out);
    } else
        return;
}

static void
pcl_rgb_cs_to_cm(gx_device * dev, const gs_gstate * pgs, frac r, frac g,
                 frac b, frac out[])
{
    pcl_mono_palette_subclass_data *psubclass_data;
    frac gray;

    while(dev && dev->child) {
        if (strncmp(dev->dname, "PCL_Mono_Palette", 16) == 0)
            break;
        dev = dev->child;
    };

    if (dev && dev->child) {
        psubclass_data = dev->subclass_data;
        gray = color_rgb_to_gray(r, g, b, NULL);

        psubclass_data->device_cm_procs->map_rgb(dev, pgs, gray, gray, gray, out);
    } else
        return;
}

static void
pcl_cmyk_cs_to_cm(gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    pcl_mono_palette_subclass_data *psubclass_data;
    frac gray;

    while(dev && dev->child) {
        if (strncmp(dev->dname, "PCL_Mono_Palette", 16) == 0)
            break;
        dev = dev->child;
    };

    if (dev && dev->child) {
        psubclass_data = dev->subclass_data;
        gray = color_cmyk_to_gray(c, m, y, k, NULL);

        psubclass_data->device_cm_procs->map_cmyk(dev, gray, gray, gray, gray, out);
    } else
        return;
}

const gx_cm_color_map_procs *pcl_mono_palette_get_color_mapping_procs(const gx_device *dev)
{
    pcl_mono_palette_subclass_data *psubclass_data = dev->subclass_data;

    if (psubclass_data->device_cm_procs == 0L) {
        psubclass_data->pcl_mono_procs.map_gray = pcl_gray_cs_to_cm;
        psubclass_data->pcl_mono_procs.map_rgb = pcl_rgb_cs_to_cm;
        psubclass_data->pcl_mono_procs.map_cmyk = pcl_cmyk_cs_to_cm;
        psubclass_data->device_cm_procs = (gx_cm_color_map_procs *)dev_proc(dev->child, get_color_mapping_procs) (dev->child);
    }
    return &psubclass_data->pcl_mono_procs;
}
