/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* rtmisc.c */
/* Miscellanous HP RTL commands */
/* HAS the order of these are jumbled because of dependencies */
#include "math_.h"
#include "pgmand.h"
#include "pgdraw.h" /* for hpgl_add_pcl_point_to_path() */
#include "pgmisc.h" /* for hpgl_call */
#include "gsmemory.h"
#include "gsrop.h" 
#include "gscoord.h"
#include "pcdraw.h"

/* ---------------- Chapter 4 ---------------- */

/* Import the table of pointers to initialization data. */
extern const pcl_init_t *pcl_init_table[];

private int /* ESC E */
rtl_printer_reset(pcl_args_t *pargs, pcl_state_t *pcls)
{	/* Print any partial page. */
	{ int code = pcl_end_page_if_marked(pcls);
	  if ( code < 0 )
	    return code;
	}
	/* Reset to user default state. */
	return pcl_do_resets(pcls, pcl_reset_printer);
}

private int /* ESC % -12345 X */
rtl_exit_language(pcl_args_t *pargs, pcl_state_t *pcls)
{	if ( int_arg(pargs) != -12345 )
	   return e_Range;
	{ int code = rtl_printer_reset(pargs, pcls);
	  return (code < 0 ? code : e_ExitLanguage);
	}
}

/* ---------------- Chapter 13 ---------------- */

private int /* ESC * v <bool> N */
pcl_source_transparency_mode(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint i = uint_arg(pargs);
	if ( i > 1 )
	  return 0;
	pcls->source_transparent = !i;
	gs_setsourcetransparent(pcls->pgs, pcls->source_transparent);
	return 0;
}

/* ---------------- Chapter 18 ---------------- */

private int /* ESC % <enum> B */ 
rtl_enter_hpgl_mode(pcl_args_t *pargs, pcl_state_t *pcls)
{	int i = int_arg(pargs);

	/* Note: -1..3 for PCL5c, 0..1 for PCL5 */
	if ( i < 0 )
	  i = -1;
	else if ( i > 3)
	  return 0;
	/**** PARTIAL IMPLEMENTATION ****/
	if ( pcls->parse_data != 0 )
	  return 0;		/* already in HP-GL/2 mode */
	pcls->parse_data =
	  gs_alloc_bytes(pcls->memory, sizeof(hpgl_parser_state_t),
			 "hpgl parser data(enter hpgl mode)");
	if ( pcls->parse_data == 0 )
	  return_error(e_Memory);
	hpgl_process_init(pcls->parse_data);
	pcls->parse_other = hpgl_process;
	/* add the pcl cap to hpgl/2's path */
	if ( i == 1 )
	  {
	    gs_point pcl_pt;
	    pcl_pt.x = (hpgl_real_t)pcls->cap.x;
	    pcl_pt.y = (hpgl_real_t)pcls->cap.y;
	    hpgl_add_pcl_point_to_path(pcls, &pcl_pt);
	  }
	return 0;
}

/* We export this so we can call it from HP-GL/2 configurations. */
/* Note that it returns 1 iff it changed the PCL CAP. */
int /* ESC % <enum> A */ 
rtl_enter_pcl_mode(pcl_args_t *pargs, pcl_state_t *pcls)
{	int b = int_arg(pargs) & 1;

	if ( pcls->parse_data )
	  { /* We were in HP-GL/2 mode.  Destroy the gl/2 polygon path
	    and conditionally copy back the cursor position. */
	    gx_path_release(&(pcls->g.polygon.buffer.path));
	    if ( b )
	      { /* the usual user -> device -> user dance.  HAS This is
		done frequently enough that it should be separated
		out into a separate function */
		gs_point pt, dev_pt;
		hpgl_call(hpgl_set_ctm(pcls));
		hpgl_call(hpgl_get_current_position(pcls, &pt));
		hpgl_call(gs_transform(pcls->pgs, pt.x, pt.y, &dev_pt));
		hpgl_call(pcl_set_ctm(pcls, false));
		hpgl_call(gs_itransform(pcls->pgs, dev_pt.x, dev_pt.y, &pt));
		/* HPGL/2 uses floats for coordinates */
/* HAS have not checked if this is properly rounded or truncated */
#define round(x) (((x) < 0.0) ? (ceil ((x) - 0.5)) : (floor ((x) + 0.5)))
		pcls->cap.x = round(pt.x);
	        pcls->cap.y = round(pt.y);
#undef round
	      }
	    gs_free_object(pcls->memory, pcls->parse_data,
			   "hpgl parser data(enter pcl mode)");
	    pcls->parse_data = 0;
	  }
	else
	  b = 0;
	pcls->parse_other = 0;
	return b;		/* not 0, see comment above */
}

