/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcdraw.h */
/* PCL5 drawing utilities */
#include "std.h"
#include "math_.h"
#include "gstypes.h"		/* for gsstate.h */
#include "gsmatrix.h"		/* for gsstate.h */
#include "gsmemory.h"		/* for gsstate.h */
#include "gsstate.h"
#include "gscspace.h"		/* for gscolor2.h */
#include "gscolor2.h"		/* for gs_makebitmappattern */
#include "gscoord.h"
#include "gsdcolor.h"
#include "gsimage.h"
#include "gsutil.h"
#include "gxfixed.h"
#include "gxpath.h"
#include "gxstate.h"
#include "plvalue.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcfont.h"		/* for underline break/continue, HMI */
#include "pcdraw.h"

/* ------ Coordinate system ------ */

/* Compute the logical page size from physical size, orientation, */
/* and offsets. */
void
pcl_compute_logical_page_size(pcl_state_t *pcls)
{	const pcl_paper_size_t *psize = pcls->paper_size;

	/**** SHOULD USE left/top_offset_cp ****/
	if ( pcls->orientation & 1 )
	  { pcls->logical_page_width =
	      psize->height - 2 * psize->offset_landscape.x;
	    pcls->logical_page_height = psize->width;
	  }
	else
	  { pcls->logical_page_width =
	      psize->width - 2 * psize->offset_portrait.x;
	    pcls->logical_page_height = psize->height;
	  }
	if ( pcls->print_direction & 1 )
	  pcls->rotated_page_size.x = pcls->logical_page_size.y,
	    pcls->rotated_page_size.y = pcls->logical_page_size.x;
	else
	  pcls->rotated_page_size = pcls->logical_page_size;
}

/* Set the CTM from the current left and top offset, orientation, */
/* and (optionally) print direction. */
/* We export this only for HP-GL/2. */
int
pcl_set_ctm(pcl_state_t *pcls, bool print_direction)
{	gs_state *pgs = pcls->pgs;
	const pcl_paper_size_t *psize = pcls->paper_size;
	int rotation =
	  (print_direction ? pcls->orientation + pcls->print_direction :
	   pcls->orientation) & 3;

	gs_initmatrix(pgs);
	/**** DOESN'T HANDLE NEGATION FOR DUPLEX BINDING ****/
	/*
	 * Note that because we are dealing with an opposite-handed
	 * coordinate system, all the rotations must be negated.
	 */
	switch ( rotation )
	  {
	  case 0:
	    gs_translate(pgs, psize->offset_portrait.x + pcls->left_offset_cp,
			 pcls->top_offset_cp);
	    break;
	  case 1:
	    gs_rotate(pgs, -90.0);
	    gs_translate(pgs, psize->offset_landscape.x -
			 (psize->height + pcls->top_offset_cp),
			 pcls->left_offset_cp);
	    break;
	  case 2:
	    gs_rotate(pgs, -180.0);
	    gs_translate(pgs, psize->offset_portrait.x -
			 (psize->width + pcls->left_offset_cp),
			 -(psize->height + pcls->top_offset_cp));
	    break;
	  case 3:
	    gs_rotate(pgs, -270.0);
	    gs_translate(pgs,
			 psize->offset_landscape.x + pcls->top_offset_cp,
			 -(psize->width + pcls->left_offset_cp));
	    break;
	  }
	return 0;
}

/* Set the clipping region to the PCL printable area. */
private int
pcl_set_clip(pcl_state_t *pcls)
{	const pcl_paper_size_t *psize = pcls->paper_size;
	gs_matrix imat;
	fixed tx, ty;
	gs_fixed_rect box;

	gs_defaultmatrix(pcls->pgs, &imat);
	tx = float2fixed(imat.tx);
	ty = float2fixed(imat.ty);
	/* Per Figure 2-4 on page 2-9 of the PCL5 TRM, the printable area */
	/* is always just 1/6" indented from the physical page. */
	box.p.x = float2fixed(pcls->resolution.x / 6.0) + tx;
	box.p.y = float2fixed(pcls->resolution.y / 6.0) + ty;
	box.q.x = float2fixed(psize->width / 7200.0 * pcls->resolution.x)
	  - box.p.x + tx * 2;
	box.q.y = float2fixed(psize->height / 7200.0 * pcls->resolution.y)
	  - box.p.y + ty * 2;
	return gx_clip_to_rectangle(pcls->pgs, &box);
}

