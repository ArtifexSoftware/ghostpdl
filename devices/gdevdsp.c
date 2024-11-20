/* Copyright (C) 2001-2024 Artifex Software, Inc.
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

/* gdevdsp.c */

/*
 * DLL based display device driver.
 *
 * by Russell Lang, Ghostgum Software Pty Ltd
 *
 * This device is intended to be used for displays when
 * Ghostscript is loaded as a DLL/shared library/static library.
 * It is intended to work for Windows, OS/2, Linux, Mac OS 9 and
 * hopefully others.
 *
 * Before this device is opened, the address of a structure must
 * be provided using gsapi_set_display_callback(minst, callback);
 * This structure contains callback functions to notify the
 * caller when the device is opened, closed, resized, showpage etc.
 * The structure is defined in gdevdsp.h.
 *
 * Not all combinations of display formats have been tested.
 * At the end of this file is some example code showing which
 * formats have been tested.
 */

#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsdevice.h"		/* for gs_copydevice */
#include "gxdevice.h"

#include "gp.h"
#include "gpcheck.h"
#include "gsparam.h"

#include "gdevpccm.h"		/* 4-bit PC color */
#include "gxdevmem.h"
#include "gdevdevn.h"
#include "gxpcolor.h"		/* for gx_dc_devn_masked */
#include "gxdevsop.h"
#include "gsequivc.h"
#include "gdevdsp.h"
#include "gdevdsp2.h"
#include "gxclist.h"
#include "gxdevbuf.h"
#include "gxgetbit.h"
#include "gdevmpla.h"
#include "gdevprn.h"           /* For gdev_create_buf_device */
#include "gsicc_manage.h"

#include "gdevkrnlsclass.h" /* 'standard' built in subclasses, currently First/Last Page and obejct filter */

/* Initial values for width and height */
#define INITIAL_RESOLUTION 96
#define INITIAL_WIDTH ((INITIAL_RESOLUTION * 85 + 5) / 10)
#define INITIAL_HEIGHT ((INITIAL_RESOLUTION * 110 + 5) / 10)

/* Device procedures */

/* See gxdevice.h for the definitions of the procedures. */
static dev_proc_open_device(display_open);
static dev_proc_get_initial_matrix(display_get_initial_matrix);
static dev_proc_sync_output(display_sync_output);
static dev_proc_output_page(display_output_page);
static dev_proc_close_device(display_close);

static dev_proc_map_rgb_color(display_map_rgb_color_device4);
static dev_proc_map_color_rgb(display_map_color_rgb_device4);
static dev_proc_encode_color(display_encode_color_device8);
static dev_proc_decode_color(display_decode_color_device8);
static dev_proc_map_rgb_color(display_map_rgb_color_device16);
static dev_proc_map_color_rgb(display_map_color_rgb_device16);
static dev_proc_map_rgb_color(display_map_rgb_color_rgb);
static dev_proc_map_color_rgb(display_map_color_rgb_rgb);
static dev_proc_map_rgb_color(display_map_rgb_color_bgr24);
static dev_proc_map_color_rgb(display_map_color_rgb_bgr24);

static dev_proc_fill_rectangle(display_fill_rectangle);
static dev_proc_copy_mono(display_copy_mono);
static dev_proc_copy_color(display_copy_color);
static dev_proc_get_bits_rectangle(display_get_bits_rectangle);
static dev_proc_get_params(display_get_params);
static dev_proc_put_params(display_put_params);
static dev_proc_initialize_device_procs(display_initialize_device_procs);

static dev_proc_get_color_mapping_procs(display_separation_get_color_mapping_procs);
static dev_proc_get_color_comp_index(display_separation_get_color_comp_index);
static dev_proc_encode_color(display_separation_encode_color);
static dev_proc_decode_color(display_separation_decode_color);
static dev_proc_update_spot_equivalent_colors(display_update_spot_equivalent_colors);
static dev_proc_ret_devn_params(display_ret_devn_params);
static dev_proc_dev_spec_op(display_spec_op);
static dev_proc_fill_rectangle_hl_color(display_fill_rectangle_hl_color);

extern dev_proc_open_device(clist_open);
extern dev_proc_close_device(clist_close);

/* GC descriptor */
public_st_device_display();

static
ENUM_PTRS_WITH(display_enum_ptrs, gx_device_display *ddev)
    if (index < ddev->devn_params.separations.num_separations)
        ENUM_RETURN(ddev->devn_params.separations.names[index].data);
    else
        ENUM_PREFIX(st_device_clist_mutatable, ddev->devn_params.separations.num_separations);
    return 0;
ENUM_PTRS_END

static
RELOC_PTRS_WITH(display_reloc_ptrs, gx_device_display *ddev)
    RELOC_PREFIX(st_device_clist_mutatable);
    {   int i;
        for (i = 0; i < ddev->devn_params.separations.num_separations; ++i) {
            RELOC_PTR(gx_device_display, devn_params.separations.names[i].data);
        }
    }
RELOC_PTRS_END

const gx_device_display gs_display_device =
{
    std_device_std_body_type(gx_device_display,
                             display_initialize_device_procs,
                             "display",
                             &st_device_display,
                             INITIAL_WIDTH, INITIAL_HEIGHT,
                             INITIAL_RESOLUTION, INITIAL_RESOLUTION),
    {0},			/* std_procs */
    GX_CLIST_MUTATABLE_DEVICE_DEFAULTS,
    NULL,			/* callback */
    NULL,			/* pHandle */
    0,                          /* pHandle_set */
    0,				/* nFormat */
    NULL,			/* pBitmap */
    0, 				/* zBitmapSize */
    0, 				/* HWResolution_set */

    {    /* devn_params specific parameters */
      8,        /* Bits per color - must match ncomp, depth, etc. */
      DeviceCMYKComponents,     /* Names of color model colorants */
      4,                        /* Number of colorants for CMYK */
      0,                        /* MaxSeparations has not been specified */
      -1,			/* PageSpotColors has not been specified */
      {0},                      /* SeparationNames */
      0,                        /* Number of SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 } /* Initial component SeparationOrder */
    },
    { true }                   /* equivalent CMYK colors for spot colors */
};

/* prototypes for internal procedures */
static int display_check_structure(gx_device_display *dev);
static void display_free_bitmap(gx_device_display * dev);
static int display_alloc_bitmap(gx_device_display *, gx_device *);
static int display_set_color_format(gx_device_display *dev, int nFormat);
static int display_set_separations(gx_device_display *dev);
static int display_raster(gx_device_display *dev);

/* Open the display driver. */
static int
display_open(gx_device * dev)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    int ccode;
    gs_display_get_callback_t data;

    /* Erase these, in case we are opening a copied device. */
    ddev->pBitmap = NULL;
    ddev->zBitmapSize = 0;

    ddev->orig_procs = ddev->procs;

    /* Fetch our callback procedures. */
    data.callback = NULL;
    data.caller_handle = NULL;
    ccode = gx_callout(dev, DISPLAY_CALLOUT_GET_CALLBACK, sizeof(data), &data);
    if (ccode < 0) {
        ccode = gx_callout(dev, DISPLAY_CALLOUT_GET_CALLBACK_LEGACY, sizeof(data), &data);
        if (ccode < 0) {
            ddev->callback = NULL;
            ddev->pHandle = NULL;
            if (ccode != gs_error_unknownerror)
                return ccode;
        } else {
            ddev->callback = data.callback;
            ddev->pHandle_set = 0;
        }
    } else {
        ddev->callback = data.callback;
        ddev->pHandle = data.caller_handle;
        ddev->pHandle_set = 1;
    }

    /* Allow device to be opened "disabled" without a callback. */
    /* The callback will be set later and the device re-opened. */
    if (ddev->callback == NULL)
    {
        fill_dev_proc(ddev, fill_rectangle, display_fill_rectangle);
        return 0;
    }
    ccode = install_internal_subclass_devices((gx_device **)&ddev, NULL);
    if (ccode < 0)
        return ccode;
    dev = (gx_device *)ddev;

    while(dev->parent)
        dev = dev->parent;

    /* Make sure we have been passed a valid callback structure. */
    if ((ccode = display_check_structure(ddev)) < 0)
        return_error(ccode);

    /* set color info */
    if ((ccode = display_set_color_format(ddev, ddev->nFormat)) < 0)
        return_error(ccode);

    /* Tell caller that the device is open. */
    /* This is always the first callback */
    ccode = (*(ddev->callback->display_open))(ddev->pHandle, dev);
    if (ccode < 0)
        return_error(ccode);

    /* Tell caller the proposed device parameters */
    ccode = (*(ddev->callback->display_presize)) (ddev->pHandle, dev,
        dev->width, dev->height, display_raster(ddev), ddev->nFormat);
    if (ccode < 0) {
        (*(ddev->callback->display_close))(ddev->pHandle, dev);
        return_error(ccode);
    }

    /* allocate the image */
    ccode = display_alloc_bitmap(ddev, dev);
    if (ccode < 0) {
        (*(ddev->callback->display_close))(ddev->pHandle, dev);
        return_error(ccode);
    }

    /* Tell caller the device parameters */
    ccode = (*(ddev->callback->display_size))(ddev->pHandle, dev,
                          dev->width, dev->height,
                          display_raster(ddev), ddev->nFormat,
                          CLIST_MUTATABLE_HAS_MUTATED(ddev) ?
                               NULL : ((gx_device_memory *)ddev)->base);
    if (ccode < 0) {
        display_free_bitmap(ddev);
        (*(ddev->callback->display_close))(ddev->pHandle, dev);
        return_error(ccode);
    }

    return 0;
}

static void
display_get_initial_matrix(gx_device * dev, gs_matrix * pmat)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    if ((ddev->nFormat & DISPLAY_FIRSTROW_MASK) == DISPLAY_TOPFIRST)
        gx_default_get_initial_matrix(dev, pmat);
    else
        gx_upright_get_initial_matrix(dev, pmat);  /* Windows / OS/2 */
}

/* Update the display. */
int
display_sync_output(gx_device * dev)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    if (ddev->callback == NULL)
        return 0;	/* ignore the call */
    display_set_separations(ddev);

    while(dev->parent)
        dev = dev->parent;

    (*(ddev->callback->display_sync))(ddev->pHandle, dev);
    return (0);
}