/* ---------------- Comparison Guide ---------------- */

/* (From PCL5 Comparison Guide, p. 1-30 & 1-92) */
private int /* ESC & b <count> W */
pcl_appletalk_configuration(pcl_args_t *pargs, pcl_state_t *pcls)
{	const byte *data = arg_data(pargs);
	uint count = uint_arg(pargs);
	uint i;

	if ( count < 2 || data[0] == ' ' )
	  return e_Range;
	for ( i = 1; data[i] != ' '; ++i )
	  if ( i == count - 1 )
	    return e_Range;
	if ( pcls->configure_appletalk == 0 )
	  return 0;
	return (*pcls->configure_appletalk)(data, i, data + i + 1,
					    count - (i + 1));
}

/* (From PCL5 Comparison Guide, p. 1-100) */
private int /* ESC & a <bool> N */
pcl_negative_motion(pcl_args_t *pargs, pcl_state_t *pcls)
{	int motion = int_arg(pargs);
	if ( motion > 1 )
	  return e_Range;
	/* Currently we can't take any advantage of this.... */
	return 0;
}

/* ---------------- Initialization ---------------- */
private int
rtmisc_do_init(gs_memory_t *mem)
{		/* Register commands */
		/* Chapter 4 */
	DEFINE_ESCAPE('E', "Printer Reset", rtl_printer_reset)
	DEFINE_CLASS('%')
	  {0, 'X',
	     PCL_COMMAND("Exit Language", rtl_exit_language,
			 pca_neg_ok|pca_big_error)},
		/* Chapter 18 */
	  {0, 'B',
	     PCL_COMMAND("Enter HP-GL/2 Mode", rtl_enter_hpgl_mode,
			 pca_neg_ok|pca_big_ok|pca_in_macro)},
	  {0, 'A',
	     PCL_COMMAND("Enter PCL Mode", rtl_enter_pcl_mode,
			 pca_neg_ok|pca_big_ok|pca_in_macro)},
	END_CLASS
		/* Chapter 13 */
	DEFINE_CLASS('*')
	  {'v', 'N',
	     PCL_COMMAND("Source Transparency Mode",
			 pcl_source_transparency_mode,
			 pca_neg_ignore|pca_big_ignore)},
	END_CLASS
		/* Comparison Guide */
	DEFINE_CLASS('&')
	  {'b', 'W',
	     PCL_COMMAND("Appletalk Configuration",
			 pcl_appletalk_configuration,
			 pca_bytes)},
	  {'a', 'N',
	     PCL_COMMAND("Negative Motion", pcl_negative_motion,
			 pca_neg_error|pca_big_error)},
	END_CLASS
	return 0;
}
private void
rtmisc_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  {	/* Chapter 13 */
	    pcls->source_transparent = true;
	    gs_setsourcetransparent(pcls->pgs, true);
		/* Comparison Guide */
	    pcls->configure_appletalk = 0;
	  }
}
private int
rtmisc_do_copy(pcl_state_t *psaved, const pcl_state_t *pcls,
  pcl_copy_operation_t operation)
{	if ( operation & pcl_copy_after )
	  { gs_setsourcetransparent(pcls->pgs, psaved->source_transparent);
	  }
	return 0;
}
const pcl_init_t rtmisc_init = {
  rtmisc_do_init, rtmisc_do_reset, rtmisc_do_copy
};
