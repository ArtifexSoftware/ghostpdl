/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevps.c */
/* PostScript-writing driver */
#include "math_.h"
#include "time_.h"
#include "gx.h"
#include "gserrors.h"
#include "gscdefs.h"
#include "gsmatrix.h"		/* for gsiparam.h */
#include "gsiparam.h"
#include "gsparam.h"
#include "gxdevice.h"
#include "gscspace.h"
#include "gxdcolor.h"
#include "gdevpsdf.h"
#include "gdevpstr.h"
#include "strimpl.h"
#include "sa85x.h"

/****************************************************************
 * Notes:
 *	ASCII85EncodePages should use ASCIIHexEncode if LanguageLevel < 2.
 *	Images are never compressed; in fact, none of the other
 *	  Distiller parameters do anything.
 ****************************************************************/

/* ---------------- Device definition ---------------- */

/* Define the size of the internal stream buffer. */
/* (This is not a limitation: it only affects performance.) */
#define sbuf_size 512

/* Device procedures */
private dev_proc_open_device(psw_open);
private dev_proc_output_page(psw_output_page);
private dev_proc_close_device(psw_close);
private dev_proc_copy_mono(psw_copy_mono);
private dev_proc_copy_color(psw_copy_color);
private dev_proc_put_params(psw_put_params);
private dev_proc_get_params(psw_get_params);
private dev_proc_fill_mask(psw_fill_mask);
private dev_proc_begin_image(psw_begin_image);
private dev_proc_image_data(psw_image_data);
private dev_proc_end_image(psw_end_image);

#define X_DPI 720
#define Y_DPI 720

typedef struct gx_device_pswrite_s {
  gx_device_psdf_common;
	/* Settable parameters */
#define LanguageLevel_default 2.0
  float LanguageLevel;
	/* End of parameters */
  bool ProduceEPS;
  bool first_page;
  long bbox_position;
  stream *image_stream;
  byte *image_buf;
#define num_cached_images 137
  gx_bitmap_id image_cache[num_cached_images];
} gx_device_pswrite;

gs_private_st_suffix_add2_final(st_device_pswrite, gx_device_pswrite,
  "gx_device_pswrite", device_pswrite_enum_ptrs, device_pswrite_reloc_ptrs,
  gx_device_finalize, st_device_psdf, image_stream, image_buf);

#define psw_procs\
	{	psw_open,\
		gx_upright_get_initial_matrix,\
		NULL,			/* sync_output */\
		psw_output_page,\
		psw_close,\
		gx_default_rgb_map_rgb_color,\
		gx_default_rgb_map_color_rgb,\
		gdev_vector_fill_rectangle,\
		NULL,			/* tile_rectangle */\
		psw_copy_mono,\
		psw_copy_color,\
		NULL,			/* draw_line */\
		NULL,			/* get_bits */\
		psw_get_params,\
		psw_put_params,\
		NULL,			/* map_cmyk_color */\
		NULL,			/* get_xfont_procs */\
		NULL,			/* get_xfont_device */\
		NULL,			/* map_rgb_alpha_color */\
		gx_page_device_get_page_device,\
		NULL,			/* get_alpha_bits */\
		NULL,			/* copy_alpha */\
		NULL,			/* get_band */\
		NULL,			/* copy_rop */\
		gdev_vector_fill_path,\
		gdev_vector_stroke_path,\
		psw_fill_mask,\
		gdev_vector_fill_trapezoid,\
		gdev_vector_fill_parallelogram,\
		gdev_vector_fill_triangle,\
		NULL /****** WRONG ******/,	/* draw_thin_line */\
		psw_begin_image,\
		psw_image_data,\
		psw_end_image,\
		NULL,			/* strip_tile_rectangle */\
		NULL/******psw_strip_copy_rop******/\
	}

gx_device_pswrite far_data gs_pswrite_device =
{	std_device_dci_type_body(gx_device_pswrite, 0, "pswrite",
	  &st_device_pswrite,
	  DEFAULT_WIDTH_10THS*X_DPI/10, DEFAULT_HEIGHT_10THS*Y_DPI/10,
	  X_DPI, Y_DPI, 3, 24, 255, 255, 256, 256),
	psw_procs,
	psdf_initial_values(1 /*true*/),	/* (ASCII85EncodePages) */
	LanguageLevel_default,	/* LanguageLevel */
	0/*false*/		/* ProduceEPS */
};

