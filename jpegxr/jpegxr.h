
# ifndef __jpegxr_h
# define __jpegxr_h

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
* standard has been adopted; and
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
* in an ITU/ISO/IEC standard. To the extent that the original contributors
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
#pragma comment (user,"$Id: jpegxr.h,v 1.14 2012-05-17 12:42:57 thor Exp $")
#endif

# include <stdio.h>

#ifdef _MSC_VER
# ifdef JXR_DLL_EXPORTS
# define JXR_EXTERN extern "C" __declspec(dllexport)
# else
# define JXR_EXTERN extern "C" __declspec(dllimport)
# endif
#else
# ifdef _cplusplus
# define JXR_EXTERN extern "C"
# else
# define JXR_EXTERN extern
# endif
#endif

/* This version of jpegxr supports a custom allocator
 * system. */
#define JXR_REDIRECTED_MALLOCS

typedef struct
{
    void *handle;
    void *(*malloc)(void *, size_t z);
    void *(*calloc)(void *, size_t z, size_t n);
    void *(*realloc)(void *, void *ptr, size_t z);
    void (*free)(void *, void *);
} jxr_alloc;

/* JPEG XR CONTAINER */

/*
* The container type represents an optional container that may
* contain 1 or more JPEG XR images. Create the jxr_create_container
* function and free them with jxr_destroy_container. The functions
* below can be used to extract the image from the container.
*/
typedef struct jxr_container *jxr_container_t;

JXR_EXTERN jxr_container_t jxr_create_container(void);
JXR_EXTERN jxr_container_t jxr_create_container_alloc(jxr_alloc *alloc);
JXR_EXTERN void jxr_destroy_container(jxr_container_t c);

#define NUM_GUIDS 79
extern unsigned char jxr_guids[NUM_GUIDS][16];

typedef enum JXRC_GUID_e{
    JXRC_FMT_24bppRGB = 0,
    JXRC_FMT_24bppBGR,
    JXRC_FMT_32bppBGR,
    JXRC_FMT_48bppRGB,
    JXRC_FMT_48bppRGBFixedPoint,
    JXRC_FMT_48bppRGBHalf,
    JXRC_FMT_96bppRGBFixedPoint,
    JXRC_FMT_64bppRGBFixedPoint,
    JXRC_FMT_64bppRGBHalf,
    JXRC_FMT_128bppRGBFixedPoint,
    JXRC_FMT_128bppRGBFloat,
    JXRC_FMT_32bppBGRA,
    JXRC_FMT_64bppRGBA,
    JXRC_FMT_64bppRGBAFixedPoint,
    JXRC_FMT_64bppRGBAHalf,
    JXRC_FMT_128bppRGBAFixedPoint,
    JXRC_FMT_128bppRGBAFloat,
    JXRC_FMT_32bppPBGRA,
    JXRC_FMT_64bppPRGBA,
    JXRC_FMT_128bppPRGBAFloat,
    JXRC_FMT_32bppCMYK,
    JXRC_FMT_40bppCMYKAlpha,
    JXRC_FMT_64bppCMYK,
    JXRC_FMT_80bppCMYKAlpha,
    JXRC_FMT_24bpp3Channels,
    JXRC_FMT_32bpp4Channels,
    JXRC_FMT_40bpp5Channels,
    JXRC_FMT_48bpp6Channels,
    JXRC_FMT_56bpp7Channels,
    JXRC_FMT_64bpp8Channels,
    JXRC_FMT_32bpp3ChannelsAlpha,
    JXRC_FMT_40bpp4ChannelsAlpha,
    JXRC_FMT_48bpp5ChannelsAlpha,
    JXRC_FMT_56bpp6ChannelsAlpha,
    JXRC_FMT_64bpp7ChannelsAlpha,
    JXRC_FMT_72bpp8ChannelsAlpha,
    JXRC_FMT_48bpp3Channels,
    JXRC_FMT_64bpp4Channels,
    JXRC_FMT_80bpp5Channels,
    JXRC_FMT_96bpp6Channels,
    JXRC_FMT_112bpp7Channels,
    JXRC_FMT_128bpp8Channels,
    JXRC_FMT_64bpp3ChannelsAlpha,
    JXRC_FMT_80bpp4ChannelsAlpha,
    JXRC_FMT_96bpp5ChannelsAlpha,
    JXRC_FMT_112bpp6ChannelsAlpha,
    JXRC_FMT_128bpp7ChannelsAlpha,
    JXRC_FMT_144bpp8ChannelsAlpha,
    JXRC_FMT_8bppGray,
    JXRC_FMT_16bppGray,
    JXRC_FMT_16bppGrayFixedPoint,
    JXRC_FMT_16bppGrayHalf,
    JXRC_FMT_32bppGrayFixedPoint,
    JXRC_FMT_32bppGrayFloat,
    JXRC_FMT_BlackWhite,
    JXRC_FMT_16bppBGR555,
    JXRC_FMT_16bppBGR565,
    JXRC_FMT_32bppBGR101010,
    JXRC_FMT_32bppRGBE,
    JXRC_FMT_32bppCMYKDIRECT,
    JXRC_FMT_64bppCMYKDIRECT,
    JXRC_FMT_40bppCMYKDIRECTAlpha,
    JXRC_FMT_80bppCMYKDIRECTAlpha,
    JXRC_FMT_12bppYCC420,
    JXRC_FMT_16bppYCC422,
    JXRC_FMT_20bppYCC422,
    JXRC_FMT_32bppYCC422,
    JXRC_FMT_24bppYCC444,
    JXRC_FMT_30bppYCC444,
    JXRC_FMT_48bppYCC444,
    JXRC_FMT_48bppYCC444FixedPoint,
    JXRC_FMT_20bppYCC420Alpha,
    JXRC_FMT_24bppYCC422Alpha,
    JXRC_FMT_30bppYCC422Alpha,
    JXRC_FMT_48bppYCC422Alpha,
    JXRC_FMT_32bppYCC444Alpha,
    JXRC_FMT_40bppYCC444Alpha,
    JXRC_FMT_64bppYCC444Alpha,
    JXRC_FMT_64bppYCC444AlphaFixedPoint,
}jxrc_t_pixelFormat;


