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

/* gx51x.c */
/* 5.1x patches for 5c project */
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gscspace.h"		/* for ...num_components */
#include "gsutil.h"		/* for gs_next_ids */
#include "gxdevice.h"
#include "gxiparam.h"
#include "gzstate.h"
#include "gxcmap.h"

/* Structure descriptors */
public_st_gs_image_common();
public_st_gs_data_image();
public_st_gs_pixel_image();

/* ---------------- Statically allocated color spaces ---------------- */

gs_color_space *cs_DeviceGray;
gs_color_space *cs_DeviceRGB;
gs_color_space *cs_DeviceCMYK;

void
gs_51x_init(gs_memory_t * mem)
{
    gs_cspace_build_DeviceGray(&cs_DeviceGray, mem);
    gs_cspace_build_DeviceRGB(&cs_DeviceRGB, mem);
    gs_cspace_build_DeviceCMYK(&cs_DeviceCMYK, mem);
}

/* ---------------- Miscellaneous patches ---------------- */

void
gs_gscolor_init(gs_memory_t * mem)
{
}

void
gs_gscolor1_init(gs_memory_t * mem)
{
}

int
gs_setalpha(gs_state * pgs, floatp alpha)
{
    return 0;
}

void
gx_device_init(gx_device * dev, const gx_device * proto, gs_memory_t * mem,
	       bool internal)
{
    memcpy(dev, proto, proto->params_size);
    dev->memory = mem;
}

int
gs_setdevice_no_init(gs_state * pgs, gx_device * dev)
{
    /*
     * Just set the device, possibly changing color space but no other
     * device parameters.
     */
    pgs->device = dev;
    gx_set_cmap_procs((gs_imager_state *) pgs, dev);
    gx_unset_dev_color(pgs);
    return 0;
}

/* ---------------- Generic image support ---------------- */

/* GC structures for image enumerator */
public_st_gx_image_enum_common();

#define eptr ((gx_image_enum_common_t *)vptr)

private 
ENUM_PTRS_BEGIN(image_enum_common_enum_ptrs) return 0;

case 0:
ENUM_RETURN(gx_device_enum_ptr(eptr->dev));
ENUM_PTRS_END

private RELOC_PTRS_BEGIN(image_enum_common_reloc_ptrs)
{
    eptr->dev = gx_device_reloc_ptr(eptr->dev, gcst);
}
RELOC_PTRS_END

#undef eptr

/* Initialize the common parts of image structures. */
void
gs_image_common_t_init(gs_image_common_t * pic)
{
    gs_make_identity(&pic->ImageMatrix);
}
void
gs_data_image_t_init(gs_data_image_t * pim, int num_components)
{
    int i;

    gs_image_common_t_init((gs_image_common_t *) pim);
    pim->Width = pim->Height = 0;
    pim->BitsPerComponent = 1;
    if (num_components >= 0) {
	for (i = 0; i < num_components * 2; i += 2)
	    pim->Decode[i] = 0, pim->Decode[i + 1] = 1;
    } else {
	for (i = 0; i < num_components * -2; i += 2)
	    pim->Decode[i] = 1, pim->Decode[i + 1] = 0;
    }
    pim->Interpolate = false;
}
void
gs_pixel_image_t_init(gs_pixel_image_t * pim, const gs_color_space * color_space)
{
    int num_components;

    if (color_space == 0 ||
	(num_components =
	 gs_color_space_num_components(color_space)) < 0
	)
	num_components = 0;
    gs_data_image_t_init((gs_data_image_t *) pim, num_components);
    pim->format = gs_image_format_chunky;
    pim->ColorSpace = color_space;
    pim->CombineWithColor = false;
}

