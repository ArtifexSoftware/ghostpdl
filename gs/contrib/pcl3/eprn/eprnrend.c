/******************************************************************************
  File:     $Id: eprnrend.c,v 1.15 2001/08/01 05:12:56 Martin Rel $
  Contents: Colour rendering functionality for the ghostscript device 'eprn'
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany. E-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*									      *
*	Copyright (C) 2000, 2001 by Martin Lottermoser			      *
*	All rights reserved						      *
*									      *
*******************************************************************************

  Preprocessor variables:

    EPRN_TRACE
	Define this to enable tracing. Only useful for development.

    EPRN_TRAILING_BIT_BUG_FIXED
	Define this to deactivate compensation for a bug in ghostscript which
	leads to the last pixel in an RGB line being black instead of white.
	This occurs at least in gs 6.01 and 6.50. The correction covers only
	the one-bit-per-colorant case and is equivalent to clipping the pixel.

*******************************************************************************

  The eprn device uses 'gx_color_index' values with varying interpretations,
  depending on the colour model and the rendering method, and stores them at
  different pixmaps depths, normally using the smallest depth which can
  accommodate all colorants at the same number of bits per colorant.

  To simplify matters, a field for the black component is always included, even
  for RGB and CMY, i.e., there are either 1 or 4 bit fields in a
  'gx_color_index' value. If there are 4, the interpretation is either YMCK or
  BGRK, looking from left to right (most to least significant). The width of
  the fields can be found in the 'bits_per_colorant' variable in the eprn part
  of the device instance.

  Within each colorant field, not all bits need be used. Except when using the
  *_max() colour mapping functions, the values returned by
  eprn_bits_for_levels() for the parameters 'black_levels' and
  'non_black_levels' determine the number of bits which are actually
  meaningful. Only the last (least significant) bits are used.

******************************************************************************/

/* Configuration management identification */
#ifndef lint
static const char
  cm_id[] = "@(#)$Id: eprnrend.c,v 1.15 2001/08/01 05:12:56 Martin Rel $";
#endif

/*****************************************************************************/

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE	500
#endif

/* Special Aladdin header, must be included before <sys/types.h> on some
   platforms (e.g., FreeBSD). */
#include "std.h"

/* Standard headers */
#include <assert.h>
#include <stdlib.h>

/* Ghostscript headers */
#ifdef EPRN_TRACE
#include "gdebug.h"
#endif	/* EPRN_TRACE */

/* Special headers */
#include "gdeveprn.h"

/*****************************************************************************/

/* Macros for 'gx_color_index' values used mainly for non-monochrome modes and
   a pixmap depth of 4 */

/* Colorants bits, numbered from 0 on the right to 3 on the left */
#define COLORANT_0_BIT	1U
#define COLORANT_1_BIT	2U
#define COLORANT_2_BIT	4U
#define COLORANT_3_BIT	8U

/* Alias names for the bits in particular colour models */
#define BLACK_BIT	COLORANT_0_BIT
#define CYAN_BIT	COLORANT_1_BIT
#define MAGENTA_BIT	COLORANT_2_BIT
#define YELLOW_BIT	COLORANT_3_BIT
#define RED_BIT		COLORANT_1_BIT
#define GREEN_BIT	COLORANT_2_BIT
#define BLUE_BIT	COLORANT_3_BIT

/* Bit plane indices for splitting */
#define COLORANT_0_INDEX	0
#define COLORANT_1_INDEX	1
#define COLORANT_2_INDEX	2
#define COLORANT_3_INDEX	3

/*  Macro to extract the dominant 8 bits from a 'gx_color_value'. This
    definition assumes that 'gx_color_value' uses the least significant 16 bits
    of the underlying type (unsigned short). Splitting this part off looks
    inefficient because left shifts will usually follow, but I'm relying on the
    compiler to be sufficiently intelligent to eliminate this inefficiency.
    This way the code is easier to check.
    The type cast is needed to prevent problems with negative values on
    platforms where 'gx_color_index' has more bits than 'int'.
 */
#define dominant_8bits(value)	((unsigned int)((value) >> 8))

/******************************************************************************

  Function: eprn_number_of_bitplanes

  Total number of bit planes returned by eprn_get_planes().
  This value is constant while the device is open.

******************************************************************************/

unsigned int eprn_number_of_bitplanes(eprn_Device *dev)
{
  return dev->eprn.output_planes;
}

/******************************************************************************

  Function: eprn_number_of_octets

  Maximal lengths, in terms of the number of 'eprn_Octet' instances, for each
  bit plane returned by eprn_get_planes() for this device. These values
  are constant while the device is open.

******************************************************************************/

void eprn_number_of_octets(eprn_Device *dev, unsigned int lenghts[])
{
  unsigned int j, length;

  length = (dev->eprn.octets_per_line + dev->color_info.depth - 1)/
      dev->color_info.depth;
   /* This results in length >= ceiling((number of pixels per line)/8)
      because:
	      8 * octets_per_line >= pixels_per_line * depth
	<==>  octets_per_line/depth >= pixels_per_line/8
      where division is to be understood as exact.
    */

  for (j = 0; j < dev->eprn.output_planes; j++) lenghts[j] = length;

  return;
}

