/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcrect.c */
/* PCL5 rectangular area fill commands */
#include "math_.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcdraw.h"
#include "gspath.h"
#include "gspath2.h"
#include "gspaint.h"
#include "gsrop.h"

private int /* ESC * c <dp> H */
pcl_horiz_rect_size_decipoints(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->rectangle.x = float_arg(pargs) * 10;
	return 0;
}

private int /* ESC * c <units> A */
pcl_horiz_rect_size_units(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->rectangle.x = int_arg(pargs) * pcls->uom_cp;
	return 0;
}

private int /* ESC * c <dp> V */
pcl_vert_rect_size_decipoints(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->rectangle.y = float_arg(pargs) * 10;
	return 0;
}

private int /* ESC * c B */
pcl_vert_rect_size_units(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->rectangle.y = int_arg(pargs) * pcls->uom_cp;
	return 0;
}

/* ESC * c G */ /* defined in pcprint.c */

private int /* ESC * c <fill_enum> P */
pcl_fill_rect_area(pcl_args_t *pargs, pcl_state_t *pcls)
{	gs_state *pgs = pcls->pgs;
	int type = int_arg(pargs);
	int code =
	  (type == pcpt_current_pattern ?
	   pcl_set_drawing_color(pcls, pcls->pattern_type,
				 &pcls->current_pattern_id) :
	   pcl_set_drawing_color(pcls, type, &pcls->pattern_id));

	if ( code < 0 )
	  return code;
	code = pcl_set_graphics_state(pcls, true);
	if ( code < 0 )
	  return code;
	{ gs_rect r;

	  r.p.x = pcls->cap.x;
	  r.p.y = pcls->cap.y;
	  r.q.x = r.p.x + pcls->rectangle.x;
	  r.q.y = r.p.y + pcls->rectangle.y;
	  if ( type == pcpt_solid_white && gs_currenttexturetransparent(pgs) )
	    { /* Temporarily disable pattern transparency. */
	      /* See p. 13-8 in the PCL5 TRM. */
	      gs_settexturetransparent(pgs, false);
	      code = gs_rectfill(pgs, &r, 1);
	      gs_settexturetransparent(pgs, true);
	    }
	  else
	    code = gs_rectfill(pgs, &r, 1);
	}
	pcls->have_page = true;
	return 1;
}

/* Initialization */
private int
pcrect_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('*')
	  {'c', 'H',
	     PCL_COMMAND("Horizontal Rectangle Size Decipoints",
			 pcl_horiz_rect_size_decipoints,
			 pca_neg_error|pca_big_error)},
	  {'c', 'A',
	     PCL_COMMAND("Horizontal Rectangle Size Units",
			 pcl_horiz_rect_size_units,
			 pca_neg_error|pca_big_error)},
	  {'c', 'V',
	     PCL_COMMAND("Vertical Rectangle Size Decipoint",
			 pcl_vert_rect_size_decipoints,
			 pca_neg_error|pca_big_error)},
	  {'c', 'B',
	     PCL_COMMAND("Vertical Rectangle Size Units",
			 pcl_vert_rect_size_units,
			 pca_neg_error|pca_big_error)},
	  {'c', 'P',
	     PCL_COMMAND("Fill Rectangular Area", pcl_fill_rect_area,
			 pca_neg_ignore|pca_big_ignore)},
	END_CLASS
	return 0;
}
private void
pcrect_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { pcls->rectangle.x = 0;
	    pcls->rectangle.y = 0;
	  }
}
const pcl_init_t pcrect_init = {
  pcrect_do_init, pcrect_do_reset
};
