/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pginit.c */
/* Initialization and resetting for HP-GL/2. */
#include "std.h"
#include "pgmand.h"
#include "pginit.h"
#include "pgdraw.h"

/* ------ Internal procedures ------ */

/* Reset a set of font parameters to their default values. */
void
hpgl_default_font_params(pcl_font_selection_t *pfs)
{	pfs->params.symbol_set = 277;	/* Roman-8 */
	pfs->params.proportional_spacing = false;
	pfs->params.pitch_100ths = 9*100;
	pfs->params.height_4ths = (int)(11.5*4);
	pfs->params.style = 0;
	pfs->params.stroke_weight = 0;
	pfs->params.typeface_family = 48;	/* stick font */
	pfs->font = 0;		/* not looked up yet */
}

/* Reset a pen to its default color. */
void
hpgl_default_pen_color(hpgl_state_t *pgls, int pen)
{	static const byte default_colors[8][3] = {
	  {1,1,1} /*white*/, {0,0,0} /*black*/,
	  {1,0,0} /*red*/, {0,1,0} /*green*/,
	  {1,1,0} /*yellow*/, {0,0,1} /*blue*/,
	  {1,0,1} /*magenta*/, {0,1,1} /*cyan*/
	};
	int c;

	for ( c = 0; c < 3; ++c )
	  { pgls->g.pen_color[pen].rgb[c] =
	      (default_colors[pen][c] ? pgls->g.color_range[c].cmin :
	       pgls->g.color_range[c].cmax);
	  }
}

/* Reset a fill pattern to solid fill.  The index is 1-origin, */
/* as for the RF command.  free = true means free the old pattern if any. */
void
hpgl_default_fill_pattern(hpgl_state_t *pgls, int index, bool free)
{	int index0 = index - 1;
	if ( free )
	  gs_free_object(pgls->memory, pgls->g.fill_pattern[index0].data,
			 "fill pattern(reset)");
	pgls->g.fill_pattern[index0].data = 0;
}

void
hpgl_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{		/* pgframe.c (Chapter 18) */
	/* HAS we expect the pcl logical page size to be calculated in
           advance of an hpgl reset.  Alot of repetition here that can
           be done away with */
	hpgl_args_t hpgl_args;
	pcl_args_t pcl_args;

	if ( type & (pcl_reset_initial | pcl_reset_printer | pcl_reset_cold ))
	  {
	    /* HAS should not have a path now ??? */
	    hpgl_init_path(pcls);

	    arg_set_float(&pcl_args, 0, 0);
	    pcl_horiz_pic_frame_size_decipoints(&pcl_args, pcls);

	    arg_set_float(&pcl_args, 0, 0);
	    pcl_vert_pic_frame_size_decipoints(&pcl_args, pcls);

	    arg_set_uint(&pcl_args, 0);
	    pcl_set_pic_frame_anchor_point(&pcl_args, pcls);

	    arg_set_float(&pcl_args, 0, 0);
	    pcl_hpgl_plot_horiz_size(&pcl_args, pcls);

	    arg_set_float(&pcl_args, 0, 0);
	    pcl_hpgl_plot_vert_size(&pcl_args, pcls);
	    
	    hpgl_args_setup(&hpgl_args);
	    hpgl_IN(&hpgl_args, pcls);

	    /* HAS reset pcl's logical page orientation should be done
               in PCL */
	  }   
	    
	if ( type & (pcl_reset_page_params) )
	  {
	    arg_set_float(&pcl_args, 0, 0);
	    pcl_horiz_pic_frame_size_decipoints(&pcl_args, pcls);

	    arg_set_float(&pcl_args, 0, 0);
	    pcl_vert_pic_frame_size_decipoints(&pcl_args, pcls);

	    arg_set_uint(&pcl_args, 0);
	    pcl_set_pic_frame_anchor_point(&pcl_args, pcls);

	    arg_set_float(&pcl_args, 0, 0);
	    pcl_hpgl_plot_horiz_size(&pcl_args, pcls);

	    arg_set_float(&pcl_args, 0, 0);
	    pcl_hpgl_plot_vert_size(&pcl_args, pcls);

	    hpgl_args_setup(&hpgl_args);
	    hpgl_IW(&hpgl_args, pcls);
	    
	    hpgl_args_set_int(&hpgl_args,0);
	    hpgl_PM(&hpgl_args, pcls);

	    hpgl_args_set_int(&hpgl_args,2);
	    hpgl_PM(&hpgl_args, pcls);

	    /* update the cursor position ?? */
	  }
	
	if ( type & (pcl_reset_picture_frame) )
	  {
	    hpgl_args_setup(&hpgl_args);
	    hpgl_IP(&hpgl_args, pcls);
	    
	    hpgl_args_setup(&hpgl_args);
	    hpgl_IW(&hpgl_args, pcls);

	    hpgl_args_set_int(&hpgl_args,0);
	    hpgl_PM(&hpgl_args, pcls);

	    hpgl_args_set_int(&hpgl_args,2);
	    hpgl_PM(&hpgl_args, pcls);

	    /* HAS pen to P1 */
	  }

	if (type & (pcl_reset_plot_size))
	  {
	    /* HAS reset picture frame scaling factors */
	  }
}

/* ------ Copy the HP-GL/2 state for macro call/overlay/exit. */

private int
hpgl_do_copy(pcl_state_t *psaved, const pcl_state_t *pcls,
  pcl_copy_operation_t operation)
{	if ( operation & pcl_copy_after )
	  { /* Don't restore the polygon buffer. */
	    /* (Copy from pcls to psaved.) */
	  }
	return 0;
}

/* Initialization */
const pcl_init_t pginit_init = {
  0, hpgl_do_reset, hpgl_do_copy
};
