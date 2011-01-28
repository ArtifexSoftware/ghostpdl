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

/* pximage.c */
/* PCL XL bitmap painting operators */

#include "std.h"
#include "string_.h"
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
#include "plsrgb.h"
#include "jpeglib_.h"           /* for jpeg filter */
#include "sdct.h"
#include "sjpeg.h"
#include "pxptable.h"

/* Define the "freeing" procedure for patterns in a dictionary. */
void
px_free_pattern(gs_memory_t *mem, void *vptr, client_name_t cname)
{	px_pattern_t *pattern = vptr;
	rc_decrement(pattern, cname);
}
/* Define the real freeing procedure for patterns. */
static void
rc_free_px_pattern(gs_memory_t *mem, void *vptr, client_name_t cname)
{	px_pattern_t *pattern = vptr;

	gs_free_string(mem, (void *)pattern->palette.data, pattern->palette.size,
		       cname);
	gs_free_object(mem, pattern->data, cname);
	gs_free_object(mem, pattern, cname);
}
/* Define the purging procedure for the Pattern cache. */
/* The proc_data points to a pxePatternPersistence_t that specifies */
/* the maximum persistence level to purge. */
static bool
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


/* pxl delta row decompression state machine states */
typedef enum {
  next_is_bytecount,
  partial_bytecount,
  next_is_cmd,
  partial_offset,
  partial_cnt
} deltarow_parse_state_t;

/* pxl delta row decompression state storage */
typedef struct deltarow_state_s {
  deltarow_parse_state_t state;
  uint row_byte_count;
  uint short_cnt; 
  uint short_offset; 
  byte *seedrow;
  uint rowwritten;
} deltarow_state_t;

/* Define the structure for enumerating a bitmap being downloaded. */
typedef struct px_bitmap_enum_s {
    gs_memory_t *mem;            /* used only for the jpeg filter */
    uint data_per_row;		 /* ditto minus possible trailing padding */
    bool initialized;
    pxeCompressMode_t compress_type;
    stream_RLD_state rld_stream_state;	/* decompressor states */
    stream_DCT_state dct_stream_state;
    jpeg_decompress_data jdd;
    deltarow_state_t deltarow_state;
} px_bitmap_enum_t;

/* Define our image enumerator. */
#ifndef px_image_enum_DEFINED
#  define px_image_enum_DEFINED
typedef struct px_image_enum_s px_image_enum_t;
#endif
struct px_image_enum_s {
  gs_image_t image;
  byte *row;			/* buffer for one row of data */
  uint raster;
  void *info;			/* state structure for driver */
  px_bitmap_enum_t benum;
};
gs_private_st_suffix_add2(st_px_image_enum, px_image_enum_t, "px_image_enum_t",
  px_image_enum_enum_ptrs, px_image_enum_reloc_ptrs, st_color_space,
  image.ColorSpace, row);

/* ---------------- Utilities ---------------- */

/* Extract the parameters for reading a bitmap image or raster pattern. */
/* Attributes: pxaColorMapping, pxaColorDepth, pxaSourceWidth, */
/* pxaSourceHeight, pxaDestinationSize. */
static int
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
	      return_error( errorImagePaletteMismatch);
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

static int
stream_error(stream_state * st, const char *str)
{
    dprintf1("pxl stream error %s\n", str );
    return 0;
}


