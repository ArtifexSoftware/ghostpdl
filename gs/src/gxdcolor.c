/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* gxdcolor.c */
/* Pure and null device color implementation */
#include "gx.h"
#include "gsbittab.h"
#include "gxdcolor.h"
#include "gxdevice.h"

/* Define the standard device color types. */
private dev_color_proc_load(gx_dc_no_load);
private dev_color_proc_fill_rectangle(gx_dc_no_fill_rectangle);
private dev_color_proc_fill_masked(gx_dc_no_fill_masked);
private dev_color_proc_load(gx_dc_pure_load);
private dev_color_proc_fill_rectangle(gx_dc_pure_fill_rectangle);
private dev_color_proc_fill_masked(gx_dc_pure_fill_masked);
const gx_device_color_procs
  gx_dc_procs_none =
    { gx_dc_no_load, gx_dc_no_fill_rectangle, gx_dc_no_fill_masked, 0, 0 },
  /* null is different from none, but it has the same procedures. */
  gx_dc_procs_null =
    { gx_dc_no_load, gx_dc_no_fill_rectangle, gx_dc_no_fill_masked, 0, 0 },
  gx_dc_procs_pure =
    { gx_dc_pure_load, gx_dc_pure_fill_rectangle, gx_dc_pure_fill_masked, 0, 0 };
#undef gx_dc_type_none
const gx_device_color_procs _ds *gx_dc_type_none = &gx_dc_procs_none;
#define gx_dc_type_none (&gx_dc_procs_none)
#undef gx_dc_type_null
const gx_device_color_procs _ds *gx_dc_type_null = &gx_dc_procs_null;
#define gx_dc_type_null (&gx_dc_procs_null)
#undef gx_dc_type_pure
const gx_device_color_procs _ds *gx_dc_type_pure = &gx_dc_procs_pure;
#define gx_dc_type_pure (&gx_dc_procs_pure)

/* Define a null RasterOp source (for black = 0). */
const gx_rop_source_t gx_rop_no_source_0 = { gx_rop_no_source_body(0) };

/* ------ Null color ------ */

private int
gx_dc_no_load(gx_device_color *pdevc, const gs_imager_state *ignore_pis,
  gx_device *ignore_dev, gs_color_select_t ignore_select)
{	return 0;
}

private int
gx_dc_no_fill_rectangle(const gx_device_color *pdevc, int x, int y,
  int w, int h, gx_device *dev, gs_logical_operation_t lop,
  const gx_rop_source_t *source)
{	return 0;
}

private int
gx_dc_no_fill_masked(const gx_device_color *pdevc, const byte *data,
  int data_x, int raster, gx_bitmap_id id, int x, int y, int w, int h,
  gx_device *dev, gs_logical_operation_t lop, bool invert)
{	return 0;
}

/* ------ Pure color ------ */

private int
gx_dc_pure_load(gx_device_color *pdevc, const gs_imager_state *ignore_pis,
  gx_device *ignore_dev, gs_color_select_t ignore_select)
{	return 0;
}

/* Fill a rectangle with a pure color. */
/* Note that we treat this as "texture" for RasterOp. */
private int
gx_dc_pure_fill_rectangle(const gx_device_color *pdevc, int x, int y,
  int w, int h, gx_device *dev, gs_logical_operation_t lop,
  const gx_rop_source_t *source)
{	if ( source == NULL && lop_no_S_is_T(lop) )
	  return (*dev_proc(dev, fill_rectangle))(dev, x, y, w, h,
						  pdevc->colors.pure);
	{ gx_color_index colors[2];
	  gx_rop_source_t no_source;

	  colors[0] = colors[1] = pdevc->colors.pure;
	  if ( source == NULL )
	    set_rop_no_source(source, no_source, dev);
	  return (*dev_proc(dev, strip_copy_rop))
	    (dev, source->sdata, source->sourcex, source->sraster,
	     source->id, (source->use_scolors ? source->scolors : NULL),
	     NULL /*arbitrary*/, colors, x, y, w, h, 0, 0, lop);
	}
}

