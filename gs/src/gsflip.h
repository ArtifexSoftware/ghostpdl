/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.

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

/*Id: gsflip.h  */
/* Interface to routines for "flipping" image data */

#ifndef gsflip_INCLUDED
#  define gsflip_INCLUDED

/*
 * Convert line-based (MultipleDataSource) input to the chunky format
 * used everywhere else.
 *
 * We store the output at buffer.
 * Each row of input must consist of an integral number of pixels.
 * In particular, for 12-bit input, nbytes must be 0 mod 3.
 * offset is the amount to be added to each plane pointer.
 * num_planes must be 3 or 4; bits_per_sample must be 1, 2, 4, 8, or 12.
 * Returns -1 if num_planes or bits_per_sample is invalid, otherwise 0.
 */
extern int image_flip_planes(P6(byte * buffer, const byte ** planes,
				uint offset, uint nbytes,
				int num_planes, int bits_per_sample));

#endif /* gsflip_INCLUDED */
