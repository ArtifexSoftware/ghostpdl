/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* rtraster.c */
/* HP RTL raster parameter and data transfer commands */
#include "memory_.h"
#include "gstypes.h"		/* for gsmatrix.h */
#include "gsmemory.h"		/* for gxdevice.h */
#include "gxdevice.h"
#include "gscoord.h"
#include "gsdevice.h"
#include "gzstate.h"		/* for clip_path, dev_color */
#include "pcommand.h"
#include "pcstate.h"
#include "pcdraw.h"
#include "rtraster.h"

/****** Left to do ('c' means color only):
  Color space (c)
  Transparency handling (c)
  Scaling (c)
  Row padding
 ******/

/* Initialize a seed row. */
private void
initialize_row(pcl_raster_row_t *row)
{	row->data = 0;
	row->size = 0;
	row->count = 0;
	row->is_zero = true;
	row->repeat_count = 0;
}

/* Clear a seed row. */
#define clear_row(row)\
  if ( !(row)->is_zero ) (row)->count = 0, (row)->is_zero = true
/* Clear multiple seed rows. */
private void
clear_rows(pcl_state_t *pcls, int depth)
{	int i;
	for ( i = 0; i < depth; ++i )
	  clear_row(&pcls->raster.row[i]);
}

/* ------ Decompression procedures ------ */

/*
 * Decompress a row of raster data.  We only know the size of the compressed
 * data, not the uncompressed result, and we have to deal with the
 * possibilities of (1) too much decompressed data, (2) less than a full row
 * of decompressed data, or (3) syntax error (running out of input data in
 * the middle of a compressed data construct).  Thus in_size is the maximum
 * amount of uncompressed data we expect, *pr brackets the available
 * compressed data, and we set row->count to the actual amount of data
 * decompressed.
 */

/* No compression */
private int
uncompress_row_0(pcl_raster_row_t *row, uint in_size, stream_cursor_read *pr)
{	const byte *p = pr->ptr;
	uint rcount = pr->limit - p;
	uint in_count = min(in_size, rcount);
	uint count = min(row->size, in_count);

	memcpy(row->data, p + 1, count);
	pr->ptr += in_count;
	row->count = count;
	row->is_zero = count == 0;
	return 0;
}

/* Run length encoding */
private int
uncompress_row_1(pcl_raster_row_t *row, uint in_size, stream_cursor_read *pr)
{	const byte *p = pr->ptr;
	const byte *rlimit = pr->limit;
	uint left = min(in_size, row->size);
	byte *q = row->data;
	byte *end = q + left;

	if ( (rlimit - p) & 1 )
	  return -1;
	for ( ; p < rlimit && left; p += 2 )
	  { uint reps = p[1] + 1;
	    byte fill = p[2];

	    if ( reps > left )
	      reps = left;
	    left -= reps;
	    if ( reps > end - q )
	      reps = end - q;
	    memset(q, fill, reps);
	    q += reps;
	  }
	pr->ptr = p;
	row->count = q - row->data;
	row->is_zero = row->count == 0;
	return 0;
}

/* PackBits encoding */
private int
uncompress_row_2(pcl_raster_row_t *row, uint in_size, stream_cursor_read *pr)
{	const byte *p = pr->ptr;
	const byte *rlimit = pr->limit;
	uint left = min(in_size, row->size);
	byte *q = row->data;
	byte *end = q + left;

	for ( ; p < rlimit && left; )
	  { byte control = *++p;
	    uint n, copy;

	    if ( control < 128 )
	      { /* N+1 literal bytes follow */
		n = min(control + 1, left);
		if ( n > rlimit - p )
		  n = rlimit - p;
		left -= n;
		copy = min(n, end - q);
		memcpy(q, p + 1, copy);
		p += n;
	      }
	    else if ( control > 128 )
	      { /* Copy the next data byte 257-N times */
		if ( p >= rlimit )
		  break;
		n = min(257 - control, left);
		left -= n;
		copy = min(n, end - q);
		++p;
		memset(q, *p, copy);
	      }
	    else
	      continue;
	    q += copy;
	  }
	pr->ptr = p;
	row->count = q - row->data;
	row->is_zero = row->count == 0;
	return 0;
}