/******************************************************************************

  Function: eprn_map_rgb_color_for_RGB

  Colour mapping function for the process colour model 'DeviceRGB' and
  2 intensity levels per colorant.

******************************************************************************/

gx_color_index eprn_map_rgb_color_for_RGB(gx_device *device,
  const gx_color_value cv[])
{
  gx_color_value red = cv[0], green = cv[1], blue = cv[2];
  static const gx_color_value half = gx_max_color_value/2;
  gx_color_index value = 0;
  const eprn_Device *dev = (eprn_Device *)device;

#ifdef EPRN_TRACE
  if_debug3(EPRN_TRACE_CHAR,
    "! eprn_map_rgb_color_for_RGB() called for RGB = (%hu, %hu, %hu),\n",
    red, green, blue);
#endif

  assert(dev->eprn.colour_model == eprn_DeviceRGB);

  if (red   > half) value |= RED_BIT;
  if (green > half) value |= GREEN_BIT;
  if (blue  > half) value |= BLUE_BIT;

#ifdef EPRN_TRACE
  if_debug1(EPRN_TRACE_CHAR, "  returning 0x%lX.\n", (unsigned long)value);
#endif
  return value;
}

/******************************************************************************

  Function: eprn_map_rgb_color_for_CMY_or_K

  Colour mapping function for the native colour spaces DeviceGray and DeviceRGB
  and a process colour model using a selection of CMYK colorants with at most
  2 intensity levels per colorant. This function must not be called for the
  process colour models 'DeviceRGB' and 'DeviceCMYK'.

******************************************************************************/

gx_color_index eprn_map_rgb_color_for_CMY_or_K(gx_device *device,
  const gx_color_value cv[])
{
  gx_color_value red = cv[0], green = cv[1], blue = cv[2];
  static const gx_color_value half = gx_max_color_value/2;
  gx_color_index value = (CYAN_BIT | MAGENTA_BIT | YELLOW_BIT);
  const eprn_Device *dev = (eprn_Device *)device;

#ifdef EPRN_TRACE
  if_debug3(EPRN_TRACE_CHAR,
    "! eprn_map_rgb_color_for_CMY_or_K() called for RGB = (%hu, %hu, %hu),\n",
    red, green, blue);
#endif

  assert(dev->eprn.colour_model == eprn_DeviceGray && red == green &&
      green == blue && (blue == 0 || blue == gx_max_color_value) ||
    dev->eprn.colour_model == eprn_DeviceCMY ||
    dev->eprn.colour_model == eprn_DeviceCMY_plus_K);

  /* Map to CMY */
  if (red   > half) value &= ~CYAN_BIT;
  if (green > half) value &= ~MAGENTA_BIT;
  if (blue  > half) value &= ~YELLOW_BIT;

  /* Remap composite black to true black if available */
  if (dev->eprn.colour_model != eprn_DeviceCMY &&
      value == (CYAN_BIT | MAGENTA_BIT | YELLOW_BIT))
    value = BLACK_BIT;

#ifdef EPRN_TRACE
  if_debug1(EPRN_TRACE_CHAR, "  returning 0x%lX.\n", (unsigned long)value);
#endif
  return value;
}

/******************************************************************************

  Function: eprn_map_rgb_color_for_RGB_flex

  This is a 'map_rgb_color' method for the process colour model 'DeviceRGB'
  supporting any number of intensity levels.

******************************************************************************/

gx_color_index eprn_map_rgb_color_for_RGB_flex(gx_device *device,
  const gx_color_value cv[])
{
  gx_color_value red = cv[0], green = cv[1], blue = cv[2];
  gx_color_index value = 0;
  gx_color_value step;
  unsigned int level;
  const eprn_Eprn *eprn = &((eprn_Device *)device)->eprn;

#ifdef EPRN_TRACE
  if_debug3(EPRN_TRACE_CHAR,
    "! eprn_map_rgb_color_for_RGB_flex() called for RGB = (%hu, %hu, %hu),\n",
    red, green, blue);
#endif

  /* See the discussion in eprn_map_cmyk_color_flex() below. */

  step = gx_max_color_value/eprn->non_black_levels;

  /* The order has to be BGR from left to right */
  level = blue/step;
  if (level >= eprn->non_black_levels) level = eprn->non_black_levels - 1;
  value = level << eprn->bits_per_colorant;
  level = green/step;
  if (level >= eprn->non_black_levels) level = eprn->non_black_levels - 1;
  value = (value | level) << eprn->bits_per_colorant;
  level = red/step;
  if (level >= eprn->non_black_levels) level = eprn->non_black_levels - 1;
  value = (value | level) << eprn->bits_per_colorant;

#ifdef EPRN_TRACE
  if_debug1(EPRN_TRACE_CHAR, "  returning 0x%lX.\n", (unsigned long)value);
#endif
  return value;
}

/******************************************************************************

  Function: eprn_map_rgb_color_for_CMY_or_K_flex

  This is a flexible 'map_rgb_color' method. It must not be called for the
  process colour models 'DeviceRGB' and 'DeviceCMYK'.

******************************************************************************/

