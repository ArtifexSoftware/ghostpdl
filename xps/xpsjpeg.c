/* Copyright (C) 2001-2024 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* XPS interpreter - JPEG image support */

#include "ghostxps.h"

#include "stream.h"
#include "strimpl.h"
#include "gsstate.h"
#include "jpeglib_.h"
#include "sdct.h"
#include "sjpeg.h"

static int
xps_report_error(stream_state * st, const char *str)
{
    (void) gs_throw1(-1, "%s", str);
    return 0;
}

/* These four routines, extract_exif_resolution and extract_app13_resolution,
   float_can_be_int and read_value are borrowed from MuPDF, see load_jpeg.c in the MuPDF tree.
   float_can_be_int and read_value are utility functions used by the other two.
 */
/* Returns true if <x> can be represented as an integer without overflow.
 *
 * We can't use comparisons such as 'return x < INT_MAX' because INT_MAX is
 * not safely convertible to float - it ends up as INT_MAX+1 so the comparison
 * doesn't do what we want.
 *
 * Instead we do a round-trip conversion and return true if this differs by
 * less than 1. This relies on high adjacent float values that differ by more
 * than 1, actually being exact integers, so the round-trip doesn't change the
 * value.
 */
static int float_can_be_int(float x)
{
	return fabsf(x - (float)(int) x) < 1;
}
static inline int read_value(const unsigned char *data, int bytes, int is_big_endian)
{
	int value = 0;
	if (!is_big_endian)
		data += bytes;
	for (; bytes > 0; bytes--)
		value = (value << 8) | (is_big_endian ? *data++ : *--data);
	return value;
}

static int extract_exif_resolution(jpeg_saved_marker_ptr marker,
	int *xres, int *yres, uint8_t *orientation)
{
	int is_big_endian, orient;
	const unsigned char *data;
	unsigned int offset, ifd_len, res_type = 0;
	float x_res = 0, y_res = 0;

	if (!marker || marker->marker != JPEG_APP0 + 1 || marker->data_length < 14)
		return 0;
	data = (const unsigned char *)marker->data;
	if (read_value(data, 4, 1) != 0x45786966 /* Exif */ || read_value(data + 4, 2, 1) != 0x0000)
		return 0;
	if (read_value(data + 6, 4, 1) == 0x49492A00)
		is_big_endian = 0;
	else if (read_value(data + 6, 4, 1) == 0x4D4D002A)
		is_big_endian = 1;
	else
		return 0;

	offset = read_value(data + 10, 4, is_big_endian) + 6;
	if (offset < 14 || offset > marker->data_length - 2)
		return 0;
	ifd_len = read_value(data + offset, 2, is_big_endian);
	for (offset += 2; ifd_len > 0 && offset + 12 < marker->data_length; ifd_len--, offset += 12)
	{
		int tag = read_value(data + offset, 2, is_big_endian);
		int type = read_value(data + offset + 2, 2, is_big_endian);
		int count = read_value(data + offset + 4, 4, is_big_endian);
		unsigned int value_off = read_value(data + offset + 8, 4, is_big_endian) + 6;
		switch (tag)
		{
		case 0x112:
			if (type == 3 && count == 1) {
				orient = read_value(data + offset + 8, 2, is_big_endian);
			}
			break;
		case 0x11A:
			if (type == 5 && value_off > offset && value_off <= marker->data_length - 8)
				x_res = 1.0f * read_value(data + value_off, 4, is_big_endian) / read_value(data + value_off + 4, 4, is_big_endian);
			break;
		case 0x11B:
			if (type == 5 && value_off > offset && value_off <= marker->data_length - 8)
				y_res = 1.0f * read_value(data + value_off, 4, is_big_endian) / read_value(data + value_off + 4, 4, is_big_endian);
			break;
		case 0x128:
			if (type == 3 && count == 1)
				res_type = read_value(data + offset + 8, 2, is_big_endian);
			break;
		}
	}

	if (x_res <= 0 || !float_can_be_int(x_res) || y_res <= 0 || !float_can_be_int(y_res))
		return 0;
	if (res_type == 2)
	{
		*xres = (int)x_res;
		*yres = (int)y_res;
	}
	else if (res_type == 3)
	{
		*xres = (int)(x_res * 254 / 100);
		*yres = (int)(y_res * 254 / 100);
	}
	else
	{
		*xres = 0;
		*yres = 0;
	}
	return 1;
}

static int extract_app13_resolution(jpeg_saved_marker_ptr marker, int *xres, int *yres)
{
	const unsigned char *data, *data_end;

	if (!marker || marker->marker != JPEG_APP0 + 13 || marker->data_length < 42 ||
		strcmp((const char *)marker->data, "Photoshop 3.0") != 0)
	{
		return 0;
	}

	data = (const unsigned char *)marker->data;
	data_end = data + marker->data_length;
	for (data += 14; data + 12 < data_end; ) {
		int data_size = -1;
		int tag = read_value(data + 4, 2, 1);
		int value_off = 11 + read_value(data + 6, 2, 1);
		if (value_off % 2 == 1)
			value_off++;
		if (read_value(data, 4, 1) == 0x3842494D /* 8BIM */ && value_off <= data_end - data)
			data_size = read_value(data + value_off - 4, 4, 1);
		if (data_size < 0 || data_size > data_end - data - value_off)
			return 0;
		if (tag == 0x3ED && data_size == 16)
		{
			*xres = read_value(data + value_off, 2, 1);
			*yres = read_value(data + value_off + 8, 2, 1);
			return 1;
		}
		if (data_size % 2 == 1)
			data_size++;
		data += value_off + data_size;
	}

	return 0;
}

