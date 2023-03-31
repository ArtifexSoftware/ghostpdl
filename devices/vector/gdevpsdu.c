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


/* Common utilities for PostScript and PDF writers */
#include "stdio_.h"		/* for FILE for jpeglib.h */
#include "jpeglib_.h"		/* for sdct.h */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gdevpsdf.h"
#include "strimpl.h"
#include "sa85x.h"
#include "scfx.h"
#include "sdct.h"
#include "sjpeg.h"
#include "spprint.h"
#include "gsovrc.h"
#include "gsicc_cache.h"

/* Structure descriptors */
public_st_device_psdf();
public_st_psdf_binary_writer();

/* Standard color command names. */
const psdf_set_color_commands_t psdf_set_fill_color_commands = {
    "g", "rg", "k", "cs", "sc", "scn"
};
const psdf_set_color_commands_t psdf_set_stroke_color_commands = {
    "G", "RG", "K", "CS", "SC", "SCN"
};

/* Define parameter-setting procedures. */
extern stream_state_proc_put_params(s_DCTE_put_params, stream_DCT_state);

/* ---------------- Vector implementation procedures ---------------- */

int
psdf_setlinewidth(gx_device_vector * vdev, double width)
{
    pprintg1(gdev_vector_stream(vdev), "%g w\n", width);
    return 0;
}

int
psdf_setlinecap(gx_device_vector * vdev, gs_line_cap cap)
{
    switch (cap) {
        case gs_cap_butt:
        case gs_cap_round:
        case gs_cap_square:
            pprintd1(gdev_vector_stream(vdev), "%d J\n", cap);
            break;
        case gs_cap_triangle:
            /* If we get a PCL triangle cap, substitute with a round cap */
            pprintd1(gdev_vector_stream(vdev), "%d J\n", gs_cap_round);
            break;
        default:
            /* Ensure we don't write a broken file if we don't recognise the cap */
            emprintf1(vdev->memory,
                      "Unknown line cap enumerator %d, substituting butt\n",
                      cap);
            pprintd1(gdev_vector_stream(vdev), "%d J\n", gs_cap_butt);
            break;
    }
    return 0;
}

int
psdf_setlinejoin(gx_device_vector * vdev, gs_line_join join)
{
    switch (join) {
        case gs_join_miter:
        case gs_join_round:
        case gs_join_bevel:
            pprintd1(gdev_vector_stream(vdev), "%d j\n", join);
            break;
        case gs_join_none:
            /* If we get a PCL triangle join, substitute with a bevel join */
            pprintd1(gdev_vector_stream(vdev), "%d j\n", gs_join_bevel);
            break;
        case gs_join_triangle:
            /* If we get a PCL triangle join, substitute with a miter join */
            pprintd1(gdev_vector_stream(vdev), "%d j\n", gs_join_miter);
            break;
        default:
            /* Ensure we don't write a broken file if we don't recognise the join */
            emprintf1(vdev->memory,
                      "Unknown line join enumerator %d, substituting miter\n",
                      join);
            pprintd1(gdev_vector_stream(vdev), "%d j\n", gs_join_miter);
            break;
    }
    return 0;
}

int
psdf_setmiterlimit(gx_device_vector * vdev, double limit)
{
    pprintg1(gdev_vector_stream(vdev), "%g M\n", limit);
    return 0;
}

int
psdf_setdash(gx_device_vector * vdev, const float *pattern, uint count,
             double offset)
{
    stream *s = gdev_vector_stream(vdev);
    int i;

    stream_puts(s, "[ ");
    for (i = 0; i < count; ++i)
        pprintg1(s, "%g ", pattern[i]);
    pprintg1(s, "] %g d\n", offset);
    return 0;
}

int
psdf_setflat(gx_device_vector * vdev, double flatness)
{
    pprintg1(gdev_vector_stream(vdev), "%g i\n", flatness);
    return 0;
}

int
psdf_setlogop(gx_device_vector * vdev, gs_logical_operation_t lop,
              gs_logical_operation_t diff)
{
/****** SHOULD AT LEAST DETECT SET-0 & SET-1 ******/
    return 0;
}