gx_color_index eprn_map_rgb_color_for_CMY_or_K_flex(gx_device *device,
  const gx_color_value cv[])
{
  gx_color_value red = cv[0], green = cv[1], blue = cv[2];
  const eprn_Device *dev = (eprn_Device *)device;

#ifdef EPRN_TRACE
  if_debug3(EPRN_TRACE_CHAR,
    "! eprn_map_rgb_color_for_CMY_or_K_flex() called for "
      "RGB = (%hu, %hu, %hu).\n",
    red, green, blue);
#endif

  /* Treat pure grey levels differently if we have black. This implies that for
     CMY+K only "true" grey shades will be printed with black ink, all others
     will be mixed from CMY. */
  gx_color_value tmpcv[4];
  if (dev->eprn.colour_model != eprn_DeviceCMY && red == green && green == blue) {
    tmpcv[0] = 0; tmpcv[1] = 0; tmpcv[2] = 0;
    tmpcv[3] = gx_max_color_value - red;
    return eprn_map_cmyk_color_flex(device, tmpcv);

  }
  tmpcv[0] = gx_max_color_value - red; 
  tmpcv[1] = gx_max_color_value - green; 
  tmpcv[2] = gx_max_color_value - blue; 
  tmpcv[3] = 0;
  return eprn_map_cmyk_color_flex(device, tmpcv);
}

/******************************************************************************

  Function: eprn_map_rgb_color_for_RGB_max

  Colour mapping function for the process colour model 'DeviceRGB' retaining as
  much information as possible.

******************************************************************************/

gx_color_index eprn_map_rgb_color_for_RGB_max(gx_device *device,
  const gx_color_value cv[])
{
  gx_color_value red = cv[0], green = cv[1], blue = cv[2];
  gx_color_index value;

#ifdef EPRN_TRACE
  if_debug3(EPRN_TRACE_CHAR,
    "! eprn_map_rgb_color_for_RGB_max() called for RGB = (%hu, %hu, %hu),\n",
    red, green, blue);
#endif

  value  = dominant_8bits(red)   <<  8;
  value |= dominant_8bits(green) << 16;
  value |= dominant_8bits(blue)  << 24;

#ifdef EPRN_TRACE
  if_debug1(EPRN_TRACE_CHAR, "  returning 0x%08lX.\n", (unsigned long)value);
#endif
  return value;
}

/******************************************************************************

  Function: eprn_map_rgb_color_for_CMY_or_K_max

  "Maximal" colour mapping function for the process colour models "DeviceGray",
  "DeviceCMY", and "CMY+K".

******************************************************************************/

gx_color_index eprn_map_rgb_color_for_CMY_or_K_max(gx_device *device,
  const gx_color_value cv[])
{
  gx_color_value red = cv[0], green = cv[1], blue = cv[2];
  const eprn_Device *dev = (eprn_Device *)device;

#ifdef EPRN_TRACE
  if_debug3(EPRN_TRACE_CHAR,
    "! eprn_map_rgb_color_for_CMY_or_K_max() called for "
      "RGB = (%hu, %hu, %hu).\n",
    red, green, blue);
#endif

  gx_color_value tmpcv[4];
  if (dev->eprn.colour_model == eprn_DeviceGray) {
    tmpcv[0] = 0; tmpcv[1] = 0; tmpcv[2] = 0;
    tmpcv[3] = gx_max_color_value - red;
    return eprn_map_cmyk_color_max(device, tmpcv);
  }
  /* Note that the conversion from composite black to true black for CMY+K can
     only happen at the output pixel level, not here. */
  tmpcv[0] = gx_max_color_value - red; 
  tmpcv[1] = gx_max_color_value - green; 
  tmpcv[2] = gx_max_color_value - blue; 
  tmpcv[3] = 0;
  return eprn_map_cmyk_color_max(device, tmpcv);
}

/******************************************************************************

  Function: eprn_map_color_rgb

  This function is a 'map_color_rgb' method.

  Such a method should return an RGB triple for the specified 'color'. This is
  apparently intended for devices supporting colour maps.

  The function param_HWColorMap() in gsdparam.c tries to compile such a
  colour map by calling this method repeatedly for all values from 0 to
  2^color_info.depth - 1, provided the depth is less than or equal to 8 and
  the process colour model is not DeviceCMYK. If the function for one of these
  values returns a negative code, the assumption seems to be that there is no
  such colour map. Because there is a default method which always returns zero,
  such a colour map is always constructed if a device does not implement its
  own 'map_color_rgb' method. This can be seen in the appearance of the
  "HWColorMap" page device parameter.

  The key purpose of this function is therefore to return a negative code.

******************************************************************************/

int eprn_map_color_rgb(gx_device *device, gx_color_index color,
  gx_color_value rgb[])
{
#ifdef EPRN_TRACE
  if_debug1(EPRN_TRACE_CHAR,
    "! eprn_map_color_rgb() called for 0x%lX.\n", (unsigned long)color);
#endif

  /* Just to be safe we return defined values (white) */
  if (((eprn_Device *)device)->eprn.colour_model == eprn_DeviceRGB)
    rgb[0] = rgb[1] = rgb[2] = gx_max_color_value;
  else
    rgb[0] = rgb[1] = rgb[2] = 0;

  return -1;
}

/******************************************************************************

  Function: eprn_map_cmyk_color

  Colour mapping function for a process colour model of 'DeviceCMYK' with
  2 intensity levels for all colorants.

******************************************************************************/

