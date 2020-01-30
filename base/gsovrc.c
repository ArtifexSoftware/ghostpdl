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


/* overprint/overprint mode compositor implementation */

#include "assert_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsutil.h"             /* for gs_next_ids */
#include "gxcomp.h"
#include "gxdevice.h"
#include "gsdevice.h"
#include "gxgetbit.h"
#include "gsovrc.h"
#include "gxdcolor.h"
#include "gxoprect.h"
#include "gsbitops.h"
#include "gxgstate.h"
#include "gxdevsop.h"
#include "gxcldev.h"

/* GC descriptor for gs_overprint_t */
private_st_gs_overprint_t();

/*
 * Utility routine for encoding or decoding a color index. We cannot use
 * the general integer encoding routins for these, as they may be 64 bits
 * in length (the general routines are only designed for 32 bits). We also
 * cannot use the color-specific routines, as we do not have the required
 * device color information available.
 *
 * The scheme employed is the potentially 64-bit analog of the 32-bit
 * routines: the low order seven bits of each bytes represents a base-128
 * digit, and the high order bit is set if there is another digit. The
 * encoding order is little-endian.
 *
 * The write routine returns 0 on success, with *psize set to the number
 * of bytes used. Alternatively, the return value will be gs_error_rangecheck,
 * with *psize set to the number of bytes required, if there was insufficient
 * space.
 *
 * The read routine returns the number of bytes read on success, or < 0 in
 * the event of an error.
 */
static int
write_color_index(gx_color_index cindex, byte * data, uint * psize)
{
    int             num_bytes = 0;
    gx_color_index  ctmp = cindex;

    for (num_bytes = 1; (ctmp >>= 7) != 0; ++num_bytes)
        ;
    if (num_bytes > *psize) {
        *psize = num_bytes;
        return_error(gs_error_rangecheck);
    }
    ctmp = cindex;
    *psize = num_bytes;
    for (; num_bytes > 1; ctmp >>= 7, --num_bytes)
        *data++ = 0x80 | (ctmp & 0x7f);
    *data = ctmp & 0x7f;
    return 0;
}

static int
read_color_index(gx_color_index * pcindex, const byte * data, uint size)
{
    gx_color_index  cindex = 0;
    int             nbytes = 0, shift = 0;

    for (;; shift += 7, data++) {
        if (++nbytes > size)
            return_error(gs_error_rangecheck);
        else {
            unsigned char byte = *data;
            gx_color_index c = byte;

            cindex += (c & 0x7f) << shift;
            if ((c & 0x80) == 0)
                break;
        }
    }
    *pcindex = cindex;
    return nbytes;
}

/*
 * Check for equality of two overprint compositor objects.
 *
 * This is fairly simple.
 */
static bool
c_overprint_equal(const gs_composite_t * pct0, const gs_composite_t * pct1)
{
    if (pct0->type == pct1->type) {
        const gs_overprint_params_t *    pparams0;
        const gs_overprint_params_t *    pparams1;

        pparams0 = &((const gs_overprint_t *)(pct0))->params;
        pparams1 = &((const gs_overprint_t *)(pct1))->params;

        if (pparams0->is_fill_color != pparams1->is_fill_color)
            return true;		/* this changed */
        if (!pparams0->retain_any_comps)
            return !pparams1->retain_any_comps;
        else
            return pparams0->drawn_comps == pparams1->drawn_comps;
    } else
        return false;
}

/*
 * Bits corresponding to boolean values in the first byte of the string
 * representation of an overprint compositor.
 */
#define OVERPRINT_ANY_COMPS           1
#define OVERPRINT_IS_FILL_COLOR       2
#define OVERPRINT_SET_FILL_COLOR      0xc
#define OVERPRINT_EOPM                0x10

/*
 * Convert an overprint compositor to string form for use by the command
 * list device.
 */
static int
c_overprint_write(const gs_composite_t * pct, byte * data, uint * psize, gx_device_clist_writer *cdev)
{
    const gs_overprint_params_t *   pparams = &((const gs_overprint_t *)pct)->params;
    byte                            flags = 0;
    int                             used = 1, avail = *psize;

    /* Clist writer needs to store active state of op device so that
       we know when to send compositor actions to disable it */
    if (pparams->op_state == OP_STATE_NONE) {
        if (pparams->is_fill_color) {
            if (pparams->retain_any_comps)
                cdev->op_fill_active = true;
            else
                cdev->op_fill_active = false;
        } else {
            if (pparams->retain_any_comps)
                cdev->op_stroke_active = true;
            else
                cdev->op_stroke_active = false;
        }
    }

    /* encoded the booleans in a single byte */
    if (pparams->retain_any_comps || pparams->is_fill_color || pparams->op_state) {
        flags |= (pparams->retain_any_comps) ? OVERPRINT_ANY_COMPS : 0;
        flags |= (pparams->is_fill_color) ? OVERPRINT_IS_FILL_COLOR : 0;
        flags |= (pparams->op_state) << 2;
        flags |= (pparams->effective_opm) << 4;

        /* write out the component bits */
        if (pparams->retain_any_comps) {
            uint tmp_size = (avail > 0 ? avail - 1 : 0);
            int code = write_color_index(pparams->drawn_comps, data + 1,
                &tmp_size);
            if (code < 0 && code != gs_error_rangecheck)
                return code;
            used += tmp_size;
            if_debug0m('v', ((const gx_device*)cdev)->memory, "[v] drawn_comps stored\n");

        }
    }

    /* check for overflow */
    *psize = used;
    if (used > avail)
        return_error(gs_error_rangecheck);
    data[0] = flags;
    if_debug2m('v', ((const gx_device *)cdev)->memory, "[v]c_overprint_write(%d), drawn_comps=0x%x\n",
               flags, pparams->drawn_comps);
    return 0;
}

/*
 * Convert the string representation of the overprint parameter into the
 * full compositor.
 */
static int
c_overprint_read(
    gs_composite_t **       ppct,
    const byte *            data,
    uint                    size,
    gs_memory_t *           mem )
{
    gs_overprint_params_t   params;
    byte                    flags = 0;
    int                     code = 0, nbytes = 1;

    if (size < 1)
        return_error(gs_error_rangecheck);
    flags = *data;
    if_debug1m('v', mem, "[v]c_overprint_read(%d)", flags);
    params.retain_any_comps = (flags & OVERPRINT_ANY_COMPS) != 0;
    params.is_fill_color = (flags & OVERPRINT_IS_FILL_COLOR) != 0;
    params.op_state = (flags & OVERPRINT_SET_FILL_COLOR) >> 2;
    params.effective_opm = (flags & OVERPRINT_EOPM) >> 4;
    params.idle = 0;
    params.drawn_comps = 0;

    /* check if the drawn_comps array is present */
    if (params.retain_any_comps) {
        code = read_color_index(&params.drawn_comps, data + 1, size - 1);
        if (code < 0)
            return code;
        nbytes += code;
        if_debug0m('v', mem, ", drawn_comps read");
    }
    if_debug1m('v', mem, ", retain_any_comps=%d", params.retain_any_comps);
    if_debug1m('v', mem, ", is_fill_color=%d", params.is_fill_color);
    if_debug1m('v', mem, ", drawn_comps=0x%x", params.drawn_comps);
    if_debug1m('v', mem, ", op_state=%d", params.op_state);
    if_debug0m('v', mem, "\n");
    code = gs_create_overprint(ppct, &params, mem);
    return code < 0 ? code : nbytes;
}

