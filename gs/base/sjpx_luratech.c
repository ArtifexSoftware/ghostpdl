/* Copyright (C) 2001-2006 Artifex Software, Inc.
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
static void * JP2_Callback_Conv
s_jpx_alloc(long size, JP2_Callback_Param param)
{
    void *result = malloc(size);

    return result;
}

/* memory free */
static JP2_Error JP2_Callback_Conv
s_jpx_free(void *ptr, JP2_Callback_Param param)
{
    free(ptr);

    return cJP2_Error_OK;
}

/* pass any available input to the library */
static unsigned long JP2_Callback_Conv
s_jpxd_read_data(unsigned char *pucData,
                            unsigned long ulPos, unsigned long ulSize,
                            JP2_Callback_Param param)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) param;
    unsigned long copy_bytes = min(ulSize, state->inbuf_fill - ulPos);

    memcpy(pucData, state->inbuf + ulPos, copy_bytes);

    return copy_bytes;
}

static unsigned long
jp2_get_value(JP2_Decomp_Handle *handle,
              JP2_Property_Tag tag,
              short comp,
              unsigned long def)
{
    JP2_Property_Value v;

    if (JP2_Decompress_GetProp(handle, tag, &v, -1, comp) != cJP2_Error_OK)
        return def;

    return (unsigned long)v;
}

/* write decompressed data into our image buffer */
static JP2_Error JP2_Callback_Conv
s_jpxd_write_data(unsigned char * pucData,
                               short sComponent,
                               unsigned long ulRow,
                               unsigned long ulStart,
                               unsigned long ulNum,
                               JP2_Callback_Param param)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) param;
    int comp = state->clut[sComponent];

    /* check input */
    if (ulRow >= state->height) return cJP2_Error_Invalid_Height;
    if (ulStart + ulNum >= state->width) ulNum = state->width - ulStart;

    /* Here we just copy a byte at a time into an image buffer,
       interleaving with whatever data already exists. For multi-
       component images, it would be more efficient to save rows
       from each call in planar buffers and interleave a tile at
       a time into a stipe buffer for output */

    if (state->colorspace == gs_jpx_cs_indexed && sComponent == 0 && !state->alpha) {
        JP2_Palette_Params *pal;
        JP2_Error err;
        int i, c;
        unsigned char *dst = &state->image[state->stride * ulRow +
                                           state->ncomp * ulStart + comp];

        err = JP2_Decompress_GetPalette(state->handle, &pal);
        if (err != cJP2_Error_OK)
            return err;

        if (pal->ulEntries != 256)
            return cJP2_Error_Invalid_Colorspace;

        for (i = 0; i < ulNum; i++) {
            unsigned char v = pucData[i];
            for (c = 0; c < state->ncomp; c++)
                *dst++ = (unsigned char)pal->ppulPalette[c][v];
        }
    }
    else if (state->ncomp == 1 && comp == 0) {
        if (state->bpc <= 8) {
            memcpy(&state->image[state->stride*ulRow + state->ncomp*ulStart],
                   pucData, ulNum);
        }
        else {
            unsigned long i;
            unsigned short *src = (unsigned short *)pucData;
            unsigned char *dst = &state->image[state->stride * ulRow + 2 * ulStart];
            unsigned int shift = 16 - state->bpc;
            for (i = 0; i < ulNum; i++) {
                unsigned short v = *src++ << shift;
                *dst++ = (v >> 8) & 0xff;
                *dst++ = v & 0xff;
            }
        }
    }
    else if (comp >= 0) {
        unsigned long cw, ch, i, hstep, vstep, x, y;
        unsigned char *row;

        /* repeat subsampled pixels */
        cw = jp2_get_value(state->handle, cJP2_Prop_Width, comp, state->width);
        ch = jp2_get_value(state->handle, cJP2_Prop_Height, comp, state->height);
        hstep = state->width / cw;
        vstep = state->height / ch;

        if (state->bpc <= 8) {
            row = &state->image[state->stride * ulRow * vstep +
                                state->ncomp * ulStart * hstep + comp];
            for (y = 0; y < vstep; y++) {
                unsigned char *p = row;
                for (i = 0; i < ulNum; i++)
                    for (x = 0; x < hstep; x++) {
                        *p = pucData[i];
                        p += state->ncomp;
                    }
                row += state->stride;
            }
        }
        else {
            int shift = 16 - state->bpc;
            unsigned short *src = (unsigned short *)pucData;
            row = &state->image[state->stride * ulRow * vstep +
                                2 * state->ncomp * ulStart * hstep + 2 * comp];
            for (y = 0; y < vstep; y++) {
                unsigned char *p = row;
                for (i = 0; i < ulNum; i++)
                    for (x = 0; x < hstep; x++) {
                        unsigned short v = *src++ << shift;
                        p[0] = (v >> 8) & 0xff;
                        p[1] = v & 0xff;
                        p += 2 * state->ncomp;
                    }
                row += state->stride;
            }
        }
    }
    return cJP2_Error_OK;
}

