/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/* gxclimag.c */
/* Higher-level image operations for band lists */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gscspace.h"
#include "gxarith.h"
#include "gxdevice.h"
#include "gxdevmem.h"			/* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxpath.h"
#include "gxclpath.h"
#include "gxfmap.h"
#ifdef FUTURE
#include "strimpl.h"		/* for siscale.h */
#include "siscale.h"
#endif

/* Define whether we should use high-level images. */
/* (See below for additional restrictions.) */
static bool USE_HL_IMAGES = true;

#define cdev cwdev

/* Forward references */
private int cmd_put_color_mapping(P3(gx_device_clist_writer *cldev,
				     const gs_imager_state *pis,
				     bool write_rgb_to_cmyk));
private bool
check_rect_for_trivial_clip(P5(
	const gx_clip_path *pcpath,	/* May be NULL, clip to evaluate */
	int px, int py, int qx, int qy	/* corners of box to test */
));

/* Driver procedures */
private dev_proc_fill_mask(clist_fill_mask);
private dev_proc_begin_image(clist_begin_image);
private dev_proc_image_data(clist_image_data);
private dev_proc_end_image(clist_end_image);

/* Initialize the extensions to the command set. */
void
gs_climag_init(gs_memory_t *mem)
{
	gs_clist_device_procs.fill_mask = clist_fill_mask;
	gs_clist_device_procs.begin_image = clist_begin_image;
	gs_clist_device_procs.image_data = clist_image_data;
	gs_clist_device_procs.end_image = clist_end_image;
}

/* ------ Driver procedures ------ */

private int
clist_fill_mask(gx_device *dev,
  const byte *data, int data_x, int raster, gx_bitmap_id id,
  int x, int y, int width, int height,
  const gx_drawing_color *pdcolor, int depth,
  gs_logical_operation_t lop, const gx_clip_path *pcpath)
{	const byte *orig_data = data;	/* for writing tile*/
	int orig_data_x = data_x;	/* ditto */
	int orig_x = x;			/* ditto */
	int orig_width = width;		/* ditto */
	int orig_height = height;	/* ditto */
	int log2_depth = depth >> 1;	/* works for 1,2,4 */
	int y0;
	int data_x_bit;
	byte copy_op =
	  (depth > 1 ? cmd_op_copy_color_alpha :
	   gx_dc_is_pure(pdcolor) ? cmd_op_copy_mono :
	   cmd_op_copy_mono + cmd_copy_ht_color);

	fit_copy(dev, data, data_x, raster, id, x, y, width, height);
	y0 = y;			/* must do after fit_copy */

	/* If non-trivial clipping & complex clipping disabled, default */
	if ( (cdev->disable_mask & clist_disable_complex_clip)
	 && !check_rect_for_trivial_clip(pcpath, x, y, x + width, y + height) )
	  return gx_default_fill_mask(dev, data, data_x, raster, id,
	   x, y, width, height, pdcolor, depth, lop, pcpath);

	if ( cmd_check_clip_path(cdev, pcpath) )
	  cmd_clear_known(cdev, clip_path_known);
	data_x_bit = data_x << log2_depth;
	BEGIN_RECT
	int dx = (data_x_bit & 7) >> log2_depth;
	const byte *row = data + (y - y0) * raster + (data_x_bit >> 3);
	int code;

	TRY_RECT
	  code = 0;
	  if ( lop == lop_default )
	    code = code < 0 ? code : cmd_disable_lop(cdev, pcls);
	  else
	    { if ( lop != pcls->lop )
		code = code < 0 ? code : cmd_set_lop(cdev, pcls, lop);
	      code = code < 0 ? code : cmd_enable_lop(cdev, pcls);
	    }
	HANDLE_RECT(code);
	if ( depth > 1 && !pcls->color_is_alpha )
	  { byte *dp;
	    TRY_RECT
	      code = set_cmd_put_op(dp, cdev, pcls, cmd_opv_set_copy_alpha, 1);
	    HANDLE_RECT(code);
	    pcls->color_is_alpha = 1;
	  }
	TRY_RECT
	  code = cmd_do_write_unknown(cdev, pcls, clip_path_known);
	  code = code < 0 ? code : cmd_do_enable_clip(cdev, pcls, pcpath != NULL);
	HANDLE_RECT(code);
	TRY_RECT
	  code = cmd_put_drawing_color(cdev, pcls, pdcolor);
	HANDLE_RECT(code);
	/*
	 * Unfortunately, painting a character with a halftone requires the
	 * use of two bitmaps, a situation that we can neither represent in
	 * the band list nor guarantee will both be present in the tile
	 * cache; in this case, we always write the bits of the character.
	 *
	 * We could handle more RasterOp cases here directly, but it
	 * doesn't seem worth the trouble right now.
	 */
	if ( id != gx_no_bitmap_id && gx_dc_is_pure(pdcolor) &&
	     lop == lop_default
	   )
	  {	/* This is a character.  ****** WRONG IF HALFTONE CELL. ******/
		/* Put it in the cache if possible. */
		ulong offset_temp;
		if ( !cls_has_tile_id(cdev, pcls, id, offset_temp) )
		  { gx_strip_bitmap tile;
		    tile.data = (byte *)orig_data;	/* actually const */
		    tile.raster = raster;
		    tile.size.x = tile.rep_width = orig_width;
		    tile.size.y = tile.rep_height = orig_height;
		    tile.rep_shift = tile.shift = 0;
		    tile.id = id;
		    TRY_RECT
		     code = clist_change_bits(cdev, pcls, &tile, depth);
		    HANDLE_RECT_UNLESS(code,
		     (code != gs_error_VMerror || !cdev->error_is_retryable) );
		    if (code < 0)
		      { /* Something went wrong; just copy the bits. */
			goto copy;
		      }
		  }
		{ gx_cmd_rect rect;
		  int rsize;
		  byte op = copy_op + cmd_copy_use_tile;
		  byte *dp;

		  /* Output a command to copy the entire character. */
		  /* It will be truncated properly per band. */
		  rect.x = orig_x, rect.y = y0;
		  rect.width = orig_width, rect.height = yend - y0;
		  rsize = 1 + cmd_sizexy(rect);
		  TRY_RECT
		    code = 0;
		    if ( orig_data_x )
		      { int dx_msb = orig_data_x >> 5;
			code = set_cmd_put_op(dp, cdev, pcls, cmd_opv_set_misc,
				              2 + cmd_size_w(dx_msb));
		        if ( code >= 0 && dx_msb )
			  { dp[1] = cmd_set_misc_data_x + 0x20 +
			      (orig_data_x & 0x1f);
			    cmd_put_w(dx_msb, dp + 2);
			  }
		        else
			  dp[1] = cmd_set_misc_data_x + orig_data_x;
		      }
		    code = code < 0 ? code :
		     set_cmd_put_op(dp, cdev, pcls, op, rsize);
		  HANDLE_RECT(code)
		  dp++;
		  cmd_putxy(rect, dp);
		  pcls->rect = rect;
		  goto end;
		}
	  }
copy:	/*
	 * The default fill_mask implementation uses strip_copy_rop;
	 * this is exactly what we want.
	 */
	TRY_RECT
	  NEST_RECT;
	  code = gx_default_fill_mask(dev, row, dx, raster,
				      (y == y0 && height == orig_height &&
				       dx == orig_data_x ? id :
				       gx_no_bitmap_id),
				      x, y, width, height, pdcolor, depth,
				      lop, pcpath);
	  UNNEST_RECT;
	HANDLE_RECT(code);
end:	;
	END_RECT
	return 0;
}

