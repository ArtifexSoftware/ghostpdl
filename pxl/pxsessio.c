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

/* pxsessio.c */
/* PCL XL session operators */

#include "math_.h"		/* for fabs */
#include "stdio_.h"
#include "string_.h"
#include "pxoper.h"
#include "pxpthr.h"
#include "pxstate.h"
#include "pxfont.h"
#include "pjparse.h" 
#include "gschar.h"
#include "gscoord.h"
#include "gserrors.h"		/* for gs_error_undefined */
#include "gspaint.h"
#include "gsparam.h"
#include "gsstate.h"
#include "gxfixed.h"
#include "gxpath.h"             /* for gx_clip_to_rectangle */
#include "gxdcolor.h"
#include "gxpcolor.h"
#include "gxfcache.h"
#include "gxdevice.h"
#include "gxstate.h"
#include "pjtop.h"
#include "pllfont.h"
#include "plsrgb.h"
#include "pxptable.h"

/* Imported operators */
px_operator_proc(pxCloseDataSource);
px_operator_proc(pxNewPath);
px_operator_proc(pxPopGS);
px_operator_proc(pxPushGS);
px_operator_proc(pxSetHalftoneMethod);
px_operator_proc(pxSetPageDefaultCTM);

/*
 * Define the known paper sizes and unprintable margins.  For convenience,
 * we define this in terms of 300 dpi pixels, in portrait orientation.  This
 * table should obviously be device-dependent.
 */
#define media_size_scale (72.0 / 300.0)
#define m_default 50, 50, 50, 50
#define m_data(ms_enum, mstr, res, width, height)                        \
    {ms_enum, mstr, width * 300 / (res), height * 300 / (res), m_default},
static px_media_t known_media[] = {
  px_enumerate_media(m_data)
};
#undef m_data
#undef m_default

/* Define the mapping from the Measure enumeration to points. */
static const double measure_to_points[] = pxeMeasure_to_points;

/* ---------------- Internal procedures ---------------- */

/* system paper size string (same as pjl paper size) to pxl enumeration type */
static pxeMediaSize_t
px_paper_string_to_media(pjl_envvar_t *paper_str)
{
    /* table to map pjl paper type strings to pxl enums */
    int i;
    for (i = 0; i < countof(known_media); i++) {
	if ( !pjl_compare(paper_str, known_media[i].mname) )
	    return known_media[i].ms_enum;
    }
    /* not found return letter */
    return eLetterPaper;
}


/* return the default media set up in the XL state */
static px_media_t * 
px_get_default_media(px_state_t *pxs)
{
    int i;
    for (i = 0; i < countof(known_media); i++)
	if ( known_media[i].ms_enum == pxs->media_size )
	    return &known_media[i];
    /* shouldn't get here but just in case we return letter. */
    return &known_media[1];
}

void
px_get_default_media_size(px_state_t *pxs, gs_point *pt)
{
    px_media_t *media = px_get_default_media(pxs);
    pt->x = media->width * media_size_scale;
    pt->y = media->height * media_size_scale;
}

/* Finish putting one device parameter. */
static int
px_put1(gx_device *dev, gs_c_param_list *plist, int ecode)
{	int code = ecode;
	if ( code >= 0 )
	  { gs_c_param_list_read(plist);
	    code = gs_putdeviceparams(dev, (gs_param_list *)plist);
	  }
	gs_c_param_list_release(plist);
	return (code == 0 || code == gs_error_undefined ? ecode : code);
}

/* Adjust one scale factor to an integral value if we can. */
static double
px_adjust_scale(double value, double extent)
{	/* If we can make the value an integer with a total error */
	/* of less than 1/2 pixel over the entire page, we do it. */
	double int_value = floor(value + 0.5);

	return (fabs((int_value - value) * extent) < 0.5 ? int_value : value);
}

/* Clean up at the end of a page, but before rendering. */
static void
px_end_page_cleanup(px_state_t *pxs)
{	px_dict_release(&pxs->page_pattern_dict);
	/* Clean up stray gstate information. */
	while ( pxs->pxgs->stack_depth > 0 )
	  pxPopGS(NULL, pxs);
	/* Pop an extra time to mirror the push in BeginPage. */
	pxs->pxgs->stack_depth++;
	pxPopGS(NULL, pxs);
	pxNewPath(NULL, pxs);
        px_purge_pattern_cache(pxs, ePagePattern);
        pxpcl_pagestatereset();
}

