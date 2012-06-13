/* Copyright (C) 2001-2012 Artifex Software, Inc.
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



/* opj filter implementation using OpenJPeg library */

#include "memory_.h"
#include "malloc_.h"
#include "gserrors.h"
#include "gdebug.h"
#include "strimpl.h"
#include "sjpx_openjpeg.h"


/* As with the /JBIG2Decode filter, we let the library do its
   memory management through malloc() etc. and rely on our release()
   proc being called to deallocate state.
*/

gs_private_st_simple(st_jpxd_state, stream_jpxd_state,
    "JPXDecode filter state"); /* creates a gc object for our state,
                            defined in sjpx.h */

static int s_opjd_accumulate_input(stream_jpxd_state *state, stream_cursor_read * pr);

/* initialize the steam.
   this involves allocating the stream and image structures, and
   initializing the decoder.
 */
static int
s_opjd_init(stream_state * ss)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) ss;
	opj_dparameters_t parameters;	/* decompression parameters */

    if (state->jpx_memory == NULL) {
        state->jpx_memory = ss->memory->non_gc_memory;
    }

    /* get a decoder handle */
    state->opj_dinfo_p = opj_create_decompress(CODEC_JP2);

    if (state->opj_dinfo_p == NULL)
            return_error(gs_error_VMerror);

    /* catch events using our callbacks and give a local context */
    //opj_set_event_mgr((opj_common_ptr)dinfo, &event_mgr, stderr);

    /* set decoding parameters to default values */
    opj_set_default_decoder_parameters(&parameters);

    /* setup the decoder decoding parameters using user parameters */
    opj_setup_decoder(state->opj_dinfo_p, &parameters);

    state->image = NULL;
    state->inbuf = NULL;
    state->inbuf_size = 0;
    state->inbuf_fill = 0;
    state->out_offset = 0;
    state->img_offset = 0;
    state->pdata = NULL;
    state->sign_comps = NULL;

    return 0;
}

/* calculate the real component data idx after scaling */
static inline unsigned long
get_scaled_idx(stream_jpxd_state *state, int compno, unsigned long idx, unsigned long x, unsigned long y)
{
    if (state->samescale)
	return idx;
	
    return (y/state->image->comps[compno].dy*state->width + x)/state->image->comps[compno].dx;
}

/* convert state->image from YCrCb to RGB */
static int
s_jpxd_ycc_to_rgb(stream_jpxd_state *state)
{
    unsigned int max_value = ~(-1 << state->image->comps[0].prec); /* maximum of channel value */
    int flip_value = 1 << (state->image->comps[0].prec-1);
    int p[3], q[3], i;
    int sgnd[2];  /* Cr, Cb */
    unsigned long x, y, idx;
    int *row_bufs[3];

    if (state->out_numcomps != 3)
    return -1;

    for (i=0; i<2; i++)
        sgnd[i] = state->image->comps[i+1].sgnd;

    for (i=0; i<3; i++)
        row_bufs[i] = malloc(sizeof(int)*state->width);

    if (!row_bufs[0] || !row_bufs[1] || !row_bufs[2])
        return_error(gs_error_VMerror);

    idx=0;
    for (y=0; y<state->height; y++)
    {
        /* backup one row. the real buffer might be overriden when there is scale */
        for (i=0; i<3; i++)
        {
            if ((y % state->image->comps[i].dy) == 0) /* get the buffer once every dy rows */
                memcpy(row_bufs[i], &(state->image->comps[i].data[get_scaled_idx(state, i, idx, 0, y)]), sizeof(int)*state->width/state->image->comps[i].dx);
        }
        for (x=0; x<state->width; x++, idx++)
        {
            for (i = 0; i < 3; i++)
                p[i] = row_bufs[i][x/state->image->comps[i].dx];

            if (!sgnd[0])
                p[1] -= flip_value;
            if (!sgnd[1])
                p[2] -= flip_value;

            /* rotate to RGB */
#ifdef JPX_USE_IRT
            q[1] = p[0] - ((p[1] + p[2])>>2);
            q[0] = p[1] + q[1];
            q[2] = p[2] + q[1];
#else
            q[0] = (int)((double)p[0] + 1.402 * p[2]);
            q[1] = (int)((double)p[0] - 0.34413 * p[1] - 0.71414 * p[2]);
            q[2] = (int)((double)p[0] + 1.772 * p[1]);
#endif
            /* clamp */
            for (i = 0; i < 3; i++){
                if (q[i] < 0) q[i] = 0;
                else if (q[i] > max_value) q[i] = max_value;
            }

            /* write out the pixel */
            for (i = 0; i < 3; i++)
                state->image->comps[i].data[get_scaled_idx(state, i, idx, x, y)] = q[i];
        }
    }

    for (i=0; i<3; i++)
        free(row_bufs[i]);

    return 0;
}

