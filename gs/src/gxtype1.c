/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* gxtype1.c */
/* Adobe Type 1 font interpreter support */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsccode.h"
#include "gsline.h"
#include "gsstruct.h"
#include "gxarith.h"
#include "gxfixed.h"
#include "gxistate.h"
#include "gxmatrix.h"
#include "gxcoord.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxtype1.h"
#include "gzpath.h"

/*
 * The routines in this file are used for both Type 1 and Type 2
 * charstring interpreters.
 */

/*
 * Define whether or not to force hints to "big pixel" boundaries
 * when rasterizing at higher resolution.  With the current algorithms,
 * a value of 1 is better for devices without alpha capability,
 * but 0 is better if alpha is available.
 */
#define FORCE_HINTS_TO_BIG_PIXELS 1

/* Structure descriptor */
public_st_gs_font_type1();

/* Encrypt a string. */
int
gs_type1_encrypt(byte *dest, const byte *src, uint len, crypt_state *pstate)
{	register crypt_state state = *pstate;
	register const byte *from = src;
	register byte *to = dest;
	register uint count = len;
	while ( count )
	   {	encrypt_next(*from, state, *to);
		from++, to++, count--;
	   }
	*pstate = state;
	return 0;
}
/* Decrypt a string. */
int
gs_type1_decrypt(byte *dest, const byte *src, uint len, crypt_state *pstate)
{	register crypt_state state = *pstate;
	register const byte *from = src;
	register byte *to = dest;
	register uint count = len;
	while ( count )
	   {	/* If from == to, we can't use the obvious */
		/*	decrypt_next(*from, state, *to);	*/
		register byte ch = *from++;
		decrypt_next(ch, state, *to);
		to++, count--;
	   }
	*pstate = state;
	return 0;
}

/* Define the structure type for a Type 1 interpreter state. */
public_st_gs_type1_state();
/* GC procedures */
#define pcis ((gs_type1_state *)vptr)
private ENUM_PTRS_BEGIN(gs_type1_state_enum_ptrs) {
	if ( index < pcis->ips_count + 3 )
	  {	ENUM_RETURN_CONST_STRING_PTR(gs_type1_state,
					     ipstack[index - 3].char_string);
	  }
	return 0;
	}
	ENUM_PTR(0, gs_type1_state, pfont);
	ENUM_PTR(1, gs_type1_state, pis);
	ENUM_PTR(2, gs_type1_state, path);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(gs_type1_state_reloc_ptrs) {
	int i;
	RELOC_PTR(gs_type1_state, pfont);
	RELOC_PTR(gs_type1_state, pis);
	RELOC_PTR(gs_type1_state, path);
	for ( i = 0; i < pcis->ips_count; i++ )
	  {	ip_state *ipsp = &pcis->ipstack[i];
		int diff = ipsp->ip - ipsp->char_string.data;
		gs_reloc_const_string(&ipsp->char_string, gcst);
		ipsp->ip = ipsp->char_string.data + diff;
	  }
} RELOC_PTRS_END
#undef pcis

/* ------ Interpreter entry point ------ */

private int
gs_no_charstring_interpret(gs_type1_state *pcis, const gs_const_string *str,
  int *pindex)
{	return_error(gs_error_rangecheck);
}
int (*gs_charstring_interpreter[3])
     (P3(gs_type1_state *pcis, const gs_const_string *str, int *pindex)) = {
       gs_no_charstring_interpret,
       gs_no_charstring_interpret,
       gs_no_charstring_interpret
};

/*
 * Continue interpreting a Type 1 charstring.  If str != 0, it is taken as
 * the byte string to interpret.  Return 0 on successful completion, <0 on
 * error, or >0 when client intervention is required (or allowed).  The int*
 * argument is where the othersubr # is stored for callothersubr.
 */
int
gs_type1_interpret(gs_type1_state *pcis, const gs_const_string *str,
  int *pindex)
{	return (*gs_charstring_interpreter[pcis->pfont->data.CharstringType])
	  (pcis, str, pindex);
}

/* ------ Interpreter services ------ */

#define s (*ps)

/* We export this for the Type 2 charstring interpreter. */
void
accum_xy_proc(register is_ptr ps, fixed dx, fixed dy)
{	ptx += c_fixed(dx, xx),
	pty += c_fixed(dy, yy);
	if ( sfc.skewed )
	  ptx += c_fixed(dy, yx),
	  pty += c_fixed(dx, xy);
}