/* Fill a mask with a pure color. */
/* Note that there is no source in this case: the mask is the source. */
private int
gx_dc_pure_fill_masked(const gx_device_color *pdevc, const byte *data,
  int data_x, int raster, gx_bitmap_id id, int x, int y, int w, int h,
  gx_device *dev, gs_logical_operation_t lop, bool invert)
{	if ( lop_no_S_is_T(lop) )
	  { gx_color_index color0, color1;
	    if ( invert )
	      color0 = pdevc->colors.pure, color1 = gx_no_color_index;
	    else
	      color1 = pdevc->colors.pure, color0 = gx_no_color_index;
	    return (*dev_proc(dev, copy_mono))
	      (dev, data, data_x, raster, id, x, y, w, h, color0, color1);
	  }
	{ gx_color_index scolors[2];
	  gx_color_index tcolors[2];

	  scolors[0] = (*dev_proc(dev, map_rgb_color))
	    (dev, (gx_color_value)0, (gx_color_value)0, (gx_color_value)0);
	  scolors[1] = (*dev_proc(dev, map_rgb_color))
	    (dev, gx_max_color_value, gx_max_color_value, gx_max_color_value);
	  tcolors[0] = tcolors[1] = pdevc->colors.pure;
	  return (*dev_proc(dev, strip_copy_rop))
	    (dev, data, data_x, raster, id, scolors,
	     NULL, tcolors, x, y, w, h, 0, 0,
	     (invert ? rop3_invert_S(lop) : lop) | lop_S_transparent);
	}
}

/* ------ Default implementations ------ */

/* Fill a mask with a color by parsing the mask into rectangles. */
int
gx_dc_default_fill_masked(const gx_device_color *pdevc, const byte *data,
  int data_x, int raster, gx_bitmap_id id, int x, int y, int w, int h,
  gx_device *dev, gs_logical_operation_t lop, bool invert)
{	int lbit = data_x & 7;
	const byte *row = data + (data_x >> 3);
	uint one = (invert ? 0 : 0xff);
	uint zero = one ^ 0xff;
	int iy;

	for ( iy = 0; iy < h; ++iy, row += raster )
	  { const byte *p = row;
	    int bit = lbit;
	    int left = w;
	    int l0;

	    while ( left )
	      { int run, code;

	        /* Skip a run of zeros. */
	        run = byte_bit_run_length[bit][*p ^ one];
		if ( run )
		  { if ( run < 8 )
		      { if ( run >= left )
			  break;	/* end of row while skipping */
		        bit += run, left -= run;
		      }
		    else if ( (run -= 8) >= left )
		      break;		/* end of row while skipping */
		    else
		      { left -= run;
		        ++p;
			while ( left > 8 && *p == zero )
			  left -= 8, ++p;
			run = byte_bit_run_length_0[*p ^ one];
			if ( run >= left ) /* run < 8 unless very last byte */
			  break;	/* end of row while skipping */
			else
			  bit = run & 7, left -= run;
		      }
		  }

		l0 = left;
		/* Scan a run of ones, and then paint it. */
		run = byte_bit_run_length[bit][*p ^ zero];
		if ( run < 8 )
		  { if ( run >= left )
		      left = 0;
		    else
		      bit += run, left -= run;
		  }
		else if ( (run -= 8) >= left )
		  left = 0;
		else
		  { left -= run;
		    ++p;
		    while ( left > 8 && *p == one )
		      left -= 8, ++p;
		    run = byte_bit_run_length_0[*p ^ zero];
		    if ( run >= left ) /* run < 8 unless very last byte */
		      left = 0;
		    else
		      bit = run & 7, left -= run;
		  }
		code = gx_device_color_fill_rectangle(pdevc,
			x + w - l0, y + iy, l0 - left, 1, dev, lop, NULL);
		if ( code < 0 )
		  return code;
	      }
	  }
	return 0;
}
