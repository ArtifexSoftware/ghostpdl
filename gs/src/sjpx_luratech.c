/* Copyright (C) 2001-2006 artofcode LLC.
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
/* JPXDecode filter implementation -- hooks in the Luratech JPEG2K CSDK */
 
#include "memory_.h"
#include "malloc_.h"
#include "gserrors.h"
#include "gserror.h"
#include "gdebug.h"
#include "strimpl.h"
#include "sjpx_luratech.h"

#include <lwf_jp2.h>

/* JPXDecode stream implementation using the Luratech library */

/* if linking against a SDK build that requires a separate license key,
   you can change the following undefs to defines and set them here. */
/***
#ifndef JP2_LICENSE_NUM_1
# undef JP2_LICENSE_NUM_1
#endif
#ifndef JP2_LICENSE_NUM_2
# undef JP2_LICENSE_NUM_2
#endif
***/

/* As with the /JBIG2Decode filter, we let the library do its  
   memory management through malloc() etc. and rely on our release() 
   proc being called to deallocate state.
*/

private_st_jpxd_state(); /* creates a gc object for our state,
			    defined in sjpx.h */

#define JPX_BUFFER_SIZE 1024

/** callback for the codec library */

/* memory allocation */
private void * JP2_Callback_Conv
s_jpx_alloc(long size, JP2_Callback_Param param)
{
    void *result = malloc(size);

    return result;   
}

/* memory free */
private JP2_Error JP2_Callback_Conv
s_jpx_free(void *ptr, JP2_Callback_Param param)
{
    free(ptr);

    return cJP2_Error_OK;
}

/* pass any available input to the library */
private unsigned long JP2_Callback_Conv
s_jpxd_read_data(unsigned char *pucData,
			    unsigned long ulPos, unsigned long ulSize, 
			    JP2_Callback_Param param)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) param;
    unsigned long copy_bytes = min(ulSize, state->inbuf_fill - ulPos);

    memcpy(pucData, state->inbuf + ulPos, copy_bytes);

    return copy_bytes;
}

/* write decompressed data into our image buffer */
private JP2_Error JP2_Callback_Conv
s_jpxd_write_data(unsigned char * pucData,
			       short sComponent,
			       unsigned long ulRow,
			       unsigned long ulStart,
			       unsigned long ulNum,
			       JP2_Callback_Param param)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) param;
    unsigned char *p;
    unsigned long i;

    /* check input */
    if (ulRow >= state->height) return cJP2_Error_Invalid_Height;
    if (ulStart + ulNum >= state->width) ulNum = state->width - ulStart;

    /* Here we just copy a byte at a time into an image buffer,
       interleaving with whatever data already exists. For multi-
       component images, it would be more efficient to save rows
       from each call in planar buffers and interleave a tile at
       a time into a stipe buffer for output */

    /* todo: handle non-8-bit samples, subsampled components,
	and Y'CrCb colorspace rotation */

    p = state->image + state->stride*ulRow + state->ncomp*ulStart;
    if (state->ncomp == 1)
	memcpy(p, pucData, ulNum);
    else for (i = 0; i < ulNum; i++) {
	p[sComponent] = pucData[i];
	p += state->ncomp;
    }
    return cJP2_Error_OK;
}

private int
s_jpxd_inbuf(stream_jpxd_state *state, stream_cursor_read * pr)
{
    long in_size = pr->limit - pr->ptr;

    /* allocate the input buffer if needed */
    if (state->inbuf == NULL) {
	state->inbuf = malloc(JPX_BUFFER_SIZE);
	if (state->inbuf == NULL) return gs_error_VMerror;
	state->inbuf_size = JPX_BUFFER_SIZE;
	state->inbuf_fill = 0;
    }

    /* grow the input buffer if needed */
    if (state->inbuf_size < state->inbuf_fill + in_size) {
	unsigned char *new;
	unsigned long new_size = state->inbuf_size;

	while (new_size < state->inbuf_fill + in_size)
	    new_size = new_size << 1;

	if_debug1('s', "[s]jpxd growing input buffer to %lu bytes\n", 
		new_size);
	new = realloc(state->inbuf, new_size);
	if (new == NULL) return gs_error_VMerror;

	state->inbuf = new;
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

/* initialize the steam.
   this involves allocating the stream and image structures, and
   initializing the decoder.
 */
private int
s_jpxd_init(stream_state * ss)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) ss;

    state->jpx_memory = ss->memory ? ss->memory->non_gc_memory : gs_lib_ctx_get_non_gc_memory_t();

    state->handle = (JP2_Decomp_Handle)NULL;

    state->inbuf = NULL;
    state->inbuf_size = 0;
    state->inbuf_fill = 0;

    state->ncomp = 0;
    state->bpc = 0;
    state->width = 0;
    state->height = 0;
    state->stride = 0;
    state->image = NULL;
    state->offset = 0;

    return 0;
}

