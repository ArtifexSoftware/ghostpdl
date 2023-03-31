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


/* Interface for bitmap operations */


#ifndef gsbitops_INCLUDED
#  define gsbitops_INCLUDED

#include "gxcindex.h"
#include "gstypes.h"

/* ---------------- Pixel processing macros ---------------- */

/*
 * These macros support code that processes data pixel-by-pixel (or, to be
 * more accurate, packed arrays of values -- they may be complete pixels
 * or individual components of pixels).
 *
 * Supported #s of bits per value (bpv) are 1, 2, 4, or n * 8, where n <= 8.
 * The suffix 8, 12, 16, 32, or 64 on a macro name indicates the maximum
 * value of bpv that the macro is prepared to handle.
 *
 * The setup macros number bits within a byte in big-endian order, i.e.,
 * 0x80 is bit 0, 0x01 is bit 7.  However, sbit/dbit may use a different
 * representation for better performance.  ****** NYI ******
 */

/* macro to eliminate compiler warning message */
#define SAMPLE_BOUND_SHIFT(value, shift)\
    ((shift) >= 8 * sizeof(value) ? (shift) & (8 * sizeof(value) - 1) : (shift))

/* Load a value from memory, without incrementing. */
static int inline sample_load_next8(uint *value, const byte **sptr, int *sbit, int sbpv)
{
    switch ( sbpv >> 2 ) {
        case 0:
            *value = (**sptr >> (8 - *sbit - sbpv)) & (sbpv | 1);
            break;
        case 1:
            *value = (**sptr >> (4 - *sbit)) & 0xf;
            break;
        case 2:
            *value = **sptr;
            break;
        default:
            return -1;
    }
    *sbit += sbpv;
    *sptr += *sbit >> 3;
    *sbit &= 7;
    return 0;
}
static int inline sample_load_next12(uint *value, const byte **sptr, int *sbit, int sbpv)
{
    switch ( sbpv >> 2 ) {
        case 0:
            *value = (**sptr >> (8 - *sbit - sbpv)) & (sbpv | 1);
            break;
        case 1:
            *value = (**sptr >> (4 - *sbit)) & 0xf;
            break;
        case 2:
            *value = **sptr;
            break;
        case 3:
            *value = (*sbit ? ((**sptr & 0xf) << 8) | (*sptr)[1] :
                    (**sptr << 4) | ((*sptr)[1] >> 4));
          break;
        default:
            return -1;
    }
    *sbit += sbpv;
    *sptr += *sbit >> 3;
    *sbit &= 7;
    return 0;
}
static int inline sample_load16(uint *value, const byte *sptr, int sbit, int sbpv)
{
    switch (sbpv >> 2 ) {
        case 0:
            *value = (*sptr >> (8 - sbit - sbpv)) & (sbpv | 1);
            break;
        case 1:
            *value = (*sptr >> (4 - sbit)) & 0xf;
            break;
        case 2:
            *value = *sptr;
            break;
        case 3:
            *value = (sbit ? ((*sptr & 0xf) << 8) | sptr[1] :
                    (*sptr << 4) | (sptr[1] >> 4));
            break;
        case 4:
            *value = (*sptr << 8) | sptr[1];
            break;
        default:
            return -1;
    }
    return 0;
}
static int inline sample_load_next16 (ushort *value, const byte **sptr, int *sbit, int sbpv)
{
    switch ( sbpv >> 2 ) {
        case 0:
            *value = (**sptr >> (8 - *sbit - sbpv)) & (sbpv | 1);
            break;
        case 1:
            *value = (**sptr >> (4 - *sbit)) & 0xf;
            break;
        case 2:
            *value = **sptr;
            break;
        case 3:
            *value = (*sbit ? ((**sptr & 0xf) << 8) | (*sptr)[1] :
                    (**sptr << 4) | ((*sptr)[1] >> 4));
            break;
        case 4:
            *value = (**sptr << 8) | (*sptr)[1];
            break;
        default:
            return -1;
    }
    *sbit += sbpv;
    *sptr += *sbit >> 3;
    *sbit &= 7;
    return 0;
}
static int inline sample_load_next32(uint32_t *value, const byte **sptr, int *sbit, int sbpv)
{
    switch ( sbpv >> 2 ) {
        case 0:
            *value = (**sptr >> (8 - *sbit - sbpv)) & (sbpv | 1);
            break;
        case 1:
            *value = (**sptr >> (4 - *sbit)) & 0xf;
            break;
        case 2:
            *value = **sptr;
            break;
        case 3:
            *value = (*sbit ? ((**sptr & 0xf) << 8) | (*sptr)[1] :
                      (**sptr << 4) | ((*sptr)[1] >> 4));
            break;
        case 4:
            *value = (**sptr << 8) | (*sptr)[1];
            break;
        case 6:
            *value = (**sptr << 16) | ((*sptr)[1] << 8) | (*sptr)[2];
            break;
        case 8:
            *value = (**sptr << 24) | ((*sptr)[1] << 16) | ((*sptr)[2] << 8) | (*sptr)[3];
            break;
        default:
            return -1;
    }
    *sbit += sbpv;
    *sptr += *sbit >> 3;
    *sbit &= 7;
    return -1;
}
static int inline sample_load_next64(uint64_t *value, const byte **sptr, int *sbit, int sbpv)
{
    switch ( sbpv >> 2 ) {
        case 0:
            *value = (**sptr >> (8 - *sbit - sbpv)) & (sbpv | 1);
            break;
        case 1:
            *value = (**sptr >> (4 - *sbit)) & 0xf;
            break;
        case 2:
            *value = **sptr;
            break;
        case 3:
            *value = (*sbit ? ((**sptr & 0xf) << 8) | (*sptr)[1] :
                      (**sptr << 4) | ((*sptr)[1] >> 4));
            break;
        case 4:
            *value = (**sptr << 8) | (*sptr)[1];
            break;
        case 6:
            *value = (**sptr << 16) | ((*sptr)[1] << 8) | (*sptr)[2];
            break;
        case 8:
            *value = ((uint64_t)(**sptr) << 24) | ((uint64_t)((*sptr)[1]) << 16) | (((uint64_t)(*sptr)[2]) << 8) | (uint64_t)((*sptr)[3]);
            break;
        case 10:
            *value = ((uint64_t)((*sptr)[0]) << SAMPLE_BOUND_SHIFT((*value), 32)) |
                    ((uint64_t)((*sptr)[1]) << 24) |
                    ((uint64_t)((*sptr)[2]) << 16) |
                    ((uint64_t)((*sptr)[3]) << 8) |
                    (uint64_t)((*sptr)[4]);
            break;
        case 12:
            *value = ((uint64_t)((*sptr)[0]) << SAMPLE_BOUND_SHIFT((*value), 40)) |
                    ((uint64_t)((*sptr)[1]) << SAMPLE_BOUND_SHIFT((*value), 32)) |
                    ((uint64_t)((*sptr)[2]) << 24) |
                    ((uint64_t)((*sptr)[3]) << 16) |
                    ((uint64_t)((*sptr)[4]) << 8) |
                    (uint64_t)((*sptr)[5]);
            break;
        case 14:
            *value = ((uint64_t)((*sptr)[0]) << SAMPLE_BOUND_SHIFT((*value), 48)) |
                    ((uint64_t)((*sptr)[1]) << SAMPLE_BOUND_SHIFT((*value), 40)) |
                    ((uint64_t)((*sptr)[2]) << SAMPLE_BOUND_SHIFT((*value), 32)) |
                    ((uint64_t)((*sptr)[3]) << 24) |
                    ((uint64_t)((*sptr)[4]) << 16) |
                    ((uint64_t)((*sptr)[5]) << 8) |
                    (uint64_t)((*sptr)[6]);
            break;
        case 16:
            *value = ((uint64_t)((*sptr)[0]) << SAMPLE_BOUND_SHIFT((*value), 56)) |
                    ((uint64_t)((*sptr)[1]) << SAMPLE_BOUND_SHIFT((*value), 48)) |
                    ((uint64_t)((*sptr)[2]) << SAMPLE_BOUND_SHIFT((*value), 40)) |
                    ((uint64_t)((*sptr)[3]) << SAMPLE_BOUND_SHIFT((*value), 32)) |
                    ((uint64_t)((*sptr)[4]) << 24) |
                    ((uint64_t)((*sptr)[5]) << 16) |
                    ((uint64_t)((*sptr)[6]) << 8) |
                    (uint64_t)((*sptr)[7]);
            break;
        default:
            return -1;
    }
    *sbit += sbpv;
    *sptr += *sbit >> 3;
    *sbit &= 7;
    return 0;
}

