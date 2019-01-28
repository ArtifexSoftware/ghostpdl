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


/* Common definitions for CCITTFax encoding and decoding filters */

#ifndef scf_INCLUDED
#  define scf_INCLUDED

#include "shc.h"

/*
 * The CCITT Group 3 (T.4) and Group 4 (T.6) fax specifications map
 * run lengths to Huffman codes.  White and black have different mappings.
 * If the run length is 64 or greater, two or more codes are needed:
 *      - One or more 'make-up' codes for 2560 pixels;
 *      - A 'make-up' code that encodes the multiple of 64;
 *      - A 'termination' code for the remainder.
 * For runs of 63 or less, only the 'termination' code is needed.
 */

/* ------ Encoding tables ------ */

/*
 * The maximum possible length of a scan line is determined by the
 * requirement that 3 runs have to fit into the stream buffer.
 * A run of length N requires approximately ceil(N / 2560) makeup codes,
 * hence 1.5 * ceil(N / 2560) bytes.  Taking the largest safe stream
 * buffer size as 32K, we arrive at the following maximum width:
 */
#if ARCH_SIZEOF_INT > 2
#  define cfe_max_width (2560 * 32000 * 2 / 3)
#else
#  define cfe_max_width (max_int - 40)	/* avoid overflows */
#endif
/* The +5 in cfe_max_code_bytes is a little conservative. */
#define cfe_max_code_bytes(width) ((width) / 2560 * 3 / 2 + 5)

typedef hce_code cfe_run;

/* Codes common to 1-D and 2-D encoding. */
/* The decoding algorithms know that EOL is 0....01. */
#define run_eol_code_length 12
#define run_eol_code_value 1
extern const cfe_run cf_run_eol;
typedef struct cf_runs_s {
    cfe_run termination[64];
    cfe_run make_up[41];
} cf_runs;
extern const cf_runs
      cf_white_runs, cf_black_runs;
extern const cfe_run cf_uncompressed[6];
extern const cfe_run cf_uncompressed_exit[10];	/* indexed by 2 x length of */

                        /* white run + (1 if next run black, 0 if white) */
/* 1-D encoding. */
extern const cfe_run cf1_run_uncompressed;

/* 2-D encoding. */
extern const cfe_run cf2_run_pass;

#define cf2_run_pass_length 4
#define cf2_run_pass_value 0x1
#define cf2_run_vertical_offset 3
extern const cfe_run cf2_run_vertical[7];	/* indexed by b1 - a1 + offset */
extern const cfe_run cf2_run_horizontal;

#define cf2_run_horizontal_value 1
#define cf2_run_horizontal_length 3
extern const cfe_run cf2_run_uncompressed;

/* 2-D Group 3 encoding. */
extern const cfe_run cf2_run_eol_1d;
extern const cfe_run cf2_run_eol_2d;

/* ------ Decoding tables ------ */

typedef hcd_code cfd_node;

#define run_length value

/*
 * The value in the decoding tables is either a white or black run length,
 * or a (negative) exceptional value.
 */
#define run_error (-1)
#define run_zeros (-2)	/* EOL follows, possibly with more padding first */
#define run_uncompressed (-3)
/* 2-D codes */
#define run2_pass (-4)
#define run2_horizontal (-5)

#define cfd_white_initial_bits 8
#define cfd_white_min_bits 4	/* shortest white run */
extern const cfd_node cf_white_decode[];

#define cfd_black_initial_bits 7
#define cfd_black_min_bits 2	/* shortest black run */
extern const cfd_node cf_black_decode[];

#define cfd_2d_initial_bits 7
#define cfd_2d_min_bits 4	/* shortest non-H/V 2-D run */
extern const cfd_node cf_2d_decode[];

#define cfd_uncompressed_initial_bits 6		/* must be 6 */
extern const cfd_node cf_uncompressed_decode[];

/* ------ Run detection macros ------ */

/*
 * For the run detection macros:
 *   white_byte is 0 or 0xff for BlackIs1 or !BlackIs1 respectively;
 *   data holds p[-1], inverted if !BlackIs1;
 *   count is the number of valid bits remaining in the scan line.
 */

/* Aliases for bit processing tables. */
#define cf_byte_run_length byte_bit_run_length_neg
#define cf_byte_run_length_0 byte_bit_run_length_0

/* Skip over white pixels to find the next black pixel in the input. */
/* Store the run length in rlen, and update data, p, and count. */
/* There are many more white pixels in typical input than black pixels, */
/* and the runs of white pixels tend to be much longer, so we use */
/* substantially different loops for the two cases. */