/* Set all necessary graphics state parameters for PCL drawing */
/* (currently only CTM and clipping region). */
int
pcl_set_graphics_state(pcl_state_t *pcls, bool print_direction)
{	int code = pcl_set_ctm(pcls, print_direction);
	return (code < 0 ? code : pcl_set_clip(pcls));
}

/* Set the PCL print direction [0..3]. */
/* We export this only for HP-GL/2. */
int
pcl_set_print_direction(pcl_state_t *pcls, int print_direction)
{	if ( pcls->print_direction != print_direction )
	  { /*
	     * Recompute the CAP in the new coordinate system.  Also permute
	     * the margins per TRM 5-11, and the rotated logical page size.
	     */
	    gs_state *pgs = pcls->pgs;
	    /* Yes, the reverse difference is correct. */
	    int diff = (pcls->print_direction - print_direction) & 3;
	    gs_point mp, mq, capt;

	    /* Transform the CAP and margins to device space. */
	    pcl_set_ctm(pcls, true);
	    gs_transform(pgs, (floatp)pcls->cap.x, (floatp)pcls->cap.y, &capt);
	    gs_transform(pgs, (floatp)pcls->left_margin,
			 (floatp)pcls->top_margin, &mp);
	    gs_transform(pgs, (floatp)pcls->right_margin,
			 (floatp)(pcls->top_margin +
				  pcls->text_length * pcls->vmi),
			 &mq);
	    /* Now we can reset the print direction. */
	    pcls->print_direction = print_direction;
	    if ( pcls->print_direction & 1 )
	      pcls->rotated_page_size.x = pcls->logical_page_size.y,
		pcls->rotated_page_size.y = pcls->logical_page_size.x;
	    else
	      pcls->rotated_page_size = pcls->logical_page_size;
	    /* Transform the CAP and margins back again. */
	    pcl_set_ctm(pcls, true);
	    gs_itransform(pgs, capt.x, capt.y, &capt);
	    pcls->cap.x = (coord)capt.x;
	    pcls->cap.y = (coord)capt.y;
	    gs_itransform(pgs, mp.x, mp.y, &mp);
	    gs_itransform(pgs, mq.x, mq.y, &mq);
	    if ( diff & 2 )
	      { gs_point mt;
	        mt = mq; mq = mp; mp = mt;
	      }
	    if ( diff & 1 )
	      { double mt;
	        mt = mp.y; mp.y = mq.y; mq.y = mt;
	      }
	    pcls->left_margin = mp.x;
	    pcls->top_margin = mp.y;
	    pcls->right_margin = mq.x;
	    pcls->text_length = (mq.y - pcls->top_margin) / pcls->vmi;
	  }
	return 0;
}

/* ------ Cursor ------ */

/* Home the cursor to the left edge of the logical page at the top margin. */
void
pcl_home_cursor(pcl_state_t *pcls)
{	
	pcl_break_underline(pcls);
	pcls->cap.x = 0;
	/* See TRM 5-18 for the following computation. */
	pcls->cap.y = pcls->top_margin + 0.75 * pcls->vmi;
	pcl_continue_underline(pcls);
}