static int decode_image(stream_jpxd_state * const state)
{
    opj_cio_t *cio = NULL;
    int numprimcomp = 0, alpha_comp = -1, compno, rowbytes;

    /* open a byte stream */
    cio = opj_cio_open((opj_common_ptr)state->opj_dinfo_p, state->inbuf, state->inbuf_fill);
    if (cio == NULL)
            return ERRC;

    /* decode the stream and fill the image structure */
    state->image = opj_decode(state->opj_dinfo_p, cio, state->colorspace == gs_jpx_cs_indexed);
    if(state->image == NULL)
    {
        dlprintf("openjpeg: failed to decode image!\n");
        opj_cio_close(cio);
        return ERRC;
    }

    /* close the byte stream */
    opj_cio_close(cio);

    /* check dimension and prec */
    if (state->image->numcomps == 0)
            return ERRC;

    state->width = state->image->comps[0].w;
    state->height = state->image->comps[0].h;
    state->bpp = state->image->comps[0].prec;
    state->samescale = true;
    for(compno = 1; compno < state->image->numcomps; compno++)
    {
        if (state->bpp != state->image->comps[compno].prec)
            return ERRC; // Not supported.
        if (state->width < state->image->comps[compno].w)
            state->width = state->image->comps[compno].w;
        if (state->height < state->image->comps[compno].h)
            state->height = state->image->comps[compno].h;
        if (state->image->comps[compno].dx != state->image->comps[0].dx ||
                state->image->comps[compno].dy != state->image->comps[0].dy)
            state->samescale = false;
    }


    /* find alpha component and regular color component by channel definition */
    for(compno = 0; compno < state->image->numcomps; compno++)
    {
        if (state->image->comps[compno].typ == CTYPE_COLOR)
            numprimcomp++;
        else if (state->image->comps[compno].typ == CTYPE_OPACITY)
            alpha_comp = compno;
    }

    /* color space and number of components */
    switch(state->image->color_space)
    {
        case CLRSPC_GRAY:
            state->colorspace = gs_jpx_cs_gray;
            break;
        case CLRSPC_CMYK:
            state->colorspace = gs_jpx_cs_cmyk;
            break;
        case CLRSPC_UNKNOWN: /* make the best guess based on number of channels */
        {
            if (numprimcomp < 3)
            {
                state->colorspace = gs_jpx_cs_gray;
            }
            else if (numprimcomp == 4)
            {
                state->colorspace = gs_jpx_cs_cmyk;
             }
            else
            {
                state->colorspace = gs_jpx_cs_rgb;
            }
            break;
        }
        default: /* CLRSPC_SRGB, CLRSPC_SYCC, CLRSPC_ERGB, CLRSPC_EYCC */
            state->colorspace = gs_jpx_cs_rgb;
    }

    state->alpha_comp = -1;
    if (state->alpha)
    {
        state->alpha_comp = alpha_comp;
        state->out_numcomps = 1;
    }
    else
        state->out_numcomps = numprimcomp;

    /* round up bpp 12->16 */
    if (state->bpp == 12)
        state->bpp = 16;

    /* calculate  total data */
    rowbytes =  (state->width*state->bpp*state->out_numcomps+7)/8;
    state->totalbytes = rowbytes*state->height;

    /* convert input from YCC to RGB */
    if (state->image->color_space == CLRSPC_SYCC || state->image->color_space == CLRSPC_EYCC)
        s_jpxd_ycc_to_rgb(state);

    state->pdata = malloc(sizeof(int*)*state->image->numcomps);
    if (!state->pdata)
        return_error(gs_error_VMerror);

    /* compensate for signed data (signed => unsigned) */
    state->sign_comps = malloc(sizeof(int)*state->image->numcomps);
    if (!state->sign_comps)
        return_error(gs_error_VMerror);

    for(compno = 0; compno < state->image->numcomps; compno++)
    {
        if (state->image->comps[compno].sgnd)
            state->sign_comps[compno] = ((state->bpp%8)==0) ? 0x80 : (1<<(state->bpp-1));
        else
            state->sign_comps[compno] = 0;
    }

    return 0;
}