gx_device_pswrite far_data gs_epswrite_device =
{	std_device_dci_type_body(gx_device_pswrite, 0, "epswrite",
	  &st_device_pswrite,
	  DEFAULT_WIDTH_10THS*X_DPI/10, DEFAULT_HEIGHT_10THS*Y_DPI/10,
	  X_DPI, Y_DPI, 3, 24, 255, 255, 256, 256),
	psw_procs,
	psdf_initial_values(1 /*true*/),	/* (ASCII85EncodePages) */
	LanguageLevel_default,	/* LanguageLevel */
	1/*true*/		/* ProduceEPS */
};

/* Vector device implementation */
private int psw_beginpage(P1(gx_device_vector *vdev));
private int psw_setcolors(P2(gx_device_vector *vdev,
			     const gx_drawing_color *pdc));
private int psw_endpath(P2(gx_device_vector *vdev, gx_path_type_t type));
private const gx_device_vector_procs psw_vector_procs = {
	/* Page management */
  psw_beginpage,
	/* Imager state */
  psdf_setlinewidth,
  psdf_setlinecap,
  psdf_setlinejoin,
  psdf_setmiterlimit,
  psdf_setdash,
  psdf_setflat,
  psdf_setlogop,
	/* Other state */
  psw_setcolors,		/* fill & stroke colors are the same */
  psw_setcolors,
	/* Paths */
  psdf_dopath,
  psdf_dorect,
  psdf_beginpath,
  psdf_moveto,
  psdf_lineto,
  psdf_curveto,
  psdf_closepath,
  psw_endpath
};

/* ---------------- File header ---------------- */

private const char *psw_ps_header[] = {
"%!PS-Adobe-3.0",
"%%Pages: (atend)",
0
};

private const char *psw_eps_header[] = {
"%!PS-Adobe-3.0 EPSF-3.0",
0
};

private const char *psw_header[] = {
"%%EndComments",
"%%BeginProlog",
"% This copyright applies to everything between here and the %%EndProlog:",
0
};

private const char *psw_prolog[] = {
"%%BeginResource: procset GS_pswrite_ProcSet",
"/GS_pswrite_ProcSet 40 dict dup begin",
"/!{bind def}bind def/X{load def}!",
"/rg/setrgbcolor X/g/setgray X/w/setlinewidth X/J/setlinecap X",
"/j/setlinejoin X/M/setmiterlimit X/d/setdash X/i/setflat X",
"/m/moveto X/l/lineto X/c/curveto X/h/closepath X",
"/lx{0 rlineto}!/ly{0 exch rlineto}!/v{currentpoint 6 2 roll c}!/y{2 copy c}!",
"/re{4 -2 roll m exch dup lx exch ly neg lx h}!",
"/q/gsave X/Q/grestore X/f/fill X/f*/eofill X/S/stroke X/rf{re f}!",
"/Y{initclip clip newpath}!/Y*{initclip eoclip newpath}!/rY{re Y}!",
"/@/currentfile X/|{string readstring pop}!",
	/* <w> <h> <x> <y> <bps/inv> <src> Ix <w> <h> <bps/inv> <mtx> <src> */
"/Ix{[1 0 0 1 9 -1 roll neg 9 -1 roll neg]exch}!",
"/It{true exch Ix imagemask}!/If{false exch Ix imagemask}!/I{exch Ix image}!",
0
};

private const char *psw_1_prolog[] = {
0
};

private const char *psw_1_5_prolog[] = {
"/Ic{Ix false 1 colorimage}!",
0
};

private const char *psw_2_prolog[] = {
"/@85{@/ASCII85Decode filter}!",
0
};

private const char *psw_end_prolog[] = {
"end def",
"%%EndResource",
"%%EndProlog",
0
};

private void
psw_put_lines(stream *s, const char **lines)
{	int i;
	for ( i = 0; lines[i] != 0; ++i )
	  pprints1(s, "%s\n", lines[i]);
}

