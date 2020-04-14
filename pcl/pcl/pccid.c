/* Copyright (C) 2001-2020 Artifex Software, Inc.
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


/* pccid.c - PCL configure image data command and  object implementation */
#include "gx.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcpalet.h"
#include "pccid.h"

/* CID accessors */
pcl_cspace_type_t
pcl_cid_get_cspace(const pcl_cid_data_t * pcid)
{
    return pcid->u.hdr.cspace;
}

pcl_encoding_type_t
pcl_cid_get_encoding(const pcl_cid_data_t * pcid)
{
    return pcid->u.hdr.encoding;
}

byte
pcl_cid_get_bits_per_index(const pcl_cid_data_t * pcid)
{
    return pcid->u.hdr.bits_per_index;
}

byte
pcl_cid_get_bits_per_primary(const pcl_cid_data_t * pcid, int index)
{
    return pcid->u.hdr.bits_per_primary[index];
}
#ifdef DEBUG

#define WHITE_CS "White"

static const char *const pcl_csnames[] = {
    "RGB", "CMY",
    "Colorimetric", "CIE",
    "LumChrom"
};

static const char *const pcl_encnames[] = {
    "Indexed By Plane", "Indexed By Pixel",
    "Direct By Plane", "Direct By Pixel"
};

const char *
pcl_cid_cspace_get_debug_name(const gs_memory_t * mem, int index)
{
    if (index == -1)
        return WHITE_CS;

    if (index < 0 || index >= countof(pcl_csnames)) {
        dmprintf(mem, "index out of range\n");
        return pcl_csnames[0];
    } else {
        return pcl_csnames[index];
    }
}

const char *
pcl_cid_enc_get_debug_name(const gs_memory_t * mem, int index)
{
    if (index < 0 || index >= countof(pcl_encnames)) {
        dmprintf(mem, "index out of range\n");
        return pcl_encnames[0];
    } else {
        return pcl_encnames[index];
    }
}

#endif

/*
 * Convert a 32-bit floating point number stored in HP's big-endian form
 * into the native form required by the host processor.
 *
 * The non-IEEE case does not even attempt optimization, or proper handling
 * of non-numbers (+-inf, nan); there are not enough non-IEEE processors that
 * this code is likely to run on to make the effort worth-while.
 */
static float
make_float(const byte * pbuff)
{
    union
    {
        float f;
        uint32 l;
    } lf;

    lf.l = (((uint32) pbuff[0]) << 24) + (((uint32) pbuff[1]) << 16)
        + (((uint32) pbuff[2]) << 8) + pbuff[3];

#if (ARCH_FLOATS_ARE_IEEE && (ARCH_SIZEOF_FLOAT == 4))
    return lf.f;
#else
    if (lf.l == 0L)
        return 0.0;
    else
        return ldexp((double)((1L << 23) | (lf.l & ((1L << 23) - 1))),
                     (((lf.l >> 23) & 0xff) - (127 + 23))
            )
            * ((lf.l & (1UL << 31)) != 0 ? -1.0 : 1.0);
#endif
}

/*
 * Convert an array of 32-bit floats from the HP big-endian form into the
 * native form required by the host processor.
 */
static void
convert_float_array(int num_floats, const byte * pinbuff, float *poutbuff)
{
    while (num_floats-- > 0) {
        (*poutbuff++) = make_float(pinbuff);;
        pinbuff += 4;
    }
}

/*
 * Convert a 16-bit integer in HP's big-endian order into native byte order.
 */
static int
make_int16(const byte * pbuff)
{
    ulong l;

    l = (((ulong) pbuff[0]) << 8) + pbuff[1];
    return (int)((int16) l);
}

/*
 * Convert an array of 16-bit integers from HP's big-endian form to the
 * native byte order required by the host processor.
 */
static void
convert_int16_array(int num_ints, const byte * pinbuff, int16 * poutbuff)
{
    while (num_ints-- > 0) {
        *poutbuff++ = make_int16(pinbuff);
        pinbuff += 2;
    }
}

/*
 * Build the  various long-form configure image data structures.
 * Returns 0 on success, < 0 in case of an error.
 */
static int
build_cid_dev_long(pcl_cid_data_t * pcid, const byte * pbuff)
{
    pcl_cid_dev_long_t *pdev = &(pcid->u.dev);

    if (pcid->len != 18)
        return e_Range;
    convert_int16_array(3, pbuff + 6, pdev->white_ref);
    convert_int16_array(3, pbuff + 12, pdev->black_ref);
    return 0;
}