/* ------ Bitmap image driver procedures ------ */

/* Define the structure for keeping track of progress through an image. */
typedef struct clist_image_enum_s {
		/* Arguments of begin_image */
	gs_memory_t *memory;
	gs_image_t image;
	gx_drawing_color dcolor;
	gs_int_rect rect;
	const gs_imager_state *pis;
	const gx_clip_path *pcpath;
		/* begin_image command prepared & ready to output */
	int begin_image_command_length;
	byte begin_image_command[3 + 2 * cmd_sizew_max + 14 * sizeof(float)
	 + 4 * cmd_sizew_max];
	byte begin_image_b;
#ifdef FUTURE
	byte begin_image_b2;
#endif
		/* Set at creation time */
	int map_rgb_to_cmyk;
	void *default_info;
#ifndef FUTURE
	  /* NOTE: currently format is always gs_image_format_chunky. */
#endif
	gs_image_format_t format;
#ifndef FUTURE
	  /* NOTE: currently support is always (0,0). */
#endif
	gs_int_point support;	/* extra source pixels for interpolation */
#ifndef FUTURE
	  /* NOTE: currently num_planes is always 1. */
#endif
	int num_planes;
	int bits_per_plane;	/* bits per pixel per plane */
	gs_matrix matrix;	/* image space -> device space */
	bool uses_color;
	byte color_space;
	int ymin, ymax;
		/* Updated dynamically */
	int y;
	bool color_map_is_known;
} clist_image_enum;
/* We can disregard the pointers in the writer by allocating */
/* the image enumerator as immovable.  This is a hack, of course. */
gs_private_st_ptrs1(st_clist_image_enum, clist_image_enum, "clist_image_enum",
  clist_image_enum_enum_ptrs, clist_image_enum_reloc_ptrs, default_info);

/* Forward declarations */
private bool image_band_box(P5(gx_device *dev, const clist_image_enum *pie,
			       int y, int h, gs_int_rect *pbox));
private int cmd_image_data(P9(gx_device_clist_writer *cldev,
			      gx_clist_state *pcls,
			      const byte **planes, int num_planes,
			      uint offset, int data_x, uint raster,
			      uint bytes_per_plane, int h));
private uint	/* mask of unknown properties(see pcls->known) */
clist_image_unknowns(P2(gx_device *dev, const clist_image_enum *pie));

private int	/* ret 0 ok, else -ve error status */
write_image_end_all(P2(gx_device *dev, const clist_image_enum *pie));

/*
 * Since currently we are limited to writing a single subrectangle of the
 * image for each band, images that are rotated by angles other than
 * multiples of 90 degrees may wind up writing many copies of the data.
 * Eventually we will fix this by breaking up the image into multiple
 * subrectangles, but for now, don't use the high-level approach if it would
 * cause the data to explode because of this.
 */
private bool
image_matrix_ok_to_band(const gs_matrix *pmat)
{	double t;

	/* Don't band if the matrix is (nearly) singular. */
	if ( fabs(pmat->xx * pmat->yy - pmat->xy * pmat->yx) < 1.0e-6 )
	  return false;
	if ( is_xxyy(pmat) || is_xyyx(pmat) )
	  return true;
	t = (fabs(pmat->xx) + fabs(pmat->yy)) /
	  (fabs(pmat->xy) + fabs(pmat->yx));
	return (t < 0.2 || t > 5);
}

