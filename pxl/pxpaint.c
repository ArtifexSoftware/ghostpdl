/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* pxpaint.c */
/* PCL XL painting operators */

#include "math_.h"
#include "stdio_.h"			/* std.h + NULL */
#include "pxoper.h"
#include "pxstate.h"
#include "pxfont.h"			/* for px_text */
#include "gsstate.h"
#include "gscoord.h"
#include "gspaint.h"
#include "gspath.h"
#include "gspath2.h"
#include "gsrop.h"
#include "gxfarith.h"
#include "gxfixed.h"
#include "gxistate.h"
#include "gxmatrix.h"
#include "gxpath.h"
#include "pxptable.h"

/*
 * The published H-P specification says that the single-object painting
 * operators (Chord, Ellipse, Pie, Rectangle, RoundRectangle) leave the
 * path set to the object; however, H-P printers apparently reset the
 * path (but leave the current cursor defined).  To obtain the latter
 * behavior, uncomment the following #define.
 */
#define NEWPATH_AFTER_PAINT_SHAPE
/*
 * The published specification says that dash patterns do not scale when
 * the CTM changes; however, H-P printers do scale the dash pattern.
 * To make dash patterns not scale, uncomment the following #define.
 */
/*#define NO_SCALE_DASH_PATTERN*/
/*
 * The H-P documentation says we are supposed to draw rectangles
 * counter-clockwise on the page, which is clockwise in user space.
 * However, the LaserJet 6 (and probably the LJ 5 as well) draw rectangles
 * clockwise!  To draw rectangles clockwise, uncomment the following
 * #define.
 * clj4550 and clj4600 draw counter-clockwise rectangles
 */
/*#define DRAW_RECTANGLES_CLOCKWISE*/
/*
 * The H-P printers do really weird things for arcs, chords, or pies where
 * the width and/or height of the bounding box is negative.  To emulate
 * their behavior, uncomment the following #define.
 */
#define REFLECT_NEGATIVE_ARCS

/* Forward references */
px_operator_proc(pxNewPath);

/* ---------------- Utilities ---------------- */

/* Add lines to the path.  line_proc is gs_lineto or gs_rlineto. */
/* Attributes: pxaEndPoint, pxaNumberOfPoints, pxaPointType. */
static int
add_lines(px_args_t *par, px_state_t *pxs,
  int (*line_proc)(gs_state *, floatp, floatp))
{	int code = 0;

	if ( par->pv[0] )
	  { /* Single segment, specified as argument. */
	    if ( par->pv[1] || par->pv[2] )
	      return_error(errorIllegalAttributeCombination);
	    return (*line_proc)(pxs->pgs, real_value(par->pv[0], 0),
				real_value(par->pv[0], 1));
	  }
	/* Multiple segments, specified in source data. */
	if ( !(par->pv[1] && par->pv[2]) )
	  return_error(errorMissingAttribute);
	{ int32_t num_points = par->pv[1]->value.i;
	  pxeDataType_t type = (pxeDataType_t)par->pv[2]->value.i;
	  int point_size = (type == eUByte || type == eSByte ? 2 : 4);

	  while ( par->source.position < num_points * point_size )
	    { const byte *dp = par->source.data;
	      int px, py;

	      if ( par->source.available < point_size )
		{ /* We don't even have one point's worth of source data. */
		  return pxNeedData;
		}
	      switch ( type )
		{
		case eUByte:
		  px = dp[0];
		  py = dp[1];
		  break;
		case eSByte:
		  px = (int)(dp[0] ^ 0x80) - 0x80;
		  py = (int)(dp[1] ^ 0x80) - 0x80;
		  break;
		case eUInt16:
		  px = uint16at(dp, pxs->data_source_big_endian);
		  py = uint16at(dp + 2, pxs->data_source_big_endian);
		  break;
		case eSInt16:
		  px = sint16at(dp, pxs->data_source_big_endian);
		  py = sint16at(dp + 2, pxs->data_source_big_endian);
		  break;
		default:		/* can't happen, pacify compiler */
		  return_error(errorIllegalAttributeValue);
		}
	      code = (*line_proc)(pxs->pgs, (floatp)px, (floatp)py);
	      if ( code < 0 )
		break;
	      par->source.position += point_size;
	      par->source.available -= point_size;
	      par->source.data += point_size;
	    }
	}
	return code;
}

