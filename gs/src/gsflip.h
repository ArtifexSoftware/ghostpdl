/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
/* Interface to routines for "flipping" image data */

#ifndef gsflip_INCLUDED
#  define gsflip_INCLUDED

/*
 * Convert planar (MultipleDataSource) input to chunky format.  The input
 * data starts at planes[0] + offset ... planes[num_planes-1] + offset; the
 * output is stored at buffer.  This procedure assumes that the input
 * consists of an integral number of pixels; in particular, for 12-bit
 * input, nbytes is rounded up to a multiple of 3.  num_planes must be >=0;
 * bits_per_sample must be 1, 2, 4, 8, or 12.  Returns -1 if num_planes or
 * bits_per_sample is invalid, otherwise 0.
 */
extern int image_flip_planes(byte * buffer, const byte ** planes,
			     int offset, int nbytes,
			     int num_planes, int bits_per_sample);

#endif /* gsflip_INCLUDED */
