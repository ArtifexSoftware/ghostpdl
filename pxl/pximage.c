/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pximage.c */
/* PCL XL bitmap painting operators */

#include "std.h"
#include "pxerrors.h"
#include "pxoper.h"
#include "pxstate.h"
#include "gdebug.h"
#include "gsrop.h"
#include "gsrefct.h"
#include "gsstruct.h"
#include "gsstate.h"
#include "gscoord.h"
#include "gsimage.h"
#include "gspaint.h"
#include "gspath.h"
#include "gspath2.h"
#include "gsuid.h"		/* for gxpcolor.h */
#include "gsutil.h"
#include "gxbitmap.h"
#include "gxcspace.h"
#include "gxdevice.h"		/* for gxpcolor.h */
#include "gxcolor2.h"
#include "gxdcolor.h"
#include "gxpcolor.h"
#include "scommon.h"
#include "strimpl.h"
#include "srlx.h"
#include "pldraw.h"
#ifdef PXL2_0
#include "jpeglib_.h"
#include "sdct.h"
#endif

/* GC descriptors */
private_st_px_pattern();
#define ppat ((px_pattern_t *)vptr)
private ENUM_PTRS_BEGIN(px_pattern_enum_ptrs) return 0;
	ENUM_CONST_STRING_PTR(0, px_pattern_t, palette);
	ENUM_PTR(1, px_pattern_t, data);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(px_pattern_reloc_ptrs) {
	RELOC_CONST_STRING_PTR(px_pattern_t, palette);
	RELOC_PTR(px_pattern_t, data);
} RELOC_PTRS_END
#undef ppat

/* Define the "freeing" procedure for patterns in a dictionary. */
void
px_free_pattern(gs_memory_t *mem, void *vptr, client_name_t cname)
{	px_pattern_t *pattern = vptr;
	rc_decrement(pattern, cname);
}
/* Define the real freeing procedure for patterns. */
private void
rc_free_px_pattern(gs_memory_t *mem, void *vptr, client_name_t cname)
{	px_pattern_t *pattern = vptr;

	gs_free_string(mem, pattern->palette.data, pattern->palette.size,
		       cname);
	gs_free_object(mem, pattern->data, cname);
	gs_free_object(mem, pattern, cname);
}
/* Define the purging procedure for the Pattern cache. */
/* The proc_data points to a pxePatternPersistence_t that specifies */
/* the maximum persistence level to purge. */
private bool
px_pattern_purge_proc(gx_color_tile *ctile, void *proc_data)
{	return ctile->uid.id % pxePatternPersistence_next <=
	  *(pxePatternPersistence_t *)proc_data;
}
void
px_purge_pattern_cache(px_state_t *pxs, pxePatternPersistence_t max_persist)
{	gx_pattern_cache_winnow(gstate_pattern_cache(pxs->pgs),
				px_pattern_purge_proc,
				(void *)&max_persist);
}

/* Define the structure for enumerating a bitmap being downloaded. */
typedef struct px_bitmap_enum_s {
    gs_memory_t *mem;            /* used only for the jpeg filter */
    uint data_per_row;		 /* ditto minus possible trailing padding */
    bool initialized;
    stream_RLD_state rld_stream_state;	/* decompressor states */
#ifdef PXL2_0
    stream_DCT_state dct_stream_state;
    jpeg_decompress_data jdd;
#endif
} px_bitmap_enum_t;

/* Define our image enumerator. */
#ifndef px_image_enum_DEFINED
#  define px_image_enum_DEFINED
typedef struct px_image_enum_s px_image_enum_t;
#endif
struct px_image_enum_s {
  gs_color_space color_space;	/* must be first, for subclassing */
  gs_image_t image;
  byte *rows;			/* buffer for rows */
  int num_rows;
  int next_row;
  uint raster;
  void *info;			/* state structure for driver */
  px_bitmap_enum_t benum;
};
gs_private_st_suffix_add2(st_px_image_enum, px_image_enum_t, "px_image_enum_t",
  px_image_enum_enum_ptrs, px_image_enum_reloc_ptrs, st_color_space,
  image.ColorSpace, rows);

