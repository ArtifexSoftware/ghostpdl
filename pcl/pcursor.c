/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcursor.c */
/* PCL5 cursor positioning commands */
#include "std.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcdraw.h"
#include "pcfont.h"

/* Control character implementations. */
/* do_CR and do_LF are exported for display_functions. */

int
pcl_do_CR(pcl_state_t *pcls)
{	
	pcl_break_underline(pcls);
	pcls->cap.x = pcls->left_margin;
	pcl_continue_underline(pcls);
	return 0;
}

private int
pcl_do_FF(pcl_state_t *pcls)
{	int code;
	code = pcl_end_page_always(pcls);
	if ( code < 0 )
	  return code;
	pcls->cap.y = pcls->top_margin;
	pcl_continue_underline(pcls);	/* (after adjusting y!) */
	return 0;
}

/* Move the cursor down, taking perforation skip into account if necessary. */
private int
move_down(pcl_state_t *pcls, coord dy)
{	coord y = pcls->cap.y + pcls->vmi;
	if ( pcls->perforation_skip &&
	     y > pcls->top_margin + pcls->text_length * pcls->vmi
	   )
	  return pcl_do_FF(pcls);
	pcl_set_cursor_y(pcls, y, false);
	return 0;
}

int
pcl_do_LF(pcl_state_t *pcls)
{	return move_down(pcls, pcls->vmi);
}

/* Commands */

int /* ESC & a <cols> C */
pcl_horiz_cursor_pos_columns(pcl_args_t *pargs, pcl_state_t *pcls)
{	float x = float_arg(pargs) * pcls->hmi;
	pcl_set_cursor_x(pcls,
			 (coord)(arg_is_signed(pargs) ? pcls->cap.x + x : x),
			 false);
	return 0;
}

int /* ESC & a <dp> H */
pcl_horiz_cursor_pos_decipoints(pcl_args_t *pargs, pcl_state_t *pcls)
{	float x = float_arg(pargs) * 10;		/* centipoints */
	pcl_set_cursor_x(pcls,
			 (coord)(arg_is_signed(pargs) ? pcls->cap.x + x : x),
			 false);
	return 0;
}

int /* ESC * p <units> X */
pcl_horiz_cursor_pos_units(pcl_args_t *pargs, pcl_state_t *pcls)
{	float x = float_arg(pargs) * pcls->uom_cp;	/* centipoints */
	pcl_set_cursor_x(pcls,
			 (coord)(arg_is_signed(pargs) ? pcls->cap.x + x : x),
			 false);
	return 0;
}

int /* CR */
pcl_CR(pcl_args_t *pargs, pcl_state_t *pcls)
{	int code = pcl_do_CR(pcls);
	if ( code < 0 )
	  return code;
	if ( pcls->line_termination & 1 )
	  code = pcl_do_LF(pcls);
	return code;
}

int /* SP */
pcl_SP(pcl_args_t *pargs, pcl_state_t *pcls)
{	/**** DOESN'T HANDLE PRINTABLE SPACES ****/
	pcl_set_cursor_x(pcls, pcls->cap.x + pcls->hmi, true);
	return 0;
}

int /* BS */
pcl_BS(pcl_args_t *pargs, pcl_state_t *pcls)
{
	pcl_set_cursor_x(pcls, pcls->cap.x - pcls->last_width.x, true);
	pcl_set_cursor_y(pcls, pcls->cap.y - pcls->last_width.y, true);
	pcls->last_was_BS = true;
	return 0;
}

int /* HT */
pcl_HT(pcl_args_t *pargs, pcl_state_t *pcls)
{	if ( pcls->hmi == 0 )
	  return 0;
	{ coord tab = pcls->hmi * 8;
	  pcl_set_cursor_x(pcls, (pcls->cap.x / tab + 1) * tab, true);
	}
	return 0;
}

int /* ESC & a <rows> R */
pcl_vert_cursor_pos_rows(pcl_args_t *pargs, pcl_state_t *pcls)
{	float y = float_arg(pargs) * pcls->vmi;		/* centipoints */
	pcl_set_cursor_y(pcls,
			 (coord)((arg_is_signed(pargs) ? pcls->cap.y :
				  pcls->top_margin) + y),
			 false);
	return 0;
}

int /* ESC & a <dp> V */
pcl_vert_cursor_pos_decipoints(pcl_args_t *pargs, pcl_state_t *pcls)
{	float y = float_arg(pargs) * 10;		/* centipoints */
	pcl_set_cursor_y(pcls,
			 (coord)((arg_is_signed(pargs) ? pcls->cap.y :
				  pcls->top_margin) + y),
			 false);
	return 0;
}

