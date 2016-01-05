/******************************************************************************
  File:     $Id: eprnfs.c,v 1.6 2001/05/01 07:02:01 Martin Rel $
  Contents: Floyd-Steinberg error diffusion for eprn
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany; e-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*									      *
*	Copyright (C) 2001 by Martin Lottermoser			      *
*	All rights reserved						      *
*									      *
*******************************************************************************

  Information about Floyd-Steinberg error diffusion should be available in a
  number of places. I've used:

    James D. Foley, Andries van Dam, Steven K. Feiner, John F. Hughes
    "Computer Graphics"
    Second edition in C
    Reading/Massachusetts, etc.: Addison-Wesley, 1996
    ISBN 0-201-84840-6

******************************************************************************/

/*****************************************************************************/

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE	500
#endif

#include "gdeveprn.h"

/*****************************************************************************/

/* Here follow some macros for code sections used in several routines. */

#define fit_to_octet(value)	((value) < 0? 0: (value) > 255? 255: (value))

#define FS_assign()				\
        new_value = *to + correction;		\
        if (new_value < 0) {			\
          *to = 0;				\
          remaining_error += new_value;		\
        }					\
        else if (255 < new_value) {		\
          *to = 255;				\
          remaining_error += new_value - 255;	\
        }					\
        else *to = new_value;

#define error_propagation_Gray()				\
  if (error != 0) {						\
    remaining_error = error;					\
                                                                \
    /* 7/16 of the error goes to the right */			\
    correction = (7*error)/16;					\
    remaining_error -= correction;				\
    if (pixel < max_pixel) {					\
      to = from + 1;						\
      FS_assign()						\
      if (pixel == pixels - 1 && *to > 0) {			\
        pixels++;						\
        line->length++;						\
      }								\
    }								\
                                                                \
    /* 3/16 of the error goes to the left and below */		\
    correction = (3*error)/16;					\
    remaining_error -= correction;				\
    if (pixel > 0) {						\
      to = next_line->str + (pixel - 1);			\
      FS_assign()						\
      if (next_line->length < pixel && *to > 0) next_line->length = pixel; \
    }								\
                                                                \
    /* 5/16 of the error goes below */				\
    correction = (5*error)/16;					\
    remaining_error -= correction;				\
    to = next_line->str + pixel;				\
    FS_assign()							\
    if (next_line->length <= pixel && *to > 0) next_line->length = pixel + 1; \
                                                                \
    /* The remainder (about 1/16 of the error) is added to the right and */ \
    /* below. */						\
    if (pixel < max_pixel) {					\
      to = next_line->str + (pixel+1);				\
      new_value = *to + remaining_error;			\
      *to = fit_to_octet(new_value);				\
      if (next_line->length < pixel + 2 && *to > 0)		\
        next_line->length = pixel + 2;				\
    }								\
  }

/* Number of octets per pixel for the non-monochrome cases */
#define OCTETS_PER_PIXEL	4

