
/* Copyright (C) 2011 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
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

    return 0;
}

/* convert state->image from YCrCb to RGBa */
static int
s_jpxd_ycc_to_rgb(stream_jpxd_state *state)
{
	unsigned long image_size = state->image->comps[0].w*state->image->comps[0].h, idx;
	unsigned int max_value = ~(-1 << state->image->comps[0].prec); /* maximum of channel value */
	int flip_value = 1 << (state->image->comps[0].prec-1);
	int p[3], q[3], i;
	int sgnd[2];  /* Cr, Cb */

	if (state->image->numcomps != 3)
        return -1;

	for (i=0; i<2; i++)
		sgnd[i] = state->image->comps[i+1].sgnd;
   
    for (idx = 0; idx < image_size; idx++) 
	{
        for (i = 0; i < 3; i++)
			p[i] = state->image->comps[i].data[idx];

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
            state->image->comps[i].data[idx] = q[i];
    }

    return 0;
}

/* process a secton of the input and return any decoded data.
   see strimpl.h for return codes.
 */
static int
s_opjd_process(stream_state * ss, stream_cursor_read * pr,
                 stream_cursor_write * pw, bool last)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) ss;
 	opj_cio_t *cio = NULL;
	int compno, w, h, prec, b;
	long in_size = pr->limit - pr->ptr;
	long out_size = pw->limit - pw->ptr;

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
			/* open a byte stream */
			cio = opj_cio_open((opj_common_ptr)state->opj_dinfo_p, state->inbuf, state->inbuf_fill);
			if (cio == NULL)
				return ERRC;

			/* decode the stream and fill the image structure */
			state->image = opj_decode(state->opj_dinfo_p, cio);
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

			w = state->image->comps[0].w;
			h = state->image->comps[0].h;
			prec = state->image->comps[0].prec;
			for(compno = 1; compno < state->image->numcomps; compno++)
			{
				if (w != state->image->comps[compno].w || h != state->image->comps[compno].h 
						|| prec != state->image->comps[compno].prec)
						return ERRC; // To do: Add support for this.
			}

			/* round up prec to multiple of 8, eg 12->16 */
			prec = (prec+7)/8*8;

			/* calculate row byte and total data */
			state->rowbytes = (w*prec*state->image->numcomps+7)/8;
			state->totalbytes = state->rowbytes*h;

			if (state->image->color_space == CLRSPC_SYCC)
				s_jpxd_ycc_to_rgb(state);
			else if (state->image->color_space == CLRSPC_GRAY)
				state->colorspace = gs_jpx_cs_gray;
		}

		/* copy out available data */
		w = state->image->comps[0].w;
		h = state->image->comps[0].h;
		prec = state->image->comps[0].prec;

		/* round up prec to multiple of 8, eg 12->16 */
		prec = (prec+7)/8*8;
		
		{
			/* read data from image to outbuf */
			int bytepp1 = prec/8, bytepp = state->image->numcomps*bytepp1;
			unsigned long write_size = min(out_size-out_size%bytepp, state->totalbytes-state->out_offset);
			unsigned long in_offset = state->out_offset / bytepp; /* component data offset */
			byte *out_start = pw->ptr+1;
			int shift_bit = prec-state->image->comps[0].prec;
			unsigned long i; int byte;
			pw->ptr++;
			for (i=0; i<write_size;)
			{
				for (compno=0; compno<state->image->numcomps; compno++)
				{
					for (byte=0; byte<bytepp1; byte++)
						*(pw->ptr++) = ((state->image->comps[compno].data[in_offset] << shift_bit) >> (8*(bytepp1-byte-1))) & 0xff;
				}
				state->out_offset += bytepp;
				i+=bytepp;
				in_offset ++;
			}
		}
		pw->ptr--;
		if (state->out_offset == state->totalbytes)
			return EOFC; /* all data returned */
		else
			return 1; /* need more calls */
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