/* ---------------- Utilities ---------------- */

/* Reset the image cache. */
private void
image_cache_reset(gx_device_pswrite *pdev)
{	int i;
	for ( i = 0; i < num_cached_images; ++i )
	  pdev->image_cache[i] = gx_no_bitmap_id;
}

/* Look up or enter an image ID in the cache. */
/* If id is gx_no_bitmap_id or enter is false, do not enter it. */
private bool
image_cache_lookup(gx_device_pswrite *pdev, gx_bitmap_id id, bool enter)
{	int i;

	if ( id == gx_no_bitmap_id )
	  return false;
	i = id % num_cached_images;
	if ( pdev->image_cache[i] == id )
	  return true;
	if ( enter )
	  pdev->image_cache[i] = id;
	return false;
}

/* Set up to write a device-pixel image. */
/* Return false if the image is empty. */
private bool
psw_image_setup(gx_device_pswrite *pdev, int x, int y, int w, int h)
{	stream *s = pdev->strm;

	if ( w <= 0 || h <= 0 )
	  return false;
	pprintd4(s, "%d %d %d %d ", w, h, x, y);
	return true;
}

/* Prepare the encoding stream for image data. */
/* Return 1 if we are using ASCII85 encoding. */
private const stream_procs filter_write_procs =
{	s_std_noavailable, s_std_noseek, s_std_write_reset,
	s_std_write_flush, s_filter_close
};
private int
psw_image_stream_setup(gx_device_pswrite *pdev)
{	pdev->image_stream = gdev_vector_stream((gx_device_vector *)pdev);
	if ( pdev->LanguageLevel >= 2 && pdev->params.ASCII85EncodePages )
	  { gs_memory_t *mem = pdev->v_memory;
	    stream *prev_stream = pdev->image_stream;
	    uint buf_size = 200; /* arbitrary */
	    byte *buf = pdev->image_buf =
	      gs_alloc_bytes(mem, buf_size, "psw_set_image_stream(buf)");
	    stream *es = pdev->image_stream =
	      s_alloc(mem, "psw_set_image_stream(stream)");

	    if ( es == 0 || buf == 0 )
	      { return_error(gs_error_VMerror);
	      }
	    s_std_init(es, buf, buf_size, &filter_write_procs, s_mode_write);
	    es->template = &s_A85E_template;
	    es->procs.process = es->template->process;
	    es->strm = prev_stream;
	    return 1;
	  }
	return 0;
}

/* Clean up after writing an image. */
private void
psw_image_cleanup(gx_device_pswrite *pdev)
{	gs_memory_t *mem = pdev->v_memory;

	if ( pdev->image_stream != 0 && pdev->image_stream != pdev->strm )
	  { sclose(pdev->image_stream);
	    gs_free_object(mem, pdev->image_stream, "psw_image_cleanup(stream)");
	    pdev->image_stream = 0;
	  }
	if ( pdev->image_buf )
	  { gs_free_object(mem, pdev->image_buf, "psw_image_cleanup(buf)");
	    pdev->image_buf = 0;
	  }
}

/* Write data for an image. */
/****** IGNORES data_x ******/
private void
psw_put_bits(stream *s, const byte *data, int data_x_bit, uint raster,
  uint width_bits, int height)
{	int y;
	for ( y = 0; y < height; ++y )
	  pwrite(s, data + (data_x_bit >> 3) + y * raster,
		 (width_bits + 7) >> 3);
}
private int
psw_image_write(gx_device_pswrite *pdev, const char *imagestr,
  const byte *data, int data_x_bit, uint raster, gx_bitmap_id id,
  uint width_bits, int height)
{	stream *s = pdev->strm;
	int code;
	const char *source;

	if ( image_cache_lookup(pdev, id, false) )
	  { pprintld1(s, "I%ld ", id);
	    pprints1(s, "%s\n", imagestr);
	    return 0;
	  }
	code = psw_image_stream_setup(pdev);
	if ( code < 0 )
	  return code;
	source = (code ? "@85" : "@");
	if ( id == gx_no_bitmap_id || width_bits * (ulong)height > 8000 ||
	     width_bits == 0 || height == 0
	   )
	  { pprints2(s, "%s %s\n", source, imagestr);
	    psw_put_bits(pdev->image_stream, data, data_x_bit, raster,
			 width_bits, height);
	    psw_image_cleanup(pdev);
	    spputc(s, '\n');
	  }
	else
	  { char str[40];

	    image_cache_lookup(pdev, id, true);
	    sprintf(str, "/I%ld %s %ld |\n",
		    id, source, ((width_bits + 7) >> 3) * (ulong)height);
	    pputs(s, str);
	    psw_put_bits(pdev->image_stream, data, data_x_bit, raster,
			 width_bits, height);
	    psw_image_cleanup(pdev);
	    pprintld1(s, "\ndef I%ld ", id);
	    pprints1(s, "%s\n", imagestr);
	  }
	return 0;
}