gx_color_index eprn_map_cmyk_color(gx_device *device,
  const gx_color_value cv[])
{
  gx_color_value cyan = cv[0], magenta = cv[1], yellow = cv[2], black = cv[3];
  gx_color_index value = 0;
  static const gx_color_value threshold = gx_max_color_value/2;

#ifdef EPRN_TRACE
  if_debug4(EPRN_TRACE_CHAR,
    "! eprn_map_cmyk_color() called for CMYK = (%hu, %hu, %hu, %hu),\n",
      cyan, magenta, yellow, black);
#endif

  if (cyan    > threshold) value |= CYAN_BIT;
  if (magenta > threshold) value |= MAGENTA_BIT;
  if (yellow  > threshold) value |= YELLOW_BIT;
  if (black   > threshold) value |= BLACK_BIT;

#ifdef EPRN_TRACE
  if_debug1(EPRN_TRACE_CHAR, "  returning 0x%lX.\n", (unsigned long)value);
#endif
  return value;
}

/******************************************************************************

  Function: eprn_map_cmyk_color_flex

  This is a 'map_cmyk_color' method supporting arbitrary numbers of intensity
  levels. It may be called for every colour model except DeviceRGB. Colorants
  not present in the model will be ignored.

******************************************************************************/

gx_color_index eprn_map_cmyk_color_flex(gx_device *device,
  const gx_color_value cv[])
{
  gx_color_value cyan = cv[0], magenta = cv[1], yellow = cv[2], black = cv[3];
  gx_color_index value = 0;
  gx_color_value step;
  unsigned int level;
  const eprn_Eprn *eprn = &((eprn_Device *)device)->eprn;

#ifdef EPRN_TRACE
  if_debug4(EPRN_TRACE_CHAR,
    "! eprn_map_cmyk_color_flex() called for CMYK = (%hu, %hu, %hu, %hu),\n",
      cyan, magenta, yellow, black);
#endif

  /*  I can think of three linear methods to extract discrete values from a
      continuous intensity in the range [0, 1]:
      (a) multiply by the number of levels minus 1 and truncate,
      (b) multiply by the number of levels minus 1 and round, and
      (c) multiply by the number of levels and truncate, except for an
	  intensity of 1 in which case one returns the number of levels minus 1.
      For intensity values which can be represented exactly, i.e.,

	intensity = i/(levels-1)	for some non-negative i < levels,

      these three methods are identical. (a) is however inappropriate here
      because for less than 32 levels ghostscript already provides intensity
      values which have been adjusted to a representable level. A rounding
      error could now result in a level which is too small by one. I prefer (c)
      because it gives equal shares to all levels.

      I'm using integer arithmetic here although floating point numbers would
      be more accurate. This routine may, however, be called quite frequently,
      and the loss in accuray is acceptable as long as the values determined
      for 'step' are large compared to the number of levels. If you consider
      "large" as meaning "10 times as large", the critical boundary is at about
      81 levels. The highest number of intensity levels at present supported by
      HP DeskJets is apparently 4.

      A more accurate implementation would determine 'step' as a floating point
      value, divide the intensity by it, and take the floor (entier) of the
      result as the component intensity.
  */

  /* The order has to be (YMC)(K) from left to right */
  if (eprn->colour_model != eprn_DeviceGray) {
    step = gx_max_color_value/eprn->non_black_levels;

    level = yellow/step;
    if (level >= eprn->non_black_levels) level = eprn->non_black_levels - 1;
    value = level << eprn->bits_per_colorant;
    level = magenta/step;
    if (level >= eprn->non_black_levels) level = eprn->non_black_levels - 1;
    value = (value | level) << eprn->bits_per_colorant;
    level = cyan/step;
    if (level >= eprn->non_black_levels) level = eprn->non_black_levels - 1;
    value = (value | level) << eprn->bits_per_colorant;
  }
  if (eprn->colour_model != eprn_DeviceCMY) {
    step = gx_max_color_value/eprn->black_levels;
    level = black/step;
    if (level >= eprn->black_levels) level = eprn->black_levels - 1;
    value |= level;
  }

#ifdef EPRN_TRACE
  if_debug1(EPRN_TRACE_CHAR, "  returning 0x%lX.\n", (unsigned long)value);
#endif
  return value;
}

/******************************************************************************

  Function: eprn_map_cmyk_color_max

  This is a 'map_cmyk_color' method retaining as much colour information as
  possible. The reduction to the printer's capabilities must happen in the
  output routine.

******************************************************************************/

gx_color_index eprn_map_cmyk_color_max(gx_device *device,
  const gx_color_value cv[])
{
  gx_color_value cyan = cv[0], magenta = cv[1], yellow = cv[2], black = cv[3];
  gx_color_index value;

#ifdef EPRN_TRACE
  if_debug4(EPRN_TRACE_CHAR,
    "! eprn_map_cmyk_color_max() called for CMYK = (%hu, %hu, %hu, %hu),\n",
      cyan, magenta, yellow, black);
#endif

  value = dominant_8bits(black);
  value |= dominant_8bits(cyan)    <<  8;
  value |= dominant_8bits(magenta) << 16;
  value |= dominant_8bits(yellow)  << 24;

#ifdef EPRN_TRACE
  if_debug1(EPRN_TRACE_CHAR, "  returning 0x%08lX.\n", (unsigned long)value);
#endif
  return value;
}