/* Update the display, bring to foreground. */
/* If you want to pause on showpage, delay your return from callback */
int
display_output_page(gx_device * dev, int copies, int flush)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    int code;
    int is_planar = (ddev->nFormat & (DISPLAY_PLANAR |
                                     DISPLAY_PLANAR_INTERLEAVED)) &&
                    (ddev->color_info.num_components > 1);

    if (ddev->callback == NULL)
        return gs_error_Fatal;
    display_set_separations(ddev);

    while(dev->parent)
        dev = dev->parent;

    if (CLIST_MUTATABLE_HAS_MUTATED(ddev)) {
        /* Rectangle request mode! */
        gs_get_bits_options_t options;

        options = GB_RETURN_COPY | GB_ALIGN_STANDARD |
                  GB_OFFSET_SPECIFIED | GB_RASTER_SPECIFIED |
                  GB_COLORS_NATIVE;
        switch (ddev->nFormat & DISPLAY_ALPHA_MASK) {
            default:
            case DISPLAY_ALPHA_NONE:
                break;
            case DISPLAY_ALPHA_FIRST:
            case DISPLAY_UNUSED_FIRST:
                options |=  GB_ALPHA_FIRST;
                break;
            case DISPLAY_ALPHA_LAST:
            case DISPLAY_UNUSED_LAST:
                options |=  GB_ALPHA_LAST;
                break;
        }
        if (is_planar)
            options |= GB_PACKING_PLANAR;
        else
            options |= GB_PACKING_CHUNKY;

        while (1) {
            void *mem = NULL;
            int ox, oy, x, y, w, h, i, raster, plane_raster;
            gs_int_rect rect;
            gs_get_bits_params_t params;

            code = ddev->callback->display_rectangle_request
                                                (ddev->pHandle, dev,
                                                 &mem, &ox, &oy,
                                                 &raster, &plane_raster,
                                                 &x, &y, &w, &h);
            if (w == 0 || h == 0)
                break;
            if (mem == NULL) {
                code = gs_note_error(gs_error_VMerror);
                break;
            }
            rect.p.x = x;
            rect.p.y = y;
            rect.q.x = x + w;
            rect.q.y = y + h;
            params.options = options;
            if (is_planar) {
                for (i = 0; i < ddev->color_info.num_components; i++)
                    params.data[i] = (byte *)mem + i * plane_raster;
            } else {
                params.data[0] = (byte *)mem;
            }
            params.x_offset = ox;
            params.original_y = oy;
            params.raster = raster;
            code = dev_proc(ddev, get_bits_rectangle)((gx_device *)ddev,
                                                      &rect, &params);
            if (code < 0)
                break;
        }
    } else {
        /* Full page mode. Just claim completion! */
        code = (*(ddev->callback->display_page))
                        (ddev->pHandle, dev, copies, flush);
    }

    if (code >= 0)
        code = gx_finish_output_page((gx_device *)ddev, copies, flush);
    return code;
}

/* Close the display driver */
static int
display_close(gx_device * dev)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    if (ddev->callback == NULL)
        return 0;	/* ignore the call since we were never properly opened */

    while(dev->parent)
        dev = dev->parent;

    /* Tell caller that device is about to be closed. */
    (*(ddev->callback->display_preclose))(ddev->pHandle, dev);

    /* Release memory. */
    display_free_bitmap(ddev);

    /* Tell caller that device is closed. */
    /* This is always the last callback */
    (*(ddev->callback->display_close))(ddev->pHandle, dev);

    /* Reset device proc vector to default */
    if (ddev->orig_procs.open_device != NULL)
        ddev->procs = ddev->orig_procs;
    ddev->orig_procs.open_device = NULL; /* prevent uninit'd restore of procs */

    return 0;
}

/*
 * This routine will encode a 1 Black on white color.
 */
static gx_color_index
gx_b_w_gray_encode(gx_device * dev, const gx_color_value cv[])
{
    return 1 - (cv[0] >> (gx_color_value_bits - 1));
}

/* DISPLAY_COLORS_NATIVE, 4bit/pixel */
/* Map a r-g-b color to a color code */
static gx_color_index
display_map_rgb_color_device4(gx_device * dev, const gx_color_value cv[])
{
    return pc_4bit_map_rgb_color(dev, cv);
}

/* Map a color code to r-g-b. */
static int
display_map_color_rgb_device4(gx_device * dev, gx_color_index color,
                 gx_color_value prgb[3])
{
    pc_4bit_map_color_rgb(dev, color, prgb);
    return 0;
}

/* DISPLAY_COLORS_NATIVE, 8bit/pixel */
/* Map a r-g-b-k color to a color code */
static gx_color_index
display_encode_color_device8(gx_device * dev, const gx_color_value cv[])
{
    /* palette of 96 colors */
    /* 0->63 = 00RRGGBB, 64->95 = 010YYYYY */
    gx_color_value r = cv[0];
    gx_color_value g = cv[1];
    gx_color_value b = cv[2];
    gx_color_value k = cv[3]; /* 0 = black */
    if ((r == 0) && (g == 0) && (b == 0)) {
        k = ((k >> (gx_color_value_bits - 6)) + 1) >> 1;
        if (k > 0x1f)
            k = 0x1f;
        return (k + 0x40);
    }
    if (k > 0) {
        /* The RGB->RGBK color mapping shouldn't generate this. */
        r = ((r+k) > gx_max_color_value) ? gx_max_color_value :
            (gx_color_value)(r+k);
        g = ((g+k) > gx_max_color_value) ? gx_max_color_value :
            (gx_color_value)(g+k);
        b = ((b+k) > gx_max_color_value) ? gx_max_color_value :
            (gx_color_value)(b+k);
    }
    r = ((r >> (gx_color_value_bits - 3)) + 1) >> 1;
    if (r > 0x3)
        r = 0x3;
    g = ((g >> (gx_color_value_bits - 3)) + 1) >> 1;
    if (g > 0x3)
        g = 0x3;
    b = ((b >> (gx_color_value_bits - 3)) + 1) >> 1;
    if (b > 0x3)
        b = 0x3;
    return (r << 4) + (g << 2) + b;
}

/* Map a color code to r-g-b-k. */
static int
display_decode_color_device8(gx_device * dev, gx_color_index color,
                 gx_color_value prgb[])
{
    gx_color_value one;
    /* palette of 96 colors */
    /* 0->63 = 00RRGGBB, 64->95 = 010YYYYY */
    if (color < 64) {
        one = (gx_color_value) (gx_max_color_value / 3);
        prgb[0] = (gx_color_value) (((color >> 4) & 3) * one);
        prgb[1] = (gx_color_value) (((color >> 2) & 3) * one);
        prgb[2] = (gx_color_value) (((color) & 3) * one);
        prgb[3] = 0;
    }
    else if (color < 96) {
        one = (gx_color_value) (gx_max_color_value / 31);
        prgb[0] = prgb[1] = prgb[2] = 0;
        prgb[3] = (gx_color_value) ((color & 0x1f) * one);
    }
    else {
        prgb[0] = prgb[1] = prgb[2] = prgb[3] = 0;
    }
    return 0;
}

/* DISPLAY_COLORS_NATIVE, 16bit/pixel */
/* Map a r-g-b color to a color code */
static gx_color_index
display_map_rgb_color_device16(gx_device * dev, const gx_color_value cv[])
{
    gx_device_display *ddev = (gx_device_display *) dev;
    gx_color_value r = cv[0];
    gx_color_value g = cv[1];
    gx_color_value b = cv[2];
    /* FIXME: Simple truncation isn't ideal. Should round really. */
    if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN) {
        if ((ddev->nFormat & DISPLAY_555_MASK) == DISPLAY_NATIVE_555)
            /* byte0=0RRRRRGG byte1=GGGBBBBB */
            return ((r >> (gx_color_value_bits - 5)) << 10) +
                ((g >> (gx_color_value_bits - 5)) << 5) +
                (b >> (gx_color_value_bits - 5));
        else
            /* byte0=RRRRRGGG byte1=GGGBBBBB */
            return ((r >> (gx_color_value_bits - 5)) << 11) +
                ((g >> (gx_color_value_bits - 6)) << 5) +
                (b >> (gx_color_value_bits - 5));
    }

    if ((ddev->nFormat & DISPLAY_555_MASK) == DISPLAY_NATIVE_555)
        /* byte0=GGGBBBBB byte1=0RRRRRGG */
        return ((r >> (gx_color_value_bits - 5)) << 2) +
            (((g >> (gx_color_value_bits - 5)) & 0x7) << 13) +
            (((g >> (gx_color_value_bits - 5)) & 0x18) >> 3) +
            ((b >> (gx_color_value_bits - 5)) << 8);

    /* byte0=GGGBBBBB byte1=RRRRRGGG */
    return ((r >> (gx_color_value_bits - 5)) << 3) +
        (((g >> (gx_color_value_bits - 6)) & 0x7) << 13) +
        (((g >> (gx_color_value_bits - 6)) & 0x38) >> 3) +
        ((b >> (gx_color_value_bits - 5)) << 8);
}

/* Map a color code to r-g-b. */
static int
display_map_color_rgb_device16(gx_device * dev, gx_color_index color,
                 gx_color_value prgb[3])
{
    gx_device_display *ddev = (gx_device_display *) dev;
    ushort value;

    if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN) {
        if ((ddev->nFormat & DISPLAY_555_MASK) == DISPLAY_NATIVE_555) {
            /* byte0=0RRRRRGG byte1=GGGBBBBB */
            value = (ushort) (color >> 10);
            prgb[0] = (gx_color_value)
                (((value << 11) + (value << 6) + (value << 1) +
                (value >> 4)) >> (16 - gx_color_value_bits));
            value = (ushort) ((color >> 5) & 0x1f);
            prgb[1] = (gx_color_value)
                (((value << 11) + (value << 6) + (value << 1) +
                (value >> 4)) >> (16 - gx_color_value_bits));
            value = (ushort) (color & 0x1f);
            prgb[2] = (gx_color_value)
                (((value << 11) + (value << 6) + (value << 1) +
                (value >> 4)) >> (16 - gx_color_value_bits));
        }
        else {
            /* byte0=RRRRRGGG byte1=GGGBBBBB */
            value = (ushort) (color >> 11);
            prgb[0] = ((value << 11) + (value << 6) + (value << 1) +
                (value >> 4)) >> (16 - gx_color_value_bits);
            value = (ushort) ((color >> 5) & 0x3f);
            prgb[1] = (gx_color_value)
                ((value << 10) + (value << 4) + (value >> 2))
                      >> (16 - gx_color_value_bits);
            value = (ushort) (color & 0x1f);
            prgb[2] = (gx_color_value)
                ((value << 11) + (value << 6) + (value << 1) +
                (value >> 4)) >> (16 - gx_color_value_bits);
        }
    }
    else {
        if ((ddev->nFormat & DISPLAY_555_MASK) == DISPLAY_NATIVE_555) {
            /* byte0=GGGBBBBB byte1=0RRRRRGG */
            value = (ushort) ((color >> 2) & 0x1f);
            prgb[0] = (gx_color_value)
                ((value << 11) + (value << 6) + (value << 1) +
                (value >> 4)) >> (16 - gx_color_value_bits);
            value = (ushort)
                (((color << 3) & 0x18) +  ((color >> 13) & 0x7));
            prgb[1] = (gx_color_value)
                ((value << 11) + (value << 6) + (value << 1) +
                (value >> 4)) >> (16 - gx_color_value_bits);
            value = (ushort) ((color >> 8) & 0x1f);
            prgb[2] = (gx_color_value)
                ((value << 11) + (value << 6) + (value << 1) +
                (value >> 4)) >> (16 - gx_color_value_bits);
        }
        else {
            /* byte0=GGGBBBBB byte1=RRRRRGGG */
            value = (ushort) ((color >> 3) & 0x1f);
            prgb[0] = (gx_color_value)
                (((value << 11) + (value << 6) + (value << 1) +
                (value >> 4)) >> (16 - gx_color_value_bits));
            value = (ushort)
                (((color << 3) & 0x38) +  ((color >> 13) & 0x7));
            prgb[1] = (gx_color_value)
                (((value << 10) + (value << 4) + (value >> 2))
                      >> (16 - gx_color_value_bits));
            value = (ushort) ((color >> 8) & 0x1f);
            prgb[2] = (gx_color_value)
                (((value << 11) + (value << 6) + (value << 1) +
                (value >> 4)) >> (16 - gx_color_value_bits));
        }
    }
    return 0;
}

