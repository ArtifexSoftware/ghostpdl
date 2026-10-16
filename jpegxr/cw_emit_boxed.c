/*************************************************************************
*
* This software module was originally contributed by Microsoft
* Corporation in the course of development of the
* ITU-T T.832 | ISO/IEC 29199-2 ("JPEG XR") format standard for
* reference purposes and its performance may not have been optimized.
*
* This software module is an implementation of one or more
* tools as specified by the JPEG XR standard.
*
* ITU/ISO/IEC give You a royalty-free, worldwide, non-exclusive
* copyright license to copy, distribute, and make derivative works
* of this software module or modifications thereof for use in
* products claiming conformance to the JPEG XR standard as
* specified by ITU-T T.832 | ISO/IEC 29199-2.
*
* ITU/ISO/IEC give users the same free license to this software
* module or modifications thereof for research purposes and further
* ITU/ISO/IEC standardization.
*
* Those intending to use this software module in products are advised
* that its use may infringe existing patents. ITU/ISO/IEC have no
* liability for use of this software module or modifications thereof.
*
* Copyright is not released for products that do not conform to
* to the JPEG XR standard as specified by ITU-T T.832 |
* ISO/IEC 29199-2.
*
******** Section to be removed when the standard is published ************
*
* Assurance that the contributed software module can be used
* (1) in the ITU-T "T.JXR" | ISO/IEC 29199 ("JPEG XR") standard once the
*     standard has been adopted; and
* (2) to develop the JPEG XR standard:
*
* Microsoft Corporation and any subsequent contributors to the development
* of this software grant ITU/ISO/IEC all rights necessary to include
* the originally developed software module or modifications thereof in the
* JPEG XR standard and to permit ITU/ISO/IEC to offer such a royalty-free,
* worldwide, non-exclusive copyright license to copy, distribute, and make
* derivative works of this software module or modifications thereof for
* use in products claiming conformance to the JPEG XR standard as
* specified by ITU-T T.832 | ISO/IEC 29199-2, and to the extent that
* such originally developed software module or portions of it are included
* in an ITU/ISO/IEC standard.  To the extent that the original contributors
* may own patent rights that would be required to make, use, or sell the
* originally developed software module or portions thereof included in the
* ITU/ISO/IEC standard in a conforming product, the contributors will
* assure ITU/ISO/IEC that they are willing to negotiate licenses under
* reasonable and non-discriminatory terms and conditions with
* applicants throughout the world and in accordance with their patent
* rights declarations made to ITU/ISO/IEC (if any).
*
* Microsoft, any subsequent contributors, and ITU/ISO/IEC additionally
* gives You a free license to this software module or modifications
* thereof for the sole purpose of developing the JPEG XR standard.
*
******** end of section to be removed when the standard is published *****
*
* Microsoft Corporation retains full right to modify and use the code
* for its own purpose, to assign or donate the code to a third party,
* and to inhibit third parties from using the code for products that
* do not conform to the JPEG XR standard as specified by ITU-T T.832 |
* ISO/IEC 29199-2.
*
* This copyright notice must be included in all copies or derivative
* works.
*
* Copyright (c) ITU-T/ISO/IEC 2008, 2009.
***********************************************************************/

#ifdef _MSC_VER
#pragma comment (user,"$Id: cw_emit_boxed.c,v 1.10 2012-03-18 18:29:23 thor Exp $")
#endif

# include  "jxr_priv.h"
# include  <stdlib.h>
# include  <string.h>
# include  <assert.h>

/*
** This file writes the JPX (ISO) box-based format for JPEG-XR.
**
*/

/*
** Generate a box-ID from the four-character identifier
*/

#define MAKE_ID(a,b,c,d) (((a) << 24) | ((b) << 16) | ((c) << 8) | ((d) << 0))

/*
** prototypes
*/
static int get_bpc(jxr_container_t cp);
static int is_black_on_white_pxfmt(jxr_container_t cp);
static int is_white_on_black_pxfmt(jxr_container_t cp);

int jxrc_set_image_profile(jxr_container_t cp,int profile)
{
  cp->profile_idc = profile;
  return 0;
}

int jxrc_set_inverted_bw(jxr_container_t cp,int inverted)
{
  cp->black_is_one = inverted;
  return 0;
}

/*
** Write four bytes in big-endian format.
*/
static void jxrc_write_ULONG(jxr_container_t cp,uint32_t d)
{
  char buffer[4];

  buffer[0] = (d >> 24) & 0xff;
  buffer[1] = (d >> 16) & 0xff;
  buffer[2] = (d >>  8) & 0xff;
  buffer[3] = (d >>  0) & 0xff;

  if (cp->fd)
    fwrite(buffer,1,sizeof(buffer),cp->fd);
  cp->size_counter += 4;
}

static void jxrc_write_UBYTE(jxr_container_t cp,uint32_t d)
{
  char buffer[1];

  buffer[0] = (d >>  0) & 0xff;

  if (cp->fd)
    fwrite(buffer,1,sizeof(buffer),cp->fd);
  cp->size_counter += 1;
}

static void jxrc_write_UWORD(jxr_container_t cp,uint32_t d)
{
  char buffer[2];

  buffer[0] = (d >>  8) & 0xff;
  buffer[1] = (d >>  0) & 0xff;

  if (cp->fd)
    fwrite(buffer,1,sizeof(buffer),cp->fd);
  cp->size_counter += 2;
}


/*
** write a box header for the given box type and the box length.
*/
static void jxrc_write_box_header(jxr_container_t cp,uint32_t boxtype,uint32_t boxsize)
{
  cp->size_counter = 0;
  jxrc_write_ULONG(cp,boxsize);
  jxrc_write_ULONG(cp,boxtype);
}

/*
** write the JP2 signature box
*/
static void jxrc_write_signature_box(jxr_container_t cp)
{
  jxrc_write_box_header(cp,MAKE_ID('j','P',' ',' '),12);
  if (cp->fd) {
    fputc(0x0d,cp->fd);
    fputc(0x0a,cp->fd);
    fputc(0x87,cp->fd);
    fputc(0x0a,cp->fd);
  } else {
    cp->size_counter += 4;
  }
}

