/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.

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


/* Generic image support */
#include "gx.h"
#include "gscspace.h"
#include "gserrors.h"
#include "gsmatrix.h"
#include "gsutil.h"
#include "gxiparam.h"
#include "stream.h"

/* ---------------- Generic image support ---------------- */

/* Structure descriptors */
public_st_gs_image_common();
public_st_gs_data_image();
public_st_gs_pixel_image();

/* Initialize the common parts of image structures. */
void
gs_image_common_t_init(gs_image_common_t * pic)
{
    gs_make_identity(&pic->ImageMatrix);
}
void
gs_data_image_t_init(gs_data_image_t * pim, int num_components)
{
    int i;

    gs_image_common_t_init((gs_image_common_t *) pim);
    pim->Width = pim->Height = 0;
    pim->BitsPerComponent = 1;
    if (num_components >= 0) {
	for (i = 0; i < num_components * 2; i += 2)
	    pim->Decode[i] = 0, pim->Decode[i + 1] = 1;
    } else {
	for (i = 0; i < num_components * -2; i += 2)
	    pim->Decode[i] = 1, pim->Decode[i + 1] = 0;
    }
    pim->Interpolate = false;
}
void
gs_pixel_image_t_init(gs_pixel_image_t * pim,
		      const gs_color_space * color_space)
{
    int num_components;

    if (color_space == 0 ||
	(num_components =
	 gs_color_space_num_components(color_space)) < 0
	)
	num_components = 0;
    gs_data_image_t_init((gs_data_image_t *) pim, num_components);
    pim->format = gs_image_format_chunky;
    pim->ColorSpace = color_space;
    pim->CombineWithColor = false;
}

/* Initialize the common part of an image-processing enumerator. */
int
gx_image_enum_common_init(gx_image_enum_common_t * piec,
			  const gs_image_common_t * pic,
			  const gx_image_enum_procs_t * piep,
			  gx_device * dev, int bits_per_component,
			  int num_components, gs_image_format_t format)
{
    piec->image_type = pic->type;
    piec->procs = piep;
    piec->dev = dev;
    piec->id = gs_next_ids(1);
    switch (format) {
	case gs_image_format_chunky:
	    piec->num_planes = 1;
	    piec->plane_depths[0] = bits_per_component * num_components;
	    break;
	case gs_image_format_component_planar:
	    piec->num_planes = num_components;
	    {
		int i;

		for (i = 0; i < num_components; ++i)
		    piec->plane_depths[i] = bits_per_component;
	    }
	    break;
	case gs_image_format_bit_planar:
	    piec->num_planes = bits_per_component * num_components;
	    {
		int i;

		for (i = 0; i < piec->num_planes; ++i)
		    piec->plane_depths[i] = 1;
	    }
#if 0	/* **************** */
	    break;
#endif	/* **************** */
	default:
	    return_error(gs_error_rangecheck);
    }
    return 0;
}

/* Compute the source size of an ordinary image with explicit data. */
int
gx_data_image_source_size(const gs_imager_state * pis,
			  const gs_image_common_t * pim, gs_int_point * psize)
{
    const gs_data_image_t *pdi = (const gs_data_image_t *)pim;

    psize->x = pdi->Width;
    psize->y = pdi->Height;
    return 0;
}

/* Process the next piece of an image with no source data. */
/* This procedure should never be called. */
int
gx_no_image_plane_data(gx_device * dev,
		       gx_image_enum_common_t * info,
		       const gx_image_plane_t * planes, int height)
{
    return_error(gs_error_Fatal);
}

/* Clean up after processing an image with no source data. */
/* This procedure may be called, but should do nothing. */
int
gx_ignore_end_image(gx_device * dev, gx_image_enum_common_t * info,
		    bool draw_last)
{
    return 0;
}

/* ---------------- Serialization ---------------- */

/* Define the default Decode values. */
#define DECODE_DEFAULT(i)\
  ((i) == 1 ? decode_default_1 : (i) & 1)

/* Put a variable-length uint on a stream. */
void
sput_variable_uint(stream *s, uint w)
{
    for (; w > 0x7f; w >>= 7)
	sputc(s, (byte)(w | 0x80));
    sputc(s, w);
}