/* ---------------- Utilities ---------------- */

/* Extract the parameters for reading a bitmap image or raster pattern. */
/* Attributes: pxaColorMapping, pxaColorDepth, pxaSourceWidth, */
/* pxaSourceHeight, pxaDestinationSize. */
private int
begin_bitmap(px_bitmap_params_t *params, px_bitmap_enum_t *benum,
  const px_args_t *par, const px_state_t *pxs)
{	px_gstate_t *pxgs = pxs->pxgs;
	int depth = "\001\004\010"[par->pv[1]->value.i];
	int num_components = (pxgs->color_space == eGray ? 1 : 3);

	params->width = par->pv[2]->value.i;
	params->height = par->pv[3]->value.i;
	params->depth = depth;
	params->color_space = pxgs->color_space;
	if ( par->pv[0]->value.i == eIndexedPixel )
	  { if ( pxgs->palette.data == 0 )
	      return_error(errorMissingPalette);
	    if ( pxgs->palette.size != (1 << depth) * num_components )
	      return_error(errorImagePaletteMismatch);
	    params->indexed = true;
	    num_components = 1;
	  }
	else
	  params->indexed = false;
	params->dest_width = real_value(par->pv[4], 0);
	params->dest_height = real_value(par->pv[4], 1);
	benum->data_per_row =
	  round_up(params->width * params->depth * num_components, 8) >> 3;
	benum->mem = pxs->memory;
	benum->initialized = false;
	return 0;
}

/*
 * Read a (possibly partial) row of bitmap data.  This is most of the
 * implementation of ReadImage and ReadRastPattern.  We use source.position
 * to track the uncompressed input byte position within the current block.
 * Return 0 if we've processed all the data in the block, 1 if we have a
 * complete scan line, pxNeedData for an incomplete scan line, or <0 for
 * an error condition.  *pdata must point to a scan line buffer; this
 * routine may reset it to point into the input buffer (if a complete
 * scan line is available).
 * Attributes: pxaStartLine (ignored), pxaBlockHeight, pxaCompressMode.
 */