/*
 * Check for closing compositor.
 */
static gs_compositor_closing_state
c_overprint_is_closing(const gs_composite_t *this, gs_composite_t **ppcte, gx_device *dev)
{
    return COMP_ENQUEUE;	/* maybe extra work, but these actions are fast */
}

static composite_create_default_compositor_proc(c_overprint_create_default_compositor);
static composite_equal_proc(c_overprint_equal);
static composite_write_proc(c_overprint_write);
static composite_is_closing_proc(c_overprint_is_closing);
static composite_read_proc(c_overprint_read);

/* methods for the overprint compositor */
const gs_composite_type_t   gs_composite_overprint_type = {
    GX_COMPOSITOR_OVERPRINT,
    {
        c_overprint_create_default_compositor,  /* procs.create_default_compositor */
        c_overprint_equal,                      /* procs.equal */
        c_overprint_write,                      /* procs.write */
        c_overprint_read,                       /* procs.read */
        gx_default_composite_adjust_ctm,
        c_overprint_is_closing,
        gx_default_composite_is_friendly,
        gx_default_composite_clist_write_update,/* procs.composite_clist_write_update */
        gx_default_composite_clist_read_update,	/* procs.composite_clist_reade_update */
        gx_default_composite_get_cropping	/* procs.composite_get_cropping */
    }                                           /* procs */
};

/*
 * Create an overprint compositor data structure.
 *
 * Currently this just a stub.
 */
int
gs_create_overprint(
    gs_composite_t **               ppct,
    const gs_overprint_params_t *   pparams,
    gs_memory_t *                   mem )
{
    gs_overprint_t *                pct;

    pct = gs_alloc_struct(mem, gs_overprint_t, &st_overprint,
                              "gs_create_overprint");
    if (pct == NULL)
        return_error(gs_error_VMerror);
    pct->type = &gs_composite_overprint_type;
    pct->id = gs_next_ids(mem, 1);
    pct->params = *pparams;
    pct->idle = false;
    *ppct = (gs_composite_t *)pct;
    return 0;
}

/*
 * Verify that a compositor data structure is for the overprint compositor.
 * This is used by the gs_pdf1.4_device (and eventually the PDFWrite
 * device), which implements overprint and overprint mode directly.
 */
int
gs_is_overprint_compositor(const gs_composite_t * pct)
{
    return pct->type == &gs_composite_overprint_type;
}

/*
 * The overprint device.
 *
 * In principle there are two versions of this device: one if the traget
 * device is separable and linear, the other if it is not. The two have
 * slightly different data structures, but differ primarily in terms of
 * the standard set of methods. Because methods are non-static in
 * GhostScript, we make use of the same data structure and handle the
 * distinction during initialization.
 *
 * The data fields reflect entries in the gs_overprint_params_t
 * structure. There is no explicit retain_any_comps field, as the current
 * setting of this field can be determined by checking the fill_rectangle
 * method.
 */
typedef struct overprint_device_s {
    gx_device_forward_common;

    /*
     * The set of components to be drawn. This field is used only if the
     * target color space is not separable and linear.  It is also used
     * for the devn color values since we may need more than 8 components
     */
    OP_FS_STATE op_state;					/* used to select drawn_comps, fill or stroke */
    gx_color_index  drawn_comps_fill;
    gx_color_index	drawn_comps_stroke;		/* pparams->is_fill_color determines which to set */
    bool retain_none_stroke;                /* These are used to know when we can set the procs to forward */
    bool retain_none_fill;

    /*
     * The mask of gx_color_index bits to be retained during a drawing
     * operation. A bit in this mask is 1 if the corresponding bit or
     * the color index is to be retained; otherwise it is 0.
     *
     * The "non-drawn" region of the drawing gx_color_index is assumed
     * to have the value zero, so for a separable and linear color
     * encoding, the per-pixel drawing operation is:
     *
     *  output = (output & retain_mask) | src
     *
     * (For the fully general case, replace src by (src & ~retain_mask).)
     * Because of byte-alignment, byte-order and performance consideration,
     * the actually implement operation may be more complex, but this does
     * not change the overall effect.
     *
     * The actual value of retain_mask will be byte swap if this is
     * required. It will be required if depth > 8 and the host processor
     * is little-endian.
     */
    gx_color_index  retain_mask_fill;
    gx_color_index	retain_mask_stroke;

    bool copy_alpha_hl;

    /* We hold 3 sets of device procedures here. These are initialised from
     * the equivalently named globals when the device is created, but are
     * then used from here as we fiddle with them. This ensures that the
     * globals are only ever read, and as such are safe in multithreaded
     * environments. */
    gx_device_procs generic_overprint_procs;
    gx_device_procs no_overprint_procs;
    gx_device_procs sep_overprint_procs;

    /* Due to the setting of stroke and fill overprint we can get in
       a situation where one makes the device idle.  We need to know
       if that is the case when doing a compositor push even when
       no parameters have changed */
    bool is_idle;

} overprint_device_t;

gs_private_st_suffix_add0_final( st_overprint_device_t,
                                 overprint_device_t,
                                 "overprint_device_t",
                                 overprint_device_t_enum_ptrs,
                                 overprint_device_t_reloc_ptrs,
                                 gx_device_finalize,
                                 st_device_forward);

/*
 * In the default (overprint false) case, the overprint device is almost
 * a pure forwarding device: only the open_device and create_compositor
 * methods are not pure-forwarding methods. The
 * gx_device_foward_fill_in_procs procedure does not fill in all of the
 * necessary procedures, so some of them are provided explicitly below.
 * The put_params procedure also requires a small modification, so that
 * the open/close state of this device always reflects that of its
 * target.
 *
 * This and other method arrays are not declared const so that they may
 * be initialized once via gx_device_forward_fill_in_procs. They are
 * constant once this initialization is complete.
 */
static dev_proc_open_device(overprint_open_device);
static dev_proc_put_params(overprint_put_params);
static dev_proc_get_page_device(overprint_get_page_device);
static dev_proc_create_compositor(overprint_create_compositor);
static dev_proc_get_color_comp_index(overprint_get_color_comp_index);
static dev_proc_fill_stroke_path(overprint_fill_stroke_path);
static dev_proc_fill_path(overprint_fill_path);
static dev_proc_stroke_path(overprint_stroke_path);
static dev_proc_text_begin(overprint_text_begin);
static  dev_proc_dev_spec_op(overprint_dev_spec_op);