/******************************************************************************

  Function: eprn_map_cmyk_color_glob

  Global eprn_map_cmyk_color for all cases handled above used to
  be able to work together with ghostscript 8.15 and CMYK model.

******************************************************************************/
gx_color_index eprn_map_cmyk_color_glob(gx_device *device, const gx_color_value cv[])
{
  const eprn_Eprn *eprn = &((eprn_Device *)device)->eprn;
  if (eprn->intensity_rendering == eprn_IR_FloydSteinberg)
    return eprn_map_cmyk_color_max(device, cv);
  else if (device->color_info.max_gray > 1 || device->color_info.max_color > 1)
    return eprn_map_cmyk_color_flex(device, cv);
  else
    return eprn_map_cmyk_color(device, cv);
}

/******************************************************************************

  Function: eprn_finalize

  This function fills the last octet in a set of bit planes with white if
  needed and sets the lengths of all these planes.

  'is_RGB' denotes whether the colour model is eprn_DeviceRGB,
  'non_black_levels' is the number of intensity levels for non-black colorants,
  'planes' the number of bit planes,
  'plane' points to an array of at least 'planes' bit planes,
  'ptr' points to an array of at least 'planes' pointers into the bit planes,
  'pixels' is the number of pixels written to the group of bit planes.

  If 'pixels' is divisible by 8, the 'ptr' pointers point to octets after
  the last completely written octets. Otherwise, they point to the last
  incompletely written octet in which the pixels written so far occupy the
  least significant bits.

******************************************************************************/

void eprn_finalize(bool is_RGB, unsigned int non_black_levels,
  int planes, eprn_OctetString *plane, eprn_Octet **ptr, int pixels)
{
  int j;

  /* Execute remaining left shifts in the last octet of the output planes when
     the number of pixels is not a multiple of 8, and fill with white on the
     right */
  if (pixels % 8 != 0) {
    int shift = 8 - pixels % 8;

    if (is_RGB) {
      /* White may be any intensity, but it's the same for all three colorants,
	 and it's the highest. */
      eprn_Octet imax = non_black_levels - 1;
      int c, rgb_planes = eprn_bits_for_levels(non_black_levels);

      j = 0;	/* next output plane */

      /* Loop over RGB */
      for (c = 0; c < 3; c++) {
	eprn_Octet value = imax;
	int m;

	/* Loop over all planes for this colorant */
	for (m = 0; m < rgb_planes; m++, j++) {
	  eprn_Octet bit = value & 1;
	  int p;

	  value = value >> 1;

	  /* Put the bit into all remaining pixels for this plane */
	  for (p = 0; p < shift; p++)
	    *ptr[j] = (*ptr[j] << 1) | bit;
	}
      }
    }
    else /* White is zero */
      for (j = 0; j < planes; j++)
	*ptr[j] = *ptr[j] << shift;

    /* Advance all plane pointers by 1 */
    for (j = 0; j < planes; j++) ptr[j]++;
  }

  /* Set the lengths of the bit plane strings */
  for (j = 0; j < planes; j++) {
    if (pixels == 0) plane[j].length = 0;
    else plane[j].length = ptr[j] - plane[j].str;
  }

  return;
}

/******************************************************************************

  Function: split_line_le8

  This is the first of the "split_line implementations". See the body of
  eprn_get_planes() for the calling conventions common to them.

  This particular implementation has the following restrictions:
  - The pixmap depth must be a divisor of 8.

******************************************************************************/

static void split_line_le8(eprn_Device *dev, const eprn_Octet *line,
  int length, eprn_OctetString plane[])
{
  gx_color_index
    pixel;
  int
    black_planes,	/* number of planes to send for black */
    non_black_planes,	/* number of planes to send for each of CMY/RGB */
    j,
    k,
    pixels,		/* number of pixels transferred to bit planes */
    planes;		/* number of planes to send */
  eprn_Octet
    comp_mask = 0,	/* 'bits_per_component' 1s in the lowest part */
    pixel_mask = 0,	/* 'depth' 1s in the lowest part */
    *ptr[8];		/* pointers into planes (next octet to write to) */

  /* Number of planes to send */
  black_planes = eprn_bits_for_levels(dev->eprn.black_levels);
  non_black_planes = eprn_bits_for_levels(dev->eprn.non_black_levels);
  planes = black_planes + 3*non_black_planes;

  /* Initialize the bit plane pointers */
  for (j = 0; j < planes; j++) ptr[j] = plane[j].str;

  /* Determine some bit masks */
  for (j = 0; j < dev->color_info.depth; j++)
    pixel_mask = (pixel_mask << 1) | 1;
  for (j = 0; j < dev->eprn.bits_per_colorant; j++)
    comp_mask = (comp_mask << 1) | 1;

  /* Copy from 'line' to 'plane[]', converting Z format to XY format */
  pixels = 0;
  k = 0; /* Next octet index in the input line */
  while (k < length) {
    int l, m, p;

    /* Initialize plane storage if it's a new output octet */
    if (pixels % 8 == 0) for (j = 0; j < planes; j++) *ptr[j] = 0;

    /* Loop over pixels within the input octet, starting at the leftmost
       pixel (highest-order bits) */
    p = 8/dev->color_info.depth - 1;
    do {
      eprn_Octet comp;

      /* Extract pixel */
      pixel = (line[k] >> p*dev->color_info.depth) & pixel_mask;

      /* Extract components from the pixel and distribute each over its planes
       */
      comp = pixel & comp_mask;	/* black */
      for (j = 0; j < black_planes; j++) {
	*ptr[j] = (*ptr[j] << 1) | comp & 1;
	comp >>= 1;
      }
      if (non_black_planes > 0) for (l = 1; l < 4; l++) {
	comp = (pixel >> l*dev->eprn.bits_per_colorant) & comp_mask;
	for (m = 0; m < non_black_planes; m++, j++) {
	  *ptr[j] = (*ptr[j] << 1) | comp & 1;
	  comp >>= 1;
	}
      }

      pixels++;
      p--;
    } while (p >= 0);
    k++;

    /* Increase plane pointers if an output octet boundary has been reached */
    if (pixels % 8 == 0) for (j = 0; j < planes; j++) ptr[j]++;
  }

  eprn_finalize(dev->eprn.colour_model == eprn_DeviceRGB,
    dev->eprn.non_black_levels, planes, plane, ptr, pixels);

  return;
}