/* Map a r-g-b color to a color code */
static gx_color_index
display_map_rgb_color_rgb(gx_device * dev, const gx_color_value cv[])
{
    gx_device_display *ddev = (gx_device_display *) dev;
    gx_color_value r = cv[0];
    gx_color_value g = cv[1];
    gx_color_value b = cv[2];
    int drop = gx_color_value_bits - 8;
    gx_color_value red, green, blue;

    red  = r >> drop;
    green = g >> drop;
    blue = b >> drop;

    switch (ddev->nFormat & DISPLAY_ALPHA_MASK) {
        case DISPLAY_ALPHA_NONE:
            if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN) {
                gx_color_value rgb[3];
                rgb[0] = r; rgb[1] = g; rgb[2] = b;
                return gx_default_rgb_map_rgb_color(dev, rgb); /* RGB */
            }
            else
                return ((gx_color_index)blue<<16) + (green<<8) + red;		/* BGR */
        case DISPLAY_ALPHA_FIRST:
        case DISPLAY_UNUSED_FIRST:
            if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN)
                return ((gx_color_index)red<<16) + (green<<8) + blue;		/* xRGB */
            else
                return ((gx_color_index)blue<<16) + (green<<8) + red;		/* xBGR */
        case DISPLAY_ALPHA_LAST:
        case DISPLAY_UNUSED_LAST:
            if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN)
                return ((gx_color_index)red<<24) + ((gx_color_index)green<<16) + (blue<<8);	/* RGBx */
            else
                return ((gx_color_index)blue<<24) + ((gx_color_index)green<<16) + (red<<8);	/* BGRx */
    }
    return 0;
}

/* Map a color code to r-g-b. */
static int
display_map_color_rgb_rgb(gx_device * dev, gx_color_index color,
                 gx_color_value prgb[3])
{
    gx_device_display *ddev = (gx_device_display *) dev;
    uint bits_per_color = 8;
    uint color_mask;

    color_mask = (1 << bits_per_color) - 1;

    switch (ddev->nFormat & DISPLAY_ALPHA_MASK) {
        case DISPLAY_ALPHA_NONE:
            if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN)
                return gx_default_rgb_map_color_rgb(dev, color, prgb); /* RGB */
            else {
                /* BGR */
                prgb[0] = (gx_color_value) (((color) & color_mask) *
                        (ulong) gx_max_color_value / color_mask);
                prgb[1] = (gx_color_value)
                        (((color >> bits_per_color) & color_mask) *
                        (ulong) gx_max_color_value / color_mask);
                prgb[2] = (gx_color_value)
                        (((color >> 2*bits_per_color) & color_mask) *
                        (ulong) gx_max_color_value / color_mask);
            }
            break;
        case DISPLAY_ALPHA_FIRST:
        case DISPLAY_UNUSED_FIRST:
            if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN) {
                /* xRGB */
                prgb[0] = (gx_color_value)
                        (((color >> 2*bits_per_color) & color_mask) *
                        (ulong) gx_max_color_value / color_mask);
                prgb[1] = (gx_color_value)
                        (((color >> bits_per_color) & color_mask) *
                        (ulong) gx_max_color_value / color_mask);
                prgb[2] = (gx_color_value) (((color) & color_mask) *
                        (ulong) gx_max_color_value / color_mask);
            }
            else {
                /* xBGR */
                prgb[0] = (gx_color_value)
                        (((color) & color_mask) *
                        (ulong) gx_max_color_value / color_mask);
                prgb[1] = (gx_color_value)
                        (((color >> bits_per_color)   & color_mask) *
                        (ulong) gx_max_color_value / color_mask);
                prgb[2] = (gx_color_value)
                        (((color >> 2*bits_per_color) & color_mask) *
                        (ulong) gx_max_color_value / color_mask);
            }
            break;
        case DISPLAY_ALPHA_LAST:
        case DISPLAY_UNUSED_LAST:
            if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN) {
                /* RGBx */
                prgb[0] = (gx_color_value)
                        (((color >> 3*bits_per_color) & color_mask) *
                        (ulong) gx_max_color_value / color_mask);
                prgb[1] = (gx_color_value)
                        (((color >> 2*bits_per_color) & color_mask) *
                        (ulong) gx_max_color_value / color_mask);
                prgb[2] = (gx_color_value)
                        (((color >> bits_per_color)   & color_mask) *
                        (ulong) gx_max_color_value / color_mask);
            }
            else {
                /* BGRx */
                prgb[0] = (gx_color_value)
                        (((color >> bits_per_color)   & color_mask) *
                        (ulong) gx_max_color_value / color_mask);
                prgb[1] = (gx_color_value)
                        (((color >> 2*bits_per_color) & color_mask) *
                        (ulong) gx_max_color_value / color_mask);
                prgb[2] = (gx_color_value)
                        (((color >> 3*bits_per_color) & color_mask) *
                        (ulong) gx_max_color_value / color_mask);
            }
    }
    return 0;
}

/* Map a r-g-b color to a color code */
static gx_color_index
display_map_rgb_color_bgr24(gx_device * dev, const gx_color_value cv[])
{
    gx_color_value r = cv[0];
    gx_color_value g = cv[1];
    gx_color_value b = cv[2];
    return (gx_color_value_to_byte(b)<<16) +
           (gx_color_value_to_byte(g)<<8) +
            gx_color_value_to_byte(r);
}

/* Map a color code to r-g-b. */
static int
display_map_color_rgb_bgr24(gx_device * dev, gx_color_index color,
                 gx_color_value prgb[3])
{
    prgb[0] = gx_color_value_from_byte(color & 0xff);
    prgb[1] = gx_color_value_from_byte((color >> 8) & 0xff);
    prgb[2] = gx_color_value_from_byte((color >> 16) & 0xff);
    return 0;
}

/* Fill a rectangle */
static int
display_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                       gx_color_index color)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    if (ddev->callback == NULL)
        return 0;		/* ignore -- needed for fillpage when device wasn't really opened */
    ddev->mutated_procs.fill_rectangle(dev, x, y, w, h, color);

    while(dev->parent)
        dev = dev->parent;

    if (ddev->callback->display_update)
        (*(ddev->callback->display_update))(ddev->pHandle, dev, x, y, w, h);
    return 0;
}

/* Copy a monochrome bitmap */
static int
display_copy_mono(gx_device * dev,
                  const byte * base, int sourcex, int raster,
                  gx_bitmap_id id, int x, int y, int w, int h,
                  gx_color_index zero, gx_color_index one)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    if (ddev->callback == NULL)
        return gs_error_Fatal;
    ddev->mutated_procs.copy_mono(dev, base, sourcex, raster, id,
                                  x, y, w, h, zero, one);

    while(dev->parent)
        dev = dev->parent;

    if (ddev->callback->display_update)
        (*(ddev->callback->display_update))(ddev->pHandle, dev, x, y, w, h);
    return 0;
}

/* Copy a color pixel map  */
static int
display_copy_color(gx_device * dev,
                   const byte * base, int sourcex, int raster,
                   gx_bitmap_id id, int x, int y, int w, int h)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    if (ddev->callback == NULL)
        return gs_error_Fatal;
    ddev->mutated_procs.copy_color(dev, base, sourcex, raster, id, x, y, w, h);

    while(dev->parent)
        dev = dev->parent;

    if (ddev->callback->display_update)
        (*(ddev->callback->display_update))(ddev->pHandle, dev, x, y, w, h);
    return 0;
}

static int
display_get_bits_rectangle(gx_device * dev, const gs_int_rect *rect,
                           gs_get_bits_params_t *params)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    if (ddev->callback == NULL)
        return gs_error_Fatal;
    return ddev->mutated_procs.get_bits_rectangle(dev, rect, params);
}

static int
display_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    int code;
    gs_param_string dhandle;
    int idx;
    int val;
    int i = 0;
    size_t dptr;
    char buf[64];

    code = gx_default_get_params(dev, plist);
    if (code < 0)
        return code;

    if (!ddev->pHandle_set) {
        idx = ((int)sizeof(size_t)) * 8 - 4;
        buf[i++] = '1';
        buf[i++] = '6';
        buf[i++] = '#';
        dptr = (size_t)(ddev->pHandle);
        while (idx >= 0) {
            val = (int)(dptr >> idx) & 0xf;
            if (val <= 9)
                buf[i++] = '0' + val;
            else
                buf[i++] = 'a' - 10 + val;
            idx -= 4;
        }
        buf[i] = '\0';

        param_string_from_transient_string(dhandle, buf);
        code = param_write_string(plist, "DisplayHandle", &dhandle);
    }

    (void)(code < 0 ||
        (code = param_write_int(plist,
            "DisplayFormat", &ddev->nFormat)) < 0 ||
        (code = param_write_float(plist,
            "DisplayResolution", &ddev->HWResolution[1])) < 0);
    if (code >= 0 &&
        (ddev->nFormat & DISPLAY_COLORS_MASK) == DISPLAY_COLORS_SEPARATION)
        code = devn_get_params(dev, plist, &ddev->devn_params,
                &ddev->equiv_cmyk_colors);
    return code;
}

/* Put parameters. */
/* The parameters "DisplayHandle" and "DisplayFormat"
 * can be changed when the device is closed, but not when open.
 * The device width and height can be changed when open.
 */
