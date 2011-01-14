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

/* pxgstate.c */
/* PCL XL graphics state operators */

#include "math_.h"
#include "memory_.h"
#include "stdio_.h"			/* std.h + NULL */
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "pxoper.h"
#include "pxstate.h"
#include "pxparse.h"
#include "gdebug.h"
#include "gsstate.h"
#include "gscoord.h"
#include "gxcspace.h"			/* must precede gscolor2.h */
#include "gscie.h"
#include "gsicc_manage.h"
#include "gsimage.h"
#include "gspath.h"
#include "gspath2.h"
#include "gsrop.h"
#include "gxpath.h"
#include "gzstate.h"
#include "gscolor2.h"
#include "plsrgb.h"
#include "pxptable.h"

/*
 * There is an apparent bug in the LJ5 and LJ6MP firmware that causes
 * SetClipIntersect with even/odd mode and exterior region to behave the
 * same as SetClipReplace.  To emulate this bug, uncomment the following
 * #define.
 */
#define CLIP_INTERSECT_EXTERIOR_REPLACES
/*
 * H-P printers apparently have a maximum dash pattern length of 20.
 * Our library doesn't impose a limit, but we may as well emulate the
 * H-P limit here for error checking and Genoa test compatibility.
 */
#define MAX_DASH_ELEMENTS 20

/* Imported operators */
px_operator_proc(pxRectanglePath);
/* Forward references */
px_operator_proc(pxSetClipIntersect);


/* Imported color space types */
extern const gs_color_space_type gs_color_space_type_Indexed; /* gscolor2.c */
extern const gs_color_space_type gs_color_space_type_Pattern; /* gspcolor.c */

/* Define 'client procedures' for copying the PCL XL state. */
/* We export the reference count adjustment procedures for the sake of */
/* the state setting code in pxink.c. */
void
px_paint_rc_adjust(px_paint_t *ppt, int delta, gs_memory_t *mem)
{	if ( ppt->type == pxpPattern )
	  { /*
	     * There is no public API for adjusting the reference count of a
	     * gs_client_color, and even the private API requires having a
	     * color space available.  We'll need to fix this properly
	     * sooner or later, but for the moment, fake it.
	     */
	    gs_color_space cspace;

	    /*
	     * Even though this is a colored Pattern, and hence does have a
	     * base space, we set has_base_space to false to prevent the
	     * adjust_color_count procedure from trying to call the
	     * adjustment procedures for the base space.
	     */
	    cspace.type = &gs_color_space_type_Pattern;
	    cspace.params.pattern.has_base_space = false;
	    (*cspace.type->adjust_color_count)(&ppt->value.pattern.color,
					       &cspace, delta);
	    rc_adjust_only(ppt->value.pattern.pattern, delta,
			   "px_paint_rc_adjust");
	  }
}
void
px_gstate_rc_adjust(px_gstate_t *pxgs, int delta, gs_memory_t *mem)
{	px_paint_rc_adjust(&pxgs->pen, delta, mem);
	px_paint_rc_adjust(&pxgs->brush, delta, mem);
}
static void *
px_gstate_client_alloc(gs_memory_t *mem)
{	
    px_gstate_t *pxgs = 
        (px_gstate_t *)gs_alloc_bytes(mem, 
                                      sizeof(px_gstate_t),
                                      "px_gstate_alloc");

    if ( pxgs == 0 )
        return 0;
    /* Initialize reference-counted pointers and other pointers */
    /* needed to establish invariants. */
    pxgs->memory = mem;
    pxgs->halftone.thresholds.data = 0;
    pxgs->halftone.thresholds.size = 0;
    pxgs->dither_matrix.data = 0;
    pxgs->dither_matrix.size = 0;
    pxgs->brush.type = pxpNull;
    pxgs->pen.type = pxpNull;
    px_dict_init(&pxgs->temp_pattern_dict, mem, px_free_pattern);
    return pxgs;
}
static int
px_gstate_client_copy_for(void *to, void *from, gs_state_copy_reason_t reason)
{
#define pxfrom ((px_gstate_t *)from)
#define pxto ((px_gstate_t *)to)
	px_gstate_rc_adjust(pxfrom, 1, pxfrom->memory);
	px_gstate_rc_adjust(pxto, -1, pxto->memory);
	/*
	 * In the context of the PCL XL code, this routine may be called for
	 * gsave, grestore, or gstate (copying the gstate for makepattern
	 * or Pattern rendering).  See gxstate.h for details of the 'from'
	 * and 'to' arguments for each of these cases.
	 *
	 * We have some structures that belong to the individual gstates for
	 * storage management purposes.  Currently these are:
	 *	dither_matrix, temp_pattern_dict
	 * px_gstate_client_alloc initializes them to an empty state.
	 * For gsave and gstate, the new current gstate or copied gstate
	 * respectively should have the empty structure.  For grestore,
	 * we want to swap the structures, because we will then free the
	 * saved gstate (and release the structure that was in the current
	 * gstate before the grestore).
	 *
	 * halftone.thresholds is a different special case.  We need to
	 * copy it for gsave and gstate, and free it on grestore.
	 */
	{ gs_string tmat;
	  gs_string thtt;
	  pl_dict_t tdict;
	  gs_string *phtt;

	  tmat = pxto->dither_matrix;
	  thtt = pxto->halftone.thresholds;
	  tdict = pxto->temp_pattern_dict;
	  *pxto = *pxfrom;
	  switch ( reason )
	    {
	    case copy_for_gstate:
	      /* Just put back the (empty) 'to' structures. */
	      pxto->dither_matrix = tmat;
	      pxto->temp_pattern_dict = tdict;
	      phtt = &pxto->halftone.thresholds;
	      goto copy;
	    case copy_for_gsave:
	      /* Swap the structures, but set the parent of the new */
	      /* (empty) dictionary to the old one. */
	      pxfrom->dither_matrix = tmat;
	      pxfrom->temp_pattern_dict = tdict;
	      pl_dict_set_parent(&pxfrom->temp_pattern_dict,
				 &pxto->temp_pattern_dict);
	      phtt = &pxfrom->halftone.thresholds;
copy:	      if ( phtt->data )
		{ byte *str = gs_alloc_string(pxfrom->memory, phtt->size,
					      "px_gstate_client_copy(thresholds)");
		  if ( str == 0 )
		    return_error(errorInsufficientMemory);
		  memcpy(str, phtt->data, phtt->size);
		  phtt->data = str;
		}
	      break;
	    default:		/* copy_for_grestore */
	      /* Swap the structures. */
	      pxfrom->dither_matrix = tmat;
	      pxfrom->halftone.thresholds = thtt;
	      pxfrom->temp_pattern_dict = tdict;
	    }
	}
	return 0;
#undef pxfrom
#undef pxto
}
static void
px_gstate_client_free(void *old, gs_memory_t *mem)
{	px_gstate_t *pxgs = old;

	px_dict_release(&pxgs->temp_pattern_dict);
	if ( pxgs->halftone.thresholds.data )
            gs_free_string(mem, (byte *)pxgs->halftone.thresholds.data,
			 pxgs->halftone.thresholds.size,
			 "px_gstate_free(halftone.thresholds)");
	if ( pxgs->dither_matrix.data )
            gs_free_string(mem, (byte *)pxgs->dither_matrix.data,
			 pxgs->dither_matrix.size,
			 "px_gstate_free(dither_matrix)");
	px_gstate_rc_adjust(old, -1, mem);
	gs_free_object(mem, old, "px_gstate_free");
}
static const gs_state_client_procs px_gstate_procs = {
  px_gstate_client_alloc,
  0,				/* copy -- superseded by copy_for */
  px_gstate_client_free,
  px_gstate_client_copy_for
};