/******************************************************************************

  Function: split_line_ge8

  This split_line function has the following restrictions:
  - The pixmap depth must be a multiple of 8.
  - There may be at most 8 bits per colorant.
    (Hence the depth is at most 32.)
  - 'length' must be divisible by depth/8.

******************************************************************************/

static void split_line_ge8(eprn_Device *dev, const eprn_Octet *line,
  int length, eprn_OctetString plane[])
{
  gx_color_index
    pixel;
  int
    black_planes,	/* number of planes to send for black */
    non_black_planes,	/* number of planes to send for each of CMY/RGB */
    j,
    k,
    octets_per_pixel = dev->color_info.depth/8,
    pixels,		/* number of pixels transferred to bit planes */
    planes;		/* number of planes to send */
  eprn_Octet
    comp_mask = 0,	/* bits_per_component 1s in the lowest part */
    *ptr[32];		/* pointers into planes (next octet to write to) */

  /* Number of planes to send */
  black_planes = eprn_bits_for_levels(dev->eprn.black_levels);
  non_black_planes = eprn_bits_for_levels(dev->eprn.non_black_levels);
  planes = black_planes + 3*non_black_planes;

  /* Initialize the bit plane pointers */
  for (j = 0; j < planes; j++) ptr[j] = plane[j].str;

  /* Determine the component mask */
  for (j = 0; j < dev->eprn.bits_per_colorant; j++)
    comp_mask = (comp_mask << 1) | 1;

  /* Copy from 'line' to 'plane[]', converting Z format to XY format */
  pixels = 0;
  k = 0; /* Next octet index in the input line */
  while (k < length) {
    eprn_Octet comp;
    int l, m;

    /* Initialize plane storage if it's a new octet */
    if (pixels % 8 == 0) for (j = 0; j < planes; j++) *ptr[j] = 0;

    /* Reconstruct pixel from several octets */
    j = 0;
    pixel = line[k];
    do {
      j++; k++;
      if (j >= octets_per_pixel) break;
      pixel = (pixel << 8) | line[k];	/* MSB (Big Endian) */
    } while (1);

    /* Split and distribute over planes */
    comp = pixel & comp_mask;	/* black */
    for (j = 0; j < black_planes; j++) {
      *ptr[j] = (*ptr[j] << 1) | comp & 1;
      comp >>= 1;
    }
    for (l = 1; l < 4; l++) {
      comp = (pixel >> l*dev->eprn.bits_per_colorant) & comp_mask;
      for (m = 0; m < non_black_planes; m++, j++) {
	*ptr[j] = (*ptr[j] << 1) | comp & 1;
	comp >>= 1;
      }
    }

    pixels++;

    /* Increase plane pointers if an octet boundary has been reached */
    if (pixels % 8 == 0) for (j = 0; j < planes; j++) ptr[j]++;
  }

  eprn_finalize(dev->eprn.colour_model == eprn_DeviceRGB,
    dev->eprn.non_black_levels, planes, plane, ptr, pixels);

  return;
}

/******************************************************************************

  Function: split_line_3or4x1

  This function is a split_line() implementation for a non-monochrome colour
  model (3 or 4 components) with 1 bit per component.

******************************************************************************/

