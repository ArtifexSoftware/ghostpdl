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

/* Definitions and interface for PDF 1.4 rendering device */

#ifndef gdevp14_INCLUDED
#  define gdevp14_INCLUDED

#include "gxcmap.h"
#include "gsmatrix.h"
#include "gxcolor2.h"
#include "gxdcolor.h"
#include "gxpcolor.h"
#include "gdevdevn.h"
#include "gsgstate.h"

typedef enum {
    PDF14_DeviceGray = 0,
    PDF14_DeviceRGB = 1,
    PDF14_DeviceCMYK = 2,
    PDF14_DeviceCMYKspot = 3,
    PDF14_DeviceCustom = 4
} pdf14_default_colorspace_t;

typedef enum {
    PDF14_OP_STATE_NONE = 0,
    PDF14_OP_STATE_FILL = 1,
    PDF14_OP_STATE_STROKE = 2,
} PDF14_OP_FS_STATE;

/*
 * This structure contains procedures for processing routine which differ
 * between the different blending color spaces.
 */
typedef struct {
    /*
     * Unpack a device color.  This routine is similar to the device's
     * decode_color procedure except for two things.  The procedure produces
     * 1 byte values instead of gx_color_values (2 bytes) and the output
     * values are inverted for subtractive color spaces (like CMYK).
     * A separate procedure is used instead of the decode_color to minimize
     * execution time.
     */
    void (* unpack_color)(int num_comp, gx_color_index color,
                                pdf14_device * p14dev, byte * out);
    /*
     * This procedure sends the final rasterized transparency data to the
     * output device as an image.
     */
    int (* put_image)(gx_device * dev,
                      gs_gstate * pgs, gx_device * target);

    /* And a 16 bit variant */
    void (* unpack_color16)(int num_comp, gx_color_index color,
                            pdf14_device * p14dev, uint16_t * out);
} pdf14_procs_s;

typedef pdf14_procs_s pdf14_procs_t;

/* A stack structure for the softmask buffers.
   The mask will be pdf14 buffers that are wrapped
   in a refernce counted structure.  We need this to
   be referenced counted since we need to be able to push
   multiple copies of the same buffer on the
   stack as we get multiple q operations when
   a soft mask is present in the graphic state. */

typedef struct pdf14_rcmask_s pdf14_rcmask_t;

struct pdf14_rcmask_s {

    pdf14_buf   *mask_buf;
    rc_header rc;
    gs_memory_t *memory;

};

typedef struct pdf14_mask_s pdf14_mask_t;

struct pdf14_mask_s {

    pdf14_rcmask_t *rc_mask;
    pdf14_mask_t *previous;
    gs_memory_t *memory;

};

/* A structure to hold information
 * about the parent color related
 * procs and other information.
 * These may change depending upon
 * if the blending space is different
 * than the base space.  The structure
 * is a list that is updated upon
 * every transparency group push and pop */

typedef struct pdf14_parent_color_s pdf14_parent_color_t;

struct pdf14_parent_color_s {
    int num_components;
    bool isadditive;
    gx_color_polarity_t polarity;
    byte comp_shift[GX_DEVICE_COLOR_MAX_COMPONENTS]; /* These are needed for the shading code */
    byte comp_bits[GX_DEVICE_COLOR_MAX_COMPONENTS];
    byte depth;  /* used in clist writer cmd_put_color */
    uint max_gray;  /* Used to determine if device halftones */
    uint max_color; /* Causes issues if these are not maintained */
    const gx_color_map_procs *(*get_cmap_procs)(const gs_gstate *,
                                                     const gx_device *);
    const gx_cm_color_map_procs *(*parent_color_mapping_procs)(const gx_device *);
    gx_color_index (*encode)(gx_device *, const gx_color_value value[]);
    int (*decode)(gx_device *, gx_color_index, gx_color_value *);
    int (*parent_color_comp_index)(gx_device *, const char *, int, int);
    const pdf14_procs_t * unpack_procs;
    const pdf14_nonseparable_blending_procs_t * parent_blending_procs;
    cmm_profile_t *icc_profile;  /* Opaque to GC.  Allocated in non-gc memory */
    pdf14_parent_color_t *previous;
};