/* Start processing an image. */
private int
clist_begin_image(gx_device *dev,
  const gs_imager_state *pis, const gs_image_t *pim,
  gs_image_format_t format, const gs_int_rect *prect,
  const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
  gs_memory_t *mem, void **pinfo)
{	clist_image_enum *pie;
	int base_index;
	bool indexed;
	int num_components;
	int bits_per_pixel;
	bool uses_color;
	gs_matrix mat;
	int code;
	gs_rect sbox, dbox;
	int use_default_image;

	/* See above for why we allocate the enumerator as immovable. */
	pie = gs_alloc_struct_immovable(mem, clist_image_enum,
					&st_clist_image_enum,
					"clist_begin_image");
	if ( pie == 0 )
	  return_error(gs_error_VMerror);
	pie->memory = mem;
	*pinfo = pie;
	if ( pim->ImageMask )
	  { base_index = gs_color_space_index_DeviceGray;  /* arbitrary */
	    indexed = false;
	    num_components = 1;
	    uses_color = true;
	  }
	else
	  { const gs_color_space *pcs = pim->ColorSpace;
	    base_index = gs_color_space_get_index(pcs);
	    if ( base_index == gs_color_space_index_Indexed )
	      { const gs_color_space *pbcs =
		  gs_color_space_indexed_base_space(pcs);
		indexed = true;
		base_index = gs_color_space_get_index(pbcs);
		num_components = 1;
	      }
	    else
	      { indexed = false;
		num_components = gs_color_space_num_components(pcs);
	      }
	    uses_color = pim->CombineWithColor && rop3_uses_T(pis->log_op);
	  }

	use_default_image =
	 (
	     !USE_HL_IMAGES ||		/* Always use the default. */
	     (cdev->disable_mask & clist_disable_hl_image) || 
	     cdev->in_image ||		/* Can't handle nested images */
	     /****** CAN'T HANDLE CIE COLOR YET ******/
	     base_index > gs_color_space_index_DeviceCMYK ||
	     /****** CAN'T HANDLE INDEXED COLOR (READING MAP) ******/
	     indexed ||
	     /****** CAN'T HANDLE NON-PURE COLORS YET ******/
	     ( uses_color && !gx_dc_is_pure(pdcolor) ) ||
#ifdef DPNEXT
	     /****** CAN'T HANDLE IMAGES WITH ALPHA YET ******/
	     pim->HasAlpha ||
#endif
	     ( code = gs_matrix_invert(&pim->ImageMatrix, &mat) ) < 0 ||
	     ( code = gs_matrix_multiply(&mat, &ctm_only(pis), &mat) ) < 0 ||
	     !image_matrix_ok_to_band(&mat) ||
#ifndef FUTURE
	     /****** CAN'T HANDLE MULTI-PLANE IMAGES YET ******/
	     /****** (requires flipping in cmd_image_data) ******/
	     (  format != gs_image_format_chunky &&
	      ( num_components > 1 ||
	       ( format == gs_image_format_bit_planar &&
	          pim->BitsPerComponent > 1) )  ) ||
	     /****** CAN'T HANDLE INTERPOLATION YET ******/
	     pim->Interpolate ||	/* NB: interpolate won't work w/async rendering */
#endif
	     0
	 );
	if ( !use_default_image
	 && (cdev->disable_mask & clist_disable_nonrect_hl_image) )
	  use_default_image = !( is_xxyy(&mat) || is_xyyx(&mat) );
	if (!use_default_image)
	  { int bytes_per_plane, bytes_per_row;
	    /* Do cheap enum init, since some of it's needed for the test */
	    if ( prect )
	      pie->rect = *prect;
	    else
	      { pie->rect.p.x = 0, pie->rect.p.y = 0;
	        pie->rect.q.x = pim->Width, pie->rect.q.y = pim->Height;
	      }
	    bits_per_pixel = pim->BitsPerComponent * num_components;
	    pie->default_info = 0;
	    pie->image = *pim;
	    pie->dcolor = *pdcolor;
	    pie->pis = pis;
	    pie->pcpath = pcpath;
	    pie->format = format;
	    switch ( format )
	      {
	      case gs_image_format_chunky:
	        pie->num_planes = 1; break;
	      case gs_image_format_component_planar:
	        pie->num_planes = num_components; break;
	      case gs_image_format_bit_planar:
	        pie->num_planes = bits_per_pixel; break;
	      }
	    pie->bits_per_plane = bits_per_pixel / pie->num_planes;
	    pie->matrix = mat;
	    pie->uses_color = uses_color;
	    pie->color_space = (base_index << 4) |
	      (indexed ? (pim->ColorSpace->params.indexed.use_proc ? 12 : 8) : 0);
	    pie->y = pie->rect.p.y;

	    /* Image row has to fit in cmd writer's buffer */
	    bytes_per_plane
	     = (pim->Width * pie->bits_per_plane + 7) >> 3;
	    bytes_per_row = bytes_per_plane * pie->num_planes;
	    bytes_per_row = max(bytes_per_row, 1);
  	    use_default_image = cmd_largest_size + bytes_per_row
	     > cdev->cend - cdev->cbuf;
	  }
	if (!use_default_image)
	  { sbox.p.x = pie->rect.p.x;
	    sbox.p.y = pie->rect.p.y;
	    sbox.q.x = pie->rect.q.x;
	    sbox.q.y = pie->rect.q.y;
	    gs_bbox_transform(&sbox, &mat, &dbox);

	    if (cdev->disable_mask & clist_disable_complex_clip)
	      use_default_image
	       = !check_rect_for_trivial_clip( pcpath,
	       (int)(dbox.p.x), (int)(dbox.p.y),
	       (int)ceil(dbox.q.x), (int)ceil(dbox.q.y) );
	   }
	pie->map_rgb_to_cmyk = dev->color_info.num_components == 4 &&
	 base_index == gs_color_space_index_DeviceRGB;
	pie->color_map_is_known = false;
	if (use_default_image)
	    { int code = gx_default_begin_image(dev, pis, pim, format, prect,
						pdcolor, pcpath, mem,
						&pie->default_info);
	      if ( code < 0 )
		gs_free_object(mem, pie, "clist_begin_image");
	      return code;
	    }

	/* Create ready-4-use copy of the begin_image command(s) in enum */
	{ 
	  byte *cp = pie->begin_image_command;
	  byte b;
#ifdef FUTURE
	  b2 = 0;
#endif

	  cmd_put2w(pim->Width, pim->Height, cp);
	  if ( pim->ImageMask )
	    b = 0;
	  else switch ( pim->BitsPerComponent )
	    {
	    case 1: b = 1 << 5; break;
	    case 2: b = 2 << 5; break;
	    case 4: b = 3 << 5; break;
	    case 8: b = 4 << 5; break;
	    case 12: b = 5 << 5; break;
	    default: return_error(gs_error_rangecheck);
	    }
#ifdef FUTURE
	  if ( format != gs_image_format_chunky )
	    b |= 1 << 4,
	      b2 |= format << 6;
	  if ( pim->Interpolate )
	    { b |= 1 << 4;
	      b2 |= 1 << 5;
	      pie->support.x = pie->support.y = max_support + 1;
	    }
	  else
	    pie->support.x = pie->support.y = 0;
#else
	  if ( pim->Interpolate )
	    b |= 1 << 4;
	  pie->support.x = pie->support.y = 0;
#endif
	  if ( !(pim->ImageMatrix.xx == pim->Width &&
		 pim->ImageMatrix.xy == 0 &&
		 pim->ImageMatrix.yx == 0 &&
		 pim->ImageMatrix.yy == -pim->Height &&
		 pim->ImageMatrix.tx == 0 &&
		 pim->ImageMatrix.ty == pim->Height
		)
	     )
	    { b |= 1 << 3;
	      cp = cmd_for_matrix(cp, &pim->ImageMatrix);
	    }
	  { static const float base_decode[8] = {0, 1, 0, 1, 0, 1, 0, 1};
	    float indexed_decode[2];
	    const float *default_decode = base_decode;
	    int num_decode = num_components * 2;
	    int i;

	    if ( indexed )
	      { indexed_decode[0] = 0;
		indexed_decode[1] = (1 << pim->BitsPerComponent) - 1;
		default_decode = indexed_decode;
	      }
	    for ( i = 0; i < num_decode; ++i )
	      if ( pim->Decode[i] != default_decode[i] )
		break;
	    if ( i != num_decode )
	      { byte *pdb = cp++;
		byte dflags = 0;

		b |= 1 << 2;
		for ( i = 0; i < num_decode; i += 2 )
		  { float u = pim->Decode[i], v = pim->Decode[i+1];
		    dflags <<= 2;
		    if ( u == 0 && v == default_decode[i+1] )
		      ;
		    else if ( u == default_decode[i+1] && v == 0 )
		      dflags += 1;
		    else
		      { if ( u != 0 )
			  { dflags++;
			    memcpy(cp, &u, sizeof(float));
			    cp += sizeof(float);
			  }
			dflags += 2;
			memcpy(cp, &v, sizeof(float));
			cp += sizeof(float);
		      }
		  }
		*pdb = dflags << (8 - num_decode);
	      }
	  }
	  if ( (pim->ImageMask ? pim->adjust : pim->CombineWithColor) )
	    b |= 1 << 1;
	  { int y0 = (int)floor(dbox.p.y - 0.51);  /* adjust + rounding slop */
	    int y1 = (int)ceil(dbox.q.y + 0.51);   /* ditto */

	    pie->ymin = max(y0, 0);
	    pie->ymax = min(y1, dev->height);
	  }
	pie->begin_image_b = b;
#ifdef FUTURE
	pie->begin_image_b2 = b2;
#endif
	pie->begin_image_command_length = cp - pie->begin_image_command;
	}

	/*
	 * Make sure the CTM, color space, and clipping region (and, for
	 * masked images or images with CombineWithColor, the current color)
	 * are known at the time of the begin_image command.
	 */
	cmd_clear_known( cdev,
	 clist_image_unknowns(dev, pie) | begin_image_known );

	cdev->in_image = true;
	return 0;
}

