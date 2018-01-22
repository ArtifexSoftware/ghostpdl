/* Copyright (C) 2013 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* pxvendor.c */
/* PCL XL VendorUnique operators */

/* The first implementation of CLJ 3500/3550/3600 emulation
   was provided by Hin-Tak Leung, summer 2013. */

#include "string_.h"		/* memset/memcpy */
#include "gdebug.h"
#include "gsmemory.h"
#include "strimpl.h"
#include "pxoper.h"
#include "pxstate.h"
#include "gspath.h"		/* gs_currentpoint */
#include "pldraw.h"		/* pl_begin_image/pl_image_data/pl_end_image */
#include "jpeglib_.h"
#ifndef DSTATE_READY
/* defined in libjpeg's internal jpegint.h */
#define DSTATE_READY 202
#endif
#include "sdct.h"
#include "sjpeg.h"		/* gs_jpeg_* */
#include "pxvendor.h"

/* Mode 10 pixel source */
typedef enum
{
    eNewPixel = 0x0,
    eWPixel = 0x20,
    eNEPixel = 0x40,
    eCachedMask = 0x60,
    eRLE = 0x80,
} mode10_cmd_state_t;

#define src_is_(x, y) (((x) & (eCachedMask)) == (y))
#define srcNEW(x) src_is_(x, eNewPixel)
#define src_fromW(x)   src_is_(x, eWPixel)
#define src_fromNE(x)  src_is_(x, eNEPixel)
#define src_Cached(x)  src_is_(x, eCachedMask)

typedef enum
{
    next_is_cmd,
    partial_offset,
    next_is_pixel,
    partial_count,
    process_rle,
} mode10_parse_state_t;

typedef enum
{
    vu_blank,
    vu_tagged,
    vu_body,
} vu_state_t;

typedef struct mode10_state_s
{
    mode10_parse_state_t state;
    byte cmd;
    uint32_t cached_pixel;
    uint32_t offset;
    uint32_t count;
    uint32_t cursor;		/*  do without this? */
} mode10_state_t;

typedef struct vu_tag_s
{
    uint16_t tag_id;
    uint32_t bytes_expected;
    uint32_t bytes_so_far;
} vu_tag_t;

/* Define our state enumerator. */
#ifndef px_vendor_state_DEFINED
#  define px_vendor_state_DEFINED
typedef struct px_vendor_state_s px_vendor_state_t;
#endif
struct px_vendor_state_s
{
    vu_tag_t tag;
    gs_memory_t *mem;
    pxeColorSpace_t color_space;	/* copy of pxs->pxgs->color_space */
    uint32_t SourceWidth;	/* BeginImage pv[8] */
    uint32_t StartLine;		/* ReadImage pv[10] */
    uint32_t BlockHeight;	/* Readimage pv[11] */
    uint32_t data_per_row;
    gs_image_t image;
    void *info;			/* state structure for driver */
    byte *row;			/* buffer for one row of data */
    uint rowwritten;
    int qcount;
    UINT16 *qvalues;
    stream_DCT_state dct_stream_state;
    mode10_state_t mode10_state;
    jpeg_decompress_data jdd;
    vu_state_t state;
};

gs_private_st_simple(st_px_vendor_state, px_vendor_state_t,
		     "px_vendor_state_t");
/* -------------------- private routines starts --------------- */

static int
vu_stream_error(stream_state * st, const char *str)
{
    dmprintf1(st->memory, "pxl vendor stream error %s\n", str);
    return 0;
}

static void
add_huff_table(j_decompress_ptr cinfo,
	       JHUFF_TBL ** htblptr, const UINT8 * bits, const UINT8 * val)
{
    int nsymbols, len;

    if (*htblptr == NULL)
	*htblptr = jpeg_alloc_huff_table((j_common_ptr) cinfo);

    memcpy((*htblptr)->bits, bits, sizeof((*htblptr)->bits));

    nsymbols = 0;
    for (len = 1; len <= 16; len++)
	nsymbols += bits[len];
    memcpy((*htblptr)->huffval, val, nsymbols * sizeof(UINT8));

    (*htblptr)->sent_table = FALSE;
}