/* Add Bezier curves to the path.  curve_proc is gs_curveto or gs_rcurveto. */
/* Attributes: pxaNumberOfPoints, pxaPointType, pxaControlPoint1, */
/* pxaControlPoint2, pxaEndPoint. */
static int
add_curves(px_args_t *par, px_state_t *pxs,
  int (*curve_proc)(gs_state *, floatp, floatp, floatp, floatp, floatp, floatp))
{	int code = 0;

	if ( par->pv[2] && par->pv[3] && par->pv[4] )
	  { /* Single curve, specified as argument. */
	    if ( par->pv[0] || par->pv[1] )
	      return_error(errorIllegalAttributeCombination);
	    return (*curve_proc)(pxs->pgs, real_value(par->pv[2], 0),
				 real_value(par->pv[2], 1),
				 real_value(par->pv[3], 0),
				 real_value(par->pv[3], 1),
				 real_value(par->pv[4], 0),
				 real_value(par->pv[4], 1));
	  }
	/* Multiple segments, specified in source data. */
	else if ( par->pv[0] && par->pv[1] )
	  { if ( par->pv[2] || par->pv[3] || par->pv[4] )
	      return_error(errorIllegalAttributeCombination);
	  }
	else
	  return_error(errorMissingAttribute);
	{ int32_t num_points = par->pv[0]->value.i;
	  pxeDataType_t type = (pxeDataType_t)par->pv[1]->value.i;
	  int point_size = (type == eUByte || type == eSByte ? 2 : 4);
	  int segment_size = point_size * 3;

	  if ( num_points % 3 )
	    return_error(errorIllegalDataLength);
	  while ( par->source.position < num_points * point_size )
	    { const byte *dp = par->source.data;
	      int points[6];
	      int i;

	      if ( par->source.available < point_size * 3 )
		{ /* We don't even have one point's worth of source data. */
		  return pxNeedData;
		}
	      switch ( type )
		{
		case eUByte:
		  for ( i = 0; i < 6; ++i )
		    points[i] = dp[i];
		  break;
		case eSByte:
		  for ( i = 0; i < 6; ++i )
		    points[i] = (int)(dp[i] ^ 0x80) - 0x80;
		  break;
		case eUInt16:
		  for ( i = 0; i < 12; i += 2 )
		    points[i >> 1] =
		      uint16at(dp + i, pxs->data_source_big_endian);
		  break;
		case eSInt16:
		  for ( i = 0; i < 12; i += 2 )
		    points[i >> 1]
		      = sint16at(dp + i, pxs->data_source_big_endian);
		  break;
		default:		/* can't happen, pacify compiler */
		  return_error(errorIllegalAttributeValue);
		}
	      code = (*curve_proc)(pxs->pgs,
				   (floatp)points[0], (floatp)points[1],
				   (floatp)points[2], (floatp)points[3],
				   (floatp)points[4], (floatp)points[5]);
	      if ( code < 0 )
		break;
	      par->source.position += segment_size;
	      par->source.available -= segment_size;
	      par->source.data += segment_size;
	    }
	}
	return code;
}

/*
 * Set up all the parameters for an arc, chord, ellipse, or pie.  If pp3 and
 * pp4 are NULL, we're filling the entire box.  Store the upper left box
 * corner (for repositioning the cursor), the center, the radius, and the
 * two angles in *params, and return one of the following (or a negative
 * error code):
 */
