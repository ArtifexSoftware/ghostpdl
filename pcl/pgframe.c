/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgframe.c */
/* PCL5/HP-GL/2 picture frame commands */
#include "math_.h"
#include "pgmand.h"
#include "pgdraw.h"
#include "pgmisc.h"
#include "gstypes.h"		/* for gsstate.h */
#include "gsmatrix.h"		/* for gsstate.h */
#include "gsmemory.h"		/* for gsstate.h */
#include "gsstate.h"            
#include "pcdraw.h"

/* Import the RTL implementation of ESC % # A. */
extern pcl_command_proc(rtl_enter_pcl_mode);

/* Even though these are PCL commands, */
/* they are only relevant to HPGL. */

int /* ESC * c <w_dp> X */ 
pcl_horiz_pic_frame_size_decipoints(pcl_args_t *pargs, pcl_state_t *pcls)
{
	float size = float_arg(pargs) * 10.0; /* --> centipoints */
	
	if ( (coord)size == 0 )
	  pcls->g.picture_frame_width = pcls->logical_page_width;
	else
	  pcls->g.picture_frame_width = (coord)size;

	{
	  hpgl_args_t args;
	  hpgl_args_setup(&args);
	  hpgl_IP(&args, pcls);
	
	  hpgl_args_setup(&args);
	  hpgl_IW(&args, pcls);
	
	  hpgl_args_set_int(&args,0);
	  hpgl_PM(&args, pcls);

	  hpgl_args_set_int(&args,2);
	  hpgl_PM(&args, pcls);
	}
	return 0;
}

int /* ESC * c <h_dp> Y */ 
pcl_vert_pic_frame_size_decipoints(pcl_args_t *pargs, pcl_state_t *pcls)
{	
	float size = float_arg(pargs) * 10.0; /* --> centipoints */
	
	/* default to pcl logical page */
	if ( size == 0.0 )
	  pcls->g.picture_frame_height = pcls->text_length;
	else
	  pcls->g.picture_frame_height = (coord)size;

	{
	  hpgl_args_t args;
	  hpgl_args_setup(&args);
	  hpgl_IP(&args, pcls);
	
	  hpgl_args_setup(&args);
	  hpgl_IW(&args, pcls);
	
	  hpgl_args_set_int(&args,0);
	  hpgl_PM(&args, pcls);

	  hpgl_args_set_int(&args,2);
	  hpgl_PM(&args, pcls);
	  /* HAS update the pen's position */
	}
	return 0;
}

int /* ESC * c 0 T */ 
pcl_set_pic_frame_anchor_point(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint i = uint_arg(pargs);
	if ( i != 0 )
	  return 0;

	/* The anchor point does not take print direction into account. */
	{ int dir = pcls->print_direction;
	  pcl_set_print_direction(pcls, 0);
	  pcls->g.picture_frame.anchor_point.x = pcls->cap.x;
	  pcls->g.picture_frame.anchor_point.y = pcls->cap.y;
	  pcl_set_print_direction(pcls, dir);
	}

	{
	  hpgl_args_t args;
	  hpgl_args_setup(&args);
	  hpgl_IP(&args, pcls);
	
	  hpgl_args_setup(&args);
	  hpgl_IW(&args, pcls);
	
	  hpgl_args_set_int(&args,0);
	  hpgl_PM(&args, pcls);

	  hpgl_args_set_int(&args,2);
	  hpgl_PM(&args, pcls);
	  /* HAS update the pen's position */
	}
	return 0;
}

int /* ESC * c <w_in> K */ 
pcl_hpgl_plot_horiz_size(pcl_args_t *pargs, pcl_state_t *pcls)
{	
	/* convert to centipoints as to match the picture frame */
  	float size = float_arg(pargs) * 7200.0;
	
	if ( (coord)size == 0 )
	  pcls->g.plot_width = pcls->g.picture_frame_width;
	else
	  pcls->g.plot_width = (coord)size;
	return 0;
}

int /* ESC * c <h_in> L */ 
pcl_hpgl_plot_vert_size(pcl_args_t *pargs, pcl_state_t *pcls)
{	
	/* convert to centipoints as to match the picture frame */
  	float size = float_arg(pargs) * 7200.0;
	if ( (coord)size == 0 )
	  pcls->g.plot_height = pcls->g.picture_frame_height;
	else
	  pcls->g.plot_height = (coord)size;
	return 0;
}

/* We redefine this command so we can draw the current GL path. */
private int /* ESC % <enum> A */ 
pcl_enter_pcl_mode(pcl_args_t *pargs, pcl_state_t *pcls)
{	
	hpgl_call(hpgl_draw_current_path(pcls, hpgl_rm_vector));
	return rtl_enter_pcl_mode(pargs, pcls);
}

/* Initialization */
private int
pgframe_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('*')
	  {'c', 'X',
	    PCL_COMMAND("Horizontal Picture Frame Size Decipoints",
			pcl_horiz_pic_frame_size_decipoints,
			pca_neg_error|pca_big_error)},
	  {'c', 'Y',
	    PCL_COMMAND("Vertical Picture Frame Size Decipoints",
			pcl_vert_pic_frame_size_decipoints,
			pca_neg_error|pca_big_error)},
	  {'c', 'T',
	    PCL_COMMAND("Set Picture Frame Anchor Point",
			pcl_set_pic_frame_anchor_point,
			pca_neg_error|pca_big_error)},
	  {'c', 'K',
	    PCL_COMMAND("HP-GL/2 Plot Horizontal Size",
			pcl_hpgl_plot_horiz_size,
			pca_neg_error|pca_big_error)},
	  {'c', 'L',
	    PCL_COMMAND("HP-GL/2 Plot Vertical Size",
			pcl_hpgl_plot_vert_size,
			pca_neg_error|pca_big_error)},
	END_CLASS
	DEFINE_CLASS('%')
	  {0, 'A',
	    PCL_COMMAND("Enter PCL Mode",
			pcl_enter_pcl_mode,
			pca_neg_ok|pca_big_ok|pca_in_macro)},
	END_CLASS
	return 0;
}
const pcl_init_t pgframe_init = {
  pgframe_do_init, 0
};