static int process_one_trunk(stream_jpxd_state * const state, stream_cursor_write * pw)
{
     /* read data from image to pw */
     unsigned long out_size = pw->limit - pw->ptr;
     int bytepp1 = state->bpp/8; /* bytes / pixel for one output component */
     int bytepp = state->out_numcomps*state->bpp/8; /* bytes / pixel all components */
     unsigned long write_size = min(out_size-(bytepp?(out_size%bytepp):0), state->totalbytes-state->out_offset);
     unsigned long in_offset = state->out_offset*8/state->bpp/state->out_numcomps; /* component data offset */
     int shift_bit = state->bpp-state->image->comps[0].prec; /*difference between input and output bit-depth*/
     int img_numcomps = min(state->out_numcomps, state->image->numcomps), /* the actual number of channel data used */
             compno;
     unsigned long i; int b;
     byte *pend = pw->ptr+write_size+1; /* end of write data */

     if (state->bpp < 8)
         in_offset = state->img_offset;

     pw->ptr++;

     if (state->alpha && state->alpha_comp == -1)
     {/* return 0xff for all */
         memset(pw->ptr, 0xff, write_size);
         pw->ptr += write_size;
     }
     else if (state->samescale)
     {
         if (state->alpha)
             state->pdata[0] = &(state->image->comps[state->alpha_comp].data[in_offset]);
         else
         {
             for (compno=0; compno<img_numcomps; compno++)
                 state->pdata[compno] = &(state->image->comps[compno].data[in_offset]);
         }
         if (shift_bit == 0 && state->bpp == 8) /* optimized for the most common case */
         {
             while (pw->ptr < pend)
             {
                 for (compno=0; compno<img_numcomps; compno++)
                     *(pw->ptr++) = *(state->pdata[compno]++) + state->sign_comps[compno]; /* copy input buffer to output */
             }
         }
         else
         {
             if ((state->bpp%8)==0)
             {
                 while (pw->ptr < pend)
                 {
                     for (compno=0; compno<img_numcomps; compno++)
                     {
                         for (b=0; b<bytepp1; b++)
                             *(pw->ptr++) = (((*(state->pdata[compno]) << shift_bit) >> (8*(bytepp1-b-1))))
                                                                         + (b==0 ? state->sign_comps[compno] : 0); /* split and shift input int to output bytes */
                         state->pdata[compno]++; 
                     }
                 }
             }
             else
             {   
                 /* shift_bit = 0, bpp < 8 */
                 unsigned long image_total = state->width*state->height;
                 int bt=0; int bit_pos = 0;
                 int rowbytes =  (state->width*state->bpp*state->out_numcomps+7)/8; /*row bytes */
                 int currowcnt = state->out_offset % rowbytes; /* number of bytes filled in current row */
                 int start_comp = (currowcnt*8) % img_numcomps; /* starting component for this round of output*/
                 if (start_comp != 0)
                 {
                     for (compno=start_comp; compno<img_numcomps; compno++)
                     {
                         if (state->img_offset < image_total)
                         {
                             bt <<= state->bpp;
                             bt += *(state->pdata[compno]-1) + state->sign_comps[compno];
                         }
                         bit_pos += state->bpp;
                         if (bit_pos >= 8)
                         {
                             *(pw->ptr++) = bt >> (bit_pos-8);
                             bit_pos -= 8;
                             bt &= (1<<bit_pos)-1;
                         }
                     }
                 }
                 while (pw->ptr < pend)
                 {
                     for (compno=0; compno<img_numcomps; compno++)
                     {
                         if (state->img_offset < image_total)
                         {
                             bt <<= state->bpp;
                             bt += *(state->pdata[compno]++) + state->sign_comps[compno];
                         }
                         bit_pos += state->bpp;
                         if (bit_pos >= 8)
                         {
                             *(pw->ptr++) = bt >> (bit_pos-8);
                             bit_pos -= 8;
                             bt &= (1<<bit_pos)-1;
                         }
                     }
                     state->img_offset++;
                     if (bit_pos != 0 && state->img_offset % state->width == 0)
                     {
                         /* row padding */
                         *(pw->ptr++) = bt << (8 - bit_pos);
                         bit_pos = 0;
                         bt = 0;
                     }
                 }
             }
         }
     }
     else
     {
		 /* sampling required */
         unsigned long y_offset = in_offset / state->width;
         unsigned long x_offset = in_offset % state->width;
         while (pw->ptr < pend)
         {
             if ((state->bpp%8)==0)
             {
                 if (state->alpha)
                 {
                     int in_offset_scaled = (y_offset/state->image->comps[state->alpha_comp].dy*state->width + x_offset)/state->image->comps[state->alpha_comp].dx;
                     for (b=0; b<bytepp1; b++)
                             *(pw->ptr++) = (((state->image->comps[state->alpha_comp].data[in_offset_scaled] << shift_bit) >> (8*(bytepp1-b-1))))
                                                                     + (b==0 ? state->sign_comps[state->alpha_comp] : 0);
                 }
                 else
                 {
                     for (compno=0; compno<img_numcomps; compno++)
                     {
                         int in_offset_scaled = (y_offset/state->image->comps[compno].dy*state->width + x_offset)/state->image->comps[compno].dx;
                         for (b=0; b<bytepp1; b++)
                             *(pw->ptr++) = (((state->image->comps[compno].data[in_offset_scaled] << shift_bit) >> (8*(bytepp1-b-1))))
                                                                             + (b==0 ? state->sign_comps[compno] : 0);
                     }
                 }
                 x_offset++;
                 if (x_offset >= state->width)
                 {
                     y_offset++;
                     x_offset = 0;
                 }
             }
             else
             {
                 unsigned long image_total = state->width*state->height;
                 int compno = state->alpha ? state->alpha_comp : 0;
                 /* only grayscale can have such bit-depth, also shift_bit = 0, bpp < 8 */
                  int bt=0;
                  for (i=0; i<8/state->bpp; i++)
                  {
                      bt = bt<<state->bpp;
                      if (state->img_offset < image_total && !(i!=0 && state->img_offset % state->width == 0))
                      {
                          int in_offset_scaled = (y_offset/state->image->comps[compno].dy*state->width + x_offset)/state->image->comps[compno].dx;
                          bt += state->image->comps[compno].data[in_offset_scaled] + state->sign_comps[compno];
                          state->img_offset++;
                          x_offset++;
                          if (x_offset >= state->width)
                          {
                              y_offset++;
                              x_offset = 0;
                          }
                      }
                   }
                  *(pw->ptr++) = bt;
             }
         }
     }
     state->out_offset += write_size;
     pw->ptr--;
     if (state->out_offset == state->totalbytes)
         return EOFC; /* all data returned */
     else
         return 1; /* need more calls */
 }