/* Print a matrix. */
private void
psw_put_matrix(stream *s, const gs_matrix *pmat)
{	pprintg6(s, "[%g %g %g %g %g %g]",
		 pmat->xx, pmat->xy, pmat->yx, pmat->yy, pmat->tx, pmat->ty);
}

/* ---------------- Vector device implementation ---------------- */

#define pdev ((gx_device_pswrite *)vdev)

private int
psw_beginpage(gx_device_vector *vdev)
{	stream *s = vdev->strm;
	long page = vdev->PageCount + 1;

	if ( pdev->first_page )
	  { psw_put_lines(s,
			  (pdev->ProduceEPS ? psw_eps_header : psw_ps_header));
	    if ( ftell(vdev->file) < 0 )
	      { /* File is not seekable. */
		pdev->bbox_position = -1;
		pputs(s, "%%BoundingBox: (atend)\n");
	      }
	    else
	      { /* File is seekable, leave room to rewrite bbox. */
		pdev->bbox_position = stell(s);
		pputs(s, "................................................................\n");
	      }
	    pprints1(s, "%%%%Creator: %s ", gs_product);
	    pprintld1(s, "%ld ", (long)gs_revision);
	    pprints1(s, "(%s)\n", vdev->dname);
	    { struct tm tms;
	      time_t t;
	      char date_str[25];

	      time(&t);
	      tms = *localtime(&t);
	      sprintf(date_str, "%d/%02d/%02d %02d:%02d:%02d",
		      tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
		      tms.tm_hour, tms.tm_min, tms.tm_sec);
	      pprints1(s, "%%%%CreationDate: %s\n", date_str);
	    }
	    if ( pdev->params.ASCII85EncodePages )
	      pputs(s, "%%DocumentData: Clean7Bit\n");
	    if ( pdev->LanguageLevel == 2.0 )
	      pputs(s, "%%LanguageLevel: 2\n");
	    else if ( pdev->LanguageLevel == 1.5 )
	      pputs(s, "%%Extensions: CMYK\n");
	    psw_put_lines(s, psw_header);
	    pprints1(s, "%% %s\n", gs_copyright);
	    psw_put_lines(s, psw_prolog);
	    if ( pdev->LanguageLevel < 1.5 )
	      psw_put_lines(s, psw_1_prolog);
	    else
	      { psw_put_lines(s, psw_1_5_prolog);
	        if ( pdev->LanguageLevel > 1.5 )
		  psw_put_lines(s, psw_2_prolog);
	      }
	    psw_put_lines(s, psw_end_prolog);
	  }
	pprintld2(s, "%%%%Page: %ld %ld\n%%%%BeginPageSetup\n", page, page);
	pprintg2(s, "save GS_pswrite_ProcSet begin %g %g scale\n%%%%EndPageSetup\n",
		 72.0 / vdev->HWResolution[0], 72.0 / vdev->HWResolution[1]);
	return 0;
}

private int
psw_setcolors(gx_device_vector *vdev, const gx_drawing_color *pdc)
{	/* PostScript only keeps track of a single color. */
	vdev->fill_color = *pdc;
	vdev->stroke_color = *pdc;
	return psdf_setfillcolor(vdev, pdc);
}