/* Set the cursor X or Y position, with clamping to either the margins */
/* or the logical page limit. */
void
pcl_set_cursor_x(pcl_state_t *pcls, coord cx, bool to_margins)
{	/* Note that the cursor position is relative to the logical page. */
	if ( to_margins )
	  { if ( cx < pcls->left_margin )
	      cx = pcls->left_margin;
	    if ( cx > pcls->right_margin )
	      cx = pcls->right_margin;
	  }
	else
	  { if ( cx < 0 )
	      cx = 0;
	    if ( cx > pcls->logical_page_width )
	      cx = pcls->logical_page_width;
	  }
	if ( cx < pcls->cap.x )
	  {
	    pcl_break_underline(pcls);
	    pcls->cap.x = cx;
	    pcl_continue_underline(pcls);
	  }
	else
	  pcls->cap.x = cx;
}
void
pcl_set_cursor_y(pcl_state_t *pcls, coord cy, bool to_margins)
{	/**** DOESN'T CLAMP YET ****/
	pcl_break_underline(pcls);
	pcls->cap.y = cy;
	pcl_continue_underline(pcls);
}

/* Set the HMI. */
void
pcl_set_hmi(pcl_state_t *pcls, coord hmi, bool explicit)
{	pcls->hmi_unrounded = hmi;
	pcls->hmi_set = (explicit ? hmi_set_explicitly : hmi_set_from_font);
	/* See the description of the Units of Measure command */
	/* in the PCL5 documentation for an explanation of the following. */
	pcls->hmi =
	  (explicit ? hmi :
	   (hmi + (pcls->uom_cp >> 1)) / pcls->uom_cp * pcls->uom_cp);
}	  

/* Update the HMI by recomputing it from the font. */
coord
pcl_updated_hmi(pcl_state_t *pcls)
{	coord hmi;
	int code = pcl_recompute_font(pcls);
	const pl_font_t *plfont = pcls->font;

	if ( code < 0 )
	  { /* Bad news.  Don't mark the HMI as valid. */
	    return pcls->hmi;
	  }
	if ( pl_font_is_scalable(plfont) )
	  { /* Scale the font's pitch by the requested height. */
	    hmi =
	      pl_fp_pitch_cp(&plfont->params) *
	      pcls->font_selection[pcls->font_selected].params.height_4ths / 4;
	  }
	else
	  { hmi = pl_fp_pitch_cp(&plfont->params);
	  }
	pcl_set_hmi(pcls, hmi, false);
	return pcls->hmi;
}

/* ------ Color / pattern ------ */

/* Define the built-in shading patterns. */
/* These are 8x8 bits (except for the first which is 8x16), */
/* replicated horizontally and vertically to 64x16. */
#define r4be(a,b,c,d)\
  (((uint32)(a) << 24) + ((uint32)(b) << 16) + ((c) << 8) + (d))
