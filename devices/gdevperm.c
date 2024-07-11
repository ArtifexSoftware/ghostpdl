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


/* Device which permutes color components, for testing DeviceN. */
#include "gdevprn.h"
#include "gxdcconv.h"

/**
 * With no additional parameters, the device named "permute" looks to
 * Ghostscript like a standard CMYK contone device, and outputs a PPM
 * file, using a simple CMYK->RGB transform. This should be the
 * baseline for regression testing.
 *
 * With the addition of -dPermute=1, the internal behavior changes
 * somewhat, but in most cases the resulting rendered file should be
 * the same. In this mode, the color model becomes "DeviceN" rather
 * than "DeviceCMYK", the number of components goes to six, and the
 * color model is considered to be the (yellow, cyan, cyan, magenta,
 * 0, black) tuple. This is what's rendered into the memory
 * buffer. Finally, on conversion to RGB for output, the colors are
 * permuted back.
 *
 * As such, this code should check that all imaging code paths are
 * 64-bit clean. Additionally, it should find incorrect code that
 * assumes that the color model is one of DeviceGray, DeviceRGB, or
 * DeviceCMYK.
 **/

static dev_proc_print_page(perm_print_page);
static dev_proc_get_params(perm_get_params);
static dev_proc_put_params(perm_put_params);
static dev_proc_get_color_mapping_procs(perm_get_color_mapping_procs);
static dev_proc_get_color_comp_index(perm_get_color_comp_index);
static dev_proc_encode_color(perm_encode_color);
static dev_proc_decode_color(perm_decode_color);

struct gx_device_perm_s {
    gx_device_common;
    gx_prn_device_common;
    const char **std_colorant_names;
    int num_std_colorant_names;	/* Number of names in list */
    int mode;
    int permute;
};
typedef struct gx_device_perm_s gx_device_perm_t;

static void
perm_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_bg(dev);

    set_dev_proc(dev, get_params, perm_get_params);
    set_dev_proc(dev, put_params, perm_put_params);
    set_dev_proc(dev, get_color_mapping_procs, perm_get_color_mapping_procs);
    set_dev_proc(dev, get_color_comp_index, perm_get_color_comp_index);
    set_dev_proc(dev, encode_color, perm_encode_color);
    set_dev_proc(dev, decode_color, perm_decode_color);
}

const gx_device_perm_t gs_perm_device = {
    prn_device_body_extended(gx_device_perm_t,
        perm_initialize_device_procs, "permute",
        DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS, 72, 72,
        0, 0, 0, 0,
        GX_DEVICE_COLOR_MAX_COMPONENTS, 4,
        GX_CINFO_POLARITY_SUBTRACTIVE,
        32, 0, 255, 255, 256, 256,
        GX_CINFO_SEP_LIN,
        "DeviceN",
        perm_print_page),
    NULL, 0, 0, 0
};

static int
perm_print_page(gx_device_printer *pdev, gp_file *pstream)
{
    int y;
    gx_device_perm_t * const dev = (gx_device_perm_t *)pdev;
    int ncomp = dev->num_std_colorant_names;
    int raw_raster = pdev->width * ncomp;
    byte *raw_line = NULL;
    byte *cooked_line = NULL;
    byte *row;
    int code = 0;
    int mode = dev->mode;
    int permute = dev->permute;

    fprintf(pstream, "P6\n%d %d\n255\n", dev->width, dev->height);
    raw_line = gs_alloc_bytes(pdev->memory, raw_raster, "perm_print_page");
    cooked_line = gs_alloc_bytes(pdev->memory, dev->width * 3, "perm_print_page");
    if (raw_line == NULL || cooked_line == NULL) {
        gs_free_object(pdev->memory, raw_line, "perm_print_page");
        return_error(gs_error_VMerror);
    }
    for (y = 0; y < dev->height; y++) {
        int x;
        code = gdev_prn_get_bits(pdev, y, raw_line, &row);
        if (code < 0)
            break;
        for (x = 0; x < dev->width; x++) {
            int c, m, y, k;
            int r, g, b;

            if (mode == 0) {
                if (permute) {
                    c = row[x * ncomp + 1];
                    m = row[x * ncomp + 3];
                    y = row[x * ncomp + 0];
                    k = row[x * ncomp + 5];
                } else {
                    c = row[x * ncomp];
                    m = row[x * ncomp + 1];
                    y = row[x * ncomp + 2];
                    k = row[x * ncomp + 3];
                }
            } else /* if (mode == 1) */ {
                if (permute) {
                    c = row[x * ncomp + 1];
                    m = row[x * ncomp + 3];
                    y = row[x * ncomp + 0];
                    k = 0;
                } else {
                    c = row[x * ncomp];
                    m = row[x * ncomp + 1];
                    y = row[x * ncomp + 2];
                    k = 0;
                }
            }
            r = (255 - c) * (255 - k) / 255;
            g = (255 - m) * (255 - k) / 255;
            b = (255 - y) * (255 - k) / 255;
            cooked_line[x * 3] = r;
            cooked_line[x * 3 + 1] = g;
            cooked_line[x * 3 + 2] = b;
        }
        gp_fwrite(cooked_line, 1, dev->width * 3, pstream);
    }
    gs_free_object(pdev->memory, cooked_line, "perm_print_page");
    gs_free_object(pdev->memory, raw_line, "perm_print_page");
    return code;
}

