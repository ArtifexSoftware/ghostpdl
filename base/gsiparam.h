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


/* Image parameter definition */

#ifndef gsiparam_INCLUDED
#  define gsiparam_INCLUDED

#include "gsccolor.h"		/* for GS_CLIENT_COLOR_MAX_COMPONENTS */
#include "gsmatrix.h"
#include "gsstype.h"		/* for extern_st */
#include "gxbitmap.h"

/* ---------------- Image parameters ---------------- */

/*
 * Unfortunately, we defined the gs_image_t type as designating an ImageType
 * 1 image or mask before we realized that there were going to be other
 * ImageTypes.  We could redefine this type to include a type field without
 * perturbing clients, but it would break implementations of driver
 * begin_image procedures, since they are currently only prepared to handle
 * ImageType 1 images and would have to be modified to check the ImageType.
 * Therefore, we use gs_image_common_t for an abstract image type, and
 * gs_image<n>_t for the various ImageTypes.
 */

/*
 * Define the data common to all image types.  The type structure is
 * opaque here, defined in gxiparam.h.
 */
typedef struct gx_image_type_s gx_image_type_t;

/*  Parent image type enumerations.  Since type3 images can give rise to
    type 1 image types, we want to know the origin of these to avoid
    doing different halftone methods to the image and the mask.  */
typedef enum {
    gs_image_type1,
    gs_image_type2,
    gs_image_type3,
    gs_image_type3x,
    gs_image_type4
} gs_image_parent_t;

/*
 * Define the maximum number of components/planes in image data.
 * The +1 is for either color + alpha or mask + color.
 */
#define GS_IMAGE_MAX_COLOR_COMPONENTS GS_CLIENT_COLOR_MAX_COMPONENTS
#define GS_IMAGE_MAX_COMPONENTS (GS_IMAGE_MAX_COLOR_COMPONENTS + 1)

/*
 * Define the structure for defining data common to ImageType 1 images,
 * ImageType 3 DataDicts and MaskDicts, and ImageType 4 images -- i.e.,
 * all the image types that use explicitly supplied data.  It follows
 * closely the discussion on pp. 219-223 of the PostScript Language
 * Reference Manual, Second Edition, with the following exceptions:
 *
 *      DataSource and MultipleDataSources are not members of this
 *      structure, since the structure doesn't take a position on
 *      how the data are actually supplied.
 */
#define gs_data_image_common\
                /*\
                 * Define the transformation from user space to image space.\
                 */\
        gs_matrix ImageMatrix;\
                /*\
                 * Define the width of source image in pixels.\
                 */\
        int Width;\
                /*\
                 * Define the height of source image in pixels.\
                 */\
        int Height;\
                /*\
                 * Define B, the number of bits per pixel component.\
                 * Currently this must be 1 for masks.\
                 */\
        int BitsPerComponent;\
                /*\
                 * Define the linear remapping of the input values.\
                 * For the I'th pixel component, we start by treating\
                 * the B bits of component data as a fraction F between\
                 * 0 and 1; the actual component value is then\
                 * Decode[I*2] + F * (Decode[I*2+1] - Decode[I*2]).\
                 * For masks, only the first two entries are used;\
                 * they must be 1,0 for write-0s masks, 0,1 for write-1s.\
                 */\
        float Decode[GS_IMAGE_MAX_COMPONENTS * 2];\
                /*\
                 * Define whether to smooth the image.\
                 */\
        bool Interpolate;\
                 /* If set, then the imagematrices have been changed (currently\
                  * just by the pdfwrite device for the purposes of handling\
                  * type3 masked images), so we can't trust them for mapping\
                  * backwards for source clipping. */\
        bool imagematrices_are_untrustworthy;\
                 /* Put the type field last. See why below.*/\
        const gx_image_type_t *type
typedef struct gs_data_image_s {
    gs_data_image_common;
} gs_data_image_t;