/* Store a value and increment the pointer. */
static int inline sample_store_next8(uint value, byte **dptr, int *dbit, int dbpv, byte *dbbyte)
{
    switch (dbpv >> 2 ) {
        case 0:
            if ( (*dbit += dbpv) == 8 ) {
               *(*dptr)++ = *dbbyte | (byte)value;
               *dbbyte = 0;
               *dbit = 0;
            }
            else
                *dbbyte |= (byte)(value << (8 - *dbit));
            break;
        case 1:
            if ( *dbit ^= 4 )
                *dbbyte = (byte)(value << 4);
            else
                *(*dptr)++ = *dbbyte | ((byte)value);
            break;
        case 2:
            *(*dptr)++ = (byte)value;
            break;
        default:
            return -1;
    }
    return 0;
}

static void inline sample_store_next_12 (uint value, byte **dptr, int *dbit, byte *dbbyte)
{
    if ( *dbit ^= 4 )
        *(*dptr)++ = (byte)(value >> 4), *dbbyte = (byte)(value << 4);
    else
      *(*dptr) = *dbbyte | (byte)(value >> 8), (*dptr)[1] = (byte)(value), *dptr += 2;
}
static int inline sample_store_next12(uint value, byte **dptr, int *dbit, int dbpv, byte *dbbyte)
{
    switch (dbpv >> 2 ) {
    case 0:
        if ((*dbit += dbpv) == 8 )
            *(*dptr)++ = *dbbyte | (byte)value, *dbbyte = 0, *dbit = 0;
        else
            *dbbyte |= (byte)(value << (8 - *dbit));
        break;
    case 1:
        if ( *dbit ^= 4 )
            *dbbyte = (byte)(value << 4);
        else
            *(*dptr)++ = *dbbyte | ((byte)value);
        break;
    case 3:
        if ( *dbit ^= 4 )
            *(*dptr)++ = (byte)(value >> 4), *dbbyte = (byte)(value << 4);
        else
          *(*dptr) = *dbbyte | (byte)(value >> 8), (*dptr)[1] = (byte)value, *dptr += 2;
        break;
    case 2:
        *(*dptr)++ = (byte)value;
        break;
    default:
          return -1;
    }
    return 0;
}