static void
perm_permute_cm(gx_device *pdev, frac out[])
{
    gx_device_perm_t * const dev = (gx_device_perm_t *)pdev;
    if (dev->permute) {
        frac y;
        out[5] = dev->mode == 0 ? out[3] : 0;
        out[4] = frac_0;
        y = out[2];
        out[3] = out[1];
        out[2] = out[0];
        out[1] = out[0];
        out[0] = y;
    }
}

static void
gray_cs_to_perm_cm_0(gx_device *dev, frac gray, frac out[])
{
    out[0] = out[1] = out[2] = frac_0;
    out[3] = frac_1 - gray;
    perm_permute_cm(dev, out);
}

static void
rgb_cs_to_perm_cm_0(gx_device *dev, const gs_gstate *pgs,
                                  frac r, frac g, frac b, frac out[])
{
    color_rgb_to_cmyk(r, g, b, pgs, out, dev->memory);
    perm_permute_cm(dev, out);
}

static void
cmyk_cs_to_perm_cm_0(gx_device *dev, frac c, frac m, frac y, frac k, frac out[])
{
    out[0] = c;
    out[1] = m;
    out[2] = y;
    out[3] = k;
    perm_permute_cm(dev, out);
};

static void
gray_cs_to_perm_cm_1(gx_device *dev, frac gray, frac out[])
{
    out[0] = out[1] = out[2] = frac_1 - gray;
    perm_permute_cm(dev, out);
}

static void
rgb_cs_to_perm_cm_1(gx_device *dev, const gs_gstate *pgs,
                                  frac r, frac g, frac b, frac out[])
{
    out[0] = frac_1 - r;
    out[1] = frac_1 - g;
    out[2] = frac_1 - b;
    perm_permute_cm(dev, out);
}

static void
cmyk_cs_to_perm_cm_1(gx_device *dev, frac c, frac m, frac y, frac k, frac out[])
{
    color_cmyk_to_rgb(c, m, y, k, NULL, out, dev->memory);
    out[0] = frac_1 - out[0];
    out[1] = frac_1 - out[1];
    out[2] = frac_1 - out[2];
    perm_permute_cm(dev, out);
};

static const gx_cm_color_map_procs perm_cmapping_procs_0 = {
    gray_cs_to_perm_cm_0, rgb_cs_to_perm_cm_0, cmyk_cs_to_perm_cm_0
};

static const gx_cm_color_map_procs perm_cmapping_procs_1 = {
    gray_cs_to_perm_cm_1, rgb_cs_to_perm_cm_1, cmyk_cs_to_perm_cm_1
};

static const gx_cm_color_map_procs *perm_cmapping_procs[] = {
    &perm_cmapping_procs_0,
    &perm_cmapping_procs_1
};

static const gx_cm_color_map_procs *
perm_get_color_mapping_procs(const gx_device *dev)
{
    const gx_device_perm_t * const pdev = (const gx_device_perm_t *)dev;

    if (pdev->mode < 0 || pdev->mode >= sizeof(perm_cmapping_procs) / sizeof(perm_cmapping_procs[0]))
        return NULL;
    return perm_cmapping_procs[pdev->mode];
}

#define compare_color_names(name, name_size, str, str_size) \
    (name_size == str_size && \
        (strncmp((const char *)name, (const char *)str, name_size) == 0))

static int
perm_get_color_comp_index(const gx_device *pdev, const char *pname,
                                        int name_size, int component_type)
{
    const gx_device_perm_t * const dev = (const gx_device_perm_t *)pdev;
    int n_separation_names = dev->num_std_colorant_names;
    int i;

    for (i = 0; i < n_separation_names; i++) {
        const char *sep_name = dev->std_colorant_names[i];
        if (compare_color_names(pname, name_size, sep_name, strlen(sep_name)))
            return i;
    }
    return -1;
}

/* Note: the encode and decode procs are entirely standard. The
   permutation is all done in the color space to color model mapping.
   In fact, we could probably just use the default here.
*/

/*
 * Encode a list of colorant values into a gx_color_index_value.
 */
static gx_color_index
perm_encode_color(gx_device *dev, const gx_color_value colors[])
{
    int bpc = 8;
    gx_color_index color = 0;
    int i = 0;
    int ncomp = dev->color_info.num_components;
    COLROUND_VARS;

    COLROUND_SETUP(bpc);
    for (; i<ncomp; i++) {
        color <<= bpc;
        color |= COLROUND_ROUND(colors[i]);
    }
    return (color == gx_no_color_index ? color ^ 1 : color);
}

