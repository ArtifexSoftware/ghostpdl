/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */


/* ImageType 4 image implementation */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gscspace.h"
#include "gsiparm3.h"
#include "gsiparm4.h"
#include "gxiparam.h"
#include "gximage.h"

/* Forward references */
private dev_proc_begin_typed_image(gx_begin_image4);

/* Define the image type for ImageType 4 images. */
private const gx_image_type_t image4_type = {
    gx_begin_image4, gx_data_image_source_size,
    0/*gx_write_image4*/, 0/*gx_read_image4*/, 0/*gx_release_image4*/, 4
};
/* The implementation is shared with ImageType 1. */
private const gx_image_enum_procs_t image4_enum_procs = {
    gx_image1_plane_data, gx_image1_end_image
};

/* Initialize an ImageType 4 image. */
void
gs_image4_t_init(gs_image4_t * pim, const gs_color_space * color_space)
{
    gs_pixel_image_t_init((gs_pixel_image_t *) pim, color_space);
    pim->type = &image4_type;
    pim->MaskColor_is_range = false;
}

/* Start processing an ImageType 4 image. */
int
gx_begin_image4(gx_device * dev,
		const gs_imager_state * pis, const gs_matrix * pmat,
		const gs_image_common_t * pic, const gs_int_rect * prect,
		const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
		gs_memory_t * mem, gx_image_enum_common_t ** pinfo)
{
    gx_image_enum *penum;
    const gs_image4_t *pim = (const gs_image4_t *)pic;
    int code = gx_image_enum_alloc(dev, pis, pmat, pic, prect, pdcolor,
				   pcpath, mem, &penum);

    if (code < 0)
	return code;
    penum->alpha = gs_image_alpha_none;
    penum->masked = false;
    penum->adjust = fixed_0;
    /* Determine whether this image needs masking at all. */
    {
	bool opaque = false;
	uint max_value = (1 << pim->BitsPerComponent) - 1;
	int spp = cs_num_components(pim->ColorSpace);
	int i;

	for (i = 0; i < spp * 2; i += 2) {
	    uint c0, c1;

	    if (pim->MaskColor_is_range)
		c0 = pim->MaskColor[i], c1 = pim->MaskColor[i + 1];
	    else
		c0 = c1 = pim->MaskColor[i >> 1];

	    if (c1 > max_value)
		c1 = max_value;
	    if (c0 > c1) {
		opaque = true;	/* pixel can never match mask color */
		break;
	    }
	    penum->mask_color.values[i] = c0;
	    penum->mask_color.values[i + 1] = c1;
	}
	penum->use_mask_color = !opaque;
    }
    code = gx_image_enum_begin(dev, pis, pic, prect, pdcolor, pcpath, mem,
			       penum);
    if (code >= 0)
	*pinfo = (gx_image_enum_common_t *)penum;
    return code;
}
