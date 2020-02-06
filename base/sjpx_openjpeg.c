/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/



/* opj filter implementation using OpenJPeg library */

#include "memory_.h"
#include "gserrors.h"
#include "gdebug.h"
#include "strimpl.h"
#include "sjpx_openjpeg.h"
#include "gxsync.h"
#include "assert_.h"
#if !defined(SHARE_JPX) || (SHARE_JPX == 0)
#include "opj_malloc.h"
#endif
/* Some locking to get around the criminal lack of context
 * in the openjpeg library. */
#if !defined(SHARE_JPX) || (SHARE_JPX == 0)
static gs_memory_t *opj_memory;
#endif

int sjpxd_create(gs_memory_t *mem)
{
#if !defined(SHARE_JPX) || (SHARE_JPX == 0)
    gs_lib_ctx_t *ctx = mem->gs_lib_ctx;

    ctx->sjpxd_private = gx_monitor_label(gx_monitor_alloc(mem), "sjpxd_monitor");
    if (ctx->sjpxd_private == NULL)
        return gs_error_VMerror;
#endif
    return 0;
}

void sjpxd_destroy(gs_memory_t *mem)
{
#if !defined(SHARE_JPX) || (SHARE_JPX == 0)
    gs_lib_ctx_t *ctx = mem->gs_lib_ctx;

    gx_monitor_free((gx_monitor_t *)ctx->sjpxd_private);
    ctx->sjpxd_private = NULL;
#endif
}

static int opj_lock(gs_memory_t *mem)
{
#if !defined(SHARE_JPX) || (SHARE_JPX == 0)
    int ret;

    gs_lib_ctx_t *ctx = mem->gs_lib_ctx;

    ret = gx_monitor_enter((gx_monitor_t *)ctx->sjpxd_private);
    assert(opj_memory == NULL);
    opj_memory = mem->non_gc_memory;
    return ret;
#else
    return 0;
#endif
}

static int opj_unlock(gs_memory_t *mem)
{
#if !defined(SHARE_JPX) || (SHARE_JPX == 0)
    gs_lib_ctx_t *ctx = mem->gs_lib_ctx;

    assert(opj_memory != NULL);
    opj_memory = NULL;
    return gx_monitor_leave((gx_monitor_t *)ctx->sjpxd_private);
#else
    return 0;
#endif
}

#if !defined(SHARE_JPX) || (SHARE_JPX == 0)
/* Allocation routines that use the memory pointer given above */
void *opj_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    assert(opj_memory != NULL);

    if (size > (size_t) ARCH_MAX_UINT)
	    return NULL;

    return (void *)gs_alloc_bytes(opj_memory, size, "opj_malloc");
}

void *opj_calloc(size_t n, size_t size)
{
    void *ptr;

    /* FIXME: Check for overflow? */
    size *= n;

    ptr = opj_malloc(size);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}

void *opj_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return opj_malloc(size);

    if (size == 0)
    {
        opj_free(ptr);
        return NULL;
    }

    return gs_resize_object(opj_memory, ptr, size, "opj_malloc");
}

void opj_free(void *ptr)
{
    gs_free_object(opj_memory, ptr, "opj_malloc");
}

static inline void * opj_aligned_malloc_n(size_t size, size_t align)
{
    uint8_t *ptr;
    int off;

    if (size == 0)
        return NULL;

    size += align + sizeof(uint8_t);
    ptr = opj_malloc(size);
    if (ptr == NULL)
        return NULL;
    off = align - (((int)(intptr_t)ptr) & (align - 1));
    ptr[off-1] = off;
    return ptr + off;
}

void * opj_aligned_malloc(size_t size)
{
    return opj_aligned_malloc_n(size, 16);
}

void *opj_aligned_32_malloc(size_t size)
{
    return opj_aligned_malloc_n(size, 32);
}

void opj_aligned_free(void* ptr_)
{
    uint8_t *ptr = (uint8_t *)ptr_;
    uint8_t off;
    if (ptr == NULL)
        return;

    off = ptr[-1];
    opj_free((void *)(((unsigned char *)ptr) - off));
}

#if 0
/* UNUSED currently, and moderately tricky, so deferred until required */
void * opj_aligned_realloc(void *ptr, size_t size)
{
	return opj_realloc(ptr, size);
}
#endif
#endif

gs_private_st_simple(st_jpxd_state, stream_jpxd_state,
    "JPXDecode filter state"); /* creates a gc object for our state,
                            defined in sjpx.h */