typedef enum {
  /*
   * Arc box is degenerate (zero width and/or height).
   * Only origin and center have been set.
   */
  arc_degenerate = 0,
  /*
   * Arc box is square.  No CTM adjustment was required; save_ctm is
   * not set.
   */
  arc_square,
  /*
   * Arc box is rectangular, CTM has been saved and adjusted.
   */
  arc_rectangular
} px_arc_type_t;
typedef struct px_arc_params_s {
  gs_point origin;
  gs_point center;
  double radius;
  double ang3, ang4;
  gs_matrix save_ctm;
  bool reversed;
} px_arc_params_t;
static int /* px_arc_type_t or error code */
setup_arc(px_arc_params_t *params, const px_value_t *pbox,
  const px_value_t *pp3, const px_value_t *pp4, const px_state_t *pxs, bool ellipse)
{	real x1 = real_value(pbox, 0);
	real y1 = real_value(pbox, 1);
	real x2 = real_value(pbox, 2);
	real y2 = real_value(pbox, 3);
	real xc = (x1 + x2) * 0.5;
	real yc = (y1 + y2) * 0.5;
	real xr, yr;
	bool rotated;
	int code;

#ifdef REFLECT_NEGATIVE_ARCS
	rotated = x1 > x2;
	params->reversed = rotated ^ (y1 > y2);
#else
	rotated = false;
	params->reversed = false;
#endif
	if ( x1 > x2 )
	  { real temp = x1; x1 = x2; x2 = temp;
	  }
	if ( y1 > y2 )
	  { real temp = y1; y1 = y2; y2 = temp;
	  }
	params->origin.x = x1;
	params->origin.y = y1;
	xr = (x2 - x1) * 0.5;
	yr = (y2 - y1) * 0.5;
	/* From what we can gather ellipses are degenerate if both
           width and height of the bounding box are 0.  Other objects
           behave as expected.  A 0 area bounding box is degenerate */
	if ( ellipse ) {
	    /* The bounding box is degenerate, set what we can and exit. */
	    if ( xr == 0 && yr == 0 ) {
		params->center.x = xc;
		params->center.y = yc;
		return arc_degenerate;
	    }
	} else {
	    if ( xr == 0 || yr == 0 ) {
		params->center.x = xc;
		params->center.y = yc;
		return arc_degenerate;
	    }
	}
    
	if ( pp3 && pp4 )
	  { real dx3 = real_value(pp3, 0) - xc;
	    real dy3 = real_value(pp3, 1) - yc;
	    real dx4 = real_value(pp4, 0) - xc;
	    real dy4 = real_value(pp4, 1) - yc;
	    
	    if ( (dx3 == 0 && dy3 == 0) || (dx4 == 0 && dy4 == 0) )
	      return_error(errorIllegalAttributeValue);
	    { double ang3 = atan2(dy3 * xr, dx3 * yr) * radians_to_degrees;
	      double ang4 = atan2(dy4 * xr, dx4 * yr) * radians_to_degrees;

	      if ( rotated )
		ang3 += 180, ang4 += 180;
	      params->ang3 = ang3;
	      params->ang4 = ang4;
	    }
	  }
	params->radius = yr;
	if ( xr == yr )
	  { params->center.x = xc;
	    params->center.y = yc;
	    return arc_square;
	  }
	else
	  { /* Must adjust the CTM.  Move the origin to (xc,yc) */
	    /* for simplicity. */
	    if ( (code = gs_currentmatrix(pxs->pgs, &params->save_ctm)) < 0 ||
		 (code = gs_translate(pxs->pgs, xc, yc)) < 0 ||
		 (code = gs_scale(pxs->pgs, xr, yr)) < 0
	       )
	      return code;
	    params->center.x = 0;
	    params->center.y = 0;
	    params->radius = 1.0;
	    return arc_rectangular;
	  }
}

/* per the nonsense in 5.7.3 (The ROP3 Operands) from the pxl
   reference manual the following rops are allowed for stroking. */