static const gx_device_procs no_overprint_procs = {
    overprint_open_device,              /* open_device */
    0,                                  /* get_initial_matrix */
    0,                                  /* sync_output */
    0,                                  /* output_page */
    0,                                  /* close_device */
    0,                                  /* map_rgb_color */
    0,                                  /* map_color_rgb */
    gx_forward_fill_rectangle,          /* fill_rectangle */
    gx_forward_tile_rectangle,          /* tile_rectangle */
    gx_forward_copy_mono,               /* copy_mono */
    gx_forward_copy_color,              /* copy_color */
    0,                                  /* draw_line (obsolete) */
    0,                                  /* get_bits */
    0,                                  /* get_params */
    overprint_put_params,               /* put_params */
    0,                                  /* map_cmyk_color */
    0,                                  /* get_xfont_procs */
    0,                                  /* get_xfont_device */
    0,                                  /* map_rgb_alpha_color */
    overprint_get_page_device,          /* get_page_device */
    0,                                  /* get_alpha_bits (obsolete) */
    0,                                  /* copy alpha */
    0,                                  /* get_band */
    0,                                  /* copy_rop */
    0,                                  /* fill_path */
    0,                                  /* stroke_path */
    0,                                  /* fill_mask */
    0,                                  /* fill_trapezoid */
    0,                                  /* fill_parallelogram */
    0,                                  /* fill_triangle */
    0,                                  /* draw_thin_line */
    0,                                  /* begin_image */
    0,                                  /* image_data (obsolete) */
    0,                                  /* end_image (obsolete) */
    gx_forward_strip_tile_rectangle,    /* strip_tile_rectangle */
    0,                                  /* strip_copy_rop */
    0,                                  /* get_clipping_box */
    0,                                  /* begin_typed_image */
    0,                                  /* get_bits_rectangle */
    0,                                  /* map_color_rgb_alpha */
    overprint_create_compositor,        /* create_compositor */
    0,                                  /* get_hardware_params */
    0,                                  /* text_begin */
    0,                                  /* gx_finish_copydevice */
    0,                                  /* begin_transparency_group */
    0,                                  /* end_transparency_group */
    0,                                  /* being_transparency_mask */
    0,                                  /* end_transparency_mask */
    0,                                  /* discard_transparency_layer */
    0,                                  /* get_color_mapping_procs */
    overprint_get_color_comp_index,	/* get_color_comp_index */
    0,                                  /* encode_color */
    0,                                  /* decode_color */
    0,					/* pattern_manage */
    0,					/* fill_rectangle_hl_color */
    0,					/* include_color_space */
    0,					/* fill_linear_color_scanline */
    0,					/* fill_linear_color_trapezoid */
    0,					/* fill_linear_color_triangle */
    0,					/* update_spot_equivalent_colors */
    0,					/* ret_devn_params */
    gx_forward_fillpage,
    0,                                  /* push_transparency_state */
    0,                                  /* pop_transparency_state */
    0,                                  /* put_image */
    overprint_dev_spec_op,              /* dev_spec_op */
    gx_forward_copy_planes,
    0,                                  /* get profile */
    0,                                  /* set graphics type tag */
    0,                                  /* strip_copy_rop2 */
    0,                                  /* strip_tile_rect_devn */
    gx_forward_copy_alpha_hl_color,     /* copy_alpha_hl_color */
    NULL,                               /* process_page */\
    NULL,				/* transform_pixel_region */\
    gx_forward_fill_stroke_path,        /* fill_stroke */\
};

/*
 * If overprint is set, the high and mid-level rendering methods are
 * replaced by the default routines. The low-level color rendering methods
 * are replaced with one of two sets of functions, depending on whether or
 * not the target device has a separable and linear color encoding.
 *
 *  1. If the target device does not have a separable and linear
 *     encoding, an overprint-specific fill_rectangle method is used,
 *     and the default methods are used for all other low-level rendering
 *     methods. There is no way to achieve good rendering performance
 *     when overprint is true and the color encoding is not separable
 *     and linear, so there is little reason to use more elaborate
 *     methods int this case.
 *
 *  2. If the target device does have a separable and linear color
 *     model, at least the fill_rectangle method and potentially other
 *     methods will be replaced by overprint-specific methods. Those
 *     methods not replaced will have their default values. The number
 *     of methods replaced is dependent on the desired level of
 *     performance: the more methods, the better the performance.
 *
 *     Note that certain procedures, such as copy_alpha and copy_rop,
 *     are likely to always be given their default values, as the concepts
 *     of alpha-compositing and raster operations are not compatible in
 *     a strict sense.
 */
static dev_proc_fill_rectangle(overprint_generic_fill_rectangle);
static dev_proc_fill_rectangle(overprint_sep_fill_rectangle);
static dev_proc_fill_rectangle_hl_color(overprint_fill_rectangle_hl_color);
static dev_proc_copy_planes(overprint_copy_planes);
static dev_proc_copy_alpha_hl_color(overprint_copy_alpha_hl_color);

/* other low-level overprint_sep_* rendering methods prototypes go here */

static const gx_device_procs generic_overprint_procs = {
    overprint_open_device,              /* open_device */
    0,                                  /* get_initial_matrix */
    0,                                  /* sync_output */
    0,                                  /* output_page */
    0,                                  /* close_device */
    0,                                  /* map_rgb_color */
    0,                                  /* map_color_rgb */
    overprint_generic_fill_rectangle,   /* fill_rectangle */
    gx_default_tile_rectangle,          /* tile_rectangle */
    gx_default_copy_mono,               /* copy_mono */
    gx_default_copy_color,              /* copy_color */
    gx_default_draw_line,               /* draw_line (obsolete) */
    0,                                  /* get_bits */
    0,                                  /* get_params */
    overprint_put_params,               /* put_params */
    0,                                  /* map_cmyk_color */
    0,                                  /* get_xfont_procs */
    gx_default_get_xfont_device,        /* get_xfont_device */
    0,                                  /* map_rgb_alpha_color */
    overprint_get_page_device,          /* get_page_device */
    0,                                  /* get_alpha_bits (obsolete) */
    gx_default_copy_alpha,              /* copy alpha */
    0,                                  /* get_band */
    gx_default_copy_rop,                /* copy_rop */
    overprint_fill_path,                /* fill_path */
    overprint_stroke_path,              /* stroke_path */
    gx_default_fill_mask,               /* fill_mask */
    gx_default_fill_trapezoid,          /* fill_trapezoid */
    gx_default_fill_parallelogram,      /* fill_parallelogram */
    gx_default_fill_triangle,           /* fill_triangle */
    gx_default_draw_thin_line,          /* draw_thin_line */
    gx_default_begin_image,             /* begin_image */
    0,                                  /* image_data (obsolete) */
    0,                                  /* end_image (obsolete) */
    gx_default_strip_tile_rectangle,    /* strip_tile_rectangle */
    gx_default_strip_copy_rop,          /* strip_copy_rop */
    0,                                  /* get_clipping_box */
    gx_default_begin_typed_image,       /* begin_typed_image */
    0,                                  /* get_bits_rectangle */
    0,                                  /* map_color_rgb_alpha */
    overprint_create_compositor,        /* create_compositor */
    0,                                  /* get_hardware_params */
    overprint_text_begin,              /* text_begin */
    0,                                  /* gx_finish_copydevice */
    0,                                  /* begin_transparency_group */
    0,                                  /* end_transparency_group */
    0,                                  /* begin_transparency_mask */
    0,                                  /* end_transparency_mask */
    0,                                  /* discard_transparency_layer */
    0,                                  /* get_color_mapping_procs */
    overprint_get_color_comp_index,	/* get_color_comp_index */
    0,                                  /* encode_color */
    0,                                  /* decode_color */
    0,                                  /* pattern_manage */
    overprint_fill_rectangle_hl_color,  /* fill_rectangle_hl_color */
    0,                                  /* include_color_space */
    0,                                  /* fill_linear_color_scanline */
    0,                                  /* fill_linear_color_trapezoid */
    0,                                  /* fill_linear_color_triangle */
    0,                                  /* update_spot_equivalent_colors */
    0,                                  /* ret_devn_params */
    0,                                  /* fillpage */
    0,                                  /* push_transparency_state */
    0,                                  /* pop_transparency_state */
    0,                                  /* put_image */
    overprint_dev_spec_op,              /* dev_spec_op */
    gx_forward_copy_planes,
    0,                                  /* get profile */
    0,                                  /* set graphics type tag */
    0,                                  /* strip_copy_rop2 */
    0,                                  /* strip_tile_rect_devn */
    gx_forward_copy_alpha_hl_color,     /* copy_alpha_hl_color */
    NULL,                               /* process_page */\
    NULL,				/* transform_pixel_region */\
    overprint_fill_stroke_path,         /* fill_stroke */
};