private int
read_bitmap(px_bitmap_enum_t *benum, byte **pdata, px_args_t *par)
{	uint input_per_row = round_up(benum->data_per_row, 4);
	ulong end_pos;
#ifdef PXL2_0
	if ( par->pv[3] )
	    input_per_row = round_up(benum->data_per_row, par->pv[3]->value.i);
#endif
	/* ####### wrong ######## */
	/*	if ( par->pv[2]->value.i == eJPEGCompression ) 
		input_per_row = benum->data_per_row; */
	end_pos = (ulong)input_per_row * par->pv[1]->value.i;
	if ( par->source.position >= end_pos )
	  return 0;
	{ uint data_per_row = benum->data_per_row;
	  uint pos_in_row = par->source.position % input_per_row;
	  const byte *data = par->source.data;
	  uint avail = par->source.available;
	  uint used;

	    if ( par->pv[2]->value.i == eRLECompression )
	      { /* RLE decompress the input. */
        	stream_RLD_state *ss = (&benum->rld_stream_state);
		stream_cursor_read r;
		stream_cursor_write w;

		if ( !benum->initialized )
		  { ss->EndOfData = false;
		    s_RLD_init_inline(ss);
		    benum->initialized = true;
		  }
		r.ptr = data - 1;
		r.limit = r.ptr + avail;
		if ( pos_in_row < data_per_row )
		  { /* Read more of the current row. */
		    byte *data = *pdata;

		    w.ptr = data + pos_in_row - 1;
		    w.limit = data + data_per_row - 1;
		    (*s_RLD_template.process)((stream_state *)ss, &r, &w, false);
		    used = w.ptr + 1 - data - pos_in_row;
		    pos_in_row += used;
		    par->source.position += used;
		  }
		if ( pos_in_row >= data_per_row && pos_in_row < input_per_row )
		  { /* We've read all the real data; skip the padding. */
		    byte pad[3];	/* maximum padding per row */

		    w.ptr = pad - 1;
		    w.limit = w.ptr + input_per_row - pos_in_row;
		    (*s_RLD_template.process)((stream_state *)ss, &r, &w, false);
		    used = w.ptr + 1 - pad;
		    pos_in_row += used;
		    par->source.position += used;
		  }
		used = r.ptr + 1 - data;
		par->source.data = r.ptr + 1;
		par->source.available = avail - used;
		return (pos_in_row < input_per_row ? pxNeedData : 1);
	      }
#ifdef PXL2_0
	    else if ( par->pv[2]->value.i == eJPEGCompression ) {
		stream_DCT_state *ss = (&benum->dct_stream_state);
		stream_cursor_read r;
		stream_cursor_write w;
		if ( !benum->initialized )
		  { 
		      jpeg_decompress_data *jddp = &(benum->jdd);
		      /* use the graphics library support for DCT streams */
		      s_DCTD_template.set_defaults((stream_state *)ss);
		      ss->data.decompress = jddp;
		      jddp->memory = ss->jpeg_memory = benum->mem; 	/* set now for allocation */
		      jddp->scanline_buffer = NULL;                     /* set this early for safe error exit */
		      if ( gs_jpeg_create_decompress(ss) < 0 )
			  return_error(errorInsufficientMemory);
		      (*s_DCTD_template.init)((stream_state *)ss);
		      jddp->template = s_DCTD_template;
		      benum->initialized = true;
		  }
		r.ptr = data - 1;
		r.limit = r.ptr + avail;
		if ( pos_in_row < data_per_row )
		  { /* Read more of the current row. */
		    byte *data = *pdata;

		    w.ptr = data + pos_in_row - 1;
		    w.limit = data + data_per_row - 1;
		    (*s_DCTD_template.process)((stream_state *)ss, &r, &w, false);
		    used = w.ptr + 1 - data - pos_in_row;
		    pos_in_row += used;
		    par->source.position += used;
		  }
		if ( pos_in_row >= data_per_row && pos_in_row < input_per_row )
		  { /* We've read all the real data; skip the padding. */
		      int used = input_per_row - data_per_row;
		      pos_in_row += used;
		      par->source.position += used;
		      r.ptr += used;
		  }
		used = r.ptr + 1 - data;
		par->source.data = r.ptr + 1;
		par->source.available = avail - used;
		return (pos_in_row < input_per_row ? pxNeedData : 1);
	      }
#endif
	    else
	      { /* Just read the input. */
		int code;
		if ( avail >= input_per_row && pos_in_row == 0 )
		  { /* Use the data directly from the input buffer. */
		    *pdata = (byte *)data;	/* actually const */
		    used = input_per_row;
		    code = 1;
		  }
		else
		  { used = min(avail, input_per_row - pos_in_row);
		    if ( pos_in_row < data_per_row )
		      memcpy(*pdata + pos_in_row, data,
			     min(used, data_per_row - pos_in_row));
		    code = (pos_in_row + used < input_per_row ?
			    pxNeedData : 1);
		  }
		par->source.position += used;
		par->source.data = data + used;
		par->source.available = avail - used;
		return code;
	      }
	  }
}

/* ---------------- Image operators ---------------- */