static int s_opjd_accumulate_input(stream_jpxd_state *state, stream_cursor_read * pr);

static OPJ_SIZE_T sjpx_stream_read(void * p_buffer, OPJ_SIZE_T p_nb_bytes, void * p_user_data)
{
	stream_block *sb = (stream_block *)p_user_data;
	OPJ_SIZE_T len;

	len = sb->fill - sb->pos;
        if (sb->fill < sb->pos)
		len = 0;
	if (len == 0)
		return (OPJ_SIZE_T)-1;  /* End of file! */
	if ((OPJ_SIZE_T)len > p_nb_bytes)
		len = p_nb_bytes;
	memcpy(p_buffer, sb->data + sb->pos, len);
	sb->pos += len;
	return len;
}

static OPJ_OFF_T sjpx_stream_skip(OPJ_OFF_T skip, void * p_user_data)
{
	stream_block *sb = (stream_block *)p_user_data;

	if (skip > sb->fill - sb->pos)
		skip = sb->fill - sb->pos;
	sb->pos += skip;
	return sb->pos;
}

static OPJ_BOOL sjpx_stream_seek(OPJ_OFF_T seek_pos, void * p_user_data)
{
	stream_block *sb = (stream_block *)p_user_data;

	if (seek_pos > sb->fill)
		return OPJ_FALSE;
	sb->pos = seek_pos;
	return OPJ_TRUE;
}

static void sjpx_error_callback(const char *msg, void *ptr)
{
	dlprintf1("openjpeg error: %s", msg);
}

static void sjpx_info_callback(const char *msg, void *ptr)
{
#ifdef DEBUG
	/* prevent too many messages during normal build */
	dlprintf1("openjpeg info: %s", msg);
#endif
}

static void sjpx_warning_callback(const char *msg, void *ptr)
{
	dlprintf1("openjpeg warning: %s", msg);
}

/* initialize the stream */
static int
s_opjd_init(stream_state * ss)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) ss;
    state->codec = NULL;

    state->image = NULL;
    state->sb.data= NULL;
    state->sb.size = 0;
    state->sb.pos = 0;
    state->sb.fill = 0;
    state->out_offset = 0;
    state->pdata = NULL;
    state->sign_comps = NULL;
    state->stream = NULL;
    state->row_data = NULL;

    return 0;
}

/* setting the codec format,
   allocating the stream and image structures, and
   initializing the decoder.
 */
static int
s_opjd_set_codec_format(stream_state * ss, OPJ_CODEC_FORMAT format)
{
    stream_jpxd_state *const state = (stream_jpxd_state *) ss;
    opj_dparameters_t parameters;	/* decompression parameters */

    /* set decoding parameters to default values */
    opj_set_default_decoder_parameters(&parameters);

    /* get a decoder handle */
    state->codec = opj_create_decompress(format);
    if (state->codec == NULL)
        return_error(gs_error_VMerror);

    /* catch events using our callbacks */
    opj_set_error_handler(state->codec, sjpx_error_callback, stderr);
    opj_set_info_handler(state->codec, sjpx_info_callback, stderr);
    opj_set_warning_handler(state->codec, sjpx_warning_callback, stderr);

    if (state->colorspace == gs_jpx_cs_indexed) {
        parameters.flags |= OPJ_DPARAMETERS_IGNORE_PCLR_CMAP_CDEF_FLAG;
    }

    /* setup the decoder decoding parameters using user parameters */
    if (!opj_setup_decoder(state->codec, &parameters))
    {
        dlprintf("openjpeg: failed to setup the decoder!\n");
        return ERRC;
    }

    /* open a byte stream */
    state->stream = opj_stream_default_create(OPJ_TRUE);
    if (state->stream == NULL)
    {
        dlprintf("openjpeg: failed to open a byte stream!\n");
        return ERRC;
    }

    opj_stream_set_read_function(state->stream, sjpx_stream_read);
    opj_stream_set_skip_function(state->stream, sjpx_stream_skip);
    opj_stream_set_seek_function(state->stream, sjpx_stream_seek);

    return 0;
}

static void
ycc_to_rgb_8(unsigned char *row, unsigned long row_size)
{
    unsigned char y;
    signed char u, v;
    int r,g,b;
    do
    {
        y = row[0];
        u = row[1] - 128;
        v = row[2] - 128;
        r = (int)((double)y + 1.402 * v);
        if (r < 0)
            r = 0;
        if (r > 255)
            r = 255;
        g = (int)((double)y - 0.34413 * u - 0.71414 * v);
        if (g < 0)
            g = 0;
        if (g > 255)
            g = 255;
        b = (int)((double)y + 1.772 * u);
        if (b < 0)
            b = 0;
        if (b > 255)
            b = 255;
        row[0] = r;
        row[1] = g;
        row[2] = b;
        row += 3;
        row_size -= 3;
    }
    while (row_size);
}