static bool
pxl_allow_rop_for_stroke(gs_state *pgs)
{
    gs_rop3_t rop = gs_currentrasterop(pgs);
    if ( rop == 0 || rop == 160 || rop == 170 || rop == 240 || rop == 250 || rop == 255 )
        return true;
    return false;
}

/* Paint the current path, optionally resetting it afterwards. */
static int
paint_path(px_state_t *pxs, bool reset)
{	gs_state *pgs = pxs->pgs;
	gx_path *ppath = gx_current_path(pgs);
	px_gstate_t *pxgs = pxs->pxgs;
	bool will_stroke = pxgs->pen.type != pxpNull;
	gs_point cursor;
	int code = 0;
	gx_path *save_path = 0;

	if ( gx_path_is_void(ppath) )
	  return 0;		/* nothing to draw */
#ifdef NO_SCALE_DASH_PATTERN
#  define save_for_stroke (!reset || pxgs->dashed)
#else
#  define save_for_stroke (!reset)
#endif
	if ( pxgs->brush.type != pxpNull )
	  { int (*fill_proc)(gs_state *) =
	      (pxgs->fill_mode == eEvenOdd ? gs_eofill : gs_fill);

	    if ( (code = px_set_paint(&pxgs->brush, pxs)) < 0 )
	      return code;
	    pxs->have_page = true;
	    if ( !will_stroke && reset )
	      { gs_currentpoint(pgs, &cursor);
	        code = (*fill_proc)(pgs);
		gs_moveto(pgs, cursor.x, cursor.y);
		return code;
	      }
	    save_path = gx_path_alloc(pxs->memory, "paint_path(save_path)");
	    if ( save_path == 0 )
	      return_error(errorInsufficientMemory);
	    gx_path_assign_preserve(save_path, ppath);
	    code = (*fill_proc)(pgs);
	    if ( code < 0 )
	      goto rx;
	    if ( !will_stroke )
	      goto rx;
	    if ( save_for_stroke )
		gx_path_assign_preserve(ppath, save_path);
	    else {
	        gx_path_assign_free(ppath, save_path);
                gx_setcurrentpoint_from_path((gs_imager_state *)pgs, ppath);
		gs_currentpoint(pgs, &cursor);
	        save_path = 0;
	      }
	  }
	else if ( !will_stroke )
	  return 0;
	else if ( save_for_stroke )
	  { save_path =
	      gx_path_alloc(pxs->memory, "paint_path(save_path)");
	    if ( save_path == 0 )
	      return_error(errorInsufficientMemory);
	    gx_path_assign_preserve(save_path, ppath);
	  }
	else
	  gs_currentpoint(pgs, &cursor);
	/*
	 * The PCL XL documentation from H-P says that dash lengths do not
	 * scale according to the CTM, but according to H-P developer
	 * support, this isn't true.  We went to the trouble of implementing
	 * the published specification, so if it turns out to be right after
	 * all, we execute the following block of code.
	 */
#ifdef NO_SCALE_DASH_PATTERN
	/*
	 * If we have a dash pattern that was defined with a different
	 * CTM than the current one, we must pre-expand the dashes.
	 * (Eventually we should expand the library API to handle this.)
	 */
	if ( pxgs->dashed )
	  { gs_matrix mat;
	    gs_currentmatrix(pgs, &mat);
	    if ( mat.xx != pxgs->dash_matrix.xx ||
		 mat.xy != pxgs->dash_matrix.xy ||
		 mat.yx != pxgs->dash_matrix.yx ||
		 mat.yy != pxgs->dash_matrix.yy
	       )
	      { code = gs_flattenpath(pgs);
		if ( code < 0 )
		  goto rx;
		gs_setmatrix(pgs, &pxgs->dash_matrix);
		code = gs_dashpath(pgs);
		gs_setmatrix(pgs, &mat);
		if ( code < 0 )
		  goto rx;
	      }
	  }
#endif
	/*
	 * Per the description in the PCL XL reference documentation,
	 * set a standard logical operation and transparency for stroking.
	 */
        {  
            gs_rop3_t save_rop = gs_currentrasterop(pgs);
            bool save_transparent = gs_currenttexturetransparent(pgs);
            bool need_restore_rop = false;

            if (pxl_allow_rop_for_stroke(pgs) == false) {
                gs_setrasterop(pgs, rop3_T);
                gs_settexturetransparent(pgs, false);
                need_restore_rop = true;
            }
            pxs->have_page = true;
            /*
             * The H-P printers thicken very thin strokes slightly.
             * We do the same here.
             */
            { 
                float width = gs_currentlinewidth(pgs);
                gs_matrix mat;
                float sx, sy;

                gs_currentmatrix(pgs, &mat);
                sx = fabs(mat.xx) + fabs(mat.xy);
                sy = fabs(mat.yx) + fabs(mat.yy);
                width *= min(sx, sy);
                if ( width < 5 )
                    gs_setfilladjust(pgs, 0.5, 0.5);
            }
            if ( (code = px_set_paint(&pxgs->pen, pxs)) < 0 ||
                 (code = gs_stroke(pgs)) < 0
                 )
                DO_NOTHING;
            if ( need_restore_rop ) {
                gs_setrasterop(pgs, save_rop);
                gs_settexturetransparent(pgs, save_transparent);
            }
            gs_setfilladjust(pgs, 0.0, 0.0);
        }
 rx:	if ( save_path ) {
	    gx_path_assign_free(ppath, save_path);   /* path without a Current point! */
	    gx_setcurrentpoint_from_path((gs_imager_state *)pgs, ppath);  
	}
	else /* Iff save_path is NULL, set currentpoint back to original */
	    gs_moveto(pgs, cursor.x, cursor.y);
	return code;
}