#define error_propagation_colour()				\
  if (error != 0) {						\
    remaining_error = error;					\
                                                                \
    /* 7/16 of the error goes to the right */			\
    correction = (7*error)/16;					\
    remaining_error -= correction;				\
    if (pixel < max_pixel) {					\
      to = from + OCTETS_PER_PIXEL;				\
      FS_assign()						\
      if (pixel == pixels - 1 && *to > 0) {			\
        pixels++;						\
        line->length += OCTETS_PER_PIXEL;			\
      }								\
    }								\
                                                                \
    /* 3/16 of the error goes to the left and below */		\
    correction = (3*error)/16;					\
    remaining_error -= correction;				\
    if (pixel > 0) {						\
      to = next_line->str + (pixel - 1)*OCTETS_PER_PIXEL + colorant; \
      FS_assign()						\
      if (next_line->length < pixel*OCTETS_PER_PIXEL && *to > 0) \
        next_line->length = pixel*OCTETS_PER_PIXEL;		\
    }								\
                                                                \
    /* 5/16 of the error goes below */				\
    correction = (5*error)/16;					\
    remaining_error -= correction;				\
    to = next_line->str + pixel*OCTETS_PER_PIXEL + colorant;	\
    FS_assign()							\
    if (next_line->length <= pixel*OCTETS_PER_PIXEL && *to > 0)	\
      next_line->length = (pixel + 1)*OCTETS_PER_PIXEL;		\
                                                                \
    /* The remainder (about 1/16 of the error) is added to the right and */ \
    /* below. */						\
    if (pixel < max_pixel) {					\
      to = next_line->str + (pixel+1)*OCTETS_PER_PIXEL + colorant; \
      new_value = *to + remaining_error;			\
      *to = fit_to_octet(new_value);				\
      if (next_line->length < (pixel + 2)*OCTETS_PER_PIXEL && *to > 0) \
        next_line->length = (pixel + 2)*OCTETS_PER_PIXEL;	\
    }								\
  }

/******************************************************************************

  Function: split_Gray_2

  Floyd-Steinberg error diffusion for the process colour model Gray and
  2 intensity levels.

******************************************************************************/

static void split_Gray_2(eprn_OctetString *line, eprn_OctetString *next_line,
  int max_octets, eprn_OctetString bitplanes[])
{
  const int
    max_pixel = max_octets - 1;
  int
    correction,
    error,
    new_value,
    pixel,
    pixel_mod_8,
    pixels = line->length,
    remaining_error;
  eprn_Octet
    approx,
    *from,
    *ptr,
    *to;

  ptr = bitplanes[0].str;

  /* Loop over pixels in the scan line. Note that 'pixels' may increase
     within the loop. */
  for (pixel = 0, pixel_mod_8 = 8; pixel < pixels; pixel++, pixel_mod_8++) {
    if (pixel_mod_8 == 8) {
      pixel_mod_8 = 0;
      *ptr = 0;
    }

    /* Determine approximation and error for this pixel */
    from = line->str + pixel;
    approx = *from >> 7;	/* take the most significant bit */
    error = *from - 255*approx;
     /* The sign of 'error' is chosen such that 'error' is positive if
        colorant intensity has to be added to the picture. */

    /* Insert the approximation into the output plane */
    *ptr = (*ptr << 1) | approx;

    error_propagation_Gray()

    if (pixel_mod_8 == 7) ptr++;
  }

  eprn_finalize(false, 0, 1, bitplanes, &ptr, pixels);

  return;
}

/******************************************************************************

  Function: split_Gray()

  Floyd-Steinberg error diffusion for the process colour model Gray and an
  arbitrary number of intensity levels.

******************************************************************************/

static void split_Gray(eprn_OctetString *line, eprn_OctetString *next_line,
  int max_octets, unsigned int black_levels, eprn_OctetString bitplanes[])
{
  const int
    max_pixel = max_octets - 1,
    planes = eprn_bits_for_levels(black_levels);
  int
    correction,
    error,
    new_value,
    pixel,
    pixel_mod_8,
    pixels = line->length,
    plane,
    remaining_error;
  eprn_Octet
    approx,
    *from,
    *ptr[8],
    *to;
  const unsigned int
    divisor = 256/black_levels,
    max_level = black_levels - 1;

  for (plane = 0; plane < planes; plane++) ptr[plane] = bitplanes[plane].str;

  /* Loop over pixels in the scan line. Note that 'pixels' may increase
     within the loop. */
  for (pixel = 0, pixel_mod_8 = 8; pixel < pixels; pixel++, pixel_mod_8++) {
    if (pixel_mod_8 == 8) {
      pixel_mod_8 = 0;
      for (plane = 0; plane < planes; plane++) *ptr[plane] = 0;
    }

    /* Determine approximation and error for this pixel */
    from = line->str + pixel;
    approx = *from/divisor;
    error = *from - (255*approx)/max_level;
     /* The sign of 'error' is chosen such that 'error' is positive if
        colorant intensity has to be added to the picture. */

    /* Distribute the approximation over the bit planes */
    for (plane = 0; plane < planes; plane++) {
      *ptr[plane] = (*ptr[plane] << 1) | (approx & 0x01);
      approx >>= 1;
    }

    error_propagation_Gray()

    if (pixel_mod_8 == 7)
      for (plane = 0; plane < planes; plane++) ptr[plane]++;
  }

  eprn_finalize(false, 0, planes, bitplanes, ptr, pixels);

  return;
}