/* Determine which image-related properties are unknown */
private uint	/* mask of unknown properties(see pcls->known) */
clist_image_unknowns(gx_device *dev, const clist_image_enum *pie)
{
	uint unknown = 0;
	/*
	 * Determine if the CTM, color space, and clipping region (and, for
	 * masked images or images with CombineWithColor, the current color)
	 * are unknown. Set the device state in anticipation of the values
	 * becoming known.
	 */
	if (cdev->imager_state.ctm.xx != pie->pis->ctm.xx ||
	    cdev->imager_state.ctm.xy != pie->pis->ctm.xy ||
	    cdev->imager_state.ctm.yx != pie->pis->ctm.yx ||
	    cdev->imager_state.ctm.yy != pie->pis->ctm.yy ||
	    cdev->imager_state.ctm.tx != pie->pis->ctm.tx ||
	    cdev->imager_state.ctm.ty != pie->pis->ctm.ty
	) { unknown |= ctm_known;
	    cdev->imager_state.ctm = pie->pis->ctm;
	  }
	/****** hival CHECK IS NOT SUFFICIENT ******/
	if ( cdev->color_space != pie->color_space ||
	     ((cdev->color_space & 8) != 0 &&
	      cdev->indexed_params.hival !=
	      pie->image.ColorSpace->params.indexed.hival)
	   )
	  { unknown |= color_space_known;
	    cdev->color_space = pie->color_space;
	    if ( cdev->color_space & 8 )
	      cdev->indexed_params = pie->image.ColorSpace->params.indexed;
	  }
	if ( cmd_check_clip_path(cdev, pie->pcpath) )
	  unknown |= clip_path_known;

	return unknown;
}