/* identical to jcparam.c */
static void
std_huff_tables(j_decompress_ptr cinfo)
{
    static const UINT8 bits_dc_luminance[17] =
	{ /* 0-base */ 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
    static const UINT8 val_dc_luminance[] =
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

    static const UINT8 bits_dc_chrominance[17] =
	{ /* 0-base */ 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
    static const UINT8 val_dc_chrominance[] =
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

    static const UINT8 bits_ac_luminance[17] =
	{ /* 0-base */ 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
    static const UINT8 val_ac_luminance[] =
	{ 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
	0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
	0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
	0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
	0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
	0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
	0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
	0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
	0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
	0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
	0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
	0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
	0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
	0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
	0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
	0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
	0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa
    };

    static const UINT8 bits_ac_chrominance[17] =
	{ /* 0-base */ 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
    static const UINT8 val_ac_chrominance[] =
	{ 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
	0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
	0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
	0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
	0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
	0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
	0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
	0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
	0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
	0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
	0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
	0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
	0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
	0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
	0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
	0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa
    };
    add_huff_table(cinfo, &cinfo->dc_huff_tbl_ptrs[0],
		   bits_dc_luminance, val_dc_luminance);
    add_huff_table(cinfo, &cinfo->ac_huff_tbl_ptrs[0],
		   bits_ac_luminance, val_ac_luminance);
    add_huff_table(cinfo, &cinfo->dc_huff_tbl_ptrs[1],
		   bits_dc_chrominance, val_dc_chrominance);
    add_huff_table(cinfo, &cinfo->ac_huff_tbl_ptrs[1],
		   bits_ac_chrominance, val_ac_chrominance);
}

/* TODO: the whole purpose of fastforward_jpeg_stream_state(),
   (as well as add_huff_table()/add_huff_tables() above), is to wind
   the state of the jpeg stream forward, since the incoming
   data is headerless. On hide-sight, it might be cleaner
   and more future-proof against changes in libjpeg
   to construct a made-up header, read it then switch data stream. */
static void
fastforward_jpeg_stream_state(jpeg_decompress_data * jddp,
			      stream_DCT_state * ss, px_state_t * pxs)
{
    px_vendor_state_t *v_state = pxs->vendor_state;
    j_decompress_ptr cinfo = &jddp->dinfo;
    int i;
    int j, qcount = 0;
    jpeg_component_info *compptr;
    int factor = ((v_state->color_space == eGraySub) ? 8 : 1);
    extern const int jpeg_natural_order[];

    if (cinfo->comp_info == NULL)
	cinfo->comp_info = (jpeg_component_info *)
	    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo,
					JPOOL_PERMANENT,
					MAX_COMPONENTS *
					sizeof(jpeg_component_info));
    /* default_decompress_parms() starts */
    if (v_state->color_space == eGraySub) {
	cinfo->jpeg_color_space = JCS_GRAYSCALE;
	cinfo->out_color_space = JCS_GRAYSCALE;
    } else {
	cinfo->jpeg_color_space = JCS_YCbCr;
	cinfo->out_color_space = JCS_YCbCr;
    }
    cinfo->scale_num = 8;
    cinfo->scale_denom = 8;
    cinfo->output_gamma = 1.0;
    cinfo->buffered_image = FALSE;
    cinfo->raw_data_out = FALSE;
    cinfo->dct_method = JDCT_DEFAULT;
    cinfo->do_fancy_upsampling = TRUE;
    cinfo->do_block_smoothing = TRUE;
    cinfo->quantize_colors = FALSE;
    /* */
    cinfo->dither_mode = JDITHER_FS;
    /* */
    cinfo->two_pass_quantize = 1;
    /* */
    cinfo->desired_number_of_colors = 256;
    /* */
    cinfo->enable_1pass_quant = 0;
    cinfo->enable_external_quant = 0;
    cinfo->enable_2pass_quant = FALSE;
    /* default_decompress_parms() ends */
    std_huff_tables(cinfo);
    /* get_soi begins */
    for (i = 0; i < NUM_ARITH_TBLS; i++) {
	cinfo->arith_dc_L[i] = 0;
	cinfo->arith_dc_U[i] = 1;
	cinfo->arith_ac_K[i] = 5;
    }
    cinfo->restart_interval = 0;
    cinfo->CCIR601_sampling = FALSE;
    /* */
    cinfo->JFIF_major_version = 1;	/* Default JFIF version = 1.01 */
    cinfo->JFIF_minor_version = 1;
    cinfo->density_unit = 0;	/* Pixel size is unknown by default */
    cinfo->X_density = 1;	/* Pixel aspect ratio is square by default */
    cinfo->Y_density = 1;
    /* get_soi ends */
    /* get_sof */
    cinfo->is_baseline = 1;
    cinfo->progressive_mode = FALSE;
    cinfo->arith_code = FALSE;
    cinfo->data_precision = BITS_IN_JSAMPLE;
    if (v_state->color_space == eGraySub)
	cinfo->num_components = 1;
    else
	cinfo->num_components = 3;

    cinfo->image_width = v_state->SourceWidth;
    cinfo->image_height = v_state->BlockHeight;

    for (i = 0; i < cinfo->num_components; i++) {
	if (cinfo->quant_tbl_ptrs[i] == NULL)
	    cinfo->quant_tbl_ptrs[i] =
		jpeg_alloc_quant_table((j_common_ptr) cinfo);
	for (j = 0; j < 64; j++)
	    cinfo->quant_tbl_ptrs[i]->quantval[j] =
		v_state->qvalues[qcount++];
    }

#define SET_COMP(index,id,hsamp,vsamp,quant,dctbl,actbl)  \
        (compptr = &cinfo->comp_info[index], \
         compptr->component_id = (id), \
         compptr->h_samp_factor = (hsamp), \
         compptr->v_samp_factor = (vsamp), \
         compptr->quant_tbl_no = (quant), \
         compptr->dc_tbl_no = (dctbl), \
         compptr->ac_tbl_no = (actbl), \
         compptr->DCT_h_scaled_size = 8, \
         compptr->DCT_v_scaled_size = 8, \
         compptr->width_in_blocks = cinfo->image_width/factor, \
         compptr->height_in_blocks = cinfo->image_height/factor, \
         compptr->quant_table = cinfo->quant_tbl_ptrs[index], \
         compptr->component_index = index, \
         compptr->component_needed = TRUE, \
         compptr->downsampled_width = cinfo->image_width, \
         compptr->downsampled_height = cinfo->image_height)

    SET_COMP(0, 1, 1, 1, 0, 0, 0);	/* not default! */
    if (v_state->color_space != eGraySub) {
	SET_COMP(1, 2, 1, 1, 1, 1, 1);
	SET_COMP(2, 3, 1, 1, 1, 1, 1);
    }
    /* end_sof */
    cinfo->rec_outbuf_height = 1;

    if (v_state->color_space == eGraySub) {
	cinfo->out_color_components = 1;
	cinfo->output_components = 1;
    } else {
	cinfo->out_color_components = 3;
	cinfo->output_components = 3;
    }
    cinfo->global_state = DSTATE_READY;
    /* no SOS markers, so we just do this by hand - mostly from get_sos() */
    cinfo->cur_comp_info[0] = &cinfo->comp_info[0];
    cinfo->comp_info[0].dc_tbl_no = 0;
    cinfo->comp_info[0].ac_tbl_no = 0;
    if (v_state->color_space != eGraySub) {
	cinfo->cur_comp_info[1] = &cinfo->comp_info[1];
	cinfo->comp_info[1].dc_tbl_no = 1;
	cinfo->comp_info[1].ac_tbl_no = 1;
	cinfo->cur_comp_info[2] = &cinfo->comp_info[2];
	cinfo->comp_info[2].dc_tbl_no = 1;
	cinfo->comp_info[2].ac_tbl_no = 1;
    }
    cinfo->Ss = 0;
    cinfo->Se = 63;
    cinfo->Ah = 0;
    cinfo->Al = 0;
    /* initial_setup() */
    cinfo->input_scan_number = 1;
    cinfo->block_size = DCTSIZE;

    cinfo->natural_order = jpeg_natural_order;
    cinfo->lim_Se = DCTSIZE2 - 1;
    cinfo->min_DCT_h_scaled_size = 8;
    cinfo->min_DCT_v_scaled_size = 8;
    cinfo->max_h_samp_factor = 1;
    cinfo->max_v_samp_factor = 1;
    if (v_state->color_space == eGraySub)
	cinfo->comps_in_scan = 1;
    else
	cinfo->comps_in_scan = 3;

    /* */
    jpeg_core_output_dimensions(cinfo);
    cinfo->total_iMCU_rows = 16;

    ss->phase = 2;		/* fast forward to phase 2, on to start_decompress */
}

/* TODO: This might be better done and more accurate via cinfo->cconvert */
#define jpeg_custom_color_fixup(rowin, color_space, data_per_row)       \
  {                                                                     \
    if (color_space == eGraySub) {                                      \
      int i = data_per_row;                                             \
      byte *row = rowin; /* don't touch the original pointer */         \
      while (i > 0) {                                                   \
        byte temp = *row;                                               \
        *row++ = temp ^ 0xff;                                           \
        i--;                                                            \
      }                                                                 \
    } else {                                                            \
      int i = data_per_row / 3;                                         \
      byte *row = rowin; /* don't touch the original pointer */         \
      while (i > 0) {                                                   \
        int red, green, blue;                                           \
        red = 2 * (*row); /* +1 ? */                                    \
        *row++ = min(max(red, 0), 255);                                 \
        green = red + 256 - 2 * (*row);                                 \
        *row++ = min(max(green, 0), 255);                               \
        blue = red + 256 - 2 * (*row);                                  \
        *row++ = min(max(blue, 0), 255);                                \
        i--;                                                            \
      }                                                                 \
    }                                                                   \
  }

static int
vu_begin_image(px_state_t * pxs)
{
    px_vendor_state_t *v_state = pxs->vendor_state;
    gs_image_t image = v_state->image;
    px_bitmap_params_t params;
    gs_point origin;
    int code;

    if (v_state->color_space == eGraySub)
	params.color_space = eGray;
    else
	params.color_space = eSRGB;
    params.width = (uint)v_state->SourceWidth;
    params.dest_width = (real)v_state->SourceWidth;
    params.height = (uint)v_state->BlockHeight;
    params.dest_height = (real)v_state->BlockHeight;
    params.depth = 8;
    params.indexed = false;
    code = px_image_color_space(&image, &params,
				(const gs_string *)&pxs->pxgs->palette,
				pxs->pgs);
    if (code < 0) {
	return code;
    }

    /* Set up the image parameters. */
    if (gs_currentpoint(pxs->pgs, &origin) < 0)
	return_error(errorCurrentCursorUndefined);
    image.Width = v_state->SourceWidth;
    image.Height = v_state->BlockHeight;
    {
	gs_matrix imat, dmat;

	gs_make_scaling(image.Width, image.Height, &imat);
	gs_make_translation(origin.x, origin.y + v_state->StartLine, &dmat);
	gs_matrix_scale(&dmat, image.Width, image.Height, &dmat);
	/* The ImageMatrix is dmat' * imat. */
	gs_matrix_invert(&dmat, &dmat);
	gs_matrix_multiply(&dmat, &imat, &image.ImageMatrix);
    }
    image.CombineWithColor = true;
    image.Interpolate = pxs->interpolate;
    code = pl_begin_image(pxs->pgs, &image, &v_state->info);
    if (code < 0)
	return code;
    return 0;
}

/* TODO: consider using source.count (and possibly also phase) */
static int
read_headless_jpeg_bitmap_data(byte ** pdata,
			       px_args_t * par, px_state_t * pxs)
{
    px_vendor_state_t *v_state = pxs->vendor_state;
    uint data_per_row = v_state->data_per_row;	/* jpeg doesn't pad */
    uint avail = par->source.available;

    uint pos_in_row = par->source.position % data_per_row;
    const byte *data = par->source.data;
    stream_DCT_state *ss = (&v_state->dct_stream_state);
    stream_cursor_read r;
    stream_cursor_write w;
    uint used;
    int code = -1;

    if_debug3('w', "jpeg begin pos=%d row=%d/%d\n",
	      pos_in_row, v_state->rowwritten, v_state->BlockHeight);

    /* consumed all of the data */
    if ((v_state->state == vu_body)
	&& v_state->rowwritten == v_state->BlockHeight) {
	if (par->source.available == 2) {
	    /* the data includes EOI; the filter may or may not consume it */
	    par->source.data += 2;
	    par->source.available -= 2;
	}
	gs_jpeg_destroy((&v_state->dct_stream_state));
	v_state->state = vu_blank;
	v_state->rowwritten = 0;
	code = pl_end_image(pxs->pgs, v_state->info, true);

	if (code < 0)
	    return code;
	return 0;
    }

    if (v_state->state == vu_tagged) {
	jpeg_decompress_data *jddp = &(v_state->jdd);

	v_state->rowwritten = 0;
	code = vu_begin_image(pxs);
	if (code < 0)
	    return code;

	/* use the graphics library support for DCT streams */
	ss->memory = pxs->memory;
	ss->templat = &s_DCTD_template;
	s_DCTD_template.set_defaults((stream_state *) ss);
	ss->report_error = vu_stream_error;
	ss->data.decompress = jddp;
	/* set now for allocation */
	jddp->memory = ss->jpeg_memory = pxs->memory;
	/* set this early for safe error exit */
	jddp->scanline_buffer = NULL;
    jddp->PassThrough = 0;
    jddp->PassThroughfn = 0;
    jddp->device = (void *)0;
	if (gs_jpeg_create_decompress(ss) < 0)
	    return_error(errorInsufficientMemory);
	(*s_DCTD_template.init) ((stream_state *) ss);
	jddp->templat = s_DCTD_template;
	fastforward_jpeg_stream_state(jddp, ss, pxs);
	v_state->state = vu_body;
    }

    r.ptr = data - 1;
    r.limit = r.ptr + avail;
    if (pos_in_row < data_per_row) {
	/* Read more of the current row. */
	byte *data = *pdata;

	w.ptr = data + pos_in_row - 1;
	w.limit = data + data_per_row - 1;
	code =
	    (*s_DCTD_template.process) ((stream_state *) ss, &r, &w, false);
	/* code = num scanlines processed (0=need more data, -ve=error) */
	if_debug1('w', "s_DCTD_template.process returns %d\n", code);
	used = w.ptr + 1 - data - pos_in_row;
	if ((code == EOFC) && (used > 0))
	    code = 1;
	if_debug2('w', "data generated: %d/%d\n", used, data_per_row);
	/* pos_in_row += used; */
	par->source.position += used;
    }
    used = r.ptr + 1 - data;
    par->source.data = r.ptr + 1;
    par->source.available = avail - used;
    if_debug2('w', "scanlines %d used %d\n", code, used);
    if (code == 0)
	return pxNeedData;
    if (code == EOFC)		/* used = 0 is possible, and earlier than end */
	return 0;
    if (code > 0) {
	v_state->rowwritten++;
	if_debug1('w', "row written:%d\n", v_state->rowwritten);
	jpeg_custom_color_fixup(v_state->row,
				v_state->color_space, v_state->data_per_row);
	return 1;
    }
    return code;
}

/* TODO: speed-wise we are testing v_state->color_space == eGraySub
   and mode9031 almost almost every pixel; might be better off
   having two or even three versions of each routines.
*/

#define update_pout(pout)                                               \
  pout = *pdata + mode10_state->cursor * (v_state->color_space == eGraySub ? 1:3)

#define update_advance_pixel(pout, pin, mode9031)       \
  if (v_state->color_space == eGraySub) {               \
    *pout++ = pin[0] ^ 0xff;                            \
  } else if (mode9031) {                                \
    *pout++ = pin[0];                                   \
    *pout++ = pin[0];                                   \
    *pout++ = pin[0];                                   \
  } else {                                              \
    *pout++ = pin[0];                                   \
    *pout++ = pin[1];                                   \
    *pout++ = pin[2];                                   \
  }

#define copy_pixel(pout, pixel)                 \
  if (v_state->color_space == eGraySub) {       \
    *pout++ = pixel & 0xff;                     \
  } else {                                      \
    *pout++ = (pixel >> 16 & 0xff);             \
    *pout++ = (pixel >> 8 & 0xff);              \
    *pout++ = (pixel & 0xff);                   \
  }

static uint32_t
get_pixel(byte * pout, uint32_t * cached_pixel, byte cmd,
	  pxeColorSpace_t color_space)
{
    uint32_t pixel = *cached_pixel;

    if (color_space == eGraySub) {
	if (src_fromW(cmd)
	    || srcNEW(cmd))	/* New pixel would have over-written first pixel */
	    pixel = *(pout - 1);
	if (src_fromNE(cmd))
	    pixel = *(pout + 1);
    } else {
	if (src_fromW(cmd)
	    || srcNEW(cmd))	/* New pixel would have over-written first pixel */
	    pixel = (*(pout - 3) << 16) | (*(pout - 2) << 8) | (*(pout - 1));
	if (src_fromNE(cmd))
	    pixel = (*(pout + 3) << 16) | (*(pout + 4) << 8) | (*(pout + 5));
    }
    *cached_pixel = pixel;
    return pixel;
}

/* Almost the same as Mode 10, except without pixel delta. */
/* Note: gray input also have 3 channels. */
/* TODO: consider using source.position and source.count (and possibly also phase) */
static int
read_mode10_bitmap_data(byte ** pdata, px_args_t * par, px_state_t * pxs,
			bool mode9031)
{
    px_vendor_state_t *v_state = pxs->vendor_state;
    mode10_state_t *mode10_state = &v_state->mode10_state;
    uint avail = min(par->source.available,
		     (v_state->tag.bytes_expected -
		      v_state->tag.bytes_so_far));
    const byte *pin = par->source.data;
    byte *pout;
    bool end_of_row = false;
    uint32_t pixel;
    int i;

    update_pout(pout);

    if ((v_state->state == vu_body)
	&& v_state->rowwritten == v_state->BlockHeight) {
	int code = pl_end_image(pxs->pgs, v_state->info, true);

	if (code < 0)
	    return code;

	v_state->state = vu_blank;
	v_state->rowwritten = 0;
	return 0;
    }

    /* initialize at begin of image */
    if (v_state->state == vu_tagged) {
	int code = vu_begin_image(pxs);

	if (code < 0)
	    return code;

	mode10_state->state = next_is_cmd;
	mode10_state->cursor = 0;
	v_state->rowwritten = 0;
	v_state->state = vu_body;
    }

    /* one byte at a time until end of input or end of row */
    while ((avail || mode10_state->state == process_rle)	/* process_rle does not consume */
	   &&(!end_of_row)) {
	switch (mode10_state->state) {
	    case next_is_cmd:{
		    mode10_state->cmd = *pin++;
		    --avail;
		    if_debug4('w',
			      "command:%02X row written:%d cursor:%d cached=%08X\n",
			      mode10_state->cmd,
			      v_state->rowwritten,
			      mode10_state->cursor,
			      mode10_state->cached_pixel);
		    mode10_state->offset = (mode10_state->cmd >> 3) & 0x03;
		    mode10_state->count = mode10_state->cmd & 0x07;
		    mode10_state->cursor += mode10_state->offset;	/* may be partial */
		    if ((mode10_state->offset < 3)
			&& (mode10_state->count < 7)
			&& (!srcNEW(mode10_state->cmd))
			) {
			/* completed command */
			if (mode10_state->cmd & eRLE) {
			    mode10_state->state = process_rle;
			} else {
			    /* literal, non-new */
			    update_pout(pout);
			    pixel =
				get_pixel(pout, &mode10_state->cached_pixel,
					  mode10_state->cmd,
					  v_state->color_space);

			    copy_pixel(pout, pixel);
			    mode10_state->cursor += 1;
			    if (mode10_state->count > 0) {
				mode10_state->count--;
				mode10_state->state = next_is_pixel;
			    } else
				mode10_state->state = next_is_cmd;
			}
		    } else if (mode10_state->offset == 3)
			mode10_state->state = partial_offset;
		    else if (srcNEW(mode10_state->cmd)) {
			if ((mode10_state->cmd & eRLE)
			    && (mode10_state->cursor >= v_state->SourceWidth)) {
			    /* special case */
			    if_debug0('w', "special\n");
			    mode10_state->cursor = 0;
			    end_of_row = 1;
			    mode10_state->state = next_is_cmd;
			} else
			    mode10_state->state = next_is_pixel;
		    } else if (mode10_state->count == 7)
			mode10_state->state = partial_count;
		    else
			mode10_state->state = next_is_cmd;	/* does not happen */
		    break;
		}

	    case partial_offset:{
		    uint offset = *pin++;

		    avail--;
		    mode10_state->offset += offset;
		    mode10_state->cursor += offset;
		    if (offset == 0xff)
			mode10_state->state = partial_offset;
		    else {
			/* completed offset */
			if_debug5('w',
				  "%02X row=%d offset=%d cursor=%d/%d\n",
				  mode10_state->cmd,
				  v_state->rowwritten,
				  mode10_state->offset, mode10_state->cursor,
				  v_state->SourceWidth);
			if (srcNEW(mode10_state->cmd)) {
			    if ((mode10_state->cmd & eRLE)
				&& (mode10_state->cursor >=
				    v_state->SourceWidth)) {
				/* special case */
				if_debug0('w', "special\n");
				mode10_state->cursor = 0;
				end_of_row = 1;
				mode10_state->state = next_is_cmd;
			    } else
				mode10_state->state = next_is_pixel;
			} else if (mode10_state->count == 7) {
			    mode10_state->state = partial_count;
			} else {
			    /* not new pixels, counts under 7, so we need to process there */
			    if (mode10_state->cmd & eRLE) {
				mode10_state->state = process_rle;
			    } else {
				/* literal non-new */
				update_pout(pout);
				pixel = get_pixel(pout,
						  &mode10_state->cached_pixel,
						  mode10_state->cmd,
						  v_state->color_space);
				copy_pixel(pout, pixel);
				mode10_state->cursor += 1;
				if (mode10_state->count > 0) {
				    mode10_state->count--;
				    mode10_state->state = next_is_pixel;
				} else
				    mode10_state->state = next_is_cmd;
			    }
			}
		    }
		    break;
		}

	    case partial_count:{
		    uint count = *pin++;

		    avail--;
		    mode10_state->count += count;
		    if (count == 0xff)
			mode10_state->state = partial_count;
		    else {
			/* finished count - partial count only happens on RLE */
			if_debug1('w', "count=%d\n", mode10_state->count);
			mode10_state->state = process_rle;
		    }
		    break;
		}

	    case next_is_pixel:{
		    if (!mode9031 && (avail < 3)) {
			/* get to outer loop to get more data */
			avail = 0;
			break;
		    }
		    if_debug3('w', "pixel:%02X%02X%02X\n", pin[0], pin[1],
			      pin[2]);
		    if (v_state->color_space == eGraySub) {
			/* bug in recent hpijs */
			mode10_state->cached_pixel =
			    (pin[0] << 16 | pin[0] << 8 | pin[0]) ^
			    0x00ffffff;
		    } else if (mode9031) {
			mode10_state->cached_pixel =
			    (pin[0] << 16 | pin[0] << 8 | pin[0]);
		    } else
			mode10_state->cached_pixel =
			    (pin[0] << 16 | pin[1] << 8 | pin[2]);
		    update_pout(pout);
		    update_advance_pixel(pout, pin, mode9031);
		    if (mode9031) {
			pin += 1;
			avail -= 1;
		    } else {
			pin += 3;
			avail -= 3;
		    }
		    mode10_state->cursor++;
		    if ((mode10_state->cmd & eRLE)
			&& (mode10_state->count == 7))
			mode10_state->state = partial_count;
		    else if (mode10_state->cmd & eRLE) {
			mode10_state->state = process_rle;
		    } else if (mode10_state->count > 0) {
			/* literal */
			mode10_state->count--;
			mode10_state->state = next_is_pixel;
		    } else
			mode10_state->state = next_is_cmd;
		    break;
		}

	    case process_rle:{
		    update_pout(pout);
		    pixel =
			get_pixel(pout, &mode10_state->cached_pixel,
				  mode10_state->cmd, v_state->color_space);

		    mode10_state->cursor += mode10_state->count + 2;
		    i = mode10_state->count + 2;

		    if (srcNEW(mode10_state->cmd)) {
			i--;	/* already moved cursor in the case of new pixel */
			mode10_state->cursor--;
		    }
		    while (i > 0) {
			copy_pixel(pout, pixel);
			i--;
		    }
		    mode10_state->state = next_is_cmd;
		    break;
		}

	}			/* end switch */

	/* conditional on state may not be necessary */
	if ((mode10_state->state == next_is_cmd) &&
	    (mode10_state->cursor >= v_state->SourceWidth)) {
	    mode10_state->cursor = 0;
	    end_of_row = 1;
	}
    }				/* end of while */

    par->source.available -= pin - par->source.data;	/* subtract compressed data used */
    par->source.position += pin - par->source.data;
    par->source.data = pin;	/* new compressed data position */

    if (end_of_row) {
	v_state->rowwritten++;
	return 1;
    }
    return pxNeedData;		/* not end of row so request more data */
}

static int
tag_dispatch_90X1(px_args_t * par, px_state_t * pxs)
{
    px_vendor_state_t *v_state = pxs->vendor_state;
    int code = 0;

    for (;;) {
	uint32_t old_avail = par->source.available;

	switch (v_state->tag.tag_id) {
	    case 0x9031:
		code =
		    read_mode10_bitmap_data(&(v_state->row), par, pxs, true);
		break;
	    case 0x9011:
		code =
		    read_mode10_bitmap_data(&(v_state->row), par, pxs, false);
		break;
	    case 0x9021:
		code =
		    read_headless_jpeg_bitmap_data(&(v_state->row), par, pxs);
		break;
	}
	v_state->tag.bytes_so_far += old_avail - par->source.available;
	if_debug6('I',
		  "new src.p %lu old.avail %u new.avail %u bytes=%d/%d code %d\n",
		  par->source.position, old_avail, par->source.available,
		  v_state->tag.bytes_so_far, v_state->tag.bytes_expected,
		  code);
	if (code < 0)
	    return_error(errorIllegalDataValue);
	if ((code == pxNeedData)
	    && (v_state->tag.bytes_so_far >= v_state->tag.bytes_expected))
	    return_error(errorIllegalDataValue);	/* 3550's behavior */
	if (code != 1)		/* = 0 */
	    return code;

	code =
	    pl_image_data(pxs->pgs, v_state->info,
			  (const byte **)&(v_state->row), 0,
			  v_state->data_per_row, 1);
	if (code < 0)
	    return code;
	pxs->have_page = true;
    }
    /* unreachable */
    return code;
}
static int
tag_dispatch_8000(px_args_t * par, px_state_t * pxs)
{
    px_vendor_state_t *v_state = pxs->vendor_state;
    int i = 192 - v_state->qcount;

    while ((i > 0) && (par->source.available >= 4)) {
	v_state->qvalues[v_state->qcount++] =
	    uint32at(par->source.data, pxs->data_source_big_endian);
	par->source.data += 4;
	par->source.available -= 4;
	par->source.position += 4;
	i--;
    }
    if (i == 0) {
	v_state->state = vu_blank;
	return 0;
    } else
	return pxNeedData;
}
static int
tag_dispatch_generic(px_args_t * par, px_state_t * pxs)
{
    px_vendor_state_t *v_state = pxs->vendor_state;
    int code = 0;
    uint32_t bytes_to_end =
	v_state->tag.bytes_expected - v_state->tag.bytes_so_far;
    uint32_t copy = min(bytes_to_end,
			par->source.available);

    par->source.data += copy;
    par->source.available -= copy;
    par->source.position += copy;
    v_state->tag.bytes_so_far += copy;
    if (v_state->tag.bytes_so_far == v_state->tag.bytes_expected) {
	v_state->state = vu_blank;
	code = 0;
    } else
	code = pxNeedData;
    return code;
}
static int
vu_tag_dispatch(px_args_t * par, px_state_t * pxs)
{
    px_vendor_state_t *v_state = pxs->vendor_state;

    if (v_state->state == vu_blank) {
	if (par->source.available < 6)
	    return pxNeedData;
	v_state->tag.tag_id
	    = uint16at(par->source.data, pxs->data_source_big_endian);
	v_state->tag.bytes_expected
	    = uint32at(par->source.data + 2, pxs->data_source_big_endian);
	v_state->tag.bytes_so_far = 0;
	v_state->state = vu_tagged;
	memset(v_state->row, 0xff, v_state->data_per_row);
	par->source.data += 6;
	par->source.available -= 6;
	par->source.position += 6;
	if_debug2('I', "Signature %04X expect=%d\n",
		  v_state->tag.tag_id, v_state->tag.bytes_expected);
    };
    if (v_state->state != vu_blank) {
	if_debug4('I', "tag %04X bytes=%d/%d avail=%d\n",
		  v_state->tag.tag_id,
		  v_state->tag.bytes_so_far,
		  v_state->tag.bytes_expected, par->source.available);
	switch (v_state->tag.tag_id) {
	    case 0x9031:
		/* CLJ3600 specific; 3550 returns IllegalTag */
	    case 0x9011:
	    case 0x9021:
		return tag_dispatch_90X1(par, pxs);
		break;
	    case 0x1001:
		/* do nothing */
		if (v_state->tag.bytes_expected == 0) {
		    v_state->state = vu_blank;
		    return 0;
		}
		/* probably should return error */
		return tag_dispatch_generic(par, pxs);
		break;
	    case 0x8000:
		return tag_dispatch_8000(par, pxs);
		break;
	    case 0x8001:
		return tag_dispatch_generic(par, pxs);
		break;
	    default:
		return_error(errorIllegalTag);
		break;
	}
    }
    /* unreachable */
    return pxNeedData;
}
/* -------------------- private routines ends ----------------- */
/* -------------------- public routines below ----------------- */
int
pxJR3BeginImage(px_args_t * par, px_state_t * pxs)
{
    int code = 0;

    if (!par->source.available) {
	px_vendor_state_t *v_state =
	    gs_alloc_struct(pxs->memory, px_vendor_state_t,
			    &st_px_vendor_state,
			    "pxJR3BeginImage(vendor_state)");
	byte *row;
	UINT16 *qvalues;

	if (v_state == 0)
	    return_error(errorInsufficientMemory);
	if (par->pv[8]) {	/* SourceWidth */
	    v_state->SourceWidth = par->pv[8]->value.i;
	    v_state->data_per_row = par->pv[8]->value.i;
	    v_state->color_space = pxs->pxgs->color_space;
	    if (v_state->color_space != eGraySub)
		v_state->data_per_row *= 3;
	}
	v_state->state = vu_blank;
	v_state->rowwritten = 0;

	row = gs_alloc_byte_array(pxs->memory, 1, v_state->data_per_row,
				  "pxJR3BeginImage(row)");

	if (row == 0)
	    return_error(errorInsufficientMemory);

	qvalues =
	    (UINT16 *) gs_alloc_byte_array(pxs->memory, 192, sizeof(UINT16),
					   "pxJR3BeginImage(qvalues)");

	if (qvalues == 0)
	    return_error(errorInsufficientMemory);
	pxs->vendor_state = v_state;
	pxs->vendor_state->row = row;
	pxs->vendor_state->qvalues = qvalues;
	pxs->vendor_state->qcount = 0;
    }
    if (par->pv[1]) {
	ulong len = par->pv[1]->value.i;

	/* zero or two payloads */
	if (len) {
	    code = vu_tag_dispatch(par, pxs);

	    /* Multiple payloads, reset to read 2nd */
	    if (code == 0 && par->source.position < len)
	        code = vu_tag_dispatch(par, pxs);
	}
    }
    return code;
}

int
pxJR3ReadImage(px_args_t * par, px_state_t * pxs)
{
    int code = 0;

    if (par->pv[10])
	pxs->vendor_state->StartLine = par->pv[10]->value.i;
    if (par->pv[11])
	pxs->vendor_state->BlockHeight = par->pv[11]->value.i;
    if (par->pv[1]) {
	ulong len = par->pv[1]->value.i;

	if (len && par->source.available == 0)
	    return pxNeedData;
	/* ReadImage has a single payload */
	code = vu_tag_dispatch(par, pxs);
    }
    return code;
}

int
pxJR3EndImage(px_args_t * par, px_state_t * pxs)
{
    int code = 0;

    if (par->pv[1]) {
	ulong len = par->pv[1]->value.i;

	/* EndImage does not have payload */
	if (len)
	    return_error(errorIllegalAttributeValue);
    }
    /* tidy up resources used in pxJR3BeginImage */
    gs_free_object(pxs->memory, pxs->vendor_state->qvalues,
		   "pxJR3EndImage(qvalues)");
    gs_free_object(pxs->memory, pxs->vendor_state->row, "pxJR3EndImage(row)");
    gs_free_object(pxs->memory, pxs->vendor_state,
		   "pxJR3EndImage(vendor_state)");
    return code;
}
