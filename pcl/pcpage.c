/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcpage.c */
/* PCL5 page control commands */
#include "std.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcdraw.h"
#include "pcparam.h"
#include "pcparse.h"		/* for pcl_execute_macro */
#include "pcfont.h"		/* for underline interface */
#include "gsmatrix.h"		/* for gsdevice.h */
#include "gscoord.h"
#include "gsdevice.h"
#include "gspaint.h"
#include "gxdevice.h"
#include "gdevbbox.h"

/* Define the default margin and text length values. */
#define left_margin_default 0
#define top_margin_default inch2coord(0.5)

/* Define the default HMI and VMI values. */
#define hmi_default inch2coord(12.0/120)
#define vmi_default inch2coord(8.0/48)

/* Procedures */

/* End a page, either unconditionally or only if there are marks on it. */
/* Return 1 if the page was actually printed and erased. */
int
pcl_end_page(pcl_state_t *pcls, pcl_print_condition_t condition)
{	
	pcl_break_underline(pcls);	/* (could mark page) */
	if ( condition != pcl_print_always )
	  { /* Check whether there are any marks on the page. */
	    gx_device *dev = gs_currentdevice(pcls->pgs);
	    if ( !pcls->have_page )	/* definitely no marks */
	      return 0;
	    /* Check whether we're working with a bbox device. */
	    if ( !strcmp(gs_devicename(dev), "bbox") )
	      { /* A bbox device can give us a definitive answer */
		/* (otherwise we have to be conservative). */
		gs_rect bbox;
		gx_device_bbox_bbox((gx_device_bbox *)dev, &bbox);
		if ( bbox.p.x >= bbox.q.x || bbox.p.y >= bbox.q.y )
		  return 0;
	      }
	  }
	  { int code;
	    /* If there's an overlay macro, execute it now. */
	    if ( pcls->overlay_enabled )
	      { void *value;
	        if ( pl_dict_find(&pcls->macros,
				  id_key(pcls->overlay_macro_id), 2,
				  &value)
		   )
		  { pcls->overlay_enabled = false; /**** IN reset_overlay ****/
		    code = pcl_execute_macro((const pcl_macro_t *)value, pcls,
					     pcl_copy_before_overlay,
					     pcl_reset_overlay,
					     pcl_copy_after_overlay);
		    pcls->overlay_enabled = true; /**** IN copy_after ****/
		  }
	      }
	    (*pcls->end_page)(pcls, pcls->num_copies, true);
	    pcl_continue_underline(pcls);
	    pcls->have_page = false;
	    code = gs_erasepage(pcls->pgs);
	    return (code < 0 ? code : 1);
	  }
}
int
pcl_default_end_page(pcl_state_t *pcls, int num_copies, int flush)
{	return gs_output_page(pcls->pgs, num_copies, flush);
}

/* Reset the margins. */
private void
reset_margins(pcl_state_t *pcls)
{	pcls->left_margin = left_margin_default;
	pcls->right_margin = pcls->rotated_page_width;
	pcls->top_margin =
	  (top_margin_default > pcls->rotated_page_height ? 0 :
	   top_margin_default);
}

/* Reset the text length to the default. */
private void
reset_text_length(pcl_state_t *pcls)
{	if ( pcls->vmi != 0 )
	  { int len = (int)((pcls->rotated_page_height - pcls->top_margin -
			     inch2coord(0.5)) / pcls->vmi);
	    /* We suppose that the minimum text length is 1.... */
	    pcls->text_length = max(len, 1);
	  }
}

/* Commands */

/* Define the known paper sizes. */
/* The values are taken from the H-P manual and are in 1/300" units, */
/* but the structure values are in centipoints (1/7200"). */
#define p_size(w, h, offp, offl)\
  { (w)*24L, (h)*24L, {(offp)*24L, 0}, {(offl)*24L, 0} }
private const pcl_paper_size_t
  p_size_executive = p_size(2175, 3150, 75, 60),
  p_size_letter = p_size(2550, 3300, 75, 60),
  p_size_legal = p_size(2550, 4200, 75, 60),
  p_size_ledger = p_size(3300, 5100, 75, 60),
  p_size_a4 = p_size(2480, 3507, 71, 59),
  p_size_a3 = p_size(3507, 4960, 71, 59),
  p_size_monarch = p_size(1162, 2250, 75, 60),
  p_size_com_10 = p_size(1237, 2850, 75, 60),
  p_size_dl = p_size(1299, 2598, 71, 59),
  p_size_c5 = p_size(1913, 2704, 71, 59),
  p_size_b5 = p_size(2078, 2952, 71, 59);