static int inline sample_store_next16(uint value, byte **dptr, int *dbit, int dbpv, byte *dbbyte)
{
    switch (dbpv >> 2 ) {
        case 0:
            if ( (*dbit += dbpv) == 8 )
               *(*dptr)++ = *dbbyte | (byte)value, *dbbyte = 0, *dbit = 0;
            else
                *dbbyte |= (byte)(value << (8 - *dbit));
            break;
        case 1:
            if ( *dbit ^= 4 )
                *dbbyte = (byte)(value << 4);
            else
                *(*dptr)++ = *dbbyte | ((byte)value);
            break;
        case 3:
            if ( *dbit ^= 4 )
                *(*dptr)++ = (byte)(value >> 4), *dbbyte = (byte)(value << 4);
            else
              *(*dptr) = *dbbyte | (byte)(value >> 8), (*dptr)[1] = (byte)value, *dptr += 2;
            break;
        case 4:
            *(*dptr)++ = (byte)(value >> 8);
            /* fall through */
        case 2:
            *(*dptr)++ = (byte)value;
            break;
        default:
            return -1;
    }
    return 0;
}

static int inline sample_store_next32(uint32_t value, byte **dptr, int *dbit, int dbpv, byte *dbbyte)
{
    switch (dbpv >> 2 ) {
        case 0:\
            if ( (*dbit += dbpv) == 8 )
               *(*dptr)++ = *dbbyte | (byte)value, *dbbyte = 0, *dbit = 0;\
            else
                *dbbyte |= (byte)(value << (8 - *dbit));
            break;
        case 1:
            if ( *dbit ^= 4 )
                *dbbyte = (byte)(value << 4);
            else
                *(*dptr)++ = *dbbyte | ((byte)value);
            break;
        case 3:
            if ( *dbit ^= 4 )
                *(*dptr)++ = (byte)(value >> 4), *dbbyte = (byte)(value << 4);
            else
              *(*dptr) = *dbbyte | (byte)(value >> 8), (*dptr)[1] = (byte)value, *dptr += 2;
            break;
        case 8: *(*dptr)++ = (byte)(value >> 24);
            /* fall through */
        case 6: *(*dptr)++ = (byte)(value >> 16);
            /* fall through */
        case 4: *(*dptr)++ = (byte)(value >> 8);
            /* fall through */
        case 2: *(*dptr)++ = (byte)(value);
            break;
        default:
            return -1;
    }
    return 0;
}