/*
** write the JPX file type box
*/
static void jxrc_write_file_type_box(jxr_container_t cp)
{
  jxrc_write_box_header(cp,MAKE_ID('f','t','y','p'),8+3*4);
  /* brand version: yes, this is jpx as it is defined in 15444-2 */
  jxrc_write_ULONG(cp,MAKE_ID('j','p','x',' '));
  /* version */
  jxrc_write_ULONG(cp,0);
  /* compatibility */
  if (cp->profile_idc <= 44) {
    /* sub-baseline */
    jxrc_write_ULONG(cp,MAKE_ID('j','x','r','0'));
  } else if (cp->profile_idc <= 55) {
    /* baseline */
    jxrc_write_ULONG(cp,MAKE_ID('j','x','r','1'));
  } else if (cp->profile_idc <= 66) {
    /* main */
    jxrc_write_ULONG(cp,MAKE_ID('j','x','r','2'));
  } else {
    /* all of it. */
    jxrc_write_ULONG(cp,MAKE_ID('j','x','r','c'));
  }
}

/*
** check whether the pixel format equals a specific indexed format.
*/
static int is_pxfmt(jxr_container_t cp,int which)
{
  if (!memcmp(cp->pixel_format,jxr_guids[which],16))
    return 1;
  return 0;
}


/*
** Test whether the pixel format specifies a fixed point type
*/
static int is_fixpt_pxfmt(jxr_container_t cp)
{
  if (is_pxfmt(cp,JXRC_FMT_48bppRGBFixedPoint)    ||
      is_pxfmt(cp,JXRC_FMT_96bppRGBFixedPoint)    ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBFixedPoint)    ||
      is_pxfmt(cp,JXRC_FMT_128bppRGBFixedPoint)   ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBAFixedPoint)   ||
      is_pxfmt(cp,JXRC_FMT_128bppRGBAFixedPoint)  ||
      is_pxfmt(cp,JXRC_FMT_16bppGrayFixedPoint)   ||
      is_pxfmt(cp,JXRC_FMT_32bppGrayFixedPoint)   ||
      is_pxfmt(cp,JXRC_FMT_48bppYCC444FixedPoint) ||
      is_pxfmt(cp,JXRC_FMT_64bppYCC444AlphaFixedPoint)) {
    return 1;
  }
  return 0;
}

/*
** Test whether the pixel format specifies a floating point type
*/
static int is_float_pxfmt(jxr_container_t cp)
{
  if (is_pxfmt(cp,JXRC_FMT_48bppRGBHalf)          ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBHalf)          ||
      is_pxfmt(cp,JXRC_FMT_128bppRGBFloat)        ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBAHalf)         ||
      is_pxfmt(cp,JXRC_FMT_128bppRGBAFloat)       ||
      is_pxfmt(cp,JXRC_FMT_128bppPRGBAFloat)      ||
      is_pxfmt(cp,JXRC_FMT_16bppGrayHalf)         ||
      is_pxfmt(cp,JXRC_FMT_32bppGrayFloat)) {
    return 1;
  }
  return 0;
}

/*
** Return information whether the data in here is
** signed or unsigned.
*/
static int is_signed(jxr_container_t cp)
{
  if (is_float_pxfmt(cp) || is_fixpt_pxfmt(cp))
    return 1;
  return 0;
}

/*
** Return the pixel format for the samples in the encoding of
** the pixel format box.
*/
int _jxrc_get_boxed_pixel_format(jxr_container_t cp)
{
  if (is_fixpt_pxfmt(cp)) {
    if (get_bpc(cp) == 16) {
      return 0x300d; /* 13 fractional bits, fixpoint */
    } else if (get_bpc(cp) == 32) {
      return 0x3018; /* 24 fractional bits, fixpoint */
    }
    assert(!"invalid pixel format");
  } else if (is_float_pxfmt(cp)) {
    if (get_bpc(cp) == 16) {
      return 0x400a; /* 10 mantissa bits, half float */
    } else if (get_bpc(cp) == 32) {
      return 0x4017; /* 23 mantissa bits, float */
    }
    assert(!"invalid pixel format");
  } else if (is_pxfmt(cp,JXRC_FMT_32bppRGBE)) {
    return 0x1000;
  }

  /* Everything else is integer, RGBE must be handled separately */
  return 0;
}

/*
** Test whether the pixel format requires the scRGB color space
*/
static int is_scrgb_pxfmt(jxr_container_t cp)
{
  if (is_fixpt_pxfmt(cp))
    return 1;
  if (is_float_pxfmt(cp))
    return 1;
  if (is_pxfmt(cp,JXRC_FMT_48bppRGB)           ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBA)          ||
      is_pxfmt(cp,JXRC_FMT_64bppPRGBA)         ||
      is_pxfmt(cp,JXRC_FMT_32bppRGBE))
    return 1;
  return 0;
}

/*
** Test whether this pixel format defines the greyscale color space.
*/
static int is_grey_pxfmt(jxr_container_t cp)
{
  if (is_pxfmt(cp,JXRC_FMT_8bppGray)            ||
      is_pxfmt(cp,JXRC_FMT_16bppGray)           ||
      is_pxfmt(cp,JXRC_FMT_16bppGrayFixedPoint) ||
      is_pxfmt(cp,JXRC_FMT_16bppGrayHalf)       ||
      is_pxfmt(cp,JXRC_FMT_32bppGrayFixedPoint) ||
      is_pxfmt(cp,JXRC_FMT_32bppGrayFloat))
    return 1;
  return 0;
}

/*
** Test whether the pixel format indicates an RGB color space
*/
static int is_rgb_pxfmt(jxr_container_t cp)
{
  if (is_pxfmt(cp,JXRC_FMT_24bppRGB)    ||
      is_pxfmt(cp,JXRC_FMT_24bppBGR)    ||
      is_pxfmt(cp,JXRC_FMT_32bppBGR)    ||
      is_pxfmt(cp,JXRC_FMT_32bppBGRA)   ||
      is_pxfmt(cp,JXRC_FMT_32bppPBGRA)  ||
      is_pxfmt(cp,JXRC_FMT_16bppBGR555) ||
      is_pxfmt(cp,JXRC_FMT_16bppBGR565) ||
      is_pxfmt(cp,JXRC_FMT_32bppBGR101010))
    return 1;
  return 0;
}