static void split_line_3or4x1(eprn_Device *dev, const eprn_Octet *line,
  int length, eprn_OctetString plane[])
{
  int
    from = (dev->eprn.colour_model == eprn_DeviceRGB ||
      dev->eprn.colour_model == eprn_DeviceCMY? 1: 0),
    j,
    k,
    l;
  eprn_Octet *ptr[4];	/* pointers into planes, indexed KCMY/-RGB */

  ptr[0] = NULL;	/* defensive programming */
  for (j = from; j < 4; j++) ptr[j] = plane[j-from].str;

  /*  Loop over the input line, taking four octets (8 pixels) at a time, as far
      as available, and split them into four output octets, one for each
      colorant.
  */
  k = 0;
  while (k < length) {
    eprn_Octet octet[4] = {0, 0, 0, 0};

    for (l = 0; l < 4 && k < length; l++, k++) {
      eprn_Octet part;
#define treat_quartet()						\
      octet[COLORANT_0_INDEX] <<= 1;				\
      if (part & COLORANT_0_BIT) octet[COLORANT_0_INDEX] |= 1;	\
      octet[COLORANT_1_INDEX] <<= 1;				\
      if (part & COLORANT_1_BIT) octet[COLORANT_1_INDEX] |= 1;	\
      octet[COLORANT_2_INDEX] <<= 1;				\
      if (part & COLORANT_2_BIT) octet[COLORANT_2_INDEX] |= 1;	\
      octet[COLORANT_3_INDEX] <<= 1;				\
      if (part & COLORANT_3_BIT) octet[COLORANT_3_INDEX] |= 1;

      /* Upper four bits */
      part = (line[k] >> 4) & 0x0F;
      treat_quartet()

      /* Lower four bits */
      part = line[k] & 0x0F;
      treat_quartet()

#undef treat_quartet
    }
    if (l < 4) {
      for (j = from; j < 4; j++) octet[j] <<= 8 - 2*l;
      if (dev->eprn.colour_model == eprn_DeviceRGB) {
	/* Add white in the last 8 - 2*l pixels */
	for (j = 1; j < 4; j++) {
	  int k;
	  /* We add two pixels at a time */
	  for (k = 3 - l; k >= 0; k--) octet[j] |= 0x03 << k;
	}
      }
    }
    for (j = from; j < 4; j++) *(ptr[j]++) = octet[j];
  }

  /* Set the lengths of the bit plane strings */
  for (j = 0; j < dev->eprn.output_planes; j++) {
    if (length == 0) plane[j].length = 0;
    else plane[j].length = ptr[from + j] - plane[j].str;
  }

  return;
}

/******************************************************************************

  Function: split_line_4x2

  This is a split_line() implementation for 4 colour components (i.e., CMYK)
  with 3 or 4 levels each (2 bit planes to send for each component).

******************************************************************************/

static void split_line_4x2(eprn_Device *dev, const eprn_Octet *line,
  int length, eprn_OctetString plane[])
{
  gx_color_index
    pixel;
  int
    j,
    k;
  eprn_Octet
    *ptr[8];		/* pointers into planes (next octet to write to) */

  /* Initialize the bit plane pointers */
  for (j = 0; j < 8; j++) ptr[j] = plane[j].str;

  /* Copy from 'line' to 'plane[]', converting Z format to XY format */
  for (k = 0; k < length; k++) {
    /* k is the index of the next octet in the input line and the number of
       pixels processed so far. */

    /* Initialize plane storage if it's a new octet */
    if (k % 8 == 0) for (j = 0; j < 8; j++) *ptr[j] = 0;

    /* Fetch pixel */
    pixel = line[k];

    /* Split and distribute over planes */
    *ptr[0] = (*ptr[0] << 1) | pixel & 0x01;
#define assign_bit(index) \
	*ptr[index] = (*ptr[index] << 1) | (pixel >> index) & 0x01
    assign_bit(1);
    assign_bit(2);
    assign_bit(3);
    assign_bit(4);
    assign_bit(5);
    assign_bit(6);
    assign_bit(7);
#undef assign_bit

    /* Increase plane pointers if an output octet boundary has been reached */
    if (k % 8 == 7) for (j = 0; j < 8; j++) ptr[j]++;
  }

  /* Execute remaining left shifts in the last octet of the output planes when
     the number of pixels is not a multiple of 8 */
  k = length % 8;
  if (k != 0) {
    int shift = 8 - k;
    for (j = 0; j < 8; j++)
      *(ptr[j]++) <<= shift;
  }

  /* Set the lengths of the bit plane strings */
  for (j = 0; j < 8; j++) {
    if (length == 0) plane[j].length = 0;
    else plane[j].length = ptr[j] - plane[j].str;
  }

  return;
}

/******************************************************************************

  Function: eprn_fetch_scan_line

  If there is a next scan line with y coordinate 'dev->eprn.next_y', this
  function fetches it into '*line' and returns zero. Otherwise the function
  returns a non-zero value.

  The storage allocated for 'line->str' must be at least of size
  'dev->eprn.octets_per_line'.

  On success, the 'length' field in 'line' does not include trailing zero
  pixels.

******************************************************************************/

int eprn_fetch_scan_line(eprn_Device *dev, eprn_OctetString *line)
{
  int rc;
  const eprn_Octet *str;

  rc = gdev_prn_copy_scan_lines((gx_device_printer *)dev, dev->eprn.next_y,
    line->str, dev->eprn.octets_per_line);
   /* gdev_prn_copy_scan_lines() returns the number of scan lines it fetched
      or a negative value on error. The number of lines to fetch is the value
      of the last argument divided by the length of a single line, hence in
      this case 1. */
  if (rc != 1) return 1;

  /* Set the length to ignore trailing zero octets in the scan line */
  str = line->str + (dev->eprn.octets_per_line - 1);
  while (str > line->str && *str == 0) str--;
  if (*str == 0) line->length = 0;
  else line->length = str - line->str + 1;

  /* Ensure we have an integral number of pixels in the line */
  if (dev->color_info.depth > 8) {
    int rem;
    unsigned int octets_per_pixel = dev->color_info.depth/8;
      /* If the depth is larger than 8, it is a multiple of 8. */
    rem = line->length % octets_per_pixel;
    if (rem != 0) line->length += octets_per_pixel - rem;
  }

#if 0 && defined(EPRN_TRACE)
  if (gs_debug_c(EPRN_TRACE_CHAR)) {
    int j;
    dlprintf("! eprn_fetch_scan_line(): Fetched ");
    if (line->length == 0) dprintf("empty scan line.");
    else {
      dprintf("scan line: 0x");
      for (j = 0; j < line->length; j++) dprintf1("%02X", line->str[j]);
    }
    dlprintf("\n");
  }
#endif

  return 0;
}