/* Initialize a Type 1 interpreter. */
/* The caller must supply a string to the first call of gs_type1_interpret. */
int
gs_type1_interp_init(register gs_type1_state *pcis, gs_imager_state *pis,
  gx_path *ppath, const gs_log2_scale_point *pscale, bool charpath_flag,
  int paint_type, gs_font_type1 *pfont)
{	static const gs_log2_scale_point no_scale = { 0, 0 };
	const gs_log2_scale_point *plog2_scale =
	  (FORCE_HINTS_TO_BIG_PIXELS ? pscale : &no_scale);

	pcis->pfont = pfont;
	pcis->pis = pis;
	pcis->path = ppath;
	/*
	 * charpath_flag controls coordinate rounding, hinting, and
	 * flatness enhancement.  If we allow it to be set to true,
	 * charpath may produce results quite different from show.
	 */
	pcis->charpath_flag = false /*charpath_flag*/;
	pcis->paint_type = paint_type;
	pcis->os_count = 0;
	pcis->ips_count = 1;
	pcis->ipstack[0].ip = 0;
	pcis->ipstack[0].char_string.data = 0;
	pcis->ipstack[0].char_string.size = 0;
	pcis->ignore_pops = 0;
	pcis->init_done = -1;
	pcis->sb_set = false;
	pcis->width_set = false;

	/* Set the sampling scale. */
	set_pixel_scale(&pcis->scale.x, plog2_scale->x);
	set_pixel_scale(&pcis->scale.y, plog2_scale->y);

	return 0;
}
/* Preset the left side bearing and/or width. */
void
gs_type1_set_lsb(gs_type1_state *pcis, const gs_point *psbpt)
{	pcis->lsb.x = float2fixed(psbpt->x);
	pcis->lsb.y = float2fixed(psbpt->y);
	pcis->sb_set = true;
}
void
gs_type1_set_width(gs_type1_state *pcis, const gs_point *pwpt)
{	pcis->width.x = float2fixed(pwpt->x);
	pcis->width.y = float2fixed(pwpt->y);
	pcis->width_set = true;
}
/* Finish initializing the interpreter if we are actually rasterizing */
/* the character, as opposed to just computing the side bearing and width. */
void
gs_type1_finish_init(gs_type1_state *pcis, gs_op1_state _ss *ps)
{	gs_imager_state *pis = pcis->pis;

	/* Set up the fixed version of the transformation. */
	gx_matrix_to_fixed_coeff(&ctm_only(pis), &pcis->fc, max_coeff_bits);
	sfc = pcis->fc;

	/* Set the current point of the path to the origin, */
	/* in anticipation of the initial [h]sbw. */
	{ gx_path *ppath = pcis->path;
	  ptx = pcis->origin.x = ppath->position.x;
	  pty = pcis->origin.y = ppath->position.y;
	}

	/* Initialize hint-related scalars. */
	pcis->have_hintmask = false;
	pcis->seac_base = -1;
	pcis->asb_diff = pcis->adxy.x = pcis->adxy.y = 0;
	pcis->flex_count = flex_max;		/* not in Flex */
	pcis->dotsection_flag = dotsection_out;
	pcis->vstem3_set = false;
	pcis->vs_offset.x = pcis->vs_offset.y = 0;
	pcis->hints_initial = 0;		/* probably not needed */
	pcis->hint_next = 0;
	pcis->hints_pending = 0;

	/* Assimilate the hints proper. */
	{ gs_log2_scale_point log2_scale;
	  log2_scale.x = pcis->scale.x.log2_unit;
	  log2_scale.y = pcis->scale.y.log2_unit;
	  if ( pcis->charpath_flag )
	    reset_font_hints(&pcis->fh, &log2_scale);
	  else
	    compute_font_hints(&pcis->fh, &pis->ctm, &log2_scale,
			       &pcis->pfont->data);
	}
	reset_stem_hints(pcis);

	/*
	 * Set the flatness to a value that is likely to produce reasonably
	 * good-looking curves, regardless of its current value in the
	 * graphics state.  If the character is very small, set the flatness
	 * to zero, which will produce very accurate curves.
	 */
	   {	float cxx = fabs(pis->ctm.xx), cyy = fabs(pis->ctm.yy);
		if ( cyy < cxx )
		  cxx = cyy;
		if ( !is_xxyy(&pis->ctm) )
		   {	float cxy = fabs(pis->ctm.xy), cyx = fabs(pis->ctm.yx);
			if ( cxy < cxx )
			  cxx = cxy;
			if ( cyx < cxx )
			  cxx = cyx;
		   }
		/* Don't let the flatness be worse than the default. */
		if ( cxx > pis->flatness )
		  cxx = pis->flatness;
		/* If the character is tiny, force accurate curves. */
		if ( cxx < 0.2 )
		  cxx = 0;
		pcis->flatness = cxx;
	   }

	/* Move to the side bearing point. */
	accum_xy(pcis->lsb.x, pcis->lsb.y);
	pcis->position.x = ptx;
	pcis->position.y = pty;

	pcis->init_done = 1;
}

/* ------ Operator procedures ------ */

/* We put these before the interpreter to save having to write */
/* prototypes for all of them. */