const byte apxBeginImage[] = {
  pxaColorMapping, pxaColorDepth, pxaSourceWidth, pxaSourceHeight,
  pxaDestinationSize, 0, 0
};
int
pxBeginImage(px_args_t *par, px_state_t *pxs)
{	gs_point origin;
	px_bitmap_params_t params;
	px_bitmap_enum_t benum;
	gs_state *pgs = pxs->pgs;
	px_gstate_t *pxgs = pxs->pxgs;
	px_image_enum_t *pxenum;
	int code;

	if ( gs_currentpoint(pgs, &origin) < 0 )
	  return_error(errorCurrentCursorUndefined);
	/*
	 * If the current logical operation doesn't involve the texture,
	 * don't set a null brush, which would cause the image not to
	 * appear.
	 */
	if ( pxs->pxgs->brush.type == pxpNull &&
	     !rop3_uses_T(gs_currentrasterop(pgs))
	   )
	  code = gs_setgray(pgs, 0.0);
	else
	  code = px_set_paint(&pxgs->brush, pxs);
	if ( code < 0 )
	  return code;
	/*
	 * Make sure the proper halftone is current.
	 */
	code = px_set_halftone(pxs);
	if ( code < 0 )
	  return code;
	code = begin_bitmap(&params, &benum, par, pxs);
	if ( code < 0 )
	  return code;
	pxenum =
	  gs_alloc_struct(pxs->memory, px_image_enum_t,
			  &st_px_image_enum, "setup_bitmap(pxenum)");

	if ( pxenum == 0 )
	  return_error(errorInsufficientMemory);
	{ uint raster = round_up(benum.data_per_row, align_bitmap_mod);
	  int count = 400 / raster;

	  pxenum->num_rows = count = max(count, 1);
	  pxenum->next_row = 0;
	  pxenum->raster = raster;
	  pxenum->rows = gs_alloc_byte_array(pxs->memory, count, raster,
					     "pxReadImage(rows)");
	  if ( pxenum->rows == 0 )
	    code = gs_note_error(errorInsufficientMemory);
	  else
	    code = px_image_color_space(&pxenum->color_space, &pxenum->image,
					&params, &pxgs->palette, pgs);
	}
	if ( code < 0 )
	  { gs_free_object(pxs->memory, pxenum->rows, "pxReadImage(rows)");
	    gs_free_object(pxs->memory, pxenum, "pxReadImage(pxenum)");
	    return code;
	  }
	/* Set up the image parameters. */
	pxenum->image.Width = params.width;
	pxenum->image.Height = params.height;
	{ gs_matrix imat, dmat;
	  /* We need the cast because height is unsigned. */
	  /* We also need to account for the upside-down H-P */
	  /* coordinate system. */
	  gs_make_scaling(params.width, params.height, &imat);
	  gs_make_translation(origin.x, origin.y, &dmat);
	  gs_matrix_scale(&dmat, params.dest_width, params.dest_height,
			  &dmat);
	  /* The ImageMatrix is dmat' * imat. */
	  gs_matrix_invert(&dmat, &dmat);
	  gs_matrix_multiply(&dmat, &imat, &pxenum->image.ImageMatrix);
	}
	pxenum->image.CombineWithColor = true;
	code = pl_begin_image(pgs, &pxenum->image, &pxenum->info);
	if ( code < 0 )
	  { gs_free_object(pxs->memory, pxenum->rows, "pxReadImage(rows)");
	    gs_free_object(pxs->memory, pxenum, "pxBeginImage(pxenum)");
	    return code;
	  }
	pxenum->benum = benum;
	pxs->image_enum = pxenum;
	return 0;
}

#ifdef PXL2_0
const byte apxReadImage[] = {
  pxaStartLine, pxaBlockHeight, pxaCompressMode, 0, pxaPadBytesMultiple, pxaBlockByteLength
};
#else
const byte apxReadImage[] = {
  pxaStartLine, pxaBlockHeight, pxaCompressMode, 0, 0
};
#endif
int
pxReadImage(px_args_t *par, px_state_t *pxs)
{	px_image_enum_t *pxenum = pxs->image_enum;
	gx_device *dev = gs_currentdevice(pxs->pgs);

	if ( par->pv[1]->value.i == 0 )
	  return 0;		/* no data */
	/* Make a quick check for the first call, when no data is available. */
	if ( par->source.available == 0 )
	  return pxNeedData;
	if ( pxenum->num_rows == 1 )
	  { /* We only had space to buffer a single row. */
	    for ( ; ; )
	      { byte *data = pxenum->rows;
		int code = read_bitmap(&pxenum->benum, &data, par);

		if ( code != 1 )
		  return code;
		code = (*dev_proc(dev, image_data))
		  (dev, pxenum->info, &data, 0, pxenum->benum.data_per_row, 1);
		if ( code < 0 )
		  return code;
		pxs->have_page = true;
	      }
	  }
	else
	  { /* Buffer multiple rows to reduce fixed overhead. */
	    for ( ; ; )
	      { byte *data = pxenum->rows + pxenum->next_row * pxenum->raster;
	        byte *rdata = data;
		int code = read_bitmap(&pxenum->benum, &rdata, par);

		if ( code != 1 )
		  return code;
		if ( rdata != data )
		  memcpy(data, rdata, pxenum->benum.data_per_row);
		if ( ++(pxenum->next_row) == pxenum->num_rows )
		  { code = (*dev_proc(dev, image_data))
		      (dev, pxenum->info, &pxenum->rows, 0,
		       pxenum->raster, pxenum->num_rows);
		    pxenum->next_row = 0;
		    if ( code < 0 )
		      return code;
		  }
		pxs->have_page = true;
	      }
	  }
}

