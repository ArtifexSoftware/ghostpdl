/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/* Utilities for PCL XL generation */
/* Requires gdevpxat.h, gdevpxen.h, gdevpxop.h */

#ifndef gdevpxut_INCLUDED
#  define gdevpxut_INCLUDED

#include "gsdevice.h"
#include "scommon.h"
#include "gdevpxen.h"
#include "gdevpxat.h"
#include "gdevpxop.h"
#include "gxfixed.h"

/* ---------------- High-level constructs ---------------- */

/* Write the file header, including the resolution. */
int px_write_file_header(stream *s, const gx_device *dev, bool staple);

/* Write the page header, including orientation. */
int px_write_page_header(stream *s, const gx_device *dev);

/* Write the media selection command if needed, updating the media size. */
int px_write_select_media(stream *s, const gx_device *dev,
                          pxeMediaSize_t *pms,
                          byte *media_source,
                          int page, bool Duplex, bool Tumble,
                          int media_type_set, char *media_type);

/*
 * Write the file trailer.  Note that this takes a gp_file *, not a stream *,
 * since it may be called after the stream is closed.
 */
int px_write_file_trailer(gp_file *file);

/* ---------------- Low-level data output ---------------- */

/* Write a sequence of bytes. */
#define PX_PUT_LIT(s, bytes) px_put_bytes(s, bytes, sizeof(bytes))
void px_put_bytes(stream * s, const byte * data, uint count);

/* Utilities for writing data values. */
/* H-P printers only support little-endian data, so that's what we emit. */

#define DA(a) pxt_attr_ubyte, (a)
void px_put_a(stream * s, px_attribute_t a);
void px_put_ac(stream *s, px_attribute_t a, px_tag_t op);

#define DUB(b) pxt_ubyte, (byte)(b)
void px_put_ub(stream * s, byte b);
void px_put_uba(stream *s, byte b, px_attribute_t a);

/* signed and unsigned shorts */
#define DS(i) (byte)(i), (byte)(((i) >= 0 ? (i) : ((i)|0x8000)) >> 8)
#define US(i) (byte)(i), (byte)((i) >> 8)
void px_put_s(stream * s, int i);

#define DUS(i) pxt_uint16, US(i)
void px_put_us(stream * s, uint i);
void px_put_usa(stream *s, uint i, px_attribute_t a);
void px_put_u(stream * s, uint i);

#define DUSP(ix,iy) pxt_uint16_xy, US(ix), US(iy)
void px_put_usp(stream * s, uint ix, uint iy);
void px_put_usq_fixed(stream * s, fixed x0, fixed y0, fixed x1, fixed y1);

void px_put_ss(stream * s, int i);
void px_put_ssp(stream * s, int ix, int iy);

void px_put_l(stream * s, ulong l);

void px_put_r(stream * s, double r);  /* no tag */
void px_put_rl(stream * s, double r);  /* pxt_real32 tag */
void px_put_rp(stream * s, double rx, double ry);
void px_put_rpa(stream * s, double rx, double ry, px_attribute_t a);

void px_put_ubaa(stream * s, const byte * data, uint count, px_attribute_t a);

void px_put_data_length(stream * s, uint num_bytes);

#endif /* gdevpxut_INCLUDED */
