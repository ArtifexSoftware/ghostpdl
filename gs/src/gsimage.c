/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* gsimage.c */
/* Image setup procedures for Ghostscript library */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gscspace.h"
#include "gsmatrix.h"		/* for gsiparam.h */
#include "gsimage.h"
#include "gxarith.h"		/* for igcd */
#include "gxdevice.h"
#include "gzstate.h"

/* Define the maximum number of image components. */
#ifdef DPNEXT
#  define max_components 5
#else
#  define max_components 4
#endif

/* Define the enumeration state for this interface layer. */
/*typedef struct gs_image_enum_s gs_image_enum;*/	/* in gsimage.h */
struct gs_image_enum_s {
		/* The following are set at initialization time. */
	gs_memory_t *memory;
	gx_device *dev;
	bool skip;		/* if true, just skip over the data */
	void *info;		/* driver bookkeeping structure */
	int num_components;
	bool multi;
	int num_planes;
	int width, height;
	int bpp;		/* bits per pixel (per plane, if multi) */
	uint raster;		/* bytes per row (per plane), no padding */
		/* The following are updated dynamically. */
	int plane_index;	/* index of next plane of data */
	int y;
	uint pos;		/* byte position within the scan line */
	gs_const_string sources[max_components];	/* source data */
	gs_string rows[max_components];		/* row buffers */
	bool error;
};
gs_private_st_composite(st_gs_image_enum, gs_image_enum, "gs_image_enum",
  gs_image_enum_enum_ptrs, gs_image_enum_reloc_ptrs);
#define gs_image_enum_num_ptrs 2

/* GC procedures */
#define eptr ((gs_image_enum *)vptr)
private ENUM_PTRS_BEGIN(gs_image_enum_enum_ptrs) {
	/* Enumerate the data planes. */
	index -= gs_image_enum_num_ptrs;
	if ( index < eptr->plane_index )
	  { *pep = (void *)&eptr->sources[index];
	    return ptr_string_type;
	  }
	index -= eptr->plane_index;
	if ( index < eptr->num_planes )
	  { *pep = (void *)&eptr->rows[index];
	    return ptr_string_type;
	  }
	return 0;
	}
	ENUM_PTR(0, gs_image_enum, dev);
	ENUM_PTR(1, gs_image_enum, info);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(gs_image_enum_reloc_ptrs) {
	int i;
	RELOC_PTR(gs_image_enum, dev);
	RELOC_PTR(gs_image_enum, info);
	for ( i = 0; i < eptr->plane_index; i++ )
	  RELOC_CONST_STRING_PTR(gs_image_enum, sources[i]);
	for ( i = 0; i < eptr->num_planes; i++ )
	  RELOC_STRING_PTR(gs_image_enum, rows[i]);
} RELOC_PTRS_END
#undef eptr

/* Allocate an image enumerator. */
private void
image_enum_init(gs_image_enum *pie)
{	/* Clean pointers for GC. */
	int i;
	pie->info = 0;
	pie->dev = 0;
	for ( i = 0; i < countof(pie->sources); ++i )
	  { pie->sources[i].data = 0, pie->sources[i].size = 0;
	    pie->rows[i].data = 0, pie->rows[i].size = 0;
	  }
}
gs_image_enum *
gs_image_enum_alloc(gs_memory_t *mem, client_name_t cname)
{	gs_image_enum *pie =
	  gs_alloc_struct(mem, gs_image_enum, &st_gs_image_enum, cname);

	if ( pie != 0 )
	  { pie->memory = mem;
	    image_enum_init(pie);
	  }
	return pie;
}

/* Start processing an image. */
int
gs_image_init(gs_image_enum *pie, const gs_image_t *pim, bool multi,
  gs_state *pgs)
{	gx_device *dev = gs_currentdevice_inline(pgs);
	gs_image_t image;
	ulong samples_per_row = pim->Width;
	int code;

	if ( pim->Width == 0 || pim->Height == 0 )
	  return 1;
	image = *pim;
	image_enum_init(pie);
	pie->skip = pgs->in_charpath;
	if ( image.ImageMask )
	  { image.ColorSpace = NULL;
	    if ( pgs->in_cachedevice <= 1 )
	      image.adjust = false;
	    pie->num_components = pie->num_planes = 1;
	  }
	else
	  { if ( pgs->in_cachedevice )
	      return_error(gs_error_undefined);
	    if ( image.ColorSpace == NULL )
	      image.ColorSpace = gs_color_space_DeviceGray();
	    pie->num_components =
	      gs_color_space_num_components(image.ColorSpace);
#ifdef DPNEXT
	    if ( pim->HasAlpha )
	      pie->num_components++;
#endif
	    if ( multi )
	      pie->num_planes = pie->num_components;
	    else
	      { pie->num_planes = 1;
		samples_per_row *= pie->num_components;
	      }
	  }
	if ( image.ImageMask | image.CombineWithColor )
	  gx_set_dev_color(pgs);
	if ( !pie->skip )
	  { code = (*dev_proc(dev, begin_image))
	      (dev, (const gs_imager_state *)pgs, &image,
	       (multi ? gs_image_format_component_planar : gs_image_format_chunky),
	       NULL, pgs->dev_color, pgs->clip_path, pie->memory, &pie->info);
	    if ( code < 0 )
	      return code;
	  }
	pie->dev = dev;
	pie->multi = multi;
	pie->bpp =
	  image.BitsPerComponent * pie->num_components / pie->num_planes;
	pie->width = image.Width;
	pie->height = image.Height;
	pie->raster = (samples_per_row * image.BitsPerComponent + 7) >> 3;
	/* Initialize the dynamic part of the state. */
	pie->plane_index = 0;
	pie->y = 0;
	pie->pos = 0;
	pie->error = false;
	return 0;
}