static void
ycc_to_rgb_16(unsigned char *row, unsigned long row_size)
{
    unsigned short y;
    signed short u, v;
    int r,g,b;
    do
    {
        y = (row[0]<<8) | row[1];
        u = ((row[2]<<8) | row[3]) - 32768;
        v = ((row[4]<<8) | row[5]) - 32768;
        r = (int)((double)y + 1.402 * v);
        if (r < 0)
            r = 0;
        if (r > 65535)
            r = 65535;
        g = (int)((double)y - 0.34413 * u - 0.71414 * v);
        if (g < 0)
            g = 0;
        if (g > 65535)
            g = 65535;
        b = (int)((double)y + 1.772 * u);
        if (b < 0)
            b = 0;
        if (b > 65535)
            b = 65535;
        row[0] = r>>8;
        row[1] = r;
        row[2] = g>>8;
        row[3] = g;
        row[4] = b>>8;
        row[5] = b;
        row += 6;
        row_size -= 6;
    }
    while (row_size);
}

static int decode_image(stream_jpxd_state * const state)
{
    int numprimcomp = 0, alpha_comp = -1, compno, rowbytes;

    /* read header */
    if (!opj_read_header(state->stream, state->codec, &(state->image)))
    {
    	dlprintf("openjpeg: failed to read header\n");
    	return ERRC;
    }

    /* decode the stream and fill the image structure */
    if (!opj_decode(state->codec, state->stream, state->image))
    {
        dlprintf("openjpeg: failed to decode image!\n");
        return ERRC;
    }

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
            return ERRC; /* Not supported. */
        if (state->width < state->image->comps[compno].w)
            state->width = state->image->comps[compno].w;
        if (state->height < state->image->comps[compno].h)
            state->height = state->image->comps[compno].h;
        if (state->image->comps[compno].dx != state->image->comps[0].dx ||
                state->image->comps[compno].dy != state->image->comps[0].dy)
            state->samescale = false;
    }

    /* find alpha component and regular colour component by channel definition */
    for (compno = 0; compno < state->image->numcomps; compno++)
    {
        if (state->image->comps[compno].alpha == 0x00)
            numprimcomp++;
        else if (state->image->comps[compno].alpha == 0x01 || state->image->comps[compno].alpha == 0x02)
            alpha_comp = compno;
    }

    /* color space and number of components */
    switch(state->image->color_space)
    {
        case OPJ_CLRSPC_GRAY:
            state->colorspace = gs_jpx_cs_gray;
            break;
        case OPJ_CLRSPC_UNKNOWN: /* make the best guess based on number of channels */
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
        default: /* OPJ_CLRSPC_SRGB, OPJ_CLRSPC_SYCC, OPJ_CLRSPC_EYCC */
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

    state->pdata = (int **)gs_alloc_byte_array(state->memory->non_gc_memory, sizeof(int*)*state->image->numcomps, 1, "decode_image(pdata)");
    if (!state->pdata)
        return_error(gs_error_VMerror);

    /* compensate for signed data (signed => unsigned) */
    state->sign_comps = (int *)gs_alloc_byte_array(state->memory->non_gc_memory, sizeof(int)*state->image->numcomps, 1, "decode_image(sign_comps)");
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
    int shift_bit = state->bpp-state->image->comps[0].prec; /*difference between input and output bit-depth*/
    int img_numcomps = min(state->out_numcomps, state->image->numcomps); /* the actual number of channel data used */
    int compno;
    unsigned long i;
    int b;
    byte *row;
    unsigned int x_offset;
    unsigned int y_offset;
    unsigned int row_size = (state->width * state->out_numcomps * state->bpp + 7)>>3;

    /* If nothing to write, nothing to do */
    if (write_size == 0)
        return 0;

    if (state->row_data == NULL)
    {
        state->row_data = gs_alloc_byte_array(state->memory->non_gc_memory, row_size, 1, "jpxd_openjpeg(row_data)");
        if (state->row_data == NULL)
            return gs_error_VMerror;
    }

    while (state->out_offset != state->totalbytes)
    {
        y_offset = state->out_offset / row_size;
        x_offset = state->out_offset % row_size;

        if (x_offset == 0)
        {
            /* Decode another rows worth */
            row = state->row_data;
            if (state->alpha && state->alpha_comp == -1)
            {
                /* return 0xff for all */
                memset(row, 0xff, row_size);
            }
            else if (state->samescale)
            {
                if (state->alpha)
                    state->pdata[0] = &(state->image->comps[state->alpha_comp].data[y_offset * state->width]);
                else
                {
                    for (compno=0; compno<img_numcomps; compno++)
                        state->pdata[compno] = &(state->image->comps[compno].data[y_offset * state->width]);
                }
                if (shift_bit == 0 && state->bpp == 8) /* optimized for the most common case */
                {
                    for (i = state->width; i > 0; i--)
                        for (compno=0; compno<img_numcomps; compno++)
                            *row++ = *(state->pdata[compno]++) + state->sign_comps[compno]; /* copy input buffer to output */
                }
                else if ((state->bpp%8)==0)
                {
                    for (i = state->width; i > 0; i--)
                    {
                        for (compno=0; compno<img_numcomps; compno++)
                        {
                            for (b=0; b<bytepp1; b++)
                                *row++ = (((*(state->pdata[compno]) << shift_bit) >> (8*(bytepp1-b-1))))
                                                        + (b==0 ? state->sign_comps[compno] : 0); /* split and shift input int to output bytes */
                            state->pdata[compno]++; 
                        }
                    }
                }
                else
                {   
                    /* shift_bit = 0, bpp < 8 */
                    int bt=0;
                    int bit_pos = 0;
                    for (i = state->width; i > 0; i--)
                    {
                        for (compno=0; compno<img_numcomps; compno++)
                        {
                            bt <<= state->bpp;
                            bt += *(state->pdata[compno]++) + state->sign_comps[compno];
                            bit_pos += state->bpp;
                            if (bit_pos >= 8)
                            {
                                *row++ = bt >> (bit_pos-8);
                                bit_pos -= 8;
                                bt &= (1<<bit_pos)-1;
                            }
                        }
                    }
                    if (bit_pos != 0)
                    {
                        /* row padding */
                        *row++ = bt << (8 - bit_pos);
                        bit_pos = 0;
                        bt = 0;
                    }
                }
            }
            else if ((state->bpp%8)==0)
            {
                /* sampling required */
                if (state->alpha)
                {
                    for (i = 0; i < state->width; i++)
                    {
                        int in_offset_scaled = (y_offset/state->image->comps[state->alpha_comp].dy*state->width + i)/state->image->comps[state->alpha_comp].dx;
                        for (b=0; b<bytepp1; b++)
                            *row++ = (((state->image->comps[state->alpha_comp].data[in_offset_scaled] << shift_bit) >> (8*(bytepp1-b-1))))
                                                                     + (b==0 ? state->sign_comps[state->alpha_comp] : 0);
                    }
                }
                else
                {
                    for (i = 0; i < state->width; i++)
                    {
                        for (compno=0; compno<img_numcomps; compno++)
                        {
                            int in_offset_scaled = (y_offset/state->image->comps[compno].dy*state->width + i)/state->image->comps[compno].dx;
                            for (b=0; b<bytepp1; b++)
                                *row++ = (((state->image->comps[compno].data[in_offset_scaled] << shift_bit) >> (8*(bytepp1-b-1))))
                                                                                + (b==0 ? state->sign_comps[compno] : 0);
                        }
                    }
                }
            }
            else
            {
                int compno = state->alpha ? state->alpha_comp : 0;
                int bt=0;
                int ppbyte1 = 8/state->bpp;
                /* sampling required */
                /* only grayscale can have such bit-depth, also shift_bit = 0, bpp < 8 */
                for (i = 0; i < state->width; i++)
                {
                    for (b=0; b<ppbyte1; b++)
                    {
                        int in_offset_scaled = (y_offset/state->image->comps[compno].dy*state->width + i)/state->image->comps[compno].dx;
                        bt = bt<<state->bpp;
                        bt += state->image->comps[compno].data[in_offset_scaled] + state->sign_comps[compno];
                    }
                    *row++ = bt;
                }
            }

            if (state->image->color_space == OPJ_CLRSPC_SYCC || state->image->color_space == OPJ_CLRSPC_EYCC)
            {
                /* bpp >= 8 always, as bpp < 8 only for grayscale */
                if (state->bpp == 8)
                    ycc_to_rgb_8(state->row_data, row_size);
                else
                    ycc_to_rgb_16(state->row_data, row_size);
            }
        }

        pw->ptr++;
        i = (write_size > (unsigned long)(row_size - x_offset)) ? (row_size - x_offset) : (unsigned int)write_size;
        memcpy(pw->ptr, &state->row_data[x_offset], i);
        pw->ptr += i;
        pw->ptr--;
        state->out_offset += i;
        write_size -= i;
        if (write_size == 0)
            break;
    }

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
    int locked = 0;
    int code;

    if (in_size > 0) 
    {
        /* buffer available data */
        code = opj_lock(ss->memory);
        if (code < 0) return code;
        locked = 1;

        code = s_opjd_accumulate_input(state, pr);
        if (code < 0) return code;

        if (state->codec == NULL) {
            /* state->sb.size is non-zero after successful
               accumulate_input(); 1 is probably extremely rare */
            if (state->sb.data[0] == 0xFF && ((state->sb.size == 1) || (state->sb.data[1] == 0x4F)))
                code = s_opjd_set_codec_format(ss, OPJ_CODEC_J2K);
            else
                code = s_opjd_set_codec_format(ss, OPJ_CODEC_JP2);
            if (code < 0)
            {
                (void)opj_unlock(ss->memory);
                return code;
            }
        }
    }

    if (last == 1) 
    {
        if (state->image == NULL)
        {
            int ret;

            if (locked == 0)
            {
                ret = opj_lock(ss->memory);
                if (ret < 0) return ret;
                locked = 1;
            }

#if OPJ_VERSION_MAJOR >= 2 && OPJ_VERSION_MINOR >= 1
            opj_stream_set_user_data(state->stream, &(state->sb), NULL);
#else
            opj_stream_set_user_data(state->stream, &(state->sb));
#endif
            opj_stream_set_user_data_length(state->stream, state->sb.size);
            ret = decode_image(state);
            if (ret != 0)
            {
                (void)opj_unlock(ss->memory);
                return ret;
            }
        }

        if (locked)
        {
            code = opj_unlock(ss->memory);
            if (code < 0) return code;
        }

        /* copy out available data */
        return process_one_trunk(state, pw);

    }

    if (locked)
        return opj_unlock(ss->memory);

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

    /* empty stream or failed to accumulate */
    if (state->codec == NULL)
        return;

    (void)opj_lock(ss->memory);

    /* free image data structure */
    if (state->image)
        opj_image_destroy(state->image);
    
    /* free stream */
    if (state->stream)
        opj_stream_destroy(state->stream);
		
    /* free decoder handle */
    if (state->codec)
	opj_destroy_codec(state->codec);

    (void)opj_unlock(ss->memory);

    /* free input buffer */
    if (state->sb.data)
        gs_free_object(state->memory->non_gc_memory, state->sb.data, "s_opjd_release(sb.data)");

    if (state->pdata)
        gs_free_object(state->memory->non_gc_memory, state->pdata, "s_opjd_release(pdata)");

    if (state->sign_comps)
        gs_free_object(state->memory->non_gc_memory, state->sign_comps, "s_opjd_release(sign_comps)");

    if (state->row_data)
        gs_free_object(state->memory->non_gc_memory, state->row_data, "s_opjd_release(row_data)");
}