/*
 * Write generic pixel image parameters.  Note that this does not handle
 * all possible parameter values: currently it only handles a subset of
 * the possible color spaces.  The format is the following:
 *	ABBCDDEE
 *	    A = 0 if standard ImageMatrix, 1 if explicit ImageMatrix
 *	    BB = code for BitsPerComponent: 0=1, 1=2, 2=4, 3=8
 *	    C = 0 if 2nd byte defaulted, 1 if present
 *	    DD = format
 *	    EE = (base) color space:
 *		0=DeviceGray, 1=DeviceRGB, 2=DeviceCMYK, 3=DevicePixel
 *	FGHIJ000 (if C = 1)
 *	    F = 0 if BPC <= 8, 1 if BPC = 12
 *	    G = 0 if standard (0..1) Decode, 1 if explicit Decode
 *	    H = Interpolate
 *	    I = CombineWithColor
 *	    J = color space is Indexed ****** NOT SUPPORTED YET ******
 *	Width, 7 bits per byte, low-order bits first, 0x80 = more bytes
 *	Height, encoded like width
 *	ImageMatrix (if A = 1), per gs_matrix_store/fetch
 *	Decode (if G = 1): blocks of up to 4 components
 *	    aabbccdd, where each xx is decoded as:
 *		00 = default, 01 = swapped default,
 *		10 = (0,V), 11 = (U,V)
 *	    non-defaulted components (up to 8 floats)
 */
int
gx_pixel_image_write(const gs_image_common_t *pic, stream *s)
{
    const gs_pixel_image_t *pim = (const gs_pixel_image_t *)pic;
    const gs_color_space *pcs = pim->ColorSpace;
    int num_components = gs_color_space_num_components(pcs);
    int num_decode = num_components * 2;
    byte b1 = 0, b2 = 0;
    float decode_default_1 = 1;
    int i;

    /* Construct the (first) control byte. */

    if (!(is_xxyy(&pim->ImageMatrix) &&
	  pim->ImageMatrix.xx == pim->Width &&
	  pim->ImageMatrix.yy == -pim->Height &&
	  is_fzero(pim->ImageMatrix.tx) &&
	  pim->ImageMatrix.ty == pim->Height))
	b1 |= 0x80;
    switch (pim->BitsPerComponent) {
    case 1: break;
    case 2: b1 |= 0x20; break;
    case 4: b1 |= 0x40; break;
    case 8: b1 |= 0x60; break;
    case 12: b2 |= 0x80; break;
    default: return_error(gs_error_rangecheck);
    }
    b1 |= pim->format << 2;
    switch (gs_color_space_get_index(pcs)) {
    case gs_color_space_index_DeviceGray: break;
    case gs_color_space_index_DeviceRGB: b1 |= 1; break;
    case gs_color_space_index_DeviceCMYK: b1 |= 2; break;
    case gs_color_space_index_DevicePixel: b1 |= 3; break;
    default: return_error(gs_error_rangecheck);
    }

    /* Finish the second control byte. */

    for (i = 0; i < num_decode; ++i)
	if (pim->Decode[i] != DECODE_DEFAULT(i)) {
	    b2 |= 0x40;
	    break;
	}
    if (pim->Interpolate)
	b2 |= 0x20;
    if (pim->CombineWithColor)
	b2 |= 0x10;

    /* Write the encoding on the stream. */

    if (b2) {
	sputc(s, b1 | 0x10);
	sputc(s, b2);
    } else
	sputc(s, b1);
    sput_variable_uint(s, (uint)pim->Width);
    sput_variable_uint(s, (uint)pim->Height);
    if (b1 & 0x80)
	sput_matrix(s, &pim->ImageMatrix);
    if (b2 & 0x40) {
	int i;
	uint dflags = 1;
	float decode[8];
	int di = 0;
	uint ignore;

	for (i = 0; i < num_decode; i += 2) {
	    float u = pim->Decode[i], v = pim->Decode[i + 1];
	    float dv = DECODE_DEFAULT(i + 1);

	    if (dflags >= 0x100) {
		sputc(s, dflags & 0xff);
		sputs(s, (const byte *)decode, di * sizeof(float), &ignore);
		dflags = 1;
		di = 0;
	    }
	    dflags <<= 2;
	    if (u == 0 && v == dv)
		DO_NOTHING;
	    else if (u == dv && v == 0)
		dflags += 1;
	    else {
		if (u != 0) {
		    dflags++;
		    decode[di++] = u;
		}
		dflags += 2;
		decode[di++] = v;
	    }
	}
	sputc(s, (dflags << (8 - num_decode)) & 0xff);
	sputs(s, (const byte *)decode, di * sizeof(float), &ignore);
    }
    return 0;
}