/* ---------------- Initialization ---------------- */

/* Allocate a px_gstate_t. */
px_gstate_t *
px_gstate_alloc(gs_memory_t *mem)
{	px_gstate_t *pxgs = px_gstate_client_alloc(mem);

	if ( pxgs == 0 )
	  return 0;
	pxgs->stack_depth = 0;
	px_gstate_init(pxgs, NULL);
	return pxgs;
}

/* Initialize a px_gstate_t. */
void
px_gstate_init(px_gstate_t *pxgs, gs_state *pgs)
{	pxgs->halftone.method = eDeviceBest;
	pxgs->halftone.set = false;
	pxgs->halftone.origin.x = pxgs->halftone.origin.y = 0;
	/* halftone.thresholds was initialized at alloc time */
	px_gstate_reset(pxgs);
	if ( pgs )
	    gs_state_set_client(pgs, pxgs, &px_gstate_procs, false);
}

/* Initialize the graphics state for a page. */
/* Note that this takes a px_state_t, not a px_gstate_t. */
int
px_initgraphics(px_state_t *pxs)
{	gs_state *pgs = pxs->pgs;

	px_gstate_reset(pxs->pxgs);
	gs_initgraphics(pgs);

	gs_setfilladjust(pgs, 0.5, 0.5);

	{ gs_point inch;
	  float dpi;

	  gs_dtransform(pgs, 72.0, 0.0, &inch);
	  dpi = fabs(inch.x) + fabs(inch.y);

	  /* Stroke adjustment leads to anomalies at high resolutions. */
	  if ( dpi >= 150 )
	    gs_setstrokeadjust(pgs, false);

	  /* We need the H-P interpretation of zero-length lines */
	  /* and of using bevel joins for the segments of flattened curves. */
	  gs_setdotlength(pgs, 72.0 / 300, true);
	}
	/* we always clamp coordinates hp does not seem to report
           limit checks in paths */
	gs_setlimitclamp(pgs, true);
	return 0;
}