/* Paint a shape defined by a one-operator path. */
static int
paint_shape(px_args_t *par, px_state_t *pxs, px_operator_proc((*path_op)))
{	int code;
	if ( (code = pxNewPath(par, pxs)) < 0 ||
	     (code = (*path_op)(par, pxs)) < 0
	   )
	  return code;
	/* See the beginning of the file regarding the following. */
#ifdef NEWPATH_AFTER_PAINT_SHAPE
	return paint_path(pxs, true);
#else
	return paint_path(pxs, false);
#endif
}

/* ---------------- Operators ---------------- */

const byte apxCloseSubPath[] = {0, 0};
int
pxCloseSubPath(px_args_t *par, px_state_t *pxs)
{	return gs_closepath(pxs->pgs);
}

const byte apxNewPath[] = {0, 0};
int
pxNewPath(px_args_t *par, px_state_t *pxs)
{	return gs_newpath(pxs->pgs);
}

const byte apxPaintPath[] = {0, 0};
int
pxPaintPath(px_args_t *par, px_state_t *pxs)
{	return paint_path(pxs, false);
}

const byte apxArcPath[] = {
  pxaBoundingBox, pxaStartPoint, pxaEndPoint, 0,
  pxaArcDirection, 0
};
int
pxArcPath(px_args_t *par, px_state_t *pxs)
{	/*
	 * Note that "clockwise" in user space is counter-clockwise on
	 * the page, because the Y coordinate is inverted.
	 */
	bool clockwise =
	  (par->pv[3] != 0 && par->pv[3]->value.i == eClockWise);
	px_arc_params_t params;
	int code = setup_arc(&params, par->pv[0], par->pv[1], par->pv[2], pxs, false);
	int rcode = code;

	if ( code >= 0 && code != arc_degenerate )
	  { bool closed = params.ang3 == params.ang4;
	    clockwise ^= params.reversed;
	    if ( closed )
	      { if ( clockwise ) params.ang4 += 360;
	        else params.ang3 += 360;
	      }
	    code = gs_arc_add(pxs->pgs, !clockwise, params.center.x,
			      params.center.y, params.radius, params.ang3,
			      params.ang4, false);
	    if ( code >= 0 && closed )
	      { /* We have to close the path explicitly. */
		code = gs_closepath(pxs->pgs);
	      }
	  }     
	if ( rcode == arc_rectangular )
	  gs_setmatrix(pxs->pgs, &params.save_ctm);
	return code;
}