/* Purge all */
static bool
purge_all(const gs_memory_t *mem, cached_char *cc, void *dummy)
{	
    return true;
}

/* clears the entire cache */
/* Clean up at the end of a session. */
static void
px_end_session_cleanup(px_state_t *pxs)
{	if ( pxs->data_source_open )
	  pxCloseDataSource(NULL, pxs);
	px_purge_character_cache(pxs);
	px_dict_release(&pxs->session_pattern_dict);
        if (gstate_pattern_cache(pxs->pgs)) {
            (gstate_pattern_cache(pxs->pgs)->free_all)
                (gstate_pattern_cache(pxs->pgs));
            gs_free_object(pxs->memory,
                           gstate_pattern_cache(pxs->pgs)->tiles,
                           "px_end_session_cleanup(tiles)");
            gs_free_object(pxs->memory,
                           gstate_pattern_cache(pxs->pgs),
                           "px_end_session_cleanup(struct)");
            {
                gs_state *pgs = pxs->pgs;
                while (pgs) {
                    gstate_set_pattern_cache(pgs, 0);
                    pgs = gs_state_saved(pgs);
                }
            }
        }
	/* We believe that streams do *not* persist across sessions.... */
	px_dict_release(&pxs->stream_dict);
	/* delete downloaded fonts on end of session */
	px_dict_release(&pxs->font_dict);
        pxpcl_release();
}

/* ---------------- Non-operator procedures ---------------- */

/* Clean up after an error or UEL. */
void px_state_cleanup(px_state_t *pxs)
{	px_end_page_cleanup(pxs);
	px_end_session_cleanup(pxs);
	pxs->have_page = false;
}

void px_purge_character_cache(px_state_t *pxs)
{
    gx_purge_selected_cached_chars(pxs->font_dir, purge_all, pxs);
}

/* ---------------- Operators ---------------- */

const byte apxBeginSession[] = {
  pxaMeasure, pxaUnitsPerMeasure, 0,
  pxaErrorReport, 0
};
int
pxBeginSession(px_args_t *par, px_state_t *pxs)
{	pxs->measure = par->pv[0]->value.i;
	pxs->units_per_measure.x = real_value(par->pv[1], 0);
	pxs->units_per_measure.y = real_value(par->pv[1], 1);

        pxs->stream_level = 0;

	if ( par->pv[2] )
	     pxs->error_report = par->pv[2]->value.i;
	else 
	    pxs->error_report = eNoReporting;
	px_dict_init(&pxs->session_pattern_dict, pxs->memory, px_free_pattern);
	/* Set media parameters to device defaults, in case BeginPage */
	/* doesn't specify valid ones. */
	/* This is obviously device-dependent. */
	/* get the pjl state */
	{
	    pjl_envvar_t *pjl_psize = pjl_proc_get_envvar(pxs->pjls, "paper");
	    /* NB.  We are not sure about the interaction of pjl's
               wide a4 commands so we don't attempt to implement
               it. */
	    /* bool pjl_widea4
	     = pjl_proc_compare(pxs->pjls, pjl_proc_get_envvar(pxs->pjls, "widea4"), "no"); */
	    int pjl_copies
	     = pjl_proc_vartoi(pxs->pjls, pjl_proc_get_envvar(pxs->pjls, "copies"));
	    bool pjl_duplex
	     = pjl_proc_compare(pxs->pjls, pjl_proc_get_envvar(pxs->pjls, "duplex"), "off");
	    bool pjl_bindshort
	     = pjl_proc_compare(pxs->pjls, pjl_proc_get_envvar(pxs->pjls, "binding"), "longedge");
	    bool pjl_manualfeed
	     = pjl_proc_compare(pxs->pjls, pjl_proc_get_envvar(pxs->pjls, "manualfeed"), "off");
	    pxs->media_size = px_paper_string_to_media(pjl_psize);
	    pxs->media_source = (pjl_manualfeed ? eManualFeed : eDefaultSource);
	    pxs->duplex = pjl_duplex;
	    pxs->duplex_page_mode = (pjl_bindshort ? eDuplexHorizontalBinding :
				     eDuplexVerticalBinding);
	    pxs->duplex_back_side = eFrontMediaSide;
	    pxs->copies = pjl_copies;
	    pxs->media_destination = eDefaultDestination;
	    pxs->media_type = eDefaultType;
            pxs->useciecolor =  !pjl_proc_compare(pxs->pjls,
                  pjl_proc_get_envvar(pxs->pjls, "useciecolor"), "on");
	    
	    if (!pjl_proc_compare(pxs->pjls, pjl_proc_get_envvar(pxs->pjls, "orientation"), "LANDSCAPE"))
		pxs->orientation = eLandscapeOrientation;
	    if (!pjl_proc_compare(pxs->pjls, pjl_proc_get_envvar(pxs->pjls, "orientation"), "PORTRAIT"))
		pxs->orientation = ePortraitOrientation;
	    /* NB reverse orientations missing */

	    /* install the built in fonts */
	    if ( pl_load_built_in_fonts(pjl_proc_fontsource_to_path(pxs->pjls, "I"),
					pxs->memory,
					&pxs->builtin_font_dict,
					pxs->font_dir, 
					(int)pxfsInternal, 
					true /* use unicode key names */) < 0 ) {
		dprintf("Fatal error - no resident fonts\n");
		return -1;

	    }
	}
	return 0;
}