static int inline sample_store_next64(uint64_t value, byte **dptr, int *dbit, int dbpv, byte *dbbyte)
{
    switch (dbpv >> 2 ) {
        case 0:
            if ( (*dbit += dbpv) == 8 )
               *(*dptr)++ = *dbbyte | (byte)value, *dbbyte = 0, *dbit = 0;
            else *dbbyte |= (byte)(value << (8 - *dbit));
            break;
        case 1:
            if ( *dbit ^= 4 ) *dbbyte = (byte)(value << 4);
            else *(*dptr)++ = *dbbyte | ((byte)value);
            break;
        case 3:
            if ( *dbit ^= 4 ) *(*dptr)++ = (byte)(value >> 4), *dbbyte = (byte)(value << 4);
            else
              *(*dptr) = *dbbyte | (byte)(value >> 8), (*dptr)[1] = (byte)value, *dptr += 2;
            break;
        case 16: *(*dptr)++ = (byte)(value >> SAMPLE_BOUND_SHIFT(value, 56));
        case 14: *(*dptr)++ = (byte)(value >> SAMPLE_BOUND_SHIFT(value, 48));
        case 12: *(*dptr)++ = (byte)(value >> SAMPLE_BOUND_SHIFT(value, 40));
        case 10: *(*dptr)++ = (byte)(value >> SAMPLE_BOUND_SHIFT(value, 32));
        case 8: *(*dptr)++ = (byte)(value >> 24);
        case 6: *(*dptr)++ = (byte)(value >> 16);
        case 4: *(*dptr)++ = (byte)(value >> 8);
        case 2: *(*dptr)++ = (byte)(value);
            break;
        default:
            return -1;
    }
    return 0;
}

static void inline sample_store_flush(byte *dptr, int dbit, byte dbbyte)
{
  if (dbit != 0 )\
    *dptr = dbbyte | (*dptr & (0xff >> dbit));
}

static void inline sample_store_skip_next(byte **dptr, int *dbit, int dbpv, byte *dbbyte)
{
    if ( dbpv < 8 ) {
        sample_store_flush(*dptr, *dbit, *dbbyte);
        *dbit += dbpv;
        *dptr += (*dbit) >> 3;
        *dbit &= 7;
        *dbbyte &= ~(0xff << (*dbit));
  } else
      *dptr += (dbpv >> 3);
}