/* As an experiment, various different versions of this macro were tried.
 * Firstly, this macro was updated to use native int sized accesses.
 * Sadly, in our tests, this did not result in a speedup, presumably because
 * we did not have long enough runs to make this a win. We therefore disable
 * this version of the macro here, but leave it in the source code in case it
 * becomes useful in future.
 */
#undef SKIP_PIXELS_USING_INTS

/* Secondly, as a stepping stone towards the int version, the macro was
 * updated to make more explicit use of pointer arithmetic, and to remove the
 * 'load 4 bytes, combine and test' section. This version is expected to be
 * better on platforms such as ARM.
 */
#undef SKIP_PIXELS_USING_POINTER_ARITHMETIC

#if defined(SKIP_PIXELS_USING_INTS) && (ARCH_LOG2_SIZEOF_INT >= 2) && (ARCH_ALIGN_INT_MOD <= 4) && (ARCH_LOG2_SIZEOF_SHORT >= 1) && (ARCH_ALIGN_SHORT_MOD <= 2)

#if ARCH_IS_BIG_ENDIAN
#define BYTE0OF2(S) ((byte)(S>>8))
#define BYTE1OF2(S) ((byte)S)
#define BYTE0OF4(S) ((byte)(S>>24))
#define BYTE1OF4(S) ((byte)(S>>16))
#define BYTE2OF4(S) ((byte)(S>>8))
#define BYTE3OF4(S) ((byte)S)
#else
#define BYTE0OF2(S) ((byte)S)
#define BYTE1OF2(S) ((byte)(S>>8))
#define BYTE0OF4(S) ((byte)S)
#define BYTE1OF4(S) ((byte)(S>>8))
#define BYTE2OF4(S) ((byte)(S>>16))
#define BYTE3OF4(S) ((byte)(S>>24))
#endif

#define skip_white_pixels(data, p, count, white_byte, rlen)\
BEGIN\
    rlen = cf_byte_run_length[count & 7][data ^ 0xff];\
    if ( rlen >= 8 ) {		/* run extends past byte boundary */\
        if ( white_byte == 0 ) {\
            register short s;\
            if      ( (data = *p++) ) { rlen -= 8; }\
            else if ( (data = *p++) ) { }\
            else if ((((int)p) & 1) && (rlen += 8, (data = *p++))) { }\
            else if ((((int)p) & 2) && (rlen += 16, p += 2, (s = *(short *)(void *)(p-2)))) {\
                if ((data = BYTE0OF2(s))) {rlen -= 8; p--;} else data = BYTE1OF2(s); }\
            else {\
                register int i;\
                while ((p += 4, (i = *(int *)(void *)(p-4))) == 0) rlen += 32; \
                if      ( (data = BYTE0OF4(i)) ) { rlen += 8;  p -= 3; }\
                else if ( (data = BYTE1OF4(i)) ) { rlen += 16; p -= 2; }\
                else if ( (data = BYTE2OF4(i)) ) { rlen += 24; p -= 1; }\
                else    {  data = BYTE3OF4(i);     rlen += 32;         }\
            }\
        } else {\
            register short s;\
            if      ( (data = (byte)~*p++) ) { rlen -= 8; }\
            else if ( (data = (byte)~*p++) ) { }\
            else if ((((int)p) & 1) && (rlen += 8, data = (byte)~*p++)) { }\
            else if ((((int)p) & 2) && (rlen += 16, p += 2, s = (short)~*(short *)(void *)(p-2))) {\
                if ((data = BYTE0OF2(s))) {rlen -= 8; p--;} else data = BYTE1OF2(s); }\
            else {\
                register int i;\
                while ((p += 4, (i = ~*(int *)(void *)(p-4))) == 0) rlen += 32; \
                if      ( (data = BYTE0OF4(i)) ) { rlen += 8;  p -= 3; }\
                else if ( (data = BYTE1OF4(i)) ) { rlen += 16; p -= 2; }\
                else if ( (data = BYTE2OF4(i)) ) { rlen += 24; p -= 1; }\
                else    {  data = BYTE3OF4(i);     rlen += 32;         }\
            }\
        }\
        rlen += cf_byte_run_length_0[data ^ 0xff];\
    }\
    count -= rlen;\
END

#elif defined(SKIP_PIXELS_USING_PTR_ARITHMETIC)