#define UNDEF_GUID (NUM_GUIDS + 1)
/*
* Given a clean container handle, read the specified file (open for
* read) to get all the characteristics of the container. This just
* opens and parses the container, it does *not* decompress the
* image, or even necessarily read the image data. That is left for
* the jxr_read_image_bitstream below. Instead, this function just
* collects all the data in the container tags and makes it available
* via the jxrc_* functions below.
*
* JXR_EC_BADMAGIC
* The magic number is wrong, implying that it is not an JPEG XR
* container. Perhaps it is a bitstream?
*
* NOTE: This call automatically detects an ISO based box format
* and then branches off to the call below.
*/
JXR_EXTERN int jxr_read_image_container(jxr_container_t c, FILE*fd);

/*
 * Read an ISO boxed-base format. Usually, no need to care about this
 * yourself as the above call jumps to it itself.
 * However, if you want to call it yourself, it expects that the JP2
 * signature box is already parsed off.
 */
JXR_EXTERN int jxr_read_image_container_boxed(jxr_container_t c, FILE*fd);

/*
* jxr_c_image_count returns the number of images in this
* container. The images are then numbers starting from 0, with 0
* intended to be the default view.
*/
JXR_EXTERN int jxrc_image_count(jxr_container_t c);

/* File position for image in container. */
JXR_EXTERN unsigned long jxrc_image_offset(jxr_container_t c, int image);
/* Byte count for image in container. */
JXR_EXTERN unsigned long jxrc_image_bytecount(jxr_container_t c, int image);
/* File position for alpha image plane in container. */
JXR_EXTERN unsigned long jxrc_alpha_offset(jxr_container_t c, int image);
/* Byte count for alpha image in container. */
JXR_EXTERN unsigned long jxrc_alpha_bytecount(jxr_container_t c, int image);
/* Image width in container*/
JXR_EXTERN unsigned long jxrc_image_width(jxr_container_t container, int image);
/* Profile/Level in the container */
JXR_EXTERN int jxrc_profile_level_container(jxr_container_t container, int image, unsigned char * profile, unsigned char * level);
/* Image height in container*/
JXR_EXTERN unsigned long jxrc_image_height(jxr_container_t container, int image);
/* Spatial transfrom primary in container*/
JXR_EXTERN unsigned long jxrc_spatial_xfrm_primary(jxr_container_t container, int image);
/* Width resolution in container */
JXR_EXTERN float jxrc_width_resolution(jxr_container_t container, int image);
/* Height resolution in container */
JXR_EXTERN float jxrc_height_resolution(jxr_container_t container, int image);
/* Image band presence in container */
JXR_EXTERN unsigned char jxrc_image_band_presence(jxr_container_t container, int image);
/* Alpha band presence in container */
JXR_EXTERN unsigned char jxrc_alpha_band_presence(jxr_container_t container, int image);
/* Image type in container */
JXR_EXTERN unsigned long jxrc_image_type(jxr_container_t container, int image);
/* PTM Color Info in container */
JXR_EXTERN int jxrc_ptm_color_info(jxr_container_t container, int image, unsigned char * buf);
/* Color space info in container */
JXR_EXTERN unsigned short jxrc_color_space(jxr_container_t container, int image);
/* Padding information in the container, returns 1 if a padding channel is indicated */
JXR_EXTERN int jxrc_includes_padding_channel(jxr_container_t container);
/* Returns true in case it is suggested to place the blue channel at the lower memory location -
** note that this is only an output suggestion a decoder might or might not respect. Typically,
** a decoder will, of course, place the color channel where the hardware requires it and not
** where the file indicates it.
*/
JXR_EXTERN int jxrc_blue_first(jxr_container_t container);
/*
** return a TIFF photometric interpretation indicator:
** WhiteIsZero 0
** BlackIsZero 1
** RGB 2
** RGB Palette 3 (not used)
** Transparency mask 4
** CMYK 5
** YCbCr 6
** CIELab 8
*/
JXR_EXTERN int jxrc_photometric_interpretation(jxr_container_t container);
/*
** Return 1 if the samples are integers
*/
JXR_EXTERN int jxrc_integer_samples(jxr_container_t container);
/*
** Return 1 if the samples are fixpoint
*/
JXR_EXTERN int jxrc_fixpoint_samples(jxr_container_t container);
/*
** Return 1 if the samples are floating point
*/
JXR_EXTERN int jxrc_float_samples(jxr_container_t container);
/*
** Return 1 if the samples are exponent/mantissa separated, i.e. RGBE
*/
JXR_EXTERN int jxrc_exponent_mantissa_samples(jxr_container_t container);
/*
** Return the number of color channels available in total, not including alpha channels
*/
JXR_EXTERN int jxrc_color_channels(jxr_container_t container);
/*
** Return the number of bits per channel. Return 6 for the 565 mode.
*/
JXR_EXTERN int jxrc_bits_per_channel(jxr_container_t container);