int
psdf_dorect(gx_device_vector * vdev, fixed x0, fixed y0, fixed x1, fixed y1,
            gx_path_type_t type)
{
    int code = (*vdev_proc(vdev, beginpath)) (vdev, type);

    if (code < 0)
        return code;
    pprintg4(gdev_vector_stream(vdev), "%g %g %g %g re\n",
             fixed2float(x0), fixed2float(y0),
             fixed2float(x1 - x0), fixed2float(y1 - y0));
    return (*vdev_proc(vdev, endpath)) (vdev, type);
}

int
psdf_beginpath(gx_device_vector * vdev, gx_path_type_t type)
{
    return 0;
}

int
psdf_moveto(gx_device_vector * vdev, double x0, double y0, double x, double y,
            gx_path_type_t type)
{
    pprintg2(gdev_vector_stream(vdev), "%g %g m\n", x, y);
    return 0;
}

int
psdf_lineto(gx_device_vector * vdev, double x0, double y0, double x, double y,
            gx_path_type_t type)
{
    pprintg2(gdev_vector_stream(vdev), "%g %g l\n", x, y);
    return 0;
}

int
psdf_curveto(gx_device_vector * vdev, double x0, double y0,
           double x1, double y1, double x2, double y2, double x3, double y3,
             gx_path_type_t type)
{
    if (x1 == x0 && y1 == y0 && x2 == x3 && y2 == y3)
        pprintg2(gdev_vector_stream(vdev), "%g %g l\n", x3, y3);
    else if (x1 == x0 && y1 == y0)
        pprintg4(gdev_vector_stream(vdev), "%g %g %g %g v\n",
                 x2, y2, x3, y3);
    else if (x3 == x2 && y3 == y2)
        pprintg4(gdev_vector_stream(vdev), "%g %g %g %g y\n",
                 x1, y1, x2, y2);
    else
        pprintg6(gdev_vector_stream(vdev), "%g %g %g %g %g %g c\n",
                 x1, y1, x2, y2, x3, y3);
    return 0;
}

int
psdf_closepath(gx_device_vector * vdev, double x0, double y0,
               double x_start, double y_start, gx_path_type_t type)
{
    stream_puts(gdev_vector_stream(vdev), "h\n");
    return 0;
}

/* endpath is deliberately omitted. */

/* ---------------- Utilities ---------------- */

gx_color_index
psdf_adjust_color_index(gx_device_vector *vdev, gx_color_index color)
{
    /*
     * Since gx_no_color_index is all 1's, we can't represent
     * a CMYK color consisting of full ink in all 4 components.
     * However, this color must be available for registration marks.
     * gxcmap.c fudges this by changing the K component to 254;
     * undo this fudge here.
     */
    return (color == (gx_no_color_index ^ 1) ? gx_no_color_index : color);
}

/* Round a double value to a specified precision. */
double
psdf_round(double v, int precision, int radix)
{
    double mul = 1;
    double w = v;

    if (w <= 0)
        return w;
    while (w < precision) {
        w *= radix;
        mul *= radix;
    }
    return (int)(w + 0.5) / mul;
}

/*
 * Since we only have 8 bits of color to start with, round the
 * values to 3 digits for more compact output.
 */
static inline double
round_byte_color(gx_color_index cv)
{
    return (int)((uint)cv * (1000.0 / 255.0) + 0.5) / 1000.0;
}
int
psdf_set_color(gx_device_vector * vdev, const gx_drawing_color * pdc,
               const psdf_set_color_commands_t *ppscc)
{
    const char *setcolor;
    int num_des_comps, code;
    cmm_dev_profile_t *dev_profile;

    code = dev_proc((gx_device *)vdev, get_profile)((gx_device *)vdev, &dev_profile);
    if (code < 0)
        return code;
    num_des_comps = gsicc_get_device_profile_comps(dev_profile);

    if (!gx_dc_is_pure(pdc))
        return_error(gs_error_rangecheck);
    {
        stream *s = gdev_vector_stream(vdev);
        gx_color_index color =
            psdf_adjust_color_index(vdev, gx_dc_pure_color(pdc));
        /*
         * Normally we would precompute all of v0 .. v3, but gcc 2.7.2.3
         * generates incorrect code for Intel CPUs if we do this.  The code
         * below is longer, but does less computation in some cases.
         */
        double v3 = round_byte_color(color & 0xff);

        switch (num_des_comps) {
        case 4:
            /* if (v0 == 0 && v1 == 0 && v2 == 0 && ...) */
            if ((color & 0xffffff00) == 0 && ppscc->setgray != 0) {
                v3 = 1.0 - v3;
                goto g;
            }
            pprintg4(s, "%g %g %g %g", round_byte_color(color >> 24),
                     round_byte_color((color >> 16) & 0xff),
                     round_byte_color((color >> 8) & 0xff), v3);
            setcolor = ppscc->setcmykcolor;
            break;
        case 3:
            /* if (v1 == v2 && v2 == v3 && ...) */
            if (!((color ^ (color >> 8)) & 0xffff) && ppscc->setgray != 0)
                goto g;
            pprintg3(s, "%g %g %g", round_byte_color((color >> 16) & 0xff),
                     round_byte_color((color >> 8) & 0xff), v3);
            setcolor = ppscc->setrgbcolor;
            break;
        case 1:
        g:
            pprintg1(s, "%g", v3);
            setcolor = ppscc->setgray;
            break;
        default:		/* can't happen */
            return_error(gs_error_rangecheck);
        }
        if (setcolor)
            pprints1(s, " %s\n", setcolor);
    }
    return 0;
}