const byte apxBezierPath[] = {
  0, pxaNumberOfPoints, pxaPointType, pxaControlPoint1, pxaControlPoint2,
  pxaEndPoint, 0
};
int
pxBezierPath(px_args_t *par, px_state_t *pxs)
{	return add_curves(par, pxs, gs_curveto);
}

const byte apxBezierRelPath[] = {
  0, pxaNumberOfPoints, pxaPointType, pxaControlPoint1, pxaControlPoint2,
  pxaEndPoint, 0
};
int
pxBezierRelPath(px_args_t *par, px_state_t *pxs)
{	return add_curves(par, pxs, gs_rcurveto);
}

const byte apxChord[] = {
  pxaBoundingBox, pxaStartPoint, pxaEndPoint, 0, 0
};
px_operator_proc(pxChordPath);
int
pxChord(px_args_t *par, px_state_t *pxs)
{	return paint_shape(par, pxs, pxChordPath);
}

const byte apxChordPath[] = {
  pxaBoundingBox, pxaStartPoint, pxaEndPoint, 0, 0
};
int
pxChordPath(px_args_t *par, px_state_t *pxs)
{	px_arc_params_t params;
	int code = setup_arc(&params, par->pv[0], par->pv[1], par->pv[2], pxs, false);
	int rcode = code;

	/* See ArcPath above for the meaning of "clockwise". */
	if ( code >= 0 && code != arc_degenerate )
	  { if ( params.ang3 == params.ang4 )
	      params.ang3 += 360;
	    code = gs_arc_add(pxs->pgs, !params.reversed,
			      params.center.x, params.center.y,
			      params.radius, params.ang3, params.ang4,
			      false);
	    if ( code >= 0 )
	      code = gs_closepath(pxs->pgs);
	  }
	if ( rcode == arc_rectangular )
	  gs_setmatrix(pxs->pgs, &params.save_ctm);
	if ( code >= 0 )
	  code = gs_moveto(pxs->pgs, params.origin.x, params.origin.y);
	return code;

}

const byte apxEllipse[] = {
  pxaBoundingBox, 0, 0
};
px_operator_proc(pxEllipsePath);
int
pxEllipse(px_args_t *par, px_state_t *pxs)
{	return paint_shape(par, pxs, pxEllipsePath);
}

const byte apxEllipsePath[] = {
  pxaBoundingBox, 0, 0
};
int
pxEllipsePath(px_args_t *par, px_state_t *pxs)
{	px_arc_params_t params;
	int code = setup_arc(&params, par->pv[0], NULL, NULL, pxs, true);
	int rcode = code;
	real a_start = 180.0;
	real a_end = -180.0;

	/* swap start and end angles if counter clockwise ellipse */
	if ( params.reversed )
	  { a_start = -180.0;
	    a_end = 180.0;
	  }
	/* See ArcPath above for the meaning of "clockwise". */
	if ( code < 0 || code == arc_degenerate ||
	     (code = gs_arc_add(pxs->pgs, !params.reversed,
				params.center.x, params.center.y,
				params.radius, a_start, a_end, false)) < 0 ||
	     /* We have to close the path explicitly. */
	     (code = gs_closepath(pxs->pgs)) < 0
	   )
	  DO_NOTHING;
	if ( rcode == arc_rectangular )
	  gs_setmatrix(pxs->pgs, &params.save_ctm);
	if ( code >= 0 )
	  code = gs_moveto(pxs->pgs, params.origin.x, params.origin.y);
	return code;
}

const byte apxLinePath[] = {
  0, pxaEndPoint, pxaNumberOfPoints, pxaPointType, 0
};
int
pxLinePath(px_args_t *par, px_state_t *pxs)
{	return add_lines(par, pxs, gs_lineto);
}