typedef struct pdf14_ctx_s pdf14_ctx;

struct pdf14_buf_s {
    pdf14_buf *saved;
    byte *backdrop;  /* This is needed for proper non-isolated knockout support */
    bool isolated;
    bool knockout;
    uint16_t alpha;
    uint16_t shape;
    gs_blend_mode_t blend_mode;
    int num_spots;  /* helpful when going between Gray+spots, RGB+spots, CMYK+spots */
    bool has_alpha_g;
    bool has_shape;
    bool has_tags;
    bool deep; /* false => 8 bits, true => 16 bits */
    bool page_group;

    gs_int_rect rect;
    /* Note: the traditional GS name for rowstride is "raster" */

    /* Data is stored in planar format. Order of planes is: pixel values,
       alpha, shape if present, alpha_g if present. */

    int rowstride;
    int planestride;
    int n_chan;   /* number of pixel planes including alpha */
    int n_planes; /* total number of planes including alpha, shape, alpha_g */
    byte *data;
    byte *transfer_fn;
    bool is_ident;
    int matte_num_comps;
    uint16_t *matte;
    gs_int_rect dirty;
    pdf14_mask_t *mask_stack;
    bool idle;

    gs_transparency_mask_subtype_t SMask_SubType;

    uint mask_id;
    pdf14_parent_color_t *parent_color_info;

    gs_transparency_color_t color_space;  /* Different groups can have different spaces for blending */
    gs_memory_t *memory;
};

typedef struct pdf14_smaskcolor_s {
    gsicc_smask_t *profiles;
    int           ref_count;
} pdf14_smaskcolor_t;

struct pdf14_ctx_s {
    pdf14_buf *stack;
    pdf14_mask_t *mask_stack;
    gs_memory_t *memory;
    gs_int_rect rect;
    bool additive;
    int n_chan;
    int smask_depth;  /* used to catch smasks embedded in smasks.  bug691803 */
    bool smask_blend;
    bool deep; /* If true, 16 bit data, false, 8 bit data. */
};

typedef struct gs_pdf14trans_params_s gs_pdf14trans_params_t;

/*
 * Define the default post-clist (clist reader) PDF 1.4 compositing device.
 * We actually use the same structure for both the clist writer and reader
 * devices.  However we use separate names to identify the routines for each
 * device.
 */