int /* ESC * p <units> Y */
pcl_vert_cursor_pos_units(pcl_args_t *pargs, pcl_state_t *pcls)
{	float y = float_arg(pargs) * pcls->uom_cp;	/* centipoints */
	pcl_set_cursor_y(pcls,
			 (coord)((arg_is_signed(pargs) ? pcls->cap.y :
				  pcls->top_margin) + y),
			 false);
	return 0;
}

int /* ESC = */
pcl_half_line_feed(pcl_args_t *pargs, pcl_state_t *pcls)
{	return move_down(pcls, pcls->vmi / 2);
}

int /* LF */
pcl_LF(pcl_args_t *pargs, pcl_state_t *pcls)
{	if ( pcls->line_termination & 2 )
	{	int code = pcl_do_CR(pcls);
		if ( code < 0 ) return code;
	}
	return pcl_do_LF(pcls);
}

int /* FF */
pcl_FF(pcl_args_t *pargs, pcl_state_t *pcls)
{	if ( pcls->line_termination & 2 )
	{	int code = pcl_do_CR(pcls);
		if ( code < 0 ) return code;
	}
	return pcl_do_FF(pcls);
}

int /* ESC & k G */
pcl_line_termination(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint ui = uint_arg(pargs);
	if ( ui > 3 )
	  return e_Range;
	pcls->line_termination = ui;
	return 0;
}

int /* ESC & f <pp_enum> S */
pcl_push_pop_cursor(pcl_args_t *pargs, pcl_state_t *pcls)
{	switch ( uint_arg(pargs) )
	{
	case 0:		/* push cursor */
		if ( pcls->cursor_stack.depth < 20 )
		  pcls->cursor_stack.values[pcls->cursor_stack.depth++] =
		    pcls->cap;
		break;
	case 1:		/* pop cursor */
		if ( pcls->cursor_stack.depth > 0 )
		  { pcls->cap =
		      pcls->cursor_stack.values[--(pcls->cursor_stack.depth)];
		    pcl_clamp_cursor_x(pcls, false);
		    pcl_clamp_cursor_y(pcls, false);
		  }
		break;
	}
	return 0;
}

/* Initialization */
private int
pcursor_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('&')
	  {'a', 'C', {pcl_horiz_cursor_pos_columns, pca_neg_ok|pca_big_ok}},
	  {'a', 'H', {pcl_horiz_cursor_pos_decipoints, pca_neg_ok|pca_big_ok}},
	END_CLASS
	DEFINE_CLASS('*')
	  {'p', 'X', {pcl_horiz_cursor_pos_units, pca_neg_ok|pca_big_ok}},
	END_CLASS
	DEFINE_CONTROL(CR, pcl_CR)
	DEFINE_CONTROL(SP, pcl_SP)
	DEFINE_CONTROL(BS, pcl_BS)
	DEFINE_CONTROL(HT, pcl_HT)
	DEFINE_CLASS('&')
	  {'a', 'R', {pcl_vert_cursor_pos_rows, pca_neg_ok|pca_big_ok}},
	  {'a', 'V', {pcl_vert_cursor_pos_decipoints, pca_neg_ok|pca_big_ok}},
	END_CLASS
	DEFINE_CLASS('*')
	  {'p', 'Y', {pcl_vert_cursor_pos_units, pca_neg_ok|pca_big_ok}},
	END_CLASS
	DEFINE_ESCAPE('=', pcl_half_line_feed)
	DEFINE_CONTROL(LF, pcl_LF)
	DEFINE_CONTROL(FF, pcl_FF)
	DEFINE_CLASS('&')
	  {'k', 'G', {pcl_line_termination, pca_neg_error|pca_big_error}},
	  {'f', 'S', {pcl_push_pop_cursor, pca_neg_ignore|pca_big_ignore}},
	END_CLASS
	return 0;
}
private void
pcursor_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { pcls->line_termination = 0;
	    if ( type & pcl_reset_initial )
	      { /**** SHOULD reset_printer ALSO RESET THESE? ****/
		pcls->cap.x = 0;
	        pcls->cap.y = 0;
		pcls->cursor_stack.depth = 0;
	      }
	  }
}
const pcl_init_t pcursor_init = {
  pcursor_do_init, pcursor_do_reset
};