static const gx_device_procs sep_overprint_procs = {
    overprint_open_device,              /* open_device */
    0,                                  /* get_initial_matrix */
    0,                                  /* sync_output */
    0,                                  /* output_page */
    0,                                  /* close_device */
    0,                                  /* map_rgb_color */
    0,                                  /* map_color_rgb */
    overprint_sep_fill_rectangle,       /* fill_rectangle */
    gx_default_tile_rectangle,          /* tile_rectangle */
    gx_default_copy_mono,               /* copy_mono */
    gx_default_copy_color,              /* copy_color */
    gx_default_draw_line,               /* draw_line (obsolete) */
    0,                                  /* get_bits */
    0,                                  /* get_params */
    overprint_put_params,               /* put_params */
    0,                                  /* map_cmyk_color */
    0,                                  /* get_xfont_procs */
    gx_default_get_xfont_device,        /* get_xfont_device */
    0,                                  /* map_rgb_alpha_color */
    overprint_get_page_device,          /* get_page_device */
    0,                                  /* get_alpha_bits (obsolete) */
    gx_default_copy_alpha,              /* copy alpha */
    0,                                  /* get_band */
    gx_default_copy_rop,                /* copy_rop */
    overprint_fill_path,                /* fill_path */
    overprint_stroke_path,              /* stroke_path */
    gx_default_fill_mask,               /* fill_mask */
    gx_default_fill_trapezoid,          /* fill_trapezoid */
    gx_default_fill_parallelogram,      /* fill_parallelogram */
    gx_default_fill_triangle,           /* fill_triangle */
    gx_default_draw_thin_line,          /* draw_thin_line */
    gx_default_begin_image,              /* begin_image */
    0,                                  /* image_data (obsolete) */
    0,                                  /* end_image (obsolete) */
    gx_default_strip_tile_rectangle,    /* strip_tile_rectangle */
    gx_default_strip_copy_rop,          /* strip_copy_rop */
    0,                                  /* get_clipping_box */
    gx_default_begin_typed_image,       /* begin_typed_image */
    0,                                  /* get_bits_rectangle */
    0,                                  /* map_color_rgb_alpha */
    overprint_create_compositor,        /* create_compositor */
    0,                                  /* get_hardware_params */
    overprint_text_begin,               /* text_begin */
    0,                                  /* gx_finish_copydevice */
    0,                                  /* begin_transparency_group */
    0,                                  /* end_transparency_group */
    0,                                  /* begin_transparency_mask */
    0,                                  /* end_transparency_mask */
    0,                                  /* discard_transparency_layer */
    0,                                  /* get_color_mapping_procs */
    overprint_get_color_comp_index,	    /* get_color_comp_index */
    0,                                  /* encode_color */
    0,                                  /* decode_color */
    0,                                  /* pattern_manage */
    overprint_fill_rectangle_hl_color,  /* fill_rectangle_hl_color */
    0,                                  /* include_color_space */
    0,                                  /* fill_linear_color_scanline */
    0,                                  /* fill_linear_color_trapezoid */
    0,                                  /* fill_linear_color_triangle */
    0,                                  /* update_spot_equivalent_colors */
    0,                                  /* ret_devn_params */
    0,                                  /* fillpage */
    0,                                  /* push_transparency_state */
    0,                                  /* pop_transparency_state */
    0,                                  /* put_image */
    overprint_dev_spec_op,              /* dev_spec_op */
    overprint_copy_planes,              /* copy planes */
    0,                                  /* get profile */
    0,                                  /* set graphics type tag */
    0,                                  /* strip_copy_rop2 */
    0,                                  /* strip_tile_rect_devn */
    overprint_copy_alpha_hl_color,      /* copy_alpha_hl_color */
    NULL,                               /* process_page */\
    NULL,				/* transform_pixel_region */\
    overprint_fill_stroke_path,         /* fill_stroke */
};

/*
 * The prototype for the overprint device does not provide much
 * information; it exists primarily to facilitate use gx_init_device
 * and sundry other device utility routines.
 */
const overprint_device_t    gs_overprint_device = {
    std_device_std_body_open( overprint_device_t,   /* device type */
                              0,                    /* static_procs */
                              "overprint_device",   /* dname */
                              0, 0,                 /* width, height */
                              1, 1 ),               /* HWResolution */
    { 0 }                                           /* procs */
};

/*
 * Utility to reorder bytes in a color or mask based on the endianness of
 * the current device. This is required on little-endian machines if the
 * depth is larger 8. The resulting value is also replicated to fill the
 * entire gx_color_index if the depth is a divisor of the color index
 * size. If this is not the case, the result will be in the low-order
 * bytes of the color index.
 *
 * Though this process can be handled in full generality, the code below
 * takes advantage of the fact that depths that are > 8 must be a multiple
 * of 8 and <= 64
 */
#if !ARCH_IS_BIG_ENDIAN

static gx_color_index
swap_color_index(int depth, gx_color_index color)
{
    int             shift = depth - 8;
    gx_color_index  mask = 0xff;

    color =  ((color >> shift) & mask)
           | ((color & mask) << shift)
           | (color & ~((mask << shift) | mask));
    if (depth > 24) {
        shift -= 16;
        mask <<= 8;
        color =  ((color >> shift) & mask)
               | ((color & mask) << shift)
               | (color & ~((mask << shift) | mask));

        if (depth > 40) {
            shift -= 16;
            mask <<= 8;
            color =  ((color >> shift) & mask)
                   | ((color & mask) << shift)
                   | (color & ~((mask << shift) | mask));

            if (depth > 56) {
                shift -= 16;
                mask <<= 8;
                color =  ((color >> shift) & mask)
                       | ((color & mask) << shift)
                       | (color & ~((mask << shift) | mask));
            }
        }
    }

    return color;
}

#endif  /* !ARCH_IS_BIG_ENDIAN */

/*
 * Update the retain_mask field to reflect the information in the
 * drawn_comps field. This is useful only if the device color model
 * is separable.
 */
static void
set_retain_mask(overprint_device_t * opdev, bool is_fill_color)
{
    uchar i, ncomps = opdev->color_info.num_components;
    gx_color_index  drawn_comps = is_fill_color ?
                                  opdev->drawn_comps_fill : opdev->drawn_comps_stroke;
    gx_color_index retain_mask = 0;
#if !ARCH_IS_BIG_ENDIAN
    int depth = opdev->color_info.depth;
#endif

    for (i = 0; i < ncomps; i++, drawn_comps >>= 1) {
        if ((drawn_comps & 0x1) == 0)
            retain_mask |= opdev->color_info.comp_mask[i];
    }
#if !ARCH_IS_BIG_ENDIAN
    if (depth > 8)
        retain_mask = swap_color_index(depth, retain_mask);
#endif
    if (is_fill_color)
        opdev->retain_mask_fill = retain_mask;
    else
        opdev->retain_mask_stroke = retain_mask;
}