/* convert state->image from YCrCb to RGBa */
static int 
s_jpxd_ycc_to_rgb(stream_jpxd_state *state)
{
    int i, y, x;
    int is_signed[2];  /* Cr, Cb */

    if (state->ncomp != 3)
        return -1;

    for (i = 0; i < 2; i++) {
        int comp = state->clut[i + 1];  /* skip Y */
        is_signed[i] = !jp2_get_value(state->handle,
                                   cJP2_Prop_Signed_Samples, comp, 0);
    }

    for (y = 0; y < state->height; y++) {
        unsigned char *row = &state->image[y * state->stride];

        for (x = 0; x < state->stride; x += 3) {
            int p[3], q[3];

            for (i = 0; i < 3; i++)
                p[i] = (int)row[x + i];

            if (is_signed[0])
                p[1] -= 0x80;
            if (is_signed[1])
                p[2] -= 0x80;

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
                else if (q[i] > 0xFF) q[i] = 0xFF;
            }
            /* write out the pixel */
            for (i = 0; i < 3; i++)
                row[x + i] = (unsigned char)q[i];
        }
    }

    return 0;
}

static int
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
static int
s_jpxd_init(stream_state * ss)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) ss;

    if (state->jpx_memory == NULL) {
        state->jpx_memory = ss->memory->non_gc_memory;
    }

    state->handle = (JP2_Decomp_Handle)NULL;

    state->inbuf = NULL;
    state->inbuf_size = 0;
    state->inbuf_fill = 0;

    state->ncomp = 0;
    state->bpc = 0;
    state->clut = NULL;
    state->width = 0;
    state->height = 0;
    state->stride = 0;
    state->image = NULL;
    state->offset = 0;

    return 0;
}

/* Set the defaults */
static int
s_jpxd_set_defaults(stream_state * ss) {
    stream_jpxd_state *const state = (stream_jpxd_state *) ss;
   
    state->alpha = false;
    return 0;
}

/* write component mapping into 'clut' and return number of used components
 */
static int
map_components(JP2_Channel_Def_Params *chans, int nchans, int alpha, int clut[])
{
    int i, cnt = 0;

    for (i = 0; i < nchans; i++)
        clut[i] = -1;

    /* always write the alpha channel as first component */
    if (alpha) {
        for (i = 0; i < nchans; i++) {
            if (chans[i].ulType == cJP2_Channel_Type_Opacity) {
                clut[i] = 0;
                cnt++;
                break;
            }
        }
    } else {
        for (i = 0; i < nchans; i++) {
            if (chans[i].ulType == cJP2_Channel_Type_Color) {
                int assoc = chans[i].ulAssociated -1;
                if (assoc >= nchans)
                    return -1;
                clut[i] = assoc;
                cnt++;
            }
        }
    }
    return cnt;
}

/* process a secton of the input and return any decoded data.
   see strimpl.h for return codes.
 */