int
gs_op1_closepath(register is_ptr ps)
{	/* Note that this does NOT reset the current point! */
	gx_path *ppath = sppath;
	subpath *psub;
	segment *pseg;
	fixed dx, dy;
	int code;

	/* Check for and suppress a microscopic closing line. */
	if ( (psub = ppath->current_subpath) != 0 &&
	     (pseg = psub->last) != 0 &&
	     (dx = pseg->pt.x - psub->pt.x,
	      any_abs(dx) < float2fixed(0.1)) &&
	     (dy = pseg->pt.y - psub->pt.y,
	      any_abs(dy) < float2fixed(0.1))
	   )
	  switch ( pseg->type )
	    {
	    case s_line:
	      code = gx_path_pop_close_subpath(sppath);
	      break;
	    case s_curve:
	      /*
	       * Unfortunately, there is no "s_curve_close".  (Maybe there
	       * should be?)  Just adjust the final point of the curve so it
	       * is identical to the closing point.
	       */
	      pseg->pt = psub->pt;
#define pcseg ((curve_segment *)pseg)
	      pcseg->p2.x -= dx;
	      pcseg->p2.y -= dy;
#undef pcseg
	      /* falls through */
	    default:
	      /* What else could it be?? */
	      code = gx_path_close_subpath(sppath);
	    }
	else
	  code = gx_path_close_subpath(sppath);
	if ( code < 0 )
	  return code;
	return gx_path_add_point(ppath, ptx, pty);	/* put the point where it was */
}

int
gs_op1_rrcurveto(register is_ptr ps, fixed dx1, fixed dy1,
  fixed dx2, fixed dy2, fixed dx3, fixed dy3)
{	gs_fixed_point pt1, pt2;
	fixed ax0 = sppath->position.x - ptx;
	fixed ay0 = sppath->position.y - pty;
	accum_xy(dx1, dy1);
	pt1.x = ptx + ax0, pt1.y = pty + ay0;
	accum_xy(dx2, dy2);
	pt2.x = ptx, pt2.y = pty;
	accum_xy(dx3, dy3);
	return gx_path_add_curve(sppath, pt1.x, pt1.y, pt2.x, pt2.y, ptx, pty);
}

#undef s

/* Record the side bearing and character width. */
int
gs_type1_sbw(gs_type1_state *pcis, fixed lsbx, fixed lsby, fixed wx, fixed wy)
{	if ( !pcis->sb_set )
	  pcis->lsb.x = lsbx, pcis->lsb.y = lsby,
	    pcis->sb_set = true;	/* needed for accented chars */
	if ( !pcis->width_set )
	  pcis->width.x = wx, pcis->width.y = wy,
	    pcis->width_set = true;
	if_debug4('1',"[1]sb=(%g,%g) w=(%g,%g)\n",
		  fixed2float(pcis->lsb.x), fixed2float(pcis->lsb.y),
		  fixed2float(pcis->width.x), fixed2float(pcis->width.y));
	return 0;
}

/* Handle the end of a character. */
int
gs_type1_endchar(gs_type1_state *pcis)
{	gs_imager_state *pis = pcis->pis;
	gx_path *ppath = pcis->path;

	if ( pcis->hint_next != 0 || path_is_drawing(ppath) )
	  apply_path_hints(pcis, true);
	/* Set the current point to the character origin */
	/* plus the width. */
	{ gs_fixed_point pt;

	  gs_point_transform2fixed(&pis->ctm,
				   fixed2float(pcis->width.x),
				   fixed2float(pcis->width.y),
				   &pt);
	  gx_path_add_point(ppath, pt.x, pt.y);
	}
	if ( pcis->scale.x.log2_unit + pcis->scale.y.log2_unit == 0 )
	{	/*
		 * Tweak up the fill adjustment.  This is a hack for when
		 * we can't oversample.  The values here are based entirely
		 * on experience, not theory, and are designed primarily
		 * for displays and low-resolution fax.
		 */
		gs_fixed_rect bbox;
		int dx, dy, dmax;

		gx_path_bbox(ppath, &bbox);
		dx = fixed2int_ceiling(bbox.q.x - bbox.p.x);
		dy = fixed2int_ceiling(bbox.q.y - bbox.p.y);
		dmax = max(dx, dy);
		if ( pcis->fh.snap_h.count || pcis->fh.snap_v.count ||
		     pcis->fh.a_zone_count
		   )
		{	/* We have hints.  Only tweak up a little at */
			/* very small sizes, to help nearly-vertical */
			/* or nearly-horizontal diagonals. */
			pis->fill_adjust.x = pis->fill_adjust.y =
				(dmax < 15 ? float2fixed(0.15) :
				 dmax < 25 ? float2fixed(0.1) :
				 fixed_0);
		}
		else
		{	/* No hints.  Tweak a little more to compensate */
			/* for lack of snapping to pixel grid. */
			pis->fill_adjust.x = pis->fill_adjust.y =
				(dmax < 10 ? float2fixed(0.2) :
				 dmax < 25 ? float2fixed(0.1) :
				 float2fixed(0.05));
		}
	}
	else
	{	/* Don't do any adjusting. */
		pis->fill_adjust.x = pis->fill_adjust.y = fixed_0;
	}
	/* Set the flatness for curve rendering. */
	if ( !pcis->charpath_flag )
	  gs_imager_setflat(pis, pcis->flatness);
	return 0;
}