/* Reset a px_gstate_t, initially or at the beginning of a page. */
void
px_gstate_reset(px_gstate_t *pxgs)
{	pxgs->brush.type = pxpGray;
	pxgs->brush.value.gray = 0;
	pxgs->char_angle = 0;
	pxgs->char_bold_value = 0;
	pxgs->char_scale.x = pxgs->char_scale.y = 1;
	pxgs->char_shear.x = pxgs->char_shear.y = 0;
	/* The order of transforms is arbitrary, */
	/* because the transforms are all no-ops. */
	pxgs->char_transforms[0] = pxct_rotate;
	pxgs->char_transforms[1] = pxct_shear;
	pxgs->char_transforms[2] = pxct_scale;
	pxgs->char_sub_mode = eNoSubstitution;
	pxgs->clip_mode = eNonZeroWinding;
	pxgs->color_space = eRGB;
	pxgs->palette.size = 0;
	pxgs->palette.data = 0;
	pxgs->palette_is_shared = false;
	pxgs->base_font = 0;
	/* halftone_method was set above. */
	pxgs->fill_mode = eNonZeroWinding;
	pxgs->dashed = false;
	pxgs->pen.type = pxpGray;
	pxgs->pen.value.gray = 0;
	/* temp_pattern_dict was set at allocation time. */
	gs_make_identity(&pxgs->text_ctm);
	pxgs->char_matrix_set = false;
	pxgs->symbol_map = 0;
	pxgs->writing_mode = eHorizontal;
}

/* initial clip region note, we don't use gs_initclip() because we
   give special handling for the XL 1/6" border */
int
px_initclip(px_state_t *pxs) 
{
    return gs_initclip(pxs->pgs);
}

static bool
px_is_currentcolor_pattern(const gs_state *pgs)
{
    return (gs_color_space_num_components(gs_currentcolorspace(pgs)) < 1);
}

/* Set up the color space information for a bitmap image or pattern. */
int
px_image_color_space(gs_image_t *pim,
  const px_bitmap_params_t *params, const gs_string *palette,
  const gs_state *pgs)
{	

    int depth = params->depth;
    gs_color_space *pbase_pcs = NULL;
    gs_color_space *pcs = NULL;
    bool cie_space = false;
    int code = 0;
    switch ( params->color_space ) {
    case eGray:
	pbase_pcs = gs_cspace_new_DeviceGray(pgs->memory);
#ifdef ICCBRANCH
        pbase_pcs->cmm_icc_profile_data = pgs->icc_manager->default_gray;
        rc_increment(pbase_pcs->cmm_icc_profile_data);
#endif
	break;
    case eRGB:
        pbase_pcs = gs_cspace_new_DeviceRGB(pgs->memory);
#ifdef ICCBRANCH
        pbase_pcs->cmm_icc_profile_data = pgs->icc_manager->default_rgb;
        rc_increment(pbase_pcs->cmm_icc_profile_data);
#endif
        break;
    case eSRGB:
    case eCRGB:
        if ( pl_cspace_init_SRGB(&pbase_pcs, pgs) < 0 )
            /* should not happen */
            return_error(errorInsufficientMemory);
        cie_space = true;
#ifdef ICCBRANCH
        pbase_pcs->cmm_icc_profile_data = pgs->icc_manager->default_rgb;
        rc_increment(pbase_pcs->cmm_icc_profile_data);
#endif
	break;
    default:
	return_error(errorIllegalAttributeValue);
    }
    if (pbase_pcs == NULL)
        return_error(errorInsufficientMemory);

    if ( params->indexed ) { 
        pcs = gs_cspace_alloc(pgs->memory, &gs_color_space_type_Indexed);
        if ( pcs == NULL ) {
            /* free the base space also */
            rc_decrement(pbase_pcs, "px_image_color_space");
            return_error(errorInsufficientMemory);
        }
	pcs->base_space = pbase_pcs;
	pcs->params.indexed.hival = (1 << depth) - 1;
	pcs->params.indexed.lookup.table.size = palette->size;
        {
            uint  n = palette->size;
            byte *p = gs_alloc_string(pgs->memory, n,
                           "px_image_color_space(palette)");
            if ( p == 0 ) {
                rc_decrement(pbase_pcs, "px_image_color_space");
                return_error(errorInsufficientMemory);

            }
            memcpy(p, palette->data, n);
            pcs->params.indexed.lookup.table.data = p;
        }
	pcs->params.indexed.use_proc = 0;
    } else {
        pcs = pbase_pcs;
    }
    gs_image_t_init(pim, pcs);
    pim->ColorSpace = pcs;
    pim->BitsPerComponent = depth;
    if ( params->indexed )
	pim->Decode[1] = (1 << depth) - 1;
    /* NB - this needs investigation */
    if (cie_space && !px_is_currentcolor_pattern(pgs)) {
        code = pl_setSRGBcolor((gs_state *)pgs, 0.0, 0.0, 0.0);
    }
    return code;
}

/* Check the setting of the clipping region. */
#define check_clip_region(par, pxs)\
  if ( par->pv[0]->value.i == eExterior && pxs->pxgs->clip_mode != eEvenOdd )\
    return_error(errorClipModeMismatch)