/* Initialize the common part of an image-processing enumerator. */
int
gx_image_enum_common_init(gx_image_enum_common_t * piec,
	  const gs_image_common_t * pic, const gx_image_enum_procs_t * piep,
		gx_device * dev, int bits_per_component, int num_components,
			  gs_image_format_t format)
{
    piec->image_type = pic->type;
    piec->procs = piep;
    piec->dev = dev;
    piec->id = gs_next_ids(1);
    switch (format) {
	case gs_image_format_chunky:
	    piec->num_planes = 1;
	    piec->plane_depths[0] = bits_per_component * num_components;
	    break;
	case gs_image_format_component_planar:
	    piec->num_planes = num_components;
	    {
		int i;

		for (i = 0; i < num_components; ++i)
		    piec->plane_depths[i] = bits_per_component;
	    }
	    break;
	case gs_image_format_bit_planar:
	    piec->num_planes = bits_per_component * num_components;
	    {
		int i;

		for (i = 0; i < piec->num_planes; ++i)
		    piec->plane_depths[i] = 1;
	    }
#if 0				/* **************** */
	    break;
#endif /* **************** */
	default:
	    return_error(gs_error_rangecheck);
    }
    return 0;
}

/* ---------------- Image processing ---------------- */

int
gx_device_begin_typed_image(gx_device * dev,
			const gs_imager_state * pis, const gs_matrix * pmat,
		   const gs_image_common_t * pic, const gs_int_rect * prect,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
		      gs_memory_t * memory, gx_image_enum_common_t ** pinfo)
{				/*
				 * If this is an ImageType 1 image using the imager's CTM,
				 * defer to begin_image.
				 */
    if (pic->type->begin_typed_image == gx_begin_image1) {
	const gs_image_t *pim = (const gs_image_t *)pic;

	if (pmat == 0 || !memcmp(pmat, &ctm_only(pis), sizeof(*pmat))) {
	    int code = (*dev_proc(dev, begin_image))
	    (dev, pis, pim, pim->format, prect, pdcolor,
	     pcpath, memory, pinfo);

	    if (code >= 0)
		return code;
	}
    }
    return (*pic->type->begin_typed_image)
	(dev, pis, pmat, pic, prect, pdcolor, pcpath, memory, pinfo);
}

int
gx_device_image_data(gx_device * dev,
	gx_image_enum_common_t * info, const byte ** plane_data, int data_x,
		     uint raster, int height)
{
    int num_planes = info->num_planes;
    gx_image_plane_t planes[gs_image_max_components];
    int i;

#ifdef DEBUG
    if (num_planes > gs_image_max_components) {
	lprintf2("num_planes=%d > gs_image_max_components=%d!\n",
		 num_planes, gs_image_max_components);
	return_error(gs_error_Fatal);
    }
#endif
    for (i = 0; i < num_planes; ++i) {
	planes[i].data = plane_data[i];
	planes[i].data_x = data_x;
	planes[i].raster = raster;
    }
    return gx_device_image_plane_data(dev, info, planes, height);
}

int
gx_device_image_plane_data(gx_device * dev,
 gx_image_enum_common_t * info, const gx_image_plane_t * planes, int height)
{
    /* We must use the device in the enumerator, not the argument. */
    return info->procs->plane_data(info->dev, info, planes, height);
}

int
gx_device_end_image(gx_device * dev,
		    gx_image_enum_common_t * info, bool draw_last)
{
    /* We must use the device in the enumerator, not the argument. */
    return info->procs->end_image(info->dev, info, draw_last);
}

/* Process the next piece of an image with no source data. */
/* This procedure should never be called. */
int
gx_no_image_plane_data(gx_device * dev,
 gx_image_enum_common_t * info, const gx_image_plane_t * planes, int height)
{
    return_error(gs_error_Fatal);
}

/* Clean up after processing an image with no source data. */
/* This procedure may be called, but should do nothing. */
int
gx_ignore_end_image(gx_device * dev, gx_image_enum_common_t * info,
		    bool draw_last)
{
    return 0;
}

/* ---------------- ImageType 1 images ---------------- */

/* Define the image type for ImageType 1 images. */
private const gx_image_type_t image1_type =
{image1_type_data};