/* Ghostscript uses macros to define 'extended' structures.
 * Suppose we want to define a structure foo, that can be extended to
 * bigger_foo (and then maybe even to even_bigger_foo). We first
 * define a macro such as 'foo_common' that contains the fields for
 * foo.
 *
 * #define foo_common \
 *      void *a;\
 *      int b
 *
 * Then we define foo in terms of this:
 *
 * typedef struct {
 *     foo_common;
 * } foo;
 *
 * Then we can extend this to other types as follows:
 *
 * typedef struct {
 *     foo_common;
 *     int c;
 *     int d;
 * } bigger_foo;
 *
 * Or we can use macros to use even further extension:
 *
 * #define bigger_foo_common \
 *      foo_common;\
 *      int c;\
 *      int d
 *
 * typedef struct {
 *     bigger_foo_common;
 * } bigger_foo;
 *
 * and hence:
 *
 * typedef struct {
 *     bigger_foo_common;
 *     int e;
 * } even_bigger_foo;
 *
 * On the whole, this works well, and avoids the extra layer of
 * structure that would occur if we used the more usual structure
 * definition way of working:
 *
 * typedef struct {
 *     void *a;
 *     int   b;
 * } foo;
 *
 * typedef struct {
 *     foo base;
 *     int c;
 *     int d;
 * } bigger_foo;
 *
 * typedef struct {
 *     bigger_foo base;
 *     int e;
 * } even_bigger_foo;
 *
 * In this formulation even_bigger_foo would need to access 'a' as
 * base.base.a, whereas the ghostscript method can just use 'a'.
 *
 * Unfortunately, there is one drawback to this method, to do with
 * structure packing in C. C likes structures to be easily used in
 * arrays. Hence (in a 64bit build), foo will be 16 bytes, where
 * the foo fields included at the start of bigger_foo will only
 * take 12.
 *
 * This means that constructs such as:
 *
 * void simple(foo *a)
 * {
 *    bigger_foo b;
 *    b.c = 1;
 *    *(foo *)b = *a;
 *    ...
 * }
 *
 * where we attempt to initialise the 'foo' fields of a bigger_foo, b, by
 * copying them from an existing 'foo', will corrupt b.c.
 *
 * To allow this idiom to work, we either need to ensure that the largest
 * alignment object in the 'foo' fields comes last, or we need to introduce
 * padding. The former is easier.
 */

#define public_st_gs_data_image() /* in gximage.c */\
  gs_public_st_simple(st_gs_data_image, gs_data_image_t,\
    "gs_data_image_t")

/* Historically these were different. No longer. */
typedef gs_data_image_t gs_image_common_t;

/*
 * Define the data common to ImageType 1 images, ImageType 3 DataDicts,
 * and ImageType 4 images -- i.e., all the image types that provide pixel
 * (as opposed to mask) data.  The following are added to the PostScript
 * image parameters:
 *
 *      format is not PostScript or PDF standard: it is normally derived
 *      from MultipleDataSources.
 *
 *      ColorSpace is added from PDF.
 *
 *      CombineWithColor is not PostScript or PDF standard: see the
 *      RasterOp section of Language.htm for a discussion of
 *      CombineWithColor.
 */
typedef enum {
    /* Single plane, chunky pixels. */
    gs_image_format_chunky = 0,
    /* num_components planes, chunky components. */
    gs_image_format_component_planar = 1,
    /* BitsPerComponent * num_components planes, 1 bit per plane */
    gs_image_format_bit_planar = 2
} gs_image_format_t;

/* Define an opaque type for a color space. */
typedef struct gs_color_space_s gs_color_space;

/* NOTE: Ensure that this macro always ends on a pointer
 * (or on something that will align at least with a pointer).
 * Otherwise you'll get problems on 64bit builds, presumably
 * because something is doing sizeof(this) ? */