/*
 * Update the overprint-specific device parameters.
 *
 * If spot colors are to be retain, the set of process (non-spot) colors is
 * determined by mapping through the standard color spaces and check which
 * components assume non-zero values.
 */
static int
update_overprint_params(
    overprint_device_t* opdev,
    const gs_overprint_params_t* pparams)
{
    /* We can only turn off the overprint compositor if
       BOTH the stroke and fill op are false.  Otherwise
       we will turn it off when setting one and turn on
       when setting the other (or vice versa) */

    /* Note if pparams is to set the opdev fill stroke state.  Do that now and exit */
    if (pparams->op_state != OP_STATE_NONE) {
        opdev->op_state = pparams->op_state;
        return 0;
    }

    if_debug4m(gs_debug_flag_overprint, opdev->memory,
        "[overprint] update_overprint_params enter. retain_any_comps = %d, idle = %d, drawn_comps = 0x%x, is_fill_color = %d\n",
        pparams->retain_any_comps, pparams->idle, pparams->drawn_comps, pparams->is_fill_color);

    /* check if overprint is to be turned off */
    if (!pparams->retain_any_comps || pparams->idle) {
        if (pparams->is_fill_color) {
            opdev->retain_none_fill = true;
            opdev->drawn_comps_fill =
                ((gx_color_index)1 << (opdev->color_info.num_components)) - (gx_color_index)1;
        } else {
            opdev->retain_none_stroke = true;
            opdev->drawn_comps_stroke =
                ((gx_color_index)1 << (opdev->color_info.num_components)) - (gx_color_index)1;
        }

        /* Set to forward only if both stroke and fill are not retaining any
           and if we have not already set it to forward */
        if (dev_proc(opdev, fill_rectangle) != gx_forward_fill_rectangle &&
            opdev->retain_none_fill && opdev->retain_none_stroke) {
            memcpy(&opdev->procs,
                &opdev->no_overprint_procs,
                sizeof(opdev->no_overprint_procs));
            opdev->is_idle = true;
            if_debug0m(gs_debug_flag_overprint, opdev->memory,
                "[overprint] overprint fill_rectangle set to forward\n");
        }

        if_debug4m(gs_debug_flag_overprint, opdev->memory,
            "[overprint] update_overprint_params exit. drawn_comps_fill = 0x%x, drawn_comps_stroke = 0x%x, retain_none_fill = %d, retain_none_stroke = %d \n",
            opdev->drawn_comps_fill, opdev->drawn_comps_stroke, opdev->retain_none_fill, opdev->retain_none_stroke);
        return 0;
    }

    opdev->is_idle = false;
    /* set the procedures according to the color model */
    if (colors_are_separable_and_linear(&opdev->color_info)) {
        memcpy(&opdev->procs, &opdev->sep_overprint_procs,
            sizeof(opdev->sep_overprint_procs));
        if_debug0m(gs_debug_flag_overprint, opdev->memory,
            "[overprint] overprint procs set to sep\n");
    } else {
        memcpy(&opdev->procs, &opdev->generic_overprint_procs,
            sizeof(opdev->generic_overprint_procs));
        if_debug0m(gs_debug_flag_overprint, opdev->memory,
            "[overprint] overprint procs set to generic\n");
    }

    if (pparams->is_fill_color) {
        opdev->retain_none_fill = false;
        opdev->drawn_comps_fill = pparams->drawn_comps;
    } else {
        opdev->retain_none_stroke = false;
        opdev->drawn_comps_stroke = pparams->drawn_comps;
    }

    if_debug4m(gs_debug_flag_overprint, opdev->memory,
        "[overprint] update_overprint_params exit. drawn_comps_fill = 0x%x, drawn_comps_stroke = 0x%x, retain_none_fill = %d, retain_none_stroke = %d \n",
        opdev->drawn_comps_fill, opdev->drawn_comps_stroke, opdev->retain_none_fill, opdev->retain_none_stroke);

    /* if appropriate, update the retain_mask field */
    if (colors_are_separable_and_linear(&opdev->color_info))
        set_retain_mask(opdev, pparams->is_fill_color);

    return 0;
}

/*
 * The open_device method for the overprint device is about as close to
 * a pure "forwarding" open_device operation as is possible. Its only
 * significant function is to ensure that the is_open field of the
 * overprint device matches that of the target device.
 *
 * We assume this procedure is called only if the device is not already
 * open, and that gs_opendevice will take care of the is_open flag.
 */
static int
overprint_open_device(gx_device * dev)
{
    overprint_device_t *    opdev = (overprint_device_t *)dev;
    gx_device *             tdev = opdev->target;
    int                     code = 0;

    /* the overprint device must have a target */
    if (tdev == 0)
        return_error(gs_error_unknownerror);
    if ((code = gs_opendevice(tdev)) >= 0) {
        gx_device_copy_params(dev, tdev);
        opdev->copy_alpha_hl = false;
        opdev->is_idle = false;
    }
    return code;
}

/*
 * The put_params method for the overprint device will check if the
 * target device has closed and, if so, close itself.
 */
static int
overprint_put_params(gx_device * dev, gs_param_list * plist)
{
    overprint_device_t *    opdev = (overprint_device_t *)dev;
    gx_device *             tdev = opdev->target;
    int                     code = 0;

    if (tdev != 0 && (code = dev_proc(tdev, put_params)(tdev, plist)) >= 0) {
        gx_device_decache_colors(dev);
        if (!tdev->is_open)
            code = gs_closedevice(dev);
    }
    return code;
}

/*
 * If the target device 'auto detects' new spot colors, then it will
 * change its color_info data.  Make sure that we have a current copy.
 */
int
overprint_get_color_comp_index(gx_device * dev, const char * pname,
                                        int name_size, int component_type)
{
    overprint_device_t * opdev = (overprint_device_t *)dev;
    gx_device * tdev = opdev->target;
    int code;

    if (tdev == 0)
        code = gx_error_get_color_comp_index(dev, pname,
                                name_size, component_type);
    else {
        code = dev_proc(tdev, get_color_comp_index)(tdev, pname,
                                name_size, component_type);
        opdev->color_info = tdev->color_info;
    }
    return code;
}

/*
 * The overprint device must never be confused with a page device.
 * Thus, we always forward the request for the page device to the
 * target, as should all forwarding devices.
 */
static gx_device *
overprint_get_page_device(gx_device * dev)
{
    overprint_device_t *    opdev = (overprint_device_t *)dev;
    gx_device *             tdev = opdev->target;

    return tdev == 0 ? 0 : dev_proc(tdev, get_page_device)(tdev);
}

/*
 * Calling create_compositor on the overprint device just updates the
 * overprint parameters; no new device is created.
 */
