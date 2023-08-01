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


/*$Id: gxhts_thresh.c  $ */
/* Halftone thresholding code */

#include <stdlib.h> /* abs() */
#include "memory_.h"
#include "gx.h"
#include "gxgstate.h"
#include "gsiparam.h"
#include "math_.h"
#include "gxfixed.h"  /* needed for gximage.h */
#include "gximage.h"
#include "gxdevice.h"
#include "gxdht.h"
#include "gxht_thresh.h"
#include "gzht.h"
#include "gxdevsop.h"

/* Enable the following define to perform a little extra work to stop
 * spurious valgrind errors. The code should perform perfectly even without
 * this enabled, but enabling it makes debugging much easier.
 */
/* #define PACIFY_VALGRIND */

#ifndef __WIN32__
#define __align16  __attribute__((aligned(16)))
#else
#define __align16 __declspec(align(16))
#endif
#define fastfloor(x) (((int)(x)) - (((x)<0) && ((x) != (float)(int)(x))))

#ifdef HAVE_SSE2

#include <emmintrin.h>

static const byte bitreverse[] =
{ 0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0,
  0x30, 0xB0, 0x70, 0xF0, 0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
  0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 0x04, 0x84, 0x44, 0xC4,
  0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC,
  0x3C, 0xBC, 0x7C, 0xFC, 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
  0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 0x0A, 0x8A, 0x4A, 0xCA,
  0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6,
  0x36, 0xB6, 0x76, 0xF6, 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
  0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 0x01, 0x81, 0x41, 0xC1,
  0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9,
  0x39, 0xB9, 0x79, 0xF9, 0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5,
  0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, 0x0D, 0x8D, 0x4D, 0xCD,
  0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3,
  0x33, 0xB3, 0x73, 0xF3, 0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB,
  0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB, 0x07, 0x87, 0x47, 0xC7,
  0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF,
  0x3F, 0xBF, 0x7F, 0xFF};
#endif

#if RAW_HT_DUMP
/* This is slow thresholding, byte output for debug only */
void
gx_ht_threshold_row_byte(byte *contone, byte *threshold_strip, int contone_stride,
                              byte *halftone, int dithered_stride, int width,
                              int num_rows)
{
    int k, j;
    byte *contone_ptr;
    byte *thresh_ptr;
    byte *halftone_ptr;

    /* For the moment just do a very slow compare until we get
       get this working */
    for (j = 0; j < num_rows; j++) {
        contone_ptr = contone;
        thresh_ptr = threshold_strip + contone_stride * j;
        halftone_ptr = halftone + dithered_stride * j;
        for (k = 0; k < width; k++) {
            if (contone_ptr[k] < thresh_ptr[k]) {
                halftone_ptr[k] = 0;
            } else {
                halftone_ptr[k] = 255;
            }
        }
    }
}
#endif

#ifndef HAVE_SSE2
/* A simple case for use in the landscape mode. Could probably be coded up
   faster */
static void
threshold_16_bit(byte *contone_ptr, byte *thresh_ptr, byte *ht_data)
{
    int j;

    for (j = 2; j > 0; j--) {
        byte h = 0;
        byte bit_init = 0x80;
        do {
            if (*contone_ptr++ < *thresh_ptr++) {
                h |=  bit_init;
            }
            bit_init >>= 1;
        } while (bit_init != 0);
        *ht_data++ = h;
    }
}
#else
/* Note this function has strict data alignment needs */
static void
threshold_16_SSE(byte *contone_ptr, byte *thresh_ptr, byte *ht_data)
{
    __m128i input1;
    __m128i input2;
    register int result_int;
    const unsigned int mask1 = 0x80808080;
    __m128i sign_fix = _mm_set_epi32(mask1, mask1, mask1, mask1);

    /* Load */
    input1 = _mm_load_si128((const __m128i *)contone_ptr);
    input2 = _mm_load_si128((const __m128i *) thresh_ptr);
    /* Unsigned subtraction does Unsigned saturation so we
       have to use the signed operation */
    input1 = _mm_xor_si128(input1, sign_fix);
    input2 = _mm_xor_si128(input2, sign_fix);
    /* Subtract the two */
    input2 = _mm_subs_epi8(input1, input2);
    /* Grab the sign mask */
    result_int = _mm_movemask_epi8(input2);
    /* bit wise reversal on 16 bit word */
    ht_data[0] = bitreverse[(result_int & 0xff)];
    ht_data[1] = bitreverse[((result_int >> 8) & 0xff)];
}

/* Not so fussy on its alignment */
static void
threshold_16_SSE_unaligned(byte *contone_ptr, byte *thresh_ptr, byte *ht_data)
{
    __m128i input1;
    __m128i input2;
    int result_int;
    byte *sse_data;
    const unsigned int mask1 = 0x80808080;
    __m128i sign_fix = _mm_set_epi32(mask1, mask1, mask1, mask1);

    sse_data = (byte*) &(result_int);
    /* Load */
    input1 = _mm_loadu_si128((const __m128i *)contone_ptr);
    input2 = _mm_loadu_si128((const __m128i *) thresh_ptr);
    /* Unsigned subtraction does Unsigned saturation so we
       have to use the signed operation */
    input1 = _mm_xor_si128(input1, sign_fix);
    input2 = _mm_xor_si128(input2, sign_fix);
    /* Subtract the two */
    input2 = _mm_subs_epi8(input1, input2);
    /* Grab the sign mask */
    result_int = _mm_movemask_epi8(input2);
    /* bit wise reversal on 16 bit word */
    ht_data[0] = bitreverse[sse_data[0]];
    ht_data[1] = bitreverse[sse_data[1]];
}
#endif

/* SSE2 and non-SSE2 implememntation of thresholding a row. Subtractive case
   There is some code replication between the two of these (additive and subtractive)
   that I need to go back and determine how we can combine them without
   any performance loss. */