const byte apxEndSession[] = {0, 0};
int
pxEndSession(px_args_t *par, px_state_t *pxs)
{	px_end_session_cleanup(pxs);
	if ( pxs->warning_length )
	  return_error(errorWarningsReported);
	return 0;
}

const byte apxBeginPage[] = {
  0, pxaOrientation,
  pxaMediaSource, pxaMediaSize, pxaCustomMediaSize, pxaCustomMediaSizeUnits,
  pxaSimplexPageMode, pxaDuplexPageMode, pxaDuplexPageSide,
  pxaMediaDestination, pxaMediaType, 0
};
int
pxBeginPage(px_args_t *par, px_state_t *pxs)
{	gs_state *pgs = pxs->pgs;
	gx_device *dev = gs_currentdevice(pgs);
	gs_point page_size_pixels;
	gs_matrix points2device;
	bool no_pv_2 = false;

        /* check for 2.1 no parameter special cases */
        {
            int i;
            bool have_params = false;
            for ( i = (par->pv[0] == 0 ? 0 : 1); i<sizeof(par->pv)/sizeof(par->pv[0]); i++ ) {
                if (par->pv[i]) {
                    have_params = true;
                    break;
                }
            }
            if ( have_params == false ) {
                if (par->pv[0]) { 
                    int32_t orientation = par->pv[0]->value.i;
                    if ( orientation < 0 || orientation >= pxeOrientation_next )
                        { px_record_warning("IllegalOrientation", true, pxs);
                            orientation = ePortraitOrientation;
                        }
                    pxs->orientation = (pxeOrientation_t)orientation;
                }
                goto setd;
            }
        }
	/* Check parameter presence for legal combinations. */
	if ( par->pv[2] )
	  { if ( par->pv[3] || par->pv[4] )
	      return_error(errorIllegalAttributeCombination);
	  }
	else if ( par->pv[3] && par->pv[4] )
	  { if ( par->pv[2] )
	      return_error(errorIllegalAttributeCombination);
	  }
	else {
	    pxs->pm = px_get_default_media(pxs);
	    no_pv_2 = true;
	}
	if ( par->pv[5] )
	  { if ( par->pv[6] || par->pv[7] )
	      return_error(errorIllegalAttributeCombination);
	  }
	else if ( par->pv[6] )
	  { if ( par->pv[5] )
	      return_error(errorIllegalAttributeCombination);
	  }


	/* Copy parameters to the PCL XL state. */
	/* For some reason, invalid Orientations only produces a warning. */
	if ( par->pv[0] ) {
	    int32_t orientation = par->pv[0]->value.i;
	    if ( orientation < 0 || orientation >= pxeOrientation_next ) { 
		px_record_warning("IllegalOrientation", true, pxs);
		orientation = ePortraitOrientation;
	    }
	    pxs->orientation = (pxeOrientation_t)orientation;
	}

	if ( par->pv[1] )
	  pxs->media_source = par->pv[1]->value.i;
	if ( par->pv[2] ) {
	    /* default to letter */
	    pxeMediaSize_t ms_enum = eLetterPaper;
	    int i;
	    /* could be an array or enumeration */
	    if ( par->pv[2]->type & pxd_array ) {
		/* it's an array, so convert it to the associated
                   enumeration */
		byte *str = gs_alloc_string(pxs->memory,
					    array_value_size(par->pv[2]) + 1,
					    "pxBeginPage");
		if ( str == 0 )
		    return_error(errorInsufficientMemory);
		/* null terminate */
		memcpy(str, par->pv[2]->value.array.data, array_value_size(par->pv[2]));
		str[array_value_size(par->pv[2])] = '\0';
		ms_enum = px_paper_string_to_media(/* NB */(pjl_envvar_t *)str);
		gs_free_string(pxs->memory, str,
			       array_value_size(par->pv[2]) + 1, "pxBeginPage");

	    } else if ( par->pv[2]->value.i ) { /* it's an enumeration */
		ms_enum = par->pv[2]->value.i;
	    }
	    if ( ms_enum == eDefaultPaperSize ) {
		pxs->pm = px_get_default_media(pxs);
	    }
	    else {
                bool found_media = false;
	        for ( pxs->pm = known_media, i = 0; i < countof(known_media); ++pxs->pm, ++i )
		    if ( pxs->pm->ms_enum == ms_enum ) {
                        found_media = true;
                        break;
                    }
		if ( !found_media ) { /* No match, select default media. */
		   pxs->pm = px_get_default_media(pxs);
		   px_record_warning("IllegalMediaSize", false, pxs);
		}
	    }
media:	    pxs->media_size = pxs->pm->ms_enum;
	    pxs->media_dims.x = pxs->pm->width * media_size_scale;
	    pxs->media_dims.y = pxs->pm->height * media_size_scale;
	    pxs->media_height = pxs->pm->height;
	    pxs->media_width = pxs->pm->width;
	} else if ( no_pv_2 ) {
	    goto media;
	} else { /* custom (!par->pv[2]) */ 
	    double scale = measure_to_points[par->pv[4]->value.i];
	    pxs->media_dims.x = real_value(par->pv[3], 0) * scale;
	    pxs->media_dims.y = real_value(par->pv[3], 1) * scale;
	    /*
	     * Assume the unprintable margins for custom media are the same
	     * as for the default media.  This may not be right.
	     */
	    pxs->pm = px_get_default_media(pxs);
	    pxs->media_height = pxs->media_dims.y / media_size_scale;
	    pxs->media_width = pxs->media_dims.x / media_size_scale;
	}
	if ( par->pv[5] )
	  { pxs->duplex = false;
	  }
	else if ( par->pv[6] )
	  { pxs->duplex = true;
	    pxs->duplex_page_mode = par->pv[6]->value.i;
	    if ( par->pv[7] )
	      pxs->duplex_back_side = (par->pv[7]->value.i == eBackMediaSide);
	  }
	if ( par->pv[8] )
	  pxs->media_destination = par->pv[8]->value.i;
	if ( par->pv[9] )
	  pxs->media_type = par->pv[9]->value.i;

	/* Pass the media parameters to the device. */
setd:	{ gs_memory_t *mem = pxs->memory;
	  gs_c_param_list list;
#define plist ((gs_param_list *)&list)
	  gs_param_float_array fa;
	  float fv[4];
	  int iv;
	  bool bv;
	  int ecode = 0;
	  int code;

	  fa.data = fv;
	  fa.persistent = false;

	  gs_c_param_list_write(&list, mem);
	  iv = pxs->orientation;	/* might not be an int */
	  code = param_write_int(plist, "Orientation", &iv);
	  ecode = px_put1(dev, &list, ecode);

	  gs_c_param_list_write(&list, mem);
	  fv[0] = pxs->media_dims.x;
	  fv[1] = pxs->media_dims.y;
	  fa.size = 2;
	  code = param_write_float_array(plist, ".MediaSize", &fa);
	  ecode = px_put1(dev, &list, ecode);
	  gs_c_param_list_write(&list, mem);

	  /* Set the mis-named "Margins" (actually the offset on the page) */
	  /* to zero. */
	  gs_c_param_list_write(&list, mem);
	  fv[0] = 0;
	  fv[1] = 0;
	  fa.size = 2;
	  code = param_write_float_array(plist, "Margins", &fa);
	  ecode = px_put1(dev, &list, ecode);

	  iv = pxs->media_source;	/* might not be an int */
	  if ( iv < 0 || iv >= pxeMediaSource_next )
	    px_record_warning("IllegalMediaSource", false, pxs);
	  else
	    { gs_c_param_list_write(&list, mem);
	      code = param_write_int(plist, ".MediaSource", &iv);
	      ecode = px_put1(dev, &list, ecode);
	    }

	  gs_c_param_list_write(&list, mem);
	  code = param_write_bool(plist, "Duplex", &pxs->duplex);
	  ecode = px_put1(dev, &list, ecode);

	  gs_c_param_list_write(&list, mem);
	  bv = pxs->duplex_page_mode == eDuplexHorizontalBinding;
	  code = param_write_bool(plist, "Tumble", &bv);
	  ecode = px_put1(dev, &list, ecode);

	  gs_c_param_list_write(&list, mem);
	  bv = !pxs->duplex_back_side;
	  code = param_write_bool(plist, "FirstSide", &bv);
	  ecode = px_put1(dev, &list, ecode);

	  gs_c_param_list_write(&list, mem);
	  iv = pxs->media_destination;	/* might not be an int */
	  code = param_write_int(plist, ".MediaDestination", &iv);
	  ecode = px_put1(dev, &list, ecode);

	  gs_c_param_list_write(&list, mem);
	  iv = pxs->media_type;		/* might not be an int */
	  code = param_write_int(plist, ".MediaType", &iv);
	  ecode = px_put1(dev, &list, ecode);

	  /*
	   * We aren't sure what to do if the device rejects the parameter
	   * value....
	   */
	  switch ( ecode )
	    {
	    case 1:
	      code = gs_setdevice(pgs, dev);
	      if ( code < 0 )
		return code;
	    case 0:
	      break;
	    default:
	      return_error(errorIllegalAttributeValue);
	    }
#undef plist
	}
	{ int code;

	  px_initgraphics(pxs);
	  gs_currentmatrix(pgs, &points2device);
	  gs_dtransform(pgs, pxs->media_dims.x, pxs->media_dims.y,
			&page_size_pixels);
	  { /*
	     * Put the origin at the upper left corner of the page;
	     * also account for the orientation.
	     */
	    gs_matrix orient;

	    orient.xx = orient.xy = orient.yx = orient.yy =
	      orient.tx = orient.ty = 0;
	    switch ( pxs->orientation )
	      {
	      case eDefaultOrientation:
	      case ePortraitOrientation:
		code = gs_translate(pgs, 0.0, pxs->media_dims.y);
		orient.xx = 1, orient.yy = -1;
		break;
	      case eLandscapeOrientation:
		code = 0;
		orient.xy = 1, orient.yx = 1;
		break;
	      case eReversePortrait:
		code = gs_translate(pgs, pxs->media_dims.x, 0);
		orient.xx = -1, orient.yy = 1;
		break;
	      case eReverseLandscape:
		code = gs_translate(pgs, pxs->media_dims.x, pxs->media_dims.y);
		orient.xy = -1, orient.yx = -1;
		break;
	      default:			/* can't happen */
		return_error(errorIllegalAttributeValue);
	      }
	    if ( code < 0 ||
		 (code = gs_concat(pgs, &orient)) < 0
	       )
	      return code;
	  }
	  { /* Scale according to session parameters. */
	    /* If we can make the scale integral safely, we do. */
	    double scale = measure_to_points[pxs->measure];
	    gs_matrix mat;

	    if ( (code = gs_scale(pgs, scale / pxs->units_per_measure.x,
				  scale / pxs->units_per_measure.y)) < 0
				  )
	      return code;
	    gs_currentmatrix(pgs, &mat);
	    mat.xx = px_adjust_scale(mat.xx, page_size_pixels.x);
	    mat.xy = px_adjust_scale(mat.xy, page_size_pixels.y);
	    mat.yx = px_adjust_scale(mat.yx, page_size_pixels.x);
	    mat.yy = px_adjust_scale(mat.yy, page_size_pixels.y);
	    gs_setmatrix(pgs, &mat);
	    pxs->initial_matrix = mat;
	  }
	  {
	      /* note we don't expect errors here since the
                 coordinates are functions of media sizes known at
                 compile time */
	      gs_rect page_bbox, device_page_bbox;
	      gs_fixed_rect fixed_bbox;
	      /* XL requires a 1/6" border to print correctly, this
                 will set up the border as long as we do not exceed
                 the boundary of the hardware margins.  If the
                 engine's border is larger than 1/6" the XL output
                 will be clipped by the engine and will not behave as
                 expected */
	      page_bbox.p.x = max(dev->HWMargins[0], pxs->pm->m_left * media_size_scale);
	      page_bbox.p.y = max(dev->HWMargins[1], pxs->pm->m_top * media_size_scale);
	      page_bbox.q.x = (pxs->media_width * media_size_scale) -
		  max(dev->HWMargins[2], pxs->pm->m_bottom * media_size_scale);
	      page_bbox.q.y = (pxs->media_height * media_size_scale) - 
		  max(dev->HWMargins[3], pxs->pm->m_right * media_size_scale);
	      gs_bbox_transform(&page_bbox, &points2device, &device_page_bbox);
	      /* clip to rectangle takes fixed coordinates */
	      fixed_bbox.p.x = float2fixed(device_page_bbox.p.x);
	      fixed_bbox.p.y = float2fixed(device_page_bbox.p.y);
	      fixed_bbox.q.x = float2fixed(device_page_bbox.q.x);
	      fixed_bbox.q.y = float2fixed(device_page_bbox.q.y);
	      gx_clip_to_rectangle(pgs, &fixed_bbox);
	      pxs->pxgs->initial_clip_rect = fixed_bbox;
	  }
	}
	{ /*
	     * Set the default halftone method.  We have to do this here,
	     * rather than earlier, so that the origin is set correctly.
	     */
	    px_args_t args;
	    px_value_t device_matrix;
            memset(args.pv, 0, sizeof(args.pv));
	    args.pv[1] = &device_matrix;	/* DeviceMatrix */
	      device_matrix.type = pxd_scalar | pxd_ubyte;
	      device_matrix.value.i = eDeviceBest;
	    pxSetHalftoneMethod(&args, pxs);
	  }
	/* Initialize other parts of the PCL XL state. */
	px_dict_init(&pxs->page_pattern_dict, pxs->memory, px_free_pattern);
	gs_erasepage(pgs);
	pxs->have_page = false;
	/* Make sure there is a legitimate halftone installed. */
	{ int code = px_set_halftone(pxs);
	  if ( code < 0 )
	    return code;
	}
	/*
	 * Do a gsave so we can be sure to get rid of all page-related
	 * state at the end of the page, but make sure PopGS doesn't pop
	 * this state from the stack.
	 */
	{ int code = pxPushGS(NULL, pxs);
	  if ( code < 0 )
	    return code;
	  pxs->pxgs->stack_depth--;
	  return code;
	}
}