/* UTF8 strings in container */
JXR_EXTERN int jxrc_document_name(jxr_container_t container, int image, char ** string);
JXR_EXTERN int jxrc_image_description(jxr_container_t container, int image, char ** string);
JXR_EXTERN int jxrc_equipment_make(jxr_container_t container, int image, char ** string);
JXR_EXTERN int jxrc_equipment_model(jxr_container_t container, int image, char ** string);
JXR_EXTERN int jxrc_page_name(jxr_container_t container, int image, char ** string);
JXR_EXTERN int jxrc_software_name_version(jxr_container_t container, int image, char ** string);
JXR_EXTERN int jxrc_date_time(jxr_container_t container, int image, char ** string);
JXR_EXTERN int jxrc_artist_name(jxr_container_t container, int image, char ** string);
JXR_EXTERN int jxrc_host_computer(jxr_container_t container, int image, char ** string);
JXR_EXTERN int jxrc_copyright_notice(jxr_container_t container, int image, char ** string);
/* page number in container */
JXR_EXTERN int jxrc_page_number(jxr_container_t container, int image, unsigned short * value);
/* padding data in container */
JXR_EXTERN int jxrc_padding_data(jxr_container_t container, int image);

/*
* When writing an image file, start with a call to
* jxrc_start_file. This binds the FILE pointer to the container and
* gets the file pointers started.
*
* Then use the jxrc_begin_ifd_entry() function to create the next ifd
* entry. This makes the next IFD ready for its settings. Load the
* settings to reflect the image using the jxrc_set_* functions.
*
* When ready to start writing the image, use the jxr_begin_image_data
* to close the ifd and start writing the image data. At this point
* the caller can write the bit stream data itself.
*
* The jxrc_write_container_post, finalizes the
* IFD value for IMAGE_BYTE_COUNT and IMAGE_OFFSET and leaves the pointer ready for the end of the image.
*
* The jxrc_write_container_post_alpha, finalizes the
* IFD value for ALPHA_BYTE_COUNT and ALPHA_OFFSET and leaves the pointer ready for the end of the image.
*
*/

JXR_EXTERN int jxrc_start_file(jxr_container_t c, FILE*fd);
JXR_EXTERN int jxrc_start_file_boxed(jxr_container_t c, FILE*fd);
JXR_EXTERN int jxrc_begin_ifd_entry(jxr_container_t c);
JXR_EXTERN int jxrc_set_pixel_format(jxr_container_t c, jxrc_t_pixelFormat fmt);
JXR_EXTERN jxrc_t_pixelFormat jxrc_get_pixel_format(jxr_container_t c);
JXR_EXTERN int jxrc_set_image_shape(jxr_container_t c, unsigned wid, unsigned hei);
JXR_EXTERN int jxrc_set_image_band_presence(jxr_container_t cp, unsigned bands);
JXR_EXTERN int jxrc_set_separate_alpha_image_plane(jxr_container_t cp, unsigned int alpha_present);
JXR_EXTERN int jxrc_set_image_profile(jxr_container_t cp,int profile);
JXR_EXTERN int jxrc_set_inverted_bw(jxr_container_t cp,int inverted);
JXR_EXTERN int jxrc_begin_image_data(jxr_container_t c);
JXR_EXTERN int jxrc_begin_image_data_boxed(jxr_container_t c);
JXR_EXTERN int jxrc_write_container_post(jxr_container_t c);
JXR_EXTERN int jxrc_write_container_post_boxed(jxr_container_t c);
JXR_EXTERN int jxrc_write_container_post_alpha(jxr_container_t c);
JXR_EXTERN int jxrc_write_container_post_alpha_boxed(jxr_container_t c);

/* JPEG XR BITSTREAM */

/*
* The jxr_image_t is an opaque image type that represents an JPEG-XR
* bitstream image. It holds the state for various of the following
* operations.
*
* For reading (decompress), the general process goes like this:
*
* - Create an jxr_image_t object with the jxr_create_input() function,
*
* - Attach a callback function to receive blocks of image data with
* the jxr_set_block_output() function, then
*
* - Run the decompress with the jxr_read_image_bitstream() function.
*
* - Destroy the jxr_image_T object with the jxr_destroy() function.
*
* The jxr_read_image_bitstream() function calls the user block
* function periodically with data for a macroblock (16x16 pixels) of
* the image. The callback function dispatches the data, for example
* by formatting it into an image buffer or writing it to an output
* file or rendering it to the screen.
*/
typedef struct jxr_image *jxr_image_t;