/******************************************************************************

  Function: eprn_get_planes

  For the description of this function, see gdeveprn.h.

******************************************************************************/

int eprn_get_planes(eprn_Device *dev, eprn_OctetString bitplanes[])
{
  eprn_OctetString *line; /* where to copy the scan line from the prn device */
  int
    j,
    rc;

  /* Avoid a copying step for a depth of 1 */
  if (dev->color_info.depth == 1) line = bitplanes;
  else line = &dev->eprn.scan_line;

  /* Fetch the scan line if available */
  if (dev->eprn.intensity_rendering == eprn_IR_FloydSteinberg &&
      dev->eprn.next_y == 0) return 1;
  rc = eprn_fetch_scan_line(dev, line);
  if (rc == 0) dev->eprn.next_y++;
  else {
    if (dev->eprn.intensity_rendering != eprn_IR_FloydSteinberg) return 1;
    dev->eprn.next_y = 0;
  }

  if (dev->color_info.depth == 1) return 0;

  if (dev->eprn.intensity_rendering == eprn_IR_FloydSteinberg) {
    eprn_OctetString tmp;

    eprn_split_FS(&dev->eprn.next_scan_line, &dev->eprn.scan_line,
      dev->eprn.octets_per_line,
      dev->eprn.colour_model,
      dev->eprn.black_levels, dev->eprn.non_black_levels,
      bitplanes);

    /* Switch 'next_scan_line' to refer to what is currently 'scan_line' */
    tmp = dev->eprn.next_scan_line;
    dev->eprn.next_scan_line = dev->eprn.scan_line;
    dev->eprn.scan_line = tmp;
  }
  else {
    /*  Here we split multi-bit pixels which are already adapted to the
	printer's capabilities.

	All the functions called here have the following signature:

	  static void split_line...(eprn_Device *dev, const eprn_Octet *line,
	    int length, eprn_OctetString plane[])

	Such a "split_line implementation" must take the scan line of length
	'length', pointed to by 'line', split it into bit planes according to
	the state of 'dev', and return these planes via 'plane'. The length
	fields of the planes must be set. Trailing zero octets should not be
	removed because it's done here afterwards anyway.
    */

    if (dev->eprn.colour_model == eprn_DeviceGray)
      split_line_le8(dev, line->str, line->length, bitplanes);
    else {
      if (dev->eprn.bits_per_colorant == 1) {
#ifndef EPRN_TRAILING_BIT_BUG_FIXED
	if (dev->eprn.colour_model == eprn_DeviceRGB &&
	    line->length == dev->eprn.octets_per_line) {
	 /* At least gs 6.01 and 6.50 sometimes generate pixel lines where the
	    last pixel is not white but black (last octet in 'line' is 0xE0
	    instead of 0xEE; with pcl3 it shows up for A6 and A4, but not for
	    A3, A5, US Letter, or US Legal).
	    I'm overwriting it with white. */
#ifdef EPRN_TRACE
	  if (gs_debug_c(EPRN_TRACE_CHAR)) {
	    static bool already_noted = false;
	    if (!already_noted && line->str[line->length - 1] != 0xEE) {
	      dlprintf1("! eprn_get_planes(): "
		"Line-terminating octet is 0x%02X.\n",
		line->str[line->length - 1]);
	      already_noted = true;
	    }
	  }
#endif	/* EPRN_TRACE */
	  line->str[line->length - 1] |= RED_BIT | GREEN_BIT | BLUE_BIT;
	}
#endif	/* EPRN_TRAILING_BIT_BUG_FIXED */
	split_line_3or4x1(dev, line->str, line->length, bitplanes);
      }
      else if (dev->eprn.bits_per_colorant == 2 && dev->eprn.black_levels > 2 &&
	  dev->eprn.non_black_levels > 2)
	split_line_4x2(dev, line->str, line->length, bitplanes);
      else if (dev->color_info.depth < 8)
	split_line_le8(dev, line->str, line->length, bitplanes);
      else split_line_ge8(dev, line->str, line->length, bitplanes);
    }
  }

  /* Reduce the lengths of the individual bit planes to not include trailing
     zero octets */
  for (j = 0; j < dev->eprn.output_planes; j++) {
    if (bitplanes[j].length > 0) {
      const eprn_Octet *str = bitplanes[j].str + (bitplanes[j].length - 1);
      while (str > bitplanes[j].str && *str == 0) str--;
      if (*str == 0) bitplanes[j].length = 0;
      else bitplanes[j].length = (str - bitplanes[j].str) + 1;
    }
  }

  return 0;
}