/* Process the next piece of an image. */
private int
clist_image_data(gx_device *dev,
  void *info, const byte **planes, int data_x, uint raster, int yh)
{	clist_image_enum *pie = info;
	gs_rect sbox, dbox;
	int code;
	int y0, y1;
	int y, height;		/* for BEGIN/END_RECT */
	int full_y, full_height;

	if ( pie->default_info )
	  return gx_default_image_data(dev, pie->default_info, planes, data_x,
				       raster, yh);
	sbox.p.x = pie->rect.p.x;
	sbox.p.y = y0 = pie->y;
	sbox.q.x = pie->rect.q.x;
	sbox.q.y = y1 = pie->y += yh;
	gs_bbox_transform(&sbox, &pie->matrix, &dbox);
	/*
	 * In order to keep the band list consistent, we must write out
	 * the image data in precisely those bands whose begin_image
	 * Y range includes the respective image scan lines.  Because of
	 * rounding, we must expand the dbox by a little extra, and then
	 * use image_band_box to calculate the precise range for each band.
	 * This is slow, but we don't see any faster way to do it in the
	 * general case.
	 */
	{ int ry0 = (int)floor(dbox.p.y) - 2;
	  int ry1 = (int)ceil(dbox.q.y) + 2;
	  int band_height = cdev->page_band_height;

	  y = max(ry0, 0) / band_height * band_height;
	  height = min(round_up(ry1, band_height), dev->height) - y;
	}

	BEGIN_RECT
	  gs_int_rect ibox;

	  /* If this band doesn't intersect data, skip it for now */
	  if ( !image_band_box(dev, pie, y, height, &ibox) )
	    continue;

	  /* Write out begin_image & its preamble for this band */
	  if ( !(pcls->known & begin_image_known) )
	    {
	      gs_logical_operation_t lop = pie->pis->log_op;
	      byte *dp;
	      gs_int_rect entirebox;
	      byte *bp = pie->begin_image_command + pie->begin_image_command_length;
	      byte cb = pie->begin_image_b;
#ifdef FUTURE
	      uint clen = (pie->begin_image_b & (1 << 4) ? 3 : 2);
#else
	      uint clen = 2;
#endif
	      uint len, total_len;
	      uint band_ymax, band_ymin;

	      /* Compute intersection of entire band & entire image src rect */
	      band_ymax = min(band_end, pie->ymax);
	      band_ymin = max(band_end - band_height, pie->ymin);
	      image_band_box(dev, pie, band_ymin,
	                     band_ymax - band_ymin, &entirebox);

	      /* Make sure the imager state is up to date. */
	      TRY_RECT
	        code = pie->color_map_is_known ? 0
	         : cmd_put_color_mapping(cdev, pie->pis, pie->map_rgb_to_cmyk);
			pie->color_map_is_known = true;
	        code = code < 0 ? code : cmd_do_write_unknown(cdev, pcls,
	                           ctm_known | clip_path_known | color_space_known);
	        code = code < 0 ? code :
	         cmd_do_enable_clip(cdev, pcls, pie->pcpath != NULL);
	        if ( lop == lop_default )
	          code = code < 0 ? code : cmd_disable_lop(cdev, pcls);
	        else
	          { if ( lop != pcls->lop )
	            {
	                code = code < 0 ? code : cmd_set_lop(cdev, pcls, lop);
	                code = code < 0 ? code : cmd_enable_lop(cdev, pcls);
	            }
	          }
	      HANDLE_RECT(code);
	      if ( pie->uses_color )
	        TRY_RECT
	          code = cmd_put_drawing_color(cdev, pcls, &pie->dcolor);
	        HANDLE_RECT(code);
	      if ( entirebox.p.x != 0 || entirebox.p.y != 0 ||
	           entirebox.q.x != pie->image.Width ||
		   entirebox.q.y != pie->image.Height
	         )
	        { cb |= 1 << 0;
	          cmd_put2w(entirebox.p.x, entirebox.p.y, bp);
	          cmd_put2w(pie->image.Width - entirebox.q.x,
	                    pie->image.Height - entirebox.q.y, bp);
	        }
	      len = bp - pie->begin_image_command;
	      total_len = clen + len;
	      TRY_RECT
	        code
	         = set_cmd_put_op(dp, cdev, pcls, cmd_opv_begin_image, total_len);
	      HANDLE_RECT(code);
	      dp[1] = cb;
#ifdef FUTURE
	      dp[2] = pie->begin_image_b2;
#endif
	      dp += clen;
	      memcpy(dp, pie->begin_image_command, len);

	      /* Mark band's begin_image as known */
	      pcls->known |= begin_image_known;
	    }
	  { /*
	     * Just transmit the subset of the data that intersects this band.
	     * Note that y and height always define a complete band.
	     */
	    int bpp = pie->bits_per_plane;
	    uint offset;
	    int iy, ih, xskip, nrows;
	    uint bytes_per_plane, bytes_per_row, rows_per_cmd;
#define bx0 ibox.p.x
#define by0 ibox.p.y
#define bx1 ibox.q.x
#define by1 ibox.q.y

	    if ( by0 < y0 )
	      by0 = y0;
	    if ( by1 > y1 )
	      by1 = y1;
	    offset = (by0 - y0) * raster;
	    /*
	     * Make sure we're skipping an integral number of pixels, by
	     * truncating the initial X coordinate to the next lower
	     * value that is an exact multiple of a byte.
	     */
	    xskip = bx0 & -(int)"\001\010\004\010\002\010\004\010"[bpp & 7];
	    offset += (xskip * bpp) >> 3;
	    xskip = bx0 - xskip;
	    bytes_per_plane = ((xskip + bx1 - bx0) * bpp + 7) >> 3;
	    bytes_per_row = bytes_per_plane * pie->num_planes;
	    rows_per_cmd =
	      (cbuf_size - cmd_largest_size) / max(bytes_per_row, 1);
	    if ( rows_per_cmd == 0 )
	      { /* The reader will have to buffer a row separately. */
	        rows_per_cmd = 1;
	      }
	    for ( iy = by0, ih = by1 - by0; ih > 0;
	          iy += nrows, ih -= nrows, offset += raster * nrows
	        )
	      { nrows = min(ih, rows_per_cmd);
	        TRY_RECT
	          code = cmd_image_data(cdev, pcls, planes, pie->num_planes,
	                                offset, data_x + xskip, raster,
	                                bytes_per_plane, nrows);
	        HANDLE_RECT(code);
	      }
#undef bx0
#undef by0
#undef bx1
#undef by1
	  }
	END_RECT_ON_ERROR(\
	 (  ++cdev->ignore_lo_mem_warnings,\
	  NEST_RECT,\
	  code = write_image_end_all(dev, pie),\
	  UNNEST_RECT,\
	  --cdev->ignore_lo_mem_warnings,\
	  ( code < 0 ? (band_code = code) : code ) >= 0  ),\
	 (  cmd_clear_known( cdev,\
	  clist_image_unknowns(dev, pie) | begin_image_known ),\
	  pie->color_map_is_known = false, true  )\
	)

	/* Update sub-rect in case memory exhaustion forced end_image */
	if (!pie->image.Interpolate)
	  pie->rect.p.y += yh;   /* interpolate & mem recovery currently incompat */
	return pie->y >= pie->rect.q.y;
}