#if arch_is_big_endian
#  define r4(a,b,c,d) r4be(a,b,c,d), r4be(a,b,c,d)
#else
#  define r4(a,b,c,d) r4be(d,c,b,a), r4be(d,c,b,a)
#endif
#define r2(a,b) r4(a,b,a,b)
#define r1(a) r2(a,a)
#define p4(a,b,c,d) r1(a), r1(b), r1(c), r1(d)
#define p8(a,b,c,d,e,f,g,h) p4(a,b,c,d), p4(e,f,g,h)
#define p2x8(a,b,c,d,e,f,g,h) p8(a,b,c,d,e,f,g,h), p8(a,b,c,d,e,f,g,h) 
private const uint32
  shade_1_2[] = {
    p8(0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
    p8(0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)
  },
  shade_3_10[] = {
    p2x8(0x80, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00)
  },
  shade_11_20[] = {
    p2x8(0xc0, 0xc0, 0x00, 0x00, 0x0c, 0x0c, 0x00, 0x00)
  },
  shade_21_35[] = {
    p2x8(0x20, 0x70, 0x70, 0x20, 0x02, 0x07, 0x07, 0x02)
  },
  shade_36_55[] = {
    p2x8(0x22, 0x70, 0xfa, 0x70, 0x22, 0x07, 0xaf, 0x07)
  },
  shade_56_80[] = {
    p2x8(0x1f, 0x1f, 0x1f, 0xff, 0xf1, 0xf1, 0xf1, 0xff)
  },
  shade_81_99[] = {
    p2x8(0xbf, 0x1f, 0xbf, 0xff, 0xfb, 0xf1, 0xfb, 0xff)
  };

/* Define the built-in cross-hatch patterns. */
/* These are 16x16 bits, replicated horizontally to 64x16. */
/* Note that the 45-degree patterns are swapped because of the inverted-Y */
/* coordinate system. */
private const uint32
  ch_horizontal[] = {
    p8(0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00),
    p8(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)
  },
  ch_vertical[] = {
    r2(0x01, 0x80), r2(0x01, 0x80), r2(0x01, 0x80), r2(0x01, 0x80),
    r2(0x01, 0x80), r2(0x01, 0x80), r2(0x01, 0x80), r2(0x01, 0x80),
    r2(0x01, 0x80), r2(0x01, 0x80), r2(0x01, 0x80), r2(0x01, 0x80),
    r2(0x01, 0x80), r2(0x01, 0x80), r2(0x01, 0x80), r2(0x01, 0x80)
  },
  ch_45_degree[] = {
    r2(0x80, 0x01), r2(0xc0, 0x00), r2(0x60, 0x00), r2(0x30, 0x00),
    r2(0x18, 0x00), r2(0x0c, 0x00), r2(0x06, 0x00), r2(0x03, 0x00),
    r2(0x01, 0x80), r2(0x00, 0xc0), r2(0x00, 0x60), r2(0x00, 0x30),
    r2(0x00, 0x18), r2(0x00, 0x0c), r2(0x00, 0x06), r2(0x00, 0x03)
  },
  ch_135_degree[] = {
    r2(0x00, 0xc0), r2(0x01, 0x80), r2(0x03, 0x00), r2(0x06, 0x00),
    r2(0x0c, 0x00), r2(0x18, 0x00), r2(0x30, 0x00), r2(0x60, 0x00),
    r2(0xc0, 0x00), r2(0x80, 0x01), r2(0x00, 0x03), r2(0x00, 0x06),
    r2(0x00, 0x0c), r2(0x00, 0x18), r2(0x00, 0x30), r2(0x00, 0x60)
  },
  ch_square[] = {
    r2(0x01, 0x80), r2(0x01, 0x80), r2(0xff, 0xff), r2(0xff, 0xff),
    r2(0x01, 0x80), r2(0x01, 0x80), r2(0x01, 0x80), r2(0x01, 0x80),
    r2(0x01, 0x80), r2(0x01, 0x80), r2(0x01, 0x80), r2(0x01, 0x80),
    r2(0x01, 0x80), r2(0x01, 0x80), r2(0x01, 0x80), r2(0x01, 0x80)
  },
  ch_diamond[] = {
    r2(0x80, 0xc1), r2(0xc1, 0x80), r2(0x63, 0x00), r2(0x36, 0x00),
    r2(0x1c, 0x00), r2(0x1c, 0x00), r2(0x36, 0x00), r2(0x63, 0x00),
    r2(0xc1, 0x80), r2(0x80, 0xc1), r2(0x00, 0x63), r2(0x00, 0x36),
    r2(0x00, 0x1c), r2(0x00, 0x1c), r2(0x00, 0x36), r2(0x00, 0x63)
  };
private const uint32 *cross_hatch_patterns[] = {
  ch_horizontal, ch_vertical, ch_45_degree, ch_135_degree,
  ch_square, ch_diamond
};

/* Define an analogue of makebitmappattern that allows scaling. */
/* (This might get migrated back into the library someday.) */
private int pcl_bitmap_PaintProc(P2(const gs_client_color *, gs_state *));
int
pcl_makebitmappattern(gs_client_color *pcc, const gx_tile_bitmap *tile,
  gs_state *pgs, floatp scale_x, floatp scale_y, int rotation /* 0..3 */)
{	gs_client_pattern pat;
	gs_matrix mat, smat;
	int angle = rotation * 90;

	if ( tile->raster != bitmap_raster(tile->size.x) )
	  return_error(gs_error_rangecheck);
	uid_set_UniqueID(&pat.uid, gs_next_ids(1));
	pat.PaintType = 1;
	pat.TilingType = 1;
	pat.BBox.p.x = 0;
	pat.BBox.p.y = 0;
	pat.BBox.q.x = tile->size.x;
	pat.BBox.q.y = tile->rep_height;
	pat.XStep = tile->size.x;
	pat.YStep = tile->rep_height;
	pat.PaintProc = pcl_bitmap_PaintProc;
	pat.client_data = tile->data;
	gs_currentmatrix(pgs, &smat);
	gs_make_identity(&mat);
	gs_setmatrix(pgs, &mat);
	gs_make_scaling(scale_x, scale_y, &mat);
	if ( smat.yy > 0 )
	  mat.yy = -mat.yy, angle = -angle;
	gs_matrix_rotate(&mat, angle, &mat);
	gs_makepattern(pcc, &pat, &mat, pgs, NULL);
	gs_setmatrix(pgs, &smat);
	return 0;
}
private int
pcl_bitmap_PaintProc(const gs_client_color *pcolor, gs_state *pgs)
{	gs_image_enum *pen =
	  gs_image_enum_alloc(gs_state_memory(pgs), "bitmap_PaintProc");
	const gs_client_pattern *ppat = gs_getpattern(pcolor);
	const byte *dp = ppat->client_data;
	gs_image_t image;
	int n;
	uint nbytes, raster, used;

	gs_image_t_init_gray(&image);
	image.Decode[0] = 1.0;
	image.Decode[1] = 0.0;
	image.Width = (int)ppat->XStep;
	image.Height = (int)ppat->YStep;
	raster = bitmap_raster(image.Width);
	nbytes = (image.Width + 7) >> 3;
	gs_image_init(pen, &image, false, pgs);
	for ( n = image.Height; n > 0; dp += raster, --n )
	  gs_image_next(pen, dp, nbytes, &used);
	gs_image_cleanup(pen);
	gs_free_object(gs_state_memory(pgs), pen, "bitmap_PaintProc");
	return 0;
}

/* Set the color and pattern for drawing. */
int
pcl_set_drawing_color(pcl_state_t *pcls, pcl_pattern_type_t type,
  const pcl_id_t *pid)
{	gs_state *pgs = pcls->pgs;
	gs_pattern_instance **ppi;
	gs_pattern_instance *pi[4];	/* for user-defined pattern hack */
	gs_int_point resolution;
	gx_tile_bitmap tile;

	switch ( type )
	  {
	  case pcpt_solid_black:
	    gs_setgray(pgs, 0.0);
	    return 0;
	  case pcpt_solid_white:
	    gs_setgray(pgs, 1.0);
	    return 0;
	  case pcpt_shading:
	    { uint percent = id_value(*pid);
	      if ( percent == 0 )
		{ gs_setgray(pgs, 1.0);
		  return 0;
		}
#define else_if_percent(maxp, shade, index)\
  else if ( percent <= maxp )\
    tile.data = (const byte *)shade,\
    ppi = &pcls->cached_shading[index*4]
	      else_if_percent(2, shade_1_2, 0);
	      else_if_percent(10, shade_3_10, 1);
	      else_if_percent(20, shade_11_20, 2);
	      else_if_percent(35, shade_21_35, 3);
	      else_if_percent(55, shade_36_55, 4);
	      else_if_percent(80, shade_56_80, 5);
	      else_if_percent(99, shade_81_99, 6);
#undef else_if_percent
	      else
		{ gs_setgray(pgs, 0.0);
		  return 0;
		}
	      tile.raster = 8;
	      tile.size.x = 64;
	      tile.size.y = 16;
	      tile.rep_width = 8;
	      tile.rep_height = (percent <= 2 ? 16 : 8);
	    }
	    /*
	     * TRM 13-18 says that the default resolution for downloaded
	     * patterns is 300 dpi; apparently (by observation), the
	     * built-in patterns are also at 300 dpi regardless of the
	     * printer resolution.
	     */
	    resolution.x = resolution.y = 300;
	    break;
	  case pcpt_cross_hatch:
	    /*
	     * The pattern ID may be out of range: we can't check this at
	     * the time the pattern ID is set, so we have to check it here.
	     */
	    { int index = id_value(*pid) - 1;
	      if ( index < 0 || index >= 6 )
		{ /* Invalid index, just use black. */
		  gs_setgray(pgs, 0.0);
		  return 0;
		}
	      ppi = &pcls->cached_cross_hatch[index*4];
	      tile.data = (const byte *)cross_hatch_patterns[index];
	    }
	    tile.raster = 8;
	    tile.size.x = 64;
	    tile.size.y = 16;
	    tile.rep_width = 16;
	    tile.rep_height = 16;
	    /* See above re resolution. */
	    resolution.x = resolution.y = 300;
	    break;
	  case pcpt_user_defined:
	    { void *value;

	      if ( !pl_dict_find(&pcls->patterns, id_key(*pid), 2, &value) )
		{ /*
		   * No pattern with this ID exists.  We think the correct
		   * action is to default to black, but it's possible that
		   * if the pattern was current, it stays around until a
		   * reference count goes to zero.
		   */
		  gs_setgray(pgs, 0.0);
		  return 0;
		}
#define ppt ((pcl_pattern_t *)value)
	      tile.data =
		(byte *)value + pattern_data_offset;
	      tile.size.x =
		tile.rep_width =
		  (ppt->width[0] << 8) + ppt->width[1];
	      tile.size.y =
		tile.rep_height =
		  (ppt->height[0] << 8) + ppt->height[1];
	      tile.raster =
		bitmap_raster(tile.size.x);
	      resolution.x = pl_get_uint16(ppt->x_resolution);
	      resolution.y = pl_get_uint16(ppt->y_resolution);
#undef ppt
	      /*
	       ****** HACK: just create a new pattern each time.
	       ****** THIS IS UNACCEPTABLE, but will get us through the CET.
	       */
	      pi[0] = pi[1] = pi[2] = pi[3] = 0;
	      ppi = pi;
	    }
	    break;
	  /* case pcpt_current_pattern: */	/* caller handles this */
	  default:
	    return_error(gs_error_rangecheck);
	  }
	tile.id = gx_no_bitmap_id;
	/* 
	 * Create a bitmap pattern if one doesn't already exist.
	 * We should track changes in orientation, but we don't yet.
	 */
	{ gs_client_color ccolor;
	  int rotation = (pcls->orientation + pcls->print_direction) & 3;

	  ppi += rotation;
	  ccolor.pattern = *ppi;
	  if ( ccolor.pattern == 0 )
	    { /*
	       * Make a bitmap pattern at the specified resolution.
	       * Assume the CTM was set by pcl_set_ctm.
	       */
	      floatp scale_x = pcls->resolution.x / resolution.x;
	      floatp scale_y = pcls->resolution.y / resolution.y;
	      int code;

	      /* If the resolution ratios are almost integers, */
	      /* make them integers. */
#define adjust_scale(s)\
  if ( s - floor(s) < 0.001 || ceil(s) - s < 0.001 )\
    s = floor(s + 0.0015)
	      adjust_scale(scale_x);
	      adjust_scale(scale_y);
#undef adjust_scale
	      code = pcl_makebitmappattern(&ccolor, &tile, pgs, scale_x,
					   scale_y, rotation);

	      if ( code < 0 )
		return code;
	      *ppi = ccolor.pattern;
	    }
	  gs_setpattern(pgs, &ccolor);
	}
	return 0;
}

/* Initialization */
private int
pcdraw_do_init(gs_memory_t *mem)
{	return 0;
}
private void
pcdraw_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & pcl_reset_initial )
	  { int i;
	    for ( i = 0; i < countof(pcls->cached_shading); ++i )
	      pcls->cached_shading[i] = 0;
	    for ( i = 0; i < countof(pcls->cached_cross_hatch); ++i )
	      pcls->cached_cross_hatch[i] = 0;
	  }
}
const pcl_init_t pcdraw_init = {
  pcdraw_do_init, pcdraw_do_reset
};