#define gs_pixel_image_common\
        gs_data_image_common;\
                /*\
                 * Define how the pixels are divided up into planes.\
                 */\
        gs_image_format_t format;\
                /*\
                 * Define whether to use the drawing color as the\
                 * "texture" for RasterOp.  For more information,\
                 * see the discussion of RasterOp in Language.htm.\
                 */\
        bool CombineWithColor;\
                 /*\
                  * Usually we can tell whether we are in an smask\
                  * by asking the device we are in. Sometimes (like\
                  * when dealing with the masked portion of a type 3\
                  * image), we are using a different device, and so\
                  * can't use that method. Instead the caller will\
                  * indicate it here. */\
        int override_in_smask;\
                /*\
                 * Define the source color space (must be NULL for masks).\
                 *\
                 * Make the pointer the last element of the structure.\
                 * Otherwise, the padding at the end overwrites the 1st\
                 * member of the subclass, when the base structure is assigned\
                 * to the subclass structure. Bugs 613909, 688725\
                 */\
        gs_color_space *ColorSpace

typedef struct gs_pixel_image_s {
    gs_pixel_image_common;
} gs_pixel_image_t;

extern_st(st_gs_pixel_image);
#define public_st_gs_pixel_image() /* in gximage.c */\
  gs_public_st_ptrs1(st_gs_pixel_image, gs_pixel_image_t,\
    "gs_data_image_t", pixel_image_enum_ptrs, pixel_image_reloc_ptrs,\
    ColorSpace)

/*
 * Define an ImageType 1 image.  ImageMask is an added member from PDF.
 * adjust and Alpha are not PostScript or PDF standard.
 */
typedef enum {
    /* No alpha.  This must be 0 for true-false tests. */
    gs_image_alpha_none = 0,
    /* Alpha precedes color components. */
    gs_image_alpha_first,
    /* Alpha follows color components. */
    gs_image_alpha_last
} gs_image_alpha_t;

typedef struct gs_image1_s {
    gs_pixel_image_common;
    /*
     * Define whether this is a mask or a solid image.
     * For masks, Alpha must be 'none'.
     */
    bool ImageMask;
    /*
     * Define whether to expand each destination pixel, to make
     * masked characters look better.  Only used for masks.
     */
    bool adjust;
    /*
     * Define whether there is an additional component providing
     * alpha information for each pixel, in addition to the
     * components implied by the color space.
     */
    gs_image_alpha_t Alpha;
    /*
     * Define the parent image type that gave rise to this.
     * Used to avoid the use of mixed halftoning methods
     * between images and their masks, which
     * can cause misalignment issues in pixel replications.
     */
    gs_image_parent_t image_parent_type;
} gs_image1_t;

/* The descriptor is public for soft masks. */
extern_st(st_gs_image1);
#define public_st_gs_image1()	/* in gximage1.c */\
  gs_public_st_suffix_add0(st_gs_image1, gs_image1_t, "gs_image1_t",\
    image1_enum_ptrs, image1_reloc_ptrs, st_gs_pixel_image)

/*
 * In standard PostScript Level 1 and 2, this is the only defined ImageType.
 */
typedef gs_image1_t gs_image_t;

/*
 * Define procedures for initializing the standard forms of image structures
 * to default values.  Note that because these structures may add more
 * members in the future, all clients constructing gs_*image*_t values
 * *must* start by initializing the value by calling one of the following
 * procedures.  Note also that these procedures do not set the image type.
 */
void
  /*
   * Sets ImageMatrix to the identity matrix.
   */
     gs_image_common_t_init(gs_image_common_t * pic),
  /*
   * Also sets Width = Height = 0, BitsPerComponent = 1,
   * format = chunky, Interpolate = false.
   * If num_components = N > 0, sets the first N elements of Decode to (0, 1);
   * if num_components = N < 0, sets the first -N elements of Decode to (1, 0);
   * if num_components = 0, doesn't set Decode.
   */
     gs_data_image_t_init(gs_data_image_t * pim, int num_components),
  /*
   * Also sets CombineWithColor = false, ColorSpace = color_space, Alpha =
   * none.  num_components is obtained from ColorSpace; if ColorSpace =
   * NULL or ColorSpace is a Pattern space, num_components is taken as 0
   * (Decode is not initialized).
   */
    gs_pixel_image_t_init(gs_pixel_image_t * pim,
                          gs_color_space * color_space);