static int
display_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    int ecode = 0, code;
    bool is_open = dev->is_open;
    gs_param_float_array hwra;
    float dispres = 0.0;

    int old_width = dev->width;
    int old_height = dev->height;
    int old_format = ddev->nFormat;
    int old_npp = ddev->num_planar_planes;
    void *old_handle = ddev->pHandle;
    gs_devn_params *pdevn_params = &ddev->devn_params;
    equivalent_cmyk_color_params *pequiv_colors = &ddev->equiv_cmyk_colors;
    /* Save current data in case we have a problem */
    gs_devn_params saved_devn_params = *pdevn_params;
    equivalent_cmyk_color_params saved_equiv_colors = *pequiv_colors;
    int format;
    void *handle;
    int found_string_handle = 0;
    gs_param_string dh = { 0 };

    /* Handle extra parameters */

    switch (code = param_read_int(plist, "DisplayFormat", &format)) {
        case 0:
            if (dev->is_open) {
                if (ddev->nFormat != format)
                    ecode = gs_error_rangecheck;
                else
                    break;
            }
            else {
                code = display_set_color_format(ddev, format);
                if (code < 0)
                    ecode = code;
                else
                    break;
            }
            goto cfe;
        default:
            ecode = code;
          cfe:param_signal_error(plist, "DisplayFormat", ecode);
        case 1:
            break;
    }

    if (!ddev->pHandle_set) {
        /* 64-bit systems need to use DisplayHandle as a string */
        switch (code = param_read_string(plist, "DisplayHandle", &dh)) {
            case 0:
                found_string_handle = 1;
                break;
            default:
                if ((code == gs_error_typecheck) && (sizeof(size_t) <= 4)) {
                    /* 32-bit systems can use the older long type */
                    switch (code = param_read_long(plist, "DisplayHandle",
                        (long *)(&handle))) {
                        case 0:
                            if (dev->is_open) {
                                if (ddev->pHandle != handle)
                                    ecode = gs_error_rangecheck;
                                else
                                    break;
                            }
                            else {
                                ddev->pHandle = handle;
                                break;
                            }
                            goto hdle;
                        default:
                            ecode = code;
                          hdle:param_signal_error(plist, "DisplayHandle", ecode);
                        case 1:
                            break;
                    }
                    break;
                }
                ecode = code;
                param_signal_error(plist, "DisplayHandle", ecode);
                /* fall through */
            case 1:
                dh.data = 0;
                break;
        }
        if (found_string_handle) {
            /*
             * Convert from a string to a pointer.
             * It is assumed that size_t has the same size as a pointer.
             * Allow formats (1234), (10#1234) or (16#04d2).
             */
            size_t ptr = 0;
            int i;
            int base = 10;
            int val;
            code = 0;
            for (i=0; i<dh.size; i++) {
                val = dh.data[i];
                if ((val >= '0') && (val <= '9'))
                    val = val - '0';
                else if ((val >= 'A') && (val <= 'F'))
                    val = val - 'A' + 10;
                else if ((val >= 'a') && (val <= 'f'))
                    val = val - 'a' + 10;
                else if (val == '#') {
                    base = (int)ptr;
                    ptr = 0;
                    if ((base != 10) && (base != 16)) {
                        code = gs_error_rangecheck;
                        break;
                    }
                    continue;
                }
                else {
                    code = gs_error_rangecheck;
                    break;
                }

                if (base == 10)
                    ptr = ptr * 10 + val;
                else if (base == 16)
                    ptr = ptr * 16 + val;
                else {
                    code = gs_error_rangecheck;
                    break;
                }
            }
            if (code == 0) {
                if (dev->is_open) {
                    if (ddev->pHandle != (void *)ptr)
                        code = gs_error_rangecheck;
                }
                else
                    ddev->pHandle = (void *)ptr;
            }
            if (code < 0) {
                ecode = code;
                param_signal_error(plist, "DisplayHandle", ecode);
            }
        }
    }

    /*
     * Set the initial display resolution.
     * If HWResolution is explicitly set, e.g. using -rDPI on the
     * command line, then use that.  Otherwise, use DisplayResolution
     * which is typically set by the client to the display
     * logical resolution.  Once either of these have been
     * used, ignore all further DisplayResolution parameters.
     */
    if (param_read_float_array(plist, "HWResolution", &hwra) == 0)
        ddev->HWResolution_set = 1;

    switch (code = param_read_float(plist, "DisplayResolution", &dispres)) {
        case 0:
            if (!ddev->HWResolution_set) {
                gx_device_set_resolution(dev, dispres, dispres);
                ddev->HWResolution_set = 1;
            }
            break;
        default:
            ecode = code;
            param_signal_error(plist, "DisplayResolution", ecode);
        case 1:
            break;
    }

    if (ecode >= 0 &&
        (ddev->nFormat & DISPLAY_COLORS_MASK) == DISPLAY_COLORS_SEPARATION) {
        if (ddev->nFormat & (DISPLAY_PLANAR | DISPLAY_PLANAR_INTERLEAVED))
        {
            /* If we are in planar mode, then we only want to allocate as many
             * separations as we need (which may be 'MAX' in the PS case where
             * we don't know how many we need). This means that we might want to
             * close the device in order to reopen it with the revised number
             * later on. Use a utility function for this. */
            int n;
            ecode = devn_generic_put_params(dev, plist, pdevn_params, pequiv_colors, 0);
            n = pdevn_params->num_std_colorant_names + pdevn_params->separations.num_separations;
            if (n > ddev->color_info.max_components)
                n = ddev->color_info.max_components;
            ddev->color_info.num_components = n;
            ddev->color_info.depth = n * 8;
            if (ddev->num_planar_planes)
                ddev->num_planar_planes = ddev->color_info.num_components;
            is_open = ddev->is_open;
        }
        else
        {
            /* In the chunky case, we always just use ARCH_SIZEOF_COLOR_INDEX (8) spots.
             * Setting MaxSeparations or PageSpotColors will change the color_info.depth
             * in devn_put_params, so we need to put it back to 64bpp. */
            ecode = devn_put_params(dev, plist, pdevn_params, pequiv_colors);
            ddev->color_info.depth = ARCH_SIZEOF_COLOR_INDEX * 8;
        }
    }

    if (ecode >= 0) {
        /* Prevent gx_default_put_params from closing the device. */
        dev->is_open = false;
        ecode = gx_default_put_params(dev, plist);
        dev->is_open = is_open;
    }
    if (ecode < 0) {
        /* If we have an error then restore original data. */
        *pdevn_params = saved_devn_params;
        *pequiv_colors = saved_equiv_colors;
        if (format != old_format)
            display_set_color_format(ddev, old_format);
        ddev->pHandle = old_handle;
        dev->width = old_width;
        dev->height = old_height;
        dev->num_planar_planes = old_npp;
        return ecode;
    }

    if ( is_open && ddev->callback &&
        ((old_width != dev->width) || (old_height != dev->height)) ) {
        /* We can resize this device while it is open, but we cannot
         * change the color format or handle.
         */

        while(dev->parent) {
            dev = dev->parent;
            gx_update_from_subclass(dev);
        }

        /* Tell caller we are about to change the device parameters */
        if ((*ddev->callback->display_presize)(ddev->pHandle, dev,
            ddev->width, ddev->height, display_raster(ddev),
            ddev->nFormat) < 0) {
            /* caller won't let us change the size */
            /* restore parameters then return an error */
            *pdevn_params = saved_devn_params;
            *pequiv_colors = saved_equiv_colors;
            display_set_color_format(ddev, old_format);
            ddev->nFormat = old_format;
            ddev->pHandle = old_handle;
            ddev->width = old_width;
            ddev->height = old_height;
            dev->num_planar_planes = old_npp;
            return_error(gs_error_rangecheck);
        }

        dev = (gx_device *)ddev;
        while(dev->parent) {
            dev = dev->parent;
            gx_update_from_subclass(dev);
        }

        display_free_bitmap(ddev);

        code = display_alloc_bitmap(ddev, dev);
        if (code < 0) {
            int ecode;

            /* if we failed (probably VMerror) try to revert to old settings */
            *pdevn_params = saved_devn_params;
            *pequiv_colors = saved_equiv_colors;
            display_set_color_format(ddev, old_format);
            ddev->nFormat = old_format;
            dev->width = old_width;
            dev->height = old_height;
            dev->num_planar_planes = old_npp;
            ecode = display_alloc_bitmap(ddev, dev);
            if (ecode < 0) {
                emprintf(dev->memory, "*** Fatal error in display_put_params, could not allocate bitmap ***\n");
                return_error(gs_error_Fatal);
            }
            return_error(code);
        }

        /* tell caller about the new size */
        if ((*ddev->callback->display_size)(ddev->pHandle, dev,
            dev->width, dev->height, display_raster(ddev), ddev->nFormat,
            CLIST_MUTATABLE_HAS_MUTATED(ddev) ? NULL :
                                  ((gx_device_memory *)ddev)->base) < 0)
            return_error(gs_error_rangecheck);
    }
    /*
     * Make the color_info.depth correct for the bpc and num_components since
     * devn mode always has the display bitmap set up for 64-bits, but others,
     * such as pdf14 compositor expect it to match (for "deep" detection).
     */
    if (ddev->icc_struct && ddev->icc_struct->supports_devn) {
        ddev->color_info.depth = ddev->devn_params.bitspercomponent *
                                     ddev->color_info.num_components;
    }
    return 0;
}

static int display_initialize_device(gx_device *dev)
{
    gx_device_display *ddev = (gx_device_display *) dev;

    /* Mark the new instance as closed. */
    ddev->is_open = false;

    /* Clear pointers */
    ddev->pBitmap = NULL;
    ddev->zBitmapSize = 0;

    return 0;
}

static void
display_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, initialize_device, display_initialize_device);
    set_dev_proc(dev, open_device, display_open);
    set_dev_proc(dev, get_initial_matrix, display_get_initial_matrix);
    set_dev_proc(dev, sync_output, display_sync_output);
    set_dev_proc(dev, output_page, display_output_page);
    set_dev_proc(dev, close_device, display_close);
    set_dev_proc(dev, map_rgb_color, gx_default_w_b_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gx_default_w_b_map_color_rgb);
    set_dev_proc(dev, get_params, display_get_params);
    set_dev_proc(dev, put_params, display_put_params);
    set_dev_proc(dev, map_cmyk_color, gx_default_cmyk_map_cmyk_color);
    set_dev_proc(dev, get_page_device, gx_page_device_get_page_device);
    set_dev_proc(dev, fill_rectangle_hl_color, display_fill_rectangle_hl_color);
    set_dev_proc(dev, update_spot_equivalent_colors, display_update_spot_equivalent_colors);
    set_dev_proc(dev, ret_devn_params, display_ret_devn_params);
    set_dev_proc(dev, dev_spec_op, display_spec_op);
}

/*
 * The following procedures are used to map the standard color spaces into
 * the separation color components for the display device.
 */
static void
display_separation_gray_cs_to_cmyk_cm(const gx_device * dev, frac gray, frac out[])
{
    int * map =
      (int *)(&((gx_device_display *) dev)->devn_params.separation_order_map);

    gray_cs_to_devn_cm(dev, map, gray, out);
}

static void
display_separation_rgb_cs_to_cmyk_cm(const gx_device * dev,
    const gs_gstate *pgs, frac r, frac g, frac b, frac out[])
{
    int * map =
      (int *)(&((gx_device_display *) dev)->devn_params.separation_order_map);

    rgb_cs_to_devn_cm(dev, map, pgs, r, g, b, out);
}

static void
display_separation_cmyk_cs_to_cmyk_cm(const gx_device * dev,
    frac c, frac m, frac y, frac k, frac out[])
{
    const int * map =
      (int *)(&((gx_device_display *) dev)->devn_params.separation_order_map);

    cmyk_cs_to_devn_cm(dev, map, c, m, y, k, out);
}

static const gx_cm_color_map_procs display_separation_cm_procs = {
    display_separation_gray_cs_to_cmyk_cm,
    display_separation_rgb_cs_to_cmyk_cm,
    display_separation_cmyk_cs_to_cmyk_cm
};

static const gx_cm_color_map_procs *
display_separation_get_color_mapping_procs(const gx_device * dev, const gx_device **map_dev)
{
    *map_dev = dev;
    return &display_separation_cm_procs;
}

/*
 * Encode a list of colorant values into a gx_color_index_value.
 */
static gx_color_index
display_separation_encode_color(gx_device *dev, const gx_color_value colors[])
{
    int bpc = ((gx_device_display *)dev)->devn_params.bitspercomponent;
    gx_color_index color = 0;
    int i = 0;
    int ncomp = dev->color_info.num_components;
    COLROUND_VARS;

    COLROUND_SETUP(bpc);
    for (; i<ncomp; i++) {
        color <<= bpc;
        color |= COLROUND_ROUND(colors[i]);
    }
    if (bpc*ncomp < ARCH_SIZEOF_COLOR_INDEX * 8)
        color <<= (ARCH_SIZEOF_COLOR_INDEX * 8 - ncomp * bpc);
    return (color == gx_no_color_index ? color ^ 1 : color);
}