int
pxBeginPageFromPassthrough(px_state_t *pxs)
{ 
    int code;
    gs_state *pgs = pxs->pgs;
    gx_device *dev = gs_currentdevice(pgs);
    gs_point page_size_pixels;
    gs_matrix points2device;

    px_initgraphics(pxs);
    gs_currentmatrix(pgs, &points2device);
    gs_dtransform(pgs, pxs->media_dims.x, pxs->media_dims.y,
		  &page_size_pixels);
    { /*
       * Put the origin at the upper left corner of the page;
       * also account for the orientation.
       */
	gs_matrix orient;

	orient.xx = orient.xy = orient.yx = orient.yy =
	    orient.tx = orient.ty = 0;
	switch ( pxs->orientation )
	    {
	    case eDefaultOrientation:
	    case ePortraitOrientation:
		code = gs_translate(pgs, 0.0, pxs->media_dims.y);
		orient.xx = 1, orient.yy = -1;
		break;
	    case eLandscapeOrientation:
		code = 0;
		orient.xy = 1, orient.yx = 1;
		break;
	    case eReversePortrait:
		code = gs_translate(pgs, pxs->media_dims.x, 0);
		orient.xx = -1, orient.yy = 1;
		break;
	    case eReverseLandscape:
		code = gs_translate(pgs, pxs->media_dims.x, pxs->media_dims.y);
		orient.xy = -1, orient.yx = -1;
		break;
	    default:			/* can't happen */
		return_error(errorIllegalAttributeValue);
	    }
	if ( code < 0 ||
	     (code = gs_concat(pgs, &orient)) < 0
	     )
	    return code;
    }
    { /* Scale according to session parameters. */
	/* If we can make the scale integral safely, we do. */
	double scale = measure_to_points[pxs->measure];
	gs_matrix mat;
	
	if ( (code = gs_scale(pgs, scale / pxs->units_per_measure.x,
			      scale / pxs->units_per_measure.y)) < 0
	     )
	    return code;
	gs_currentmatrix(pgs, &mat);
	mat.xx = px_adjust_scale(mat.xx, page_size_pixels.x);
	mat.xy = px_adjust_scale(mat.xy, page_size_pixels.y);
	mat.yx = px_adjust_scale(mat.yx, page_size_pixels.x);
	mat.yy = px_adjust_scale(mat.yy, page_size_pixels.y);
	gs_setmatrix(pgs, &mat);
	pxs->initial_matrix = mat;
    }
    {
	/* note we don't expect errors here since the
	   coordinates are functions of media sizes known at
	   compile time */
	gs_rect page_bbox, device_page_bbox;
	gs_fixed_rect fixed_bbox;
	/* XL requires a 1/6" border to print correctly, this
	   will set up the border as long as we do not exceed
	   the boundary of the hardware margins.  If the
	   engine's border is larger than 1/6" the XL output
	   will be clipped by the engine and will not behave as
	   expected */
	page_bbox.p.x = max(dev->HWMargins[0], pxs->pm->m_left * media_size_scale);
	page_bbox.p.y = max(dev->HWMargins[1], pxs->pm->m_top * media_size_scale);
	page_bbox.q.x = (pxs->media_width * media_size_scale) -
	    max(dev->HWMargins[2], pxs->pm->m_bottom * media_size_scale);
	page_bbox.q.y = (pxs->media_height * media_size_scale) - 
	    max(dev->HWMargins[3], pxs->pm->m_right * media_size_scale);
	gs_bbox_transform(&page_bbox, &points2device, &device_page_bbox);
	/* clip to rectangle takes fixed coordinates */
	fixed_bbox.p.x = float2fixed(device_page_bbox.p.x);
	fixed_bbox.p.y = float2fixed(device_page_bbox.p.y);
	fixed_bbox.q.x = float2fixed(device_page_bbox.q.x);
	fixed_bbox.q.y = float2fixed(device_page_bbox.q.y);
	gx_clip_to_rectangle(pgs, &fixed_bbox);
	pxs->pxgs->initial_clip_rect = fixed_bbox;
    }

    pxs->have_page = true;
    return 0;
}