private int
psw_endpath(gx_device_vector *vdev, gx_path_type_t type)
{	stream *s = vdev->strm;
	const char *star = (type & gx_path_type_even_odd ? "*" : "");

	if ( type & gx_path_type_fill )
	  { if ( type & (gx_path_type_stroke | gx_path_type_clip) )
	      pprints1(s, "q f%s Q ", star);
	    else
	      pprints1(s, "f%s\n", star);
	  }
	if ( type & gx_path_type_stroke )
	  { if ( type & gx_path_type_clip )
	      pputs(s, "q S Q ");
	    else
	      pputs(s, "S\n");
	  }
	if ( type & gx_path_type_clip )
	  pprints1(s, "Y%s\n", star);
	return 0;
}

#undef pdev

/* ---------------- Driver procedures ---------------- */

#define vdev ((gx_device_vector *)dev)
#define pdev ((gx_device_pswrite *)dev)

/* ------ Open/close/page ------ */

/* Open the device. */
private int
psw_open(gx_device *dev)
{	vdev->v_memory = dev->memory; /****** WRONG ******/
	vdev->vec_procs = &psw_vector_procs;
	{ int code = gdev_vector_open_file_bbox(vdev, 512, true);
	  if ( code < 0 )
	    return code;
	}
	gdev_vector_init(vdev);
	pdev->first_page = true;
	image_cache_reset(pdev);
	return 0;
}

/* Wrap up ("output") a page. */
private int
psw_output_page(gx_device *dev, int num_copies, int flush)
{	stream *s = gdev_vector_stream(vdev);
	if ( num_copies != 1 )
	  pprintd1(s, "userdict /#copies %d put\n", num_copies);
	pprints1(s, "end %s restore\n%%%%PageTrailer\n",
		 (flush ? "showpage" : "copypage"));
	sflush(s);
	vdev->in_page = false;
	pdev->first_page = false;
	gdev_vector_reset(vdev);
	image_cache_reset(pdev);
	return 0;
}

/* Close the device. */
/* Note that if this is being called as a result of finalization, */
/* the stream may no longer exist; but the file will still be open. */
private int
psw_close(gx_device *dev)
{	FILE *f = vdev->file;

	fprintf(f, "%%%%Trailer\n%%%%Pages: %ld\n", dev->PageCount);
	{ gs_rect bbox;
	  long save_pos;

	  gx_device_bbox_bbox(vdev->bbox_device, &bbox);
	  if ( pdev->bbox_position >= 0 )
	    { save_pos = ftell(f);
	      fseek(f, pdev->bbox_position, SEEK_SET);
	    }
	  fprintf(f, "%%%%BoundingBox: %d %d %d %d\n",
		  (int)floor(bbox.p.x), (int)floor(bbox.p.y),
		  (int)ceil(bbox.q.x), (int)ceil(bbox.q.y));
	  if ( pdev->bbox_position >= 0 )
	    { fputc('%', f);
	      fseek(f, save_pos, SEEK_SET);
	    }
	}
	if ( !pdev->ProduceEPS )
	  fputs("%%EOF\n", f);
	gdev_vector_close_file(vdev);
	return 0;
}

/* ---------------- Get/put parameters ---------------- */

/* Get parameters. */
private int
psw_get_params(gx_device *dev, gs_param_list *plist)
{	int code = gdev_psdf_get_params(dev, plist);
	int ecode;

	if ( code < 0 )
	  return code;
	if ( (ecode = param_write_float(plist, "LanguageLevel", &pdev->LanguageLevel)) < 0 )
	  return ecode;
	return code;
}

/* Put parameters. */
private int
psw_put_params(gx_device *dev, gs_param_list *plist)
{	int ecode = 0;
	int code;
	gs_param_name param_name;
	float ll = pdev->LanguageLevel;

	switch ( code = param_read_float(plist, (param_name = "LanguageLevel"), &ll) )
	  {
	  case 0:
	    if ( ll == 1.0 || ll == 1.5 || ll == 2.0 )
	      break;
	    code = gs_error_rangecheck;
	  default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	  case 1:
	    ;
	  }

	if ( ecode < 0 )
	  return ecode;
	code = gdev_psdf_put_params(dev, plist);
	if ( code < 0 )
	  return code;

	pdev->LanguageLevel = ll;
	return code;
}