/*****************************************************************************/

/* Index of the black colorant in a pixel value (gx_color_index) for the
   non-monochrome cases */
#define BLACK_INDEX		3

/******************************************************************************

  Function: split_colour_CMYK_2()

  Floyd-Steinberg error diffusion for the CMYK colour model using 2 intensity
  levels for all colorants.

  This function is about 14 % faster than split_colour_at_most_2(), and every
  bit helps.

******************************************************************************/

#define PLANES		4

static void split_colour_CMYK_2(eprn_OctetString *line,
  eprn_OctetString *next_line, int max_octets, eprn_OctetString bitplanes[])
{
  const int
    max_pixel = max_octets/OCTETS_PER_PIXEL - 1;
  int
    colorant,
    correction,
    error,
    new_value,
    pixel,
    pixel_mod_8,
    pixels = line->length/OCTETS_PER_PIXEL,
    plane,
    remaining_error;
  eprn_Octet
    approx,
    *from,
    *ptr[4],
    *to;

  for (plane = 0; plane < PLANES; plane++) ptr[plane] = bitplanes[plane].str;

  /* Loop over pixels in the scan line. Note that 'pixels' may increase
     within the loop. */
  for (pixel = 0, pixel_mod_8 = 8; pixel < pixels; pixel++, pixel_mod_8++) {
    if (pixel_mod_8 == 8) {
      pixel_mod_8 = 0;
      for (plane = 0; plane < PLANES; plane++) *ptr[plane] = 0;
    }

    /* Loop over colorants within a scan line. Remember that the order within
       a pixel is YMCK. */
    for (colorant = BLACK_INDEX; colorant >= 0; colorant--) {
      from = line->str + pixel*OCTETS_PER_PIXEL + colorant;

      /* Determine approximation and error for this pixel */
      approx = *from >> 7;
      error = *from - 255*approx;
       /* The sign of 'error' is chosen such that 'error' is positive if
          colorant intensity has to be added to the picture. */

      /* Insert the approximation in the bit plane */
      plane = BLACK_INDEX - colorant;
      *ptr[plane] = (*ptr[plane] << 1) | approx;

      error_propagation_colour()
    }

    if (pixel_mod_8 == 7)
      for (plane = 0; plane < PLANES; plane++) ptr[plane]++;
  }

  eprn_finalize(false, 2, PLANES, bitplanes, ptr, pixels);

  return;
}

/******************************************************************************

  Function: split_colour_at_most_2()

  Floyd-Steinberg error diffusion for the non-monochrome process colour models
  using 2 intensity levels for the CMY colorants and at most 2 for the black
  colorant.

******************************************************************************/

