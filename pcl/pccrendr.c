/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pccrendr.c */
/* PCL5c output color rendering commands */
#include "math_.h"
#include "memory_.h"
#include "std.h"
#include "pcommand.h"
#include "pcstate.h"
#include "gsrop.h"
#include "gxht.h"

/* Internal routines */

/* Convert a downloaded dither matrix to a threshold halftone. */
const byte *
load_threshold(gs_threshold_halftone *ptht, const byte *data,
  gs_memory_t *mem)
{	uint h = (data[0] << 8) + data[1];
	uint w = (data[2] << 8) + data[3];
	uint size = h * w;
	byte *thresholds = ptht->thresholds.data;

	/**** CAN WE REALLY OVERWRITE EXISTING THRESHOLDS ARRAY? ****/
	if ( thresholds == 0 )
	  thresholds = gs_alloc_string(mem, size, "threshold array");
	else if ( ptht->height * ptht->width != size )
	  thresholds = gs_resize_string(mem, thresholds, ptht->thresholds.size,
					size, "threshold array");
	if ( thresholds == 0 )
	  return 0;		/* out of memory */
	ptht->width = w;
	ptht->height = h;
	memcpy(thresholds, data + 4, size);
	ptht->thresholds.data = thresholds;
	ptht->thresholds.size = size;
	ptht->transfer = 0;
	return data + 4 + size;
}

/* Reset the color lookup tables, optionally freeing them.. */
private void
release_clut(pcl_state_t *pcls, bool free)
{	int csi;
	pcls->palette.clut.v.cmy = 0;	/* don't free twice */
	for ( csi = 0; csi < countof(pcls->palette.clut.i); ++csi )
	  { if ( free )
	      gs_free_object(pcls->memory, pcls->palette.clut.i[csi],
			     "release_clut");
	    pcls->palette.clut.i[csi] = 0;
	  }
}

/* Commands */

private int /* ESC * m <nbytes> W */
pcl_download_dither_matrix(pcl_args_t *pargs, pcl_state_t *pcls)
{	const byte *data = arg_data(pargs);
	uint count = uint_arg(pargs);
	int i;
	const byte *p = data + 2;
	gs_memory_t *mem = pcls->memory;
	gs_halftone *pht;

	if ( count < 7 )
	  return e_Range;
	if ( data[0] != 0 || (data[1] != 1 && data[1] != 3) )
	  return e_Range;
	for ( i = 0; i < data[1]; ++i )
	  { uint h, w;
	    if ( p + 4 > data + count )
	      return e_Range;
	    h = (p[0] << 8) + p[1];
	    w = (p[2] << 8) + p[3];
	    p += 4;
	    if ( h == 0 || w == 0 || h * w > data + count - p )
	      return e_Range;
	    p += h * w;
	  }
	if ( p != data + count )
	  return e_Range;
	/**** RELEASE OLD HALFTONE (?) ****/
	pht = gs_alloc_struct(mem, gs_halftone, &st_halftone,
			      "download screen(halftone)");
	if ( pht == 0 )
	  return_error(e_Memory);
	if ( data[1] == 1 )
	  { /* Single screen */
	    if ( load_threshold(&pht->params.threshold, data + 2, mem) == 0 )
	      { gs_free_object(mem, pht, "download screen(halftone)");
	        return_error(e_Memory);
	      }
	    pht->type = ht_type_threshold;
	  }
	else
	  { /* Separate R/G/B screens.  The library demands a Default, */
	    /* so we arbitrarily use the blue screen. */
	    int i;
	    static const gs_ht_separation_name rgb_names[3] = {
	      gs_ht_separation_Red, gs_ht_separation_Green,
	      gs_ht_separation_Blue
	    };
	    gs_halftone_component *phcs =
	      gs_alloc_struct_array(mem, 3, gs_halftone_component,
				    &st_halftone_component,
				    "download screen(components)");
	    const byte *next = p + 2;

	    if ( phcs == 0 )
	      { gs_free_object(mem, pht, "download screen(halftone)");
	        return_error(e_Memory);
	      }
	    for ( i = 0; i < 3; ++i )
	      { phcs[i].cname = rgb_names[i];
	        phcs[i].type = ht_type_threshold;
		next = load_threshold(&phcs[i].params.threshold, p, mem);
		if ( next == 0 )
		  { /**** FREE STRUCTURES ****/
		    return_error(e_Memory);
		  }
	      }
	    pht->params.multiple.components = phcs;
	    pht->params.multiple.num_comp = 3;
	    pht->type = ht_type_multiple;
	  }
	/**** INSTALL IF APPROPRIATE ****/
	pcls->palette.halftone = pht;
	return 0;
}