/* process a secton of the input and return any decoded data.
   see strimpl.h for return codes.
 */
private int
s_jpxd_process(stream_state * ss, stream_cursor_read * pr,
                 stream_cursor_write * pw, bool last)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) ss;
    JP2_Error err;
    JP2_Property_Value result;
    long in_size = pr->limit - pr->ptr;
    long out_size = pw->limit - pw->ptr;

    if (in_size > 0) {
	/* buffer available data */
	s_jpxd_inbuf(state, pr);
    }

    if (last == 1) {
	/* we have all the data, decode and return */

	if (state->handle == (JP2_Decomp_Handle)NULL) {
	    /* initialize decompressor */
	    err = JP2_Decompress_Start(&state->handle,
		/* memory allocator callbacks */ 
		s_jpx_alloc, (JP2_Callback_Param)state,
		s_jpx_free,  (JP2_Callback_Param)state,
		/* our read callback */
		s_jpxd_read_data, (JP2_Callback_Param)state
	    );
	    if (err != cJP2_Error_OK) {
		dlprintf1("Luratech JP2 error %d starting decompression\n", (int)err);
		return ERRC;
	    }
#if defined(JP2_LICENSE_NUM_1) && defined(JP2_LICENSE_NUM_2)
            /* set the license keys if appropriate */
            error = JP2_Decompress_SetLicense(state->handle,
                JP2_LICENSE_NUM_1, JP2_LICENSE_NUM_2);
            if (error != cJP2_Error_OK) {
		dlprintf1("Luratech JP2 error %d setting license\n", (int)err);
		return ERRC;
	    }
#endif
	    /* parse image parameters */
	    err = JP2_Decompress_GetProp(state->handle,
		cJP2_Prop_Components, &result, -1, -1);
	    if (err != cJP2_Error_OK) {
		dlprintf1("Luratech JP2 error %d decoding number of image components\n", (int)err);
		return ERRC;
	    }
	    state->ncomp = result;

	    if_debug1('w', "[w]jpxd image has %d components\n", state->ncomp);
	
	    {
		const char *cspace = "unknown";
		err = JP2_Decompress_GetProp(state->handle,
			cJP2_Prop_Extern_Colorspace, &result, -1, -1);
		if (err != cJP2_Error_OK) {
		    dlprintf1("Luratech JP2 error %d decoding colorspace\n", (int)err);
		    return ERRC;
		}
		switch (result) {
		    case cJP2_Colorspace_Gray: cspace = "gray"; break;
		    case cJP2_Colorspace_RGBa: cspace = "sRGB"; break;
		    case cJP2_Colorspace_RGB_YCCa:
			cspace = "sRGB YCrCb"; break;
		    case cJP2_Colorspace_CIE_LABa:
			cspace = "CIE Lab"; break;
		    case cJP2_Colorspace_ICCa:
			cspace = "ICC profile"; break;
		    case cJP2_Colorspace_Palette_Gray:
			cspace = "indexed gray"; break;
		    case cJP2_Colorspace_Palette_RGBa:
			cspace = "indexed sRGB"; break;
		    case cJP2_Colorspace_Palette_RGB_YCCa:
			cspace = "indexed sRGB YCrCb"; break;
		    case cJP2_Colorspace_Palette_CIE_LABa:
			cspace = "indexed CIE Lab"; break;
		    case cJP2_Colorspace_Palette_ICCa:
			cspace = "indexed with ICC profile"; break;
	    	}
		if_debug1('w', "[w]jpxd image colorspace is %s\n", cspace);
	    }

	    /* the library doesn't return the overall image dimensions
	       or depth, so we take the maximum of the component values */
	    state->width = 0;
            state->height = 0;
	    state->bpc = 0;
	    {
		int comp;
		int width, height;
		int bits, is_signed;
		for (comp = 0; comp < state->ncomp; comp++) {
		    err= JP2_Decompress_GetProp(state->handle,
			cJP2_Prop_Width, &result, -1, (short)comp);
		    if (err != cJP2_Error_OK) {
			dlprintf2("Luratech JP2 error %d decoding "
				"width for component %d\n", (int)err, comp);
			return ERRC;
		    }
		    width = result;
		    err= JP2_Decompress_GetProp(state->handle,
			cJP2_Prop_Height, &result, -1, (short)comp);
		    if (err != cJP2_Error_OK) {
			dlprintf2("Luratech JP2 error %d decoding "
				"height for component %d\n", (int)err, comp);
			return ERRC;
		    }
		    height = result;
		    err= JP2_Decompress_GetProp(state->handle,
			cJP2_Prop_Bits_Per_Sample, &result, -1, (short)comp);
		    if (err != cJP2_Error_OK) {
			dlprintf2("Luratech JP2 error %d decoding "
				"bits per sample for component %d\n", (int)err, comp);
			return ERRC;
		    }
		    bits = result;
		    err= JP2_Decompress_GetProp(state->handle,
			cJP2_Prop_Signed_Samples, &result, -1, (short)comp);
		    if (err != cJP2_Error_OK) {
			dlprintf2("Luratech JP2 error %d decoding " 
				"signedness of component %d\n", (int)err, comp);
			return ERRC;
		    }
		    is_signed = result;
		    if_debug5('w',
			"[w]jpxd image component %d has %dx%d %s %d bit samples\n",
			comp, width, height,
			is_signed ? "signed" : "unsigned", bits);

		    /* update image maximums */
		    if (state->width < width) state->width = width;
		    if (state->height < height) state->height = height;
		    if (state->bpc < bits) state->bpc = bits;
		}
	    }
	    if_debug3('w', "[w]jpxd decoding image at %ldx%ld"
		" with %d bits per component\n", 
		state->width, state->height, state->bpc);

	}

	if (state->handle != (JP2_Decomp_Handle)NULL && 
		state->image == NULL) {

	    /* allocate our output buffer */
	    state->stride = state->width*state->ncomp;
	    state->image = malloc(state->stride*state->height);
	    if (state->image == NULL) return ERRC;

	    /* attach our output callback */
	    err = JP2_Decompress_SetProp(state->handle,
		cJP2_Prop_Output_Parameter, (JP2_Property_Value)state);
	    if (err != cJP2_Error_OK) {
		dlprintf1("Luratech JP2 error %d setting output parameter\n", (int)err);
		return ERRC;
	    }
	    err = JP2_Decompress_SetProp(state->handle,
		cJP2_Prop_Output_Function, 
		(JP2_Property_Value)s_jpxd_write_data);
	    if (err != cJP2_Error_OK) {
		dlprintf1("Luratech JP2 error %d setting output function\n", (int)err);
		return ERRC;
	    }

	    /* decompress the image */
	    err = JP2_Decompress_Image(state->handle);
	    if (err != cJP2_Error_OK) {
		dlprintf1("Luratech JP2 error %d decoding image data\n", (int)err);
		return ERRC; /* parsing error */
	    }
	}

	/* copy out available data */
	if (state->image != NULL && out_size > 0) {
	    /* copy some output data */
	    long available = min(out_size,
		state->stride*state->height - state->offset);
	    memcpy(pw->ptr + 1, state->image + state->offset, available);
	    state->offset += available;
	    pw->ptr += available;
	    /* more output to deliver? */
	    if (state->offset == state->stride*state->height) return 1;
	}
    }

    /* ask for more data */
    return 0;
}

/* stream release.
   free all our decoder state.
 */
private void
s_jpxd_release(stream_state *ss)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) ss;
    JP2_Error err;

    if (state) {
	err = JP2_Decompress_End(state->handle);
	if (state->inbuf) free(state->inbuf);
	if (state->image) free(state->image);
    }
}

/* stream template */
const stream_template s_jpxd_template = {
    &st_jpxd_state, 
    s_jpxd_init,
    s_jpxd_process,
    1024, 1024,   /* min in and out buffer sizes we want */
    s_jpxd_release
};