void
gx_ht_threshold_row_bit_sub(byte *contone,  byte *threshold_strip,  int contone_stride,
                  byte *halftone, int dithered_stride, int width,
                  int num_rows, int offset_bits)
{
#ifndef HAVE_SSE2
    int k, j;
    byte *contone_ptr;
    byte *thresh_ptr;
    byte *halftone_ptr;
    byte bit_init;

    /* For the moment just do a very slow compare until we get
       get this working.  This could use some serious optimization */
    width -= offset_bits;
    for (j = 0; j < num_rows; j++) {
        byte h;
        contone_ptr = contone;
        thresh_ptr = threshold_strip + contone_stride * j;
        halftone_ptr = halftone + dithered_stride * j;
        /* First get the left remainder portion.  Put into MSBs of first byte */
        bit_init = 0x80;
        h = 0;
        k = offset_bits;
        if (k > 0) {
            do {
                if (*contone_ptr++ > *thresh_ptr++) {
                    h |=  bit_init;
                }
                bit_init >>= 1;
                if (bit_init == 0) {
                    bit_init = 0x80;
                    *halftone_ptr++ = h;
                    h = 0;
                }
                k--;
            } while (k > 0);
            bit_init = 0x80;
            *halftone_ptr++ = h;
            h = 0;
            if (offset_bits < 8)
                *halftone_ptr++ = 0;
        }
        /* Now get the rest, which will be 16 bit aligned. */
        k = width;
        if (k > 0) {
            do {
                if (*contone_ptr++ > *thresh_ptr++) {
                    h |=  bit_init;
                }
                bit_init >>= 1;
                if (bit_init == 0) {
                    bit_init = 0x80;
                    *halftone_ptr++ = h;
                    h = 0;
                }
                k--;
            } while (k > 0);
            if (bit_init != 0x80) {
                *halftone_ptr++ = h;
            }
            if ((width & 15) < 8)
                *halftone_ptr++ = 0;
        }
    }
#else
    byte *contone_ptr;
    byte *thresh_ptr;
    byte *halftone_ptr;
    int num_tiles = (width - offset_bits + 15)>>4;
    int k, j;

    for (j = 0; j < num_rows; j++) {
        /* contone and thresh_ptr are 128 bit aligned.  We do need to do this in
           two steps to ensure that we pack the bits in an aligned fashion
           into halftone_ptr.  */
        contone_ptr = contone;
        thresh_ptr = threshold_strip + contone_stride * j;
        halftone_ptr = halftone + dithered_stride * j;
        if (offset_bits > 0) {
            /* Since we allowed for 16 bits in our left remainder
               we can go directly in to the destination.  threshold_16_SSE
               requires 128 bit alignment.  contone_ptr and thresh_ptr
               are set up so that after we move in by offset_bits elements
               then we are 128 bit aligned.  */
            threshold_16_SSE_unaligned(thresh_ptr, contone_ptr,
                                       halftone_ptr);
            halftone_ptr += 2;
            thresh_ptr += offset_bits;
            contone_ptr += offset_bits;
        }
        /* Now we should have 128 bit aligned with our input data. Iterate
           over sets of 16 going directly into our HT buffer.  Sources and
           halftone_ptr buffers should be padded to allow 15 bit overrun */
        for (k = 0; k < num_tiles; k++) {
            threshold_16_SSE(thresh_ptr, contone_ptr, halftone_ptr);
            thresh_ptr += 16;
            contone_ptr += 16;
            halftone_ptr += 2;
        }
    }
#endif
}

/* SSE2 and non-SSE2 implememntation of thresholding a row. additive case  */
void
gx_ht_threshold_row_bit(byte *contone,  byte *threshold_strip,  int contone_stride,
                  byte *halftone, int dithered_stride, int width,
                  int num_rows, int offset_bits)
{
#ifndef HAVE_SSE2
    int k, j;
    byte *contone_ptr;
    byte *thresh_ptr;
    byte *halftone_ptr;
    byte bit_init;

    /* For the moment just do a very slow compare until we get
       get this working.  This could use some serious optimization */
    width -= offset_bits;
    for (j = 0; j < num_rows; j++) {
        byte h;
        contone_ptr = contone;
        thresh_ptr = threshold_strip + contone_stride * j;
        halftone_ptr = halftone + dithered_stride * j;
        /* First get the left remainder portion.  Put into MSBs of first byte */
        bit_init = 0x80;
        h = 0;
        k = offset_bits;
        if (k > 0) {
            do {
                if (*contone_ptr++ < *thresh_ptr++) {
                    h |=  bit_init;
                }
                bit_init >>= 1;
                if (bit_init == 0) {
                    bit_init = 0x80;
                    *halftone_ptr++ = h;
                    h = 0;
                }
                k--;
            } while (k > 0);
            bit_init = 0x80;
            *halftone_ptr++ = h;
            h = 0;
            if (offset_bits < 8)
                *halftone_ptr++ = 0;
        }
        /* Now get the rest, which will be 16 bit aligned. */
        k = width;
        if (k > 0) {
            do {
                if (*contone_ptr++ < *thresh_ptr++) {
                    h |=  bit_init;
                }
                bit_init >>= 1;
                if (bit_init == 0) {
                    bit_init = 0x80;
                    *halftone_ptr++ = h;
                    h = 0;
                }
                k--;
            } while (k > 0);
            if (bit_init != 0x80) {
                *halftone_ptr++ = h;
            }
            if ((width & 15) < 8)
                *halftone_ptr++ = 0;
        }
    }
#else
    byte *contone_ptr;
    byte *thresh_ptr;
    byte *halftone_ptr;
    int num_tiles = (width - offset_bits + 15)>>4;
    int k, j;

    for (j = 0; j < num_rows; j++) {
        /* contone and thresh_ptr are 128 bit aligned.  We do need to do this in
           two steps to ensure that we pack the bits in an aligned fashion
           into halftone_ptr.  */
        contone_ptr = contone;
        thresh_ptr = threshold_strip + contone_stride * j;
        halftone_ptr = halftone + dithered_stride * j;
        if (offset_bits > 0) {
            /* Since we allowed for 16 bits in our left remainder
               we can go directly in to the destination.  threshold_16_SSE
               requires 128 bit alignment.  contone_ptr and thresh_ptr
               are set up so that after we move in by offset_bits elements
               then we are 128 bit aligned.  */
            threshold_16_SSE_unaligned(contone_ptr, thresh_ptr,
                                       halftone_ptr);
            halftone_ptr += 2;
            thresh_ptr += offset_bits;
            contone_ptr += offset_bits;
        }
        /* Now we should have 128 bit aligned with our input data. Iterate
           over sets of 16 going directly into our HT buffer.  Sources and
           halftone_ptr buffers should be padded to allow 15 bit overrun */
        for (k = 0; k < num_tiles; k++) {
            threshold_16_SSE(contone_ptr, thresh_ptr, halftone_ptr);
            thresh_ptr += 16;
            contone_ptr += 16;
            halftone_ptr += 2;
        }
    }
#endif
}