/* Delta row encoding */
private int
uncompress_row_3(pcl_raster_row_t *row, uint in_size, stream_cursor_read *pr)
{	const byte *p = pr->ptr;
	const byte *rlimit = pr->limit;
	uint left = min(in_size, row->size);
	byte *q = row->data;
	byte *end = q + left;

	for ( ; p < rlimit && left; )
	  { byte command = *++p;
	    uint skip = command & 31;
	    uint copy = (command >> 5) + 1;
	    if ( skip == 31 )
	      { while ( p < rlimit )
		  { skip += *++p;
		    if ( *p == 255 )
		      break;
		  }
	      }
	    if ( skip > left )
	      skip = left;
	    left -= skip;
	    if ( skip > end - q )
	      q = end;
	    else
	      q += skip;
	    if ( copy > left )
	      copy = left;
	    left -= copy;
	    if ( copy > rlimit - p )
	      copy = rlimit - p;
	    if ( copy > end - q )
	      copy = end - q;
	    memcpy(q, p + 1, copy);
	    p += copy;
	    q += copy;
	  }
	pr->ptr = p;
	{ uint count = q - row->data;
	  if ( count > row->count )
	    row->count = count;
	  row->is_zero &= count == 0;
	}
	return 0;
}

/* Adaptive (mixed) encoding */
private int
uncompress_row_5(pcl_raster_row_t *row, uint in_size, stream_cursor_read *pr)
{	const byte *p = pr->ptr;
	const byte *rlimit = pr->limit;
	uint count;
	int method;

top:	if ( row->repeat_count )
	  { row->repeat_count--;
	    return 0;
	  }
	if ( rlimit - p < 3 )
	  return (p == rlimit ? 1 : gs_note_error(e_Syntax));
	count = (p[2] << 8) + p[3];
	method = p[1];
	switch ( method )
	  {
	  case 4:		/* empty row */
	    clear_row(row);
	  case 5:		/* duplicate row */
	    row->repeat_count = count;
	    pr->ptr = p += 3;
	    goto top;
	  default:		/* error */
	    clear_row(row);
	    pr->ptr = p;
	    return_error(e_Syntax);
	  case 0: case 1: case 2: case 3: /* valid methods */
	    { stream_cursor_read r;
	      int code;

	      r.ptr = p += 3;
	      r.limit = p + min(rlimit - p, count);
	      code = (*pcl_compression_methods[method].proc)(row, in_size, &r);
	      pr->ptr = r.limit;	/* skip extra data */
	      return code;
	    }
	  }
}

/* Decompression method table */
pcl_compression_method_t pcl_compression_methods[10] = {
  {uncompress_row_0, 0/*false*/},
  {uncompress_row_1, 0/*false*/},
  {uncompress_row_2, 0/*false*/},
  {uncompress_row_3, 0/*false*/},
  {0, 0},
  {uncompress_row_5, 1/*true*/}
};

/* Register a compression method. */
void
pcl_register_compression_method(int method, pcl_uncompress_proc_t proc,
  bool block)
{	pcl_compression_methods[method].proc = proc;
	pcl_compression_methods[method].block = block;
}

/* ------ Internal procedures ------ */