static void split_colour_at_most_2(eprn_OctetString *line,
  eprn_OctetString *next_line, int max_octets, eprn_ColourModel colour_model,
  eprn_OctetString bitplanes[])
{
  const int
    last_colorant =
      colour_model == eprn_DeviceCMY_plus_K || colour_model == eprn_DeviceCMYK?
        BLACK_INDEX: 2,
    max_pixel = max_octets/OCTETS_PER_PIXEL - 1,
    planes =
      colour_model == eprn_DeviceCMY_plus_K || colour_model == eprn_DeviceCMYK?
        4: 3;
  int
    colorant,
    correction,
    error,
    new_value,
    pixel,
    pixel_mod_8,
    pixels = line->length/OCTETS_PER_PIXEL,
    plane,
    remaining_error;
  eprn_Octet
    approx[4],
    *from,
    *ptr[4],
    *to;

  for (plane = 0; plane < planes; plane++) ptr[plane] = bitplanes[plane].str;

  /* Loop over pixels in the scan line. Note that 'pixels' may increase
     within the loop. */
  for (pixel = 0, pixel_mod_8 = 8; pixel < pixels; pixel++, pixel_mod_8++) {
    if (pixel_mod_8 == 8) {
      pixel_mod_8 = 0;
      for (plane = 0; plane < planes; plane++) *ptr[plane] = 0;
    }

    /* Loop over colorants within a scan line. Remember that the order within
       a pixel is YMCK or BGR-. */
    for (colorant = last_colorant; colorant >= 0; colorant--) {
      from = line->str + pixel*OCTETS_PER_PIXEL + colorant;

      /* Determine approximation and error for this pixel */
      approx[colorant] = *from >> 7;
      error = *from - 255*approx[colorant];
       /* The sign of 'error' is chosen such that 'error' is positive if
          colorant intensity has to be added to the picture. */

      error_propagation_colour()
    }

    /* Determine the black component for CMY+K */
    if (colour_model == eprn_DeviceCMY_plus_K &&
        approx[0] == approx[1] && approx[1] == approx[2] && approx[0] > 0) {
      approx[BLACK_INDEX] = approx[0];
      approx[0] = approx[1] = approx[2] = 0;
    }

    /* Distribute the approximation over the bit planes */
    for (colorant = last_colorant, plane = 0; colorant >= 0;
        colorant--, plane++) {
      *ptr[plane] = (*ptr[plane] << 1) | approx[colorant];
    }

    if (pixel_mod_8 == 7)
      for (plane = 0; plane < planes; plane++) ptr[plane]++;
  }

  eprn_finalize(colour_model == eprn_DeviceRGB, 2, planes, bitplanes, ptr,
    pixels);

  return;
}

/******************************************************************************

  Function: split_colour()

  Floyd-Steinberg error diffusion for the non-monochrome process colour models
  and an arbitrary number of intensity levels.

******************************************************************************/