typedef struct pdf14_device_s {
    gx_device_forward_common;
    gs_devn_params devn_params;    /* Must follow gx_device_forward_common */
    const pdf14_procs_t * pdf14_procs;	   /* Must follow devn_params. */
    const pdf14_nonseparable_blending_procs_t * blend_procs; /* Must follow pdf14_procs */
    int num_std_colorants;

    pdf14_ctx *ctx;
    pdf14_smaskcolor_t *smaskcolor;
    float opacity;
    float shape;
    float alpha; /* alpha = opacity * shape */
    float fillconstantalpha;
    float strokeconstantalpha;
    gs_blend_mode_t blend_mode;
    bool text_knockout;
    bool overprint;
    bool effective_overprint_mode;
    bool stroke_effective_op_mode;
    bool stroke_overprint;
    int text_group;
    gx_color_index drawn_comps;		/* Used for overprinting.  Passed from overprint compositor */
    gx_color_index drawn_comps_fill;		/* selected by color_is_fill */
    gx_color_index drawn_comps_stroke;
    PDF14_OP_FS_STATE op_state;
    gx_device * pclist_device;
    bool free_devicen;              /* Used to avoid freeing a deviceN parameter from target clist device */
    bool sep_device;
    bool using_blend_cs;

    /* We now have some variables to help us determine whether
     * we are in an SMask or not. Firstly, we have in_smask_construction,
     * initially 0, incremented whenever we begin smask in the compositor,
     * and decremented whenever we finish one. Thus this being non-zero
     * implies we are in the 'construction' phase of at least one smask.
     */
    int in_smask_construction;
    /* Next, we have smask_constructed. Again, initially 0. This is set
     * to 1 whenever in_smask_construction returns to 0. It is reset to
     * 0 on the next transparency group push (which takes possession of
     * the smask that was just created. Thus this being 1 implies
     * we have just constructed an smask, but not used it yet. */
    int smask_constructed;
    /* Finally, then we have depth_within_smask. This is used to keep the
     * count of how many times we need to pop a transparency group to
     * finish being governed by an smask. To keep this figure, we watch
     * pushes of new transparency groups. On each such push, if
     * smask_constructed is true, or depth_within_smask is already non-zero,
     * we increment depth_within_smask. (i.e. whenever we start a group
     * that will be governed by (and take possession of) an smask, or whenever
     * we are already being governed by an smask, we know that we've just
     * increased our nested depth by 1). Whenever we pop a group and it is
     * non zero, we decrement it.
     */
    int depth_within_smask;
    const gx_color_map_procs *(*save_get_cmap_procs)(const gs_gstate *,
                                                     const gx_device *);
    gx_device_color_info saved_target_color_info;
    dev_proc_encode_color(*saved_target_encode_color);
    dev_proc_decode_color(*saved_target_decode_color);
    dev_proc_get_color_mapping_procs(*saved_target_get_color_mapping_procs);
    dev_proc_get_color_comp_index(*saved_target_get_color_comp_index);
    dev_proc_encode_color(*my_encode_color);
    dev_proc_decode_color(*my_decode_color);
    dev_proc_get_color_mapping_procs(*my_get_color_mapping_procs);
    dev_proc_get_color_comp_index(*my_get_color_comp_index);

    pdf14_parent_color_t *trans_group_parent_cmap_procs;

} pdf14_device_t;

/*
 * Define the default pre-clist (clist writer) PDF 1.4 compositing device.
 * We actually use the same structure for both the clist writer and reader
 * devices.  However we use separate names to identify the routines for each
 * device.
 */
typedef	struct pdf14_device_s pdf14_clist_device;

/*
 * Send a PDF 1.4 transparency compositor action to the specified device.
 */
int send_pdf14trans(gs_gstate * pgs, gx_device * dev,
    gx_device * * pcdev, gs_pdf14trans_params_t * pparams, gs_memory_t * mem);

/*
 * This procedure will save information from the PDF 1.4 clist writing
 * clist compositior device.  This is information about the compressed
 * color list info which is needed for the support of spot colors in
 * PDF 1.4 compositing.  This info needs to be passed to the PDF 1.4
 * clist reading compositor.  However this device is not created until
 * the clist is read.  To get this info to that device, we have to
 * temporarily store that info in the output device.  This routine saves
 * that info in the output device.
 */
int
pdf14_put_devn_params(gx_device * pdev, gs_devn_params * pdevn_params,
                                        gs_param_list * plist);

/* Used to passed along information about the buffer created by the
   pdf14 device.  This is used by the pattern accumulator when the
   pattern contains transparency */
int pdf14_get_buffer_information(const gx_device * dev,
                                 gx_pattern_trans_t *transbuff, gs_memory_t *mem,
                                 bool free_device);

/* Not static due to call from pattern logic */
int pdf14_disable_device(gx_device * dev);

/* Needed so that we can set the monitoring in the target device */
int gs_pdf14_device_color_mon_set(gx_device *pdev, bool monitoring);

/* When playing back the clist, we need to know if the buffer device is compatible */
/* with the pdf14 compositor that was used when writing the clist. Colorspace and  */
/* depth are critical since these must match when reading back colors.             */
bool pdf14_ok_to_optimize(gx_device *bdev);

#endif /* gdevp14_INCLUDED */