/* ---------------- Images ---------------- */

/* Copy a monochrome bitmap. */
private int
psw_copy_mono(gx_device *dev, const byte *data,
  int data_x, int raster, gx_bitmap_id id, int x, int y, int w, int h,
  gx_color_index zero, gx_color_index one)
{	gx_drawing_color color;
	const char *op;
	int code = 0;

	(*dev_proc(vdev->bbox_device, copy_mono))
	  ((gx_device *)vdev->bbox_device, data, data_x, raster, id,
	   x, y, w, h, zero, one);
	if ( one == gx_no_color_index )
	  { color_set_pure(&color, zero);
	    code = gdev_vector_update_fill_color((gx_device_vector *)pdev,
						 &color);
	    op = "If";
	  }
	else if ( zero == vdev->black && one == vdev->white )
	  op = "1 I";
	else
	  { if ( zero != gx_no_color_index )
	      { code = (*dev_proc(dev, fill_rectangle))(dev, x, y, w, h, zero);
	        if ( code < 0 )
		  return code;
	      }
	    color_set_pure(&color, one);
	    code = gdev_vector_update_fill_color((gx_device_vector *)pdev,
						 &color);
	    op = "It";
	  }
	if ( code < 0 )
	  return 0;
	return (psw_image_setup(pdev, x, y, w, h) ?
		psw_image_write(pdev, op, data, data_x, raster, id,
				       w, h) : 0);
}

/* Copy a color bitmap. */
private int
psw_copy_color(gx_device *dev,
  const byte *data, int data_x, int raster, gx_bitmap_id id,
  int x, int y, int w, int h)
{	int depth = dev->color_info.depth;

	(*dev_proc(vdev->bbox_device, copy_color))
	  ((gx_device *)vdev->bbox_device, data, data_x, raster, id,
	   x, y, w, h);
	if ( !psw_image_setup(pdev, x, y, w, h) )
	  return 0;
	pprintd1(pdev->strm, " %d", depth);
	return psw_image_write(pdev, "Ic", data, data_x * depth,
				      raster, id, w * depth, h);
}

/* Fill a mask. */
private int
psw_fill_mask(gx_device *dev,
  const byte *data, int data_x, int raster, gx_bitmap_id id,
  int x, int y, int w, int h,
  const gx_drawing_color *pdcolor, int depth,
  gs_logical_operation_t lop, const gx_clip_path *pcpath)
{	if ( depth > 1 ||
	     gdev_vector_update_fill_color(vdev, pdcolor) < 0 ||
	     gdev_vector_update_clip_path(vdev, pcpath) < 0 ||
	     gdev_vector_update_log_op(vdev, lop) < 0
	   )
	  return gx_default_fill_mask(dev, data, data_x, raster, id,
				      x, y, w, h, pdcolor, depth, lop, pcpath);
	(*dev_proc(vdev->bbox_device, fill_mask))
	  ((gx_device *)vdev->bbox_device, data, data_x, raster, id,
	   x, y, w, h, pdcolor, depth, lop, pcpath);
	return (psw_image_setup(pdev, x, y, w, h) ?
		psw_image_write(pdev, "It",
				       data, data_x, raster, id, w, h) : 0);
}

/* ---------------- High-level images ---------------- */