/* Record the most recent character transformation. */
static void
add_char_transform(px_gstate_t *pxgs, px_char_transform_t trans)
{	/* Promote this transformation to the head of the list. */
	if ( pxgs->char_transforms[2] == trans )
	  pxgs->char_transforms[2] = pxgs->char_transforms[1],
	    pxgs->char_transforms[1] = pxgs->char_transforms[0];
	else if ( pxgs->char_transforms[1] == trans )
	  pxgs->char_transforms[1] = pxgs->char_transforms[0];
	pxgs->char_transforms[0] = trans;
	pxgs->char_matrix_set = false;
}

/* ---------------- Operators ---------------- */

const byte apxPopGS[] = {0, 0};
int
pxPopGS(px_args_t *par, px_state_t *pxs)
{	gs_state *pgs = pxs->pgs;
	px_gstate_t *pxgs = pxs->pxgs;
	int code;

	/*
	 * Even though the H-P documentation says that a PopGS with an
	 * empty stack is illegal, the implementations apparently simply
	 * do nothing in this case.
	 */
	if ( pxgs->stack_depth == 0 )
	  return 0;
	if ( pxgs->palette.data && !pxgs->palette_is_shared ) {
            gs_free_string(pxs->memory, (byte *)pxgs->palette.data,
			 pxgs->palette.size, "pxPopGS(palette)");
	  pxgs->palette.data = 0;
	}
	px_purge_pattern_cache(pxs, eTempPattern);
	code = gs_grestore(pgs);
	pxs->pxgs = gs_state_client_data(pgs);
	return code;
}

const byte apxPushGS[] = {0, 0};
int
pxPushGS(px_args_t *par, px_state_t *pxs)
{	gs_state *pgs = pxs->pgs;
	int code = gs_gsave(pgs);
	px_gstate_t *pxgs;

	if ( code < 0 )
	  return code;
	pxgs = pxs->pxgs = gs_state_client_data(pgs);
	if ( pxgs->palette.data )
	  pxgs->palette_is_shared = true;
	++(pxgs->stack_depth);
	return code;
}

/* To restore the interpreters to default state we assemble the
   following pxl commands and execute them as a stream.

ubyte 2 ColorSpace
SetColorSpace

ubyte_array [ uint16 3
0000   00 00 00                                           ...
] RGBColor
SetBrushSource

uint16 0 CharAngle
SetCharAngle

SetPageDefaultCTM
ubyte_array [ uint16 3
0000   00 00 00                                           ...

] RGBColor
SetPenSource

ubyte 1 PenWidth
SetPenWidth

ubyte 0 TxMode
SetSourceTxMode

ubyte 0 TxMode
SetPatternTxMode

ubyte 252 ROP3
SetROP

ubyte 0 LineCapStyle
SetLineCap

ubyte 0 SolidLine
SetLineDash

ubyte 0 LineJoinStyle
SetLineJoin

uint16 10 MiterLength
SetMiterLimit

real32_xy 1.000000 1.000000 CharScale
SetCharScale

real32_xy 0.000000 0.000000 CharShear
SetCharShear

ubyte 0 DeviceMatrix
SetHalftoneMethod

ubyte 0 ClipMode
SetClipMode

SetClipToPage

NewPath

ubyte 0 FillMode
SetFillMode

*/

/* assembled (little endian) stream */
static const byte pxSetDefaultGSstr[] = {
    192,2,248,3,106,200,193,3,0,0,0,0,248,11,99,193,
    0,0,248,161,100,116,200,193,3,0,0,0,0,248,11,121,192,1,248,75,122,192,
    0,248,45,124,192,0,248,45,120,192,252,248,44,123,192,0,248,71,113,192,
    0,248,78,112,192,0,248,72,114,193,10,0,248,73,115,213,0,0,128,63,0,0,128,63,
    248,164,101,213,0,0,0,0,0,0,0,0,248,165,102,192,0,248,33,109,192,0,248,
    84,127,105,133,192,0,248,70,110
};


const byte apxSetDefaultGS[] = {0, 0};
int
pxSetDefaultGS(px_args_t *par, px_state_t *pxs)
{
    px_parser_state_t *pst = par->parser;
    px_parser_state_t st;
    stream_cursor_read r;
    int code;

    st.memory = pxs->memory;
    px_process_init(&st, false /* big_endian */);

    st.macro_state = pst->macro_state | ptsExecStream;
    r.ptr = pxSetDefaultGSstr - 1;
    r.limit = r.ptr + sizeof(pxSetDefaultGSstr);

    /* we don't expect this to fail */
    code = px_process(&st, pxs, &r);

    pst->macro_state = st.macro_state & ~ptsExecStream;
    return code;
}

const byte apxSetClipReplace[] = {
  pxaClipRegion, 0, 0
};
int
pxSetClipReplace(px_args_t *par, px_state_t *pxs)
{	int code;

	check_clip_region(par, pxs);
	if ( (code = px_initclip(pxs)) < 0 )
	  return code;
	return pxSetClipIntersect(par, pxs);
}