/* The documentation is not clear if these are specified 0..255 or 0..1
   all cid long examples in the ats tests use 0..255.  We use 0..1 for
   the short form */
static void
normalize_cid_minmax_valrange_long(float *minmax)
{
    int i;

    for (i = 0; i < 6; i++) {
        minmax[i] /= 255;
    }
}

static int
build_cid_col_long(pcl_cid_data_t * pcid, const byte * pbuff)
{
    pcl_cid_col_long_t *pcol = &(pcid->u.col);

    if (pcid->len != 86)
        return e_Range;
    convert_float_array(8, pbuff + 6, (float *)(pcol->colmet.chroma));
    convert_float_array(6, pbuff + 38, (float *)(pcol->colmet.nonlin));
    convert_float_array(6, pbuff + 62, (float *)(pcol->minmax.val_range));
    normalize_cid_minmax_valrange_long((float *)(pcol->minmax.val_range));
    return 0;
}

static int
build_cid_lab_long(pcl_cid_data_t * pcid, const byte * pbuff)
{
    pcl_cid_Lab_long_t *plab = &(pcid->u.lab);

    if (pcid->len != 30)
        return e_Range;
    convert_float_array(6, pbuff + 6, (float *)(plab->minmax.val_range));
    return 0;
}

static int
build_cid_lum_long(pcl_cid_data_t * pcid, const byte * pbuff)
{
    pcl_cid_lum_long_t *plum = &(pcid->u.lum);

    if (pcid->len != 122)
        return e_Range;
    convert_float_array(9, pbuff + 6, plum->matrix);
    convert_float_array(6, pbuff + 42, (float *)(plum->minmax.val_range));
    convert_float_array(8, pbuff + 66, (float *)(plum->colmet.chroma));
    convert_float_array(6, pbuff + 98, (float *)(plum->colmet.nonlin));
    return 0;
}

static int (*const build_cid_longform[]) (pcl_cid_data_t *, const byte *) = {
    build_cid_dev_long,         /* pcl_cspace_RGB */
        build_cid_dev_long,     /* pcl_cspace_CMY */
        build_cid_col_long,     /* pcl_cspace_Colorimetric */
        build_cid_lab_long,     /* pcl_cspace_CIELab */
        build_cid_lum_long      /* pcl_cspace_LumChrom */
};

/*
 * Check the configure image data short form structure.
 *
 * Returns 0 on success, < 0 in case of an error.
 */
static int
check_cid_hdr(pcl_state_t * pcs, pcl_cid_data_t * pcid)
{
    pcl_cid_hdr_t *pcidh = &(pcid->u.hdr);
    int i;

    if ((pcidh->cspace >= pcl_cspace_num)
        || (pcidh->encoding >= pcl_penc_num))
        return -1;

    /* apparently direct by pixel encoding mode defaults bits per
       index to 8 */
    if (pcidh->encoding == pcl_penc_direct_by_pixel)
        pcidh->bits_per_index = 8;

    /*
     * Map zero values. Zero bits per index is equivalent to one bit per index;
     * zero bits per primary is equivalent to 8 bits per primary.
     */
    if (pcidh->bits_per_index == 0)
        pcidh->bits_per_index = 1;
    for (i = 0; i < countof(pcidh->bits_per_primary); i++) {
        if (pcidh->bits_per_primary[i] == 0)
            pcidh->bits_per_primary[i] = 8;
        if (pcs->personality == pcl5e && pcidh->bits_per_primary[i] != 1)
            dmprintf(pcs->memory, "pcl5e personality with color primaries\n");
    }

    if (pcidh->bits_per_index > 8)
        return -1;

    switch (pcidh->encoding) {

        case pcl_penc_indexed_by_pixel:
            if ((pcidh->bits_per_index & (pcidh->bits_per_index - 1)) != 0)
                return -1;
            break;

        case pcl_penc_indexed_by_plane:
            break;

        case pcl_penc_direct_by_plane:
            /* must be device-specific color space */
            if ((pcidh->cspace != pcl_cspace_RGB)
                && (pcidh->cspace != pcl_cspace_CMY))
                return -1;
            if ((pcidh->bits_per_primary[0] != 1) ||
                (pcidh->bits_per_primary[1] != 1) ||
                (pcidh->bits_per_primary[2] != 1))
                return -1;
            break;

        case pcl_penc_direct_by_pixel:
            if ((pcidh->bits_per_primary[0] != 8) ||
                (pcidh->bits_per_primary[1] != 8) ||
                (pcidh->bits_per_primary[2] != 8))
                return -1;
            break;
    }

    /*
     * Strange HP-ism: for device independent color spaces, bits per primary
     * is always 8. For the direct by plane/pixel cases, this will already be
     * the case, but the indexed by pixel/plane cases may require modification.
     */
    if ((pcidh->encoding < pcl_penc_direct_by_plane) &&
        (pcidh->cspace > pcl_cspace_CMY)) {
        pcidh->bits_per_primary[0] = 8;
        pcidh->bits_per_primary[1] = 8;
        pcidh->bits_per_primary[2] = 8;
    }

    /* 
     * All long form color commands appear to be deprecated on modern
     * HP devices.  We simply truncate all long form commands to the 6
     * byte short form.
     */

    pcid->len = 6;

    /* remap the colorimetric color space to rgb */
    if (pcidh->cspace == pcl_cspace_Colorimetric)
        pcidh->cspace = pcl_cspace_RGB;

    return 0;
}

