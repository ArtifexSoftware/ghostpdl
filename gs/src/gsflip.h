/* Copyright (C) 1996, 1999 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

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
extern int image_flip_planes(P6(byte * buffer, const byte ** planes,
				int offset, int nbytes,
				int num_planes, int bits_per_sample));

#endif /* gsflip_INCLUDED */