const byte apxSetCharAngle[] = {
  pxaCharAngle, 0, 0
};
int
pxSetCharAngle(px_args_t *par, px_state_t *pxs)
{	real angle = real_value(par->pv[0], 0);
	px_gstate_t *pxgs = pxs->pxgs;

	if ( angle != pxgs->char_angle ||
	     pxgs->char_transforms[0] != pxct_rotate
	   )
	  { pxgs->char_angle = angle;
	    add_char_transform(pxgs, pxct_rotate);
	  }
	return 0;
}

/* confusion in the 3.0 spec - this argument identifier WritingMode
   173 is now being used by SelectPCLFont */
const byte apxSetCharAttributes[] = {
    pxaWritingMode, 0, 0
};
int
pxSetCharAttributes(px_args_t *par, px_state_t *pxs)
{
    pxeWritingMode_t arg = par->pv[0]->value.i;
    if ( arg != pxs->pxgs->writing_mode ) {
        pxs->pxgs->writing_mode = arg;
        px_purge_character_cache(pxs);
    }
    return 0;
}

const byte apxSetCharScale[] = {
  pxaCharScale, 0, 0
};
int
pxSetCharScale(px_args_t *par, px_state_t *pxs)
{	real x_scale = real_value(par->pv[0], 0);
	real y_scale = real_value(par->pv[0], 1);
	px_gstate_t *pxgs = pxs->pxgs;

	if ( x_scale != pxgs->char_scale.x || y_scale != pxgs->char_scale.y ||
	     pxgs->char_transforms[0] != pxct_scale
	   )
	  { pxgs->char_scale.x = x_scale;
	    pxgs->char_scale.y = y_scale;
	    add_char_transform(pxgs, pxct_scale);
	  }
	return 0;
}

const byte apxSetCharShear[] = {
  pxaCharShear, 0, 0
};

 /* experiments indicate the character shearing operands are
    clamped after range checking though it is not documented in
    the HP manual. */

#define SHEAR_LIMIT 16383.0

int
pxSetCharShear(px_args_t *par, px_state_t *pxs)
{	real x_shear = real_value(par->pv[0], 0);
	real y_shear = real_value(par->pv[0], 1);
	px_gstate_t *pxgs = pxs->pxgs;

        x_shear = x_shear > SHEAR_LIMIT ? SHEAR_LIMIT : x_shear;
        y_shear = y_shear > SHEAR_LIMIT ? SHEAR_LIMIT : y_shear;

	if ( x_shear != pxgs->char_shear.x || y_shear != pxgs->char_shear.y ||
	     pxgs->char_transforms[0] != pxct_shear
	   )
	  { pxgs->char_shear.x = x_shear;
	    pxgs->char_shear.y = y_shear;
	    add_char_transform(pxgs, pxct_shear);
	  }
	return 0;
}

const byte apxSetClipIntersect[] = {
  pxaClipRegion, 0, 0
};
int
pxSetClipIntersect(px_args_t *par, px_state_t *pxs)
{	gs_state *pgs = pxs->pgs;
	pxeClipRegion_t clip_region  = par->pv[0]->value.i;
	int code;

	check_clip_region(par, pxs);
	/*
	 * The discussion of ClipMode and ClipRegion in the published
	 * specification is confused and incorrect.  Based on testing with
	 * the LaserJet 6MP, we believe that ClipMode works just like
	 * PostScript clip and eoclip, and that ClipRegion applies only to
	 * the *current* path (i.e., support for "outside clipping" in the
	 * library is unnecessary).  If we had only known....
	 */
#if 0
	gs_setclipoutside(pgs, false);
#endif
	if ( clip_region == eExterior )
	  { /*
	     * We know clip_mode is eEvenOdd, so we can complement the
	     * region defined by the current path by just adding a
	     * rectangle that encloses the entire page.
	     */
	    gs_rect bbox;

	    code = gs_gsave(pgs);
	    if ( code < 0 )
	      return code;
	    px_initclip(pxs);
	    if ( (code = gs_clippath(pgs)) < 0 ||
		 (code = gs_pathbbox(pgs, &bbox)) < 0
	       )
	      DO_NOTHING;
	    gs_grestore(pgs);
	    if ( code < 0 ||
		 (code = gs_rectappend(pgs, &bbox, 1)) < 0
	       )
	      return code;
#ifdef CLIP_INTERSECT_EXTERIOR_REPLACES
	    px_initclip(pxs);
#endif
	  }
	code =
	  (pxs->pxgs->clip_mode == eEvenOdd ? gs_eoclip(pgs) : gs_clip(pgs));
	if ( code < 0 )
	  return code;
#if 0
	gs_setclipoutside(pgs, clip_region == eExterior);
#endif
	return gs_newpath(pgs);
}