private int /* ESC & l <psize_enum> A */
pcl_page_size(pcl_args_t *pargs, pcl_state_t *pcls)
{	/* Note: not all values are implemented on all printers. */
	int i = int_arg(pargs);
	const pcl_paper_size_t *psize;

	switch ( i )
	  {
	  case   1: psize = &p_size_executive; break;
	  case   2: psize = &p_size_letter; break;
	  case   3: psize = &p_size_legal; break;
	  case   6: psize = &p_size_ledger; break;
	  case  26: psize = &p_size_a4; break;
	  case  27: psize = &p_size_a3; break;
	  case  80: psize = &p_size_monarch; break;
	  case  81: psize = &p_size_com_10; break;
	  case  90: psize = &p_size_dl; break;
	  case  91: psize = &p_size_c5; break;
	  case 100: psize = &p_size_b5; break;
	  default:
	    return 0;
	  }
	{ int code = pcl_end_page_if_marked(pcls);
	  if ( code < 0 )
	    return code;
	}
	pcls->paper_size = psize;
	pcl_compute_logical_page_size(pcls);
	/* Paper sizes are in centipoints. */
	gx_device_set_media_size(gs_currentdevice(pcls->pgs),
				 psize->width * 0.01,
				 psize->height * 0.01);
	reset_margins(pcls);
	reset_text_length(pcls);
	pcl_home_cursor(pcls);
	pcls->overlay_enabled = false;
	return 0;
}

private int /* ESC & l <feed_enum> H */
pcl_paper_source(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint i = uint_arg(pargs);
	/* Note: not all printers support all possible values. */
	if ( i > 6 )
	  return e_Range;
	{ int code = pcl_end_page_if_marked(pcls);
	  if ( code < 0 )
	    return code;
	  if ( i > 0 )
	    { code = put_param1_int(pcls, "%MediaSource", i);
	      if ( code < 0 )
		return code;
	    }
	}
	pcl_home_cursor(pcls);
	return 0;
}

private int /* ESC & l <orient> O */
pcl_page_orientation(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint i = uint_arg(pargs);
	if ( i > 3 )
	  return 0;

	/*
	 * In apparent contradiction to TRM 5-6, setting the orientation to
	 * the same value is a no-op.
	 */
	if ( i == pcls->orientation )
	  return 0;
	{ int code = pcl_end_page_if_marked(pcls);
	  if ( code < 0 )
	    return code;
	}
	pcls->orientation = i;
	reset_margins(pcls);
	pcls->hmi_set = hmi_not_set;
	pcls->vmi = vmi_default;
	pcl_home_cursor(pcls);
	pcls->overlay_enabled = false;
	pcl_compute_logical_page_size(pcls);
	reset_text_length(pcls);
	return 0;
}

private int /* ESC & a <angle> P */
pcl_print_direction(pcl_args_t *pargs, pcl_state_t *pcls)
{	int i = int_arg(pargs);
	switch ( i )
	  {
	  case 0: case 90: case 180: case 270:
	    break;
	  default:
	    return 0;
	  }
	return pcl_set_print_direction(pcls, i / 90);
}

private int /* ESC & a <col> L */
pcl_left_margin(pcl_args_t *pargs, pcl_state_t *pcls)
{	coord lmarg = int_arg(pargs) * pcl_hmi(pcls);

	if ( lmarg < pcls->right_margin )
	  { pcls->left_margin = lmarg;
	    pcl_clamp_cursor_x(pcls, true);
	  }
	return 0;
}

private int /* ESC & a <col> M */
pcl_right_margin(pcl_args_t *pargs, pcl_state_t *pcls)
{	/*
	 * The right margin is set to the *right* edge of the specified
	 * column, so we need to add 1 to the column number.
	 */
	coord rmarg = (int_arg(pargs) + 1) * pcl_hmi(pcls);

	if ( rmarg > pcls->rotated_page_width )
	  rmarg = pcls->rotated_page_width;
	if ( rmarg > pcls->left_margin )
	  { pcls->right_margin = rmarg;
	    pcl_clamp_cursor_x(pcls, true);
	  }
	return 0;
}

private int /* ESC 9 */
pcl_clear_horizontal_margins(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->left_margin = 0;
	pcls->right_margin = pcls->rotated_page_width;
	pcl_clamp_cursor_x(pcls, true);
	return 0;
}

private int /* ESC & l <line> E */
pcl_top_margin(pcl_args_t *pargs, pcl_state_t *pcls)
{	coord tmarg = int_arg(pargs) * pcls->vmi;
	if ( pcls->vmi == 0 || tmarg > pcls->rotated_page_height )
	  return 0;
	pcls->top_margin = tmarg;
	reset_text_length(pcls);
	/* The manual doesn't call for clamping the cursor here.... */
	return 0;
}

private int /* ESC & l <lines> F */
pcl_text_length(pcl_args_t *pargs, pcl_state_t *pcls)
{	int len = int_arg(pargs);
	if ( pcls->vmi == 0 )
	  return 0;
	if ( pcls->top_margin + len * pcls->vmi <= pcls->rotated_page_height )
	  pcls->text_length = len;
	return 0;
}

private int /* ESC & l <enable> L */
pcl_perforation_skip(pcl_args_t *pargs, pcl_state_t *pcls)
{	bool new_skip;

	switch ( uint_arg(pargs) )
	{
	case 0 /* disable */: new_skip = false; break;
	case 1 /* enable */: new_skip = true; break;
	default: return 0;
	}
	/* PCLTRM 5-21 - changing skip mode restores top margin, page
           length and text length and margins to their defaults */
	if ( new_skip != pcls->perforation_skip )
	  {
	    pcls->top_margin = 
	      (top_margin_default > pcls->rotated_page_height ? 0 : 
	       top_margin_default);
	    reset_text_length(pcls);
	    pcls->perforation_skip = new_skip;
	  }
	return 0;
}