static int
read_jpeg_bitmap_data(px_bitmap_enum_t *benum, byte **pdata, px_args_t *par)
{
    uint data_per_row = benum->data_per_row; /* jpeg doesn't pad */
    uint avail = par->source.available;
    uint end_pos = data_per_row * par->pv[1]->value.i;
    uint pos_in_row = par->source.position % data_per_row;
    const byte *data = par->source.data;
    stream_DCT_state *ss = (&benum->dct_stream_state);
    stream_cursor_read r;
    stream_cursor_write w;
    uint used;
    int code = -1;

    /* consumed all of the data */
    if ( (par->source.position >= end_pos) && (ss->phase != 4) ) {
        /* shutdown jpeg filter if necessary */
        if (benum->initialized)
            gs_jpeg_destroy((&benum->dct_stream_state));
        return 0;
    }

    if ( !benum->initialized ) {
        jpeg_decompress_data *jddp = &(benum->jdd);
        /* use the graphics library support for DCT streams */
        ss->memory = benum->mem;
        s_DCTD_template.set_defaults((stream_state *)ss);
        ss->report_error = stream_error;
        ss->data.decompress = jddp;
        /* set now for allocation */
        jddp->memory = ss->jpeg_memory = benum->mem;
        /* set this early for safe error exit */
        jddp->scanline_buffer = NULL;
        if ( gs_jpeg_create_decompress(ss) < 0 )
            return_error(errorInsufficientMemory);
        (*s_DCTD_template.init)((stream_state *)ss);
        jddp->template = s_DCTD_template;
        benum->initialized = true;
    }
    r.ptr = data - 1;
    r.limit = r.ptr + avail;
    if ( pos_in_row < data_per_row ) {
        /* Read more of the current row. */
        byte *data = *pdata;

        w.ptr = data + pos_in_row - 1;
        w.limit = data + data_per_row - 1;
        code = (*s_DCTD_template.process)((stream_state *)ss, &r, &w, false);
        used = w.ptr + 1 - data - pos_in_row;
        pos_in_row += used;
        par->source.position += used;
    }
    used = r.ptr + 1 - data;
    par->source.data = r.ptr + 1;
    par->source.available = avail - used;
    return ( code == 0 ? pxNeedData : 1);
}

static int
read_uncompressed_bitmap_data(px_bitmap_enum_t *benum, byte **pdata, px_args_t *par)
{
    int code;
    uint avail = par->source.available;
    uint data_per_row = benum->data_per_row;
    uint pad = 4; /* default padding */
    const byte *data = par->source.data;
    uint pos_in_row, data_per_row_padded, used;

    /* overrided default padding */
    if ( par->pv[3] )
        pad = par->pv[3]->value.i;

    data_per_row_padded = round_up(data_per_row, pad);
    pos_in_row = par->source.position % data_per_row_padded;

    /* consumed all of the data */
    if ( par->source.position >= data_per_row_padded * par->pv[1]->value.i )
        return 0;

    if ( avail >= data_per_row_padded && pos_in_row == 0 ) { 
        /* Use the data directly from the input buffer. */
        *pdata = (byte *)data;
        used = data_per_row_padded;
        code = 1;
    } else { 
        used = min(avail, data_per_row_padded - pos_in_row);
        if ( pos_in_row < data_per_row )
            memcpy(*pdata + pos_in_row, data,
                   min(used, data_per_row - pos_in_row));
        code = (pos_in_row + used < data_per_row_padded ?
                pxNeedData : 1);
    }
    par->source.position += used;
    par->source.data = data + used;
    par->source.available = avail - used;
    return code;
}

static int
read_rle_bitmap_data(px_bitmap_enum_t *benum, byte **pdata, px_args_t *par)
{
    stream_RLD_state *ss = (&benum->rld_stream_state);
    uint avail = par->source.available;
    const byte *data = par->source.data;
    uint pad = 4;
    stream_cursor_read r;
    stream_cursor_write w;
    uint pos_in_row, data_per_row, data_per_row_padded, used;

    /* overrided default padding */
    if ( par->pv[3] )
        pad = par->pv[3]->value.i;
    data_per_row = benum->data_per_row;
    data_per_row_padded = round_up(benum->data_per_row, pad);
    pos_in_row = par->source.position % data_per_row_padded;

    /* consumed all of the data */
    if ( par->source.position >= data_per_row_padded * par->pv[1]->value.i )
        return 0;

    if ( !benum->initialized ) {
        ss->EndOfData = false;
        s_RLD_init_inline(ss);
        benum->initialized = true;
    }
    r.ptr = data - 1;
    r.limit = r.ptr + avail;
    if ( pos_in_row < data_per_row ) { 
        /* Read more of the current row. */
        byte *data = *pdata;

        w.ptr = data + pos_in_row - 1;
        w.limit = data + data_per_row - 1;
        (*s_RLD_template.process)((stream_state *)ss, &r, &w, false);
        used = w.ptr + 1 - data - pos_in_row;
        pos_in_row += used;
        par->source.position += used;
    }
    if ( pos_in_row >= data_per_row && pos_in_row < data_per_row_padded )
        { /* We've read all the real data; skip the padding. */
            byte pad[3];	/* maximum padding per row */
		    
            w.ptr = pad - 1;
            w.limit = w.ptr + data_per_row_padded - pos_in_row;
            (*s_RLD_template.process)((stream_state *)ss, &r, &w, false);
            used = w.ptr + 1 - pad;
            pos_in_row += used;
            par->source.position += used;
        }

    used = r.ptr + 1 - data;
    par->source.data = r.ptr + 1;
    par->source.available = avail - used;
    return (pos_in_row < data_per_row_padded ? pxNeedData : 1);
}