/* This thresholds a buffer that is LAND_BITS wide by data_length tall.
   Subtractive case */
void
gx_ht_threshold_landscape_sub(byte *contone_align, byte *thresh_align,
                    ht_landscape_info_t *ht_landscape, byte *halftone,
                    int data_length)
{
    __align16 byte contone[LAND_BITS];
    int position_start, position, curr_position;
    int *widths = &(ht_landscape->widths[0]);
    int local_widths[LAND_BITS];
    int num_contone = ht_landscape->num_contones;
    int k, j, w, contone_out_posit;
    byte *contone_ptr, *thresh_ptr, *halftone_ptr;
#ifdef PACIFY_VALGRIND
    int extra = 0;
#endif

    /* Work through chunks of 16.  */
    /* Data may have come in left to right or right to left. */
    if (ht_landscape->index > 0) {
        position = position_start = 0;
    } else {
        position = position_start = ht_landscape->curr_pos + 1;
    }
    thresh_ptr = thresh_align;
    halftone_ptr = halftone;
    /* Copy the widths to a local array, and truncate the last one (which may
     * be the first one!) if required. */
    k = 0;
    for (j = 0; j < num_contone; j++)
        k += (local_widths[j] = widths[position_start+j]);
    if (k > LAND_BITS) {
        if (ht_landscape->index > 0) {
            local_widths[num_contone-1] -= k-LAND_BITS;
        } else {
            local_widths[0] -= k-LAND_BITS;
        }
    }
#ifdef PACIFY_VALGRIND
    if (k < LAND_BITS) {
        extra = LAND_BITS - k;
    }
#endif
    for (k = data_length; k > 0; k--) { /* Loop on rows */
        contone_ptr = &(contone_align[position]); /* Point us to our row start */
        curr_position = 0; /* We use this in keeping track of widths */
        contone_out_posit = 0; /* Our index out */
        for (j = num_contone; j > 0; j--) {
            byte c = *contone_ptr;
            /* The microsoft compiler, cleverly spots that the following loop
             * can be replaced by a memset. Unfortunately, it can't spot that
             * the typical length values of the memset are so small that we'd
             * be better off doing it the slow way. We therefore introduce a
             * sneaky 'volatile' cast below that stops this optimisation. */
            w = local_widths[curr_position];
            do {
                ((volatile byte *)contone)[contone_out_posit] = c;
                contone_out_posit++;
            } while (--w);
#ifdef PACIFY_VALGRIND
            if (extra)
                memset(contone+contone_out_posit, 0, extra);
#endif
            curr_position++; /* Move us to the next position in our width array */
            contone_ptr++;   /* Move us to a new location in our contone buffer */
        }
        /* Now we have our left justified and expanded contone data for
           LAND_BITS/16 sets of 16 bits. Go ahead and threshold these. */
        contone_ptr = &contone[0];
#if LAND_BITS > 16
        j = LAND_BITS;
        do {
#endif
#ifdef HAVE_SSE2
            threshold_16_SSE(thresh_ptr, contone_ptr, halftone_ptr);
#else
            threshold_16_bit(thresh_ptr, contone_ptr, halftone_ptr);
#endif
            thresh_ptr += 16;
            position += 16;
            halftone_ptr += 2;
            contone_ptr += 16;
#if LAND_BITS > 16
            j -= 16;
        } while (j > 0);
#endif
    }
}

/* This thresholds a buffer that is LAND_BITS wide by data_length tall.
   Additive case.  Note I could likely do some code reduction between
   the additive and subtractive cases */