/*
 * Decode a gx_color_index value back to a list of colorant values.
 */
static int
display_separation_decode_color(gx_device * dev, gx_color_index color,
    gx_color_value * out)
{
    int bpc = ((gx_device_display *)dev)->devn_params.bitspercomponent;
    int mask = (1 << bpc) - 1;
    int i = 0;
    int ncomp = dev->color_info.num_components;
    COLDUP_VARS;

    COLDUP_SETUP(bpc);
    if (bpc*ncomp < ARCH_SIZEOF_COLOR_INDEX * 8)
        color >>= (ARCH_SIZEOF_COLOR_INDEX * 8 - ncomp * bpc);
    for (; i<ncomp; i++) {
        out[ncomp - i - 1] = COLDUP_DUP(color & mask);
        color >>= bpc;
    }
    return 0;
}

/*
 *  Device proc for updating the equivalent CMYK color for spot colors.
 */
static int
display_update_spot_equivalent_colors(gx_device * dev, const gs_gstate * pgs, const gs_color_space *pcs)
{
    gx_device_display * ddev = (gx_device_display *)dev;

    if ((ddev->nFormat & DISPLAY_COLORS_MASK) == DISPLAY_COLORS_SEPARATION)
        update_spot_equivalent_cmyk_colors(dev, pgs, pcs,
                    &ddev->devn_params, &ddev->equiv_cmyk_colors);
    return 0;
}

/*
 *  Device proc for returning a pointer to DeviceN parameter structure
 */
static gs_devn_params *
display_ret_devn_params(gx_device * dev)
{
    gx_device_display * pdev = (gx_device_display *)dev;

    return &pdev->devn_params;
}

static int
display_spec_op(gx_device *dev, int op, void *data, int datasize)
{
    gx_device_display *ddev = (gx_device_display *)dev;

    if (op == gxdso_supports_devn || op == gxdso_skip_icc_component_validation) {
        /* If we're SEPARATION, then we certainly support devn. */
        if (ddev->nFormat & DISPLAY_COLORS_SEPARATION)
            return 1;
        /* Not sure about this test. Historically this is what we've done, but
         * it fails for planar SEPARATIONS because we're using mem_planar_fill_rectangle_hl_color. */
        return (dev_proc(dev, fill_rectangle_hl_color) == display_fill_rectangle_hl_color);
    }
    if (op == gxdso_reopen_after_init) {
        return 1;
    }
    if (op == gxdso_adjust_bandheight)
    {
        if (ddev->callback->display_adjust_band_height)
            return ddev->callback->display_adjust_band_height(ddev->pHandle,
                                                              ddev,
                                                              datasize);
        return 0;
    }

    return gx_default_dev_spec_op(dev, op, data, datasize);
}

/* Fill a rectangle with a high level color.  This is used in separation mode */
static int
display_fill_rectangle_hl_color(gx_device *dev, const gs_fixed_rect *rect,
    const gs_gstate *pgs, const gx_drawing_color *pdcolor,
    const gx_clip_path *pcpath)
{
    int x = fixed2int(rect->p.x);
    int y = fixed2int(rect->p.y);
    int w = fixed2int(rect->q.x) - x;
    int h = fixed2int(rect->q.y) - y;
    gx_color_index pure_color;

    /* We can only handle devn cases, so use the default if not */
    /* We can get called here from gx_dc_devn_masked_fill_rectangle */
    if (pdcolor->type != gx_dc_type_devn && pdcolor->type != &gx_dc_devn_masked) {
        return gx_fill_rectangle_device_rop( x, y, w, h, pdcolor, dev, lop_default);
    }
    pure_color = dev_proc(dev, encode_color)(dev, pdcolor->colors.devn.values);
    return dev_proc(dev, fill_rectangle)(dev, x, y, w, h, pure_color);
}

/*
 * This routine will check to see if the color component name  match those
 * that are available amoung the current device's color components.
 *
 * Parameters:
 *   dev - pointer to device data structure.
 *   pname - pointer to name (zero termination not required)
 *   nlength - length of the name
 *
 * This routine returns a positive value (0 to n) which is the device colorant
 * number if the name is found.  It returns GX_DEVICE_COLOR_MAX_COMPONENTS if
 * the colorant is not being used due to a SeparationOrder device parameter.
 * It returns a negative value if not found.
 */
static int
display_separation_get_color_comp_index(gx_device * dev,
    const char * pname, int name_size, int component_type)
{
    return devn_get_color_comp_index(dev,
                &(((gx_device_display *)dev)->devn_params),
                &(((gx_device_display *)dev)->equiv_cmyk_colors),
                pname, name_size, component_type, ENABLE_AUTO_SPOT_COLORS);
}

/* ------ Internal routines ------ */

/* Make sure we have been given a valid structure */
/* Return 0 on success, gs_error_rangecheck on failure */
static int display_check_structure(gx_device_display *ddev)
{
    if (ddev->callback == NULL)
        return_error(gs_error_rangecheck);

    if (ddev->callback->size == sizeof(struct display_callback_v1_s)) {
        /* Original V1 structure */
        if (ddev->callback->version_major != DISPLAY_VERSION_MAJOR_V1)
            return_error(gs_error_rangecheck);

        /* complain if caller asks for newer features */
        if (ddev->callback->version_minor > DISPLAY_VERSION_MINOR_V1)
            return_error(gs_error_rangecheck);
    }
    else if (ddev->callback->size == sizeof(struct display_callback_v2_s)) {
        /* V2 structure with added display_separation callback */
        if (ddev->callback->version_major != DISPLAY_VERSION_MAJOR_V2)
            return_error(gs_error_rangecheck);

        /* complain if caller asks for newer features */
        if (ddev->callback->version_minor > DISPLAY_VERSION_MINOR_V2)
            return_error(gs_error_rangecheck);
    }
    else {
        /* V3 structure with added display_separation callback */
        if (ddev->callback->size != sizeof(display_callback))
            return_error(gs_error_rangecheck);

        if (ddev->callback->version_major != DISPLAY_VERSION_MAJOR)
            return_error(gs_error_rangecheck);

        /* complain if caller asks for newer features */
        if (ddev->callback->version_minor > DISPLAY_VERSION_MINOR)
            return_error(gs_error_rangecheck);
    }

    if ((ddev->callback->display_open == NULL) ||
        (ddev->callback->display_close == NULL) ||
        (ddev->callback->display_presize == NULL) ||
        (ddev->callback->display_size == NULL) ||
        (ddev->callback->display_sync == NULL) ||
        (ddev->callback->display_page == NULL))
        return_error(gs_error_rangecheck);

    /* Don't test display_update, display_memalloc or display_memfree
     * since these may be NULL if not provided.
     * Don't test display_separation, since this may be NULL if
     * separation format is not supported.
     */

    return 0;
}

static void
display_free_bitmap(gx_device_display * ddev)
{
    if (ddev->callback == NULL)
        return;
    if (ddev->pBitmap) {
        if (ddev->callback->display_memalloc
            && ddev->callback->display_memfree
            && ddev->pBitmap) {
            (*ddev->callback->display_memfree)(ddev->pHandle, ddev,
                ddev->pBitmap);
        }
        else {
            gs_free_object(ddev->memory->non_gc_memory,
                ddev->pBitmap, "display_free_bitmap");
        }
        ddev->pBitmap = NULL;
        if (!CLIST_MUTATABLE_HAS_MUTATED(ddev))
            ((gx_device_memory *)ddev)->base = NULL;
    }

    if (CLIST_MUTATABLE_HAS_MUTATED(ddev)) {
        gx_device_clist *const pclist_dev = (gx_device_clist *)ddev;
        gx_device_clist_common * const pcldev = &pclist_dev->common;
        gx_device_clist_reader * const pcrdev = &pclist_dev->reader;
        /* Close cmd list device & point to the storage */
        clist_close( (gx_device *)pcldev );
        gs_free_object(ddev->memory->non_gc_memory, ddev->buf, "clist cmd buffer");
        ddev->buf = NULL;
        ddev->buffer_space = 0;

        gs_free_object(pcldev->memory->non_gc_memory, pcldev->cache_chunk, "free tile cache for clist");
        pcldev->cache_chunk = 0;

        rc_decrement(pcldev->icc_cache_cl, "gdev_prn_tear_down");
        pcldev->icc_cache_cl = NULL;

        clist_free_icc_table(pcldev->icc_table, pcldev->memory);
        pcldev->icc_table = NULL;

        /* If the clist is a reader clist, free any color_usage_array
         * memory used by same.
         */
        if (!CLIST_IS_WRITER(pclist_dev))
            gs_free_object(pcrdev->memory, pcrdev->color_usage_array, "clist_color_usage_array");
    }
}

/* calculate byte length of a row */
static int
display_raster(gx_device_display *dev)
{
    int align = 0;
    int n = (dev->nFormat & (DISPLAY_PLANAR | DISPLAY_PLANAR_INTERLEAVED)) ?
             dev->color_info.num_components : 1;
    int bytewidth = ((dev->width * dev->color_info.depth / n) + 7) /8;
    switch (dev->nFormat & DISPLAY_ROW_ALIGN_MASK) {
        case DISPLAY_ROW_ALIGN_4:
            align = 4;
            break;
        case DISPLAY_ROW_ALIGN_8:
            align = 8;
            break;
        case DISPLAY_ROW_ALIGN_16:
            align = 16;
            break;
        case DISPLAY_ROW_ALIGN_32:
            align = 32;
            break;
        case DISPLAY_ROW_ALIGN_64:
            align = 64;
            break;
    }
    if (align < ARCH_ALIGN_PTR_MOD)
        align = ARCH_ALIGN_PTR_MOD;
    align -= 1;
    bytewidth = (bytewidth + align) & (~align);
    if (dev->nFormat & DISPLAY_PLANAR_INTERLEAVED)
        bytewidth *= n;
    return bytewidth;
}