int
xps_decode_jpeg(xps_context_t *ctx, byte *rbuf, int rlen, xps_image_t *image)
{
    jpeg_decompress_data jddp;
    stream_DCT_state state;
    stream_cursor_read rp;
    stream_cursor_write wp;
    int code;
    int wlen;
    byte *wbuf;
    jpeg_saved_marker_ptr curr_marker;

    s_init_state((stream_state*)&state, &s_DCTD_template, ctx->memory);
    state.report_error = xps_report_error;

    s_DCTD_template.set_defaults((stream_state*)&state);

    state.jpeg_memory = ctx->memory;
    state.data.decompress = &jddp;

    jddp.templat = s_DCTD_template;
    jddp.memory = ctx->memory;
    jddp.scanline_buffer = NULL;
    jddp.PassThrough = 0;
    jddp.device = (void *)NULL;
    jddp.PassThroughfn = 0;

    if ((code = gs_jpeg_create_decompress(&state)) < 0)
        return gs_throw(-1, "cannot gs_jpeg_create_decompress");

    s_DCTD_template.init((stream_state*)&state);

    rp.ptr = rbuf - 1;
    rp.limit = rbuf + rlen - 1;

    /* read the header only by not having a write buffer */
    wp.ptr = 0;
    wp.limit = 0;

    /* Set up to save the ICC marker APP2.
     * According to the spec we should be getting APP1 APP2 and APP13.
     * Library gets APP0 and APP14. */
	jpeg_save_markers(&(jddp.dinfo), JPEG_APP0+1, 0xffff);
	jpeg_save_markers(&(jddp.dinfo), JPEG_APP0+13, 0xffff);
	jpeg_save_markers(&(jddp.dinfo), JPEG_APP0+2, 0xffff);

    code = s_DCTD_template.process((stream_state*)&state, &rp, &wp, true);
    if (code != 1) {
        code = gs_throw(-1, "premature EOF or error in jpeg");
        goto error;
    }

    /* Check if we had an ICC profile */
    curr_marker = jddp.dinfo.marker_list;
    while (curr_marker != NULL)
    {
        if (curr_marker->marker == 0xe2)
        {
            /* Found ICC profile. Create a buffer and copy over now.
             * Strip JPEG APP2 14 byte header */
            image->profilesize = curr_marker->data_length - 14;
            image->profile = xps_alloc(ctx, image->profilesize);
            if (image->profile)
            {
                /* If we can't create it, just ignore */
                memcpy(image->profile, &(curr_marker->data[14]), image->profilesize);
            }
            break;
        }
        curr_marker = curr_marker->next;
    }

    image->width = jddp.dinfo.output_width;
    image->height = jddp.dinfo.output_height;
    image->comps = jddp.dinfo.output_components;
    image->bits = 8;
    image->stride = image->width * image->comps;
    image->invert_decode = false;

    if (image->comps == 1) {
        rc_increment(ctx->gray);
        image->colorspace = ctx->gray;
    }
    if (image->comps == 3) {
        rc_increment(ctx->srgb);
        image->colorspace = ctx->srgb;
    }
    if (image->comps == 4) {
        rc_increment(ctx->cmyk);
        image->colorspace = ctx->cmyk;
        image->invert_decode = true;
    }

	if (extract_exif_resolution(jddp.dinfo.marker_list, &image->xres, &image->yres, NULL))
		/* XPS prefers EXIF resolution to JFIF density */;
	else if (extract_app13_resolution(jddp.dinfo.marker_list, &image->xres, &image->yres))
		/* XPS prefers APP13 resolution to JFIF density */;
	else if (jddp.dinfo.density_unit == 1)
    {
        /* According to the XPS specification we should also use the EXIF and APP13 marker segments
         * to set the resolution, but we don't and adding that would be more effort than we want to
         * go to for XPS currently. This at least means that images won't go completely AWOL.
         */
        if (jddp.dinfo.X_density != 0)
            image->xres = jddp.dinfo.X_density;
        else
            image->xres = 96;
        if (jddp.dinfo.Y_density != 0)
            image->yres = jddp.dinfo.Y_density;
        else
            image->yres = 96;
    }
    else if (jddp.dinfo.density_unit == 2)
    {
        image->xres = (int)(jddp.dinfo.X_density * 2.54 + 0.5);
        image->yres = (int)(jddp.dinfo.Y_density * 2.54 + 0.5);
    }
    else
    {
        image->xres = 96;
        image->yres = 96;
    }

    wlen = image->stride * image->height;
    wbuf = xps_alloc(ctx, wlen);
    if (!wbuf) {
        code = gs_throw1(gs_error_VMerror, "out of memory allocating samples: %d", wlen);
        goto error;
    }

    image->samples = wbuf;

    wp.ptr = wbuf - 1;
    wp.limit = wbuf + wlen - 1;

    code = s_DCTD_template.process((stream_state*)&state, &rp, &wp, true);
    if (code != EOFC) {
        code = gs_throw1(-1, "error in jpeg (code = %d)", code);
        goto error;
    }

    code = gs_okay;
error:
    gs_jpeg_destroy(&state);
    if (jddp.scanline_buffer != NULL) {
        gs_free_object(gs_memory_stable(ctx->memory),
                       jddp.scanline_buffer,
                       "xps_decode_jpeg");
    }

    return code;
}