const byte apxEndPage[] = {
  0,
  pxaPageCopies, 0
};
int
pxEndPage(px_args_t *par, px_state_t *pxs)
{	px_end_page_cleanup(pxs);
	(*pxs->end_page)(pxs, (par->pv[0] ? par->pv[0]->value.i : pxs->copies), 1);
	pxs->have_page = false;
	return 0;
}
/* The default end-page procedure just calls the device procedure. */
int
px_default_end_page(px_state_t *pxs, int num_copies, int flush)
{	return gs_output_page(pxs->pgs, num_copies, flush);
}

const byte apxVendorUnique[] = {
    pxaVUExtension, 0, pxaVUAttr1, pxaVUAttr2, pxaVUAttr3, 0
};

/** we do NOTHING with the vendor unique command.
 * it is undocumented, but appears that it contains the sames color commands as the
 * XL 2.1 spec.  This is based on only finding it in hpclj 4500 driver output.
 * of course HP denys that the 4500 supports XL.  
 */
int
pxVendorUnique(px_args_t *par, px_state_t *pxs)
{	
    return 0;
}



const byte apxComment[] = {
  0,
  pxaCommentData, 0
};
int
pxComment(px_args_t *par, px_state_t *pxs)
{	return 0;
}

const byte apxOpenDataSource[] = {
  pxaSourceType, pxaDataOrg, 0, 0
};
int
pxOpenDataSource(px_args_t *par, px_state_t *pxs)
{	if ( pxs->data_source_open )
	  return_error(errorDataSourceNotClosed);
	pxs->data_source_open = true;
	pxs->data_source_big_endian =
	  par->pv[1]->value.i == eBinaryHighByteFirst;
	return 0;
}

const byte apxCloseDataSource[] = {0, 0};
int
pxCloseDataSource(px_args_t *par, px_state_t *pxs)
{	pxs->data_source_open = false;
	return 0;
}