const byte apxEndImage[] = {0, 0};
int
pxEndImage(px_args_t *par, px_state_t *pxs)
{	gx_device *dev = gs_currentdevice(pxs->pgs);
	px_image_enum_t *pxenum = pxs->image_enum;

	if ( pxenum->next_row > 0 )
	  { /* Output any remaining rows. */
	    int code = (*dev_proc(dev, image_data))
	      (dev, pxenum->info, &pxenum->rows, 0,
	       pxenum->raster, pxenum->next_row);
	    pxenum->next_row = 0;
	    if ( code < 0 )
	      return code;
	    pxs->have_page = true;
	  }
	(*dev_proc(dev, end_image))(dev, pxenum->info, true);
	gs_free_object(pxs->memory, pxenum->rows, "pxEndImage(rows)");
	gs_free_object(pxs->memory, pxenum, "pxEndImage(pxenum)");
	pxs->image_enum = 0;
	return 0;
}

/* ---------------- Raster pattern operators ---------------- */

/* Define the enumerator for downloading raster patterns. */
#ifndef px_pattern_enum_DEFINED
#  define px_pattern_enum_DEFINED
typedef struct px_pattern_enum_s px_pattern_enum_t;
#endif
struct px_pattern_enum_s {
  px_bitmap_enum_t benum;
  integer pattern_id;
  pxePatternPersistence_t persistence;
  px_pattern_t *pattern;
};
gs_private_st_ptrs1(st_px_pattern_enum, px_pattern_enum_t,
  "px_pattern_enum_t", pattern_enum_enum_ptrs, pattern_enum_reloc_ptrs,
  pattern);

const byte apxBeginRastPattern[] = {
  pxaColorMapping, pxaColorDepth, pxaSourceWidth, pxaSourceHeight,
  pxaDestinationSize, pxaPatternDefineID, pxaPatternPersistence, 0, 0
};
int
pxBeginRastPattern(px_args_t *par, px_state_t *pxs)
{	gs_memory_t *mem = pxs->memory;
	px_bitmap_params_t params;
	px_pattern_t *pattern;
	px_pattern_enum_t *pxenum;
	px_bitmap_enum_t benum;
	byte *data;
	uint psize;
	byte *pdata;
	int code = begin_bitmap(&params, &benum, par, pxs);

	if ( code < 0 )
	  return code;
	rc_alloc_struct_1(pattern, px_pattern_t, &st_px_pattern, mem,
			  return_error(errorInsufficientMemory),
			  "raster pattern");
	pattern->rc.free = rc_free_px_pattern;
	data = gs_alloc_byte_array(mem, params.height, benum.data_per_row,
				   "raster pattern data");
	if ( params.indexed )
	  { psize = pxs->pxgs->palette.size;
	    pdata = gs_alloc_string(mem, psize, "raster pattern palette");
	    if ( pdata != 0 )
	      memcpy(pdata, pxs->pxgs->palette.data, psize);
	  }
	else
	  { psize = 0;
	    pdata = 0;
	  }
	pxenum = gs_alloc_struct(mem, px_pattern_enum_t, &st_px_pattern_enum,
				 "raster pattern enum");
	if ( data == 0 || (params.indexed && pdata == 0) || pxenum == 0 )
	  { gs_free_object(mem, pxenum, "raster pattern enum");
	    gs_free_string(mem, pdata, psize, "raster pattern palette");
	    gs_free_object(mem, data, "raster pattern data");
	    gs_free_object(mem, pattern, "raster pattern");
	    return_error(errorInsufficientMemory);
	  }
	pxenum->benum = benum;
	pxenum->pattern_id = par->pv[5]->value.i;
	pxenum->persistence = par->pv[6]->value.i;
	pattern->params = params;
	pattern->palette.data = pdata;
	pattern->palette.size = psize;
	pattern->data = data;
	pattern->id = gs_next_ids(1);
	pxenum->pattern = pattern;
	pxs->pattern_enum = pxenum;
	return 0;
}