/* process a section of the input and return any decoded data.
   see strimpl.h for return codes.
 */
static int
s_opjd_process(stream_state * ss, stream_cursor_read * pr,
                 stream_cursor_write * pw, bool last)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) ss;
    long in_size = pr->limit - pr->ptr;

    if (state->opj_dinfo_p == NULL)
	return ERRC;

    if (in_size > 0) 
    {
        /* buffer available data */
        s_opjd_accumulate_input(state, pr);
    }

    if (last == 1) 
    {
        if (state->image == NULL)
        {
            int ret = decode_image(state);
            if (ret != 0)
                return ret;
        }

        /* copy out available data */
        return process_one_trunk(state, pw);

    }

    /* ask for more data */
    return 0;
}

/* Set the defaults */
static void
s_opjd_set_defaults(stream_state * ss) {
    stream_jpxd_state *const state = (stream_jpxd_state *) ss;

    state->alpha = false;
    state->colorspace = gs_jpx_cs_rgb;
}

/* stream release.
   free all our decoder state.
 */
static void
s_opjd_release(stream_state *ss)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) ss;

    /* free image data structure */
    if (state->image)
        opj_image_destroy(state->image);
		
    /* free decoder handle */
    if (state->opj_dinfo_p)
	opj_destroy_decompress(state->opj_dinfo_p);

    /* free input buffer */
    if (state->inbuf)
	free(state->inbuf);

    if (state->pdata)
	free(state->pdata);

    if (state->sign_comps)
	free(state->sign_comps);
}


static int
s_opjd_accumulate_input(stream_jpxd_state *state, stream_cursor_read * pr)
{
    long in_size = pr->limit - pr->ptr;

    /* grow the input buffer if needed */
    if (state->inbuf_size < state->inbuf_fill + in_size) 
    {
        unsigned char *new_buf;
        unsigned long new_size = state->inbuf_size==0 ? in_size : state->inbuf_size;

        while (new_size < state->inbuf_fill + in_size)
            new_size = new_size << 1;

        if_debug1('s', "[s]opj growing input buffer to %lu bytes\n",
                new_size);
	if (state->inbuf == NULL)
            new_buf = (byte *) malloc(new_size);
	else
            new_buf = (byte *) realloc(state->inbuf, new_size);
        if (new_buf == NULL) return_error( gs_error_VMerror);

        state->inbuf = new_buf;
        state->inbuf_size = new_size;
    }

    /* copy the available input into our buffer */
    /* note that the gs stream library uses offset-by-one
        indexing of its buffers while we use zero indexing */
    memcpy(state->inbuf + state->inbuf_fill, pr->ptr + 1, in_size);
    state->inbuf_fill += in_size;
    pr->ptr += in_size;

    return 0;
}

/* stream template */
const stream_template s_jpxd_template = {
    &st_jpxd_state,
    s_opjd_init,
    s_opjd_process,
    1024, 1024,   /* min in and out buffer sizes we can handle
                     should be ~32k,64k for efficiency? */
    s_opjd_release,
	s_opjd_set_defaults
};