/** PCL XL delta row decompression 
 *
 * delta row Algorithm:  
 *
 * Seed Row is initialized with zeros.
 *
 * lsb,msb row byte count
 * delta row data
 * repeat for each row
 *
 * if row byte count is zero duplicate previous row
 * if row byte count doesn't fill row duplicate remainder and end the row (undocumented)
 * 
 * delta row data: command byte, optional extra offset bytes, delta raster snippit
 * command byte 7-5 delta raster length: 4-0 offset 
 *
 * offset = bits 4-0;
 * if offset == 31 then do { add next byte } repeat while next byte was 0xFF 
 * example offset = 31 + 255 + 255 + 255 + 4
 *
 * delta length = bits 5-7  + 1; range 1 to 8 bytes. 
 *    
 * output raster is:
 * last position + offset; "copies" old data
 * copy delta length bytes from input to output 
 *           
 * Internal Algorithm:
 *  
 *  No row padding is used.
 *  State is need since available data can be short at any time.  
 *  read = *pin++; // out of data? save state, return eNeedData 
 *          
 * deltarow.state maintains state between requests for more data
 * state             : description
 *                   -> next state
 * --------------------------------------- 
 * next_is_bytecount : lsb of row bytecount 
 *                   -> partial_bytecount
 * partial_bytecount : msb of row bytecount
 *                   -> next_is_cmd
 * next_is_cmd       : 1 byte cmd contains cnt and partial offset
 *                   -> partial_offset or partial_cnt
 * partial_offset    : accumulates extra offset bytes, moves output by offset  
 *                   -> partial_offset or partial_cnt
 * partial_cnt       : copies cnt bytes one at a time from input
 *                   -> partial_cnt or next_is_cmd or (next_bytecount && end_of_row) 
 *
 * RETURN values: 
 *  0 == end of image   // end of row returns, next call returns end of image.
 *  1 == end of row
 *  eNeedData == on need more input
 */