/*
 * Decode a gx_color_index value back to a list of colorant values.
 */
static int
perm_decode_color(gx_device *dev, gx_color_index color, gx_color_value *out)
{
    int bpc = 8;
    int drop = sizeof(gx_color_value) * 8 - bpc;
    int mask = (1 << bpc) - 1;
    int i = 0;
    int ncomp = dev->color_info.num_components;
    COLDUP_VARS;

    COLDUP_SETUP(bpc);
    for (; i<ncomp; i++) {
        out[ncomp - i - 1] = COLDUP_DUP(color & mask);
        color >>= bpc;
    }
    return 0;
}

#define set_param_array(a, d, s)\
  (a.data = d, a.size = s, a.persistent = false);

static int
perm_get_params(gx_device *pdev, gs_param_list *plist)
{
    gx_device_perm_t * const dev = (gx_device_perm_t *)pdev;
    int code;

    code = param_write_int(plist, "Permute", &dev->permute);
    if (code >= 0)
        code = param_write_int(plist, "Mode", &dev->mode);
    /*
     * We need to specify the SeparationColorNames if we are permuting the colors.
     */
    if (code >= 0 && dev->permute == 1) {
        int i;
        /* Temp variables.  The data is copied into the plist below. */
        gs_param_string_array scna;
        gs_param_string scn[6];

        set_param_array(scna, scn, dev->num_std_colorant_names);
        /* Place colorant names into string array elements */
        for (i = 0; i < dev->num_std_colorant_names; i++)
            param_string_from_string(scn[i], dev->std_colorant_names[i]);
        /*
         * Place the name array in the plist.  This includes allocating
         * memory for the name array element and the actual string array.
         */
        code = param_write_name_array(plist, "SeparationColorNames", &scna);
    }
    if (code >= 0)
        code = gdev_prn_get_params(pdev, plist);
    return code;
}

#undef set_param_array

static const char * DeviceCMYKComponents[] = {
        "Cyan",
        "Magenta",
        "Yellow",
        "Black",
        0		/* List terminator */
};

static const char * DeviceCMYComponents[] = {
        "Cyan",
        "Magenta",
        "Yellow",
        0		/* List terminator */
};

static const char * DeviceNComponents[] = {
        "Yellow",
        "Cyan",
        "Cyan2",
        "Magenta",
        "Zero",
        "Black",
        0		/* List terminator */
};

static int
perm_set_color_model(gx_device_perm_t *dev, int mode, int permute)
{
    dev->mode = mode;
    dev->permute = permute;
    if (mode == 0 && permute == 0) {
        dev->std_colorant_names = DeviceCMYKComponents;
        dev->num_std_colorant_names = 4;
        dev->color_info.cm_name = "DeviceCMYK";
        dev->color_info.polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
    } else if (mode == 0 && permute == 1) {
        dev->std_colorant_names = DeviceNComponents;
        dev->num_std_colorant_names = 6;
        dev->color_info.cm_name = "DeviceN";
        dev->color_info.polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
    } else if (mode == 1 && permute == 0) {
        dev->std_colorant_names = DeviceCMYComponents;
        dev->num_std_colorant_names = 3;
        dev->color_info.cm_name = "DeviceCMY";
        dev->color_info.polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
    } else if (mode == 1 && permute == 1) {
        dev->std_colorant_names = DeviceNComponents;
        dev->num_std_colorant_names = 6;
        dev->color_info.cm_name = "DeviceN";
        dev->color_info.polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
    } else {
        return -1;
    }
    dev->color_info.num_components = dev->num_std_colorant_names;
    dev->color_info.depth = 8 * dev->num_std_colorant_names;

    return 0;
}

static int
perm_put_params(gx_device *pdev, gs_param_list *plist)
{
    gx_device_perm_t * const dev = (gx_device_perm_t *)pdev;
    gx_device_color_info save_info;
    int code;
    int new_permute = dev->permute;
    int new_mode = dev->mode;

    code = param_read_int(plist, "Permute", &new_permute);
    if (code < 0)
        return code;
    code = param_read_int(plist, "Mode", &new_mode);
    if (code < 0)
        return code;
    if (new_mode < 0 || new_mode >= sizeof(perm_cmapping_procs) / sizeof(perm_cmapping_procs[0])) {
        dmlprintf(pdev->memory, "rangecheck!\n");
        return_error(gs_error_rangecheck);
    }
    dev->permute = new_permute;
    dev->mode = new_mode;
    save_info = pdev->color_info;
    code = perm_set_color_model(dev, dev->mode, dev->permute);
    if (code >= 0)
        code = gdev_prn_put_params(pdev, plist);
    if (code < 0)
        pdev->color_info = save_info;
    return code;
}