private int /* ESC * l <nbytes> W */
pcl_color_lookup_tables(pcl_args_t *pargs, pcl_state_t *pcls)
{	const byte *data = arg_data(pargs);
	uint count = uint_arg(pargs);
	gs_memory_t *mem = pcls->memory;

	if ( count == 0 )
	  { /* Remove existing CLUT. */
	    release_clut(pcls, true);
	    pcls->palette.rgb_correction = rgb_use_none;
	  }
	else if ( count == 770 )
	  { /* Download CLUT. */
	    int csi = data[0];
	    int j;
	    byte *clut;

	    if ( csi == pcl_csi_DeviceCMY )
	      csi = pcl_csi_DeviceRGB;
	    clut = pcls->palette.clut.i[csi];
	    if ( clut == 0 )
	      { clut = gs_alloc_bytes(mem, 768, "download clut");
	        if ( clut == 0 )
		  return_error(e_Memory);
		pcls->palette.clut.i[csi] = clut;
		pcls->palette.clut.v.cmy = pcls->palette.clut.v.rgb;
	      }
	    /* Rearrange CLUT into chunky pixels. */
	    for ( j = 0; j < 256; ++j )
	      { byte *dest = &clut[j * 3];
	        dest[0] = data[2 + j];
		dest[1] = data[2 + 256 + j];
		dest[2] = data[2 + 2*256 + j];
	      }
	    pcls->palette.rgb_correction = rgb_use_clut;
	  }
	else
	  return e_Range;
	return 0;
}

private int /* ESC * t <gamma> I */
pcl_gamma_correction(pcl_args_t *pargs, pcl_state_t *pcls)
{	float gamma = float_arg(pargs);

	pcls->palette.gamma = gamma;
	if ( gamma == 0 || gamma == 1 )
	  pcls->palette.rgb_correction = rgb_use_none;
	else
	  pcls->palette.rgb_correction = rgb_use_gamma;
	return 0;
}

private int /* ESC * i <nbytes> W */
pcl_viewing_illuminant(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint count = uint_arg(pargs);
	const byte *data = arg_data(pargs);

	if ( count != 8 )
	  return e_Range;
	/* The PCL5 Color Reference Manual, p. 4-16, says that */
	/* the l.s.w. comes first, unlike all other float parameters.... */
	{ int i;
	  for ( i = 0; i < 8; i += 4 )
	    pcls->palette.white_point.i[i >> 2] =
	      word2float( ((uint32)((data[i+2] << 8) + data[i+3]) << 16) +
			  (data[i] << 8) + data[i+1] );
	}
	return 0;
}

private int /* ESC & b <bool> M */
pcl_monochrome_print_mode(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint b = uint_arg(pargs);

	if ( b <= 1 )
	  pcls->monochrome_print = b;
	return 0;
}

private int /* ESC * o <> W */
pcl_driver_configuration(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint count = uint_arg(pargs);
	const byte *data = arg_data(pargs);

	if ( count < 3 )
	  return e_Range;
	if ( data[0] != 6 )
	  return e_Range;
	switch ( data[1] )
	      {
	      case 0:
	      case 1:
		if ( count != 3 )
		  return e_Range;
		{ int value = (data[2] & 0x7f) - (data[2] & 0x80);
		  if ( value < -100 || value > 100 )
		    return e_Range;
		  if ( data[1] )
		    pcls->lightness = value;
		  else
		    pcls->saturation = value;
		}
		break;
	      case 3:
		if ( count != 3 || data[2] > 2 )
		  return e_Range;
		pcls->scaling_algorithm = data[2];
		break;
	      case 4:
		if ( count != 3 || data[2] > 5 )
		  return e_Range;
		pcls->color_treatment = data[2];
		break;
	      case 5:
		/* The PCL5 Color Technical Reference Manual (p. 4-20) */
		/* says the color map is 14739 bytes, but that seems */
		/* pretty unlikely to me. */
		if ( count < 5 )
		  return e_Range;
		switch ( data[2] )
		  {
		  case 1:
		  case 3:
		    break;
		  default:
		    return e_Range;
		  }
		if ( data[3] >= 1 && data[3] <= 24 )
		  /**** DEVICE DEPENDENT COLOR ****/
		  ;
		else if ( data[3] >= 51 && data[3] <= 74 )
		  /**** DEVICE INDEPENDENT COLOR ****/
		  ;
		else
		  return e_Range;
		/**** DOWNLOAD COLOR MAP ****/
		return e_Unimplemented;
	      default:
		return e_Range;
	      }
	return 0;
}

/* Initialization */
private int
pccrendr_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('*')
	  {'m', 'W', {pcl_download_dither_matrix, pca_bytes}},
	  {'l', 'W', {pcl_color_lookup_tables, pca_bytes}},
	  {'t', 'I', {pcl_gamma_correction, pca_neg_ignore|pca_big_ignore}},
	  {'i', 'W', {pcl_viewing_illuminant, pca_bytes}},
	END_CLASS
	DEFINE_CLASS_COMMAND('&', 'b', 'M', pcl_monochrome_print_mode)
	DEFINE_CLASS_COMMAND_ARGS('*', 'o', 'W', pcl_driver_configuration, pca_bytes)
	return 0;
}
private void
pccrendr_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { pcls->palette.halftone = 0;
	    pcls->palette.gamma = 0;
	    release_clut(pcls, !(type & pcl_reset_initial));
	    pcls->palette.rgb_correction = rgb_use_none;
	    pcls->palette.white_point.v.x = 0.3127;	/* D65(6500K), per H-P spec */
	    pcls->palette.white_point.v.y = 0.3290;	/* ditto */
	    pcls->monochrome_print = false;
	    pcls->lightness = 0;
	    pcls->saturation = 0;
	    pcls->scaling_algorithm = 0;
	    pcls->color_treatment = 0;
	  }
}
const pcl_init_t pccrendr_init = {
  pccrendr_do_init, pccrendr_do_reset
};