/* ---------------- Binary data writing ---------------- */

/* Begin writing binary data. */
int
psdf_begin_binary(gx_device_psdf * pdev, psdf_binary_writer * pbw)
{
    gs_memory_t *mem = pbw->memory = pdev->v_memory;

    pbw->target = pdev->strm;
    pbw->dev = pdev;
    pbw->strm = 0;		/* for GC in case of failure */
    /* If not binary, set up the encoding stream. */
    if (!pdev->binary_ok) {
#define BUF_SIZE 100		/* arbitrary */
        byte *buf = gs_alloc_bytes(mem, BUF_SIZE, "psdf_begin_binary(buf)");
        stream_A85E_state *ss = (stream_A85E_state *)
            s_alloc_state(mem, s_A85E_template.stype,
                          "psdf_begin_binary(stream_state)");
        stream *s = s_alloc(mem, "psdf_begin_binary(stream)");

        if (buf == 0 || ss == 0 || s == 0) {
            gs_free_object(mem, s, "psdf_begin_binary(stream)");
            gs_free_object(mem, ss, "psdf_begin_binary(stream_state)");
            gs_free_object(mem, buf, "psdf_begin_binary(buf)");
            return_error(gs_error_VMerror);
        }
        ss->templat = &s_A85E_template;
        s_init_filter(s, (stream_state *)ss, buf, BUF_SIZE, pdev->strm);
#undef BUF_SIZE
        pbw->strm = s;
    } else {
        pbw->strm = pdev->strm;
    }
    return 0;
}

/* Add an encoding filter.  The client must have allocated the stream state, */
/* if any, using pdev->v_memory. */
int
psdf_encode_binary(psdf_binary_writer * pbw, const stream_template * templat,
                   stream_state * ss)
{
    return (s_add_filter(&pbw->strm, templat, ss, pbw->memory) == 0 ?
            gs_note_error(gs_error_VMerror) : 0);
}

/*
 * Acquire parameters, and optionally set up the filter for, a DCTEncode
 * filter.  This is a separate procedure so it can be used to validate
 * filter parameters when they are set, rather than waiting until they are
 * used.  pbw = NULL means just set up the stream state.
 */
