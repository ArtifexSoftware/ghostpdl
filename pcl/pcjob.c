/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcjob.c */
/* PCL5 job control commands */
#include "std.h"
#include "gsmemory.h"
#include "gstypes.h"		/* for gsdevice.h */
#include "gsmatrix.h"		/* for gsdevice.h */
#include "gsdevice.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcparam.h"
#include "pcdraw.h"

/* Commands */

private int /* ESC E */
pcl_printer_reset(pcl_args_t *pargs, pcl_state_t *pcls)
{	if ( pcls->macro_level )
	  return e_Range;	/* not allowed inside macro */
	/* Print any partial page. */
	{ int code = pcl_end_page_if_marked(pcls);
	  if ( code < 0 )
	    return code;
	}
	/* exit hpgl/2 mode if necessary */
	{ pcl_args_t args;
	  arg_set_uint(&args, 0);
	  rtl_enter_pcl_mode(pargs, pcls);
	}
	/* Reset to user default state. */
	return pcl_do_resets(pcls, pcl_reset_printer);
}

private int /* ESC % -12345 X */
pcl_exit_language(pcl_args_t *pargs, pcl_state_t *pcls)
{	if ( int_arg(pargs) != -12345 )
	   return e_Range;
	{ int code = pcl_printer_reset(pargs, pcls);
	  return (code < 0 ? code : e_ExitLanguage);
	}
}

private int /* ESC & l <num_copies> X */
pcl_number_of_copies(pcl_args_t *pargs, pcl_state_t *pcls)
{	int i = int_arg(pargs);
	if ( i < 1 )
	  return 0;
	pcls->num_copies = i;
	return put_param1_int(pcls, "NumCopies", i);
}

private int /* ESC & l <sd_enum> S */
pcl_simplex_duplex_print(pcl_args_t *pargs, pcl_state_t *pcls)
{	int code;
	bool reopen = false;

	switch ( int_arg(pargs) )
	  {
	  case 0:
	    pcls->duplex = false;
	    break;
	  case 1:
	    pcls->duplex = true;
	    pcls->bind_short_edge = false;
	    break;
	  case 2:
	    pcls->duplex = true;
	    pcls->bind_short_edge = false;
	    break;
	  default:
	    return 0;
	  }
	code = put_param1_bool(pcls, "Duplex", pcls->duplex);
	switch ( code )
	  {
	  case 1:		/* reopen device */
	    reopen = true;
	  case 0:
	    break;
	  case gs_error_undefined:
	    return 0;
	  default:		/* error */
	    if ( code < 0 )
	      return code;
	  }
	code = put_param1_bool(pcls, "BindShortEdge", pcls->bind_short_edge);
	switch ( code )
	  {
	  case 1:		/* reopen device */
	    reopen = true;
	  case 0:
	  case gs_error_undefined:
	    break;
	  default:		/* error */
	    if ( code < 0 )
	      return code;
	  }
	return (reopen ? gs_setdevice_no_erase(pcls->pgs,
					       gs_currentdevice(pcls->pgs)) :
		0);
}

private int /* ESC & l <xoff_dp> U */
pcl_left_offset_registration(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->left_offset_cp = float_arg(pargs) * 10; /* decipoints => cp */
	pcl_compute_logical_page_size(pcls);
	return 0;
}

private int /* ESC & l <yoff_dp> Z */
pcl_top_offset_registration(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->top_offset_cp = float_arg(pargs) * 10; /* decipoints => cp */
	pcl_compute_logical_page_size(pcls);
	return 0;
}

private int /* ESC & a <side_enum> G */
pcl_duplex_page_side_select(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint i = uint_arg(pargs);
	int code;

	if ( i > 2 )
	  return 0;
	/* According to H-P documentation, this command ejects the page */
	/* even if nothing has been printed on it. */
	code = pcl_end_page_always(pcls);
	if ( code < 0 )
	  return code;
	if ( i > 0 && pcls->duplex )
	  put_param1_bool(pcls, "FirstSide", i == 1);
	return 0;
}