void
gx_ht_threshold_landscape(byte *contone_align, byte *thresh_align,
                    ht_landscape_info_t *ht_landscape, byte *halftone,
                    int data_length)
{
    __align16 byte contone[LAND_BITS];
    int position_start, position, curr_position;
    int *widths = &(ht_landscape->widths[0]);
    int local_widths[LAND_BITS];
    int num_contone = ht_landscape->num_contones;
    int k, j, w, contone_out_posit;
    byte *contone_ptr, *thresh_ptr, *halftone_ptr;
#ifdef PACIFY_VALGRIND
    int extra = 0;
#endif

    /* Work through chunks of 16.  */
    /* Data may have come in left to right or right to left. */
    if (ht_landscape->index > 0) {
        position = position_start = 0;
    } else {
        position = position_start = ht_landscape->curr_pos + 1;
    }
    thresh_ptr = thresh_align;
    halftone_ptr = halftone;
    /* Copy the widths to a local array, and truncate the last one (which may
     * be the first one!) if required. */
    k = 0;
    for (j = 0; j < num_contone; j++)
        k += (local_widths[j] = widths[position_start+j]);
    if (k > LAND_BITS) {
        if (ht_landscape->index > 0) {
            local_widths[num_contone-1] -= k-LAND_BITS;
        } else {
            local_widths[0] -= k-LAND_BITS;
        }
    }
#ifdef PACIFY_VALGRIND
    if (k < LAND_BITS) {
        extra = LAND_BITS - k;
    }
#endif
    for (k = data_length; k > 0; k--) { /* Loop on rows */
        contone_ptr = &(contone_align[position]); /* Point us to our row start */
        curr_position = 0; /* We use this in keeping track of widths */
        contone_out_posit = 0; /* Our index out */
        for (j = num_contone; j > 0; j--) {
            byte c = *contone_ptr;
            /* The microsoft compiler, cleverly spots that the following loop
             * can be replaced by a memset. Unfortunately, it can't spot that
             * the typical length values of the memset are so small that we'd
             * be better off doing it the slow way. We therefore introduce a
             * sneaky 'volatile' cast below that stops this optimisation. */
            w = local_widths[curr_position];
            do {
                ((volatile byte *)contone)[contone_out_posit] = c;
                contone_out_posit++;
            } while (--w);
#ifdef PACIFY_VALGRIND
            if (extra)
                memset(contone+contone_out_posit, 0, extra);
#endif
            curr_position++; /* Move us to the next position in our width array */
            contone_ptr++;   /* Move us to a new location in our contone buffer */
        }
        /* Now we have our left justified and expanded contone data for
           LAND_BITS/16 sets of 16 bits. Go ahead and threshold these. */
        contone_ptr = &contone[0];
#if LAND_BITS > 16
        j = LAND_BITS;
        do {
#endif
#ifdef HAVE_SSE2
            threshold_16_SSE(contone_ptr, thresh_ptr, halftone_ptr);
#else
            threshold_16_bit(contone_ptr, thresh_ptr, halftone_ptr);
#endif
            thresh_ptr += 16;
            position += 16;
            halftone_ptr += 2;
            contone_ptr += 16;
#if LAND_BITS > 16
            j -= 16;
        } while (j > 0);
#endif
    }
}