/*
 * Construct a configure image data object, and use it to overwrite the current
 * active palette.
 *
 * Returns 0 if successful, < 0 in case of error.
 */
static int
install_cid_data(int len,       /* length of data */
                 const byte * pbuff,    /* the data provided with command */
                 pcl_state_t * pcs,     /* current state */
                 bool fixed,    /* from set simple color mode */
                 bool gl2       /* from IN command in GL/2 */
    )
{
    pcl_cid_data_t cid;
    int code = 0;

    if (len < 6)
        return e_Range;
    cid.len = len;
    memcpy(&(cid.u.hdr), pbuff, sizeof(pcl_cid_hdr_t));

    if_debug2m('c', pcs->memory,
               "[c] cid before check color space: %s encoding: %s\n",
               pcl_cid_cspace_get_debug_name(pcs->memory,
                                             pcl_cid_get_cspace(&cid)),
               pcl_cid_enc_get_debug_name(pcs->memory,
                                          pcl_cid_get_encoding(&cid)));

    /* check the header, this will also make corrections if possible */
    code = check_cid_hdr(pcs, &cid);

    if_debug2m('c', pcs->memory,
               "[c] cid after check color space: %s encoding: %s\n",
               pcl_cid_cspace_get_debug_name(pcs->memory,
                                             pcl_cid_get_cspace(&cid)),
               pcl_cid_enc_get_debug_name(pcs->memory,
                                          pcl_cid_get_encoding(&cid)));

    if (code >= 0) {
        cid.original_cspace = pcl_cspace_num;
        if (cid.len > 6)
            code = build_cid_longform[pbuff[0]] (&cid, pbuff);
    }

    if (code < 0) {
        if (code == -1)
            code = e_Range;
        return code;
    } else
        return pcl_palette_set_cid(pcs, &cid, fixed, gl2);
}

/*
 *  Build a "simple color mode" pcl_cid_data_t structure, and use it to
 *  overwrite the current palette. This routine is separated out because it
 *  is required by both the simple color space command and the reset code.
 */
static int
set_simple_color_mode(int type, pcl_state_t * pcs)
{
    static const byte cid_K[6] = { (byte) pcl_cspace_RGB,
        (byte) pcl_penc_indexed_by_plane,
        1, 1, 1, 1
    };
    static const byte cid_CMY[6] = { (byte) pcl_cspace_CMY,
        (byte) pcl_penc_indexed_by_plane,
        3, 1, 1, 1
    };
    static const byte cid_RGB[6] = { (byte) pcl_cspace_RGB,
        (byte) pcl_penc_indexed_by_plane,
        3, 1, 1, 1
    };
    const byte *pbuff = 0;

    if (type == 1)
        pbuff = cid_K;
    else if (type == 3)
        pbuff = cid_RGB;
    else if (type == -3)
        pbuff = cid_CMY;
    else
        return e_Range;

    /* install the new color space */
    return install_cid_data(6, pbuff, pcs, true, false);
}

/*
 * ESC * v <nbytes> W
 *
 * This command creates only the basic element of the the palette: the cid_data
 * array. Other parts are created as needed.
 */