/*
* Create (destroy) an JPEG XR image cookie.
*
* jxr_create_image -
* Create an jxr_image_t handle that will be used for writing an
* image out.
*
* jxr_create_input -
* Create an jxr_image_t handle that can be used to read an image.
*
* jxr_destroy -
* Destroy an jxr_image_t object.
*/
JXR_EXTERN jxr_image_t jxr_create_image_alloc(jxr_alloc *alloc, int width, int height, unsigned char * windowing);
JXR_EXTERN jxr_image_t jxr_create_input_alloc(jxr_alloc *alloc);
JXR_EXTERN jxr_image_t jxr_create_image(int width, int height, unsigned char * windowing);
JXR_EXTERN jxr_image_t jxr_create_input(void);
JXR_EXTERN void jxr_destroy(jxr_image_t image);

/*
* Some user-controlled flags.
*/
JXR_EXTERN void jxr_flag_SKIP_HP_DATA(jxr_image_t image, int flag);
JXR_EXTERN void jxr_flag_SKIP_FLEX_DATA(jxr_image_t image, int flag);

/*
* Applications may attach a single pointer to the jxr_image_t handle,
* and retrieve it anytime, including within callbacks. The user data
* is intended to be used by the callbacks to get information about
* the context of the image process.
*/
JXR_EXTERN void jxr_set_user_data(jxr_image_t image, void*data);
JXR_EXTERN void*jxr_get_user_data(jxr_image_t image);

/*
* Functions for getting/setting various flags of the image. These
* either reflect the image that has been read, or controls how to
* write the image out.
*
* IMAGE_WIDTH/IMAGE_HEIGHT
* Dimensions of the image.
*
* IMAGE_CHANNELS
* Number of channels in the image (not including alpha). For
* example, 1==GRAY, 3==RGB.
*
* TILING_FLAG
* TRUE if tiles are present in the image.
*
* FREQUENCY_MODE_CODESTREAM_FLAG
* 0 == Spatial mode
* 1 == Frequency mode
*
* TILE_WIDTH/TILE_HEIGHT
* Dimensions of the tile rows/columns if present in the image, or
* 0 otherwise. The widths/heights of all the columns/rows will
* always be a multiple of 16 (the size of a macroblock).
*
* WINDOWING_FLAG
* TRUE if there are windowin parameters in the image.
*
* ALPHACHANNEL_FLAG
* TRUE if there is an alpha channel present.
*
* NOTE about setting flags: In many cases, the order that you set
* things matters as internal configuration is devined from previous
* values. So the order that jxr_set_* functions are listed in this
* file is the order they should be applied to a file that is being
* built up for output.
*/
JXR_EXTERN unsigned jxr_get_IMAGE_WIDTH(jxr_image_t image);
JXR_EXTERN unsigned jxr_get_IMAGE_HEIGHT(jxr_image_t image);
JXR_EXTERN unsigned jxr_get_EXTENDED_IMAGE_WIDTH(jxr_image_t image);
JXR_EXTERN unsigned jxr_get_EXTENDED_IMAGE_HEIGHT(jxr_image_t image);
/* WARNING: This call returns the number of channels in the codestream,
** which is *likely* not the information you care about. Instead, the
** number of components is defined through the pixel format in the
** container.
*/
JXR_EXTERN int jxr_get_IMAGE_CHANNELS(jxr_image_t image);
/* While the above returns the number of channels encoded in the codestream,
** the following returns the nominal number of channels indicated in the
** component. Note that this might be different because Y-Only is enabled
** and thus everything but the first channel is missing. Bummer!
*/
JXR_EXTERN int jxrc_get_CONTAINER_CHANNELS(jxr_container_t container);
JXR_EXTERN int jxr_get_TILING_FLAG(jxr_image_t image);
JXR_EXTERN int jxr_get_FREQUENCY_MODE_CODESTREAM_FLAG(jxr_image_t image);
JXR_EXTERN unsigned jxr_get_TILE_COLUMNS(jxr_image_t image);
JXR_EXTERN unsigned jxr_get_TILE_ROWS(jxr_image_t image);
JXR_EXTERN int jxr_get_TILE_WIDTH(jxr_image_t image, unsigned column);
JXR_EXTERN int jxr_get_TILE_HEIGHT(jxr_image_t image, unsigned row);

/*
** Check whether the codestream includes an interleaved alpha channel.
** WARNING: This is most likely not what you care about. In the JXR
** file format, you may also have a separate alpha channel in a second
** codestream.
*/
JXR_EXTERN int jxr_get_ALPHACHANNEL_FLAG(jxr_image_t image);

/*
** Check whether the alpha channel in the codestream is premultiplied.
** WARNING: See above, this is probably not what you care about!
*/
JXR_EXTERN int jxr_get_ALPHA_PREMULTIPLIED_FLAG(jxr_image_t image);

JXR_EXTERN int jxr_get_ALPHA_PRESENCE_FLAG(jxr_image_t image);


/*
** Check whether the container format includes an alpha channel
** flag. This alpha channel can be either realized as separate or as
** interleaved alpha channel. This call returns 0 for no alpha channel,
** 1 for the regular alpha channel or 2 for the premultiplied case.
*/
JXR_EXTERN int jxrc_get_CONTAINER_ALPHA_FLAG(jxr_container_t con);