/*
 * Return the number of bytes of data per row
 * (per plane, if MultipleDataSources is true).
 */
uint
gs_image_bytes_per_row(const gs_image_enum *pie)
{	return pie->raster;
}

/* Process the next piece of an image. */
private int
copy_planes(gx_device *dev, gs_image_enum *pie, const byte **planes, int h)
{	int code =
	  (pie->skip ? (pie->y + h < pie->height ? 0 : 1) : 
	   (*dev_proc(dev, image_data))(dev, pie->info, planes, 0,
					pie->raster, h));

	if ( code < 0 )
	  pie->error = true;
	return code;
}
int
gs_image_next(gs_image_enum *pie, const byte *dbytes, uint dsize,
  uint *pused)
{	gx_device *dev;
	uint left;
	int num_planes;
	uint raster;
	uint pos;
	int code;

	/*
	 * Handle the following differences between gs_image_next and
	 * the device image_data procedure:
	 *
	 *	- image_data requires an array of planes; gs_image_next
	 *	expects planes in successive calls.
	 *
	 *	- image_data requires that each call pass entire rows;
	 *	gs_image_next allows arbitrary amounts of data.
	 */
	if ( pie->plane_index != 0 )
	  if ( dsize != pie->sources[0].size )
	    return_error(gs_error_rangecheck);
	pie->sources[pie->plane_index].data = dbytes;
	pie->sources[pie->plane_index].size = dsize;
	if ( ++(pie->plane_index) != pie->num_planes )
	  return 0;
	/* We have a full set of planes. */
	dev = pie->dev;
	left = dsize;
	num_planes = pie->num_planes;
	raster = pie->raster;
	pos = pie->pos;
	code = 0;
	while ( left && pie->y < pie->height )
	  { const byte *planes[max_components];
	    int i;

	    for ( i = 0; i < num_planes; ++i )
	      planes[i] = pie->sources[i].data + dsize - left;
	    if ( pos == 0 && left >= raster )
	      { /* Pass (a) row(s) directly from the source. */
		int h = left / raster;
		if ( h > pie->height - pie->y )
		  h = pie->height - pie->y;
		code = copy_planes(dev, pie, planes, h);
		if ( code < 0 )
		  break;
		left -= raster * h;
		pie->y += h;
	      }
	    else
	      { /* Buffer a partial row. */
		uint count = min(left, raster - pos);
		if ( pie->rows[0].data == 0 )
		  { /* Allocate the row buffers. */
		    for ( i = 0; i < num_planes; ++i )
		      { byte *row = gs_alloc_string(pie->memory, raster,
						    "gs_image_next(row)");
		        if ( row == 0 )
			  { code = gs_note_error(gs_error_VMerror);
			    while ( --i >= 0 )
			      { gs_free_string(pie->memory, pie->rows[i].data,
					       raster, "gs_image_next(row)");
			        pie->rows[i].data = 0;
				pie->rows[i].size = 0;
			      }
			    break;
			  }
			pie->rows[i].data = row;
			pie->rows[i].size = raster;
		      }
		    if ( code < 0 )
		      break;
		  }
		for ( i = 0; i < num_planes; ++i )
		  memcpy(pie->rows[i].data + pos, planes[i], count);
		pos += count;
		left -= count;
		if ( pos == raster )
		  { for ( i = 0; i < num_planes; ++i )
		      planes[i] = pie->rows[i].data;
		    code = copy_planes(dev, pie, planes, 1);
		    if ( code < 0 )
		      break;
		    pos = 0;
		    pie->y++;
		  }
	      }
	  }
	pie->pos = pos;
	pie->plane_index = 0;
	*pused = dsize - left;
	return code;
}

/* Clean up after processing an image. */
void
gs_image_cleanup(gs_image_enum *pie)
{	gx_device *dev = pie->dev;
	int i;

	for ( i = 0; i < pie->num_planes; ++i )
	  gs_free_string(pie->memory, pie->rows[i].data,
			 pie->rows[i].size, "gs_image_cleanup(row)");
	if ( !pie->skip )
	  (*dev_proc(dev, end_image))(dev, pie->info, !pie->error);
	/* Don't free the local enumerator -- the client does that. */
}