private int /* ESC & k <x> H */
pcl_horiz_motion_index(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcl_set_hmi(pcls,
		    (coord)(float_arg(pargs) * (pcl_coord_scale / 120.0)),
		    true);
	return 0;
}

private int /* ESC & l <y> C */
pcl_vert_motion_index(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->vmi = float_arg(pargs) * (pcl_coord_scale / 48.0);
	return 0;
}

private int /* ESC & l <lpi> D */
pcl_line_spacing(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint lpi = uint_arg(pargs);
	if ( lpi != 0 && (48 % lpi) != 0 )
	  return 0;
	pcls->vmi = (lpi == 0 ? 0 : pcl_coord_scale / lpi);
	return 0;
}

/* (From PCL5 Comparison Guide, p. 1-98) */
private int /* ESC & l <type> M */
pcl_media_type(pcl_args_t *pargs, pcl_state_t *pcls)
{	int type = int_arg(pargs);
	if ( type > 4 )
	  return e_Range;
	{ int code = pcl_end_page_if_marked(pcls);
	  if ( code < 0 )
	    return code;
	}
	return e_Unimplemented;
}

/* (From PCL5 Comparison Guide, p. 1-99) */
private int /* ESC * o <quality> Q */
pcl_print_quality(pcl_args_t *pargs, pcl_state_t *pcls)
{	int quality = int_arg(pargs);
	if ( quality < -1 || quality > 1 )
	  return e_Range;
	{ int code = pcl_end_page_if_marked(pcls);
	  if ( code < 0 )
	    return code;
	}
	pcl_home_cursor(pcls);
	/* We don't do anything about this.... */
	return 0;
}

/* Initialization */
private int
pcpage_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('&')
	  {'l', 'A',
	     PCL_COMMAND("Page Size", pcl_page_size,
			 pca_neg_ignore|pca_big_ignore)},
	  {'l', 'H',
	     PCL_COMMAND("Paper Source", pcl_paper_source,
			 pca_neg_error|pca_big_error)},
	  {'l', 'O',
	     PCL_COMMAND("Page Orientation", pcl_page_orientation,
			 pca_neg_ignore|pca_big_ignore)},
	  {'a', 'P',
	     PCL_COMMAND("Print Direction", pcl_print_direction,
			 pca_neg_ignore|pca_big_ignore)},
	  {'a', 'L',
	     PCL_COMMAND("Left Margin", pcl_left_margin,
			 pca_neg_error|pca_big_error)},
	  {'a', 'M',
	     PCL_COMMAND("Right Margin", pcl_right_margin,
			 pca_neg_error|pca_big_error)},
	END_CLASS
	DEFINE_ESCAPE('9', "Clear Horizontal Margins",
		      pcl_clear_horizontal_margins)
	DEFINE_CLASS('&')
	  {'l', 'E',
	     PCL_COMMAND("Top Margin", pcl_top_margin,
			 pca_neg_ignore|pca_big_error)},
	  {'l', 'F',
	     PCL_COMMAND("Text Length", pcl_text_length,
			 pca_neg_ignore|pca_big_error)},
	  {'l', 'L',
	     PCL_COMMAND("Perforation Skip", pcl_perforation_skip,
			 pca_neg_error|pca_big_error)},
	  {'k', 'H',
	     PCL_COMMAND("Horizontal Motion Index", pcl_horiz_motion_index,
			 pca_neg_error|pca_big_error)},
	  {'l', 'C',
	     PCL_COMMAND("Vertical Motion Index", pcl_vert_motion_index,
			 pca_neg_error|pca_big_error)},
	  {'l', 'D',
	     PCL_COMMAND("Line Spacing", pcl_line_spacing,
			 pca_neg_ignore|pca_big_ignore)},
	  {'l', 'M',
	     PCL_COMMAND("Media Type", pcl_media_type,
			 pca_neg_error|pca_big_ignore)},
	END_CLASS
	DEFINE_CLASS_COMMAND_ARGS('*', 'o', 'Q', "Print Quality",
				  pcl_print_quality, pca_neg_ok|pca_big_error)
	return 0;
}
private void
pcpage_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { if ( type & pcl_reset_initial )
	      pcls->end_page = pcl_default_end_page;
	    pcls->paper_size = &p_size_letter;
	    pcls->manual_feed = false;
	    pcls->paper_source = 0;		/* ??? */
	    pcls->print_direction = 0;
	    pcls->orientation = 0;
	    reset_margins(pcls);
	    pcls->perforation_skip = true;
	    pcls->hmi_set = hmi_not_set;
	    pcls->vmi = vmi_default;
	    pcl_compute_logical_page_size(pcls);
	    reset_text_length(pcls);
	    pcls->have_page = false;
	  }
}
const pcl_init_t pcpage_init = {
  pcpage_do_init, pcpage_do_reset
};