/*
** Test whether this pixel format is a generic profile that requires
** an additional profile to define it.
*/
static int is_generic_pxfmt(jxr_container_t cp)
{
  if (is_pxfmt(cp,JXRC_FMT_24bpp3Channels) ||
      is_pxfmt(cp,JXRC_FMT_32bpp4Channels) ||
      is_pxfmt(cp,JXRC_FMT_40bpp5Channels) ||
      is_pxfmt(cp,JXRC_FMT_48bpp6Channels) ||
      is_pxfmt(cp,JXRC_FMT_56bpp7Channels) ||
      is_pxfmt(cp,JXRC_FMT_64bpp8Channels) ||
      is_pxfmt(cp,JXRC_FMT_32bpp3ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_40bpp4ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_48bpp5ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_56bpp6ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_64bpp7ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_72bpp8ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_48bpp3Channels) ||
      is_pxfmt(cp,JXRC_FMT_64bpp4Channels) ||
      is_pxfmt(cp,JXRC_FMT_80bpp5Channels) ||
      is_pxfmt(cp,JXRC_FMT_96bpp6Channels) ||
      is_pxfmt(cp,JXRC_FMT_112bpp7Channels) ||
      is_pxfmt(cp,JXRC_FMT_128bpp8Channels) ||
      is_pxfmt(cp,JXRC_FMT_64bpp3ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_80bpp4ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_96bpp5ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_112bpp6ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_128bpp7ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_144bpp8ChannelsAlpha))
    return 1;
  return 0;
}

/*
** Test whether the pixel format specifies unmultiplied opacity
*/
int _jxrc_is_alpha_pxfmt(jxr_container_t cp)
{
  if (is_pxfmt(cp,JXRC_FMT_32bppBGRA)            ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBA)            ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBAFixedPoint)  ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBAHalf)        ||
      is_pxfmt(cp,JXRC_FMT_128bppRGBAFixedPoint) ||
      is_pxfmt(cp,JXRC_FMT_128bppRGBAFloat)      ||
      is_pxfmt(cp,JXRC_FMT_40bppCMYKAlpha)       ||
      is_pxfmt(cp,JXRC_FMT_80bppCMYKAlpha)       ||
      is_pxfmt(cp,JXRC_FMT_32bpp3ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_40bpp4ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_48bpp5ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_56bpp6ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_64bpp7ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_72bpp8ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_64bpp3ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_80bpp4ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_96bpp5ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_112bpp6ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_128bpp7ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_144bpp8ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_40bppCMYKDIRECTAlpha) ||
      is_pxfmt(cp,JXRC_FMT_80bppCMYKDIRECTAlpha) ||
      is_pxfmt(cp,JXRC_FMT_20bppYCC420Alpha)     ||
      is_pxfmt(cp,JXRC_FMT_24bppYCC422Alpha)     ||
      is_pxfmt(cp,JXRC_FMT_30bppYCC422Alpha)     ||
      is_pxfmt(cp,JXRC_FMT_48bppYCC422Alpha)     ||
      is_pxfmt(cp,JXRC_FMT_32bppYCC444Alpha)     ||
      is_pxfmt(cp,JXRC_FMT_40bppYCC444Alpha)     ||
      is_pxfmt(cp,JXRC_FMT_64bppYCC444Alpha)     ||
      is_pxfmt(cp,JXRC_FMT_64bppYCC444AlphaFixedPoint)) {
    return 1;
  }
  return 0;
}

int _jxrc_is_pre_alpha_pxfmt(jxr_container_t cp)
{
  if (is_pxfmt(cp,JXRC_FMT_32bppPBGRA) ||
      is_pxfmt(cp,JXRC_FMT_64bppPRGBA) ||
      is_pxfmt(cp,JXRC_FMT_128bppPRGBAFloat)) {
    return 1;
  }
  return 0;
}

static int is_black_on_white_pxfmt(jxr_container_t cp)
{
  if (is_pxfmt(cp,JXRC_FMT_BlackWhite)) {
    if (cp->black_is_one)
      return 1;
  }
  return 0;
}

static int is_white_on_black_pxfmt(jxr_container_t cp)
{
  if (is_pxfmt(cp,JXRC_FMT_BlackWhite)) {
    if (!cp->black_is_one)
      return 1;
  }
  return 0;
}

/*
** Test whether a pixel format is YCbCr. Unfortunately,
** this software does not yet write the PTM_COLOR_INFO, thus
** the nature of YCbCr is not defined. It is here assumed
** that YCbCr is always full range.
*/
static int is_ycbcr_pxfmt(jxr_container_t cp)
{
  if (is_pxfmt(cp,JXRC_FMT_12bppYCC420)           ||
      is_pxfmt(cp,JXRC_FMT_16bppYCC422)           ||
      is_pxfmt(cp,JXRC_FMT_20bppYCC422)           ||
      is_pxfmt(cp,JXRC_FMT_32bppYCC422)           ||
      is_pxfmt(cp,JXRC_FMT_24bppYCC444)           ||
      is_pxfmt(cp,JXRC_FMT_30bppYCC444)           ||
      is_pxfmt(cp,JXRC_FMT_48bppYCC444)           ||
      is_pxfmt(cp,JXRC_FMT_48bppYCC444FixedPoint) ||
      is_pxfmt(cp,JXRC_FMT_20bppYCC420Alpha)      ||
      is_pxfmt(cp,JXRC_FMT_24bppYCC422Alpha)      ||
      is_pxfmt(cp,JXRC_FMT_30bppYCC422Alpha)      ||
      is_pxfmt(cp,JXRC_FMT_48bppYCC422Alpha)      ||
      is_pxfmt(cp,JXRC_FMT_32bppYCC444Alpha)      ||
      is_pxfmt(cp,JXRC_FMT_40bppYCC444Alpha)      ||
      is_pxfmt(cp,JXRC_FMT_64bppYCC444Alpha)      ||
      is_pxfmt(cp,JXRC_FMT_64bppYCC444AlphaFixedPoint))
    return 1;
  return 0;
}