/* Begin raster graphics.  Only call this if !pcls->raster.graphics_mode. */
int
pcl_begin_raster_graphics(pcl_state_t *pcls, int setting)
{	bool across =
	  pcls->raster.across_physical_page &&
	  (pcls->orientation & 1);
	int code = pcl_set_drawing_color(pcls, pcls->pattern_type,
					 &pcls->current_pattern_id);

	if ( code < 0 )
	  return code;
	pcls->raster.graphics_mode = true;
	pcls->raster.y = 0;
	pcls->raster.last_width = 0;
	pcls->raster.image_info = 0;
	{ int i;
	  for ( i = 0; i < countof(pcls->raster.row); ++i )
	    { pcl_raster_row_t *row = &pcls->raster.row[i];
	      clear_row(row);
	      row->repeat_count = 0;
	    }
	}
	pcls->raster.scaling = (setting & 2) != 0;
	pcls->raster.plane = 0;
	/*
	 * The discussion of raster direction in TRM 15-9 et seq doesn't
	 * say how print direction affects raster images in presentation
	 * mode 3.  We assume here that print direction affects raster
	 * images regardless of presentation mode.
	 */
	pcl_set_graphics_state(pcls, true);  /* with print direction */
	if ( across )	/* across_physical_page & landscape */
	{ gs_point cap_dev, cap_image;

	  /* Convert CAP to device coordinates, since it mustn't move. */
	  gs_transform(pcls->pgs, (floatp)pcls->cap.x, (floatp)pcls->cap.y,
		       &cap_dev);
	  gs_rotate(pcls->pgs, -90.0);
	  gs_itransform(pcls->pgs, cap_dev.x, cap_dev.y, &cap_image);
	  pcls->cap.x = (coord)cap_image.x;
	  pcls->cap.y = (coord)cap_image.y;
	}
	/* See TRM 15-10 for the following algorithm. */
	if ( !(setting & 1 ) )
	  { if ( across )
	      pcls->cap.x = (coord)(50.0 / pcls->raster.resolution *
				    pcl_coord_scale);
	    else
	      pcls->cap.x = 0;
	  }
	if ( pcls->raster.depth == 1 && pcls->source_transparent )
	  { gs_image_t_init_mask(&pcls->raster.image, true);
	  }
	else
	  { /**** MONOCHROME ONLY FOR NOW ****/
	    gs_image_t_init_gray(&pcls->raster.image);
	    pcls->raster.image.Decode[0] = 1;
	    pcls->raster.image.Decode[1] = 0;
	    pcls->raster.image.CombineWithColor = true;
	  }
	pcls->raster.left_margin = pcls->cap.x;
	if ( !pcls->raster.width_set )
	  { /* Compute the default raster width per TRM 15-14. */
	    int direction =
	      (pcls->raster.across_physical_page ? 0 : pcls->orientation) +
	      pcls->print_direction;
	    coord width =
	      (direction & 1 ? pcls->logical_page_size.y :
	       pcls->logical_page_size.x) - pcls->raster.left_margin;

	    /* The width shouldn't be negative, but if it is.... */
	    pcls->raster.width =
	      (width <= 0 ? 0 : coord2inch(width) * pcls->raster.resolution);
	  }
	return 0;
}

/* Resize (expand) a row buffer if needed. */
int
pcl_resize_row(pcl_raster_row_t *row, uint new_size, gs_memory_t *mem,
  client_name_t cname)
{	if ( new_size > row->size )
	  { byte *new_data =
	      (row->data == 0 ?
	       gs_alloc_bytes(mem, new_size, cname) :
	       gs_resize_object(mem, row->data, new_size, cname));
	    if ( new_data == 0 )
	      return_error(e_Memory);
	    row->data = new_data;
	    row->size = new_size;
	  }
	return 0;
}

/* Read one plane of data.  We export this for rtcrastr.c. */
int
pcl_read_raster_data(pcl_args_t *pargs, pcl_state_t *pcls, int bits_per_pixel,
  bool last_plane)
{	uint count = uint_arg(pargs);
	const byte *data = arg_data(pargs);
	int method = pcls->raster.compression;
	int plane = pcls->raster.plane;
	pcl_raster_row_t *row = &pcls->raster.row[plane];
	uint row_size;
	int code;

	/* Make sure we have a large enough row buffer. */
	row_size = (pcls->raster.width * bits_per_pixel + 7) >> 3;
	code = pcl_resize_row(row, row_size, pcls->memory, "pcl image row");
	/* Decompress the data into the row. */
	{ stream_cursor_read r;

	  r.ptr = data - 1;
	  r.limit = r.ptr + count;
	  code = (*pcl_compression_methods[method].proc)(row, row_size, &r);
	  if ( code < 0 )
	    clear_row(row);
	  if ( last_plane )
	    { /*
	       * Pass the data to the image.  We have to do this here so we
	       * can handle block-oriented compression modes, which can
	       * generate multiple rows.
	       */
	      while ( !code )
		{ pcls->have_page = true;
		  code = pcl_image_row(pcls, row);
		  if ( code < 0 || !pcl_compression_methods[method].block ||
		       r.ptr >= r.limit
		     )
		    break;
		  code = (*pcl_compression_methods[method].proc)(row, row_size, &r);
		}
	      if ( code > 0 )
		code = 0;
	    }
	}
	return code;
}