/* Set the buffer device to planar mode. */
static int
set_planar(gx_device_memory *mdev, const gx_device *tdev, int interleaved)
{
    int num_comp = tdev->color_info.num_components;
    gx_render_plane_t planes[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int depth = tdev->color_info.depth / num_comp;
    int k;

    if (num_comp < 1 || num_comp > GX_DEVICE_COLOR_MAX_COMPONENTS)
        return_error(gs_error_rangecheck);
    /* Round up the depth per plane to a power of 2. */
    while (depth & (depth - 1))
        --depth, depth = (depth | (depth >> 1)) + 1;

    /* We want the most significant plane to come out first. */
    planes[num_comp-1].shift = 0;
    planes[num_comp-1].depth = depth;
    for (k = (num_comp - 2); k >= 0; k--) {
        planes[k].depth = depth;
        planes[k].shift = planes[k + 1].shift + depth;
    }
    return gdev_mem_set_planar_interleaved(mdev, num_comp, planes,
                                           interleaved);
}

static int
display_create_buf_device(gx_device **pbdev, gx_device *target, int y,
                          const gx_render_plane_t *render_plane,
                          gs_memory_t *mem, gx_color_usage_t *color_usage)
{
    int depth;
    const gx_device_memory *mdproto;
    gx_device_memory *mdev;
    gx_device_display *ddev = (gx_device_display *)target;

    depth = target->color_info.depth;
    if (target->num_planar_planes)
        depth /= target->num_planar_planes;

    mdproto = gdev_mem_device_for_bits(depth);
    if (mdproto == NULL)
        return_error(gs_error_rangecheck);
    if (mem) {
        mdev = gs_alloc_struct_immovable(mem, gx_device_memory, &st_device_memory,
                               "create_buf_device");
        if (mdev == NULL)
            return_error(gs_error_VMerror);
    } else {
        mdev = (gx_device_memory *)*pbdev;
    }
    if (target == (gx_device *)mdev) {
        dev_t_proc_dev_spec_op((*orig_dso), gx_device) = dev_proc(mdev, dev_spec_op);
        /* The following is a special hack for setting up printer devices. */
        assign_dev_procs(mdev, mdproto);
        mdev->initialize_device_procs = mdproto->initialize_device_procs;
        mdev->initialize_device_procs((gx_device *)mdev);
        /* We know mdev->procs.initialize_device is NULL. */
        /* Do not override the dev_spec_op! */
        dev_proc(mdev, dev_spec_op) = orig_dso;
        check_device_separable((gx_device *)mdev);
        gx_device_fill_in_procs((gx_device *)mdev);
    } else {
        gs_make_mem_device(mdev, mdproto, mem, (color_usage == NULL ? 1 : 0),
                           target);
    }
    if (ddev->nFormat & DISPLAY_COLORS_SEPARATION)
        mdev->procs.fill_rectangle_hl_color = display_fill_rectangle_hl_color;
    mdev->width = target->width;
    mdev->band_y = y;
    mdev->log2_align_mod = target->log2_align_mod;
    mdev->pad = target->pad;
    mdev->num_planar_planes = target->num_planar_planes;
    /*
     * The matrix in the memory device is irrelevant,
     * because all we do with the device is call the device-level
     * output procedures, but we may as well set it to
     * something halfway reasonable.
     */
    gs_deviceinitialmatrix(target, &mdev->initial_matrix);
    /****** QUESTIONABLE, BUT BETTER THAN OMITTING ******/
    if (&mdev->color_info != &target->color_info) /* Pacify Valgrind */
        mdev->color_info = target->color_info;
    *pbdev = (gx_device *)mdev;

    if (ddev->nFormat & (DISPLAY_PLANAR | DISPLAY_PLANAR_INTERLEAVED)) {
        int interleaved = (ddev->nFormat & DISPLAY_PLANAR_INTERLEAVED);
        if (gs_device_is_memory(*pbdev) /* == render_plane->index < 0 */) {
            return set_planar((gx_device_memory *)*pbdev, *pbdev, interleaved);
        }
    }

    return 0;
}

static int
display_size_buf_device(gx_device_buf_space_t *space, gx_device *target,
                        const gx_render_plane_t *render_plane,
                        int height, bool for_band)
{
    gx_device_display *ddev = (gx_device_display *)target;
    gx_device_memory mdev = { 0 };
    int code;
    int planar = ddev->nFormat & (DISPLAY_PLANAR | DISPLAY_PLANAR_INTERLEAVED);
    int interleaved = (ddev->nFormat & DISPLAY_PLANAR_INTERLEAVED);

    if (!planar || (render_plane && render_plane->index >= 0))
        return gx_default_size_buf_device(space, target, render_plane,
                                          height, for_band);

    /* Planar case */
    mdev.color_info = target->color_info;
    if (ddev->nFormat & DISPLAY_COLORS_SEPARATION)
    {
        /* For planar separations, we use the real number of comps
         * with 8 bits per plane. */
        mdev.color_info.depth = mdev.color_info.num_components * 8;
    }
    mdev.pad = target->pad;
    mdev.log2_align_mod = target->log2_align_mod;
    mdev.num_planar_planes = target->num_planar_planes;
    code = set_planar(&mdev, target, interleaved);
    if (code < 0)
        return code;
    if (gdev_mem_bits_size(&mdev, target->width, height, &(space->bits)) < 0)
        return_error(gs_error_VMerror);
    space->line_ptrs = gdev_mem_line_ptrs_size(&mdev, target->width, height);
    space->raster = display_raster(ddev);
    return 0;
}

static gx_device_buf_procs_t display_buf_procs = {
    display_create_buf_device,
    display_size_buf_device,
    gx_default_setup_buf_device,
    gx_default_destroy_buf_device
};

/* Allocate the backing bitmap. */
static int
display_alloc_bitmap(gx_device_display * ddev, gx_device * param_dev)
{
    int ccode;
    gx_device_buf_space_t buf_space;

    if (ddev->callback == NULL)
        return gs_error_Fatal;

    /* free old bitmap (if any) */
    display_free_bitmap(ddev);

    /* Initialise the clist/memory device specific fields. */
    memset(ddev->skip, 0, sizeof(ddev->skip));
    /* Calculate the size required for the a memory device. */
    display_size_buf_device(&buf_space, (gx_device *)ddev,
                            NULL, ddev->height, false);
    ddev->zBitmapSize = buf_space.bits + buf_space.line_ptrs;

    if (ddev->callback->version_major > DISPLAY_VERSION_MAJOR_V2 ||
        ddev->callback->display_rectangle_request != NULL) {
        /* Clist mode is a possibility. Maybe check in here whether
         * we want to suggest clist? */
        /* FIXME: For now, we'll just assume that the memalloc callback
         * is smart enough to make a sensible decision. */
    }

    /* allocate bitmap using an allocator not subject to GC */
    if (ddev->callback->display_memalloc
        && ddev->callback->display_memfree) {
        /* Note: For Planar buffers, we allocate the linepointers
         * as part of this allocation, just after the bitmap. Maybe we
         * want to allocate them ourselves, so they aren't exposed to
         * the caller? (Or the caller can pass in a pointer to a
         * structure of it's own without having to allow for these
         * pointers?)*/
        if (ddev->callback->version_major > DISPLAY_VERSION_MAJOR_V2)
            ddev->pBitmap = (*ddev->callback->display_memalloc)(ddev->pHandle,
                             ddev, ddev->zBitmapSize);
        else if (ddev->zBitmapSize > ARCH_MAX_ULONG)
            ddev->pBitmap = NULL;
        else {
            struct display_callback_v2_s *v2;
            v2 = (struct display_callback_v2_s *)(ddev->callback);
            ddev->pBitmap = (v2->display_memalloc)(ddev->pHandle,
                                 ddev, (unsigned long)ddev->zBitmapSize);
        }
    }
    else {
        ddev->pBitmap = gs_alloc_byte_array_immovable(ddev->memory->non_gc_memory,
                          ddev->zBitmapSize, 1, "display_alloc_bitmap");
    }

    if (ddev->pBitmap == NULL) {
        /* Bitmap failed to allocate. Can we recover by using rectangle
         * request mode? */
        if (ddev->callback->version_major <= DISPLAY_VERSION_MAJOR_V2 ||
            ddev->callback->display_rectangle_request == NULL) {
            /* No. Hard fail. */
            ddev->width = 0;
            ddev->height = 0;
            return_error(gs_error_VMerror);
        }
        /* Let's set up as a clist. */
        ccode = clist_mutate_to_clist((gx_device_clist_mutatable *)ddev,
                                      ddev->memory->non_gc_memory,
                                      NULL,
                                      &ddev->space_params,
                                      0,
                                      &display_buf_procs,
                                      ddev->orig_procs.dev_spec_op,
                                      MIN_BUFFER_SPACE);
        if (ccode >= 0) {
            ddev->initialize_device_procs = clist_initialize_device_procs;
            ddev->initialize_device_procs((gx_device *)ddev);
            /* ddev->initialize() has already been done, and does not
             * need to redone for the clist. */
            gx_device_fill_in_procs((gx_device *)ddev);
        }
    } else {
        /* Set up as PageMode. */
        gx_device *bdev = (gx_device *)ddev;

        /* Ensure we're not seen as a clist device. */
        ddev->buffer_space = 0;
        if ((ccode = gdev_create_buf_device
                 (display_create_buf_device,
                  &bdev, bdev, 0, NULL, NULL, NULL)) < 0 ||
                (ccode = gx_default_setup_buf_device
                 (bdev, ddev->pBitmap, buf_space.raster,
                  (byte **)((byte *)ddev->pBitmap + buf_space.bits), 0, ddev->height,
                  ddev->height)) < 0
                ) {
            /* Catastrophic. Shouldn't ever happen */
            display_free_bitmap(ddev);
            return_error(ccode);
        }
    }

#define COPY_PROC(p) set_dev_proc(ddev, p, ddev->orig_procs.p)
    COPY_PROC(get_initial_matrix);
    COPY_PROC(output_page);
    COPY_PROC(close_device);
    COPY_PROC(map_rgb_color);
    COPY_PROC(map_color_rgb);
    COPY_PROC(get_params);
    COPY_PROC(put_params);
    COPY_PROC(map_cmyk_color);
    set_dev_proc(ddev, get_page_device, gx_page_device_get_page_device);
    COPY_PROC(get_clipping_box);
    COPY_PROC(get_hardware_params);
    COPY_PROC(get_color_mapping_procs);
    COPY_PROC(get_color_comp_index);
    COPY_PROC(encode_color);
    COPY_PROC(decode_color);
    COPY_PROC(update_spot_equivalent_colors);
    COPY_PROC(ret_devn_params);
    /* This can be set from the memory device (planar) or target */
    if ( dev_proc(ddev, put_image) == gx_default_put_image )
        set_dev_proc(ddev, put_image, ddev->orig_procs.put_image);
#undef COPY_PROC

     /* Now, we want to hook various procs to give the callbacks
      * progress reports. But only in non-clist mode. */
    if (!CLIST_MUTATABLE_HAS_MUTATED(ddev)) {
        ddev->mutated_procs = ddev->procs;
        ddev->procs.fill_rectangle = display_fill_rectangle;
        ddev->procs.copy_mono = display_copy_mono;
        ddev->procs.copy_color = display_copy_color;
        ddev->procs.get_bits_rectangle = display_get_bits_rectangle;
    }

    /* In command list mode, we've already opened the device. */
    if (!CLIST_MUTATABLE_HAS_MUTATED(ddev)) {
        ccode = dev_proc(ddev, open_device)((gx_device *)ddev);
        if (ccode < 0)
            display_free_bitmap(ddev);
    }

    /* erase bitmap - before display gets redrawn */
    /*
     * Note that this will fill all 64 bits even if we've reset depth in the
     * devn case, since the underlying mdev is 64-bit (see above).
     */
    if (ccode == 0) {
        int i;
        gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
        for (i=0; i<GX_DEVICE_COLOR_MAX_COMPONENTS; i++)
            cv[i] = (ddev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
                ? gx_max_color_value : 0;
        dev_proc(ddev, fill_rectangle)((gx_device *)ddev,
                 0, 0, ddev->width, ddev->height,
                 dev_proc(ddev, encode_color)((gx_device *)ddev, cv));
    }

    return ccode;
}

static int
display_set_separations(gx_device_display *dev)
{
    if (((dev->nFormat & DISPLAY_COLORS_MASK) == DISPLAY_COLORS_SEPARATION) &&
        (dev->callback->version_major > DISPLAY_VERSION_MAJOR_V1) &&
        (dev->callback->display_separation != NULL)) {
        /* Tell the client about the separation to composite mapping */
        char name[64];
        int num_spot = dev->devn_params.separations.num_separations;
        int num_std_colorants = dev->devn_params.num_std_colorant_names;
        int num_comp = num_std_colorants + num_spot;
        int comp_map[GX_DEVICE_COLOR_MAX_COMPONENTS];
        int comp_num;
        int sep_num;
        int sep_name_size;
        unsigned int c, m, y, k;
        gx_device_display *head = dev;

        if (num_comp > GX_DEVICE_COLOR_MAX_COMPONENTS)
            num_comp = GX_DEVICE_COLOR_MAX_COMPONENTS;

        while(head->parent)
            head = (gx_device_display *)head->parent;

        /* Map the separation numbers to component numbers */
        memset(comp_map, 0, sizeof(comp_map));
        for (sep_num = 0; sep_num < num_comp; sep_num++) {
            comp_num = dev->devn_params.separation_order_map[sep_num];
            if (comp_num >= 0 && comp_num < GX_DEVICE_COLOR_MAX_COMPONENTS)
                comp_map[comp_num] = sep_num;
        }
        /* For each component, tell the client the separation mapping */
        for (comp_num = 0; comp_num < num_comp; comp_num++) {
            c = y = m = k = 0;
            sep_num = comp_map[comp_num];
            /* Get the CMYK equivalent */
            if (sep_num < dev->devn_params.num_std_colorant_names) {
                sep_name_size =
                    strlen(dev->devn_params.std_colorant_names[sep_num]);
                if (sep_name_size > sizeof(name)-2)
                    sep_name_size = sizeof(name)-1;
                memcpy(name, dev->devn_params.std_colorant_names[sep_num],
                    sep_name_size);
                name[sep_name_size] = '\0';
                switch (sep_num) {
                    case 0: c = 65535; break;
                    case 1: m = 65535; break;
                    case 2: y = 65535; break;
                    case 3: k = 65535; break;
                }
            }
            else {
                sep_num -= dev->devn_params.num_std_colorant_names;
                sep_name_size =
                    dev->devn_params.separations.names[sep_num].size;
                if (sep_name_size > sizeof(name)-2)
                    sep_name_size = sizeof(name)-1;
                memcpy(name, dev->devn_params.separations.names[sep_num].data,
                    sep_name_size);
                name[sep_name_size] = '\0';
                if (dev->equiv_cmyk_colors.color[sep_num].color_info_valid) {
                    c = dev->equiv_cmyk_colors.color[sep_num].c
                           * 65535 / frac_1;
                    m = dev->equiv_cmyk_colors.color[sep_num].m
                           * 65535 / frac_1;
                    y = dev->equiv_cmyk_colors.color[sep_num].y
                           * 65535 / frac_1;
                    k = dev->equiv_cmyk_colors.color[sep_num].k
                           * 65535 / frac_1;
                }
            }
            (*head->callback->display_separation)(dev->pHandle, head,
                comp_num, name,
                (unsigned short)c, (unsigned short)m,
                (unsigned short)y, (unsigned short)k);
        }
    }
    return 0;
}

typedef enum DISPLAY_MODEL_e {
    DISPLAY_MODEL_GRAY=0,
    DISPLAY_MODEL_RGB=1,
    DISPLAY_MODEL_RGBK=2,
    DISPLAY_MODEL_CMYK=3,
    DISPLAY_MODEL_SEP=4
} DISPLAY_MODEL;

/*
 * This is a utility routine to build the display device's color_info
 * structure (except for the anti alias info).
 */
static void
set_color_info(gx_device_color_info * pdci, DISPLAY_MODEL model,
    int nc, int maxc, int depth, int maxgray, int maxcolor)
{
    pdci->num_components = nc;
    pdci->max_components = maxc;
    pdci->depth = depth;
    pdci->gray_index = 0;
    pdci->max_gray = maxgray;
    pdci->max_color = maxcolor;
    pdci->dither_grays = maxgray + 1;
    pdci->dither_colors = maxcolor + 1;
    pdci->separable_and_linear = GX_CINFO_UNKNOWN_SEP_LIN;
    switch (model) {
        case DISPLAY_MODEL_GRAY:
            pdci->polarity = GX_CINFO_POLARITY_ADDITIVE;
            pdci->cm_name = "DeviceGray";
            pdci->gray_index = 0;
            break;
        case DISPLAY_MODEL_RGB:
            pdci->polarity = GX_CINFO_POLARITY_ADDITIVE;
            pdci->cm_name = "DeviceRGB";
            pdci->gray_index = GX_CINFO_COMP_NO_INDEX;
            break;
        case DISPLAY_MODEL_RGBK:
            pdci->polarity = GX_CINFO_POLARITY_ADDITIVE;
            pdci->cm_name = "DeviceRGBK";
            pdci->gray_index = 3;
            break;
        case DISPLAY_MODEL_CMYK:
            pdci->polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
            pdci->cm_name = "DeviceCMYK";
            pdci->gray_index = 3;
            break;
        default:
        case DISPLAY_MODEL_SEP:
            /* Anything else is separations */
            pdci->polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
            pdci->cm_name = "DeviceCMYK";
            pdci->gray_index = GX_CINFO_COMP_NO_INDEX; /* may not have K */
            break;
    }
}

/*
 * This is an utility routine to set up the color procs for the display
 * device.  The display device can change its setup.
 */
static void
set_color_procs(gx_device * pdev,
        dev_proc_encode_color((*encode_color)),
        dev_proc_decode_color((*decode_color)),
        dev_proc_get_color_mapping_procs((*get_color_mapping_procs)),
        dev_proc_get_color_comp_index((*get_color_comp_index)),
        dev_proc_fill_rectangle_hl_color((*fill_hl_color)))
{
    pdev->procs.get_color_mapping_procs = get_color_mapping_procs;
    pdev->procs.get_color_comp_index = get_color_comp_index;
    pdev->procs.encode_color = encode_color;
    pdev->procs.decode_color = decode_color;
    pdev->procs.fill_rectangle_hl_color = fill_hl_color;
}

/*
 * This is an utility routine to set up the color procs for the display
 * device.  This routine is used when the display device is Gray.
 */
static void
set_gray_color_procs(gx_device * pdev,
        dev_t_proc_encode_color((*encode_color), gx_device),
        dev_t_proc_decode_color((*decode_color), gx_device))
{
    set_color_procs(pdev, encode_color, decode_color,
        gx_default_DevGray_get_color_mapping_procs,
        gx_default_DevGray_get_color_comp_index,
        gx_default_fill_rectangle_hl_color);
}

/*
 * This is an utility routine to set up the color procs for the display
 * device.  This routine is used when the display device is RGB.
 */
static void
set_rgb_color_procs(gx_device * pdev,
        dev_t_proc_encode_color((*encode_color), gx_device),
        dev_t_proc_decode_color((*decode_color), gx_device))
{
    set_color_procs(pdev, encode_color, decode_color,
        gx_default_DevRGB_get_color_mapping_procs,
        gx_default_DevRGB_get_color_comp_index,
        gx_default_fill_rectangle_hl_color);
}

/*
 * This is an utility routine to set up the color procs for the display
 * device.  This routine is used when the display device is RGBK.
 */
static void
set_rgbk_color_procs(gx_device * pdev,
        dev_t_proc_encode_color((*encode_color), gx_device),
        dev_t_proc_decode_color((*decode_color), gx_device))
{
    set_color_procs(pdev, encode_color, decode_color,
        gx_default_DevRGBK_get_color_mapping_procs,
        gx_default_DevRGBK_get_color_comp_index,
        gx_default_fill_rectangle_hl_color);
}

/*
 * This is an utility routine to set up the color procs for the display
 * device.  This routine is used when the display device is CMYK.
 */
static void
set_cmyk_color_procs(gx_device * pdev,
        dev_t_proc_encode_color((*encode_color), gx_device),
        dev_t_proc_decode_color((*decode_color), gx_device))
{
    set_color_procs(pdev, encode_color, decode_color,
        gx_default_DevCMYK_get_color_mapping_procs,
        gx_default_DevCMYK_get_color_comp_index,
        gx_default_fill_rectangle_hl_color);
}

/* Set the color_info and mapping functions for this instance of the device */
static int
display_set_color_format(gx_device_display *ddev, int nFormat)
{
    gx_device * pdev = (gx_device *) ddev;
    gx_device_color_info dci = ddev->color_info;
    int bpc;	/* bits per component */
    int bpp;	/* bits per pixel */
    int maxvalue;
    int align;
    int npp;

    switch (nFormat & DISPLAY_DEPTH_MASK) {
        case DISPLAY_DEPTH_1:
            bpc = 1;
            break;
        case DISPLAY_DEPTH_2:
            bpc = 2;
            break;
        case DISPLAY_DEPTH_4:
            bpc = 4;
            break;
        case DISPLAY_DEPTH_8:
            bpc = 8;
            break;
        case DISPLAY_DEPTH_12:
            bpc = 12;
            break;
        case DISPLAY_DEPTH_16:
            bpc = 16;
            break;
        default:
            return_error(gs_error_rangecheck);
    }
    maxvalue = (1 << bpc) - 1;
    ddev->devn_params.bitspercomponent = bpc;

    switch (ddev->nFormat & DISPLAY_ROW_ALIGN_MASK) {
        case DISPLAY_ROW_ALIGN_DEFAULT:
            align = ARCH_ALIGN_PTR_MOD;
            if (sizeof(void *) == 4)
                ddev->log2_align_mod = 2;
            else
                ddev->log2_align_mod = 3;
            break;
        case DISPLAY_ROW_ALIGN_4:
            align = 4;
            ddev->log2_align_mod = 2;
            break;
        case DISPLAY_ROW_ALIGN_8:
            align = 8;
            ddev->log2_align_mod = 3;
            break;
        case DISPLAY_ROW_ALIGN_16:
            align = 16;
            ddev->log2_align_mod = 4;
            break;
        case DISPLAY_ROW_ALIGN_32:
            align = 32;
            ddev->log2_align_mod = 5;
            break;
        case DISPLAY_ROW_ALIGN_64:
            align = 64;
            ddev->log2_align_mod = 6;
            break;
        default:
            align = 0;	/* not permitted */
    }
    if (align < ARCH_ALIGN_PTR_MOD)
        return_error(gs_error_rangecheck);

    switch (ddev->nFormat & DISPLAY_ALPHA_MASK) {
        case DISPLAY_ALPHA_FIRST:
        case DISPLAY_ALPHA_LAST:
            /* Not implemented and unlikely to ever be implemented
             * because they would interact with linear_and_separable
             */
            return_error(gs_error_rangecheck);
    }

    switch (nFormat & (DISPLAY_PLANAR | DISPLAY_PLANAR_INTERLEAVED))
    {
        case DISPLAY_CHUNKY:
            npp = 0;
            break;
        default:
            npp = 1;
            break;
    }

    switch (nFormat & DISPLAY_COLORS_MASK) {
        case DISPLAY_COLORS_NATIVE:
            switch (nFormat & DISPLAY_DEPTH_MASK) {
                case DISPLAY_DEPTH_1:
                    /* 1bit/pixel, black is 1, white is 0 */
                    set_color_info(&dci, DISPLAY_MODEL_GRAY, 1, 1, 1, 1, 0);
                    dci.separable_and_linear = GX_CINFO_SEP_LIN_NONE;
                    set_gray_color_procs(pdev, gx_b_w_gray_encode,
                                                gx_default_b_w_map_color_rgb);
                    break;
                case DISPLAY_DEPTH_4:
                    /* 4bit/pixel VGA color */
                    set_color_info(&dci, DISPLAY_MODEL_RGB, 3, 3, 4, 3, 2);
                    dci.separable_and_linear = GX_CINFO_SEP_LIN_NONE;
                    set_rgb_color_procs(pdev, display_map_rgb_color_device4,
                                                display_map_color_rgb_device4);
                    break;
                case DISPLAY_DEPTH_8:
                    /* 8bit/pixel 96 color palette */
                    set_color_info(&dci, DISPLAY_MODEL_RGBK, 4, 4, 8, 31, 3);
                    dci.separable_and_linear = GX_CINFO_SEP_LIN_NONE;
                    set_rgbk_color_procs(pdev, display_encode_color_device8,
                                                display_decode_color_device8);
                    break;
                case DISPLAY_DEPTH_16:
                    /* Windows 16-bit display */
                    /* Is maxgray = maxcolor = 63 correct? */
                    if ((ddev->nFormat & DISPLAY_555_MASK)
                        == DISPLAY_NATIVE_555)
                        set_color_info(&dci, DISPLAY_MODEL_RGB, 3, 3, 16, 31, 31);
                    else
                        set_color_info(&dci, DISPLAY_MODEL_RGB, 3, 3, 16, 63, 63);
                    set_rgb_color_procs(pdev, display_map_rgb_color_device16,
                                                display_map_color_rgb_device16);
                    break;
                default:
                    return_error(gs_error_rangecheck);
            }
            dci.gray_index = GX_CINFO_COMP_NO_INDEX;
            break;
        case DISPLAY_COLORS_GRAY:
            set_color_info(&dci, DISPLAY_MODEL_GRAY, 1, 1, bpc, maxvalue, 0);
            if (bpc == 1)
                set_gray_color_procs(pdev, gx_default_gray_encode,
                                                gx_default_w_b_map_color_rgb);
            else
                set_gray_color_procs(pdev, gx_default_gray_encode,
                                                gx_default_gray_map_color_rgb);
            break;
        case DISPLAY_COLORS_RGB:
            if ((nFormat & DISPLAY_ALPHA_MASK) == DISPLAY_ALPHA_NONE)
                bpp = bpc * 3;
            else
                bpp = bpc * 4;
            set_color_info(&dci, DISPLAY_MODEL_RGB, 3, 3, bpp, maxvalue, maxvalue);
            if (((nFormat & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) &&
                ((nFormat & DISPLAY_ALPHA_MASK) == DISPLAY_ALPHA_NONE)) {
                if ((nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN)
                    set_rgb_color_procs(pdev, gx_default_rgb_map_rgb_color,
                                                gx_default_rgb_map_color_rgb);
                else
                    set_rgb_color_procs(pdev, display_map_rgb_color_bgr24,
                                                display_map_color_rgb_bgr24);
            }
            else {
                /* slower flexible functions for alpha/unused component */
                set_rgb_color_procs(pdev, display_map_rgb_color_rgb,
                                                display_map_color_rgb_rgb);
            }
            break;
        case DISPLAY_COLORS_CMYK:
            bpp = bpc * 4;
            set_color_info(&dci, DISPLAY_MODEL_CMYK, 4, 4, bpp, maxvalue, maxvalue);
            if ((nFormat & DISPLAY_ALPHA_MASK) != DISPLAY_ALPHA_NONE)
                return_error(gs_error_rangecheck);
            if ((nFormat & DISPLAY_ENDIAN_MASK) != DISPLAY_BIGENDIAN)
                return_error(gs_error_rangecheck);

            if ((nFormat & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_1)
                set_cmyk_color_procs(pdev, cmyk_1bit_map_cmyk_color,
                                                cmyk_1bit_map_color_cmyk);
            else if ((nFormat & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8)
                set_cmyk_color_procs(pdev, cmyk_8bit_map_cmyk_color,
                                                cmyk_8bit_map_color_cmyk);
            else
                return_error(gs_error_rangecheck);
            break;
        case DISPLAY_COLORS_SEPARATION:
            if ((nFormat & DISPLAY_ENDIAN_MASK) != DISPLAY_BIGENDIAN)
                return_error(gs_error_rangecheck);
            if (ddev->num_planar_planes)
            {
                int n;
                if (ddev->devn_params.separations.num_separations == 0)
                    n = GS_CLIENT_COLOR_MAX_COMPONENTS;
                else {
                    n = ddev->devn_params.num_std_colorant_names + ddev->devn_params.separations.num_separations;
                    if (n == 0)
                        n = GS_CLIENT_COLOR_MAX_COMPONENTS;
                    if (n > GS_CLIENT_COLOR_MAX_COMPONENTS)
                        n = GS_CLIENT_COLOR_MAX_COMPONENTS;
                }
                bpp = n * 8;
                set_color_info(&dci, DISPLAY_MODEL_SEP, n, GS_CLIENT_COLOR_MAX_COMPONENTS, bpp,
                    maxvalue, maxvalue);
            }
            else
            {
                bpp = ARCH_SIZEOF_COLOR_INDEX * 8;
                set_color_info(&dci, DISPLAY_MODEL_SEP, bpp/bpc, bpp/bpc, bpp,
                    maxvalue, maxvalue);
            }
            if ((nFormat & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
                ddev->devn_params.bitspercomponent = bpc;
                if (ddev->icc_struct == NULL) {
                    ddev->icc_struct = gsicc_new_device_profile_array((gx_device *)ddev);
                    if (ddev->icc_struct == NULL)
                        return_error(gs_error_VMerror);
                }
                ddev->icc_struct->supports_devn = true;
                set_color_procs(pdev,
                    display_separation_encode_color,
                    display_separation_decode_color,
                    display_separation_get_color_mapping_procs,
                    display_separation_get_color_comp_index,
                    display_fill_rectangle_hl_color);
            }
            else
                return_error(gs_error_rangecheck);
            break;
        default:
            return_error(gs_error_rangecheck);
    }

    /* restore old anti_alias info */
    dci.anti_alias = ddev->color_info.anti_alias;
    ddev->color_info = dci;
    ddev->num_planar_planes = npp ? ddev->color_info.num_components : 0;
    check_device_separable(pdev);
    switch (nFormat & DISPLAY_COLORS_MASK) {
        case DISPLAY_COLORS_NATIVE:
            ddev->color_info.gray_index = GX_CINFO_COMP_NO_INDEX;
            if ((nFormat & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_1)
                ddev->color_info.gray_index = 0;
            else if ((nFormat & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8)
                ddev->color_info.gray_index = 3;
            break;
        case DISPLAY_COLORS_RGB:
            ddev->color_info.gray_index = GX_CINFO_COMP_NO_INDEX;
            break;
        case DISPLAY_COLORS_GRAY:
            ddev->color_info.gray_index = 0;
            break;
        case DISPLAY_COLORS_CMYK:
            ddev->color_info.gray_index = 3;
            break;
        case DISPLAY_COLORS_SEPARATION:
            ddev->color_info.gray_index = GX_CINFO_COMP_NO_INDEX;
            break;
    }
    ddev->nFormat = nFormat;

    return 0;
}

/* ------ Begin Test Code ------ */

/*********************************************************************
typedef struct test_mode_s test_mode;
struct test_mode_s {
    char *name;
    unsigned int format;
};

test_mode test_modes[] = {
    {"1bit/pixel native, black is 1, Windows",
     DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_1 |
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST},
    {"4bit/pixel native, Windows VGA 16 color palette",
     DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_4 |
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST},
    {"8bit/pixel native, Windows SVGA 96 color palette",
     DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 |
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST},
    {"16bit/pixel native, Windows BGR555",
     DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_16 |
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST | DISPLAY_NATIVE_555},
    {"16bit/pixel native, Windows BGR565",
     DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_16 |
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST | DISPLAY_NATIVE_565},
    {"1bit/pixel gray, black is 0, topfirst",
     DISPLAY_COLORS_GRAY | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_1 |
     DISPLAY_BIGENDIAN | DISPLAY_TOPFIRST},
    {"4bit/pixel gray, bottom first",
     DISPLAY_COLORS_GRAY | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_4 |
     DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST},
    {"8bit/pixel gray, bottom first",
     DISPLAY_COLORS_GRAY | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 |
     DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST},
    {"24bit/pixel color, bottom first, Windows BGR24",
     DISPLAY_COLORS_RGB | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 |
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST},
    {"24bit/pixel color, bottom first, RGB24",
     DISPLAY_COLORS_RGB | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 |
     DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST},
    {"24bit/pixel color, top first, GdkRgb RGB24",
     DISPLAY_COLORS_RGB | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 |
     DISPLAY_BIGENDIAN | DISPLAY_TOPFIRST},
    {"32bit/pixel color, top first, Macintosh xRGB",
     DISPLAY_COLORS_RGB | DISPLAY_UNUSED_FIRST | DISPLAY_DEPTH_8 |
     DISPLAY_BIGENDIAN | DISPLAY_TOPFIRST},
    {"32bit/pixel color, bottom first, xBGR",
     DISPLAY_COLORS_RGB | DISPLAY_UNUSED_FIRST | DISPLAY_DEPTH_8 |
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST},
    {"32bit/pixel color, bottom first, Windows BGRx",
     DISPLAY_COLORS_RGB | DISPLAY_UNUSED_LAST | DISPLAY_DEPTH_8 |
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST},
    {"32bit/pixel color, bottom first, RGBx",
     DISPLAY_COLORS_RGB | DISPLAY_UNUSED_LAST | DISPLAY_DEPTH_8 |
     DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST},
    {"32bit/pixel CMYK, bottom first",
     DISPLAY_COLORS_CMYK | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 |
     DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST},
    {"64bit/pixel separations, bottom first",
     DISPLAY_COLORS_SEPARATIONS | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 |
     DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST},
    {"4bit/pixel CMYK, bottom first",
     DISPLAY_COLORS_CMYK | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_1 |
     DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST},
    {"1bit/pixel native, black is 1, 8 byte alignment",
     DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_1 |
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST | DISPLAY_ROW_ALIGN_8},
    {"24bit/pixel color, bottom first, BGR24, 64 byte alignment",
     DISPLAY_COLORS_RGB | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 |
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST | DISPLAY_ROW_ALIGN_64}
};

void
test(int index)
{
    char buf[1024];
    sprintf(buf, "gs -dDisplayFormat=16#%x examples/colorcir.ps -c quit", test_modes[index].format);
    system(buf);
}

int main(int argc, char *argv[])
{
    int i;
    int dotest = 0;
    if (argc >=2) {
        if (strcmp(argv[1], "-t") == 0)
            dotest = 1;
        else {
            fprintf(stdout, "To show modes: disp\nTo run test: disp -t\n");
            return 1;
        }
    }
    for (i=0; i < sizeof(test_modes)/sizeof(test_mode); i++) {
        fprintf(stdout, "16#%x or %d: %s\n", test_modes[i].format,
                test_modes[i].format, test_modes[i].name);
        if (dotest)
            test(i);
    }
    return 0;
}
*********************************************************************/

/* ------ End Test Code ------ */