static int
s_opjd_accumulate_input(stream_jpxd_state *state, stream_cursor_read * pr)
{
    long in_size = pr->limit - pr->ptr;

    /* grow the input buffer if needed */
    if (state->sb.size < state->sb.fill + in_size)
    {
        unsigned char *new_buf;
        unsigned long new_size = state->sb.size==0 ? in_size : state->sb.size;

        while (new_size < state->sb.fill + in_size)
            new_size = new_size << 1;

        if_debug1('s', "[s]opj growing input buffer to %lu bytes\n",
                new_size);
        if (state->sb.data == NULL)
            new_buf = (byte *) gs_alloc_byte_array(state->memory->non_gc_memory, new_size, 1, "s_opjd_accumulate_input(alloc)");
        else
            new_buf = (byte *) gs_resize_object(state->memory->non_gc_memory, state->sb.data, new_size, "s_opjd_accumulate_input(resize)");
        if (new_buf == NULL) return_error( gs_error_VMerror);

        state->sb.data = new_buf;
        state->sb.size = new_size;
    }

    /* copy the available input into our buffer */
    /* note that the gs stream library uses offset-by-one
        indexing of its buffers while we use zero indexing */
    memcpy(state->sb.data + state->sb.fill, pr->ptr + 1, in_size);
    state->sb.fill += in_size;
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