static int is_cmyk_pxfmt(jxr_container_t cp)
{
  if (is_pxfmt(cp,JXRC_FMT_32bppCMYK)            ||
      is_pxfmt(cp,JXRC_FMT_40bppCMYKAlpha)       ||
      is_pxfmt(cp,JXRC_FMT_64bppCMYK)            ||
      is_pxfmt(cp,JXRC_FMT_80bppCMYKAlpha)       ||
      is_pxfmt(cp,JXRC_FMT_32bppCMYKDIRECT)      ||
      is_pxfmt(cp,JXRC_FMT_64bppCMYKDIRECT)      ||
      is_pxfmt(cp,JXRC_FMT_40bppCMYKDIRECTAlpha) ||
      is_pxfmt(cp,JXRC_FMT_80bppCMYKDIRECTAlpha)) {
    return 1;
  }
  return 0;
}

/*
** Return the number of channels in the container including any
** alpha channel.
*/
int _jxrc_PixelFormatToChannels(jxr_container_t cp)
{
  if (is_pxfmt(cp,JXRC_FMT_24bppRGB)            ||
      is_pxfmt(cp,JXRC_FMT_24bppBGR)            ||
      is_pxfmt(cp,JXRC_FMT_32bppBGR)            ||
      is_pxfmt(cp,JXRC_FMT_48bppRGB)            ||
      is_pxfmt(cp,JXRC_FMT_48bppRGBFixedPoint)  ||
      is_pxfmt(cp,JXRC_FMT_48bppRGBHalf)        ||
      is_pxfmt(cp,JXRC_FMT_96bppRGBFixedPoint)  ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBFixedPoint)  ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBHalf)        ||
      is_pxfmt(cp,JXRC_FMT_128bppRGBFixedPoint) ||
      is_pxfmt(cp,JXRC_FMT_128bppRGBFloat)) {
    return 3;
  }

  if (is_pxfmt(cp,JXRC_FMT_32bppBGRA)            ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBA)            ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBAFixedPoint)  ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBAHalf)        ||
      is_pxfmt(cp,JXRC_FMT_128bppRGBAFixedPoint) ||
      is_pxfmt(cp,JXRC_FMT_128bppRGBAFloat)      ||
      is_pxfmt(cp,JXRC_FMT_32bppPBGRA)           ||
      is_pxfmt(cp,JXRC_FMT_64bppPRGBA)           ||
      is_pxfmt(cp,JXRC_FMT_128bppPRGBAFloat)) {
    return 4;
  }

  if (is_pxfmt(cp,JXRC_FMT_32bppCMYK)       ||
      is_pxfmt(cp,JXRC_FMT_64bppCMYK)       ||
      is_pxfmt(cp,JXRC_FMT_32bppCMYKDIRECT) ||
      is_pxfmt(cp,JXRC_FMT_64bppCMYKDIRECT)) {
    return 4;
  }

  if (is_pxfmt(cp,JXRC_FMT_40bppCMYKAlpha)       ||
      is_pxfmt(cp,JXRC_FMT_80bppCMYKAlpha)       ||
      is_pxfmt(cp,JXRC_FMT_40bppCMYKDIRECTAlpha) ||
      is_pxfmt(cp,JXRC_FMT_80bppCMYKDIRECTAlpha)) {
    return 5;
  }

  if (is_pxfmt(cp,JXRC_FMT_24bpp3Channels) ||
      is_pxfmt(cp,JXRC_FMT_48bpp3Channels))
    return 3;

  if (is_pxfmt(cp,JXRC_FMT_32bpp4Channels) ||
      is_pxfmt(cp,JXRC_FMT_64bpp4Channels))
    return 4;

  if (is_pxfmt(cp,JXRC_FMT_40bpp5Channels) ||
      is_pxfmt(cp,JXRC_FMT_80bpp5Channels))
    return 5;

  if (is_pxfmt(cp,JXRC_FMT_48bpp6Channels) ||
      is_pxfmt(cp,JXRC_FMT_96bpp6Channels))
    return 6;

  if (is_pxfmt(cp,JXRC_FMT_56bpp7Channels) ||
      is_pxfmt(cp,JXRC_FMT_112bpp7Channels))
    return 7;

  if (is_pxfmt(cp,JXRC_FMT_64bpp8Channels) ||
      is_pxfmt(cp,JXRC_FMT_128bpp8Channels))
    return 8;

  if (is_pxfmt(cp,JXRC_FMT_32bpp3ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_64bpp3ChannelsAlpha)) {
    return 4;
  }

  if (is_pxfmt(cp,JXRC_FMT_40bpp4ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_80bpp4ChannelsAlpha)) {
    return 5;
  }

  if (is_pxfmt(cp,JXRC_FMT_48bpp5ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_96bpp5ChannelsAlpha)) {
    return 6;
  }

  if (is_pxfmt(cp,JXRC_FMT_56bpp6ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_112bpp6ChannelsAlpha)) {
    return 7;
  }

  if (is_pxfmt(cp,JXRC_FMT_64bpp7ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_128bpp7ChannelsAlpha)) {
    return 8;
  }

  if (is_pxfmt(cp,JXRC_FMT_72bpp8ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_144bpp8ChannelsAlpha)) {
    return 9;
  }

  if (is_pxfmt(cp,JXRC_FMT_8bppGray)            ||
      is_pxfmt(cp,JXRC_FMT_16bppGray)           ||
      is_pxfmt(cp,JXRC_FMT_16bppGrayFixedPoint) ||
      is_pxfmt(cp,JXRC_FMT_16bppGrayHalf)       ||
      is_pxfmt(cp,JXRC_FMT_32bppGrayFixedPoint) ||
      is_pxfmt(cp,JXRC_FMT_32bppGrayFloat)      ||
      is_pxfmt(cp,JXRC_FMT_BlackWhite))
    return 1;

  if (is_pxfmt(cp,JXRC_FMT_16bppBGR555) ||
      is_pxfmt(cp,JXRC_FMT_16bppBGR565) ||
      is_pxfmt(cp,JXRC_FMT_32bppBGR101010))
    return 3;

  if (is_pxfmt(cp,JXRC_FMT_32bppRGBE))
    return 4; /* Number of components in the codestream. Though only three colors */

  if (is_pxfmt(cp,JXRC_FMT_12bppYCC420) ||
      is_pxfmt(cp,JXRC_FMT_16bppYCC422) ||
      is_pxfmt(cp,JXRC_FMT_20bppYCC422) ||
      is_pxfmt(cp,JXRC_FMT_32bppYCC422) ||
      is_pxfmt(cp,JXRC_FMT_24bppYCC444) ||
      is_pxfmt(cp,JXRC_FMT_30bppYCC444) ||
      is_pxfmt(cp,JXRC_FMT_48bppYCC444) ||
      is_pxfmt(cp,JXRC_FMT_48bppYCC444FixedPoint))
    return 3;

  if (is_pxfmt(cp,JXRC_FMT_20bppYCC420Alpha) ||
      is_pxfmt(cp,JXRC_FMT_24bppYCC422Alpha) ||
      is_pxfmt(cp,JXRC_FMT_30bppYCC422Alpha) ||
      is_pxfmt(cp,JXRC_FMT_48bppYCC422Alpha) ||
      is_pxfmt(cp,JXRC_FMT_32bppYCC444Alpha) ||
      is_pxfmt(cp,JXRC_FMT_40bppYCC444Alpha) ||
      is_pxfmt(cp,JXRC_FMT_64bppYCC444Alpha) ||
      is_pxfmt(cp,JXRC_FMT_64bppYCC444AlphaFixedPoint)) {
    return 4;
  }

  return 0;
}