static int
overprint_create_compositor(
    gx_device *             dev,
    gx_device **            pcdev,
    const gs_composite_t *  pct,
    gs_gstate *	            pgs,
    gs_memory_t *           memory,
    gx_device *             cdev)
{
    if (pct->type != &gs_composite_overprint_type)
        return gx_default_create_compositor(dev, pcdev, pct, pgs, memory, cdev);
    else {
        gs_overprint_params_t params = ((const gs_overprint_t *)pct)->params;
        overprint_device_t *opdev = (overprint_device_t *)dev;
        int     code = 0;
        bool update;

        if (params.is_fill_color)
            update = (params.drawn_comps != opdev->drawn_comps_fill) ||
            ((params.retain_any_comps == 0) != opdev->retain_none_fill);
        else
            update = (params.drawn_comps != opdev->drawn_comps_stroke) ||
            ((params.retain_any_comps == 0) != opdev->retain_none_stroke);

        params.idle = pct->idle;
        /* device must already exist, so just update the parameters if settings change */
        if_debug6m(gs_debug_flag_overprint, opdev->memory,
            "[overprint] overprint_create_compositor test for change. params.idle = %d vs. opdev->is_idle = %d \n  params.is_fill_color = %d: params.drawn_comps = 0x%x vs. opdev->drawn_comps_fill =  0x%x OR opdev->drawn_comps_stroke = 0x%x\n",
            params.idle, opdev->is_idle, params.is_fill_color, params.drawn_comps, opdev->drawn_comps_fill, opdev->drawn_comps_stroke);

        if (update || params.idle != opdev->is_idle || params.op_state != OP_STATE_NONE)
            code = update_overprint_params(opdev, &params);
        if (code >= 0)
            *pcdev = dev;
        return code;
    }
}

/*
 * The two rectangle-filling routines (which do the actual work) are just
 * stubbs for the time being. The actual routines would allocate a buffer,
 * use get_bits_rectangle to build a buffer of the existing data, modify
 * the appropriate components, then invoke the copy_color procedure on the
 * target device.
 */
static int
overprint_generic_fill_rectangle(
    gx_device *     dev,
    int             x,
    int             y,
    int             width,
    int             height,
    gx_color_index  color )
{
    overprint_device_t *    opdev = (overprint_device_t *)dev;
    gx_device *             tdev = opdev->target;

    if (tdev == 0)
        return 0;
    else {

        assert(opdev->op_state != 0);

        /* See if we even need to do any overprinting.  We have to maintain
           the compositor active for fill/stroke cases even if we are only
           doing a fill or a stroke */
        if ((opdev->op_state == OP_STATE_FILL && opdev->retain_none_fill) ||
            (opdev->op_state == OP_STATE_STROKE && opdev->retain_none_stroke))
            return (*dev_proc(tdev, fill_rectangle)) (tdev, x, y, width, height, color);

        return gx_overprint_generic_fill_rectangle(tdev,
            opdev->op_state == OP_STATE_FILL ?
            opdev->drawn_comps_fill : opdev->drawn_comps_stroke,
            x, y, width, height, color, dev->memory);
    }
}

static int
overprint_copy_alpha_hl_color(gx_device * dev, const byte * data, int data_x,
           int raster, gx_bitmap_id id, int x, int y, int width, int height,
                      const gx_drawing_color *pdcolor, int depth)
{
    /* copy_alpha_hl_color will end up calling copy_planes which for the
       copy alpha case we need to make sure we do in a proper overprint
       fashion.  Other calls of copy_alpha for example from the pattern
       tiling call are not done with overprint control.  So we set an
       appopriate flag so that we know to handle this properly when we
       get to copy_alpha */

    overprint_device_t *    opdev = (overprint_device_t *)dev;
    int code;

    opdev->copy_alpha_hl = true;
    code = gx_default_copy_alpha_hl_color(dev, data, data_x, raster, id, x, y,
                                          width, height, pdcolor, depth);
    opdev->copy_alpha_hl = false;
    return code;
}

/* Currently we really should only be here if the target device is planar
   AND it supports devn colors AND is 8 bit.  This could use a rewrite to
   make if more efficient but I had to get something in place that would
   work */
static int
overprint_copy_planes(gx_device * dev, const byte * data, int data_x, int raster_in,
                  gx_bitmap_id id, int x, int y, int w, int h, int plane_height)
{
    overprint_device_t *    opdev = (overprint_device_t *)dev;
    gx_device *             tdev = opdev->target;
    byte *                  gb_buff = 0;
    gs_get_bits_params_t    gb_params;
    gs_int_rect             gb_rect;
    int                     code = 0;
    unsigned int            raster;
    int                     byte_depth;
    int                     depth;
    uchar                   num_comps;
    uchar                   k,j;
    gs_memory_t *           mem = dev->memory;
    gx_color_index          comps = opdev->op_state == OP_STATE_FILL ? opdev->drawn_comps_fill : opdev->drawn_comps_stroke;
    byte                    *curr_data = (byte *) data + data_x;
    int                     row, offset;

    if (tdev == 0)
        return 0;

    if  (opdev->copy_alpha_hl) {
       /* We are coming here via copy_alpha_hl_color due to the use of AA.
          We will want to handle the overprinting here */

        depth = tdev->color_info.depth;
        num_comps = tdev->color_info.num_components;

        fit_fill(tdev, x, y, w, h);
        byte_depth = depth / num_comps;

        /* allocate a buffer for the returned data */
        raster = bitmap_raster(w * byte_depth);
        gb_buff = gs_alloc_bytes(mem, raster * num_comps , "overprint_copy_planes");
        if (gb_buff == 0)
            return gs_note_error(gs_error_VMerror);

        /* Initialize the get_bits parameters. Here we just get a plane at a  time. */
        gb_params.options =  GB_COLORS_NATIVE
                           | GB_ALPHA_NONE
                           | GB_DEPTH_ALL
                           | GB_PACKING_PLANAR
                           | GB_RETURN_COPY
                           | GB_ALIGN_STANDARD
                           | GB_OFFSET_0
                           | GB_RASTER_STANDARD
                           | GB_SELECT_PLANES;

        gb_params.x_offset = 0;
        gb_params.raster = raster;
        gb_rect.p.x = x;
        gb_rect.q.x = x + w;

        /* step through the height */
        row = 0;
        while (h-- > 0 && code >= 0) {
            gb_rect.p.y = y++;
            gb_rect.q.y = y;
            offset = row * raster_in + data_x;
            row++;
            curr_data = (byte *) data + offset; /* start us at the start of row */
            /* And now through each plane */
            for (k = 0; k < tdev->color_info.num_components; k++) {
                /* First set the params to zero for all planes except the one we want */
                for (j = 0; j < tdev->color_info.num_components; j++)
                        gb_params.data[j] = 0;
                    gb_params.data[k] = gb_buff + k * raster;
                    code = dev_proc(tdev, get_bits_rectangle) (tdev, &gb_rect,
                                                               &gb_params, 0);
                    if (code < 0) {
                        gs_free_object(mem, gb_buff, "overprint_copy_planes" );
                        return code;
                    }
                    /* Skip the plane if this component is not to be drawn.  If
                       its the one that we want to draw, replace it with our
                       buffer data */
                    if ((comps & 0x01) == 1) {
                        memcpy(gb_params.data[k], curr_data, w);
                    }
                    /* Next plane */
                    curr_data += plane_height * raster_in;
                    comps >>= 1;
            }
            code = dev_proc(tdev, copy_planes)(tdev, gb_buff, 0, raster,
                                               gs_no_bitmap_id, x, y - 1, w, 1, 1);
        }
        gs_free_object(mem, gb_buff, "overprint_copy_planes" );
        return code;
    } else {
        /* This is not a case where copy planes should be doing overprinting.
           For example, if we came here via the pattern tiling code, so just
           pass this along to the target */
        return (*dev_proc(tdev, copy_planes)) (tdev, data, data_x, raster_in, id,
                                               x, y, w, h, plane_height);
    }
}
static void
my_memset16_be(uint16_t *dst, uint16_t col, size_t w)
{
#if !ARCH_IS_BIG_ENDIAN
    col = (col>>8) | (col<<8);
#endif
    while (w--) {
        *dst++ = col;
    }
}