int
gxht_thresh_image_init(gx_image_enum *penum)
{
    int code = 0;
    fixed ox;
    int temp;
    int dev_width, max_height;
    int spp_out;
    int k;
    gx_ht_order *d_order;
    gx_dda_fixed dda_ht;

    if (gx_device_must_halftone(penum->dev)) {
        if (penum->pgs != NULL && penum->pgs->dev_ht[HT_OBJTYPE_DEFAULT] != NULL) {
            gx_device_halftone *pdht = gx_select_dev_ht(penum->pgs);

            for (k = 0; k < pdht->num_comp; k++) {
                d_order = &(pdht->components[k].corder);
                code = gx_ht_construct_threshold(d_order, penum->dev,
                                                 penum->pgs, k);
                if (code < 0 ) {
                    return gs_rethrow(code, "threshold creation failed");
                }
            }
        } else {
            return -1;
        }
    }
    spp_out = penum->dev->color_info.num_components;
    /* Precompute values needed for rasterizing. */
    penum->dxx = float2fixed(penum->matrix.xx + fixed2float(fixed_epsilon) / 2);
    /* If the image is landscaped then we want to maintain a buffer
       that is sufficiently large so that we can hold a byte
       of halftoned data along the column.  This way we avoid doing
       multiple writes into the same position over and over.
       The size of the buffer we need depends upon the bitdepth of
       the output device, the number of device coloranants and the
       number of  colorants in the source space.  Note we will
       need to eventually  consider  multi-level halftone case
       here too.  For now, to make use of the SSE2 stuff, we would
       like to have a multiple of 16 bytes of data to process at a time.
       So we will collect the columns of data in a buffer that is LAND_BITS
       wide.  We will also keep track of the widths of each column.  When
       the total width count reaches LAND_BITS, we will create our
       threshold array and apply it.  We may have one column that is
       buffered between calls in this case.  Also if a call is made
       with h=0 we will flush the buffer as we are at the end of the
       data.  */
    if (penum->posture == image_landscape) {
        int col_length = fixed2int_var_rounded(any_abs(penum->x_extent.y));
        dda_ht = penum->dda.pixel0.y;
        if (penum->dxx > 0)
            dda_translate(dda_ht, -fixed_epsilon);      /* to match rounding in non-fast code */

        ox = dda_current(penum->dda.pixel0.x);
        temp = gxht_dda_length(&dda_ht, penum->rect.w);
        if (col_length < temp)
            col_length = temp;          /* choose max to make sure line_size is large enough */
        temp = (col_length + LAND_BITS)/LAND_BITS;      /* round up to allow for offset bits */
        /* bitmap_raster() expects the width in bits, hence "* 8" */
        penum->line_size = bitmap_raster((temp * LAND_BITS) * 8);  /* The stride */
        /* Now we need at most LAND_BITS of these */
        penum->line = gs_alloc_bytes(penum->memory,
                                     LAND_BITS * penum->line_size * spp_out + 16,
                                     "gxht_thresh");
        /* Same with this.  However, we only need one plane here */
        penum->thresh_buffer = gs_alloc_bytes(penum->memory,
                                           penum->line_size * LAND_BITS + 16,
                                           "gxht_thresh");
        /* That maps into (LAND_BITS/8) bytes of Halftone data */
        penum->ht_buffer =
                        gs_alloc_bytes(penum->memory,
                           penum->line_size * (LAND_BITS>>3) * spp_out,
                           "gxht_thresh");
        penum->ht_plane_height = penum->line_size;
        penum->ht_stride = penum->line_size;
        if (penum->line == NULL || penum->thresh_buffer == NULL
                    || penum->ht_buffer == NULL)
            return -1;
        penum->ht_landscape.count = 0;
        penum->ht_landscape.num_contones = 0;
        if (penum->y_extent.x < 0) {
            /* Going right to left */
            penum->ht_landscape.curr_pos = LAND_BITS-1;
            penum->ht_landscape.index = -1;
        } else {
            /* Going left to right */
            penum->ht_landscape.curr_pos = 0;
            penum->ht_landscape.index = 1;
        }
        if (penum->x_extent.y < 0) {
            penum->ht_landscape.flipy = true;
            penum->ht_landscape.y_pos =
                fixed2int_pixround_perfect(dda_current(penum->dda.pixel0.y) + penum->x_extent.y);
        } else {
            penum->ht_landscape.flipy = false;
            penum->ht_landscape.y_pos =
                fixed2int_pixround_perfect(dda_current(penum->dda.pixel0.y));
        }
        memset(&(penum->ht_landscape.widths[0]), 0, sizeof(int)*LAND_BITS);
        penum->ht_landscape.offset_set = false;
        penum->ht_offset_bits = 0; /* Will get set in call to render */
        if (code >= 0) {
#if defined(DEBUG) || defined(PACIFY_VALGRIND)
            memset(penum->line, 0, LAND_BITS * penum->line_size * spp_out + 16);
            memset(penum->ht_buffer, 0, penum->line_size * (LAND_BITS>>3) * spp_out);
            memset(penum->thresh_buffer, 0, LAND_BITS * penum->line_size + 16);
#endif
        }
    } else {
        /* In the portrait case we allocate a single line buffer
           in device width, a threshold buffer of the same size
           and possibly wider and the buffer for the halftoned
           bits. We have to do a bit of work to enable 16 byte
           boundary after an offset to ensure that we can make use
           of  the SSE2 operations for thresholding.  We do the
           allocations now to avoid doing them with every line */
        dda_ht = penum->dda.pixel0.x;
        if (penum->dxx > 0)
            dda_translate(dda_ht, -fixed_epsilon);      /* to match rounding in non-fast code */
        /* Initialize the ht_landscape stuff to zero */
        memset(&(penum->ht_landscape), 0, sizeof(ht_landscape_info_t));
        ox = dda_current(dda_ht);
        dev_width = gxht_dda_length(&dda_ht, penum->rect.w);
        /* Get the bit position so that we can do a copy_mono for
           the left remainder and then 16 bit aligned copies for the
           rest.  The right remainder will be OK as it will land in
           the MSBit positions. Note the #define chunk bits16 in
           gdevm1.c.  Allow also for a 15 sample over run.
        */
        penum->ht_offset_bits = (-fixed2int_var_rounded(ox)) & (bitmap_raster(1) - 1);
        if (penum->ht_offset_bits > 0) {
            penum->ht_stride = bitmap_raster((7 + (dev_width + 4)) + (ARCH_SIZEOF_LONG * 8));
        } else {
            penum->ht_stride = bitmap_raster((7 + (dev_width + 2)) + (ARCH_SIZEOF_LONG * 8));
        }
        /* We want to figure out the maximum height that we may
           have in taking a single source row and going to device
           space */
        max_height = (int) ceil(fixed2float(any_abs(penum->dst_height)) /
                                            (float) penum->Height);
        if (max_height <= 0)
            return -1;		/* shouldn't happen, but check so we don't div by zero */
        if (penum->ht_stride * spp_out > max_int / max_height)
            return -1;         /* overflow */

        penum->ht_buffer =
                        gs_alloc_bytes(penum->memory,
                           (size_t)penum->ht_stride * max_height * spp_out,
                           "gxht_thresh");
        penum->ht_plane_height = penum->ht_stride * max_height;
        /* We want to have 128 bit alignement for our contone and
           threshold strips so that we can use SSE operations
           in the threshold operation.  Add in a minor buffer and offset
           to ensure this.  If gs_alloc_bytes provides at least 16
           bit alignment so we may need to move 14 bytes.  However, the
           HT process is split in two operations.  One that involves
           the HT of a left remainder and the rest which ensures that
           we pack in the HT data in the bits with no skew for a fast
           copy into the gdevm1 device (16 bit copies).  So, we
           need to account for those pixels which occur first and which
           are NOT aligned for the contone buffer.  After we offset
           by this remainder portion we should be 128 bit aligned.
           Also allow a 15 sample over run during the execution.  */
        temp = (int) ceil((float) ((dev_width + 15.0) + 15.0)/16.0);
        penum->line_size = bitmap_raster(temp * 16 * 8);  /* The stride */
        if (penum->line_size > max_int / max_height) {
            gs_free_object(penum->memory, penum->ht_buffer, "gxht_thresh");
            penum->ht_buffer = NULL;
            return -1;         /* thresh_buffer size overflow */
        }
        penum->line = gs_alloc_bytes(penum->memory, penum->line_size * spp_out,
                                     "gxht_thresh");
        penum->thresh_buffer = gs_alloc_bytes(penum->memory,
                                              (size_t)penum->line_size * max_height,
                                              "gxht_thresh");
        if (penum->line == NULL || penum->thresh_buffer == NULL ||
            penum->ht_buffer == NULL) {
            return -1;
        } else {
#if defined(DEBUG) || defined(PACIFY_VALGRIND)
            memset(penum->line, 0, penum->line_size * spp_out);
            memset(penum->ht_buffer, 0, penum->ht_stride * max_height * spp_out);
            memset(penum->thresh_buffer, 0, penum->line_size * max_height);
#endif
        }
    }
    return code;
}