/*
** Number of components including the alpha channel
** if it is interleaved.
*/
static int get_num_components(jxr_container_t cp)
{
  int channels = _jxrc_PixelFormatToChannels(cp);

  if (_jxrc_is_alpha_pxfmt(cp) || _jxrc_is_pre_alpha_pxfmt(cp)) {
    if (cp->separate_alpha_image_plane) {
      /* alpha channel goes into a separate image */
      channels--;
    }
  }

  return channels;
}

/*
** get the number of bits per component, return 6 for
** the 565 mode.
*/
int _jxrc_PixelFormatToBpp(jxr_container_t cp)
{
  int bpc = get_bpc(cp);

  if (bpc == 256)
    return 6;

  return bpc;
}

/*
** get the number of bits per component, or return 256
** for the strange 565 format.
*/
static int get_bpc(jxr_container_t cp)
{
  if (is_pxfmt(cp,JXRC_FMT_24bppRGB)             ||
      is_pxfmt(cp,JXRC_FMT_24bppBGR)             ||
      is_pxfmt(cp,JXRC_FMT_32bppBGR)             ||
      is_pxfmt(cp,JXRC_FMT_32bppBGRA)            ||
      is_pxfmt(cp,JXRC_FMT_32bppPBGRA)           ||
      is_pxfmt(cp,JXRC_FMT_32bppCMYK)            ||
      is_pxfmt(cp,JXRC_FMT_40bppCMYKAlpha)       ||
      is_pxfmt(cp,JXRC_FMT_24bpp3Channels)       ||
      is_pxfmt(cp,JXRC_FMT_32bpp4Channels)       ||
      is_pxfmt(cp,JXRC_FMT_40bpp5Channels)       ||
      is_pxfmt(cp,JXRC_FMT_48bpp6Channels)       ||
      is_pxfmt(cp,JXRC_FMT_56bpp7Channels)       ||
      is_pxfmt(cp,JXRC_FMT_64bpp8Channels)       ||
      is_pxfmt(cp,JXRC_FMT_32bpp3ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_40bpp4ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_48bpp5ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_56bpp6ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_64bpp7ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_72bpp8ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_8bppGray)             ||
      is_pxfmt(cp,JXRC_FMT_32bppCMYKDIRECT)      ||
      is_pxfmt(cp,JXRC_FMT_40bppCMYKDIRECTAlpha) ||
      is_pxfmt(cp,JXRC_FMT_12bppYCC420)          ||
      is_pxfmt(cp,JXRC_FMT_16bppYCC422)          ||
      is_pxfmt(cp,JXRC_FMT_24bppYCC444)          ||
      is_pxfmt(cp,JXRC_FMT_20bppYCC420Alpha)     ||
      is_pxfmt(cp,JXRC_FMT_24bppYCC422Alpha)     ||
      is_pxfmt(cp,JXRC_FMT_32bppYCC444Alpha))
    return 8;

  if (is_pxfmt(cp,JXRC_FMT_48bppRGB)             ||
      is_pxfmt(cp,JXRC_FMT_48bppRGBFixedPoint)   ||
      is_pxfmt(cp,JXRC_FMT_48bppRGBHalf)         ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBFixedPoint)   ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBAFixedPoint)  ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBHalf)         ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBAHalf)        ||
      is_pxfmt(cp,JXRC_FMT_64bppRGBA)            ||
      is_pxfmt(cp,JXRC_FMT_64bppPRGBA)           ||
      is_pxfmt(cp,JXRC_FMT_64bppCMYK)            ||
      is_pxfmt(cp,JXRC_FMT_80bppCMYKAlpha)       ||
      is_pxfmt(cp,JXRC_FMT_48bpp3Channels)       ||
      is_pxfmt(cp,JXRC_FMT_64bpp4Channels)       ||
      is_pxfmt(cp,JXRC_FMT_80bpp5Channels)       ||
      is_pxfmt(cp,JXRC_FMT_96bpp6Channels)       ||
      is_pxfmt(cp,JXRC_FMT_112bpp7Channels)      ||
      is_pxfmt(cp,JXRC_FMT_128bpp8Channels)      ||
      is_pxfmt(cp,JXRC_FMT_64bpp3ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_80bpp4ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_96bpp5ChannelsAlpha)  ||
      is_pxfmt(cp,JXRC_FMT_112bpp6ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_128bpp7ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_144bpp8ChannelsAlpha) ||
      is_pxfmt(cp,JXRC_FMT_16bppGray)            ||
      is_pxfmt(cp,JXRC_FMT_16bppGrayFixedPoint)  ||
      is_pxfmt(cp,JXRC_FMT_16bppGrayHalf)        ||
      is_pxfmt(cp,JXRC_FMT_64bppCMYKDIRECT)      ||
      is_pxfmt(cp,JXRC_FMT_80bppCMYKDIRECTAlpha) ||
      is_pxfmt(cp,JXRC_FMT_32bppYCC422)          ||
      is_pxfmt(cp,JXRC_FMT_48bppYCC444)          ||
      is_pxfmt(cp,JXRC_FMT_48bppYCC444FixedPoint)||
      is_pxfmt(cp,JXRC_FMT_48bppYCC422Alpha)     ||
      is_pxfmt(cp,JXRC_FMT_64bppYCC444Alpha)     ||
      is_pxfmt(cp,JXRC_FMT_64bppYCC444AlphaFixedPoint))
    return 16;

  if (is_pxfmt(cp,JXRC_FMT_96bppRGBFixedPoint)   ||
      is_pxfmt(cp,JXRC_FMT_128bppRGBFixedPoint)  ||
      is_pxfmt(cp,JXRC_FMT_128bppRGBAFixedPoint) ||
      is_pxfmt(cp,JXRC_FMT_128bppRGBFloat)       ||
      is_pxfmt(cp,JXRC_FMT_128bppRGBAFloat)      ||
      is_pxfmt(cp,JXRC_FMT_128bppPRGBAFloat)     ||
      is_pxfmt(cp,JXRC_FMT_32bppGrayFixedPoint)  ||
      is_pxfmt(cp,JXRC_FMT_32bppGrayFloat))
    return 32;

  if (is_pxfmt(cp,JXRC_FMT_BlackWhite))
    return 1;

  if (is_pxfmt(cp,JXRC_FMT_16bppBGR555))
    return 5; /* bit depths varies. */

  if (is_pxfmt(cp,JXRC_FMT_16bppBGR565))
    return 256; /* bit depths varies */

  if (is_pxfmt(cp,JXRC_FMT_32bppBGR101010) ||
      is_pxfmt(cp,JXRC_FMT_20bppYCC422)    ||
      is_pxfmt(cp,JXRC_FMT_30bppYCC444)    ||
      is_pxfmt(cp,JXRC_FMT_30bppYCC422Alpha) ||
      is_pxfmt(cp,JXRC_FMT_40bppYCC444Alpha))
    return 10;

  if (is_pxfmt(cp,JXRC_FMT_32bppRGBE))
    return 8;

  assert(!"unknown pixel format");
  return 0;
}