/* Currently we really should only be here if the target device is planar
   AND it supports devn colors AND is 8 or 16 bit. */
static int
overprint_fill_rectangle_hl_color(gx_device *dev,
    const gs_fixed_rect *rect, const gs_gstate *pgs,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    overprint_device_t *    opdev = (overprint_device_t *)dev;
    gx_device *             tdev = opdev->target;
    byte *                  gb_buff = 0;
    gs_get_bits_params_t    gb_params;
    gs_int_rect             gb_rect;
    int                     code = 0;
    unsigned int            raster;
    int                     byte_depth;
    int                     depth;
    uchar                   num_comps;
    int                     x, y, w, h;
    uchar                   k, j;
    gs_memory_t *           mem = dev->memory;
    gx_color_index          comps;
    gx_color_index          mask;
    int                     shift;
    int                     deep;

    if (tdev == 0)
        return 0;

    assert(opdev->op_state != 0);

    /* See if we even need to do any overprinting.  We have to maintain
       the compositor active for fill/stroke cases even if we are only
       doing a fill or a stroke */
    if ((opdev->op_state == OP_STATE_FILL && opdev->retain_none_fill) ||
        (opdev->op_state == OP_STATE_STROKE && opdev->retain_none_stroke))
        return (*dev_proc(tdev, fill_rectangle_hl_color)) (tdev, rect, pgs, pdcolor, pcpath);

    depth = tdev->color_info.depth;
    num_comps = tdev->color_info.num_components;

    x = fixed2int(rect->p.x);
    y = fixed2int(rect->p.y);
    w = fixed2int(rect->q.x) - x;
    h = fixed2int(rect->q.y) - y;

    fit_fill(tdev, x, y, w, h);
    byte_depth = depth / num_comps;
    mask = ((gx_color_index)1 << byte_depth) - 1;
    shift = 16 - byte_depth;
    deep = byte_depth == 16;

    /* allocate a buffer for the returned data */
    raster = bitmap_raster(w * byte_depth);
    gb_buff = gs_alloc_bytes(mem, raster * num_comps , "overprint_fill_rectangle_hl_color");
    if (gb_buff == 0)
        return gs_note_error(gs_error_VMerror);

    /* Initialize the get_bits parameters. Here we just get a plane at a  time. */
    gb_params.options =  GB_COLORS_NATIVE
                       | GB_ALPHA_NONE
                       | GB_DEPTH_ALL
                       | GB_PACKING_PLANAR
                       | GB_RETURN_COPY
                       | GB_ALIGN_STANDARD
                       | GB_OFFSET_0
                       | GB_RASTER_STANDARD
                       | GB_SELECT_PLANES;

    gb_params.x_offset = 0;     /* for consistency */
    gb_params.raster = raster;
    gb_rect.p.x = x;
    gb_rect.q.x = x + w;

    /* step through the height */
    while (h-- > 0 && code >= 0) {
        gb_rect.p.y = y++;
        gb_rect.q.y = y;
        comps = opdev->op_state == OP_STATE_FILL ? opdev->drawn_comps_fill : opdev->drawn_comps_stroke;
        /* And now through each plane */
        for (k = 0; k < tdev->color_info.num_components; k++) {
            /* First set the params to zero for all planes except the one we want */
            for (j = 0; j < tdev->color_info.num_components; j++)
                gb_params.data[j] = 0;
            gb_params.data[k] = gb_buff + k * raster;
            code = dev_proc(tdev, get_bits_rectangle) (tdev, &gb_rect,
                                                       &gb_params, 0);
            if (code < 0) {
                gs_free_object(mem, gb_buff,
                               "overprint_fill_rectangle_hl_color" );
                return code;
            }
            /* Skip the plane if this component is not to be drawn.  We have
                to do a get bits for each plane due to the fact that we have
                to do a copy_planes at the end.  If we had a copy_plane
                operation we would just get the ones needed and set those. */
            if ((comps & 0x01) == 1) {
                /* Not sure if a loop or a memset is better here */
                if (deep)
                    my_memset16_be((uint16_t *)(gb_params.data[k]),
                                   pdcolor->colors.devn.values[k], w);
                else
                    memset(gb_params.data[k],
                           ((pdcolor->colors.devn.values[k]) >> shift & mask), w);
            }
            comps >>= 1;
        }
        code = dev_proc(tdev, copy_planes)(tdev, gb_buff, 0, raster,
                                           gs_no_bitmap_id, x, y - 1, w, 1, 1);
    }
    gs_free_object(mem, gb_buff,
                    "overprint_fill_rectangle_hl_color" );
    return code;
}

static int
overprint_sep_fill_rectangle(
    gx_device *     dev,
    int             x,
    int             y,
    int             width,
    int             height,
    gx_color_index  color )
{
    overprint_device_t *    opdev = (overprint_device_t *)dev;
    gx_device *             tdev = opdev->target;

    if (tdev == 0)
        return 0;
    else {
        int     depth = tdev->color_info.depth;

        assert(opdev->op_state != 0);

        /* See if we even need to do any overprinting.  We have to maintain
           the compositor active for fill/stroke cases even if we are only
           doing a fill or a stroke */
        if ((opdev->op_state == OP_STATE_FILL && opdev->retain_none_fill) ||
            (opdev->op_state == OP_STATE_STROKE && opdev->retain_none_stroke))
            return (*dev_proc(tdev, fill_rectangle)) (tdev, x, y, width, height, color);

        /*
         * Swap the color index into the order required by a byte-oriented
         * bitmap. This is required only for littl-endian processors, and
         * then only if the depth > 8.
         */
#if !ARCH_IS_BIG_ENDIAN
        if (depth > 8)
            color = swap_color_index(depth, color);
#endif

        /*
         * We can handle rectangle filling via bits_fill_rectangle_masked
         * if the depth is a divisor of 8 * sizeof(mono_fill_chunk). The
         * non-masked fill_rectangle code uses a byte-oriented routine
         * if depth > 8, but there is not much advantage to doing so if
         * masking is required.
         *
         * Directly testing (8 * sizeof(mono_fill_chunk)) % depth is
         * potentially expensive, since many rectangles are small. We
         * can avoid the modulus operation by noting that
         * 8 * sizeof(mono_fill_chunk) will be a power of 2, and so
         * we need only check that depth is a power of 2 and
         * depth < 8 * sizeof(mono_fill_chunk).
         */
        if ( depth <= 8 * sizeof(mono_fill_chunk) && (depth & (depth - 1)) == 0)
            return gx_overprint_sep_fill_rectangle_1(tdev, opdev->op_state == OP_STATE_FILL ?
                                                     opdev->retain_mask_fill : opdev->retain_mask_stroke,
                                                     x, y, width, height,
                                                     color, dev->memory);
        else
            return gx_overprint_sep_fill_rectangle_2(tdev, opdev->op_state == OP_STATE_FILL ?
                                                     opdev->retain_mask_fill : opdev->retain_mask_stroke,
                                                     x, y, width, height,
                                                     color, dev->memory);
    }
}