static int
read_deltarow_bitmap_data(px_bitmap_enum_t *benum, byte **pdata, px_args_t *par)
{
  deltarow_state_t *deltarow = &benum->deltarow_state;
  uint avail = par->source.available;
  const byte *pin = par->source.data;
  byte *pout = *pdata + par->source.position % benum->data_per_row;
  const byte *pout_start = pout;
  bool end_of_row = false;

  if ( benum->initialized && deltarow->rowwritten == par->pv[1]->value.i ) {
      deltarow->rowwritten = 0;
      return 0;
  }

  /* initialize at begin of image */
  if ( !benum->initialized ) {	
    /* zero seed row */
    deltarow->seedrow = gs_alloc_bytes(benum->mem, benum->data_per_row, "read_deltarow_bitmap_data");
    memset(deltarow->seedrow, 0, benum->data_per_row);
    deltarow->row_byte_count = 0;
    deltarow->short_cnt = 0;
    deltarow->short_offset = 0;
    deltarow->state = next_is_bytecount;
    deltarow->rowwritten = 0;
    benum->initialized = true;
  }
  
  if (deltarow->row_byte_count == 0) {
    memcpy(*pdata, deltarow->seedrow, benum->data_per_row);
  }
  
  /* one byte at a time until end of input or end of row */
  while (avail && !end_of_row) {
    switch ( deltarow->state ) {

    case next_is_bytecount:{
      deltarow->short_cnt = *pin++; 
      --avail; 
      deltarow->state = partial_bytecount;
      break;
    }
  
    case partial_bytecount: {
      deltarow->row_byte_count = deltarow->short_cnt + ((*pin++) << 8 );
      --avail;
	
      if ( deltarow->row_byte_count == 0 ) {
	/* duplicate the row */
	deltarow->state = next_is_bytecount;
	end_of_row = true; 
      }
      else
	deltarow->state = next_is_cmd;
      break;
    }
      
    case next_is_cmd: {
      uint val = *pin++;

      --avail;
      deltarow->row_byte_count--;
      deltarow->short_cnt = (val >> 5) + 1;  /* 1 to 8 new bytes to copy */
      deltarow->short_offset = val & 0x1f;   /* num to retain from last row, skip */
      if (deltarow->short_offset == 0x1f) 
	deltarow->state = partial_offset;    /* accumulate more offset */
      else {
	pout += deltarow->short_offset;      /* skip keeps old data in row */
	deltarow->state = partial_cnt;       /* done with offset do count */ 
      }
      break;
    }

    case partial_offset: {
      uint offset = *pin++;
      avail--;
      deltarow->row_byte_count--;
      
      deltarow->short_offset += offset;
      
      if (offset == 0xff) 
	deltarow->state = partial_offset;  /* 0x1f + ff ff ff ff ff + 1 */ 
      else {
	pout += deltarow->short_offset;    /* skip keeps old data in row */
	deltarow->state = partial_cnt;     /* done with offset do count */ 
      }
      break;
    }

    case partial_cnt: {
        /* check for possible row overflow */
      if (pout >= *pdata + benum->data_per_row)
          return -1;
      *pout++ = *pin++;           /* copy new data into row */
      avail--;
      deltarow->row_byte_count--;
      deltarow->short_cnt--;
      
      if (deltarow->row_byte_count == 0) {	
	end_of_row = true;
	deltarow->state = next_is_bytecount;
      }
      else if (deltarow->short_cnt == 0) 
	deltarow->state = next_is_cmd;
      /* else more bytes to copy */
      break;
    }

    } /* end switch */
  } /* end of while */
    

  par->source.available -= pin - par->source.data;  /* subract comressed data used */
  par->source.data = pin;                           /* new compressed data position */

  if (end_of_row) {
      /* uncompressed raster position */
      par->source.position = 
          (par->source.position / benum->data_per_row + 1) * benum->data_per_row;
      deltarow->rowwritten++;
      memcpy(deltarow->seedrow, *pdata, benum->data_per_row);
      return 1;
  }
  par->source.position += pout - pout_start;       /* amount of raster written */
  return pxNeedData;                               /* not end of row so request more data */
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
static int
read_bitmap(px_bitmap_enum_t *benum, byte **pdata, px_args_t *par)
{	
    benum->compress_type = par->pv[2]->value.i;
    switch( benum->compress_type ) {
    case eRLECompression:
        return read_rle_bitmap_data(benum, pdata, par);
    case eJPEGCompression:
        return read_jpeg_bitmap_data(benum, pdata, par);
    case eDeltaRowCompression:
        return read_deltarow_bitmap_data(benum, pdata, par);
    case eNoCompression:
        return read_uncompressed_bitmap_data(benum, pdata, par);
    default:
        break;
    }
    return -1;
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
             ) {
            if ( pxs->useciecolor )
                code = pl_setSRGBcolor(pgs, 0.0, 0.0, 0.0);
            else
                code = gs_setgray(pgs, 0.0);
        }
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
	{ 
            pxenum->raster = round_up(benum.data_per_row, align_bitmap_mod);
            pxenum->row = gs_alloc_byte_array(pxs->memory, 1, pxenum->raster,
                                              "pxReadImage(row)");
	  if ( pxenum->row == 0 )
	    code = gs_note_error(errorInsufficientMemory);
	  else 
	    code = px_image_color_space(&pxenum->image, &params, (const gs_string *)&pxgs->palette, pgs);
	}
	if ( code < 0 )
	  { gs_free_object(pxs->memory, pxenum->row, "pxReadImage(row)");
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
        pxenum->image.Interpolate = pxs->interpolate;

	code = pl_begin_image(pgs, &pxenum->image, &pxenum->info);
	if ( code < 0 )
	  { gs_free_object(pxs->memory, pxenum->row, "pxReadImage(row)");
	    gs_free_object(pxs->memory, pxenum, "pxBeginImage(pxenum)");
	    return code;
	  }
	pxenum->benum = benum;
	pxs->image_enum = pxenum;
	return 0;
}

const byte apxReadImage[] = {
    pxaStartLine, pxaBlockHeight, pxaCompressMode, 0, pxaPadBytesMultiple, pxaBlockByteLength, 0
};

int
pxReadImage(px_args_t *par, px_state_t *pxs)
{	
    px_image_enum_t *pxenum = pxs->image_enum;
    gx_device *dev = gs_currentdevice(pxs->pgs);
    
    if ( par->pv[1]->value.i == 0 )
        return 0;		/* no data */
    /* Make a quick check for the first call, when no data is available. */
    if ( par->source.available == 0 )
        return pxNeedData;
    for ( ; ; ) {
        byte *data = pxenum->row;
        int code = read_bitmap(&pxenum->benum, &data, par);
        if ( code != 1 )
            return code;
        code = (*dev_proc(dev, image_data))
            (dev, pxenum->info, (const byte **)&data,
             0, pxenum->benum.data_per_row, 1);
        if ( code < 0 )
            return code;
        pxs->have_page = true;
    }
}

const byte apxEndImage[] = {0, 0};
int
pxEndImage(px_args_t *par, px_state_t *pxs)
{	
    gx_device *dev = gs_currentdevice(pxs->pgs);
    px_image_enum_t *pxenum = pxs->image_enum;
    px_bitmap_enum_t *pbenum = &pxenum->benum;
    (*dev_proc(dev, end_image))(dev, pxenum->info, true);
    gs_free_object(pxs->memory, pxenum->row, "pxEndImage(row)");
    if ( pbenum->compress_type == eDeltaRowCompression )
        gs_free_object(pbenum->mem, pbenum->deltarow_state.seedrow, "pxEndImage(seedrow)");
    if ( pxenum->image.ColorSpace )
        rc_decrement(pxenum->image.ColorSpace, "pxEndImage(image.ColorSpace)");
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
  int32_t pattern_id;
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
        static const gs_memory_struct_type_t st_px_pattern = 
            {sizeof(px_pattern_t), "", 0, 0, 0, 0, 0};

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
	pattern->id = gs_next_ids(mem, 1);
	pxenum->pattern = pattern;
	pxs->pattern_enum = pxenum;
	return 0;
}

const byte apxReadRastPattern[] = {
    pxaStartLine, pxaBlockHeight, pxaCompressMode, 0, pxaPadBytesMultiple, pxaBlockByteLength, 0
};

int
pxReadRastPattern(px_args_t *par, px_state_t *pxs)
{	px_pattern_enum_t *pxenum = pxs->pattern_enum;
	int code;
	uint input_per_row = round_up(pxenum->benum.data_per_row, 4);
	if ( par->pv[3] )
	    input_per_row = round_up(pxenum->benum.data_per_row, par->pv[3]->value.i);
	/* Make a quick check for the first call, when no data is available. */
	if ( par->source.available == 0 && par->pv[1]->value.i != 0 )
	  return pxNeedData;
        /* emulate hp bug */
        if ( par->pv[2]->value.i == eDeltaRowCompression )
            input_per_row = pxenum->benum.data_per_row;

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
	int32_t id = pxenum->pattern_id;
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