/* Clean up by releasing the buffers. */
private int
clist_end_image(gx_device *dev, void *info, bool draw_last)
{	clist_image_enum *pie = info;
	int code;

	if ( pie->default_info )
	  code = gx_default_end_image(dev, pie->default_info, draw_last);
	else
	  {
	    NEST_RECT;
	    do
	      { code = write_image_end_all(dev, pie);
	      }
	    while (  code < 0 && cdev->error_is_retryable
	     && ( code = clist_try_recover_VMerror(cdev, code) ) >= 0  );

	    /* if couldn't write successsfully, do a hard flush */
	    if (code < 0 && cdev->error_is_retryable)
	      {
	      int retry_code;
	      ++cdev->ignore_lo_mem_warnings;
	      retry_code = write_image_end_all(dev, pie); /* force it out */
	      --cdev->ignore_lo_mem_warnings;
	      if (retry_code >= 0 && cdev->driver_call_nesting == 0)
	        code = clist_try_recover_VMerror_flush(cdev, code);
	      }
	    UNNEST_RECT;
	    cdev->in_image = false;
	  }
	gs_free_object(pie->memory, pie, "clist_end_image");
	return code;
}

/* ------ Utilities ------ */

/* Add commands to represent a halftone order. */
private int
cmd_put_ht_order(gx_device_clist_writer *cldev, const gx_ht_order *porder,
  gs_ht_separation_name cname,
  int component /* -1 = default/gray/black screen */)
{	byte command[cmd_max_intsize(sizeof(long)) * 8];
	byte *cp;
	uint len;
	byte *dp;
	uint i, n;
	int code;

	/* Put out the order parameters. */
	cp = cmd_put_w(component + 1, command);
	if ( component >= 0 )
	  cp = cmd_put_w(cname, cp);
	cp = cmd_put_w(porder->width, cp);
	cp = cmd_put_w(porder->height, cp);
	cp = cmd_put_w(porder->raster, cp);
	cp = cmd_put_w(porder->shift, cp);
	cp = cmd_put_w(porder->num_levels, cp);
	cp = cmd_put_w(porder->num_bits, cp);
	len = cp - command;
	code = set_cmd_put_all_op(dp, cldev, cmd_opv_set_ht_order, len + 1);
	if (code < 0)
	  return code;
	memcpy(dp + 1, command, len);

	/* Put out the transfer function, if any. */
	cmd_put_color_map(cldev, cmd_map_ht_transfer, porder->transfer,
			  NULL);

	/* Put out the levels array. */
#define nlevels min((cbuf_size - 2) / sizeof(*porder->levels), 255)
	for ( i = 0; i < porder->num_levels; i += n )
	  { n = porder->num_levels - i;
	    if ( n > nlevels )
	      n = nlevels;
	    code = set_cmd_put_all_op(dp, cldev, cmd_opv_set_ht_data,
			              2 + n * sizeof(*porder->levels));
	    if (code < 0)
	      return code;
	    dp[1] = n;
	    memcpy(dp + 2, porder->levels + i, n * sizeof(*porder->levels));
	  }
#undef nlevels

	/* Put out the bits array. */
#define nbits min((cbuf_size - 2) / sizeof(*porder->bits), 255)
	for ( i = 0; i < porder->num_bits; i += n )
	  { n = porder->num_bits - i;
	    if ( n > nbits )
	      n = nbits;
	    code = set_cmd_put_all_op(dp, cldev, cmd_opv_set_ht_data,
			              2 + n * sizeof(*porder->bits));
	    if (code < 0)
	      return code;
	    dp[1] = n;
	    memcpy(dp + 2, porder->bits + i, n * sizeof(*porder->bits));
	  }
#undef nbits

	return 0;
}

/* Add commands to represent a full (device) halftone. */
/* We put out the default/gray/black screen last so that the reading */
/* pass can recognize the end of the halftone. */
int
cmd_put_halftone(gx_device_clist_writer *cldev, const gx_device_halftone *pdht,
  gs_halftone_type type)
{	uint num_comp = (pdht->components == 0 ? 0 : pdht->num_comp);

	{ byte *dp;

	  int code = set_cmd_put_all_op(dp, cldev, cmd_opv_set_misc,
			                2 + cmd_size_w(num_comp));
	  if (code < 0)
	    return code;
	  dp[1] = cmd_set_misc_halftone + type;
	  cmd_put_w(num_comp, dp + 2);
	}
	if ( num_comp == 0 )
	  return cmd_put_ht_order(cldev, &pdht->order,
				  gs_ht_separation_Default, -1);
	{ int i;

	  for ( i = num_comp; --i >= 0; )
	    { int code = cmd_put_ht_order(cldev, &pdht->components[i].corder,
					  pdht->components[i].cname, i);
	      if ( code < 0 )
		return code;
	    }
	}
	return 0;
}

/* Write out any necessary color mapping data. */
private int
cmd_put_color_mapping(gx_device_clist_writer *cldev,
  const gs_imager_state *pis, bool write_rgb_to_cmyk)
{	int code;
	const gx_device_halftone *pdht = pis->dev_ht;

	/* Put out the halftone. */
	if ( pdht->id != cldev->device_halftone_id )
	  { code = cmd_put_halftone(cldev, pdht, pis->halftone->type);
	    if ( code < 0 )
	      return code;
	    cldev->device_halftone_id = pdht->id;
	  }

	/* If we need to map RGB to CMYK, put out b.g. and u.c.r. */
	if ( write_rgb_to_cmyk )
	  { code = cmd_put_color_map(cldev, cmd_map_black_generation,
				     pis->black_generation,
				     &cldev->black_generation_id);
	    if ( code < 0 )
	      return code;
	    code = cmd_put_color_map(cldev, cmd_map_undercolor_removal,
				     pis->undercolor_removal,
				     &cldev->undercolor_removal_id);
	    if ( code < 0 )
	      return code;
	  }

	/* Now put out the transfer functions. */
	{ uint which = 0;
	  bool all_same = true;
	  int i;

	  for ( i = 0; i < countof(cldev->transfer_ids); ++i )
	    { if ( pis->effective_transfer.indexed[i]->id !=
		   cldev->transfer_ids[i]
		 )
	        which |= 1 << i;
	      if ( pis->effective_transfer.indexed[i]->id !=
		   pis->effective_transfer.indexed[0]->id
		 )
		all_same = false;
	    }
	  /* There are 3 cases for transfer functions: nothing to write, */
	  /* a single function, and multiple functions. */
	  if ( which == 0 )
	    return 0;
	  if ( which == (1 << countof(cldev->transfer_ids)) - 1 && all_same )
	    { code = cmd_put_color_map(cldev, cmd_map_transfer,
				       pis->effective_transfer.indexed[0],
				       &cldev->transfer_ids[0]);
	      if ( code < 0 )
		return code;
	      for ( i = 1; i < countof(cldev->transfer_ids); ++i )
		cldev->transfer_ids[i] = cldev->transfer_ids[0];
	    }
	  else
	    for ( i = 0; i < countof(cldev->transfer_ids); ++i )
	      { code = cmd_put_color_map(cldev,
				(cmd_map_index)(cmd_map_transfer_0 + i),
				pis->effective_transfer.indexed[i],
				&cldev->transfer_ids[i]);
	        if ( code < 0 )
		  return code;
	      }
	}

	return 0;
}