/* Pass one row of raster data to an image. */
/* Note that this may pad the row with zeros. */
/* We export this for rtcrastr.c. */
int
pcl_image_row(pcl_state_t *pcls, pcl_raster_row_t *row)
{	gx_device *dev = gs_currentdevice(pcls->pgs);
	byte *data = row->data;
	uint width = row->count << 3;

	/* If the width has changed, end the current image and */
	/* start a new one. */
	if ( pcls->raster.last_width != width )
	  { /* End the current image. */
	    if ( pcls->raster.image_info )
	      { int code = (*dev_proc(dev, end_image))
		  (dev, pcls->raster.image_info, true);
	        pcls->raster.image_info = 0;
		if ( code < 0 )
		  return code;
	      }
	  }
	if ( pcls->raster.image_info == 0 )
	  { /* Start a new image. */
	    gs_state *pgs = pcls->pgs;
	    int code;

	    pcls->raster.image.Width = width;
	    pcls->raster.image.Height =
	      (pcls->raster.height_set ? pcls->raster.height :
	       dev->height * pcls->raster.resolution / dev->HWResolution[1]);
	    /**** DOESN'T DO GENERAL SCALING YET ****/
	    pcls->raster.image.ImageMatrix.xx =
	      pcls->raster.resolution / (float)pcl_coord_scale;
	    pcls->raster.image.ImageMatrix.yy =
	      pcls->raster.image.ImageMatrix.xx * pcls->raster.y_direction;
	    pcls->raster.image.ImageMatrix.tx =
	      -pcls->cap.x * pcls->raster.image.ImageMatrix.xx;
	    pcls->raster.image.ImageMatrix.ty =
	      -pcls->cap.y * pcls->raster.image.ImageMatrix.yy;
	    if ( pcls->raster.image.ImageMask )
	      gx_set_dev_color(pgs);
	    code = (*dev_proc(dev, begin_image))
	      (dev, (gs_imager_state *)pgs, &pcls->raster.image,
	       gs_image_format_chunky, NULL,
	       pgs->dev_color, pgs->clip_path,
	       pcls->memory, &pcls->raster.image_info);
	    if ( code < 0 )
	      return code;
	    pcls->raster.last_width = width;
	    pcls->raster.first_y = pcls->raster.y;
	  }
	if ( !pcls->raster.height_set || pcls->raster.y < pcls->raster.height )
	  { int code = (*dev_proc(dev, image_data))
	      (dev, pcls->raster.image_info, &data, 0,
	       (width * pcls->raster.depth + 7) >> 3, 1);
	    if ( code < 0 )
	      return code;
	    pcls->raster.y++;
	    pcls->cap.y += (pcl_coord_scale / pcls->raster.resolution) *
	      pcls->raster.y_direction;
	  }
	return 0;
}

/* ------ Commands ------ */

private int /* ESC * t <dpi> R */
pcl_raster_graphics_resolution(pcl_args_t *pargs, pcl_state_t *pcls)
{	int resolution = int_arg(pargs);
	if ( pcls->raster.graphics_mode )
	  return 0;		/* locked out */
	switch ( resolution )
	  {
	  case 75: case 100: case 150: case 200: case 300: case 600:
	    pcls->raster.resolution = resolution;
	    break;
	  default:
	    return e_Range;
	  }
	return 0;
}

private int /* ESC * r <0|3> F */
pcl_raster_graphics_presentation(pcl_args_t *pargs, pcl_state_t *pcls)
{	int orient = int_arg(pargs);
	if ( pcls->raster.graphics_mode )
	  return 0;		/* locked out */
	switch ( orient )
	  {
	  case 0: case 3:
	    break;
	  default:
	    return e_Range;
	  }
	pcls->raster.across_physical_page = orient != 0;
	return 0;
}

private int /* ESC * r <height_pixels> T */
pcl_raster_height(pcl_args_t *pargs, pcl_state_t *pcls)
{	if ( pcls->raster.graphics_mode )
	  return 0;		/* locked out */
	pcls->raster.height = uint_arg(pargs);
	pcls->raster.height_set = true;
	return 0;
}