const byte apxSetClipRectangle[] = {
  pxaClipRegion, pxaBoundingBox, 0, 0
};
int
pxSetClipRectangle(px_args_t *par, px_state_t *pxs)
{	px_args_t args;
	gs_state *pgs = pxs->pgs;
	int code;

	check_clip_region(par, pxs);
	gs_newpath(pgs);
	args.pv[0] = par->pv[1];
	if ( (code = pxRectanglePath(&args, pxs)) < 0 )
	  return code;
	return pxSetClipReplace(par, pxs);
}

const byte apxSetClipToPage[] = {0, 0};
int
pxSetClipToPage(px_args_t *par, px_state_t *pxs)
{	gs_newpath(pxs->pgs);
	return px_initclip(pxs);
}

const byte apxSetCursor[] = {
  pxaPoint, 0, 0
};
int
pxSetCursor(px_args_t *par, px_state_t *pxs)
{	return gs_moveto(pxs->pgs, real_value(par->pv[0], 0),
			 real_value(par->pv[0], 1));
}

const byte apxSetCursorRel[] = {
  pxaPoint, 0, 0
};
int
pxSetCursorRel(px_args_t *par, px_state_t *pxs)
{	return gs_rmoveto(pxs->pgs, real_value(par->pv[0], 0),
			  real_value(par->pv[0], 1));;
}

/* SetHalftoneMethod is in pxink.c */

const byte apxSetFillMode[] = {
  pxaFillMode, 0, 0
};
int
pxSetFillMode(px_args_t *par, px_state_t *pxs)
{	pxs->pxgs->fill_mode = par->pv[0]->value.i;
	return 0;
}

/* SetFont is in pxfont.c */

const byte apxSetLineDash[] = {
  0, pxaLineDashStyle, pxaDashOffset, pxaSolidLine, 0
};
int
pxSetLineDash(px_args_t *par, px_state_t *pxs)
{	px_gstate_t *pxgs = pxs->pxgs;
	gs_state *pgs = pxs->pgs;

	if ( par->pv[0] )
	  { float pattern[MAX_DASH_ELEMENTS * 2];
	    uint size = par->pv[0]->value.array.size;
	    real offset = (par->pv[1] ? real_value(par->pv[1], 0) : 0.0);
	    int code;

	    if ( par->pv[2] )
	      return_error(errorIllegalAttributeCombination);
	    if ( size > MAX_DASH_ELEMENTS )
	      return_error(errorIllegalArraySize);

	    /*
	     * The H-P documentation gives no clue about what a negative
	     * dash pattern element is supposed to do.  The H-P printers
	     * apparently interpret it as drawing a line backwards in the
	     * current direction (which may extend outside the original
	     * subpath) with the caps inside the line instead of outside; a
	     * dash pattern with a negative total length crashes the LJ 6MP
	     * firmware so badly the printer has to be power cycled!  We
	     * take a different approach here: we compensate for negative
	     * elements by propagating them to adjacent positive ones.  This
	     * doesn't produce quite the same output as the H-P printers do,
	     * but this is such an obscure feature that we don't think it's
	     * worth the trouble to emulate exactly.
	     */
	    { uint orig_size = size;
	      int i;

	      /* Acquire the pattern, duplicating it if the length is odd. */
	      if ( size & 1 )
		size <<= 1;
	      for ( i = 0; i < size; ++i )
		pattern[i] = real_elt(par->pv[0], i % orig_size);
	      /* Get rid of negative draws. */
	      if ( pattern[0] < 0 )
		offset -= pattern[0],
		  pattern[size - 1] += pattern[0],
		  pattern[1] += pattern[0],
		  pattern[0] = -pattern[0];
	      for ( i = 2; i < size; i += 2 )
		if ( pattern[i] < 0 )
		  pattern[i - 1] += pattern[i],
		    pattern[i + 1] += pattern[i],
		    pattern[i] = -pattern[i];
	      /*
	       * Now propagate negative skips iteratively.  Since each step
	       * decreases either the remaining total of negative skips or
	       * the total number of pattern elements, the process is
	       * guaranteed to terminate.
	       */
elim:	      for ( i = 0; i < size; i += 2 )
	        { float draw = pattern[i], skip = pattern[i + 1];
		  int inext, iprev;
		  float next, prev;

		  if ( skip > 0 )
		    continue;
		  if ( size == 2 ) /* => i == 0 */
		    { if ( (pattern[0] = draw + skip) <= 0 )
			pattern[0] = -pattern[0];
		      pattern[1] = 0;
		      break;
		    }
		  inext = (i == size - 2 ? 0 : i + 2);
		  next = pattern[inext];
		  /*
		   * Consider the sequence D, -S, E, where D and E are draws
		   * and -S is a negative skip.  If S <= D, replace the 3
		   * elements with D - S + E.
		   */
		  if ( draw + skip >= 0 )
		    { next += draw + skip;
		      goto shrink;
		    }
		  /*
		   * Otherwise, let T be the skip preceding D.  Replace T
		   * with T + D - S.  If S > E, replace D, -S, E with E, S -
		   * (D + E), D; otherwise, replace D, -S, E with E.  In
		   * both cases, net progress has occurred.
		   */
		  iprev = (i == 0 ? size - 1 : i - 1);
		  prev = pattern[iprev];
		  pattern[iprev] = prev + draw + skip;
		  if ( -skip > next )
		    { pattern[i] = next;
		      pattern[i + 1] = -(skip + draw + next);
		      pattern[i + 2] = draw;
		      goto elim;
		    }
shrink:		  if ( inext == 0 )
		    { offset += next - pattern[0];
		      pattern[0] = next;
		    }
		  else
		    { pattern[i] = next;
		      memmove(&pattern[i + 1], &pattern[i + 3],
			      (size - (i + 3)) * sizeof(pattern[0]));
		    }
		  size -= 2;
		  goto elim;
		}
	    }
	    code = gs_setdash(pgs, pattern, size, offset);
	    if ( code < 0 )
	      return code;
	    /* patterns with 0 total skip length are treated as solid
               line pattern on the LJ6 */
	    { 
		bool skips_have_length = false;
		int i;
		for ( i = 0; i < size; i += 2 )
		    if ( pattern[i + 1] != 0 ) {
			skips_have_length = true;
			break;
		    }
		if ( skips_have_length == false ) {
		    pxgs->dashed = false;
		    return gs_setdash(pgs, NULL, 0, 0.0);
		}
		pxgs->dashed = (size != 0);
	    }
	    gs_currentmatrix(pgs, &pxgs->dash_matrix);
	    return 0;
	  }
	else if ( par->pv[2] )
	  { if ( par->pv[1] )
	      return_error(errorIllegalAttributeCombination);
	    pxgs->dashed = false;
	    return gs_setdash(pgs, NULL, 0, 0.0);
	  }
	else
	  return_error(errorMissingAttribute);
}