static void
fill_threshold_buffer(byte *dest_strip, byte *src, byte *src_strip, int src_width,
                       int left_offset, int left_width, int num_tiles,
                       int right_width)
{
    byte *ptr_out_temp = dest_strip;
    int ii;

    /* Make sure we don't try and read before the start of the threshold array. This can happen
     * if we drop to the beginning of the array, AND we have a negative left_offset. If we do
     * have a negative left_offset this represents an area we won't actually be using, but we need
     * to move along the threshold array until we get to the point where we copy data we will use.
     * So lets simply avoid reading before the start of the data. We can leave the destination
     * buffer uninitialised because we won't be reading from that area. Bug #706795 but the ASAN
     * error occurs on a number of input files in the test suite.
     */
    if (src_strip + left_offset < src) {
        int under = src - (src_strip + left_offset);
        left_offset += under;
        ptr_out_temp += under;
        left_width -= under;
        if (left_width < 0)
            left_width = 0;
    }
    /* Left part */
    memcpy(ptr_out_temp, src_strip + left_offset, left_width);
    ptr_out_temp += left_width;
    /* Now the full parts */
    for (ii = 0; ii < num_tiles; ii++){
        memcpy(ptr_out_temp, src_strip, src_width);
        ptr_out_temp += src_width;
    }
    /* Now the remainder */
    memcpy(ptr_out_temp, src_strip, right_width);
#ifdef PACIFY_VALGRIND
    ptr_out_temp += right_width;
    ii = (dest_strip-ptr_out_temp) % (LAND_BITS-1);
    if (ii > 0)
        memset(ptr_out_temp, 0, ii);
#endif
}
/* This only moves the data but does not do a reset of the variables.  Used
   for case where we have multiple bands of data (e.g. CMYK output) */
static void
move_landscape_buffer(ht_landscape_info_t *ht_landscape, byte *contone_align,
                       int data_length)
{
    int k;
    int position_curr, position_new;

    if (ht_landscape->index < 0) {
        /* Moving right to left, move column to far right */
        position_curr = ht_landscape->curr_pos + 1;
        position_new = LAND_BITS-1;
    } else {
        /* Moving left to right, move column to far left */
        position_curr = ht_landscape->curr_pos - 1;
        position_new = 0;
    }
    if (position_curr != position_new) {
        for (k = 0; k < data_length; k++) {
                contone_align[position_new] = contone_align[position_curr];
                position_curr += LAND_BITS;
                position_new += LAND_BITS;
        }
    }
}


/* If we are in here, we had data left over.  Move it to the proper position
   and get ht_landscape_info_t set properly */
static void
reset_landscape_buffer(ht_landscape_info_t *ht_landscape, byte *contone_align,
                       int data_length, int num_used)
{
    int delta;
    int curr_x_pos = ht_landscape->xstart;

    if (ht_landscape->index < 0) {
        /* Moving right to left, move column to far right */
        delta = ht_landscape->count - num_used;
        memset(&(ht_landscape->widths[0]), 0, sizeof(int)*LAND_BITS);
        ht_landscape->widths[LAND_BITS-1] = delta;
        ht_landscape->curr_pos = LAND_BITS-2;
        ht_landscape->xstart = curr_x_pos - num_used;
    } else {
        /* Moving left to right, move column to far left */
        delta = ht_landscape->count - num_used;
        memset(&(ht_landscape->widths[0]), 0, sizeof(int)*LAND_BITS);
        ht_landscape->widths[0] = delta;
        ht_landscape->curr_pos = 1;
        ht_landscape->xstart = curr_x_pos + num_used;
    }
    ht_landscape->count = delta;
    ht_landscape->num_contones = 1;
}

/* This performs a thresholding operation on multiple planes of data and
   stores the bits into a planar buffer which can then be used for
   copy_planes */
