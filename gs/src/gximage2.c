/* Copyright (C) 1989, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* gximage2.c */
/* General monochrome image rendering */
#include "gx.h"
#include "memory_.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxfixed.h"
#include "gxarith.h"
#include "gxmatrix.h"
#include "gsccolor.h"
#include "gspaint.h"
#include "gsutil.h"
#include "gxdevice.h"
#include "gxcmap.h"
#include "gxdcolor.h"
#include "gxistate.h"
#include "gzpath.h"
#include "gxdevmem.h"
#include "gdevmem.h"			/* for mem_mono_device */
#include "gxcpath.h"
#include "gximage.h"
#include "gzht.h"

/* ------ Strategy procedure ------ */

/* We can bypass X clipping for portrait monochrome images. */
private irender_proc(image_render_mono);
private irender_proc_t
image_strategy_mono(gx_image_enum *penum)
{	/* Use slow loop for imagemask with a halftone, */
	/* or for a non-default logical operation. */
	penum->slow_loop =
	  (penum->masked && !color_is_pure(&penum->icolor1)) ||
	  penum->use_rop;
	if ( penum->spp == 1 )
	  { if ( !(penum->slow_loop || penum->posture != image_portrait) )
	      penum->clip_image &= ~(image_clip_xmin | image_clip_xmax);
	    if_debug0('b', "[b]render=mono\n");
	    /* Precompute values needed for rasterizing. */
	    penum->dxx =
	      float2fixed(penum->matrix.xx + fixed2float(fixed_epsilon) / 2);
	    return image_render_mono;
	  }
	return 0;
}

void
gs_gximage2_init(gs_memory_t *mem)
{	image_strategies.mono = image_strategy_mono;
}

/* ------ Rendering procedure ------ */