private int /* ESC * r <width_pixels> S */
pcl_raster_width(pcl_args_t *pargs, pcl_state_t *pcls)
{	if ( pcls->raster.graphics_mode )
	  return 0;		/* locked out */
	pcls->raster.width = uint_arg(pargs);
	pcls->raster.width_set = true;
	return 0;
}

private int /* ESC * r <cap_enum> A */
pcl_start_raster_graphics(pcl_args_t *pargs, pcl_state_t *pcls)
{	int setting = int_arg(pargs);

	if ( pcls->raster.graphics_mode )
	  return 0;		/* locked out */
	if ( setting > 1 )
	  setting = 0;
	return pcl_begin_raster_graphics(pcls, setting);
}

private int /* ESC * b <method> M */
pcl_set_compression_method(pcl_args_t *pargs, pcl_state_t *pcls)
{	int method = int_arg(pargs);

	if ( method < countof(pcl_compression_methods) &&
	     pcl_compression_methods[method].proc != 0
	   )
	  pcls->raster.compression = method;
	return 0;
}

/* This command is referenced in a couple of places in the PCL5 manual, */
/* but it isn't documented anywhere, and it doesn't appear in the master */
/* table of PCL5 commands in the Comparison Guide. */
private int /* ESC * b <> S */
pcl_seed_row_source(pcl_args_t *pargs, pcl_state_t *pcls)
{	return e_Unimplemented;
}

/* This is exported for pccrastr.c. */
int /* ESC * b <count> W */
pcl_transfer_raster_data(pcl_args_t *pargs, pcl_state_t *pcls)
{	enter_graphics_mode_if_needed(pcls);
	return
	  pcl_read_raster_data(pargs, pcls,
			       (pcls->raster.planar ? 1 : pcls->raster.depth),
			       true);
}

private int /* ESC * b <dy> Y */
pcl_raster_y_offset(pcl_args_t *pargs, pcl_state_t *pcls)
{	int offset = int_arg(pargs);
	int dy = offset * pcls->raster.y_direction;
	int code = 0;

	if ( !pcls->raster.graphics_mode )
	  return 0;		/* per PCL5 Color manual, p. 6-19 */
	clear_rows(pcls, pcls->raster.depth);
	if ( dy >= 0 )
	  { dy = min(dy, pcls->raster.height - pcls->raster.y);
	    offset = dy;
	  }
	else
	  { dy = min(-dy, pcls->raster.y);
	    offset = -dy;
	  }
	if ( pcls->raster.image.ImageMask )
	  { /* Just skip the rows. */
	    pcls->raster.y += dy;
	    pcls->cap.y = (coord)(pcl_coord_scale * pcls->raster.resolution) *
	      dy;
	    pcls->raster.last_width = 0;	/* force a new image */
	    return 0;
	  }
	/* Actually image blank rows. */
	while ( --offset >= 0 &&
		(code = pcl_image_row(pcls, &pcls->raster.row[0])) >= 0
	      )
	  ;
	return code;
}

/* We break out this procedure so the parser can call it to */
/* exit raster graphics mode implicitly. */
private int
end_raster_graphics(pcl_state_t *pcls)
{	bool across =
	  pcls->raster.across_physical_page &&
	  (pcls->orientation & 1);
	int code = 0;

	if ( !pcls->raster.graphics_mode )
	  return 0;
	/* If the height is set, */
	/* fill out the extra lines. */
	if ( pcls->raster.height_set && pcls->raster.height > pcls->raster.y )
	  { pcl_args_t args;

	    arg_set_uint(&args, pcls->raster.height - pcls->raster.y);
	    code = pcl_raster_y_offset(&args, pcls);
	  }
	pcls->raster.graphics_mode = false;
	if ( pcls->raster.image_info )
	  { gx_device *dev = gs_currentdevice(pcls->pgs);
	    int code2 = (*dev_proc(dev, end_image))
	      (dev, pcls->raster.image_info, true);

	    pcls->raster.image_info = 0;
	    if ( code >= 0 )
	      code = code2;
	  }
	{ int i;
	  for ( i = pcls->raster.depth; --i >= 0; )
	    { gs_free_object(pcls->memory, pcls->raster.row[i].data,
			     "end raster graphics(row)");
	      initialize_row(&pcls->raster.row[i]);
	    }
	}
	clear_rows(pcls, pcls->raster.depth);
	/* Restore the CTM if we modified it. */
	if ( across )
	  { gs_point cap_dev, cap_image;

	    /* Convert CAP to device coordinates, since it mustn't move. */
	    gs_transform(pcls->pgs, (floatp)pcls->cap.x, (floatp)pcls->cap.y,
			 &cap_dev);
	    pcl_set_graphics_state(pcls, true);
	    gs_itransform(pcls->pgs, cap_dev.x, cap_dev.y, &cap_image);
	    pcls->cap.x = (coord)cap_image.x;
	    pcls->cap.y = (coord)cap_image.y;
	  }
	return code;
}
private int /* ESC * r B */
pcl_end_raster_graphics(pcl_args_t *pargs, pcl_state_t *pcls)
{	return end_raster_graphics(pcls);
}