/* Get a variable-length uint from a stream. */
int
sget_variable_uint(stream *s, uint *pw)
{
    uint w = 0;
    int shift = 0;
    int ch;

    for (; (ch = sgetc(s)) >= 0x80; shift += 7)
	w += (ch & 0x7f) << shift;
    if (ch < 0)
	return_error(gs_error_ioerror);
    *pw = w + (ch << shift);
    return 0;
}

/*
 * Read generic pixel image parameters.
 */
int
gx_pixel_image_read(gs_image_common_t **ppic, stream *s, gs_memory_t *mem)
{
    byte b1 = sgetc(s);
    byte b2 = (b1 & 0x10 ? sgetc(s) : 0);
    float decode_default_1 = 1;
    gs_color_space *pcs;
    int num_components, num_decode;
    int i;
    gs_pixel_image_t *pim;
    int code;

    /****** ALLOCATE pim ******/
    if ((code = sget_variable_uint(s, (uint *)&pim->Width)) < 0 ||
	(code = sget_variable_uint(s, (uint *)&pim->Height)) < 0
	)
	goto fail;
    if (b1 & 0x80) {
	if ((code = sget_matrix(s, &pim->ImageMatrix)) < 0)
	    goto fail;
    } else {
	pim->ImageMatrix.xx = pim->Width;
	pim->ImageMatrix.xy = 0;
	pim->ImageMatrix.yx = 0;
	pim->ImageMatrix.xx = -pim->Height;
	pim->ImageMatrix.tx = 0;
	pim->ImageMatrix.ty = pim->Height;
    }
    pim->BitsPerComponent =
	(b2 & 0x80 ? 12 : 1 << ((b1 >> 5) & 3));
    /****** SET pcs TO COLOR SPACE ******/
    pim->ColorSpace = pcs;
    num_components = gs_color_space_num_components(pcs);
    num_decode = num_components * 2;
    if (b2 & 0x40) {
	uint dflags = 0x10000;
	float *dp = pim->Decode;

	for (i = 0; i < num_decode; i += 2, dp += 2, dflags <<= 2) {
	    uint ignore;

	    if (dflags >= 0x10000) {
		dflags = sgetc(s) + 0x100;
		if (dflags < 0x100) {
		    code = gs_note_error(gs_error_ioerror);
		    goto fail;
		}
	    }
	    switch (dflags & 0xc0) {
	    case 0x00:
		dp[0] = 0, dp[1] = DECODE_DEFAULT(i + 1);
		break;
	    case 0x40:
		dp[0] = DECODE_DEFAULT(i + 1), dp[1] = 0;
		break;
	    case 0x80:
		dp[0] = 0;
		if (sgets(s, (byte *)(dp + 1), sizeof(float), &ignore) < 0) {
		    code = gs_note_error(gs_error_ioerror);
		    goto fail;
		}
		break;
	    case 0xc0:
		if (sgets(s, (byte *)dp, sizeof(float) * 2, &ignore) < 0) {
		    code = gs_note_error(gs_error_ioerror);
		    goto fail;
		}
		break;
	    }
	}
    } else {
        for (i = 0; i < num_decode; ++i)
	    pim->Decode[i] = DECODE_DEFAULT(i);
    }
    pim->Interpolate = (b2 & 0x20) != 0;
    pim->CombineWithColor = (b2 & 0x10) != 0;
    *ppic = (gs_image_common_t *)pim;
    return 0;
fail:
    gs_free_object(mem, pim, "gx_pixel_image_read");
    return code;
}
