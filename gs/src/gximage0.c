/* Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/* gximage0.c */
/* Generic image enumeration and cleanup */
#include "gx.h"
#include "memory_.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxcpath.h"
#include "gximage.h"

/* Process the next piece of an image */
int
gx_default_image_data(gx_device *dev,
  void *info, const byte **planes, int data_x, uint raster, int height)
{	gx_image_enum *penum = info;
	int y = penum->y;
	int y_end = min(y + height, penum->rect.h);
	int width_spp = penum->rect.w * penum->spp;
	int nplanes = penum->num_planes;
	uint bcount =			/* bytes per data row */
	  ((penum->rect.w + data_x) * penum->spp / nplanes * penum->bps
	   + 7) >> 3;
	fixed adjust = penum->adjust;
	ulong offset = 0;
	int ignore_data_x;
	int code;

	if ( height == 0 )
	  return 0;

	/* Set up the clipping and/or RasterOp device if needed. */

	if ( penum->clip_dev )
	   {	gx_device_clip *cdev = penum->clip_dev;
		cdev->target = dev;
		dev = (gx_device *)cdev;
	   }
	if ( penum->rop_dev )
	  {	gx_device_rop_texture *rtdev = penum->rop_dev;
		((gx_device_forward *)rtdev)->target = dev;
		dev = (gx_device *)rtdev;
	  }

	/* Now render complete rows. */

	for ( ; penum->y < y_end; offset += raster, penum->y++ )
	   {	int px;
		/*
		 * Normally, we unpack the data into the buffer, but if
		 * there is only one plane and we don't need to expand the
		 * input samples, we may use the data directly.
		 */
	        int sourcex = data_x;
		const byte *buffer =
		  (*penum->unpack)(penum->buffer, &sourcex,
				   planes[0] + offset, data_x, bcount,
				   &penum->map[0].table, penum->spread);

		for ( px = 1; px < nplanes; ++px )
		  (*penum->unpack)(penum->buffer + (px << penum->log2_xbytes),
				   &ignore_data_x, planes[px] + offset,
				   data_x, bcount,
				   &penum->map[px].table, penum->spread);
#ifdef DEBUG
		if ( gs_debug_c('B') )
		  { int i, n = width_spp;
		    dputs("[B]row:");
		    for ( i = 0; i < n; i++ )
		      dprintf1(" %02x", buffer[i]);
		    dputs("\n");
		  }
#endif
		penum->cur.x = dda_current(penum->dda.row.x);
		dda_next(penum->dda.row.x);
		penum->cur.y = dda_current(penum->dda.row.y);
		dda_next(penum->dda.row.y);
		if ( !penum->interpolate )
		  switch ( penum->posture )
		    {
		    case image_portrait:
		      { /* Precompute integer y and height, */
			/* and check for clipping. */
			fixed yc = penum->cur.y,
			  yn = dda_current(penum->dda.row.y);

			if ( yn < yc )
			  { fixed temp = yn; yn = yc; yc = temp; }
			yc -= adjust;
			if ( yc >= penum->clip_outer.q.y ) goto mt;
			yn += adjust;
			if ( yn <= penum->clip_outer.p.y ) goto mt;
			penum->yci = fixed2int_pixround(yc);
			penum->hci = fixed2int_pixround(yn) - penum->yci;
			if ( penum->hci == 0 ) goto mt;
		      }	break;
		    case image_landscape:
		      { /* Check for no pixel centers in x. */
			fixed xc = penum->cur.x,
			  xn = dda_current(penum->dda.row.x);

			if ( xn < xc )
			  { fixed temp = xn; xn = xc; xc = temp; }
			xc -= adjust;
			if ( xc >= penum->clip_outer.q.x ) goto mt;
			xn += adjust;
			if ( xn <= penum->clip_outer.p.x ) goto mt;
			penum->xci = fixed2int_pixround(xc);
			penum->wci = fixed2int_pixround(xn) - penum->xci;
			if ( penum->wci == 0 ) goto mt;
		      }	break;
		    case image_skewed:
		      ;
		    }
		dda_translate(penum->dda.pixel0.x,
			      penum->cur.x - penum->prev.x);
		dda_translate(penum->dda.pixel0.y,
			      penum->cur.y - penum->prev.y);
		penum->prev = penum->cur;
		code = (*penum->render)(penum, buffer, sourcex, width_spp, 1,
					dev);
		if ( code < 0 )
		  goto err;
mt:		;
	   }
	if ( penum->y < penum->rect.h )
	  { code = 0;
	    goto out;
	  }
	/* End of data.  Render any left-over buffered data. */
	switch ( penum->posture )
	  {
	  case image_portrait:
	    {	fixed yc = dda_current(penum->dda.row.y);
		penum->yci = fixed2int_rounded(yc - adjust);
		penum->hci = fixed2int_rounded(yc + adjust) - penum->yci;
	    }	break;
	  case image_landscape:
	    {	fixed xc = dda_current(penum->dda.row.x);
		penum->xci = fixed2int_rounded(xc - adjust);
		penum->wci = fixed2int_rounded(xc + adjust) - penum->xci;
	    }	break;
	  case image_skewed:		/* pacify compilers */
	    ;
	  }
	dda_translate(penum->dda.pixel0.x, penum->cur.x - penum->prev.x);
	dda_translate(penum->dda.pixel0.y, penum->cur.y - penum->prev.y);
	code = (*penum->render)(penum, NULL, 0, width_spp, 0, dev);
	if ( code < 0 )
	  { penum->y--;
	    goto err;
	  }
	code = 1;
	goto out;
err:	/* Error or interrupt, restore original state. */
	while ( penum->y > y )
	  { dda_previous(penum->dda.row.x);
	    dda_previous(penum->dda.row.y);
	    --(penum->y);
	  }
	/* Note that caller must call end_image */
	/* for both error and normal termination. */
out:	return code;
}

/* Clean up by releasing the buffers. */
/* Currently we ignore draw_last. */
int
gx_default_end_image(gx_device *dev, void *info, bool draw_last)
{	gx_image_enum *penum = info;
	gs_memory_t *mem = penum->memory;
	stream_IScale_state *scaler = penum->scaler;

	gs_free_object(mem, penum->rop_dev, "image RasterOp");
	gs_free_object(mem, penum->clip_dev, "image clipper");
	if ( scaler != 0 )
	  { (*s_IScale_template.release)((stream_state *)scaler);
	    gs_free_object(mem, scaler, "image scaler state");
	  }
	gs_free_object(mem, penum->line, "image line");
	gs_free_object(mem, penum->buffer, "image buffer");
	gs_free_object(mem, penum, "gx_default_end_image");
	return 0;
}