int
gxht_thresh_planes(gx_image_enum *penum, fixed xrun,
                   int dest_width, int dest_height,
                   byte *thresh_align, gx_device * dev, int offset_contone[],
                   int contone_stride)
{
    int thresh_width, thresh_height, dx;
    int left_rem_end, left_width, vdi;
    int num_full_tiles, right_tile_width;
    int k, jj, dy, j;
    byte *thresh_tile;
    int position;
    bool replicate_tile;
    image_posture posture = penum->posture;
    const int y_pos = penum->yci;
    int width = 0; /* Init to silence compiler warnings */
    byte *ptr_out, *row_ptr, *ptr_out_temp;
    byte *threshold;
    int init_tile, in_row_offset, ii, num_tiles, tile_remainder;
    int offset_bits = penum->ht_offset_bits;
    byte *halftone;
    int dithered_stride = penum->ht_stride;
    bool is_planar_dev = dev->num_planar_planes;
    gx_color_index dev_white = gx_device_white(dev);
    gx_color_index dev_black = gx_device_black(dev);
    int spp_out = dev->color_info.num_components;
    byte *contone_align = NULL; /* Init to silence compiler warnings */
    gx_device_halftone *pdht = gx_select_dev_ht(penum->pgs);

    /* Go ahead and fill the threshold line buffer with tiled threshold values.
       First just grab the row or column that we are going to tile with and
       then do memcpy into the buffer */

    /* Figure out the tile steps.  Left offset, Number of tiles, Right offset. */
    switch (posture) {
        case image_portrait:
            vdi = penum->hci;
            /*  Iterate over the vdi and fill up our threshold buffer.  We
                 also need to loop across the planes of data */
            for (j = 0; j < spp_out; j++) {
                bool threshold_inverted = pdht->components[j].corder.threshold_inverted;

                thresh_width = pdht->components[j].corder.width;
                thresh_height = pdht->components[j].corder.full_height;
                halftone = penum->ht_buffer + j * vdi * dithered_stride;
                /* Compute the tiling positions with dest_width */
                dx = (fixed2int_var_rounded(xrun) + penum->pgs->screen_phase[0].x) % thresh_width;
                /* Left remainder part */
                left_rem_end = min(dx + dest_width, thresh_width);
                /* The left width of our tile part */
                left_width = left_rem_end - dx;
                /* Now the middle part */
                num_full_tiles =
                    (int)fastfloor((dest_width - left_width)/ (float) thresh_width);
                /* Now the right part */
                right_tile_width = dest_width -  num_full_tiles * thresh_width -
                                   left_width;
                /* Get the proper threshold for the colorant count */
                threshold = pdht->components[j].corder.threshold;
                if (threshold == NULL)
                    return_error(gs_error_unregistered);
                /* Point to the proper contone data */
                contone_align = penum->line + contone_stride * j +
                                offset_contone[j];
                for (k = 0; k < vdi; k++) {
                    /* Get a pointer to our tile row */
                    dy = (penum->yci + k -
                          penum->pgs->screen_phase[0].y) % thresh_height;
                    if (dy < 0)
                        dy += thresh_height;
                    thresh_tile = threshold + thresh_width * dy;
                    /* Fill the buffer, can be multiple rows.  Make sure
                       to update with stride */
                    position = contone_stride * k;
                    /* Tile into the 128 bit aligned threshold strip */
                    fill_threshold_buffer(&(thresh_align[position]), threshold,
                                           thresh_tile, thresh_width, dx, left_width,
                                           num_full_tiles, right_tile_width);
                }
                /* Apply the threshold operation */
                if (offset_bits > dest_width)
                    offset_bits = dest_width;

                if (threshold_inverted ||
                    (dev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE && is_planar_dev)) {
                    gx_ht_threshold_row_bit_sub(contone_align, thresh_align, contone_stride,
                                      halftone, dithered_stride, dest_width, vdi,
                                      offset_bits);
                } else {
                    gx_ht_threshold_row_bit(contone_align, thresh_align, contone_stride,
                          halftone, dithered_stride, dest_width, vdi,
                          offset_bits);
                }
            }
            /* FIXME: An improvement here would be to generate the initial
             * offset_bits at the correct offset within the byte so that they
             * align with the remainder of the line. This would mean not
             * always packing them into the first offset_bits (in MSB order)
             * of our 16 bit word, but rather into the last offset_bits
             * (in MSB order) (except when the entire run is small!).
             *
             * This would enable us to do just one aligned copy_mono call for
             * the entire scanline. */
            /* Now do the copy mono or copy plane operation */
            /* First the left remainder bits */
            if (offset_bits > 0) {
                int x_pos = fixed2int_var_rounded(xrun);
                if (!is_planar_dev) {
                    (*dev_proc(dev, copy_mono)) (dev, penum->ht_buffer, 0, dithered_stride,
                                                 gx_no_bitmap_id, x_pos, y_pos,
                                                 offset_bits, vdi, dev_white,
                                                 dev_black);
                } else {
                    (*dev_proc(dev, copy_planes)) (dev, penum->ht_buffer, 0, dithered_stride,
                                                 gx_no_bitmap_id, x_pos, y_pos,
                                                 offset_bits, vdi, vdi);
                }
            }
            if ((dest_width - offset_bits) > 0 ) {
                /* Now the primary aligned bytes */
                int curr_width = dest_width - offset_bits;
                int x_pos = fixed2int_var_rounded(xrun) + offset_bits;
                /* FIXME: This assumes the allowed offset_bits will always be <= 16 */
                int xoffs = offset_bits > 0 ? 16 : 0;

                if (!is_planar_dev) {
                    (*dev_proc(dev, copy_mono)) (dev, penum->ht_buffer, xoffs, dithered_stride,
                                                 gx_no_bitmap_id, x_pos, y_pos,
                                                 curr_width, vdi, dev_white,
                                                 dev_black);
                } else {
                    (*dev_proc(dev, copy_planes)) (dev, penum->ht_buffer, xoffs, dithered_stride,
                                                 gx_no_bitmap_id, x_pos, y_pos,
                                                 curr_width, vdi, vdi);
                }
            }

            break;
        case image_landscape:
            /* Go ahead and paint the chunk if we have LAND_BITS values or a
             * partial to get us in sync with the 1 bit devices 16 bit
             * positions. */
            vdi = penum->wci;
                /* Now do the haftoning into our buffer.  We basically check
                   first if we have enough data or are all done */
            while ( (penum->ht_landscape.count >= LAND_BITS ||
                   ((penum->ht_landscape.count >= offset_bits) &&
                    penum->ht_landscape.offset_set))) {
                /* Go ahead and 2D tile in the threshold buffer at this time */
                /* Always work the tiling from the upper left corner of our
                   LAND_BITS columns */
                for (j = 0; j < spp_out; j++) {
                    halftone = penum->ht_buffer +
                                   j * penum->ht_plane_height * (LAND_BITS>>3);
                    thresh_width = pdht->components[j].corder.width;
                    thresh_height =
                          pdht->components[j].corder.full_height;
                    /* Get the proper threshold for the colorant count */
                    threshold = pdht->components[j].corder.threshold;
                    if (threshold == NULL)
                        return_error(gs_error_unregistered);
                    /* Point to the proper contone data */
                    contone_align = penum->line + offset_contone[j] +
                                      LAND_BITS * j * contone_stride;
                    if (penum->ht_landscape.offset_set) {
                        width = offset_bits;
                    } else {
                        width = LAND_BITS;
                    }
                    if (penum->y_extent.x < 0) {
                        dx = penum->ht_landscape.xstart - width + 1;
                    } else {
                        dx = penum->ht_landscape.xstart;
                    }
                    dx = (dx + penum->pgs->screen_phase[0].x) % thresh_width;
                    if (dx < 0)
                        dx += thresh_width;
                    dy = (penum->ht_landscape.y_pos -
                              penum->pgs->screen_phase[0].y) % thresh_height;
                    if (dy < 0)
                        dy += thresh_height;
                    /* Left remainder part */
                    left_rem_end = min(dx + LAND_BITS, thresh_width);
                    left_width = left_rem_end - dx;
                    /* Now the middle part */
                    num_full_tiles = (LAND_BITS - left_width) / thresh_width;
                    /* Now the right part */
                    right_tile_width =
                        LAND_BITS - num_full_tiles * thresh_width - left_width;
                    /* Now loop over the y stuff */
                    ptr_out = thresh_align;
                    /* Do this in three parts.  We do a top part, followed by
                       larger mem copies followed by a bottom partial. After
                       a slower initial fill we are able to do larger faster
                       expansions */
                    if (dest_height <= 2 * thresh_height) {
                        init_tile = dest_height;
                        replicate_tile = false;
                    } else {
                        init_tile = thresh_height;
                        replicate_tile = true;
                    }
                    for (jj = 0; jj < init_tile; jj++) {
                        in_row_offset = (jj + dy) % thresh_height;
                        row_ptr = threshold + in_row_offset * thresh_width;
                        ptr_out_temp = ptr_out;
                        /* Left part */
                        memcpy(ptr_out_temp, row_ptr + dx, left_width);
                        ptr_out_temp += left_width;
                        /* Now the full tiles */
                        for (ii = 0; ii < num_full_tiles; ii++) {
                            memcpy(ptr_out_temp, row_ptr, thresh_width);
                            ptr_out_temp += thresh_width;
                        }
                        /* Now the remainder */
                        memcpy(ptr_out_temp, row_ptr, right_tile_width);
                        ptr_out += LAND_BITS;
                    }
                    if (replicate_tile) {
                        /* Find out how many we need to copy */
                        num_tiles =
                            (int)fastfloor((float) (dest_height - thresh_height)/ (float) thresh_height);
                        tile_remainder = dest_height - (num_tiles + 1) * thresh_height;
                        for (jj = 0; jj < num_tiles; jj ++) {
                            memcpy(ptr_out, thresh_align, LAND_BITS * thresh_height);
                            ptr_out += LAND_BITS * thresh_height;
                        }
                        /* Now fill in the remainder */
                        memcpy(ptr_out, thresh_align, LAND_BITS * tile_remainder);
                    }
                    /* Apply the threshold operation */
                    if (dev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE
                        && is_planar_dev) {
                        gx_ht_threshold_landscape_sub(contone_align, thresh_align,
                                            &(penum->ht_landscape), halftone, dest_height);
                    } else {
                        gx_ht_threshold_landscape(contone_align, thresh_align,
                                            &(penum->ht_landscape), halftone, dest_height);
                    }
                    /* We may have a line left over that has to be maintained
                       due to line replication in the resolution conversion. */
                    if (width != penum->ht_landscape.count) {
                        /* move the line do not reset the stuff */
                        move_landscape_buffer(&(penum->ht_landscape),
                                              contone_align, dest_height);
                    }
                }
                /* Perform the copy mono */
                if (penum->ht_landscape.index < 0) {
                    if (!is_planar_dev) {
                        (*dev_proc(dev, copy_mono))
                                       (dev, penum->ht_buffer, 0, LAND_BITS>>3,
                                        gx_no_bitmap_id,
                                        penum->ht_landscape.xstart - width + 1,
                                        penum->ht_landscape.y_pos,
                                        width, dest_height,
                                        dev_white, dev_black);
                    } else {
                        (*dev_proc(dev, copy_planes))
                                       (dev, penum->ht_buffer, 0, LAND_BITS>>3,
                                        gx_no_bitmap_id,
                                        penum->ht_landscape.xstart - width + 1,
                                        penum->ht_landscape.y_pos,
                                        width, dest_height,
                                        penum->ht_plane_height);
                    }
                } else {
                    if (!is_planar_dev) {
                        (*dev_proc(dev, copy_mono)) (dev, penum->ht_buffer, 0, LAND_BITS>>3,
                                                     gx_no_bitmap_id,
                                                     penum->ht_landscape.xstart,
                                                     penum->ht_landscape.y_pos,
                                                     width, dest_height,
                                                     dev_white, dev_black);
                    } else {
                        (*dev_proc(dev, copy_planes)) (dev, penum->ht_buffer, 0, LAND_BITS>>3,
                                                     gx_no_bitmap_id,
                                                     penum->ht_landscape.xstart,
                                                     penum->ht_landscape.y_pos,
                                                     width, dest_height,
                                                     penum->ht_plane_height);
                    }
                }
                penum->ht_landscape.offset_set = false;
                if (width != penum->ht_landscape.count) {
                    reset_landscape_buffer(&(penum->ht_landscape),
                                           contone_align, dest_height,
                                           width);
                } else {
                    /* Reset the whole buffer */
                    penum->ht_landscape.count = 0;
                    if (penum->ht_landscape.index < 0) {
                        /* Going right to left */
                        penum->ht_landscape.curr_pos = LAND_BITS-1;
                    } else {
                        /* Going left to right */
                        penum->ht_landscape.curr_pos = 0;
                    }
                    penum->ht_landscape.num_contones = 0;
                    memset(&(penum->ht_landscape.widths[0]), 0, sizeof(int)*LAND_BITS);
                }
            }
            break;
        default:
            return gs_rethrow(-1, "Invalid orientation for thresholding");
    }
    return 0;
}

int gxht_dda_length(gx_dda_fixed *dda, int src_size)
{
    gx_dda_fixed d = (*dda);
    dda_advance(d, src_size);
    return abs(fixed2int_var_rounded(dda_current(d)) - fixed2int_var_rounded(dda_current(*dda)));
}