/* ---------------- Definitions ---------------- */

/*
 * Define the chunk size for monobit filling operations.
 * This is always uint, regardless of byte order.
 */
#define mono_fill_chunk uint
#define mono_fill_chunk_bytes ARCH_SIZEOF_INT

/* ---------------- Procedures ---------------- */

/* Fill a rectangle of bits with an 8x1 pattern. */
/* The pattern argument must consist of the pattern in every byte, */
/* e.g., if the desired pattern is 0xaa, the pattern argument must */
/* have the value 0xaaaa (if ints are short) or 0xaaaaaaaa. */
#if mono_fill_chunk_bytes == 2
#  define mono_fill_make_pattern(byt) (uint)((uint)(byt) * 0x0101)
#else
#  define mono_fill_make_pattern(byt) (uint)((uint)(byt) * 0x01010101)
#endif
void bits_fill_rectangle(byte * dest, int dest_bit, uint raster,
                      mono_fill_chunk pattern, int width_bits, int height);
void bits_fill_rectangle_masked(byte * dest, int dest_bit, uint raster,
                      mono_fill_chunk pattern, mono_fill_chunk src_mask,
                      int width_bits, int height);

/* Replicate a bitmap horizontally in place. */
void bits_replicate_horizontally(byte * data, uint width, uint height,
               uint raster, uint replicated_width, uint replicated_raster);

/* Replicate a bitmap vertically in place. */
void bits_replicate_vertically(byte * data, uint height, uint raster,
    uint replicated_height);

/* Find the bounding box of a bitmap. */
void bits_bounding_box(const byte * data, uint height, uint raster,
    gs_int_rect * pbox);

/* Compress an oversampled image, possibly in place. */
/* The width and height must be multiples of the respective scale factors. */
/* The source must be an aligned bitmap, as usual. */
void bits_compress_scaled(const byte * src, int srcx, uint width,
    uint height, uint sraster, byte * dest, uint draster,
    const gs_log2_scale_point * plog2_scale, int log2_out_bits);

/* Extract a plane from a pixmap. */
typedef struct bits_plane_s {
    union bpd_ {        /* Bit planes must be aligned. */
        byte *write;
        const byte *read;
    } data;
    int raster;
    int depth;
    int x;                      /* starting x */
} bits_plane_t;
int bits_extract_plane(const bits_plane_t *dest /*write*/,
    const bits_plane_t *source /*read*/, int shift, int width, int height);

/* Expand a plane into a pixmap. */
int bits_expand_plane(const bits_plane_t *dest /*write*/,
    const bits_plane_t *source /*read*/, int shift, int width, int height);

/* Fill a rectangle of bytes. */
void bytes_fill_rectangle(byte * dest, uint raster,
    byte value, int width_bytes, int height);

/* Copy a rectangle of bytes. */
void bytes_copy_rectangle(byte * dest, uint dest_raster,
    const byte * src, uint src_raster, int width_bytes, int height);

/* Check if a rectangle of bytes are a constant value. Returns 0..255 (the
   constant value) if it is constant, or -1 otherwise. */
int bytes_rectangle_is_const(const byte * src, uint src_raster, int width_bytes, int height);

/* Copy a rectangle of bytes, ensuring that any padding bits at the end
 * of each dest_raster line are zeroed. */
void bytes_copy_rectangle_zero_padding(byte * dest, uint dest_raster,
    const byte * src, uint src_raster, int width_bytes, int height);

/* Copy a rectangle of bytes, ensuring that any padding bits at the end
 * of each dest_raster line are zeroed. The last row is copied without
 * any padding. */
void bytes_copy_rectangle_zero_padding_last_short(byte * dest, uint dest_raster,
    const byte * src, uint src_raster, int width_bytes, int height);

#endif /* gsbitops_INCLUDED */