static int
pcl_configure_image_data(pcl_args_t * pargs, pcl_state_t * pcs)
{
    if (pcs->personality == pcl5e || pcs->raster_state.graphics_mode)
        return 0;
#ifdef DEBUG
    if (gs_debug_c('i')) {
        pcl_debug_dump_data(pcs->memory, arg_data(pargs), uint_arg(pargs));
    }
#endif
    return install_cid_data(uint_arg(pargs),
                            arg_data(pargs), pcs, false, false);
}

/*
 * ESC * r <code> U
 *
 * Simple color space command. Note that all the simple color spaces are
 * variations of the RGB/CMY device-specific color space.
 */
static int
pcl_simple_color_space(pcl_args_t * pargs, pcl_state_t * pcs)
{
    if (pcs->personality == pcl5e || pcs->raster_state.graphics_mode)
        return 0;
    return set_simple_color_mode(int_arg(pargs), pcs);
}

/*
 * ESC * i <nbytes> W
 *
 * Set viewing illuminant. This command is related to the configure image
 * data object only in the sense that both apply to palettes. The command
 * is implemented in this file as it is the only other command that involves
 * binary floating point number arrays.
 *
 * This routine will convert the whitepoint to the form anticipated by the
 * gs_cie_render structure (i.e., Y = 1.0).
 */
static int
set_view_illuminant(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint len = uint_arg(pargs);
    const byte *pbuff = arg_data(pargs);
    float x, y;
    gs_vector3 wht_pt;

    if (pcs->personality == pcl5e || pcs->raster_state.graphics_mode)
        return 0;

    if (len != 8)
        return e_Range;
    x = make_float(pbuff);
    y = make_float(pbuff + 4);

    /*
     * A white point must have a non-zero y value, as otherwise it carries
     * no chromaticity infomration. It should also have x >= 0, y > 0, and
     * x + y <= 1, for otherwise it represents an unrealizable color.
     */
    if ((x < 0.0) || (y <= 0.0) || (x + y > 1.0))
        return e_Range;

    wht_pt.u = x / y;
    wht_pt.v = 1.0;
    wht_pt.w = (1.0 - x - y) / y;

    return pcl_palette_set_view_illuminant(pcs, &wht_pt);
}

/*
 * The following procedure supports the GL/2 "IN" command. It would be better
 * implemented via a reset flag, but at the moment there are no reset flags
 * that propagate out of GL/2.
 *
 * Returns 0 on success, < 0 in case of an error.
 */
int
pcl_cid_IN(pcl_state_t * pcs)
{
    static const byte cid_GL2_Color[6] = { (byte) pcl_cspace_RGB,
        (byte) pcl_penc_indexed_by_plane,
        3, 8, 8, 8
    };
    static const byte cid_GL2_Mono[6] = { (byte) pcl_cspace_RGB,
        (byte) pcl_penc_indexed_by_plane,
        3, 1, 1, 1
    };

    return install_cid_data(6,
                            pcs->personality ==
                            pcl5e ? cid_GL2_Mono : cid_GL2_Color, pcs, false,
                            true);
}

/*
 * All CCITT raster is monochrome so this color space is installed
 * even if the current palette is color.
 */

int
pcl_cid_CCITT_raster(pcl_state_t * pcs)
{
    static const byte cid_ccitt[6] = { (byte) pcl_cspace_RGB,
        (byte) pcl_penc_indexed_by_plane,
        1, 1, 1, 1
    };
    return install_cid_data(6, cid_ccitt, pcs, false, true);
}

/*
 * There is no copy code required for this module, as any copying that is
 * required is performed at the palette level. Similarly, there is no reset
 * code, as reset is handled at the palette level.
 */
static int
pcl_cid_do_registration(pcl_parser_state_t * pcl_parser_state,
                        gs_memory_t * pmem)
{
    DEFINE_CLASS('*') {
        'v', 'W',
            PCL_COMMAND("Configure Image Data", pcl_configure_image_data,
                        pca_bytes | pca_in_rtl | pca_raster_graphics)
    }, {
        'r', 'U',
            PCL_COMMAND("Simple Color Mode", pcl_simple_color_space,
                        pca_neg_ok | pca_in_rtl | pca_raster_graphics)
    }, {
        'i', 'W',
            PCL_COMMAND("Set Viewing Illuminant", set_view_illuminant,
                        pca_bytes | pca_in_rtl | pca_raster_graphics)
    }, END_CLASS return 0;
}

const pcl_init_t pcl_cid_init =
    { pcl_cid_do_registration, 0, 0 };
