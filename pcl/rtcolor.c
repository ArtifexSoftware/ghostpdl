/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* rtcolor.c */
/* HP RTL color setting and parameter commands */
#include "std.h"
#include "pcommand.h"
#include "pcstate.h"
#include "gsrop.h"

/* ---------------- Chapter 2 ---------------- */

/* Define the simple color palettes. */
/****** ALL THESE VALUES SHOULD BE pcl_fixed. ******/
private const int32 palette_simple_CMY[8*3] =
{	0xff,0xff,0xff,  0,0xff,0xff,  0xff,0,0xff,  0,0,0xff,
	0xff,0xff,0,  0,0xff,0,  0xff,0,0,  0,0,0
};
private const int32 palette_simple_K[2*3] =
{	0xff,0xff,0xff,  0,0,0
};
private const int32 palette_simple_RGB[8*3] =
{	0,0,0,  0xff,0,0,  0,0xff,0,  0xff,0xff,0,
	0,0,0xff,  0xff,0,0xff,  0,0xff,0xff,  0xff,0xff,0xff
};

private int /* ESC * r <cs_enum> U */
pcl_simple_color(pcl_args_t *pargs, pcl_state_t *pcls)
{	const int32 *pdata;
	uint psize;

	switch ( int_arg(pargs) )
	  {
	  case -3:
	    pcls->palette.color_space_index = pcl_csi_DeviceCMY;
	    pdata = palette_simple_CMY;
pal3:	    psize = 8;
	    pcls->palette.planar = true;
	    pcls->palette.indexed = true;
	    break;
	  case 3:
	    pcls->palette.color_space_index = pcl_csi_DeviceRGB;
	    pdata = palette_simple_RGB;
	    goto pal3;
	  case 1:
	    pcls->palette.color_space_index = pcl_csi_DeviceRGB;
	    pdata = palette_simple_K;
	    psize = 1;
	    break;
	  default:
	    return e_Range;
	  }
	/**** DESTROY OLD pcls->palette.data ****/
	/****** const MISMATCH ******/
	pcls->palette.data = pdata;
	pcls->palette.size = psize;
	return 0;
}

/* Define the data for the CID command. */
#define cid_common\
  byte color_space;\
  byte pixel_encoding_mode;\
  byte bits_per_index;\
  byte bits_per_primary[3]
typedef struct cid_common_s {
  cid_common;
} cid_common_t;

