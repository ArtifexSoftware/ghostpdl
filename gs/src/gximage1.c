/* Copyright (C) 1989, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

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


/* ImageType 1 initialization */
#include "gx.h"
#include "gxiparam.h"
#include "gximage.h"

/* Define the image type for ImageType 1 images. */
private const gx_image_type_t image1_type = {
    gx_begin_image1, gx_data_image_source_size,
    0/*gx_write_image1*/, 0/*gx_read_image1*/, 0/*gx_release_image1*/, 1
};

/* Define the procedures for initializing gs_image_ts to default values. */
void
gs_image_t_init(gs_image_t * pim, const gs_color_space * color_space)
{
    gs_pixel_image_t_init((gs_pixel_image_t *) pim, color_space);
    pim->type = &image1_type;
    pim->ImageMask = pim->adjust = (color_space == NULL);
    pim->Alpha = gs_image_alpha_none;
}
void
gs_image_t_init_mask(gs_image_t * pim, bool write_1s)
{
    gs_image_t_init(pim, NULL);
    if (write_1s)
	pim->Decode[0] = 1, pim->Decode[1] = 0;
    else
	pim->Decode[0] = 0, pim->Decode[1] = 1;
}

/* Start processing an ImageType 1 image. */
int
gx_begin_image1(gx_device * dev,
		const gs_imager_state * pis, const gs_matrix * pmat,
		const gs_image_common_t * pic, const gs_int_rect * prect,
		const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
		gs_memory_t * mem, gx_image_enum_common_t ** pinfo)
{
    gx_image_enum *penum;
    const gs_image_t *pim = (const gs_image_t *)pic;
    int code = gx_image_enum_alloc(dev, pis, pmat, pic, prect, pdcolor,
				   pcpath, mem, &penum);

    if (code < 0)
	return code;
    penum->alpha = pim->Alpha;
    penum->use_mask_color = false;
    penum->masked = pim->ImageMask;
    penum->adjust =
	(pim->ImageMask && pim->adjust ? float2fixed(0.5) : fixed_0);
    code = gx_image_enum_begin(dev, pis, pic, prect, pdcolor, pcpath, mem,
			       penum);
    if (code >= 0)
	*pinfo = (gx_image_enum_common_t *)penum;
    return code;
}
