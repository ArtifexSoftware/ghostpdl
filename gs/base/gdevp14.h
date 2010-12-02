/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/
/* $Id$ */
/* Definitions and interface for PDF 1.4 rendering device */

#ifndef gdevp14_INCLUDED
#  define gdevp14_INCLUDED

#include "gxcmap.h"
#include "gsmatrix.h"
#include "gxcolor2.h"
#include "gxdcolor.h"
#include "gxpcolor.h"
#include "gdevdevn.h"

typedef enum {
    PDF14_DeviceGray = 0,
    PDF14_DeviceRGB = 1,
    PDF14_DeviceCMYK = 2,
    PDF14_DeviceCMYKspot = 3,
    PDF14_DeviceCustom = 4
} pdf14_default_colorspace_t;

#ifndef pdf14_buf_DEFINED
#  define pdf14_buf_DEFINED
typedef struct pdf14_buf_s pdf14_buf;
#endif

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
		    gs_imager_state * pis, gx_device * target);
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
    const gx_color_map_procs *(*get_cmap_procs)(const gs_imager_state *,
						     const gx_device *);
    const gx_cm_color_map_procs *(*parent_color_mapping_procs)(const gx_device *);
    int (*parent_color_comp_index)(gx_device *, const char *, int, int); 
    const pdf14_procs_t * unpack_procs;
    const pdf14_nonseparable_blending_procs_t * parent_blending_procs;
    cmm_profile_t *icc_profile;  /* Opaque to GC.  Allocated in non-gc memory */
    pdf14_parent_color_t *previous;
 
};

typedef struct pdf14_ctx_s pdf14_ctx;

struct pdf14_buf_s {
    pdf14_buf *saved;

    bool isolated;
    bool knockout;
    byte alpha;
    byte shape;
    gs_blend_mode_t blend_mode;

    bool has_alpha_g;
    bool has_shape;
    bool has_tags;

    gs_int_rect rect;
    /* Note: the traditional GS name for rowstride is "raster" */

    /* Data is stored in planar format. Order of planes is: pixel values,
       alpha, shape if present, alpha_g if present. */

    int rowstride;
    int planestride;
    int n_chan; /* number of pixel planes including alpha */
    int n_planes; /* total number of planes including alpha, shape, alpha_g */
    byte *data;
    byte *transfer_fn;
    gs_int_rect bbox;
    pdf14_mask_t *maskbuf;
    bool idle;

    bool SMask_is_CIE;
    gs_transparency_mask_subtype_t SMask_SubType;

    uint mask_id;
    pdf14_parent_color_t *parent_color_info_procs;

    gs_transparency_color_t color_space;  /* Different groups can have different spaces for blending */
};

typedef struct pdf14_smaskcolor_s {
    gsicc_smask_t *profiles;
    int           ref_count;
} pdf14_smaskcolor_t;

struct pdf14_ctx_s {
    pdf14_buf *stack;
    pdf14_mask_t *maskbuf;
    gs_memory_t *memory;
    gs_int_rect rect;
    bool additive;
    int n_chan;
    int smask_depth;  /* used to catch smasks embedded in smasks.  bug691803 */
    bool smask_blend; 
};

#ifndef gs_devn_params_DEFINED
#  define gs_devn_params_DEFINED
typedef struct gs_devn_params_s gs_devn_params;
#endif

#ifndef gs_imager_state_DEFINED
#  define gs_imager_state_DEFINED
typedef struct gs_imager_state_s gs_imager_state;
#endif

#ifndef gx_device_DEFINED
#  define gx_device_DEFINED
typedef struct gx_device_s gx_device;
#endif

#ifndef gs_pdf14trans_params_DEFINED
#  define gs_pdf14trans_params_DEFINED
typedef struct gs_pdf14trans_params_s gs_pdf14trans_params_t;
#endif

#ifndef pdf14_device_DEFINED
#  define pdf14_device_DEFINED
typedef struct pdf14_device_s pdf14_device;
#endif



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

    pdf14_ctx *ctx;
    pdf14_smaskcolor_t *smaskcolor;
    float opacity;
    float shape;
    float alpha; /* alpha = opacity * shape */
    gs_blend_mode_t blend_mode;
    bool text_knockout;
	bool overprint;
	bool overprint_mode;
	gx_color_index drawn_comps;		/* Used for overprinting.  Passed from overprint compositor */
    gx_device * pclist_device;
    const gx_color_map_procs *(*save_get_cmap_procs)(const gs_imager_state *,
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
int send_pdf14trans(gs_imager_state * pis, gx_device * dev,
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

int pdf14_get_buffer_information(const gx_device * dev, gx_pattern_trans_t *transbuff);

/* Not static due to call from pattern logic */
int pdf14_disable_device(gx_device * dev);

#endif /* gdevp14_INCLUDED */