/* Rendering procedure for the general case of displaying a */
/* monochrome image, dealing with multiple bit-per-sample images, */
/* general transformations, and arbitrary single-component */
/* color spaces (DeviceGray, CIEBasedA, Separation, Indexed). */
/* This procedure handles a single scan line. */
private int
image_render_mono(gx_image_enum *penum, const byte *buffer, int data_x,
  uint w, int h, gx_device *dev)
{	const gs_imager_state *pis = penum->pis;
	gs_logical_operation_t lop = penum->log_op;
	const int masked = penum->masked;
	const gs_color_space *pcs;		/* only set for non-masks */
	cs_proc_remap_color((*remap_color));	/* ditto */
	gs_client_color cc;
	const gx_color_map_procs *cmap_procs = gx_device_cmap_procs(dev);
	cmap_proc_gray((*map_gray)) = cmap_procs->map_gray;
	gx_device_color *pdevc = &penum->icolor1; /* color for masking */
	/* Make sure the cache setup matches the graphics state. */
	/* Also determine whether all tiles fit in the cache. */
	int tiles_fit = gx_check_tile_cache(pis);
#define image_set_gray(sample_value)\
   { pdevc = &penum->clues[sample_value].dev_color;\
     if ( !color_is_set(pdevc) )\
      { if ( penum->device_color )\
	  (*map_gray)(byte2frac(sample_value), pdevc, pis, dev, gs_color_select_source);\
	else\
	  { decode_sample(sample_value, cc, 0);\
	    (*remap_color)(&cc, pcs, pdevc, pis, dev, gs_color_select_source);\
	  }\
      }\
     else if ( !color_is_pure(pdevc) )\
      { if ( !tiles_fit )\
         { code = gx_color_load_select(pdevc, pis, dev, gs_color_select_source);\
	   if ( code < 0 ) return code;\
	 }\
      }\
   }
	gx_dda_fixed_point next;	/* (y not used in fast loop) */
	gx_dda_step_fixed dxx2, dxx3, dxx4; /* (not used in all loops) */
	register const byte *psrc = buffer + data_x;
	const byte *endp = psrc + w;
	const byte *stop = endp;
	fixed xrun;			/* x at start of run */
	register byte run;		/* run value */
	int htrun =			/* halftone run value */
	  (masked ? 255 : -2);
	int code = 0;

	if ( h == 0 )
	  return 0;
	next = penum->dda.pixel0;
	xrun = dda_current(next.x);
	if ( !masked )
	  { pcs = penum->pcs;		/* (may not be set for masks) */
	    remap_color = pcs->type->remap_color;
	  }
	run = *psrc;
	/* Find the last transition in the input. */
	{ byte last = stop[-1];
	  while ( stop > psrc && stop[-1] == last )
	    --stop;
	}
	if ( penum->slow_loop || penum->posture != image_portrait )
	  { /* Skewed, rotated, or imagemask with a halftone. */
	    fixed yrun;
	    const fixed pdyx = dda_current(penum->dda.row.x) - penum->cur.x;
	    const fixed pdyy = dda_current(penum->dda.row.y) - penum->cur.y;
	    dev_proc_fill_parallelogram((*fill_pgram)) =
	      dev_proc(dev, fill_parallelogram);

#define xl dda_current(next.x)
#define ytf dda_current(next.y)
	    yrun = ytf;
	    if ( masked )
	      { pdevc = &penum->icolor1;
	        code = gx_color_load(pdevc, pis, dev);
	        if ( code < 0 )
		  return code;
		if ( stop <= psrc )
		  goto last;
		if ( penum->posture == image_portrait )
		  { /* We don't have to worry about the Y DDA, */
		    /* and the fill regions are rectangles. */
		    /* Calculate multiples of the DDA step. */
		    dxx2 = next.x.step;
		    dda_step_add(dxx2, next.x.step);
		    dxx3 = dxx2;
		    dda_step_add(dxx3, next.x.step);
		    dxx4 = dxx3;
		    dda_step_add(dxx4, next.x.step);
		    for ( ; ; )
		      { /* Skip a run of zeros. */
			while ( !psrc[0] )
			  if ( !psrc[1] )
			    { if ( !psrc[2] )
			        { if ( !psrc[3] )
				    { psrc += 4;
				      dda_state_next(next.x.state, dxx4);
				      continue;
				    }
				  psrc += 3;
				  dda_state_next(next.x.state, dxx3);
				  break;
				}
			      psrc += 2;
			      dda_state_next(next.x.state, dxx2);
			      break;
			    }
			  else
			    { ++psrc;
			      dda_next(next.x);
			      break;
			    }
		        xrun = xl;
			if ( psrc >= stop )
			  break;
			for ( ; *psrc; ++psrc )
			  dda_next(next.x);
			code = (*fill_pgram)(dev, xrun, yrun, xl - xrun,
					     fixed_0, fixed_0, pdyy,
					     pdevc, lop);
			if ( code < 0 )
			  return code;
			if ( psrc >= stop )
			  break;
		      }
		  }
		else
		 for ( ; ; )
		  { for ( ; !*psrc; ++psrc )
		      { dda_next(next.x);
		        dda_next(next.y);
		      }
		    yrun = ytf;
		    xrun = xl;
		    if ( psrc >= stop )
		      break;
		    for ( ; *psrc; ++psrc )
		      { dda_next(next.x);
			dda_next(next.y);
		       }
		    code = (*fill_pgram)(dev, xrun, yrun, xl - xrun,
					 ytf - yrun, pdyx, pdyy, pdevc, lop);
		    if ( code < 0 )
		      return code;
		    if ( psrc >= stop )
		      break;
		  }
	      }
	    else if ( penum->posture == image_portrait ||
		      penum->posture == image_landscape
		    )
	      {		/* Not masked, and we can fill runs quickly. */
		 if ( stop <= psrc )
		   goto last;
		 for ( ; ; )
		  { if ( *psrc != run )
		      { if ( run != htrun )
			  { htrun = run;
			    image_set_gray(run);
			  }
		        code = (*fill_pgram)(dev, xrun, yrun, xl - xrun,
					     ytf - yrun, pdyx, pdyy,
					     pdevc, lop);
			if ( code < 0 )
			  return code;
		        yrun = ytf;
		        xrun = xl;
		        if ( psrc >= stop )
			  break;
			run = *psrc;
		      }
		    psrc++;
		    dda_next(next.x);
		    dda_next(next.y);
		  }
	      }
	    else		/* not masked */
	      { /* Since we have to check for the end after every pixel */
		/* anyway, we may as well avoid the last-run code. */
		stop = endp;
		for ( ; ; )
		  { /* We can't skip large constant regions quickly, */
		    /* because this leads to rounding errors. */
		    /* Just fill the region between xrun and xl. */
		    psrc++;
		    if ( run != htrun )
		      { htrun = run;
		        image_set_gray(run);
		      }
		    code = (*fill_pgram)(dev, xrun, yrun, xl - xrun,
					 ytf - yrun, pdyx, pdyy, pdevc, lop);
		    if ( code < 0 )
		      return code;
		    yrun = ytf;
		    xrun = xl;
		    if ( psrc > stop )
		      { --psrc;
		        break;
		      }
		    run = psrc[-1];
		    dda_next(next.x);
		    dda_next(next.y);		/* harmless if no skew */
		  }
	      }
	    /* Fill the last run. */
last:	    if ( stop < endp && (*stop || !masked) )
	      { if ( !masked )
		  { image_set_gray(*stop);
		  }
	        dda_advance(next.x, endp - stop);
	        dda_advance(next.y, endp - stop);
		code = (*fill_pgram)(dev, xrun, yrun, xl - xrun,
				     ytf - yrun, pdyx, pdyy, pdevc, lop);
	      }
#undef xl
#undef ytf
	  }
	else			/* fast loop */
	  { /* No skew, and not imagemask with a halftone. */
	    const fixed adjust = penum->adjust;
	    const fixed dxx = penum->dxx;
	    fixed xa = (dxx >= 0 ? adjust : -adjust);
	    const int yt = penum->yci, iht = penum->hci;
	    dev_proc_fill_rectangle((*fill_proc)) =
	      dev_proc(dev, fill_rectangle);
	    dev_proc_strip_tile_rectangle((*tile_proc)) =
	      dev_proc(dev, strip_tile_rectangle);
	    dev_proc_copy_mono((*copy_mono_proc)) =
	      dev_proc(dev, copy_mono);
	    /*
	     * If each pixel is likely to fit in a single halftone tile,
	     * determine that now (tile_offset = offset of row within tile).
	     * Don't do this for band devices; they handle halftone fills
	     * more efficiently than copy_mono.
	     */
	    int bstart;
	    int phase_x;
	    int tile_offset =
	      ((*dev_proc(dev, get_band))(dev, yt, &bstart) == 0 ?
	       gx_check_tile_size(pis,
				  fixed2int_ceiling(any_abs(dxx) + (xa << 1)),
				  yt, iht, gs_color_select_source, &phase_x) :
	       -1);
	    int xmin = fixed2int_pixround(penum->clip_outer.p.x);
	    int xmax = fixed2int_pixround(penum->clip_outer.q.x);

#define xl dda_current(next.x)
	    /* Fold the adjustment into xrun and xl, */
	    /* including the +0.5-epsilon for rounding. */
	    xrun = xrun - xa + (fixed_half - fixed_epsilon);
	    dda_translate(next.x, xa + (fixed_half - fixed_epsilon));
	    xa <<= 1;
	    /* Calculate multiples of the DDA step. */
	    dxx2 = next.x.step;
	    dda_step_add(dxx2, next.x.step);
	    dxx3 = dxx2;
	    dda_step_add(dxx3, next.x.step);
	    dxx4 = dxx3;
	    dda_step_add(dxx4, next.x.step);
	    if ( stop > psrc )
	     for ( ; ; )
	      {	/* Skip large constant regions quickly, */
		/* but don't slow down transitions too much. */
skf:		if ( psrc[0] == run )
		  { if ( psrc[1] == run )
		      { if ( psrc[2] == run )
			  { if ( psrc[3] == run )
			      { psrc += 4;
				dda_state_next(next.x.state, dxx4);
				goto skf;
			      }
			    else
			      { psrc += 4;
				dda_state_next(next.x.state, dxx3);
			      }
			  }
		        else
			  { psrc += 3;
			    dda_state_next(next.x.state, dxx2);
			  }
		      }
		    else
		      { psrc += 2;
			dda_next(next.x);
		      }
		  }
		else
		  psrc++;
		{ /* Now fill the region between xrun and xl. */
		  int xi = fixed2int_var(xrun);
		  int wi = fixed2int_var(xl) - xi;
		  int xei, tsx;
		  const gx_strip_bitmap *tile;

		  if ( wi <= 0 )
		    { if ( wi == 0 ) goto mt;
		      xi += wi, wi = -wi;
		    }
		  if ( (xei = xi + wi) > xmax || xi < xmin )
		    { /* Do X clipping */
		      if ( xi < xmin )
			wi -= xmin - xi, xi = xmin;
		      if ( xei > xmax )
			wi -= xei - xmax;
		      if ( wi <= 0 )
			goto mt;
		    }
		  switch ( run )
		    {
		    case 0:
		      if ( masked ) goto mt;
		      if ( !color_is_pure(&penum->icolor0) ) goto ht;
		      code = (*fill_proc)(dev, xi, yt, wi, iht,
					  penum->icolor0.colors.pure);
		      break;
		    case 255:		/* just for speed */
		      if ( !color_is_pure(&penum->icolor1) ) goto ht;
		      code = (*fill_proc)(dev, xi, yt, wi, iht,
					  penum->icolor1.colors.pure);
		      break;
		    default:
ht:		      /* Use halftone if needed */
		      if ( run != htrun )
			{ image_set_gray(run);
			  htrun = run;
			}
		      /* We open-code gx_fill_rectangle, */
		      /* because we've done some of the work for */
		      /* halftone tiles in advance. */
		      if ( color_is_pure(pdevc) )
			{ code = (*fill_proc)(dev, xi, yt, wi, iht,
					      pdevc->colors.pure);
			}
		      else if ( !color_is_binary_halftone(pdevc) )
			{ code =
			    gx_fill_rectangle_device_rop(xi, yt, wi, iht,
							 pdevc, dev, lop);
			}
		      else if ( tile_offset >= 0 &&
			        (tile = &pdevc->colors.binary.b_tile->tiles,
				 (tsx = (xi + phase_x) % tile->rep_width) + wi <= tile->size.x)
			      )
			{ /* The pixel(s) fit(s) in a single (binary) tile. */
			  byte *row = tile->data + tile_offset;
			  code = (*copy_mono_proc)
			    (dev, row, tsx, tile->raster, gx_no_bitmap_id,
			     xi, yt, wi, iht,
			     pdevc->colors.binary.color[0],
			     pdevc->colors.binary.color[1]);
			}
		      else
			{ code = (*tile_proc)(dev,
					      &pdevc->colors.binary.b_tile->tiles,
					      xi, yt, wi, iht,
					      pdevc->colors.binary.color[0],
					      pdevc->colors.binary.color[1],
					      pdevc->phase.x, pdevc->phase.y);
			}
		    }
		    if ( code < 0 ) return code;
mt:		    xrun = xl - xa;	/* original xa << 1 */
		    if ( psrc > stop )
		      { --psrc;
		        break;
		      }
		    run = psrc[-1];
		}
		dda_next(next.x);
	      }
	    /* Fill the last run. */
	    if ( *stop != 0 || !masked )
	      { int xi = fixed2int_var(xrun);
	        int wi, xei;

		dda_advance(next.x, endp - stop);
		wi = fixed2int_var(xl) - xi;
		if ( wi <= 0 )
		  { if ( wi == 0 ) goto lmt;
		    xi += wi, wi = -wi;
		  }
		if ( (xei = xi + wi) > xmax || xi < xmin )
		  { /* Do X clipping */
		    if ( xi < xmin )
		      wi -= xmin - xi, xi = xmin;
		    if ( xei > xmax )
		      wi -= xei - xmax;
		    if ( wi <= 0 )
		      goto lmt;
		  }
		image_set_gray(*stop);
		code = gx_fill_rectangle_device_rop(xi, yt, wi, iht,
						    pdevc, dev, lop);
lmt:	        ;
	      }
	    }
#undef xl
	return (code < 0 ? code : 1);
}