int
psdf_DCT_filter(gs_param_list *plist /* may be NULL */,
                stream_state /*stream_DCTE_state*/ *st,
                int Columns, int Rows, int Colors,
                psdf_binary_writer *pbw /* may be NULL */)
{
        stream_DCT_state *const ss = (stream_DCT_state *) st;
        gs_memory_t *mem = st->memory;
        jpeg_compress_data *jcdp;
        gs_c_param_list rcc_list;
        int code;

        /*
         * "Wrap" the actual Dict or ACSDict parameter list in one that
         * sets Rows, Columns, and Colors.
         */
        gs_c_param_list_write(&rcc_list, mem);
        if ((code = param_write_int((gs_param_list *)&rcc_list, "Rows",
                                    &Rows)) < 0 ||
            (code = param_write_int((gs_param_list *)&rcc_list, "Columns",
                                    &Columns)) < 0 ||
            (code = param_write_int((gs_param_list *)&rcc_list, "Colors",
                                    &Colors)) < 0
            ) {
            goto rcc_fail;
        }
        gs_c_param_list_read(&rcc_list);
        if (plist)
            gs_c_param_list_set_target(&rcc_list, plist);
        /* Allocate space for IJG parameters. */
        jcdp = gs_alloc_struct_immovable(mem, jpeg_compress_data,
           &st_jpeg_compress_data, "zDCTE");
        if (jcdp == 0)
            return_error(gs_error_VMerror);
        jcdp->cinfo.mem = NULL;
        jcdp->cinfo.client_data = NULL;
        ss->data.compress = jcdp;
        jcdp->memory = ss->jpeg_memory = mem;	/* set now for allocation */
        if ((code = gs_jpeg_create_compress(ss)) < 0)
            goto dcte_fail;	/* correct to do jpeg_destroy here */
        /* Read parameters from dictionary */
        code = s_DCTE_put_params((gs_param_list *)&rcc_list, ss);
        if (code < 0)
            return code;
        /* Create the filter. */
        jcdp->templat = s_DCTE_template;
        /* Make sure we get at least a full scan line of input. */
        ss->scan_line_size = jcdp->cinfo.input_components *
            jcdp->cinfo.image_width;
        /* Profile not used in pdfwrite output */
        ss->icc_profile = NULL;
        jcdp->templat.min_in_size =
            max(s_DCTE_template.min_in_size, ss->scan_line_size);
        /* Make sure we can write the user markers in a single go. */
        jcdp->templat.min_out_size =
            max(s_DCTE_template.min_out_size, ss->Markers.size);
        if (pbw)
            code = psdf_encode_binary(pbw, &jcdp->templat, st);
        if (code >= 0) {
            gs_c_param_list_release(&rcc_list);
            return 0;
        }
    dcte_fail:
        gs_jpeg_destroy(ss);
        gs_free_object(mem, jcdp, "setup_image_compression");
        ss->data.compress = NULL; /* Avoid problems with double frees later */
    rcc_fail:
        gs_c_param_list_release(&rcc_list);
        return code;
}

/* Add a 2-D CCITTFax encoding filter. */
/* Set EndOfBlock iff the stream is not ASCII85 encoded. */
int
psdf_CFE_binary(psdf_binary_writer * pbw, int w, int h, bool invert)
{
    gs_memory_t *mem = pbw->memory;
    const stream_template *templat = &s_CFE_template;
    stream_CFE_state *st =
        gs_alloc_struct(mem, stream_CFE_state, templat->stype,
                        "psdf_CFE_binary");
    int code;

    if (st == 0)
        return_error(gs_error_VMerror);
    (*templat->set_defaults) ((stream_state *) st);
    st->K = -1;
    st->Columns = w;
    st->Rows = 0;
    st->BlackIs1 = !invert;
    st->EndOfBlock = pbw->strm->state->templat != &s_A85E_template;
    code = psdf_encode_binary(pbw, templat, (stream_state *) st);
    if (code < 0)
        gs_free_object(mem, st, "psdf_CFE_binary");
    return code;
}

/* Finish writing binary data. */
int
psdf_end_binary(psdf_binary_writer * pbw)
{
    int status = s_close_filters(&pbw->strm, pbw->target);

    return (status >= 0 ? 0 : gs_note_error(gs_error_ioerror));
}

/* ---------------- Overprint, Get Bits ---------------- */

/*
 * High level devices cannot perform get_bits_rectangle
 * operations, for obvious reasons.
 */
int
psdf_get_bits_rectangle(
    gx_device *             dev,
    const gs_int_rect *     prect,
    gs_get_bits_params_t *  params )
{
    emprintf(dev->memory,
                  "Can't set GraphicsAlphaBits or TextAlphaBits with a vector device.\n");
    return_error(gs_error_unregistered);
}

/*
 * Create compositor procedure for PostScript/PDF writer. Since these
 * devices directly support overprint (and have access to the gs_gstate),
 * no compositor is required for overprint support. Hence, this
 * routine just recognizes and discards invocations of the overprint
 * compositor.
 */
int
psdf_composite(
    gx_device *             dev,
    gx_device **            pcdev,
    const gs_composite_t *  pct,
    gs_gstate             *  pgs,
    gs_memory_t *           mem,
    gx_device *             cdev)
{
    if (gs_is_overprint_compositor(pct)) {
        *pcdev = dev;
        return 0;
    }
    return gx_default_composite(dev, pcdev, pct, pgs, mem, cdev);
}