/*
 * Compute the subrectangle of an image that intersects a band;
 * return false if it is empty.
 * It is OK for this to be too large; in fact, with the present
 * algorithm, it will be quite a bit too large if the transformation isn't
 * well-behaved ("well-behaved" meaning either xy = yx = 0 or xx = yy = 0).
 */
private void
box_merge_point(gs_int_rect *pbox, floatp x, floatp y)
{	int t;

	if ( (t = (int)floor(x)) < pbox->p.x )
	  pbox->p.x = t;
	if ( (t = (int)ceil(x)) > pbox->q.x )
	  pbox->q.x = t;
	if ( (t = (int)floor(y)) < pbox->p.y )
	  pbox->p.y = t;
	if ( (t = (int)ceil(y)) > pbox->q.y )
	  pbox->q.y = t;
}
private bool
image_band_box(gx_device *dev, const clist_image_enum *pie, int y, int h,
  gs_int_rect *pbox)
{	fixed by0 = int2fixed(y);
	fixed by1 = int2fixed(y + h);
	int px = pie->rect.p.x, py = pie->rect.p.y,
	  qx = pie->rect.q.x, qy = pie->rect.q.y;
	gs_fixed_rect cbox;	/* device clipping box */
	gs_rect bbox;		/* cbox intersected with band */

	/* Intersect the device clipping box and the band. */
	(*dev_proc(dev, get_clipping_box))(dev, &cbox);
	/* The fixed_half here is to allow for adjustment. */
	bbox.p.x = fixed2float(cbox.p.x - fixed_half);
	bbox.q.x = fixed2float(cbox.q.x + fixed_half);
	bbox.p.y = fixed2float(max(cbox.p.y, by0) - fixed_half);
	bbox.q.y = fixed2float(min(cbox.q.y, by1) + fixed_half);
#ifdef DEBUG
	if ( gs_debug_c('b') )
	  { dprintf6("[b]band box for (%d,%d),(%d,%d), band (%d,%d) =>\n",
		      px, py, qx, qy, y, y + h);
	    dprintf10("      (%g,%g),(%g,%g), matrix=[%g %g %g %g %g %g]\n",
		      bbox.p.x, bbox.p.y, bbox.q.x, bbox.q.y,
		      pie->matrix.xx, pie->matrix.xy, pie->matrix.yx,
		      pie->matrix.yy, pie->matrix.tx, pie->matrix.ty);
	  }
#endif
	if ( is_xxyy(&pie->matrix) || is_xyyx(&pie->matrix) )
	  { /*
	     * The inverse transform of the band is a rectangle aligned with
	     * the coordinate axes, so we can just intersect it with the
	     * image subrectangle.
	     */
	    gs_rect ibox;	/* bbox transformed back to image space */

	    if ( gs_bbox_transform_inverse(&bbox, &pie->matrix, &ibox) < 0 )
	      return false;
	    pbox->p.x = max(px, (int)floor(ibox.p.x));
	    pbox->q.x = min(qx, (int)ceil(ibox.q.x));
	    pbox->p.y = max(py, (int)floor(ibox.p.y));
	    pbox->q.y = min(qy, (int)ceil(ibox.q.y));
	  }
	else
	  { /*
	     * The inverse transform of the band is not aligned with the
	     * axes, i.e., is a general parallelogram.  To compute an exact
	     * bounding box, we need to find the intersections of this
	     * parallelogram with the image subrectangle.
	     *
	     * There is probably a much more efficient way to do this
	     * computation, but we don't know what it is.
	     */
	    gs_point rect[4];
	    gs_point corners[5];
	    int i;

	    /* Store the corners of the image rectangle. */
	    rect[0].x = rect[3].x = px;
	    rect[1].x = rect[2].x = qx;
	    rect[0].y = rect[1].y = py;
	    rect[2].y = rect[3].y = qy;
	    /*
	     * Compute the corners of the clipped band in image space.  If
	     * the matrix is singular or an overflow occurs, the result will
	     * be nonsense: in this case, there isn't anything useful we
	     * can do, so return an empty intersection.
	     */
	    if ( gs_point_transform_inverse(bbox.p.x, bbox.p.y, &pie->matrix,
					    &corners[0]) < 0 ||
		 gs_point_transform_inverse(bbox.q.x, bbox.p.y, &pie->matrix,
					    &corners[1]) < 0 ||
		 gs_point_transform_inverse(bbox.q.x, bbox.q.y, &pie->matrix,
					    &corners[2]) < 0 ||
		 gs_point_transform_inverse(bbox.p.x, bbox.q.y, &pie->matrix,
					    &corners[3]) < 0
	       ) {
	      if_debug0('b', "[b]can't inverse-transform a band corner!\n");
	      return false;
	    }
	    corners[4] = corners[0];
	    pbox->p.x = qx, pbox->p.y = qy;
	    pbox->q.x = px, pbox->q.y = py;
	    /*
	     * We iterate over both the image rectangle and the band
	     * parallelogram in a single loop for convenience, even though
	     * there is no coupling between the two.
	     */
	    for ( i = 0; i < 4; ++i )
	      { gs_point pa, pt;
	        double dx, dy;

		/* Check the image corner for being inside the band. */
		pa = rect[i];
		gs_point_transform(pa.x, pa.y, &pie->matrix, &pt);
		if ( pt.x >= bbox.p.x && pt.x <= bbox.q.x &&
		     pt.y >= bbox.p.y && pt.y <= bbox.q.y
		   )
		  box_merge_point(pbox, pa.x, pa.y);
		/* Check the band corner for being inside the image. */
	        pa = corners[i];
		if ( pa.x >= px && pa.x <= qx && pa.y >= py && pa.y <= qy )
		  box_merge_point(pbox, pa.x, pa.y);
		/* Check for intersections of band edges with image edges. */
		dx = corners[i+1].x - pa.x;
		dy = corners[i+1].y - pa.y;
#define in_range(t, tc, p, q)\
  (0 <= t && t <= 1 && (t = tc) >= p && t <= q)
		if ( dx != 0 )
		  { double t = (px - pa.x) / dx;

		    if_debug3('b', "   (px) t=%g => (%d,%g)\n",
			      t, px, pa.y + t * dy);
		    if ( in_range(t, pa.y + t * dy, py, qy) )
		      box_merge_point(pbox, (floatp)px, t);
		    t = (qx - pa.x) / dx;
		    if_debug3('b', "   (qx) t=%g => (%d,%g)\n",
			      t, qx, pa.y + t * dy);
		    if ( in_range(t, pa.y + t * dy, py, qy) )
		      box_merge_point(pbox, (floatp)qx, t);
		  }
		if ( dy != 0 )
		  { double t = (py - pa.y) / dy;

		    if_debug3('b', "   (py) t=%g => (%g,%d)\n",
			      t, pa.x + t * dx, py);
		    if ( in_range(t, pa.x + t * dx, px, qx) )
		      box_merge_point(pbox, t, (floatp)py);
		    t = (qy - pa.y) / dy;
		    if_debug3('b', "   (qy) t=%g => (%g,%d)\n",
			      t, pa.x + t * dx, qy);
		    if ( in_range(t, pa.x + t * dx, px, qx) )
		      box_merge_point(pbox, t, (floatp)qy);
		  }
#undef in_range
	      }
	  }
	if_debug4('b', "    => (%d,%d),(%d,%d)\n", pbox->p.x, pbox->p.y,
		  pbox->q.x, pbox->q.y);
#ifdef FUTURE
	/*
	 * If necessary, add pixels around the edges so we will have
	 * enough information to do interpolation.
	 */
	if ( (pbox->p.x -= pie->support.x) < pie->rect.p.x )
	  pbox->p.x = pie->rect.p.x;
	if ( (pbox->p.y -= pie->support.y) < pie->rect.p.y )
	  pbox->p.y = pie->rect.p.y;
	if ( (pbox->q.x += pie->support.x) > pie->rect.q.x )
	  pbox->q.x = pie->rect.q.x;
	if ( (pbox->q.y += pie->support.y) > pie->rect.q.y )
	  pbox->q.y = pie->rect.q.y;
#endif
	return (pbox->p.x < pbox->q.x && pbox->p.y < pbox->q.y);
}