private int /* ESC * v <nbytes> W */
pcl_configure_image_data(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint count = uint_arg(pargs);
	const byte *data = arg_data(pargs);
	static const byte cid_counts[5] = { 18, 18, 30, 86, 120 };
	int i;

	if ( count < 6 )
	  return e_Range;
#define pcid ((const cid_common_t *)data)
	if ( pcid->color_space >= countof(cid_counts) ||
	     (count != 6 && count != cid_counts[pcid->color_space]) ||
	     pcid->pixel_encoding_mode > 3 ||
	     (pcid->bits_per_index == 0 && pcid->pixel_encoding_mode <= 1) ||
	     pcid->bits_per_index > 8 ||
	     (pcid->pixel_encoding_mode == 1 &&
	      (pcid->bits_per_index & (pcid->bits_per_index - 1))) ||
	     pcid->bits_per_primary[0] == 0 ||
	     pcid->bits_per_primary[1] == 0 ||
	     pcid->bits_per_primary[2] == 0
	   )
	  return e_Range;
#define ppp (&(pcls->palette.params))
	if ( count == 6 )
	  { /* Short form */
	    switch ( pcid->color_space )
	      {
	      case pcl_csi_DeviceRGB:
		for ( i = 0; i < 3; ++i )
		  { ppp->rgb_cmy.black_reference[i] = 0;
		    ppp->rgb_cmy.white_reference[i] =
		      (1 << pcid->bits_per_primary[i]) - 1;
		  }
		break;
	      case pcl_csi_DeviceCMY:
		for ( i = 0; i < 3; ++i )
		  { ppp->rgb_cmy.white_reference[i] = 0;
		    ppp->rgb_cmy.black_reference[i] =
		      (1 << pcid->bits_per_primary[i]) - 1;
		  }
		break;
	      case pcl_csi_ColorimetricRGB:
		for ( i = 0; i < 3; ++i )
		  { /**** HOW TO SET chromaticity? ****/
		    ppp->colorimetric.scaling[i].gamma = 2.2;
		    ppp->colorimetric.scaling[i].gain = 1.0;
		    ppp->colorimetric.range[i].vmin = 0.0;
		    ppp->colorimetric.range[i].vmax = 1.0;
		  }
		break;
	      case pcl_csi_CIELab:
		{ static const float lab_vf[6] = {
		    0.0, 100.0, -100.0, 100.0, -100.0, 100.0
		  };
		  memcpy(&ppp->lab, lab_vf, sizeof(lab_vf));
		}
		break;
	      case pcl_csi_LuminanceChrominance:
		{ static const float lc_vf[9+6] = {
		    /**** IS DEFAULT ENCODING THE IDENTITY MATRIX? ****/
		    1.0, 0.0, 0.0,   0.0, 1.0, 0.0,   0.0, 0.0, 1.0,
		    0.0, 1.0, -0.89, 0.89, -0.70, 0.70
		    /**** WHAT ABOUT chromaticity, white_point? ****/
		  };
#define pplc (&(ppp->luminance_chrominance))
		  memcpy(ppp->vf, lc_vf, sizeof(lc_vf));
		  for ( i = 0; i < 3; ++i )
		    { pplc->scaling[i].gamma = 2.2;
		      pplc->scaling[i].gain = 1.0;
		    }
#undef pplc
		}
		break;
	      default:
		return e_Range;
	      }
	  }
	else
	  { /* Long form */
	    switch ( pcid->color_space )
	      {
	      case pcl_csi_DeviceRGB:
	      case pcl_csi_DeviceCMY:
		for ( i = 6; i < count; i += 2 )
		  ppp->vi[(i - 6) >> 1] =
		    (((data[i] & 0x7f) - (data[i] & 0x80)) << 8) + data[i+1];
		break;
	      case pcl_csi_ColorimetricRGB:
	      case pcl_csi_CIELab:
	      case pcl_csi_LuminanceChrominance:
		for ( i = 6; i < count; i += 4 )
		  ppp->vf[(i - 6) >> 2] =
		    word2float( ((uint32)((data[i] << 8) + data[i+1]) << 16) +
				(data[i+2] << 8) + data[i+3] );
		break;
	      default:
		return e_Range;
	      }
	  }
#undef ppp
	pcls->palette.color_space_index = pcid->color_space;
	pcls->palette.bits_per_index = pcid->bits_per_index;
	pcls->palette.bits_per_primary[0] = pcid->bits_per_primary[0];
	pcls->palette.bits_per_primary[1] = pcid->bits_per_primary[1];
	pcls->palette.bits_per_primary[2] = pcid->bits_per_primary[2];
	pcls->palette.planar = (pcid->pixel_encoding_mode & 1) == 0;
	pcls->palette.indexed = (pcid->pixel_encoding_mode & 2) == 0;
	return 0;
#undef pcid
}

/* ---------------- Chapter 3 ---------------- */

private int /* ESC * p <op> P */
pcl_push_pop_palette(pcl_args_t *pargs, pcl_state_t *pcls)
{	switch ( int_arg(pargs) )
	  {
	  case 0:
	    /**** PUSH PALETTE ****/
	    return e_Unimplemented;
	  case 1:
	    /**** POP PALETTE ****/
	    return e_Unimplemented;
	  default:
	    ;
	  }
	return 0;
}

private int /* ESC * v <pix> S */
pcl_foreground_color(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint index = uint_arg(pargs);
	if ( index >= pcls->palette.size )
	  return e_Range;
	return e_Unimplemented;
}

private int /* ESC * v <cc> A */
pcl_color_component_1(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->color_components[0] = pcl_fixed_arg(pargs);
	return 0;
}