const byte apxLineRelPath[] = {
  0, pxaEndPoint, pxaNumberOfPoints, pxaPointType, 0
};
int
pxLineRelPath(px_args_t *par, px_state_t *pxs)
{	return add_lines(par, pxs, gs_rlineto);
}

const byte apxPie[] = {
  pxaBoundingBox, pxaStartPoint, pxaEndPoint, 0, 0
};
px_operator_proc(pxPiePath);
int
pxPie(px_args_t *par, px_state_t *pxs)
{	return paint_shape(par, pxs, pxPiePath);
}

const byte apxPiePath[] = {
  pxaBoundingBox, pxaStartPoint, pxaEndPoint, 0, 0
};
int
pxPiePath(px_args_t *par, px_state_t *pxs)
{	px_arc_params_t params;
	int code = setup_arc(&params, par->pv[0], par->pv[1], par->pv[2], pxs, false);
	int rcode = code;

	/* See ArcPath above for the meaning of "clockwise". */
	if ( code >= 0 && code != arc_degenerate )
	  { if ( params.ang3 == params.ang4 )
	      params.ang3 += 360;
	    code = gs_moveto(pxs->pgs, params.center.x, params.center.y);
	    if ( code >= 0 )
	      { code = gs_arc_add(pxs->pgs, !params.reversed,
				  params.center.x, params.center.y,
				  params.radius, params.ang3, params.ang4,
				  true);
	      }
	  }
	if ( rcode == arc_rectangular )
	  gs_setmatrix(pxs->pgs, &params.save_ctm);
	if ( code < 0 || rcode == arc_degenerate ||
	    (code = gs_closepath(pxs->pgs)) < 0 ||
	    (code = gs_moveto(pxs->pgs, params.origin.x, params.origin.y)) < 0
	   )
	  DO_NOTHING;
	return code;
}

const byte apxRectangle[] = {
  pxaBoundingBox, 0, 0
};
px_operator_proc(pxRectanglePath);
int
pxRectangle(px_args_t *par, px_state_t *pxs)
{	return paint_shape(par, pxs, pxRectanglePath);
}

const byte apxRectanglePath[] = {
  pxaBoundingBox, 0, 0
};
int
pxRectanglePath(px_args_t *par, px_state_t *pxs)
{	floatp x1, y1, x2, y2;
	gs_fixed_point p1;
	gs_state *pgs = pxs->pgs;
	gx_path *ppath = gx_current_path(pgs);
	gs_fixed_point lines[3];
#define p2 lines[1]
#define pctm (&((const gs_imager_state *)pgs)->ctm)
	int code;

	set_box_value(x1, y1, x2, y2, par->pv[0]);
	/*
	 * Rectangles are always drawn in a canonical order.
	 */
	if ( x1 > x2 )
	  { floatp t = x1; x1 = x2; x2 = t; }
	if ( y1 > y2 )
	  { floatp t = y1; y1 = y2; y2 = t; }
	if ( (code = gs_point_transform2fixed(pctm, x1, y1, &p1)) < 0 ||
	     (code = gs_point_transform2fixed(pctm, x2, y2, &p2)) < 0 ||
	     (code = gs_moveto(pgs, x1, y1)) < 0
	   )
	  return code;
#ifdef DRAW_RECTANGLES_CLOCKWISE
	/*
	 * DRAW_RECTANGLES_CLOCKWISE means clockwise on the page, which is
	 * counter-clockwise in user space.
	 */
	if ( (code = gs_point_transform2fixed(pctm, x2, y1, &lines[0])) < 0 ||
	     (code = gs_point_transform2fixed(pctm, x1, y2, &lines[2])) < 0
	   )
	  return code;
#else
	if ( (code = gs_point_transform2fixed(pctm, x1, y2, &lines[0])) < 0 ||
	     (code = gs_point_transform2fixed(pctm, x2, y1, &lines[2])) < 0
	   )
	  return code;
#endif
	if ( (code = gx_path_add_lines(ppath, lines, 3)) < 0 )
	  return code;
	return gs_closepath(pgs);
#undef pctm
#undef p2
}