#define skip_white_pixels(data, p, count, white_byte, rlen)\
BEGIN\
    rlen = cf_byte_run_length[count & 7][data ^ 0xff];\
    if ( rlen >= 8 ) {		/* run extends past byte boundary */\
        if ( white_byte == 0 ) {\
            if ( data = *p++ ) { rlen -= 8; }\
            else if ( data = *p++ ) { }\
            else {\
                do {\
                    if      ( data = *p++ ) { rlen += 8;  break; }\
                    else if ( data = *p++ ) { rlen += 16; break; }\
                    else if ( data = *p++ ) { rlen += 24; break; }\
                    else { rlen += 32; if (data = *p++) break; }\
                } while (1);\
            }\
        } else {\
            if ( data = (byte)~*p++ ) { rlen -= 8; }\
            else if ( data = (byte)~*p++ ) { }\
            else {\
                do {\
                    if      ( data = (byte)~*p++ ) { rlen += 8;  break; }\
                    else if ( data = (byte)~*p++ ) { rlen += 16; break; }\
                    else if ( data = (byte)~*p++ ) { rlen += 24; break; }\
                    else { rlen += 32; if (data = (byte)~*p++) break; }\
                } while (1);\
            }\
        }\
        rlen += cf_byte_run_length_0[data ^ 0xff];\
    }\
    count -= rlen;\
END

#else

/* Original version */
#define skip_white_pixels(data, p, count, white_byte, rlen)\
BEGIN\
    rlen = cf_byte_run_length[count & 7][data ^ 0xff];\
    if ( rlen >= 8 ) {		/* run extends past byte boundary */\
        if ( white_byte == 0 ) {\
            if ( p[0] ) { data = p[0]; p += 1; rlen -= 8; }\
            else if ( p[1] ) { data = p[1]; p += 2; }\
            else {\
                while ( !(p[2] | p[3] | p[4] | p[5]) )\
                    p += 4, rlen += 32;\
                if ( p[2] ) {\
                    data = p[2]; p += 3; rlen += 8;\
                } else if ( p[3] ) {\
                    data = p[3]; p += 4; rlen += 16;\
                } else if ( p[4] ) {\
                    data = p[4]; p += 5; rlen += 24;\
                } else /* p[5] */ {\
                    data = p[5]; p += 6; rlen += 32;\
                }\
            }\
        } else {\
            if ( p[0] != 0xff ) { data = (byte)~p[0]; p += 1; rlen -= 8; }\
            else if ( p[1] != 0xff ) { data = (byte)~p[1]; p += 2; }\
            else {\
                while ( (p[2] & p[3] & p[4] & p[5]) == 0xff )\
                    p += 4, rlen += 32;\
                if ( p[2] != 0xff ) {\
                    data = (byte)~p[2]; p += 3; rlen += 8;\
                } else if ( p[3] != 0xff ) {\
                    data = (byte)~p[3]; p += 4; rlen += 16;\
                } else if ( p[4] != 0xff ) {\
                    data = (byte)~p[4]; p += 5; rlen += 24;\
                } else /* p[5] != 0xff */ {\
                    data = (byte)~p[5]; p += 6; rlen += 32;\
                }\
            }\
        }\
        rlen += cf_byte_run_length_0[data ^ 0xff];\
    }\
    count -= rlen;\
END
#endif

/* Skip over black pixels to find the next white pixel in the input. */
/* Store the run length in rlen, and update data, p, and count. */

#define skip_black_pixels(data, p, count, white_byte, rlen)\
BEGIN\
    rlen = cf_byte_run_length[count & 7][data];\
    if ( rlen >= 8 ) {\
        if ( white_byte == 0 )\
            for ( ; ; p += 4, rlen += 32 ) {\
                if ( p[0] != 0xff ) { data = p[0]; p += 1; rlen -= 8; break; }\
                if ( p[1] != 0xff ) { data = p[1]; p += 2; break; }\
                if ( p[2] != 0xff ) { data = p[2]; p += 3; rlen += 8; break; }\
                if ( p[3] != 0xff ) { data = p[3]; p += 4; rlen += 16; break; }\
            }\
        else\
            for ( ; ; p += 4, rlen += 32 ) {\
                if ( p[0] ) { data = (byte)~p[0]; p += 1; rlen -= 8; break; }\
                if ( p[1] ) { data = (byte)~p[1]; p += 2; break; }\
                if ( p[2] ) { data = (byte)~p[2]; p += 3; rlen += 8; break; }\
                if ( p[3] ) { data = (byte)~p[3]; p += 4; rlen += 16; break; }\
            }\
        rlen += cf_byte_run_length_0[data];\
    }\
    count -= rlen;\
END

#endif /* scf_INCLUDED */