/* Start processing an image. */
private int
psw_begin_image(gx_device *dev,
  const gs_imager_state *pis, const gs_image_t *pim,
  gs_image_format_t format, const gs_int_rect *prect,
  const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
  gs_memory_t *mem, void **pinfo)
{	gdev_vector_image_enum_t *pie =
	  gs_alloc_struct(mem, gdev_vector_image_enum_t,
			  &st_vector_image_enum, "psw_begin_image");
	const gs_color_space *pcs = pim->ColorSpace;
	gs_color_space_index index;
	int num_components;
	bool can_do = prect == 0;
	int code;

	if ( pie == 0 )
	  return_error(gs_error_VMerror);
	pie->memory = mem;
	*pinfo = pie;
	if ( !pim->ImageMask )
	  { index = gs_color_space_get_index(pcs);
	    num_components = gs_color_space_num_components(pcs);
	  }
	if ( pdev->LanguageLevel < 2 && !pim->ImageMask )
	  { /*
	     * Restrict ourselves to Level 1 images: device color spaces, [0
	     * 1] decode, bits per component <= 8, no CombineWithColor.
	     */
	    if ( pim->BitsPerComponent > 8 || pim->CombineWithColor )
	      can_do = false;
	    else
	      { int i;

		switch ( index )
		  {
		  case gs_color_space_index_DeviceGray:
		  case gs_color_space_index_DeviceRGB:
		  case gs_color_space_index_DeviceCMYK:
		    for ( i = 0; i < num_components * 2; ++i )
		      if ( pim->Decode[i] != (i & 1) )
			can_do = false;
		    break;
		  default:
		    can_do = false;
		  }
	      }
	  }
	if ( !can_do ||
	     gdev_vector_begin_image(vdev, pis, pim, format, prect, pdcolor,
				     pcpath, mem, pie) < 0 ||
	     (code = psw_image_stream_setup(pdev)) < 0
	   )
	  return gx_default_begin_image(dev, pis, pim, format, prect,
					pdcolor, pcpath, mem,
					&pie->default_info);
	/* Write the image/colorimage/imagemask preamble. */
	{ stream *s = gdev_vector_stream((gx_device_vector *)pdev);
	  const char *source = (code ? "@85" : "@");
	  gs_matrix imat;

	  pputs(s, "q");
	  (*dev_proc(dev, get_initial_matrix))(dev, &imat);
	  gs_matrix_scale(&imat, 72.0 / dev->HWResolution[0],
			  72.0 / dev->HWResolution[1], &imat);
	  gs_matrix_invert(&imat, &imat);
	  gs_matrix_multiply(&ctm_only(pis), &imat, &imat);
	  psw_put_matrix(s, &imat);
	  pprintd2(s, "concat\n%d %d ", pie->width, pie->height);
	  if ( pim->ImageMask )
	    { pputs(s, (pim->Decode[0] == 0 ? "false" : "true"));
	      psw_put_matrix(s, &pim->ImageMatrix);
	      pprints1(s, "%s imagemask\n", source);
	    }
	  else
	    { pprintd1(s, "%d", pim->BitsPerComponent);
	      psw_put_matrix(s, &pim->ImageMatrix);
	      if ( index == gs_color_space_index_DeviceGray )
		pprints1(s, "%s image\n", source);
	      else
		{ if ( format == gs_image_format_chunky )
		    pprints1(s, "%s false", source);
		  else
		    pprints2(s, "%s %strue", source,
			     "dup dup dup " + (16 - num_components * 4));
		  pprintd1(s, " %d colorimage\n", num_components);
		}
	    }
	}
	return 0;
}

/* Process the next piece of an image. */
private int
psw_image_data(gx_device *dev,
  void *info, const byte **planes, int data_x, uint raster, int height)
{	gdev_vector_image_enum_t *pie = info;

	if ( pie->default_info )
	  return gx_default_image_data(dev, pie->default_info, planes,
				       data_x, raster, height);
	(*dev_proc(vdev->bbox_device, image_data))
	  ((gx_device *)vdev->bbox_device, pie->bbox_info,
	   planes, data_x, raster, height);
	{ int plane;
	  for ( plane = 0; plane < pie->num_planes; ++plane )
	    psw_put_bits(pdev->image_stream, planes[plane],
			 data_x * pie->bits_per_pixel, raster,
			 pie->bits_per_row, height);
	}
	return (pie->y += height) >= pie->height;
}

/* Clean up by releasing the buffers. */
private int
psw_end_image(gx_device *dev, void *info, bool draw_last)
{	gdev_vector_image_enum_t *pie = info;
	int code;

	code = gdev_vector_end_image(vdev, (gdev_vector_image_enum_t *)pie,
				     draw_last, pdev->white);
	if ( code > 0 )
	  { psw_image_cleanup(pdev);
	    pputs(pdev->strm, "\nQ\n");
	  }
	return code;
}