#ifdef PXL2_0
const byte apxReadRastPattern[] = {
  pxaStartLine, pxaBlockHeight, pxaCompressMode, 0, pxaPadBytesMultiple, pxaBlockByteLength
};
#else
const byte apxReadRastPattern[] = {
  pxaStartLine, pxaBlockHeight, pxaCompressMode, 0, 0
};
#endif

int
pxReadRastPattern(px_args_t *par, px_state_t *pxs)
{	px_pattern_enum_t *pxenum = pxs->pattern_enum;
	int code;
	uint input_per_row = round_up(pxenum->benum.data_per_row, 4);
#ifdef PXL2_0
	if ( par->pv[3] )
	    input_per_row = round_up(pxenum->benum.data_per_row, par->pv[3]->value.i);
#endif
	/* Make a quick check for the first call, when no data is available. */
	if ( par->source.available == 0 && par->pv[1]->value.i != 0 )
	  return pxNeedData;
	for ( ; ; )
	  { 
	    byte *data = pxenum->pattern->data +
	      (par->pv[0]->value.i +
	       par->source.position / input_per_row)
	        * pxenum->benum.data_per_row;
	    byte *rdata = data;

	    code = read_bitmap(&pxenum->benum, &rdata, par);
	    if ( code != 1 )
	      break;
	    if ( rdata != data )
	      memcpy(data, rdata, pxenum->benum.data_per_row);
	  }
	return code;
}

const byte apxEndRastPattern[] = {0, 0};
int
pxEndRastPattern(px_args_t *par, px_state_t *pxs)
{	px_pattern_enum_t *pxenum = pxs->pattern_enum;
	/* We extract the key and value from the pattern_enum structure */
	/* and then free the structure, to encourage LIFO allocation. */
	px_pattern_t *pattern = pxenum->pattern;
	integer id = pxenum->pattern_id;
	px_value_t key;
	px_dict_t *pdict;

	switch ( pxenum->persistence )
	  {
	  case eTempPattern:
	    pdict = &pxs->pxgs->temp_pattern_dict;
	    break;
	  case ePagePattern:
	    pdict = &pxs->page_pattern_dict;
	    break;
	  case eSessionPattern:
	    pdict = &pxs->session_pattern_dict;
	    break;
	  default:		/* can't happen */
	    return_error(errorIllegalAttributeValue);
	  }
	key.type = pxd_array | pxd_ubyte;
	key.value.array.data = (byte *)&id;
	key.value.array.size = sizeof(id);
	gs_free_object(pxs->memory, pxenum, "pxEndRastPattern(pxenum)");
	return px_dict_put(pdict, &key, pattern);
}

/* ---------------- Scan line operators ---------------- */

const byte apxBeginScan[] = {0, 0};
int
pxBeginScan(px_args_t *par, px_state_t *pxs)
{	int code = px_set_paint(&pxs->pxgs->brush, pxs);

	if ( code < 0 )
	  return code;
	/* We may as well reset the path now instead of at the end. */
	return gs_newpath(pxs->pgs);
}

const byte apxEndScan[] = {0, 0};
int
pxEndScan(px_args_t *par, px_state_t *pxs)
{	return 0;
}