const byte apxSetLineCap[] = {
  pxaLineCapStyle, 0, 0
};
int
pxSetLineCap(px_args_t *par, px_state_t *pxs)
{	static const gs_line_cap cap_map[] = pxeLineCap_to_library;
	return gs_setlinecap(pxs->pgs, cap_map[par->pv[0]->value.i]);
}

const byte apxBeginUserDefinedLineCap[] = { 0, 0 };
int
pxBeginUserDefinedLineCap(px_args_t *par, px_state_t *pxs)
{

    dprintf("undocumented\n" );
    return 0;
}


const byte apxEndUserDefinedLineCap[] = { 0, 0 };
int
pxEndUserDefinedLineCap(px_args_t *par, px_state_t *pxs)
{

    dprintf("undocumented\n" );
    return 0;
}

const byte apxSetLineJoin[] = {
  pxaLineJoinStyle, 0, 0
};
int
pxSetLineJoin(px_args_t *par, px_state_t *pxs)
{	static const gs_line_join join_map[] = pxeLineJoin_to_library;
	return gs_setlinejoin(pxs->pgs, join_map[par->pv[0]->value.i]);
}

const byte apxSetMiterLimit[] = {
  pxaMiterLength, 0, 0
};
int
pxSetMiterLimit(px_args_t *par, px_state_t *pxs)
{	float limit = real_value(par->pv[0], 0);

	if ( limit == 0 )
	  { /*
	     * H-P printers interpret this to mean use the default value
	     * of 10, even though nothing in the documentation says or
	     * implies this.
	     */
	    limit = 10;
	  }
	else
	  { /* PCL XL, but not the library, allows limit values <1. */
	    if ( limit < 1 ) limit = 1;
	  }
	return gs_setmiterlimit(pxs->pgs, limit);
}

const byte apxSetPageDefaultCTM[] = {0, 0};
int
pxSetPageDefaultCTM(px_args_t *par, px_state_t *pxs)
{	gs_make_identity(&pxs->pxgs->text_ctm);
	return gs_setmatrix(pxs->pgs, &pxs->initial_matrix);
}

const byte apxSetPageOrigin[] = {
  pxaPageOrigin, 0, 0
};
int
pxSetPageOrigin(px_args_t *par, px_state_t *pxs)
{	return gs_translate(pxs->pgs, real_value(par->pv[0], 0),
			    real_value(par->pv[0], 1));
}

const byte apxSetPageRotation[] = {
  pxaPageAngle, 0, 0
};
int
pxSetPageRotation(px_args_t *par, px_state_t *pxs)
{	/* Since the Y coordinate of user space is inverted, */
	/* we must negate rotation angles. */
	real angle = -real_value(par->pv[0], 0);
	int code = gs_rotate(pxs->pgs, angle);

	if ( code < 0 )
	  return code;
	/* Post-multiply the text CTM by the rotation matrix. */
	{ gs_matrix rmat;
	  px_gstate_t *pxgs = pxs->pxgs;

	  gs_make_rotation(angle, &rmat);
	  gs_matrix_multiply(&pxgs->text_ctm, &rmat, &pxgs->text_ctm);
	}
	return 0;
}