/*
* This is the "Internal" color format of the image. It is the color
* mode that is used to encode the image data. These are the only
* formats that JPEG XR supports directly. This is not the same as
* the EXTERNAL color format, which is the format that the user
* presents/receives the data in. The available external formats are a
* larger set.
*/
typedef enum jxr_color_fmt_e{
    JXR_YONLY = 0,
    JXR_YUV420 = 1,
    JXR_YUV422 = 2,
    JXR_YUV444 = 3,
    JXR_YUVK = 4,
    JXR_fmt_reserved5 = 5,
    JXR_NCOMPONENT = 6,
    JXR_fmt_reserved7 = 7
}jxr_color_fmt_t;

JXR_EXTERN void jxr_set_INTERNAL_CLR_FMT(jxr_image_t image, jxr_color_fmt_t fmt, int channels,int alpha_channels);

/*
* This is the "output" color format of the image.
*/
typedef enum jxr_output_clr_fmt_e{
    JXR_OCF_YONLY = 0,
    JXR_OCF_YUV420 = 1,
    JXR_OCF_YUV422 = 2,
    JXR_OCF_YUV444 = 3,
    JXR_OCF_CMYK = 4,
    JXR_OCF_CMYKDIRECT = 5,
    JXR_OCF_NCOMPONENT = 6,
    JXR_OCF_RGB = 7,
    JXR_OCF_RGBE = 8,
    JXR_OCF_fmt_reserved9 = 9,
    JXR_OCF_fmt_reserved10 = 10,
    JXR_OCF_fmt_reserved11 = 11,
    JXR_OCF_fmt_reserved12 = 12,
    JXR_OCF_fmt_reserved13 = 13,
    JXR_OCF_fmt_reserved14 = 14,
    JXR_OCF_fmt_reserved15 = 15
}jxr_output_clr_fmt_t;

JXR_EXTERN void jxr_set_OUTPUT_CLR_FMT(jxr_image_t image, jxr_output_clr_fmt_t fmt);
JXR_EXTERN jxr_output_clr_fmt_t jxr_get_OUTPUT_CLR_FMT(jxr_image_t image);


/*
* This is the bit depth to use.
*/
typedef enum jxr_bitdepth_e{
    JXR_BD1WHITE1 = 0,
    JXR_BD8 = 1,
    JXR_BD16 = 2,
    JXR_BD16S = 3,
    JXR_BD16F = 4,
    JXR_BDRESERVED = 5,
    JXR_BD32S = 6,
    JXR_BD32F = 7,
    JXR_BD5 = 8,
    JXR_BD10 = 9,
    JXR_BD565 = 10,
    JXR_BD1BLACK1 = 15
}jxr_bitdepth_t;

JXR_EXTERN jxr_bitdepth_t jxr_get_OUTPUT_BITDEPTH(jxr_image_t image);
JXR_EXTERN void jxr_set_OUTPUT_BITDEPTH(jxr_image_t image, jxr_bitdepth_t bd);
JXR_EXTERN void jxr_set_SHIFT_BITS(jxr_image_t image, unsigned char shift_bits);
JXR_EXTERN void jxr_set_FLOAT(jxr_image_t image, unsigned char len_mantissa, char exp_bias);

/*
* This sets the frequency bands that are to be included in the
* image. The more bands are included, the higher the fidelity of the
* compression, but also the larger the image result.
*
* If FLEXBITS are included, the TRIM_FLEXBITS value can be used to
* reduce the amount of FLEXBIT data included. If 0 (the default) then
* all the flexbit data is included, if trim==1, then 1 bit of flexbit
* data is trimmed, and so on. TRIM_FLECBITS must be 0 <= TRIM_FLEXBITS <= 15.
*/
typedef enum jxr_bands_present_e{
    JXR_BP_ALL = 0,
    JXR_BP_NOFLEXBITS = 1,
    JXR_BP_NOHIGHPASS = 2,
    JXR_BP_DCONLY = 3,
    JXR_BP_ISOLATED = 4
}jxr_bands_present_t;

JXR_EXTERN void jxr_set_BANDS_PRESENT(jxr_image_t image, jxr_bands_present_t bp);
JXR_EXTERN void jxr_set_TRIM_FLEXBITS(jxr_image_t image, int trim);
/* Filtering
* Control overlap filtering with this function.
*
* - 0
* No overlap filtering. (The default)
*
* - 1
* Filter only the first stage, right before the first lifting pass.
*
* - 2
* Filter first and second stage.
*
* - 3
* Reserved
*
* All other values for flag are out of range.
*/
JXR_EXTERN void jxr_set_OVERLAP_FILTER(jxr_image_t image, int flag);