private int /* ESC * v <cc> B */
pcl_color_component_2(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->color_components[1] = pcl_fixed_arg(pargs);
	return 0;
}

private int /* ESC * v <cc> C */
pcl_color_component_3(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->color_components[2] = pcl_fixed_arg(pargs);
	return 0;
}

private int /* ESC * v <pix> I */
pcl_assign_color_index(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint pix = uint_arg(pargs);
	if ( pix < pcls->palette.size )
	  { /**** CHECK READ-ONLY PALETTE ****/
	    return e_Unimplemented;
	    pix *= 3;
	    pcls->palette.data[pix] = pcls->color_components[0];
	    pcls->palette.data[pix + 1] = pcls->color_components[1];
	    pcls->palette.data[pix + 2] = pcls->color_components[2];
	  }
	pcls->color_components[0] = 0;
	pcls->color_components[1] = 0;
	pcls->color_components[2] = 0;
	return 0;
}

/* ---------------- Chapter 4 ---------------- */

private int /* ESC * t <alg#> J */
pcl_render_algorithm(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint i = uint_arg(pargs);

	if ( i > 14 )
	  return e_Range;
	pcls->palette.render_algorithm = i;
	gs_setrenderalgorithm(pcls->pgs, i);
	return 0;
}

/* ---------------- Initialization ---------------- */
private int
rtcolor_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('*')
		/* Chapter 2 */
	  {'r', 'U', {pcl_simple_color, pca_neg_ok|pca_big_error}},
	  {'v', 'W', {pcl_configure_image_data, pca_bytes}},
		/* Chapter 3 */
	  {'p', 'P', {pcl_push_pop_palette}},
	  {'v', 'S', {pcl_foreground_color, pca_neg_error|pca_big_error}},
	  {'v', 'A', {pcl_color_component_1, pca_neg_ok|pca_big_error}},
	  {'v', 'B', {pcl_color_component_2, pca_neg_ok|pca_big_error}},
	  {'v', 'C', {pcl_color_component_3, pca_neg_ok|pca_big_error}},
	  {'v', 'I', {pcl_assign_color_index, pca_neg_ignore|pca_big_ignore}},
		/* Chapter 4 */
	  {'t', 'J', {pcl_render_algorithm, pca_neg_ignore|pca_big_ignore}},
	END_CLASS
	return 0;
}
private void
rtcolor_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  {	/* Chapter 2 */
	    pcls->palette.readonly = true;
	    pcls->palette.color_space_index = pcl_csi_DeviceRGB;
	    /**** SELECT simple_K PALETTE ****/
	    pcls->palette.bits_per_index = 1;
	    pcls->palette.planar = false;
	    pcls->palette.indexed = true;
	    { int i;
	      for ( i = 0; i < 3; ++i )
		pcls->palette.bits_per_primary[i] = 1;
	      pcls->palette.params.rgb_cmy.white_reference[i] = 255;
	      pcls->palette.params.rgb_cmy.black_reference[i] = 0;
	    }
		/* Chapter 3 */
	    if ( !(type & pcl_reset_initial) )
	      { /**** FREE OLD PALETTE ****/
	      }
	    pcls->palette.data = 0;
	    pcls->palette.size = 0;
	    pcls->foreground_color = 0;
	    pcls->color_components[0] = 0;
	    pcls->color_components[1] = 0;
	    pcls->color_components[2] = 0;
		/* Chapter 4 */
	    if ( type & pcl_reset_initial )
	      { pcl_args_t args;
	        arg_set_uint(&args, 3);
		pcl_render_algorithm(&args, pcls);
	      }
	  }
}
private int
rtcolor_do_copy(pcl_state_t *psaved, const pcl_state_t *pcls,
  pcl_copy_operation_t operation)
{	if ( operation & pcl_copy_after )
	  { gs_setrenderalgorithm(pcls->pgs, psaved->palette.render_algorithm);
	  }
	return 0;
}
const pcl_init_t rtcolor_init = {
  rtcolor_do_init, rtcolor_do_reset, rtcolor_do_copy
};