static void split_colour(eprn_OctetString *line, eprn_OctetString *next_line,
  int max_octets, eprn_ColourModel colour_model,
  unsigned int black_levels, unsigned int non_black_levels,
  eprn_OctetString bitplanes[])
{
  const int
    black_planes = eprn_bits_for_levels(black_levels),
    last_colorant = black_levels > 0? BLACK_INDEX: 2,
    max_pixel = max_octets/OCTETS_PER_PIXEL - 1,
    non_black_planes = eprn_bits_for_levels(non_black_levels),
    planes = black_planes + 3*non_black_planes;
  int
    colorant,
    correction,
    error,
    new_value,
    next_plane[4],
    pixel,
    pixel_mod_8,
    pixels = line->length/OCTETS_PER_PIXEL,
    plane,
    remaining_error;
  eprn_Octet
    approx[4],
    *from,
    *ptr[32],
    *to;
  unsigned int
    divisor[4],
    max_level[4];

  if (black_levels > 0) {
    divisor[BLACK_INDEX] = 256/black_levels;
    max_level[BLACK_INDEX] = black_levels - 1;
  }
  else {
    divisor[BLACK_INDEX] = 0;
    max_level[BLACK_INDEX] = 0;
  }
  next_plane[BLACK_INDEX] = black_planes;

  for (colorant = 0; colorant < BLACK_INDEX; colorant++) {
    divisor[colorant] = 256/non_black_levels;
    max_level[colorant] = non_black_levels - 1;
    next_plane[colorant] = (3 - colorant)*non_black_planes + black_planes;
  }

  for (plane = 0; plane < planes; plane++) ptr[plane] = bitplanes[plane].str;

  /* Loop over pixels in the scan line. Note that 'pixels' may increase
     within the loop. */
  for (pixel = 0, pixel_mod_8 = 8; pixel < pixels; pixel++, pixel_mod_8++) {
    if (pixel_mod_8 == 8) {
      pixel_mod_8 = 0;
      for (plane = 0; plane < planes; plane++) *ptr[plane] = 0;
    }

    /* Loop over colorants within a scan line */
    for (colorant = last_colorant; colorant >= 0; colorant--) {
      from = line->str + pixel*OCTETS_PER_PIXEL + colorant;

      /* Determine approximation and error for this pixel */
      approx[colorant] = *from/divisor[colorant];
      error = *from - (255*approx[colorant])/max_level[colorant];
       /* The sign of 'error' is chosen such that 'error' is positive if
          colorant intensity has to be added to the picture. */

      error_propagation_colour()
    }

    /* Determine the black component for CMY+K */
    if (colour_model == eprn_DeviceCMY_plus_K &&
        approx[0] == approx[1] && approx[1] == approx[2] && approx[0] > 0) {
      int value = approx[0]*(black_levels - 1);
      if (value % (non_black_levels - 1) == 0) {
        /* Black does have a level at the same intensity as the CMY levels */
        approx[BLACK_INDEX] = value/(non_black_levels - 1);
        approx[0] = approx[1] = approx[2] = 0;
      }
    }

    /* Distribute the approximation over the bit planes */
    plane = 0;
    for (colorant = last_colorant; colorant >= 0; colorant--) {
      while (plane < next_plane[colorant]) {
        *ptr[plane] = (*ptr[plane] << 1) | (approx[colorant] & 0x01);
        approx[colorant] >>= 1;
        plane++;
      }
    }

    if (pixel_mod_8 == 7) {
      int j;
      for (j = 0; j < planes; j++) ptr[j]++;
    }
  }

  eprn_finalize(colour_model == eprn_DeviceRGB, non_black_levels, planes,
    bitplanes, ptr, pixels);

  return;
}

/******************************************************************************

  Function: eprn_split_FS

  This function performs Floyd-Steinberg error diffusion on a scan line
  and returns the result as bitplanes.

  'line' points to the scan line to be split, 'next_line' to the following one.
  Both lines will be modified by this process. This modification assumes that
  the function is called successively for all lines, starting with the first.
  All octets up to 'max_octets' must be available in the input lines and, as
  far as they have not been included in the length fields, must be zero.
  The parameter 'colour_model' specifies the process colour model used.
  'black_levels' is the number of intensity levels for the black colorant,
  'non_black_levels' the corresponding number for the other colorants.
  'bitplanes' is an array of bitplanes into which the result will be stored
  in the usual format.

******************************************************************************/

void eprn_split_FS(eprn_OctetString *line, eprn_OctetString *next_line,
  int max_octets, eprn_ColourModel colour_model,
  unsigned int black_levels, unsigned int non_black_levels,
  eprn_OctetString bitplanes[])
{
  if (colour_model == eprn_DeviceGray) {
    if (black_levels == 2)
      split_Gray_2(line, next_line, max_octets, bitplanes);
    else
      split_Gray(line, next_line, max_octets, black_levels, bitplanes);
  }
  else if (colour_model == eprn_DeviceCMYK &&
      black_levels == 2 && non_black_levels == 2)
    split_colour_CMYK_2(line, next_line, max_octets, bitplanes);
  else {
    if (black_levels <= 2 && non_black_levels == 2)
      split_colour_at_most_2(line, next_line, max_octets, colour_model,
        bitplanes);
    else
      split_colour(line, next_line, max_octets, colour_model, black_levels,
        non_black_levels, bitplanes);
  }

  return;
}