static int
s_jpxd_process(stream_state * ss, stream_cursor_read * pr,
                 stream_cursor_write * pw, bool last)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) ss;
    JP2_Error err;
    JP2_Property_Value result;
    long in_size = pr->limit - pr->ptr;
    long out_size = pw->limit - pw->ptr;
    JP2_Colorspace image_cs = cJP2_Colorspace_RGBa;

    if (in_size > 0) {
        /* buffer available data */
        s_jpxd_inbuf(state, pr);
    }

    if (last == 1) {
        /* we have all the data, decode and return */

        if (state->handle == (JP2_Decomp_Handle)NULL) {
            int ncomp;
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
            ncomp = result;

            {
                JP2_Channel_Def_Params *chans = NULL;
                unsigned long nchans = 0;
                err = JP2_Decompress_GetChannelDefs(state->handle, &chans, &nchans);
                if (err != cJP2_Error_OK) {
                    dlprintf1("Luratech JP2 error %d reading channel definitions\n", (int)err);
                    return ERRC;
                }
                state->clut = malloc(nchans * sizeof(int));
                state->ncomp = map_components(chans, nchans, state->alpha, state->clut);
                if (state->ncomp < 0) {
                    dlprintf("Luratech JP2 error decoding channel definitions\n");
                    return ERRC;
                }
            }

            if_debug1('w', "[w]jpxd image has %d components\n", state->ncomp);

            {
                const char *cspace = "unknown";
                err = JP2_Decompress_GetProp(state->handle,
                        cJP2_Prop_Extern_Colorspace, &result, -1, -1);
                if (err != cJP2_Error_OK) {
                    dlprintf1("Luratech JP2 error %d decoding colorspace\n", (int)err);
                    return ERRC;
                }
                image_cs = (JP2_Colorspace)result;
                switch (result) {
                    case cJP2_Colorspace_Gray:
                        cspace = "gray";
                        state->colorspace = gs_jpx_cs_gray;
                        break;
                    case cJP2_Colorspace_RGBa:
                        cspace = "sRGB";
                        state->colorspace = gs_jpx_cs_rgb;
                        break;
                    case cJP2_Colorspace_RGB_YCCa:
                        cspace = "sRGB YCrCb"; break;
                        state->colorspace = gs_jpx_cs_rgb;
                        break;
                    case cJP2_Colorspace_CIE_LABa:
                        cspace = "CIE Lab";
                        state->colorspace = gs_jpx_cs_rgb;
                        break;
                    case cJP2_Colorspace_ICCa:
                        cspace = "ICC profile";
                        state->colorspace = gs_jpx_cs_rgb;
                        break;
                    case cJP2_Colorspace_Palette_Gray:
                        cspace = "indexed gray";
                        state->colorspace = gs_jpx_cs_indexed;
                        break;
                    case cJP2_Colorspace_Palette_RGBa:
                        cspace = "indexed sRGB";
                        state->colorspace = gs_jpx_cs_indexed;
                        break;
                    case cJP2_Colorspace_Palette_RGB_YCCa:
                        cspace = "indexed sRGB YCrCb";
                        state->colorspace = gs_jpx_cs_indexed;
                        break;
                    case cJP2_Colorspace_Palette_CIE_LABa:
                        cspace = "indexed CIE Lab";
                        state->colorspace = gs_jpx_cs_indexed;
                        break;
                    case cJP2_Colorspace_Palette_ICCa:
                        cspace = "indexed with ICC profile";
                        state->colorspace = gs_jpx_cs_indexed;
                        break;
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
                for (comp = 0; comp < ncomp; comp++) {
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
            int real_bpc = state->bpc > 8 ? 16 : state->bpc;
            state->stride = (state->width * max(1, state->ncomp) * real_bpc + 7) / 8;
            state->image = malloc(state->stride*state->height);
            if (state->image == NULL) 
                return ERRC;
            if (state->ncomp == 0) /* make fully opaque mask */
                memset(state->image, 255, state->stride*state->height);

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

            if (image_cs == cJP2_Colorspace_RGB_YCCa) {
                s_jpxd_ycc_to_rgb(state);
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
            if (state->offset == state->stride*state->height)
		return EOFC;
	    else
		return 1;
        }
    }

    /* ask for more data */
    return 0;
}

/* stream release.
   free all our decoder state.
 */
static void
s_jpxd_release(stream_state *ss)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) ss;
    JP2_Error err;

    if (state) {
        err = JP2_Decompress_End(state->handle);
        if (state->inbuf) free(state->inbuf);
        if (state->image) free(state->image);
        if (state->clut) free(state->clut);
    }
}

/* stream template */
const stream_template s_jpxd_template = {
    &st_jpxd_state,
    s_jpxd_init,
    s_jpxd_process,
    1024, 1024,   /* min in and out buffer sizes we can handle
                     should be ~32k,64k for efficiency? */
    s_jpxd_release,
    s_jpxd_set_defaults
};



/*** encode support **/

/* we provide a C-only encode filter for generating JPX image data
   for embedding in PDF. */

/* create a gc object for our state, defined in sjpx_luratech.h */
private_st_jpxe_state();

/* callback for uncompressed data input */
static JP2_Error JP2_Callback_Conv
s_jpxe_read(unsigned char *buffer, short component,
                unsigned long row, unsigned long start,
                unsigned long num, JP2_Callback_Param param)
{
    stream_jpxe_state *state = (stream_jpxe_state *)param;
    unsigned long available, sentinel;
    unsigned char *p;
    int i;

    if (component < 0 || component >= state->components) {
        dlprintf2("Luratech JP2 requested image data for unknown component %d of %u\n",
                (int)component, state->components);
        return cJP2_Error_Invalid_Component_Index;
    }

    /* todo: handle subsampled components and bpc != 8 */

    /* clip to array bounds */
    sentinel = row*state->stride + (start + num)*state->components;
    available = min(sentinel, state->infill);
    num = min(num, available / state->components);

    p = state->inbuf + state->stride*row + state->components*start;
    if (state->components == 1)
        memcpy(buffer, p, num);
    else for (i = 0; i < num; i++) {
        buffer[i] = p[component];
        p += state->components;
    }

    if (available < sentinel) return cJP2_Error_Failure_Read;
    else return cJP2_Error_OK;
}

/* callback for compressed data output */
static JP2_Error JP2_Callback_Conv
s_jpxe_write(unsigned char *buffer,
                unsigned long pos, unsigned long size,
                JP2_Callback_Param param)
{
    stream_jpxe_state *state = (stream_jpxe_state *)param;

    /* verify state */
    if (state == NULL) return cJP2_Error_Invalid_Pointer;

    /* allocate the output buffer if necessary */
    if (state->outbuf == NULL) {
        state->outbuf = malloc(JPX_BUFFER_SIZE);
        if (state->outbuf == NULL) {
            dprintf("jpx encode: failed to allocate output buffer.\n");
            return cJP2_Error_Failure_Malloc;
        }
        state->outsize = JPX_BUFFER_SIZE;
    }

    /* grow the output buffer if necessary */
    while (pos+size > state->outsize) {
        unsigned char *new = realloc(state->outbuf, state->outsize*2);
        if (new == NULL) {
            dprintf1("jpx encode: failed to resize output buffer"
                " beyond %lu bytes.\n", state->outsize);
            return cJP2_Error_Failure_Malloc;
        }
        state->outbuf = new;
        state->outsize *= 2;
        if_debug1('s', "[s] jpxe output buffer resized to %lu bytes\n",
                state->outsize);
    }

    /* copy data into our buffer; we've assured there is enough room. */
    memcpy(state->outbuf + pos, buffer, size);
    /* update high water mark */
    if (state->outfill < pos + size) state->outfill = pos + size;

    return cJP2_Error_OK;
}

/* set defaults for user-configurable parameters */
static void
s_jpxe_set_defaults(stream_state *ss)
{
    stream_jpxe_state *state = (stream_jpxe_state *)ss;

    /* most common default colorspace */
    state->colorspace = gs_jpx_cs_rgb;

    /* default to lossy 60% quality */
    state->lossless = 0;
    state->quality = 60;
}

/* initialize the stream */
static int
s_jpxe_init(stream_state *ss)
{
    stream_jpxe_state *state = (stream_jpxe_state *)ss;
    unsigned long value;
    JP2_Error err;

    /* width, height, bpc and colorspace are set by the client,
       calculate the rest */
    switch (state->colorspace) {
        case gs_jpx_cs_gray: state->components = 1; break;
        case gs_jpx_cs_rgb: state->components = 3; break;
        case gs_jpx_cs_cmyk: state->components = 4; break;
        default: state->components = 0;
    }
    state->stride = state->width * state->components;

    if_debug3('w', "[w] jpxe init %lux%lu image with %d components\n",
        state->width, state->height, state->components);
    if_debug1('w', "[w] jpxe init image is %d bits per component\n", state->bpc);

    if (state->lossless) {
        if_debug0('w', "[w] jpxe image using lossless encoding\n");
        state->quality = 100; /* implies lossless */
    } else {
        if_debug1('w', "[w] jpxe image quality level %d\n", state->quality);
    }

    /* null the input buffer */
    state->inbuf = NULL;
    state->insize = 0;
    state->infill = 0;

    /* null the output buffer */
    state->outbuf = NULL;
    state->outsize = 0;
    state->outfill = 0;
    state->offset = 0;

    /* initialize the encoder */
    err = JP2_Compress_Start(&state->handle,
        /* memory allocator callbacks */
        s_jpx_alloc, (JP2_Callback_Param)state,
        s_jpx_free,  (JP2_Callback_Param)state,
        state->components);
    if (err != cJP2_Error_OK) {
        dlprintf1("Luratech JP2 error %d starting compressor\n", (int)err);
        return ERRC;
    }

#if defined(JP2_LICENSE_NUM_1) && defined(JP2_LICENSE_NUM_2)
    /* set license keys if appropriate */
    error = JP2_Decompress_SetLicense(state->handle,
        JP2_LICENSE_NUM_1, JP2_LICENSE_NUM_2);
    if (error != cJP2_Error_OK) {
        dlprintf1("Luratech JP2 error %d setting license\n", (int)err);
        return ERRC;
    }
#endif

    /* install our callbacks */
    err = JP2_Compress_SetProp(state->handle,
        cJP2_Prop_Input_Parameter, (JP2_Property_Value)state, -1, -1);
    if (err != cJP2_Error_OK) {
        dlprintf1("Luratech JP2 error %d setting input callback parameter.\n", (int)err);
        return ERRC;
    }
    err = JP2_Compress_SetProp(state->handle,
        cJP2_Prop_Input_Function, (JP2_Property_Value)s_jpxe_read, -1, -1);
    if (err != cJP2_Error_OK) {
        dlprintf1("Luratech JP2 error %d setting input callback function.\n", (int)err);
        return ERRC;
    }
    err = JP2_Compress_SetProp(state->handle,
        cJP2_Prop_Write_Parameter, (JP2_Property_Value)state, -1, -1);
    if (err != cJP2_Error_OK) {
        dlprintf1("Luratech JP2 error %d setting compressed output callback parameter.\n", (int)err);
        return ERRC;
    }
    err = JP2_Compress_SetProp(state->handle,
        cJP2_Prop_Write_Function, (JP2_Property_Value)s_jpxe_write, -1, -1);
    if (err != cJP2_Error_OK) {
        dlprintf1("Luratech JP2 error %d setting compressed output callback function.\n", (int)err);
        return ERRC;
    }

    /* set image parameters - the same for all components */
    err = JP2_Compress_SetProp(state->handle,
        cJP2_Prop_Width, state->width, -1, -1);
    if (err != cJP2_Error_OK) {
        dlprintf1("Luratech JP2 error %d setting width\n", (int)err);
        return ERRC;
    }
    err = JP2_Compress_SetProp(state->handle,
        cJP2_Prop_Height, state->height, -1, -1);
    if (err != cJP2_Error_OK) {
        dlprintf1("Luratech JP2 error %d setting height\n", (int)err);
        return ERRC;
    }
    err = JP2_Compress_SetProp(state->handle,
        cJP2_Prop_Bits_Per_Sample, state->bpc, -1, -1);
    if (err != cJP2_Error_OK) {
        dlprintf1("Luratech JP2 error %d setting bits per sample\n", (int)err);
        return ERRC;
    }

    switch (state->colorspace) {
        case gs_jpx_cs_gray: value = cJP2_Colorspace_Gray; break;
        case gs_jpx_cs_rgb: value = cJP2_Colorspace_RGBa; break;
        case gs_jpx_cs_cmyk: value = cJP2_Colorspace_CMYKa; break;
        default:
            dlprintf1("Unknown colorspace %d initializing JP2 encoder\n",
                (int)state->colorspace);
            return ERRC;
    }
    err = JP2_Compress_SetProp(state->handle,
        cJP2_Prop_Extern_Colorspace, value, -1, -1);
    if (err != cJP2_Error_OK) {
        dlprintf1("Luratech JP2 error %d setting colorspace\n", (int)err);
        return ERRC;
    }

    if (state->lossless) {
        /* the default encoding mode is lossless */
        return 0;
    }

    /* otherwise, set 9,7 wavelets and quality-target mode */
    err = JP2_Compress_SetProp(state->handle,
        cJP2_Prop_Wavelet_Filter, cJP2_Wavelet_9_7, -1, -1);
    if (err != cJP2_Error_OK) {
        dlprintf1("Luratech JP2 error %d setting wavelet filter\n", (int)err);
        return ERRC;
    }
    err = JP2_Compress_SetProp(state->handle,
        cJP2_Prop_Rate_Quality, state->quality, -1, -1);
    if (err != cJP2_Error_OK) {
        dlprintf1("Luratech JP2 error %d setting compression quality\n", (int)err);
        return ERRC;
    }

    /* we use the encoder's defaults for all other parameters */

    return 0;
}

/* process input and return any encoded data.
   see strimpl.h for return codes. */
static int
s_jpxe_process(stream_state *ss, stream_cursor_read *pr,
                stream_cursor_write *pw, bool last)
{
    stream_jpxe_state *state = (stream_jpxe_state *)ss;
    long in_size = pr->limit - pr->ptr;
    long out_size = pw->limit - pw->ptr;
    long available;
    JP2_Error err;

    /* HACK -- reinstall our callbacks in case the GC has moved our state structure */
    /* this should be done instead from a pointer relocation callback, or initialization
       moved entirely inside the process routine. */
    err = JP2_Compress_SetProp(state->handle,
        cJP2_Prop_Input_Parameter, (JP2_Property_Value)state, -1, -1);
    if (err != cJP2_Error_OK) {
        dlprintf1("Luratech JP2 error %d setting input callback parameter.\n", (int)err);
        return ERRC;
    }
    err = JP2_Compress_SetProp(state->handle,
        cJP2_Prop_Write_Parameter, (JP2_Property_Value)state, -1, -1);
    if (err != cJP2_Error_OK) {
        dlprintf1("Luratech JP2 error %d setting compressed output callback parameter.\n", (int)err);
        return ERRC;
    }


    if (in_size > 0) {
        /* allocate our input buffer if necessary */
        if (state->inbuf == NULL) {
            state->inbuf = malloc(JPX_BUFFER_SIZE);
            if (state->inbuf == NULL) {
                dprintf("jpx encode: failed to allocate input buffer.\n");
                return ERRC;
            }
            state->insize = JPX_BUFFER_SIZE;
        }

        /* grow our input buffer if necessary */
        while (state->infill + in_size > state->insize) {
            unsigned char *new = realloc(state->inbuf, state->insize*2);
            if (new == NULL) {
                dprintf("jpx encode: failed to resize input buffer.\n");
                return ERRC;
            }
            state->inbuf = new;
            state->insize *= 2;
        }

        /* copy available input */
        memcpy(state->inbuf + state->infill, pr->ptr + 1, in_size);
        state->infill += in_size;
        pr->ptr += in_size;
    }

    if (last && state->outbuf == NULL) {
        /* We have all the data; call the compressor.
           our callback will automatically allocate the output buffer */
        if_debug0('w', "[w] jpxe process compressing image data\n");
        err = JP2_Compress_Image(state->handle);
        if (err != cJP2_Error_OK) {
            dlprintf1("Luratech JP2 error %d compressing image data.\n", (int)err);
            return ERRC;
        }
     }

     if (state->outbuf != NULL) {
        /* copy out any available output data */
        available = min(out_size, state->outfill - state->offset);
        memcpy(pw->ptr + 1, state->outbuf + state->offset, available);
        pw->ptr += available;
        state->offset += available;

        /* do we have any more data? */
        if (state->outfill - state->offset > 0) return 1;
        else return EOFC; /* all done */
    }

    /* something went wrong above */
    return last;
}

/* stream release. free all our state. */
static void
s_jpxe_release(stream_state *ss)
{
    stream_jpxe_state *state = (stream_jpxe_state *)ss;
    JP2_Error err;

    /* close the library compression context */
    err = JP2_Compress_End(state->handle);
    if (err != cJP2_Error_OK) {
        /* we can't return an error, so only print on debug builds */
        if_debug1('w', "[w]jpxe Luratech JP2 error %d"
                " closing compression context", (int)err);
    }

    /* free our own storage */
    free(state->outbuf);
    free(state->inbuf);
}

/* encoder stream template */
const stream_template s_jpxe_template = {
    &st_jpxe_state,
    s_jpxe_init,
    s_jpxe_process,
    1024, 1024, /* min in and out buffer sizes */
    s_jpxe_release,
    s_jpxe_set_defaults
};