/* Define the procedures for initializing gs_image_ts to default values. */
private void
image_t_init(gs_image_t * pim, const gs_color_space * color_space)
{
    gs_pixel_image_t_init((gs_pixel_image_t *) pim, color_space);
    pim->type = &image1_type;
    pim->ImageMask = pim->adjust = (color_space == NULL);
    pim->Alpha = gs_image_alpha_none;
}
void
gs_image_t_init_mask(gs_image_t * pim, bool write_1s)
{
    image_t_init(pim, NULL);
    if (write_1s)
	pim->Decode[0] = 1, pim->Decode[1] = 0;
    else
	pim->Decode[0] = 0, pim->Decode[1] = 1;
}
void
gs_image_t_init_gray(gs_image_t * pim)
{
    image_t_init(pim, gs_color_space_DeviceGray());
}
void
gs_image_t_init_color(gs_image_t * pim)
{
    image_t_init(pim, gs_color_space_DeviceRGB());
    pim->Decode[6] = pim->Decode[8] = 0;
    pim->Decode[7] = pim->Decode[9] = 1;
}

/* Compute the source size of an ordinary image with explicit data. */
int
gx_data_image_source_size(const gs_imager_state * pis,
			const gs_image_common_t * pim, gs_int_point * psize)
{
    const gs_data_image_t *pdi = (const gs_data_image_t *)pim;

    psize->x = pdi->Width;
    psize->y = pdi->Height;
    return 0;
}

/* Start an ImageType 1 image.  This is the driver procedure. */
int
gx_device_begin_image(gx_device * dev,
		      const gs_imager_state * pis, const gs_image_t * pim,
		      gs_image_format_t format, const gs_int_rect * prect,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
		      gs_memory_t * memory, void **pinfo)
{				/* Just call the device's begin_image procedure. */
    return (*dev_proc(dev, begin_image))
	(dev, pis, pim, pim->format, prect, pdcolor,
	 pcpath, memory, pinfo);
}

/*
 * gx_default_begin_image is only invoked for ImageType 1 images.  However,
 * the argument types are different, and if the device provides a
 * begin_typed_image procedure, we should use it.  See gxdevice.h.
 */
private int
gx_no_begin_image(gx_device * dev,
		  const gs_imager_state * pis, const gs_image_t * pim,
		  gs_image_format_t format, const gs_int_rect * prect,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
		  gs_memory_t * memory, gx_image_enum_common_t ** pinfo)
{
    return -1;
}
int
gx_default_begin_image(gx_device * dev,
		       const gs_imager_state * pis, const gs_image_t * pim,
		       gs_image_format_t format, const gs_int_rect * prect,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
	     gs_memory_t * memory, void /*gx_image_enum_common_t */ **pinfo)
{
    /*
     * Hand off to begin_typed_image, being careful to avoid a
     * possible recursion loop.
     */
    dev_proc_begin_image((*save_begin_image)) = dev_proc(dev, begin_image);
    dev_proc_begin_typed_image((*begin_typed_image)) =
	dev_proc(dev, begin_typed_image);

    gs_image_t image;
    const gs_image_t *ptim;
    int code;

    set_dev_proc(dev, begin_image, gx_no_begin_image);
    if (pim->format == format)
	ptim = pim;
    else {
	image = *pim;
	image.format = format;
	ptim = &image;
    }
    if (begin_typed_image == 0)
	begin_typed_image = gx_default_begin_typed_image;
    code = begin_typed_image(dev, pis, NULL,
			     (const gs_image_common_t *)ptim, prect,
			     pdcolor, pcpath, memory, pinfo);
    set_dev_proc(dev, begin_image, save_begin_image);
    return code;
}

/* Process the next piece of an ImageType 1 image. */
int
gx_image1_plane_data(gx_device * dev,
 gx_image_enum_common_t * info, const gx_image_plane_t * planes, int height)
{
    int data_x = planes[0].data_x;
    int raster = planes[0].raster;
    const byte *data[5];
    int i;

    for (i = 0; i < info->num_planes; ++i) {
	if (planes[i].data_x != data_x ||
	    (planes[i].raster != raster && height > 1)
	    )
	    return_error(gs_error_rangecheck);
	data[i] = planes[i].data;
    }
    return gx_default_image_data(dev, info, data, data_x, raster, height);
}

/* Clean up by releasing the buffers. */
/* Currently we ignore draw_last. */
int
gx_image1_end_image(gx_device * dev, gx_image_enum_common_t * info,
		    bool draw_last)
{
    return gx_default_end_image(dev, info, draw_last);
}