/* HardTiles
* Controls overlap filtering on tile boundaries
*
* - 0
* Corresonds to soft tiles. Overlap Filtering is appiled across tile boundaries. (The default)
*
* -1
* Corresonds to hard tiles. Overlap Filtering is not appiled across tile boundaries. (The default)
* Instead, tile boundaries and corners are treated like image boundaries and image corners
*
* All other values are out of range
*/
JXR_EXTERN void jxr_set_DISABLE_TILE_OVERLAP(jxr_image_t image, int flag);
JXR_EXTERN void jxr_set_FREQUENCY_MODE_CODESTREAM_FLAG(jxr_image_t image, int flag);
JXR_EXTERN void jxr_set_INDEX_TABLE_PRESENT_FLAG(jxr_image_t image, int flag);
JXR_EXTERN void jxr_set_ALPHA_IMAGE_PLANE_FLAG(jxr_image_t image, int flag, int premultiplied);
JXR_EXTERN void jxr_set_PROFILE_IDC(jxr_image_t image, int profile_idc);
JXR_EXTERN void jxr_set_LEVEL_IDC(jxr_image_t image, int level_idc);
JXR_EXTERN void jxr_set_LONG_WORD_FLAG(jxr_image_t image, int flag);
JXR_EXTERN int jxr_test_PROFILE_IDC(jxr_image_t image, int flag);
JXR_EXTERN int jxr_test_LEVEL_IDC(jxr_image_t image, int flag);

JXR_EXTERN void jxr_set_NUM_VER_TILES_MINUS1(jxr_image_t image, unsigned num);
JXR_EXTERN void jxr_set_TILE_WIDTH_IN_MB(jxr_image_t image, unsigned* list);
JXR_EXTERN void jxr_set_NUM_HOR_TILES_MINUS1(jxr_image_t image, unsigned num);
JXR_EXTERN void jxr_set_TILE_HEIGHT_IN_MB(jxr_image_t image, unsigned* list);
JXR_EXTERN void jxr_set_TILING_FLAG(jxr_image_t image, int flag);

/*
** Set chroma centering
*/
JXR_EXTERN void jxr_set_CHROMA_CENTERING(jxr_image_t image,unsigned x,unsigned y);

/*
** Interchange red and blue in the color transform for the
** BD555,BD565 and BD101010 flag.
** NOTE: This is only for legacy implementations. All new
** implementations should follow the specs and handle the
** data correctly.
**
** The problem happens because HDPhoto defined the blue to be
** in the MSBs of the packed formats, whereas the standard DIB
** formats expect blue in the MSB. If you just feed in data
** blindly without noting the difference, you get incorrect
** images that just appear correctly because the same mistake
** is made at the decoder side.
** The latest version of the specs do account for such errors
** and allows this, which is then indicated by a flag.
** This call modifies the encoding appropriately to address the
** needs of legacy decoders.
**
** You should actually *not* use this flag but rather write your
** applications be aware of the correct color ordering and the
** corresponding flag.
*/
JXR_EXTERN void jxr_set_R_B_swapped(jxr_image_t image,int bgr_flag);


/*
* It is a consequence of the JXR format that an image can have no
* more then 16 channels. This is because the channels count field is
* only 4 bits wide. This is also true of NUM_LP_QPS and
* NUM_HP_QPS. These constants can thus be used to size so tables.
*/

# define MAX_CHANNELS 16
# define MAX_LP_QPS 16
# define MAX_HP_QPS 16

/* Quantization
* These are functions to set up the quantization to be used for
* encoding. Only one of these can be used for a given image. The
* control over the QP values depends on which function you use.
*
* - set_QP_LOSSLESS()
* Configure the QP values for lossless compression. This will also
* set up the SCALED_FLAG appropriately. Note that this is not really
* lossless unless all the bands are present.
*
* NOTE 1: DC/LP/HP_IMAGEPLANE_UNIFORM is set to true because the
* quantization value is uniform identity.
*
* NOTE 2: Grayscale images will be configured with channel mode
* UNIFORM.
*
* NOTE 3: Although lossless can be achieved with channel modes of
* UNIFORM, SEPARATE and INDEPENDENT, this function will set
* the channel mode to SEPARATE for color images.
*
* - set_QP_UNIFORM()
* All the bands/channels are set with the same QP value. If the
* use_dc_only flag it false, then the DC/LP/HP values are identical,
* but encoded distinctly.
*
* NOTE 1: DC/LP/HP_IMAGEPLANE_UNIFORM is set to true so that the QP
* are kept in the plane header.
*
* - set_QP_SEPARATE
* All the bands are set with the same QP value, depending on the
* component. The Y component gets the first quant value, and the
* remaining channels get the other quant value.
*
* NOTE 1: DC/LP/HP_IMAGEPLANE_UNIFORM is set to true so that the QP
* are kept in the plane header.
*
* NOTE 2: This should NOT be applied to grayscale images. Images
* with 1 channel must have channel mode UNIFORM.
*
* - set_QP_INDEPENDENT
* All the bands are set with the same QP value, but each channel gets
* its own QP value.
*
* NOTE 1: DC/LP/HP_IMAGEPLANE_UNIFORM is set to true so that the QP
* are kept in the plane header.
*
* NOTE 2: For single channel images (i.e. grayscale) this actually
* sets the channel mode to UNIFORM. Only the UNIFORM
* channel mode is valid for grayscale.
*
* - set_QP_DISTRIBUTED
* This is the most complex form. Every tile/channel can have its own
* QP set, and that QP set is mapped onto each
* tile/channel/macroblock. The argument to the set_QP_DISTRIBUTED
* function is an array of jxr_time_qp objects, one object per tile in
* the complete image.
*
* NOTE 1: The tile configuration must be set before this function is
* called. If no tile configuration is set, then there is
* exactly 1 tile.
* NOTE 2: set_QP_DISTRIBUTED causes DC/LP/HP_IMAGEPLANE_UNIFORM to be
* false so that the QP values are moved to tile headers.
*/