/* Write data for a partial image. */
private int
cmd_image_data(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  const byte **planes, int num_planes, uint offset, int data_x, uint raster,
  uint bytes_per_plane, int h)
{	uint nbytes = bytes_per_plane * num_planes * h;
	uint len = 1 + cmd_size2w(h, bytes_per_plane) + nbytes;
	byte *dp;
	int plane, i;
	int code;

	if ( data_x )
	  { code = set_cmd_put_op(dp, cldev, pcls, cmd_opv_set_misc, 2);
	    if (code < 0)
	      return code;
	    dp[1] = cmd_set_misc_data_x + (data_x & 7);
	    offset += ((data_x & ~7) * cldev->color_info.depth) >> 3;
	  }
	code = set_cmd_put_op(dp, cldev, pcls, cmd_opv_image_data, len);
	if (code < 0)
	  return code;
	dp++;
	cmd_put2w(h, bytes_per_plane, dp);
	for ( plane = 0; plane < num_planes; ++plane )
	  for ( i = 0; i < h; ++i )
	    { memcpy(dp, planes[plane] + i * raster + offset, bytes_per_plane);
	      dp += bytes_per_plane;
	    }
	return 0;
}

/* Write image_end commands into all bands */
private int	/* ret 0 ok, else -ve error status */
write_image_end_all(gx_device *dev, const clist_image_enum *pie)
{	int code;
	int y = pie->ymin;
	int height = pie->ymax - y;
	BEGIN_RECT
	  byte *dp;

	  if ( !(pcls->known & begin_image_known) )
	    continue;
	  TRY_RECT
	    if_debug1('L', "[L]image_end for band %d\n", band);
	    code = set_cmd_put_op(dp, cdev, pcls, cmd_opv_image_data, 2);
	  HANDLE_RECT(code);
	  dp[1] = 0;	    /* EOD */
	  pcls->known ^= begin_image_known;
	END_RECT
	return 0;
}

/* Compare a rectangle vs. clip path. See if rect is trivially accepted, */
/* or is only clipped by a single rectangle. */
private bool	/* ret true if no clip, rect trivially accepted, or only clipped *
	/* by one rectangle. */
check_rect_for_trivial_clip(
	const gx_clip_path *pcpath,	/* May be NULL, clip to evaluate */
	int px, int py, int qx, int qy	/* corners of box to test */
)
{	gs_fixed_rect obox;
	gs_fixed_rect imgbox;

	if (!pcpath)
	  return true;

	imgbox.p.x = int2fixed(px);
	imgbox.p.y = int2fixed(py);
	imgbox.q.x = int2fixed(qx);
	imgbox.q.y = int2fixed(qy);
	if ( gx_cpath_includes_rectangle(pcpath,
	 imgbox.p.x, imgbox.p.y, imgbox.q.x, imgbox.q.y) );
	  return true;

	return ( gx_cpath_outer_box(pcpath, &obox) &&
	 obox.p.x <= imgbox.q.x && obox.q.x >= imgbox.p.x &&
	 obox.p.y <= imgbox.q.y && obox.q.y >= imgbox.p.y );
}