const byte apxScanLineRel[] = {
  0, pxaNumberOfScanLines, 0
};
int
pxScanLineRel(px_args_t *par, px_state_t *pxs)
{	/*
	 * In order to keep the number of intermediate states down to a
	 * reasonable number, we require enough data to be present to be
	 * able to read the control information for each line, or an entire
	 * x-pair.  Initially, source.position is zero.  As soon as we have
	 * read the X/YStart type byte, we change it to:
	 *	(X/YStart type) << 28 + (x-pair type << 24) +
	 *	  (# of full or partial scan lines left to process) + 1
	 * We use the separate variable source.count to keep track of
	 * the number of x-pairs left in the scan line.
	 */
	gs_state *pgs = pxs->pgs;
	bool big_endian = pxs->data_source_big_endian;
	const byte *data = par->source.data;
	pxeDataType_t
	  xystart_type = (par->source.position >> 28) & 0xf,
	  xpair_type = (par->source.position >> 24) & 0xf;
	int code = 0;
	int rcount;
	gs_rect rlist[20];	/* 20 is arbitrary */

	/* Check for initial state. */
	if ( par->source.position == 0 )
	  { /* Read XStart/YStart data type. */
	    if ( par->source.available < 1 )
	      return pxNeedData;
	    xystart_type = data[0];
	    if ( xystart_type != eSInt16 )
	      return_error(errorIllegalDataValue);
	    par->source.position =
	      ((ulong)xystart_type << 28) +
		(par->pv[0] ? par->pv[0]->value.i : 1) + 1;
	    par->source.data = data += 1;
	    par->source.available -= 1;
	    par->source.count = 0;
	  }
	for ( rcount = 0; ; )
	  {

	/* Check for start of scan line. */
	if ( par->source.count == 0 )
	  { int ystart;

	    if ( (par->source.position & 0xffffff) == 1 )
	      { code = 0;
	        break;
	      }
	    /* Read XStart and YStart values. */
	    /* We know that eSInt16 is the only possible data type. */
	    if ( par->source.available < 7 )
	      { code = pxNeedData;
	        break;
	      }
	    pxs->scan_point.x = sint16at(data, big_endian);
	    ystart = sint16at(data + 2, big_endian);
	    pxs->scan_point.y0 = ystart - 0.5;
	    pxs->scan_point.y1 = ystart + 0.5;
	    par->source.count = uint16at(data + 4, big_endian);
	    if ( par->source.count == 0 )
	      { code = gs_note_error(errorIllegalDataValue);
	        break;
	      }
	    xpair_type = data[6];
	    par->source.position =
	      (par->source.position & 0xf0ffffff) +
		((ulong)xpair_type << 24);
	    par->source.data = data += 7;
	    par->source.available -= 7;
	  }
	/* Read and process one x-pair. */
	{ uint x0, x1;
	  uint used;

	  switch ( xpair_type )
	    {
	    case eUByte:
	      if ( par->source.available < 2 )
		{ code = pxNeedData;
		  goto out;	/* 2-level break */
		}
	      x0 = data[0];
	      x1 = data[1];
	      used = 2;
	      break;
	    case eUInt16:
	      if ( par->source.available < 4 )
		{ code = pxNeedData;
		  goto out;	/* 2-level break */
		}
	      x0 = uint16at(data, big_endian);
	      x1 = uint16at(data + 2, big_endian);
	      used = 4;
	      break;
	    default:
	      code = gs_note_error(errorIllegalDataValue);
	      goto out;		/* 2-level break; */
	    }
	  if ( rcount == countof(rlist) )
	    { code = gs_rectfill(pgs, rlist, rcount);
	      rcount = 0;
	      pxs->have_page = true;
	      if ( code < 0 )
		break;
	    }
	  { gs_rect *pr = &rlist[rcount++];

	    pr->p.x = pxs->scan_point.x += x0;
	    pr->p.y = pxs->scan_point.y0;
	    pr->q.x = pxs->scan_point.x += x1;
	    pr->q.y = pxs->scan_point.y1;
	  }
	  par->source.data = data += used;
	  par->source.available -= used;
	}
	if ( !--(par->source.count) )
	  --(par->source.position);
	  }
out:
	if ( rcount > 0 && code >= 0 )
	  { int rcode = gs_rectfill(pgs, rlist, rcount);
	    pxs->have_page = true;
	    if ( rcode < 0 )
	      code = rcode;
	  }
	return code;
}