private int /* ESC & l 1 T */
pcl_job_separation(pcl_args_t *pargs, pcl_state_t *pcls)
{	int i = int_arg(pargs);
	if ( i != 1 )
	  return 0;
	/**** NEED A DRIVER PROCEDURE FOR END-OF-JOB ****/
	return 0;
}

private int /* ESC & l <bin_enum> G */
pcl_output_bin_selection(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint i = uint_arg(pargs);
	if ( i < 1 || i > 2 )
	  return e_Range;
	return put_param1_int(pcls, "OutputBin", i);
}

private int /* ESC & u <upi> B */
pcl_set_unit_of_measure(pcl_args_t *pargs, pcl_state_t *pcls)
{	int num = int_arg(pargs);

	if ( num <= 96 )
	  num = 96;
	else if ( num >= 7200 )
	  num = 7200;
	else if ( 7200 % num != 0 )
	{	/* Pick the exact divisor of 7200 with the smallest */
		/* relative error. */
		static const int values[] = {
		  96, 100, 120, 144, 150, 160, 180, 200, 225, 240, 288,
		  300, 360, 400, 450, 480, 600, 720, 800, 900,
		  1200, 1440, 1800, 2400, 3600, 7200
		};
		const int *p = values;

		while ( num > p[1] ) p++;
		/* Now *p < num < p[1]. */
		if ( (p[1] - (float)num) / p[1] < ((float)num - *p) / *p )
		  p++;
		num = *p;
	}
	pcls->uom_cp = pcl_coord_scale / num;
	pcls->uom_dots = pcls->resolution.y / num;
	return 0;
}

/* Initialization */
private int
pcjob_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_ESCAPE('E', "Printer Reset", pcl_printer_reset)
	DEFINE_CLASS('%')
	  {0, 'X', {pcl_exit_language, pca_neg_ok|pca_big_error}},
	END_CLASS
	DEFINE_CLASS('&')
	  {'l', 'X',
	     PCL_COMMAND("Number of Copies", pcl_number_of_copies,
			 pca_neg_ignore|pca_big_clamp)},
	  {'l', 'S',
	     PCL_COMMAND("Simplex/Duplex Print", pcl_simplex_duplex_print,
			 pca_neg_ignore|pca_big_ignore)},
	  {'l', 'U',
	     PCL_COMMAND("Left Offset Registration",
			 pcl_left_offset_registration,
			 pca_neg_ok|pca_big_error)},
	  {'l', 'Z',
	     PCL_COMMAND("Top Offset Registration",
			 pcl_top_offset_registration,
			 pca_neg_ok|pca_big_error)},
	  {'a', 'G',
	     PCL_COMMAND("Duplex Page Side Select",
			 pcl_duplex_page_side_select,
			 pca_neg_ignore|pca_big_ignore)},
	  {'l', 'T',
	     PCL_COMMAND("Job Separation", pcl_job_separation,
			 pca_neg_error|pca_big_error)},
	  {'l', 'G',
	     PCL_COMMAND("Output Bin Selection", pcl_output_bin_selection,
			 pca_neg_error|pca_big_error)},
	  {'u', 'D',
	     PCL_COMMAND("Set Unit of Measure", pcl_set_unit_of_measure,
			 pca_neg_error|pca_big_error)},
	END_CLASS
	return 0;
}
private void
pcjob_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { pcls->num_copies = 1;
	    pcls->duplex = false;
	    pcls->left_offset_cp = 0;
	    pcls->top_offset_cp = 0;
	    pcls->back_side = false;
	    pcls->output_bin = 1;
	    { pcl_args_t args;
	      arg_set_uint(&args, 300);
	      pcl_set_unit_of_measure(&args, pcls);
	    }
	  }
}
const pcl_init_t pcjob_init = {
  pcjob_do_init, pcjob_do_reset
};