const byte apxSetPageScale[] = {
  0, pxaPageScale, pxaMeasure, pxaUnitsPerMeasure, 0
};

int
pxSetPageScale(px_args_t *par, px_state_t *pxs)
{	
    int code;
    real sx = 1;
    real sy = 1;
    static const real units_conversion_table[3][3] = {
        { 1, 25.4, 254 },     /* in -> in, mill, 1/10 mill */
        { 0.0394, 1, 10 },    /* mill -> in, mill, 1/10 mill */
        { 0.00394, .1, 1 }    /* 1/10 mill -> in, mill, 1/10 mill */ 
    };

    /* measuure and units of measure.  Actually session user units
       divided by new user unit, bizarre. */
    if ( par->pv[1] && par->pv[2] ) {
        /* new user measure */
        real nux = real_value(par->pv[2], 0);
        real nuy = real_value(par->pv[2], 1);
        if ( nux != 0 && nuy != 0 ) {
            /* new measure */
            pxeMeasure_t mt = par->pv[1]->value.i;
            /* convert to session units */
            real factor = units_conversion_table[pxs->measure][mt];
            real sux = nux * factor;
            real suy = nuy * factor;
            sx = pxs->units_per_measure.x / sux;
            sy = pxs->units_per_measure.y / suy;
            /* check for overflow.  NB we should do a better job here */
            if ( fabs(sx) > 1000.0 ) {
                dprintf2("warning probable overflow avoided for scaling factors %f %f\n", 
			 sx, sy );
                sx = sy = 1;
            }
        }
    } else if ( par->pv[0] ) { /* page scale */
        sx = real_value(par->pv[0], 0);
        sy = real_value(par->pv[0], 1);
    }
    code = gs_scale(pxs->pgs, sx, sy);
    if ( code < 0 )
        return code;
    /* Post-multiply the text CTM by the scale matrix. */
    { 
        gs_matrix smat;
        px_gstate_t *pxgs = pxs->pxgs;
        gs_make_scaling(sx, sy, &smat);
        gs_matrix_multiply(&pxgs->text_ctm, &smat, &pxgs->text_ctm);
    }
    return 0;
}

const byte apxSetPaintTxMode[] = {
  pxaTxMode, 0, 0
};
int
pxSetPaintTxMode(px_args_t *par, px_state_t *pxs)
{	gs_settexturetransparent(pxs->pgs, par->pv[0]->value.i == eTransparent);
	return 0;
}

const byte apxSetPenWidth[] = {
  pxaPenWidth, 0, 0
};
int
pxSetPenWidth(px_args_t *par, px_state_t *pxs)
{	return gs_setlinewidth(pxs->pgs, real_value(par->pv[0], 0));
}

const byte apxSetROP[] = {
  pxaROP3, 0, 0
};
int
pxSetROP(px_args_t *par, px_state_t *pxs)
{	gs_setrasterop(pxs->pgs, (gs_rop3_t)(par->pv[0]->value.i));
	return 0;
}

const byte apxSetSourceTxMode[] = {
  pxaTxMode, 0, 0
};
int
pxSetSourceTxMode(px_args_t *par, px_state_t *pxs)
{	gs_setsourcetransparent(pxs->pgs, par->pv[0]->value.i == eTransparent);
	return 0;
}

const byte apxSetCharBoldValue[] = {
  pxaCharBoldValue, 0, 0
};
int
pxSetCharBoldValue(px_args_t *par, px_state_t *pxs)
{	
    float arg = real_value(par->pv[0], 0);
    pxs->pxgs->char_bold_value = arg;
    return 0;
}

const byte apxSetClipMode[] = {
  pxaClipMode, 0, 0
};
int
pxSetClipMode(px_args_t *par, px_state_t *pxs)
{	pxs->pxgs->clip_mode = par->pv[0]->value.i;
	return 0;
}

const byte apxSetPathToClip[] = {0, 0};
int
pxSetPathToClip(px_args_t *par, px_state_t *pxs)
{	return gs_clippath(pxs->pgs);
}

const byte apxSetCharSubMode[] = {
  pxaCharSubModeArray, 0, 0
};
int
pxSetCharSubMode(px_args_t *par, px_state_t *pxs)
{	/*
	 * It isn't clear from the documentation why the attribute is an
	 * array rather than just a Boolean, but we have to assume there
	 * is some reason for this.
	 */
	const px_value_t *psubs = par->pv[0];
        pxeCharSubModeArray_t arg;

	if ( psubs->value.array.size != 1 ||
	     psubs->value.array.data[0] >= pxeCharSubModeArray_next
	   )
	  return_error(errorIllegalAttributeValue);
        arg = psubs->value.array.data[0];
        if (pxs->pxgs->char_sub_mode != arg) {
            pxs->pxgs->char_sub_mode = arg;
            px_purge_character_cache(pxs);
        }
	return 0;
}