/*
** write the reader requirements box
*/
static void jxrc_write_rreq_box(jxr_container_t cp)
{
  int features[16] = {0};
  int *f = features;
  int bits,mask;

  if (_jxrc_is_alpha_pxfmt(cp)) {
    *f++ = 9;  /* unmultiplied alpha */
    if (cp->separate_alpha_image_plane)
      *f++ = 2;
  }
  if (_jxrc_is_pre_alpha_pxfmt(cp)) {
    *f++ = 10; /* premultiplied alpha */
    if (cp->separate_alpha_image_plane)
      *f++ = 2;
  }

  *f++ = 75; /* is 29199-2 */
  if (cp->profile_idc <= 44) {
    *f++ = 76; /* subbaseline profile */
  } else if (cp->profile_idc <= 55) {
    *f++ = 77; /* baseline profile */
  } else if (cp->profile_idc <= 66) {
    *f++ = 78; /* main profile */
  } else {
    *f++ = 79; /* advanced profile */
  }

  if (is_fixpt_pxfmt(cp)) {
    *f++ = 80; /* pixel format fixpoint is used */
  }
  if (is_float_pxfmt(cp)) {
    *f++ = 81; /* floating point is used */
  }
  if (is_pxfmt(cp,JXRC_FMT_32bppRGBE)) {
    *f++ = 82; /* exponent or mantissa is used */
  }
  if (is_scrgb_pxfmt(cp)) {
    *f++ = 83; /* scRGB pixel format */
  }

  if (is_black_on_white_pxfmt(cp)) { /* 1 is black */
    *f++ = 47; /* bi-level 1 */
  }
  if (is_white_on_black_pxfmt(cp)) { /* 0 is black */
    *f++ = 48; /* bi-level 2 */
  }
  if (is_ycbcr_pxfmt(cp)) {
    *f++ = 50;  /* ycbcr(2), full range. */
  }
  if (is_cmyk_pxfmt(cp)) {
    *f++ = 55; /* cmyk */
  }
  *f++ = 0;

  bits = f - features - 1;
  mask = (1 << bits) - 1;

  jxrc_write_box_header(cp,MAKE_ID('r','r','e','q'),8 + 1 + 1 + 1 + 2 + 3 * bits + 2);

  /* mask length = 1 */
  jxrc_write_UBYTE(cp,1);
  /* FUAM */
  jxrc_write_UBYTE(cp,mask);
  /* If the topmost bit encodes alpha, do not set in the decode
  ** correctly. Alpha is not strictly needed .
  */
  if (_jxrc_is_alpha_pxfmt(cp) || _jxrc_is_pre_alpha_pxfmt(cp)) {
    jxrc_write_UBYTE(cp,mask & ~1);
  } else {
    jxrc_write_UBYTE(cp,mask);
  }
  /*
  ** Number of standard flags
  */
  jxrc_write_UWORD(cp,bits);
  for(f = features,mask = 1 << (f - features);*f;f++,mask <<= 1) {
    jxrc_write_UWORD(cp,*f);
    jxrc_write_UBYTE(cp,mask);
  }
  /* Number of vendor flags
   */
  jxrc_write_UWORD(cp,0);
}

/*
** write the image header box
*/
static void jxrc_write_ihdr(jxr_container_t cp)
{
  jxrc_write_box_header(cp,MAKE_ID('i','h','d','r'),8 + 4 + 4 + 2 + 1 + 1 + 1 + 1);

  /* image dimensions */
  jxrc_write_ULONG(cp,cp->hei);
  jxrc_write_ULONG(cp,cp->wid);

  jxrc_write_UWORD(cp,get_num_components(cp));
  jxrc_write_UBYTE(cp,(get_bpc(cp)-1) | (is_signed(cp)?(128):(0)));
  jxrc_write_UBYTE(cp,11); /* compression type is XR */
  jxrc_write_UBYTE(cp,1);  /* color space is guessed */
  jxrc_write_UBYTE(cp,0);  /* no rights information present */
}

static void jxrc_write_bpc(jxr_container_t cp)
{
  if (is_pxfmt(cp,JXRC_FMT_16bppBGR565)) {
    /* Only in this case the BPC box is required */
    jxrc_write_box_header(cp,MAKE_ID('b','p','c','c'),8 + 3);
    jxrc_write_UBYTE(cp,5-1);
    jxrc_write_UBYTE(cp,6-1);
    jxrc_write_UBYTE(cp,5-1);
  } else {
    cp->size_counter = 0;
  }
}

