/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* rtcrastr.c */
/* HP RTL color raster graphics commands */
#include "std.h"
#include "gsbittab.h"
#include "pcommand.h"
#include "pcstate.h"
#include "rtraster.h"

/* ------ Internal procedures ------ */

/*
 * Flip 1-to-8-bit planar data to 8-bit chunky.  The output area must be
 * able to hold max(rows[i].count) * 8 bytes; return the actual number of
 * bytes produced.  If the depth is 4 or less, we could use smaller chunks
 * than 8 bits, but it would complicate and slow down the code, and the
 * extra space is insignificant.
 */
private uint
flip_rows(const pcl_raster_row_t *rows, int depth, byte *out)
{	uint count = 0;
	int plane;

	for ( plane = 0; plane < depth; ++plane )
	  if ( !rows[plane].is_zero )
	    { uint row_count = rows[plane].count;
	      const byte *data = rows[plane].data;
	      uint i, i0 = min(count, row_count);
	      static const bits32 expand[16] = {
#if arch_is_big_endian
		bit_table_4(0,0x01000000,0x00010000,0x00000100,0x00000001)
#else
		bit_table_4(0,0x00000001,0x00000100,0x00010000,0x01000000)
#endif
	      };
	      bits32 *q = (bits32 *)out;

	      /* Merge bits into existing data. */
	      for ( i = 0; i < i0 << 1; ++q, ++i )
		*q |= expand[i & 1 ? data[i >> 1] >> 4 : data[i >> 1] & 0xf];
	      if ( row_count > count )
		{ /* Store bits into added data. */
		  for ( ; i < row_count << 1; ++q, ++i )
		    *q = expand[i & 1 ? data[i >> 1] >> 4 : data[i >> 1] & 0xf];
		  count = row_count;
		}
	    }
	return count << 3;
}

/* Replacement Delta Row Encoding compression */
/* This is documented in the PCL5 Comparison Guide, pp. 1-103 ff. */
private int
uncompress_row_9(pcl_raster_row_t *row, uint in_size, stream_cursor_read *pr)
{	/**** NOT IMPLEMENTED YET ****/
	return -1;
}

/* ------ Commands ------ */

/* This replaces the implementation in pcraster.c. */
private int /* ESC * r <cap_enum> A */
pcl_c_start_raster_graphics(pcl_args_t *pargs, pcl_state_t *pcls)
{	int setting = int_arg(pargs);

	if ( pcls->raster.graphics_mode )
	  return 0;		/* locked out */
	if ( setting > 3 )
	  setting = 0;
	return pcl_begin_raster_graphics(pcls, setting);
}

/* This replaces the implementation in pcraster.c. */
extern pcl_command_proc(pcl_transfer_raster_data);
private int /* ESC * b <count> W */
pcl_c_transfer_raster_data(pcl_args_t *pargs, pcl_state_t *pcls)
{	if ( pcls->raster.plane == 0 )
	  return pcl_transfer_raster_data(pargs, pcls);
	enter_graphics_mode_if_needed(pcls);
	{ int code =
	    (pcls->raster.plane < pcls->raster.depth ?
	     pcl_read_raster_data(pargs, pcls, 1, false) : 0);
	  uint count;
	  int i;

	  if ( code < 0 )
	    return code;
	  /* Expand (or allocate) the color row buffer if needed. */
	  count = 0;
	  for ( i = 0; i < pcls->raster.depth; ++i )
	    if ( pcls->raster.row[i].count > count )
	      count = pcls->raster.row[i].count;
	  code = pcl_resize_row(&pcls->raster.color_row, count, pcls->memory,
				"pcl color row");
	  if ( code < 0 )
	    return code;
	  pcls->raster.color_row.count =
	    flip_rows(pcls->raster.row, pcls->raster.depth,
		      pcls->raster.color_row.data);
	  return pcl_image_row(pcls, &pcls->raster.color_row);
	}
}

private int /* ESC * b <nbytes> V */
pcl_transfer_raster_planes(pcl_args_t *pargs, pcl_state_t *pcls)
{	enter_graphics_mode_if_needed(pcls);
	if ( !pcls->raster.planar ||
	     pcls->raster.plane >= pcls->raster.depth
	   )
	  return e_Range;
	if ( pcl_compression_methods[pcls->raster.compression].block )
	  { /*
	     * This is an error, because block-oriented modes
	     * (potentially) generate multiple rows of data.
	     */
	    return e_Range;
	  }
	{ int code = pcl_read_raster_data(pargs, pcls, 1, false);
	  pcls->raster.plane++;
	  return code;
	}
}

private int /* ESC * t <w_dp> H */
pcl_dest_raster_width(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->raster.dest_width_dp = float_arg(pargs);
	return 0;
}

private int /* ESC * t <h_dp> V */
pcl_dest_raster_height(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->raster.dest_height_dp = float_arg(pargs);
	return 0;
}

private int /* ESC * t <bool> K */
pcl_scale_algorithm(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint i = uint_arg(pargs);
	if ( !arg_is_present(pargs) || i > 1 )
	  return 0;
	pcls->raster.enhance_dark = i != 0;
	return 0;
}

/* Initialization */
private int
rtcrastr_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('*')
	  {'r', 'A',
	     PCL_COMMAND("Start Raster Graphics", pcl_c_start_raster_graphics,
			 pca_neg_error|pca_big_error|pca_raster_graphics)},
	  {'b', 'W',
	     PCL_COMMAND("Transfer Raster Data", pcl_c_transfer_raster_data,
			 pca_bytes|pca_raster_graphics)},
	  {'b', 'V',
	     PCL_COMMAND("Transfer Raster Planes", pcl_transfer_raster_planes,
			 pca_bytes|pca_raster_graphics)},
	  {'t', 'H',
	     PCL_COMMAND("Destination Raster Width", pcl_dest_raster_width,
			 pca_neg_ignore|pca_big_ignore)},
	  {'t', 'V',
	     PCL_COMMAND("Destination Raster Height", pcl_dest_raster_height,
			 pca_neg_ignore|pca_big_ignore)},
	END_CLASS
		/* Register compression mode 9 */
	pcl_register_compression_method(9, uncompress_row_9, false);
	return 0;
}
private void
rtcrastr_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { pcls->raster.dest_width_dp = 0;
	    pcls->raster.dest_height_dp = 0;
	    if ( type & pcl_reset_initial )
	      pcls->raster.enhance_dark = false;
	    else
	      gs_free_object(pcls->memory, pcls->raster.color_row.data,
			     "rtcrastr_do_reset(color row buffer)");
	    pcls->raster.color_row.data = 0;
	    pcls->raster.color_row.size = 0;
	  }
}
const pcl_init_t rtcrastr_init = {
  rtcrastr_do_init, rtcrastr_do_reset
};
/* The following commands are only registered in PCL mode. */
private int
rtcrastr_pcl_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('*')
	  {'t', 'K', {pcl_scale_algorithm, pca_neg_ignore|pca_big_ignore}},
	END_CLASS
	return 0;
}
const pcl_init_t rtcrastr_pcl_init = {
  rtcrastr_pcl_do_init, 0
};