JXR_EXTERN void jxr_set_QP_LOSSLESS(jxr_image_t image);
JXR_EXTERN void jxr_set_QP_UNIFORM(jxr_image_t image, unsigned char quant);
JXR_EXTERN void jxr_set_QP_SEPARATE(jxr_image_t image, unsigned char*quant_y_uv);
JXR_EXTERN void jxr_set_QP_INDEPENDENT(jxr_image_t image, unsigned char*quant_per_channel);


struct jxr_tile_channel_qp{
    /* These are all the QP values available for the tile. A
    tile/channel can have only 1 unique DC QP value, and up to
    16 (each) LP and HP QP values. */
    unsigned char dc_qp;
    unsigned char num_lp, num_hp; /* Number for LP/HP QP values. */
    unsigned char lp_qp[16];
    unsigned char hp_qp[16];
};

typedef enum jxr_component_mode_e{
    JXR_CM_UNIFORM = 0,
    JXR_CM_SEPARATE = 1,
    JXR_CM_INDEPENDENT = 2,
    JXR_CM_Reserved = 3
}jxr_component_mode_t;

struct jxr_tile_qp{
    /* For every channel, have an jxr_tile_qp for all the tiles in
    the image. Each jxr_tile_channel_qp represents the QP
    values for a tile/channel. There are (up to) 16 channels,
    and each channel points to an array of jxr_tile_qp
    objects, depending on the component_mode. If the mode is
    UNIFORM, then there is only one channel of data that is
    shared for all components, etc. */
    jxr_component_mode_t component_mode;
    struct jxr_tile_channel_qp channel[16];
    /* Map the LP/HP QP values to the macroblocks. There are as
    many bytes in each array as there are macroblocks in the
    tile, and the value at each byte is the index into the
    table above. This is how the QP values are mapped to
    macroblocks. */
    unsigned char*lp_map;
    unsigned char*hp_map;
    /* Per tile Quantization parameters for up to 16 channels. This is required when quantization step sizes vary across tiles */
    unsigned char dc_quant_ch[MAX_CHANNELS];
    unsigned char lp_quant_ch[MAX_CHANNELS][MAX_LP_QPS];
    unsigned char hp_quant_ch[MAX_CHANNELS][MAX_HP_QPS];
};

typedef struct raw_info {
    unsigned int is_raw;
    unsigned int raw_width;
    unsigned int raw_height;
    unsigned char raw_format;
    unsigned char raw_bpc;
} raw_info;

JXR_EXTERN void jxr_set_QP_DISTRIBUTED(jxr_image_t image, struct jxr_tile_qp*qp);


/*
* Callbacks
*
* jxr_set_block_output -
* During read of a compressed bitstream, the block_output function
* is used to report that a 16x16 macroblock is complete. The
* application is called back via a function of type block_fun_t to
* receive and dispense with the data.
*/
typedef void (*block_fun_t)(jxr_image_t image, int mx, int my, int*data);

JXR_EXTERN void jxr_set_block_input (jxr_image_t image, block_fun_t fun);
JXR_EXTERN void jxr_set_block_output(jxr_image_t image, block_fun_t fun);

/*
* After the jxr_image_t object is all set up, the
* jxr_read_image_bitstream function is called to read the bitstream
* and decompress it. This function assumes that the fd is open for
* read and set to the beginning of the bitstream (where the magic
* number starts).
*
* JXR_EC_BADMAGIC
* The magic number is wrong, implying that it is not an JPEG XR
* bitstream.
*/
JXR_EXTERN int jxr_read_image_bitstream(jxr_image_t image, FILE*fd);

/*
** thor: Added April 2nd 2010: Initialize for stripe by stripe reading.
*/
JXR_EXTERN int jxr_init_read_stripe_bitstream(jxr_image_t image, FILE *fd);
JXR_EXTERN int jxr_read_stripe_bitstream(jxr_image_t image);

/*
* After decoder is run, test desired LONG_WORD_FLAG against
* calculations done in decoder
*/
JXR_EXTERN int jxr_test_LONG_WORD_FLAG(jxr_image_t image, int flag);

/*
* Given an image cookie that represents an image, and an fd opened
* for write, write the entire JPEG XR bit stream.
*/
JXR_EXTERN int jxr_write_image_bitstream(jxr_image_t image, FILE*fd);


/*
* jxr error codes. Many of the above functions return a positive
* (>=0) value or a negative error code. The error codes are:
*/

# define JXR_EC_OK (0) /* No error */
# define JXR_EC_ERROR (-1) /* Unspecified error */
# define JXR_EC_BADMAGIC (-2) /* Stream lacks proper magic number. */
# define JXR_EC_FEATURE_NOT_IMPLEMENTED (-3)
# define JXR_EC_IO (-4) /* Error reading/writing data */
# define JXR_EC_BADFORMAT (-5) /* Bad file format */