/*
** Return the enumerated color space index.
*/
int _jxrc_enumerated_colorspace(jxr_container_t cp)
{
  if (is_scrgb_pxfmt(cp)) {
    return 25; /* scRGB color space */
  } else if (is_black_on_white_pxfmt(cp)) {
    return 0; /* bi-level 1 */
  } else if (is_white_on_black_pxfmt(cp)) {
    return 15;
  } else if (is_ycbcr_pxfmt(cp)) {
    return 3; /* YCbCr */
  } else if (is_cmyk_pxfmt(cp)) {
    return 12; /* CMYK */
  } else if (is_grey_pxfmt(cp)) {
    return 17; /* grey-scale */
  } else if (is_rgb_pxfmt(cp)) {
    return 16; /* RGB */
  }
  return -1;
}

static void jxrc_write_colorspec(jxr_container_t cp)
{
  if (!is_generic_pxfmt(cp)) {
    jxrc_write_box_header(cp,MAKE_ID('c','o','l','r'),8 + 3 + ((is_generic_pxfmt(cp))?(0):(4)));
    jxrc_write_UBYTE(cp,1); /* enumerated method */
    jxrc_write_UBYTE(cp,1); /* precedence */
    jxrc_write_UBYTE(cp,3); /* approx. The best we know */

    cp->color = _jxrc_enumerated_colorspace(cp);

    jxrc_write_ULONG(cp,cp->color);
  } else {
    cp->size_counter = 0;
  }
}

/*
** write the jp2 channel definition box.
*/
static void jxrc_write_cdef(jxr_container_t cp)
{
  int i;
  int c     = _jxrc_PixelFormatToChannels(cp);
  int alpha = _jxrc_is_alpha_pxfmt(cp) || _jxrc_is_pre_alpha_pxfmt(cp);
  int rgbe  = 0;

  jxrc_write_box_header(cp,MAKE_ID('c','d','e','f'),8 + 2 + 2 * 3 * c);
  jxrc_write_UWORD(cp,c); /* number of channel descriptions */
  /* first, write all but the alpha channel descriptions */
  for(i = 0;i < c-alpha;i++) {
    if (i == 3 && rgbe) {
      jxrc_write_UWORD(cp,i); /* channel index */
      jxrc_write_UWORD(cp,0); /* channel type: color */
      jxrc_write_UWORD(cp,0); /* exponent channel: all of the image */
    } else {
      jxrc_write_UWORD(cp,i); /* channel index */
      jxrc_write_UWORD(cp,0); /* channel type: color */
      jxrc_write_UWORD(cp,i+1); /* channel association: just the canonical */
    }
  }
  if (alpha) {
    /* Finally, include the alpha channel */
    jxrc_write_UWORD(cp,c-1); /* channel index: the last one */
    if (_jxrc_is_pre_alpha_pxfmt(cp))
      jxrc_write_UWORD(cp,2); /* premultiplied alpha */
    else
      jxrc_write_UWORD(cp,1); /* alpha */
    jxrc_write_UWORD(cp,0); /* association: all of the image */
  }
}

/*
** Write the pixel format box
*/
static void jxrc_write_pxfm(jxr_container_t cp)
{
  int rgbe  = 0;
  int c     = _jxrc_PixelFormatToChannels(cp);

  jxrc_write_box_header(cp,MAKE_ID('p','x','f','m'),8 + 2 + (2 + 2) * c);
  jxrc_write_UWORD(cp,c);
  if (rgbe) {
    jxrc_write_UWORD(cp,0);
    jxrc_write_UWORD(cp,0x1000); /* mantissa */
    jxrc_write_UWORD(cp,1);
    jxrc_write_UWORD(cp,0x1000); /* mantissa */
    jxrc_write_UWORD(cp,2);
    jxrc_write_UWORD(cp,0x1000); /* mantissa */
    jxrc_write_UWORD(cp,3);
    jxrc_write_UWORD(cp,0x2000); /* exponent */
  } else {
    int i,pxfm = _jxrc_get_boxed_pixel_format(cp);
    for(i = 0;i < c;i++) {
      jxrc_write_UWORD(cp,i);
      jxrc_write_UWORD(cp,pxfm);
    }
  }
}

/*
** Write the jp2-header box
*/
static void jxrc_write_jp2h(jxr_container_t cp)
{
  FILE *fp = cp->fd;
  int size = 8; /* the box header. */

  cp->fd = NULL;

  jxrc_write_colorspec(cp);
  size  += cp->size_counter;
  jxrc_write_cdef(cp);
  size  += cp->size_counter;
  jxrc_write_pxfm(cp);
  size  += cp->size_counter;


  /*
  ** Now write the boxes
  */
  cp->fd = fp;
  jxrc_write_box_header(cp,MAKE_ID('j','p','2','h'),size);
  jxrc_write_colorspec(cp);
  jxrc_write_cdef(cp);
  jxrc_write_pxfm(cp);
}

/*
** In case we are compositing the layer from several codestreams
** by one that contains the alpha channel, write a compositing layer
** header box.
*/
static void jxrc_write_jplh(jxr_container_t cp)
{
  if (cp->separate_alpha_image_plane) {
    jxrc_write_box_header(cp,MAKE_ID('j','p','l','h'),8 + 8 + 4 + (2 + 4) * 2);
    jxrc_write_box_header(cp,MAKE_ID('c','r','e','g'),8 + 4 + (2 + 4) * 2);
    /* Grid width is simply 1 */
    jxrc_write_UWORD(cp,1);
    jxrc_write_UWORD(cp,1);
    jxrc_write_UWORD(cp,0); /* first codestream */
    jxrc_write_UBYTE(cp,1); /* scaling is 1-1 */
    jxrc_write_UBYTE(cp,1); /* scaling is 1-1 */
    jxrc_write_UBYTE(cp,0); /* offset is zero */
    jxrc_write_UBYTE(cp,0); /* offset is zero */
    jxrc_write_UWORD(cp,1); /* second codestream */
    jxrc_write_UBYTE(cp,1); /* scaling is 1-1 */
    jxrc_write_UBYTE(cp,1); /* scaling is 1-1 */
    jxrc_write_UBYTE(cp,0); /* offset is zero */
    jxrc_write_UBYTE(cp,0); /* offset is zero */
  }
}