/*
 * Initialize an ImageType 1 image (or imagemask).  Also sets ImageMask,
 * adjust, and Alpha, and the image type.  For masks, write_1s = false
 * paints 0s, write_1s = true paints 1s.  This is consistent with the
 * "polarity" operand of the PostScript imagemask operator.
 *
 * init and init_mask initialize adjust to true.  This is a bad decision
 * which unfortunately we can't undo without breaking backward
 * compatibility.  That is why we added init_adjust and init_mask_adjust.
 * Note that for init and init_adjust, adjust is only relevant if
 * pim->ImageMask is true.
 */
void gs_image_t_init_adjust(gs_image_t * pim, gs_color_space * pcs,
                            bool adjust);
#define gs_image_t_init(pim, pcs)\
  gs_image_t_init_adjust(pim, pcs, true)
void gs_image_t_init_mask_adjust(gs_image_t * pim, bool write_1s,
                                 bool adjust);
#define gs_image_t_init_mask(pim, write_1s)\
  gs_image_t_init_mask_adjust(pim, write_1s, true)

/* When doing thresholding in landscape mode, we collect scancolumns into a
 * buffer LAND_BITS wide, and then flush them to copy_mono. Because we use
 * SSE operations, LAND_BITS must be a multiple of 16. For performance,
 * copy_mono would prefer longer runs than shorter ones, so we leave this
 * configurable. The hope is that we can effectively trade memory for speed.
 *
 * Timings with LAND_BITS set to 32 and 128 both show slower performance than
 * 16 though, due to increased time in image_render_mono_ht in the loop
 * that copies data from scanlines to scancolumns. It seems that writing to
 * the buffer in positions [0] [16] [32] [48] etc is faster than writing in
 * positions [0] [32] [64] [96] etc. We would therefore like to leave
 * LAND_BITS set to 16.
 *
 * Unfortunately, we call through the device interface to copy_mono with the
 * results of these buffers, and various copy_mono implementations assume
 * that the raster given is a multiple of align_bitmap_mod bits. In order to
 * ensure safely, we therefore set LAND_BITS to max(16, align_bitmap_mod). We
 * are guaranteed that align_bitmap_mod is a multiple of 16.
 */
#define LAND_BITS_MIN 16
#if LAND_BITS_MIN < (align_bitmap_mod*8)
#define LAND_BITS (align_bitmap_mod*8)
#else
#define LAND_BITS LAND_BITS_MIN
#endif

/* Used for bookkeeping ht buffer information in landscape mode */
typedef struct ht_landscape_info_s {
    int count;
    int widths[LAND_BITS];
    int xstart;
    int curr_pos;
    int index;
    int num_contones;
    bool offset_set;
    bool flipy;
    int y_pos;
} ht_landscape_info_t;

/****** REMAINDER OF FILE UNDER CONSTRUCTION. PROCEED AT YOUR OWN RISK. ******/

#if 0

/* ---------------- Services ---------------- */

/*
   In order to make the driver's life easier, we provide the following callback
   procedure:
 */

int gx_map_image_color(gx_device * dev,
                       const gs_image_t * pim,
                       const gx_color_rendering_info * pcri,
                       const uint components[GS_IMAGE_MAX_COMPONENTS],
                       gx_drawing_color * pdcolor);

/*
  Map a source color to a drawing color.  The components are simply the
  pixel component values from the input data, i.e., 1 to
  GS_IMAGE_MAX_COMPONENTS B-bit numbers from the source data.  Return 0 if
  the operation succeeded, or a negative error code.
 */

#endif /*************************************************************** */

#endif /* gsiparam_INCLUDED */