private int /* ESC * r C */
pcl_end_clear_raster_graphics(pcl_args_t *pargs, pcl_state_t *pcls)
{	int code = end_raster_graphics(pcls);
	if ( code < 0 )
	  return code;
	pcls->raster.compression = 0;
	pcls->raster.left_margin = 0;
	return 0;
}

/* Initialization */
private int
rtraster_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('*')
	  {'t', 'R',
	     PCL_COMMAND("Raster Graphics Resolution",
			 pcl_raster_graphics_resolution,
			 pca_neg_error|pca_big_error|pca_raster_graphics)},
	  {'r', 'T',
	     PCL_COMMAND("Raster Height", pcl_raster_height,
			 pca_neg_error|pca_big_error|pca_raster_graphics)},
	  {'r', 'S',
	     PCL_COMMAND("Raster Width", pcl_raster_width,
			 pca_neg_error|pca_big_error|pca_raster_graphics)},
	  {'r', 'A',
	     PCL_COMMAND("Start Raster Graphics", pcl_start_raster_graphics,
			 pca_neg_error|pca_big_error)},
	  {'b', 'M',
	     PCL_COMMAND("Set Compression Method", pcl_set_compression_method,
			 pca_neg_ignore|pca_big_ignore|pca_raster_graphics)},
	  {'b', 'W',
	     PCL_COMMAND("Transfer Raster Data", pcl_transfer_raster_data,
			 pca_bytes|pca_raster_graphics)},
	  {'b', 'Y',
	     PCL_COMMAND("Raster Y Offset", pcl_raster_y_offset,
			 pca_neg_ok|pca_big_clamp|pca_raster_graphics)},
	  {'r', 'C',
	     PCL_COMMAND("End Clear Raster Graphics",
			 pcl_end_clear_raster_graphics,
			 pca_raster_graphics)},
	END_CLASS
	return 0;
}
private void
rtraster_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { pcls->raster.graphics_mode = false;
	    if ( type & pcl_reset_initial )
	      pcls->raster.end_graphics = end_raster_graphics;
	    pcls->raster.left_margin = 0;
	    pcls->raster.resolution = 75;
	    pcls->raster.across_physical_page = true;
	    pcls->raster.height_set = false;
	    pcls->raster.width_set = false;
	    pcls->raster.compression = 0;
	    { int i;
	      for ( i = 0; i < countof(pcls->raster.row); ++i )
		initialize_row(&pcls->raster.row[i]);
	    }
	    pcls->raster.y_direction = 1;
	    pcls->raster.color_row.data = 0;
	    /* Initialize for monochrome operation. */
	    pcls->raster.depth = 1;
	  }
}
const pcl_init_t rtraster_init = {
  rtraster_do_init, rtraster_do_reset
};
/* The following commands are only registered in PCL mode. */
private int
rtraster_pcl_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('*')
	  {'r', 'F',
	     PCL_COMMAND("Raster Graphics Presentation",
			 pcl_raster_graphics_presentation,
			 pca_neg_error|pca_big_error|pca_raster_graphics)},
	  {'b', 'S',
	     PCL_COMMAND("Seed Row Source", pcl_seed_row_source,
			 pca_raster_graphics)},
	  {'r', 'B',
	     PCL_COMMAND("End Raster Graphics", pcl_end_raster_graphics,
			 pca_raster_graphics)},
	END_CLASS
	return 0;
}
const pcl_init_t rtraster_pcl_init = {
  rtraster_pcl_do_init, 0
};
