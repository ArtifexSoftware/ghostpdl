/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcdraw.h */
/* PCL5 drawing utilities */
#include "std.h"
#include "gstypes.h"		/* for gsstate.h */
#include "gsmatrix.h"		/* for gsstate.h */
#include "gsmemory.h"		/* for gsstate.h */
#include "gsstate.h"
#include "gscoord.h"
#include "gsdcolor.h"
#include "gxfixed.h"
#include "gxpath.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcfont.h"		/* for underline break/continue */
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
}

/* Set the CTM from the current left and top offset, orientation, */
/* and (optionally) print direction. */
/* We export this only for HP-GL/2. */
int
pcl_set_ctm(pcl_state_t *pcls, bool print_direction)
{	gs_state *pgs = pcls->pgs;
	const pcl_paper_size_t *psize = pcls->paper_size;

	gs_initmatrix(pgs);
	/**** DOESN'T HANDLE NEGATION FOR DUPLEX BINDING ****/
	switch ( pcls->orientation )
	  {
	  case 0:
	    gs_translate(pgs, pcls->left_offset_cp, pcls->top_offset_cp);
	    break;
	  case 1:
	    gs_rotate(pgs, 90.0);
	    gs_translate(pgs, -(psize->height + pcls->top_offset_cp),
			 pcls->left_offset_cp);
	  case 2:
	    gs_rotate(pgs, 180.0);
	    gs_translate(pgs, -(psize->width + pcls->left_offset_cp),
			 -(psize->height + pcls->top_offset_cp));
	    break;
	  case 3:
	    gs_rotate(pgs, 270.0);
	    gs_translate(pgs, pcls->top_offset_cp,
			 -(psize->width + pcls->left_offset_cp));
	    break;
	  }
	if ( print_direction && pcls->print_direction )
	  gs_rotate(pgs, (float)(pcls->print_direction * 90));
	return 0;
}

/* Set the clipping region to the PCL printable area. */
private int
pcl_set_clip(pcl_state_t *pcls)
{	const pcl_paper_size_t *psize = pcls->paper_size;
	gs_fixed_rect box;

	/* Per Figure 2-4 on page 2-9 of the PCL5 TRM, the printable area */
	/* is always just 1/6" indented from the physical page. */
	box.p.x = float2fixed(pcls->resolution.x / 6.0);
	box.p.y = float2fixed(pcls->resolution.y / 6.0);
	box.q.x = float2fixed(psize->width / 7200.0 * pcls->resolution.x)
	  - box.p.x;
	box.q.y = float2fixed(psize->height / 7200.0 * pcls->resolution.y)
	  - box.p.y;
	return gx_clip_to_rectangle(pcls->pgs, &box);
}

/* Set all necessary graphics state parameters for PCL drawing */
/* (currently only CTM and clipping region). */
int
pcl_set_graphics_state(pcl_state_t *pcls, bool print_direction)
{	int code = pcl_set_ctm(pcls, print_direction);
	return (code < 0 ? code : pcl_set_clip(pcls));
}

/* ------ Cursor ------ */

/* Home the cursor to the left edge of the logical page at the top margin. */
void
pcl_home_cursor(pcl_state_t *pcls)
{	
	pcl_break_underline(pcls);
	pcls->cap.x = 0;
	/* See the description of the Top Margin command in Chapter 5 */
	/* of the PCL5 TRM for the following computation. */
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
	pcls->hmi_set = explicit;
	/* See the description of the Units of Measure command */
	/* in the PCL5 documentation for an explanation of the following. */
	pcls->hmi =
	  (explicit ? hmi :
	   (hmi + (pcls->uom_cp >> 1)) / pcls->uom_cp * pcls->uom_cp);
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
private const uint32
  ch_horizontal[] = {
    p8(0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00),
    p8(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)
  },
  ch_vertical[] = {
    p2x8(0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80)
  },
  ch_45_degree[] = {
    r2(0x00, 0xc0), r2(0x01, 0x80), r2(0x03, 0x00), r2(0x06, 0x00),
    r2(0x0c, 0x00), r2(0x18, 0x00), r2(0x30, 0x00), r2(0x60, 0x00),
    r2(0xc0, 0x00), r2(0x80, 0x01), r2(0x00, 0x03), r2(0x00, 0x06),
    r2(0x00, 0x0c), r2(0x00, 0x18), r2(0x00, 0x30), r2(0x00, 0x60)
  },
  ch_135_degree[] = {
    r2(0x80, 0x01), r2(0xc0, 0x00), r2(0x60, 0x00), r2(0x30, 0x00),
    r2(0x18, 0x00), r2(0x0c, 0x00), r2(0x06, 0x00), r2(0x03, 0x00),
    r2(0x01, 0x80), r2(0x00, 0xc0), r2(0x00, 0x60), r2(0x00, 0x30),
    r2(0x00, 0x18), r2(0x00, 0x0c), r2(0x00, 0x06), r2(0x00, 0x03)
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

/* Set the color and pattern for drawing. */
int
pcl_set_drawing_color(pcl_state_t *pcls, pcl_pattern_type_t type,
  const pcl_id_t *pid)
{	gs_state *pgs = pcls->pgs;
	/* We construct a device halftone by hand here: */
	gx_device_color dev_color;
	gx_ht_tile tile;

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
#define else_if_percent(maxp, shade)\
  else if ( percent <= maxp ) tile.tiles.data = (const byte *)shade
	      else_if_percent(2, shade_1_2);
	      else_if_percent(10, shade_3_10);
	      else_if_percent(20, shade_11_20);
	      else_if_percent(35, shade_21_35);
	      else_if_percent(55, shade_36_55);
	      else_if_percent(80, shade_56_80);
	      else_if_percent(99, shade_81_99);
#undef else_if_percent
	      else
		{ gs_setgray(pgs, 0.0);
		  return 0;
		}
	      tile.tiles.raster = 8;
	      tile.tiles.size.x = 64;
	      tile.tiles.size.y = 16;
	      tile.tiles.rep_width = 8;
	      tile.tiles.rep_height = (percent <= 2 ? 16 : 8);
	    }
	    break;
	  case pcpt_cross_hatch:
	    tile.tiles.data =
	      (const byte *)cross_hatch_patterns[id_value(*pid)];
	    tile.tiles.raster = 8;
	    tile.tiles.size.x = 64;
	    tile.tiles.size.y = 16;
	    tile.tiles.rep_width = 16;
	    tile.tiles.rep_height = 16;
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
	      tile.tiles.data = (byte *)value + pattern_data_offset;
	      tile.tiles.size.x = tile.tiles.rep_width =
		(ppt->width[0] << 8) + ppt->width[1];
	      tile.tiles.size.y = tile.tiles.rep_height =
		(ppt->height[0] << 8) + ppt->height[1];
	      tile.tiles.raster = bitmap_raster(tile.tiles.size.x);
#undef ppt
	    }
	    /**** HANDLE ROTATION & SCALING ****/
	    break;
	  /* case pcpt_current_pattern: */	/* caller handles this */
	  default:
	    return_error(gs_error_rangecheck);
	  }
	tile.tiles.id = gx_no_bitmap_id;
	tile.tiles.rep_shift = tile.tiles.shift = 0;
	color_set_binary_tile(&dev_color, 0, 1, &tile);
	return 0;
}