/*
** Write the first codestream header box or the only codestream header box if
** there are more than one codestreams (separate alpha)
*/
static void jxrc_write_jpch(jxr_container_t cp)
{
  FILE *fp = cp->fd;
  int size = 8; /* the box header. */

  cp->fd = NULL;
  jxrc_write_ihdr(cp);
  size  += cp->size_counter;
  jxrc_write_bpc(cp);
  size  += cp->size_counter;

  cp->fd = fp;
  jxrc_write_box_header(cp,MAKE_ID('j','p','c','h'),size);
  jxrc_write_ihdr(cp);
  jxrc_write_bpc(cp);
}

/*
** Write the second codestream header box for the separate alpha channel
** if required.
*/
static void jxrc_write_jpch_alpha(jxr_container_t cp)
{
  if (_jxrc_is_pre_alpha_pxfmt(cp) || _jxrc_is_alpha_pxfmt(cp)) {
    if (cp->separate_alpha_image_plane) {
      jxrc_write_box_header(cp,MAKE_ID('j','p','c','h'),8 + 8 + 4 + 4 + 2 + 1 + 1 + 1 + 1);
      jxrc_write_box_header(cp,MAKE_ID('i','h','d','r'),8 + 4 + 4 + 2 + 1 + 1 + 1 + 1);
      jxrc_write_ULONG(cp,cp->hei);
      jxrc_write_ULONG(cp,cp->wid);

      jxrc_write_UWORD(cp,1);
      jxrc_write_UBYTE(cp,get_bpc(cp)-1);
      jxrc_write_UBYTE(cp,11); /* compression type is XR */
      jxrc_write_UBYTE(cp,1);  /* color space is guessed */
      jxrc_write_UBYTE(cp,0);  /* no rights information present */
    }
  }
}


int jxrc_start_file_boxed(jxr_container_t cp, FILE*fd)
{
  assert(cp->fd == 0);

  /* initializations */
  cp->image_count_mark = 0;
  cp->alpha_count_mark = 0;
  cp->alpha_offset_mark = 0;
  cp->alpha_band = 0;

  cp->fd = fd;


  jxrc_write_signature_box(cp);
  jxrc_write_file_type_box(cp);
  jxrc_write_rreq_box(cp);
  jxrc_write_jp2h(cp);
  jxrc_write_jpch(cp);
  jxrc_write_jpch_alpha(cp);
  jxrc_write_jplh(cp);

  /* The first codestream. The length is fixed later. */
  cp->image_offset_mark = ftell(cp->fd);
  jxrc_write_box_header(cp,MAKE_ID('j','p','2','c'),0);
  return 0;
}

int jxrc_begin_image_data_boxed(jxr_container_t cp)
{
  return 0;
}

int jxrc_write_container_post_boxed(jxr_container_t cp)
{
      uint32_t mark = ftell(cp->fd);
      uint32_t count;

      assert(mark > cp->image_offset_mark);
      count = mark - cp->image_offset_mark; /* 8 is the box header */

      DODEBUG("CONTAINER: measured bitstream count=%u\n", count);

      fflush(cp->fd);
      fseek(cp->fd, cp->image_offset_mark, SEEK_SET);
      jxrc_write_ULONG(cp,count);

      if(cp->separate_alpha_image_plane) {
        fseek(cp->fd, mark, SEEK_SET);
        cp->alpha_offset_mark = mark;
        jxrc_write_box_header(cp,MAKE_ID('j','p','2','c'),0);
      }
      return 0;
}

int jxrc_write_container_post_alpha_boxed(jxr_container_t cp)
{
      uint32_t mark  = ftell(cp->fd);
      uint32_t count = mark - cp->alpha_offset_mark;
      DODEBUG("CONTAINER: measured alpha count=%u\n", count);

      if(cp->separate_alpha_image_plane) {
        fflush(cp->fd);
        fseek(cp->fd, cp->alpha_offset_mark, SEEK_SET);
        jxrc_write_ULONG(cp,count);
      }
      return 0;
}
/*
* $Log: cw_emit_boxed.c,v $
* Revision 1.10  2012-03-18 18:29:23  thor
* Fixed the separation of alpha planes and the concatenation of alpha planes,
* the number of channels was set incorrectly.
*
* Revision 1.9  2012-03-18 00:09:21  thor
* Fixed handling of YCC.
*
* Revision 1.8  2012-03-17 20:03:45  thor
* Fixed a lot of issues in the box parser - seems to work now in simple cases.
*
* Revision 1.7  2012-02-16 16:36:25  thor
* Heavily reworked, but not yet tested.
*
* Revision 1.6  2012-02-13 21:11:03  thor
* Tested now for most color formats.
*
* Revision 1.5  2012-02-13 18:56:44  thor
* Fixed parsing 565 tiff files.
*
* Revision 1.4  2012-02-13 18:23:43  thor
* Fixed writer in simple cases. Not everything explored yet.
*
* Revision 1.3  2012-02-13 17:36:50  thor
* Fixed syntax errors, not yet debugged. Added option for box output.
*
* Revision 1.2  2012-02-13 16:25:18  thor
* Updated the boxed functions.
*
* Revision 1.1  2012-02-11 04:24:20  thor
* Added incomplete box writer.
*
* Revision 1.10  2011-11-19 20:52:34  thor
* Fixed decoding of YUV422 in 10bpp, fixed 10bpp tiff reading and writing.
*
* Revision 1.9  2011-04-28 08:45:42  thor
* Fixed compiler warnings, ported to gcc 4.4, removed obsolete files.
*
* Revision 1.8  2010-10-03 13:14:42  thor
* Fixed missing preshift for BD32 images, added alpha-quantizer
* parameter. Fixed alpha plane container offset.
*
* Revision 1.7  2010-10-03 12:35:27  thor
* Fixed misaligned container sizes.
*
* Revision 1.6  2010-03-31 07:50:58  thor
* Replaced by the latest MS version.
*
* Revision 1.2 2009/05/29 12:00:00 microsoft
* Reference Software v1.6 updates.
*
* Revision 1.1 2009/04/13 12:00:00 microsoft
* Reference Software v1.5 updates.
*
*/