const byte apxRoundRectangle[] = {
  pxaBoundingBox, pxaEllipseDimension, 0, 0
};
px_operator_proc(pxRoundRectanglePath);
int
pxRoundRectangle(px_args_t *par, px_state_t *pxs)
{	return paint_shape(par, pxs, pxRoundRectanglePath);
}

const byte apxRoundRectanglePath[] = {
  pxaBoundingBox, pxaEllipseDimension, 0, 0
};
int
pxRoundRectanglePath(px_args_t *par, px_state_t *pxs)
{	floatp x1, y1, x2, y2;
	real xr = real_value(par->pv[1], 0) * 0.5;
	real yr = real_value(par->pv[1], 1) * 0.5;
	real xd, yd;
	gs_matrix save_ctm;
	gs_state *pgs = pxs->pgs;
	int code;

	set_box_value(x1, y1, x2, y2, par->pv[0]);
	xd = x2 - x1;
	yd = y2 - y1;
	/*
	 * H-P printers give an error if the points are specified out
	 * of order.
	 */
	if ( xd < 0 || yd < 0 )
	  return_error(errorIllegalAttributeValue);
	if ( xr == 0 || yr == 0 )
	  return pxRectanglePath(par, pxs);
	gs_currentmatrix(pgs, &save_ctm);
	gs_translate(pgs, x1, y1);
	if ( xr != yr )
	  { /* Change coordinates so the arcs are circular. */
	    double scale = xr / yr;
	    if ( (code = gs_scale(pgs, scale, 1.0)) < 0 )
	      return code;
	    xd *= yr / xr;
	  }
#define r yr
	/*
	 * Draw the rectangle counter-clockwise on the page, which is
	 * clockwise in user space.  (This may be reversed if the
	 * coordinates are specified in a non-standard order.)
	 */
	if ( (code = gs_moveto(pgs, 0.0, r)) < 0 ||
	     (code = gs_arcn(pgs, r, yd - r, r, 180.0, 90.0)) < 0 ||
	     (code = gs_arcn(pgs, xd - r, yd - r, r, 90.0, 0.0)) < 0 ||
	     (code = gs_arcn(pgs, xd - r, r, r, 0.0, 270.0)) < 0 ||
	     (code = gs_arcn(pgs, r, r, r, 270.0, 180.0)) < 0 ||
	     (code = gs_closepath(pgs)) < 0 ||
	     (code = gs_moveto(pgs, 0.0, 0.0)) < 0
	   )
	  return code;
#undef r
	return gs_setmatrix(pgs, &save_ctm);
}

const byte apxText[] = {
  pxaTextData, 0, pxaXSpacingData, pxaYSpacingData, 0
};
int
pxText(px_args_t *par, px_state_t *pxs)
{	{ int code = px_set_paint(&pxs->pxgs->brush, pxs);
	  if ( code < 0 )
	    return code;
	}
	if ( par->pv[0]->value.array.size != 0 &&
	     pxs->pxgs->brush.type != pxpNull
	   )
	  pxs->have_page = true;
	return px_text(par, pxs, false);
}

const byte apxTextPath[] = {
  pxaTextData, 0, pxaXSpacingData, pxaYSpacingData, 0
};
int
pxTextPath(px_args_t *par, px_state_t *pxs)
{	
    int code = px_set_paint(&pxs->pxgs->pen, pxs);
    if ( code < 0 )
        return code;
    /* NB this should be refactored with pxText (immediately above)
       and it is not a good heuristic for detecting a marked page. */
    if ( par->pv[0]->value.array.size != 0 &&
	 pxs->pxgs->pen.type != pxpNull
       )
        pxs->have_page = true;
    return px_text(par, pxs, true);
}