/* We need this to ensure the device knows we are doing a fill */
static int
overprint_fill_path(gx_device* pdev, const gs_gstate* pgs,
    gx_path* ppath, const gx_fill_params* params_fill,
    const gx_device_color* pdcolor, const gx_clip_path* pcpath)
{
    overprint_device_t* opdev = (overprint_device_t*)pdev;

    opdev->op_state = OP_STATE_FILL;
    return gx_default_fill_path(pdev, pgs, ppath, params_fill,
                                         pdcolor, pcpath);
}

/* We need this to ensure the device knows we are doing a stroke */
static int
overprint_stroke_path(gx_device* pdev, const gs_gstate* pgs,
    gx_path* ppath, const gx_stroke_params* params_stroke,
    const gx_device_color* pdcolor, const gx_clip_path* pcpath)
{
    overprint_device_t* opdev = (overprint_device_t*)pdev;
    int code;

    opdev->op_state = OP_STATE_STROKE;

    /* Stroke methods use fill path so set that to default to
       avoid mix up of is_fill_color */
    opdev->procs.fill_path = gx_default_fill_path;
    code = gx_default_stroke_path(pdev, pgs, ppath, params_stroke,
        pdcolor, pcpath);
    opdev->procs.fill_path = overprint_fill_path;

    return code;
}

/*
 *	Cannot use default_fill_stroke_path because we need to set the is_fill_color
 */
static int
overprint_fill_stroke_path(gx_device * pdev, const gs_gstate * pgs,
                           gx_path * ppath,
                           const gx_fill_params * params_fill,
                           const gx_device_color * pdevc_fill,
                           const gx_stroke_params * params_stroke,
                           const gx_device_color * pdevc_stroke,
                           const gx_clip_path * pcpath)
{
    int code;
    overprint_device_t *opdev = (overprint_device_t *)pdev;

    opdev->op_state = OP_STATE_FILL;
    code = dev_proc(pdev, fill_path)(pdev, pgs, ppath, params_fill, pdevc_fill, pcpath);
    if (code < 0)
        return code;

    /* Set up for stroke */
    opdev->op_state = OP_STATE_STROKE;
    code = dev_proc(pdev, stroke_path)(pdev, pgs, ppath, params_stroke, pdevc_stroke, pcpath);
    return code;
}

/* We need to make sure we are set up properly based upon the text mode */
static int
overprint_text_begin(gx_device* dev, gs_gstate* pgs,
    const gs_text_params_t* text, gs_font* font,
    gx_path* path, const gx_device_color* pdcolor,
    const gx_clip_path* pcpath,
    gs_memory_t* mem, gs_text_enum_t** ppte)
{
    overprint_device_t* opdev = (overprint_device_t*)dev;

    if (pgs->text_rendering_mode == 0)
        opdev->op_state = OP_STATE_FILL;
    else if (pgs->text_rendering_mode == 1)
        opdev->op_state = OP_STATE_STROKE;

    return gx_default_text_begin(dev, pgs, text, font,
        path, pdcolor, pcpath, mem, ppte);
}

static int
overprint_dev_spec_op(gx_device* pdev, int dev_spec_op,
    void* data, int size)
{
    overprint_device_t* opdev = (overprint_device_t*)pdev;
    gx_device* tdev = opdev->target;

    if (tdev == 0)
        return 0;

    if (dev_spec_op == gxdso_overprint_active)
        return !opdev->is_idle;

    return dev_proc(tdev, dev_spec_op)(tdev, dev_spec_op, data, size);
}

/* complete a procedure set */
static void
fill_in_procs(gx_device_procs * pprocs)
{
    gx_device_forward   tmpdev;

    /*
     * gx_device_forward_fill_in_procs calls gx_device_fill_in_procs, which
     * requires the color_info field of the device be set to "reasonable"
     * values. Which values is irrelevant in this case, but they must not
     * contain dangling pointers, excessive numbers of components, etc.
     */
    memcpy( &tmpdev.color_info,
            &gs_overprint_device.color_info,
            sizeof(tmpdev.color_info) );
    /*
     * Prevent the check_device_separable routine from executing while we
     * fill in the procs.  Our tmpdev is not complete enough for it.
     */
    tmpdev.color_info.separable_and_linear = GX_CINFO_SEP_LIN_NONE;
    tmpdev.static_procs = 0;
    memcpy(&tmpdev.procs, pprocs, sizeof(tmpdev.procs));
    gx_device_forward_fill_in_procs(&tmpdev);
    memcpy(pprocs, &tmpdev.procs, sizeof(tmpdev.procs));
}

/*
 * Create an overprint compositor.
 *
 * Note that this routine will be called only if the device is not already
 * an overprint compositor. Hence, if pct->params.retain_any_comps is
 * false, we can just return.
 */
static int
c_overprint_create_default_compositor(
    const gs_composite_t *  pct,
    gx_device **            popdev,
    gx_device *             tdev,
    gs_gstate *	            pgs,
    gs_memory_t *           mem )
{
    const gs_overprint_t *  ovrpct = (const gs_overprint_t *)pct;
    overprint_device_t *    opdev = 0;
    gs_overprint_params_t params;

    /* see if there is anything to do */
    if ( !ovrpct->params.retain_any_comps) {
        *popdev = tdev;
        return 0;
    }
    if (pct->idle) {
        *popdev = tdev;
        return 0;
    }

    /* build the overprint device */
    opdev = gs_alloc_struct_immovable(mem,
                                      overprint_device_t,
                                      &st_overprint_device_t,
                                      "create overprint compositor" );
    *popdev = (gx_device *)opdev;
    if (opdev == NULL)
        return_error(gs_error_VMerror);
    gx_device_init((gx_device *)opdev,
                   (const gx_device *)&gs_overprint_device,
                   mem,
                   false );
    memcpy(&opdev->no_overprint_procs,
           &no_overprint_procs,
           sizeof(no_overprint_procs));
    memcpy(&opdev->generic_overprint_procs,
           &generic_overprint_procs,
           sizeof(generic_overprint_procs));
    memcpy(&opdev->sep_overprint_procs,
           &sep_overprint_procs,
           sizeof(sep_overprint_procs));
    fill_in_procs(&opdev->no_overprint_procs);
    fill_in_procs(&opdev->generic_overprint_procs);
    fill_in_procs(&opdev->sep_overprint_procs);

    gx_device_copy_params((gx_device *)opdev, tdev);
    gx_device_set_target((gx_device_forward *)opdev, tdev);
    opdev->pad = tdev->pad;
    opdev->log2_align_mod = tdev->log2_align_mod;
    opdev->is_planar = tdev->is_planar;
    if (opdev->is_planar)
        opdev->generic_overprint_procs.copy_alpha_hl_color = overprint_copy_alpha_hl_color;

    params = ovrpct->params;
    params.idle = ovrpct->idle;

    /* Initialize the stroke and fill states */
    opdev->retain_none_fill = true;
    opdev->retain_none_stroke = true;

    /* set up the overprint parameters */
    return update_overprint_params(opdev, &params);
}