/*
** Added by thor April 2nd 2010: No memory left. I hope I got all the places,
** but the code seems to be ill-prepared to handle OOM cases.
*/
# define JXR_EC_NOMEM (-6)

/*
** Added by thor April 2nd 2010: Not really an error code, but an indication
** that the last stripe has been decoded in line based mode and no further data
** is there.
*/
# define JXR_EC_DONE (-256)

#undef JXR_EXTERN

/*
* $Log: jpegxr.h,v $
* Revision 1.14  2012-05-17 12:42:57  thor
* Bumped to 1.41, fixed PNM writer, extended the API a bit.
*
* Revision 1.13  2012-03-19 15:48:19  thor
* Fixed YOnly and the container_nc field of the image to contain always
* the correct number of container components including the alpha channel.
*
* Revision 1.12  2012-02-16 16:36:26  thor
* Heavily reworked, but not yet tested.
*
* Revision 1.11  2012-02-14 22:06:36  thor
* Started box parser.
*
* Revision 1.10  2012-02-13 17:36:50  thor
* Fixed syntax errors, not yet debugged. Added option for box output.
*
* Revision 1.9  2012-02-11 04:23:31  thor
* Start of the box-based writer.
*
* Revision 1.8  2011-11-24 11:44:09  thor
* Added an R-B swap flag.
*
* Revision 1.7  2011-04-28 08:45:43  thor
* Fixed compiler warnings, ported to gcc 4.4, removed obsolete files.
*
* Revision 1.6  2011-03-04 12:12:12  thor
* Bumped to 1.16. Fixed RGB-YOnly handling, including the handling of
* YOnly for which a new -f flag has been added.
*
* Revision 1.5  2010-05-13 16:30:03  thor
* Added options to set the chroma centering. Fixed writing of BGR565.
* Made the "-p" output option nicer.
*
* Revision 1.4  2010-05-01 11:16:08  thor
* Fixed the tiff tag order. Added spatial/line mode.
*
* Revision 1.3  2010-03-31 07:50:58  thor
* Replaced by the latest MS version.
*
* Revision 1.28 2009/05/29 12:00:00 microsoft
* Reference Software v1.6 updates.
*
* Revision 1.27 2009/04/13 12:00:00 microsoft
* Reference Software v1.5 updates.
*
* Revision 1.26 2008/03/18 18:36:56 steve
* Support compress of CMYK images.
*
* Revision 1.25 2008/03/18 15:50:08 steve
* Gray images must be UNIFORM
*
* Revision 1.24 2008/03/06 02:05:48 steve
* Distributed quantization
*
* Revision 1.23 2008/03/05 04:04:30 steve
* Clarify constraints on USE_DC_QP in image plane header.
*
* Revision 1.22 2008/03/05 01:27:15 steve
* QP_UNIFORM may use USE_DC_LP optionally.
*
* Revision 1.21 2008/03/05 00:31:17 steve
* Handle UNIFORM/IMAGEPLANE_UNIFORM compression.
*
* Revision 1.20 2008/03/04 23:01:28 steve
* Cleanup QP API in preparation for distributed QP
*
* Revision 1.19 2008/03/02 19:56:27 steve
* Infrastructure to read write BD16 files.
*
* Revision 1.18 2008/03/01 02:46:09 steve
* Add support for JXR container.
*
* Revision 1.17 2008/02/29 01:29:57 steve
* Mark more extern functions for MSC.
*
* Revision 1.16 2008/02/28 18:50:31 steve
* Portability fixes.
*
* Revision 1.15 2008/02/26 23:52:44 steve
* Remove ident for MS compilers.
*
* Revision 1.14 2008/02/26 23:28:53 steve
* Remove C99 requirements from the API.
*
* Revision 1.13 2008/02/01 22:49:53 steve
* Handle compress of YUV444 color DCONLY
*
* Revision 1.12 2008/01/08 01:06:20 steve
* Add first pass overlap filtering.
*
* Revision 1.11 2008/01/06 01:29:28 steve
* Add support for TRIM_FLEXBITS in compression.
*
* Revision 1.10 2008/01/04 17:07:35 steve
* API interface for setting QP values.
*
* Revision 1.9 2007/11/30 01:50:58 steve
* Compression of DCONLY GRAY.
*
* Revision 1.8 2007/11/26 01:47:15 steve
* Add copyright notices per MS request.
*
* Revision 1.7 2007/11/08 02:52:32 steve
* Some progress in some encoding infrastructure.
*
* Revision 1.6 2007/11/07 18:11:45 steve
* Missing error code.
*
* Revision 1.5 2007/11/06 22:32:34 steve
* Documentation for the jpegxr library use.
*
* Revision 1.4 2007/09/08 01:01:43 steve
* YUV444 color parses properly.
*
* Revision 1.3 2007/07/30 23:09:57 steve
* Interleave FLEXBITS within HP block.
*
* Revision 1.2 2007/07/21 00:25:48 steve
* snapshot 2007 07 20
*
* Revision 1.1 2007/06/06 17:19:12 steve
* Introduce to CVS.
*
*/

#endif
