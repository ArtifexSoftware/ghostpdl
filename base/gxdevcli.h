/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* Definitions for device clients */

#ifndef gxdevcli_INCLUDED
#  define gxdevcli_INCLUDED

#include "gsdevice.h"
#include "stdint_.h"
#include "gscompt.h"
#include "gsdcolor.h"
#include "gsmatrix.h"
#include "gsiparam.h"		/* requires gsmatrix.h */
#include "gsrefct.h"
#include "gsropt.h"
#include "gsstruct.h"
#include "gstparam.h"
#include "gsxfont.h"
#include "gxbitmap.h"
#include "gxcindex.h"
#include "gxcvalue.h"
#include "gxfixed.h"
#include "gxtext.h"
#include "gxcmap.h"
#include "gsnamecl.h"
#include "gp.h"
#include "gscms.h"
#include "gxrplane.h"
#include "gxdda.h"
#include "gxpath.h"
#include "gsimage.h"

/* See Drivers.htm for documentation of the driver interface. */

/* ---------------- Memory management ---------------- */

/*
 * NOTE: if you write code that creates device instances (either with
 * gs_copydevice or by allocating them explicitly), allocates device
 * instances as either local or static variables (actual instances, not
 * pointers to instances), or sets the target of forwarding devices, please
 * read the following documentation carefully.  The rules for doing these
 * things changed substantially in release 5.68, in a
 * non-backward-compatible way, and unfortunately we could not find a way to
 * make the compiler give an error at places that need changing.
 */

/*
 * Device instances are managed with reference counting: when the last
 * reference to a device from a graphics state or the target field of a
 * forwarding device is removed, the device is normally freed.  However,
 * some device instances are referenced in other ways (for example, from
 * objects in the PostScript interpreter, or from library client code) and
 * will be freed by the garbage collector (if any) or explicitly: they
 * should not be freed by reference counting.  These are called "retained"
 * device instances.  Every device instance remembers whether or not it is
 * retained, and an instance is freed iff its reference count is zero and it
 * is not retained.
 *
 * Normally devices are initialized as not retained.  However, devices
 * initialized by calling gx_device_init(pdev, proto, memory, false), or
 * created by gs_copydevice are marked as retained.  You can also set the
 * retention status of a device explicitly with gx_device_retain(pdev,
 * true-or-false).  Note that if you change a retained device to
 * non-retained, if there are no references to it from graphics states or
 * targets, it will be freed immediately.
 *
 * The preferred technique for creating a new device is now gs_copydevice.
 * There are a number of places in the code where memory is explicitly
 * allocated, then initialized with gx_device_init. These should gradually
 * be replaced.
 *
 * There are 3 ways that a device structure might be allocated:
 *	1) Allocated dynamically, e.g.,
 *		gx_device *pdev_new;
 *		gs_copydevice(&pdev_new, pdev_old, memory);
 *	2) Declared as a local or static variable, e.g.,
 *		gx_device devv;
 *	or
 *		const gx_device devc = ...;
 *	3) Embedded in an object allocated in one of the above ways.
 * If you allocate a device using #2 or #3, you must either mark it as
 * retained by calling gx_device_retain(pdev, true) or initialize it with a
 * NULL memory.  If you do not do this, an attempt will be made to free the
 * device, corrupting memory.  Note that when memory is NULL, the finalize
 * method of the device will not be called when it is freed, so you cannot
 * use it for cleanup.  */

/*
 * Do not set the target of a forwarding device with an assignment like
 *	fdev->target = tdev;
 * You must use the procedure
 *	gx_device_set_target(fdev, tdev);
 * Note that the first argument is a gx_device_forward *, not a gx_device *.
 *
 * We could have changed the member name "target" when this became
 * necessary, so the compiler would flag places that needed editing, but
 * there were literally hundreds of places that only read the target member
 * that we would have had to change, so we decided to leave the name alone.
 */

/* ---------------- Auxiliary types and structures ---------------- */

/* We need an abstract type for the pattern instance, */
/* for pattern management. */
typedef struct gs_pattern1_instance_s gs_pattern1_instance_t;

/* Define the type for colors passed to the higher-level procedures. */
typedef gx_device_color gx_drawing_color;

/* Define a type for telling get_alpha_bits what kind of object */
/* is being rendered. */
typedef enum {
    go_text,
    go_graphics
} graphics_object_type;

/* Define an edge of a trapezoid.  Requirement: end.y >= start.y. */
typedef struct gs_fixed_edge_s {
    gs_fixed_point start;
    gs_fixed_point end;
} gs_fixed_edge;

/* Define the parameters passed to get_bits_rectangle. */
typedef struct gs_get_bits_params_s gs_get_bits_params_t;

/* Define the structure for device color capabilities. */
typedef struct gx_device_anti_alias_info_s {
    int text_bits;		/* 1,2,4 */
    int graphics_bits;		/* ditto */
} gx_device_anti_alias_info;

typedef int32_t frac31; /* A fraction value in [-1,1].
    Represents a color (in [0,1])
    or a color difference (in [-1,1]) in shadings. */

/* Define an edge of a linear color trapezoid.  Requirement: end.y >= start.y. */
typedef struct gs_linear_color_edge_s {
    gs_fixed_point start;
    gs_fixed_point end;
    const frac31 *c0, *c1;
    fixed clip_x;
} gs_linear_color_edge;

/*
 * Possible values for the separable_and_linear flag in the
 * gx_device_color_info structure. These form an order, with lower
 * values having weaker properties.
 *
 *  GX_CINFO_UNKNOWN_SEP_LIN
 *    The properties of the color encoding are not yet known. This is
 *    always a safe default value.
 *
 *  GX_CINFO_SEP_LIN_NONE
 *    The encoding is not separable and linear. If this value is set,
 *    the device must provide an encode_color method, either directly
 *    or via map_rgb_color/map_cmyk_color methods. This setting is
 *    only legitimate for color models with 4 or fewer components.
 *
 *  GX_CINFO_SEP_LIN
 *    A separable and linear encoding has the separability and
 *    linearity properties.
 *
 *    Encodings with this property are completely characterized
 *    by the comp_shift array. Hence, there is no need to provide
 *    an encode_color procedure for such devices, though the device
 *    creator may choose to do so for performance reasons (e.g.: when
 *    each color component is assigned a byte).
 *
 *    Note that if the device encodes tags, the comp_shift array must
 *    provide a shift (and mask) for the tag component in the num_components
 *    element of the array. Used in gx_default_fill_linear_color_scanline.
 *
 *  GX_CINFO_SEP_LIN_NON_STANDARD
 *    A separable and linear encoding has the separability and
 *    linearity properties. In addition, we know that this encoding
 *    is NOT the 'standard' one. (i.e. it is not compatible with the
 *    encoding used by the pdf14 compositor).
 *
 *    Encodings with this property are completely characterized
 *    by the comp_shift array. Hence, there is no need to provide
 *    an encode_color procedure for such devices, though the device
 *    creator may choose to do so for performance reasons (e.g.: when
 *    each color component is assigned a byte).
 *
 *  GX_CINFO_SEP_LIN_STANDARD
 *    A separable and linear encoding has the separability and
 *    linearity properties. In addition, we know that this encoding
 *    is the 'standard' one. (i.e. it is compatible with the encoding
 *    used by the pd14 compositor).
 *
 *    Encodings with this property are completely characterized
 *    by the comp_shift array. Hence, there is no need to provide
 *    an encode_color procedure for such devices, though the device
 *    creator may choose to do so for performance reasons (e.g.: when
 *    each color component is assigned a byte).
 */

/* Note: The arithmetic ordering for these values is relied upon
 * in tests (see colors_are_separable_and_linear below) for
 * separability and linearity. Change them with care! */
typedef enum {
    GX_CINFO_UNKNOWN_SEP_LIN = -1,
    GX_CINFO_SEP_LIN_NONE = 0,
    GX_CINFO_SEP_LIN = 1,
    GX_CINFO_SEP_LIN_NON_STANDARD = 2,
    GX_CINFO_SEP_LIN_STANDARD = 3
} gx_color_enc_sep_lin_t;

/*
 * Enumerator to indicate if a color model will support overprint mode.
 *
 * Only "DeviceCMYK" color space support this option, but we interpret
 * this designation some broadly: a DeviceCMYK color model is any sub-
 * tractive color model that provides the components Cyan, Magenta,
 * Yellow, and Black, and maps the DeviceCMYK space directly to these
 * components. This includes DeviceCMYK color models with spot colors,
 * and DeviceN color models that support the requisite components (the
 * latter may vary from Adobe's implementations; this is not easily
 * tested).
 *
 * In principle this parameter could be a boolean set at initialization
 * time. Primarily for historical reasons, the determination of whether
 * or not a color model supports overprint is delayed until this
 * information is required, hence the use of an enumeration with an
 * "unknown" setting.
 *
 */
typedef enum {
    GX_CINFO_OPMSUPPORTED_UNKNOWN = -1,
    GX_CINFO_OPMSUPPORTED_NOT = 0,
    GX_CINFO_OPMSUPPORTED = 1
} gx_cm_opmsupported_t;

/* component index value used to indicate no color component.  */
#define GX_CINFO_COMP_NO_INDEX 0xff

/*
 * Additional possible value for cinfo.gray_index, to indicate which
 * component, if any, qualifies as the "gray" component.
 */
#define GX_CINFO_COMP_INDEX_UNKNOWN 0xfe

/*
 * The enlarged color model information structure: Some of the
 * information that was implicit in the component number in
 * the earlier conventions (component names, polarity, mapping
 * functions) are now explicitly provided.
 *
 * Also included is some information regarding the encoding of
 * color information into gx_color_index. Some of this information
 * was previously gathered indirectly from the mapping
 * functions in the existing code, specifically to speed up the
 * halftoned color rendering operator (see
 * gx_dc_ht_colored_fill_rectangle in gxcht.c). The information
 * is now provided explicitly because such optimizations are
 * more critical when the number of color components is large.
 *
 * Note: no pointers have been added to this structure, so there
 *       is no requirement for a structure descriptor.
 */
typedef struct gx_device_color_info_s {

    /*
     * max_components is the maximum number of components for all
     * color models supported by this device. This does not include
     * any alpha components.
     */
    uchar max_components;

    /*
     * The number of color components. This does not include any
     * alpha-channel information, which may be integrated into
     * the gx_color_index but is otherwise passed as a separate
     * component.
     */
    uchar num_components;

    /*
     * Polarity of the components of the color space, either
     * additive or subtractive. This is used to interpret transfer
     * functions and halftone threshold arrays. Possible values
     * are GX_CM_POLARITY_ADDITIVE or GX_CM_POLARITY_SUBTRACTIVE
     */
    gx_color_polarity_t polarity;

    /*
     * The number of bits of gx_color_index actually used.
     * This must be <= ARCH_SIZEOF_COLOR_INDEX, which is usually 64.
     * Note that we now have planar devices which can support much more.
     * Changing this to a ushort to reflect this.
     */
    ushort depth;

    /*
     * Index of the gray color component, if any. The max_gray and
     * dither_gray values apply to this component only; all other
     * components use the max_color and dither_color values.
     *
     * Note:  This field refers to a 'gray' colorant because of the
     * past use of the max_gray/color and dither_grays/colors fields.
     * Prior to 8.00, the 'gray' values were used for monochrome
     * devices and the 'color' values for RGB and CMYK devices.
     * Ideally we would like to have the flexibiiity of allowing
     * different numbers of intensity levels for each colorant.
     * However this is not compatible with the pre 8.00 devices.
     * With post 8.00 devices, we can have two different numbers of
     * intensity levels.  For one colorant (which is specified by
     * the gray_index) we will use the max_gray/dither_grays values.
     * The remaining colorants will use the max_color/dither_colors
     * values.  The colorant which is specified by the gray_index
     * value does not have to be gray or black.  For example if we
     * have an RGB device and we want 32 intensity levels for red and
     * blue and 64 levels for green, then we can set gray_index to
     * 1 (the green colorant), set max_gray to 63 and dither_grays to
     * 64, and set max_color to 31 and dither_colors to 32.
     *
     * This will be GX_CINFO_COMP_NO_INDEX if there is no 'gray'
     * component.
     */
    byte gray_index;

    /*
     * max_gray and max_color are the number of distinct native
     * intensity levels, less 1, for the 'gray' and all other color
     * components, respectively. For nearly all current devices
     * that support both 'gray' and non-'gray' components, the two
     * parameters have the same value.  (See comment for gray_index.)
     *
     * dither_grays and dither_colors are the number of intensity
     * levels between which halftoning can occur, for the 'gra'y and
     * all other color components, respectively. This is
     * essentially redundant information: in all reasonable cases,
     * dither_grays = max_gray + 1 and dither_colors = max_color + 1.
     * These parameters are, however, extensively used in the
     * current code, and thus have been retained.
     *
     * Note that the non-'gray' values may now be relevant even if
     * num_components == 1. This simplifies the handling of devices
     * with configurable color models which may be set for a single
     * non-'gray' color model.
     */
    uint max_gray;		/* # of distinct color levels -1 */
    uint max_color;

    uint dither_grays;
    uint dither_colors;

    /*
     * Information to control super-sampling of objects to support
     * anti-aliasing.
     */
    gx_device_anti_alias_info anti_alias;

    /*
     * Flag to indicate if gx_color_index for this device may be divided
     * into individual fields for each component. This is almost always
     * the case for printers, and is the case for most modern displays
     * as well. When this is the case, halftoning may be performed
     * separately for each component, which greatly simplifies processing
     * when the number of color components is large.
     *
     * If the gx_color_index is separable in this manner, the comp_shift
     * array provides the location of the low-order bit for each
     * component. This may be filled in by the client, but need not be.
     * If it is not provided, it will be calculated based on the values
     * in the max_gray and max_color fields as follows:
     *
     *     comp_shift[num_components - 1] = 0,
     *     comp_shift[i] = comp_shift[i + 1]
     *                      + ( i == gray_index ? ceil(log2(max_gray + 1))
     *                                          : ceil(log2(max_color + 1)) )
     *
     * The comp_mask and comp_bits fields should be left empty by the client.
     * They will be filled in during initialization using the following
     * mechanism:
     *
     *     comp_bits[i] = ( i == gray_index ? ceil(log2(max_gray + 1))
     *                                      : ceil(log2(max_color + 1)) )
     *
     *     comp_mask[i] = (((gx_color_index)1 << comp_bits[i]) - 1)
     *                       << comp_shift[i]
     *
     * (For current devices, it is almost always the case that
     * max_gray == max_color, if the color model contains both gray and
     * non-gray components.)
     *
     * If separable_and_linear is not set, the data in the other fields
     * is unpredictable and should be ignored.
     */
    gx_color_enc_sep_lin_t separable_and_linear;
    byte                   comp_shift[GX_DEVICE_COLOR_MAX_COMPONENTS];
    byte                   comp_bits[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index         comp_mask[GX_DEVICE_COLOR_MAX_COMPONENTS];

    /*
     * Pointer to name for the process color model.
     */
    const char * cm_name;

    /*
     * Indicate if overprint mode is supported. This is only supported
     * for color models that have "DeviceCMYK" like behaivor: they support
     * the cyan, magenta, yellow, and black color components, and map the
     * components of a DeviceCMYK color space directly to these compoents.
     * Most such color spaces will have the name DeviceCMYK, but it is
     * also possible for DeviceN color models this behavior.
     *
     * If opmsupported has the value GX_CINFO_OPMSUPPORTED, the process_comps will
     * be a bit mask, with the (1 << i) bit set if i'th component is the
     * cyan, magenta, yellow, or black component and black_component will
     * be set to the index of a black component.
     */
    gx_cm_opmsupported_t opmsupported;
    gx_color_index process_comps;
    uint black_component;
    bool use_antidropout_downscaler;
} gx_device_color_info;

/* Test to see if colors are separable and linear */
static inline int colors_are_separable_and_linear(gx_device_color_info *info)
{
    return (info->separable_and_linear >= GX_CINFO_SEP_LIN);
}


/* NB encoding flag ignored */
#define dci_extended_alpha_values(mcmp, nc, p, d, gi, mg, \
                        mc, dg, dc, ta, ga, sl, cn)   \
    {mcmp /* max components */, \
     nc /* number components */, \
     p /* polarity */, \
     d /* depth */, \
     gi /* gray index */, \
     mg /* max gray */, \
     mc /* max color */, \
     dg /* dither grays */, \
     dc /* dither colors */, \
     { ta, ga } /* antialias info text, graphics */, \
     sl /* separable_and_linear */, \
     { 0 } /* component shift */, \
     { 0 } /* component bits */, \
     { 0 } /* component mask */, \
     cn /* process color name */, \
     GX_CINFO_OPMSUPPORTED_UNKNOWN /* opmsupported */, \
     0  /* process_cmps */ }

/*
 * The "has color" macro requires a slightly different definition
 * with the more general color models.
 */
#define gx_device_has_color(dev)                           \
   ( (dev)->color_info.num_components > 1 ||                \
     (dev)->color_info.gray_index == GX_CINFO_COMP_NO_INDEX )

/* parameter initialization macros for backwards compatibility */

/*
 * These macros are needed to define values for fields added when
 * DeviceN compatibility was added.  Previously the graphics
 * library and the much of the device code examined the number of
 * components and assume that 1 --> DeviceGray, 3-->DeviceRGB,
 * and 4--> DeviceCMYK.  Since the old device code does not
 * specify a color model, these macros make the same assumption.
 * This assumption is incorrect for a DeviceN device and thus
 * the following macros should not be used.  The previously
 * defined macros should be used for new devices.
 */

#define dci_std_cm_name(nc)                 \
    ( (nc) == 1 ? "DeviceGray"              \
                : ((nc) == 3 ? "DeviceRGB"  \
                             : "DeviceCMYK") )

#define dci_std_polarity(nc)                    \
    ( (nc) >= 4 ? GX_CINFO_POLARITY_SUBTRACTIVE \
                : GX_CINFO_POLARITY_ADDITIVE )

      /*
       * Get the default gray_index value, based on the number of color
       * components. Note that this must be consistent with the index
       * implicitly used by the get_color_comp_index method and the
       * procedures in the structure returned by the
       * get_color_mapping_procs method.
       */
#define dci_std_gray_index(nc)    \
    ((nc) == 3 ? GX_CINFO_COMP_NO_INDEX : (nc) - 1)

#define dci_alpha_values(nc, depth, mg, mc, dg, dc, ta, ga) \
    dci_extended_alpha_values(nc, nc,			    \
                              dci_std_polarity(nc),         \
                              depth,                        \
                              dci_std_gray_index(nc),       \
                              mg, mc, dg, dc, ta, ga,       \
                              GX_CINFO_UNKNOWN_SEP_LIN,     \
                              dci_std_cm_name(nc) )
#define dci_alpha_values_add(nc, depth, mg, mc, dg, dc, ta, ga) \
    dci_extended_alpha_values(nc, nc,			    \
                              GX_CINFO_POLARITY_ADDITIVE,   \
                              depth,                        \
                              dci_std_gray_index(nc),       \
                              mg, mc, dg, dc, ta, ga,       \
                              GX_CINFO_UNKNOWN_SEP_LIN,     \
                              dci_std_cm_name(nc) )

/*
 * Determine the depth corresponding to a color_bits specification.
 * Note that color_bits == 0 ==> depth == 0; surprisingly this
 * case is used.
*/
#define dci_std_color_depth(color_bits)   \
    ((color_bits) == 1 ? 1 : ((color_bits) + 7) & ~7)

/*
 * Determine the number of components corresponding to a color_bits
 * specification. A device is monochrome only if it is bi-level;
 * the 4 and 8 bit cases are handled as mapped color displays (for
 * compatibility with existing code). The peculiar color_bits = 0
 * case is considered monochrome, for no apparent reason.
 */
#define dci_std_color_num_components(color_bits)      \
    ( (color_bits) <= 1 ? 1                           \
                      : ((color_bits) % 3 == 0 ||     \
                         (color_bits) == 4     ||     \
                         (color_bits) == 8       ) ? 3 : 4 )

/*
 * The number of bits assigned to the gray/black color component,
 * assuming there is such a component. The underlying assumption
 * is that any extra bits are assigned to this component.
 */
#define dci_std_gray_bits(nc, color_bits)    \
    ((color_bits) - ((nc) - 1) * ((color_bits) / (nc)))

/*
 * The number of bits assigned to a color component. The underlying
 * assumptions are that there is a gray component if nc != 3, and
 * that the gray component uses any extra bits.
 */
#define dci_std_color_bits(nc, color_bits)                        \
    ( (nc) == 3                                                   \
        ? (color_bits) / (nc)                                     \
        : ( (nc) == 1                                             \
              ? 0                                                 \
              : ((color_bits) - dci_std_gray_bits(nc, color_bits))\
                     / ((nc) == 1 ? (1) : (nc) - 1) ) )

/*
 * Determine the max_gray and max_color values based on the number
 * of components and the color_bits value. See the comments above
 * for information on the underlying assumptions.
 */
#define dci_std_color_max_gray(nc, color_bits)            \
    ( (nc) == 3                                           \
        ? 0                                               \
        : (1 << dci_std_gray_bits(nc, color_bits)) - 1 )

#define dci_std_color_max_color(nc, color_bits)               \
    ( (nc) == 1                                               \
        ? 0                                                   \
        : (1 << dci_std_color_bits(nc, color_bits)) - 1 )

/*
 * Define a color model based strictly on the number of bits
 * available for color representation. Please note, this is only
 * intended to work for a limited set of devices.
 */
#define dci_std_color_(nc, color_bits)                        \
    dci_values( nc,                                           \
                dci_std_color_depth(color_bits),              \
                dci_std_color_max_gray(nc, color_bits),       \
                dci_std_color_max_color(nc, color_bits),      \
                dci_std_color_max_gray(nc, color_bits) + 1,   \
                dci_std_color_max_color(nc, color_bits) + 1 )

#define dci_std_color(color_bits)                             \
    dci_std_color_( dci_std_color_num_components(color_bits), \
                    color_bits )

#define dci_values(nc,depth,mg,mc,dg,dc)\
  dci_alpha_values(nc, depth, mg, mc, dg, dc, 1, 1)
#define dci_values_add(nc,depth,mg,mc,dg,dc)\
  dci_alpha_values_add(nc, depth, mg, mc, dg, dc, 1, 1)
#define dci_black_and_white dci_std_color(1)
#define dci_black_and_white_() dci_black_and_white
#define dci_color(depth,maxv,dither)\
  dci_values(3, depth, maxv, maxv, dither, dither)

/*
 * Macro to access the name of the process color model.
 */
#define get_process_color_model_name(dev) \
    ((dev)->color_info.cm_name)

/* Structure for device procedures. */
typedef struct gx_device_procs_s gx_device_procs;

/* Structure for page device procedures. */
/* Note that these take the graphics state as a parameter. */
typedef struct gx_page_device_procs_s {

#define dev_page_proc_install(proc)\
  int proc(gx_device *dev, gs_gstate *pgs)
    dev_page_proc_install((*install));

#define dev_page_proc_begin_page(proc)\
  int proc(gx_device *dev, gs_gstate *pgs)
    dev_page_proc_begin_page((*begin_page));

#define dev_page_proc_end_page(proc)\
  int proc(gx_device *dev, int reason, gs_gstate *pgs)
    dev_page_proc_end_page((*end_page));

} gx_page_device_procs;

/* Default procedures */
dev_page_proc_install(gx_default_install);
dev_page_proc_begin_page(gx_default_begin_page);
dev_page_proc_end_page(gx_default_end_page);

/* ----------- A stroked gradient recognizer data ----------*/

/* This structure is associated with a device for
   internal needs of the graphics library.
   The main purpose is to suppress stroke adjustment
   when painting a gradient as a set of parallel strokes.
   Such gradients still come from some obsolete 3d party software.
   See bug 687974,
 */

typedef struct gx_stroked_gradient_recognizer_s {
    bool stroke_stored;
    gs_fixed_point orig[4], adjusted[4]; /* from, to, width, vector. */
} gx_stroked_gradient_recognizer_t;

/* ---------------- Device structure ---------------- */

/*
 * Define the generic device structure.
 *
 * All devices start off with no device procs. The initialize_device_procs
 * function is called as soon as the device is copied from its prototype,
 * and that fills in the procs table with the device procedure pointers.
 * This can never fail.
 *
 * Next, the 'initialize_device' proc (if there is one) is called to do
 * the minimal initialization required for a device.
 *
 * The choice of the name Margins (rather than, say, HWOffset), and the
 * specification in terms of a default device resolution rather than
 * 1/72" units, are due to Adobe.
 *
 * ****** NOTE: If you define any subclasses of gx_device, you *must* define
 * ****** the finalization procedure as gx_device_finalize.  Finalization
 * ****** procedures are not automatically inherited.
 */
typedef struct gx_device_cached_colors_s {
    gx_color_index black, white;
} gx_device_cached_colors_t;

/*
 * Define the parameters controlling banding.
 */
/* if you make any additions/changes to this structure you need to make
   the appropriate additions/changes to gdev_space_params_cmp() */
typedef struct gx_band_params_s {
    int BandWidth;		/* (optional) band width in pixels */
    int BandHeight;		/* (optional) */
    size_t BandBufferSpace;	/* (optional) */
    size_t tile_cache_size;	/* (optional) */
} gx_band_params_t;

#define BAND_PARAMS_INITIAL_VALUES 0, 0, 0, 0

typedef enum {
    BandingAuto = 0,
    BandingAlways,
    BandingNever
} gdev_banding_type;

/* if you make any additions/changes to this structure you need to make
   the appropriate additions/changes to the gdev_space_params_cmp() */
typedef struct gdev_space_params_s {
    size_t MaxBitmap;		/* max size of non-buffered bitmap */
    size_t BufferSpace;		/* space to use for buffer */
    gx_band_params_t band;	/* see gxband.h */
    bool params_are_read_only;	/* true if put_params may not modify this struct */
    gdev_banding_type banding_type;	/* used to force banding or bitmap */
} gdev_space_params;

/* Returns 0 for a match, non-zero otherwise. Like memcmp, but allowing
 * for uninitialised padding. */
int gdev_space_params_cmp(const gdev_space_params sp1,
                          const gdev_space_params sp2);

typedef struct gdev_nupcontrol_s {
        rc_header rc;
        char *nupcontrol_str;		/* NUL termintated string */
} gdev_nupcontrol;

typedef struct gdev_pagelist_s {
        rc_header rc;
        char *Pages;
} gdev_pagelist;

#define dev_t_proc_initialize_device_procs(proc, dev_t)\
  void proc(dev_t *dev)
#define dev_proc_initialize_device_procs(proc)\
  dev_t_proc_initialize_device_procs((proc), gx_device)

#define gx_device_common\
        int params_size;		/* OBSOLETE if stype != 0: */\
                                        /* size of this structure */\
        dev_proc_initialize_device_procs(*initialize_device_procs);\
                                        /* initialize_device_procs */\
        const char *dname;		/* the device name */\
        gs_memory_t *memory;		/* (0 iff static prototype) */\
        gs_memory_type_ptr_t stype;	/* memory manager structure type, */\
                                        /* may be 0 if static prototype */\
        bool stype_is_dynamic;		/* if true, free the stype when */\
                                        /* freeing the device */\
        void (*finalize)(gx_device *);  /* finalization to execute */\
                                        /* before closing device, if any */\
        rc_header rc;			/* reference count from gstates */\
                                        /* and targets, +1 if retained */\
        bool retained;			/* true if retained */\
        gx_device *parent;\
        gx_device *child;\
        void *subclass_data;    /* Must be immovable, non-GC memory, used to store subclass data */\
        gdev_pagelist *PageList;\
        bool is_open;			/* true if device has been opened */\
        int max_fill_band;		/* limit on band size for fill, */\
                                        /* must be 0 or a power of 2 */\
                                        /* (see gdevabuf.c for more info) */\
        gx_device_color_info color_info;	/* color information */\
        gx_device_cached_colors_t cached_colors;\
        int width;			/* width in pixels */\
        int height;			/* height in pixels */\
        int pad;                        /* pad to use for buffers; 0 for default */\
        int log2_align_mod;             /* align to use for buffers; 0 for default */\
        int num_planar_planes;          /* 1 planar, 0 for chunky */\
        int LeadingEdge;                /* see below */\
        float MediaSize[2];		/* media dimensions in points */\
        float ImagingBBox[4];		/* imageable region in points */\
        bool ImagingBBox_set;\
        float HWResolution[2];		/* resolution, dots per inch */\
        float Margins[2];		/* offset of physical page corner */\
                                        /* from device coordinate (0,0), */\
                                        /* in units given by HWResolution */\
        float HWMargins[4];		/* margins around imageable area, */\
                                        /* in default user units ("points") */\
        int FirstPage;\
        int LastPage;\
        bool PageHandlerPushed;    /* Handles FirstPage and LastPage operations */\
        bool DisablePageHandler;   /* Can be set by the interpreter if it will process FirstPage and LastPage itself */\
        int ObjectFilter;          /* Bit field for which object filters to apply */\
        bool ObjectHandlerPushed;  /* Handles filtering of objects to devices */\
        gdev_nupcontrol *NupControl;\
        bool NupHandlerPushed;     /* Handles Nup operations */\
        long PageCount;			/* number of pages written */\
        long ShowpageCount;		/* number of calls on showpage */\
        int NumCopies;\
        bool NumCopies_set;\
        bool IgnoreNumCopies;		/* if true, force num_copies = 1 */\
        bool UseCIEColor;		/* for PS LL3 */\
        bool LockSafetyParams;		/* If true, prevent unsafe changes */\
        long band_offset_x;		/* offsets of clist band base to (mem device) buffer */\
        long band_offset_y;		/* for rendering that is phase sensitive (old wtsimdi) */\
        bool BLS_force_memory;\
        gx_stroked_gradient_recognizer_t sgr;\
        size_t MaxPatternBitmap;	/* Threshold for switching to pattern_clist mode */\
        bool page_uses_transparency;    /* PDF 1.4 transparency is used. */\
        bool page_uses_overprint;       /* overprint is used. */\
        gdev_space_params space_params;\
        cmm_dev_profile_t *icc_struct;  /* object dependent profiles */\
        gs_graphics_type_tag_t   graphics_type_tag;   /* e.g. vector, image or text */\
        int interpolate_control;      /* default 1 (use image /Interpolate value), 0 is NOINTERPOLATE. */\
                                      /* > 1 limits interpolation, < 0 forces interpolation */\
        int non_strict_bounds;        /* If set, callers cannot rely on clipping fills etc to declared device bounds. */\
        gx_page_device_procs page_procs;       /* must be last */\
                /* end of std_device_body */\
        gx_device_procs procs	/* object procedures */

#define LEADINGEDGE_MASK 3
#define LEADINGEDGE_SET_MASK (1 << 2)
#define LEADINGEDGE_REQ_BIT (1 << 3)
#define LEADINGEDGE_REQ_VAL_SHIFT 4
#define LEADINGEDGE_REQ_VAL (LEADINGEDGE_MASK << LEADINGEDGE_REQ_VAL_SHIFT)
/*
 * The lower two bits of LeadingEdge correspond to the pagedevice
 * parameter of the same name. The next bit (hex value 4) is set if
 * the LeadingEdge was set explicitly by the user using setpagedevice.
 * Otherwise, the value is the default as determined by the device (or
 * zero if the device has no logic for setting LeadingEdge based on
 * other parameters (such as %MediaSource). This field replaces the
 * earlier TrayOrientation, which had a similar purpose but was not
 * compatible with the PostScript spec.
 */

/*
 * Note: x/y_pixels_per_inch are here only for backward compatibility.
 * They should not be used in new code.
 */
#define x_pixels_per_inch HWResolution[0]
#define y_pixels_per_inch HWResolution[1]
#define offset_margin_values(x, y, left, bot, right, top)\
  {x, y}, {left, bot, right, top}
#define margin_values(left, bot, right, top)\
  offset_margin_values(0, 0, left, bot, right, top)
#define no_margins margin_values(0, 0, 0, 0)
#define no_margins_() no_margins
/* Define macros that give the page offset ("Margins") in inches. */
#define dev_x_offset(dev) ((dev)->Margins[0] / (dev)->HWResolution[0])
#define dev_y_offset(dev) ((dev)->Margins[1] / (dev)->HWResolution[1])
#define dev_y_offset_points(dev) (dev_y_offset(dev) * 72.0)
/* Note that left/right/top/bottom are defined relative to */
/* the physical paper, not the coordinate system. */
/* For backward compatibility, we define macros that give */
/* the margins in inches. */
#define dev_l_margin(dev) ((dev)->HWMargins[0] / 72.0)
#define dev_b_margin(dev) ((dev)->HWMargins[1] / 72.0)
#define dev_b_margin_points(dev) ((dev)->HWMargins[1])
#define dev_r_margin(dev) ((dev)->HWMargins[2] / 72.0)
#define dev_t_margin(dev) ((dev)->HWMargins[3] / 72.0)
#define dev_t_margin_points(dev) ((dev)->HWMargins[3])
/* The extra () are to prevent premature expansion. */
#define open_init_closed() 0 /*false*/, 0	/* max_fill_band */
#define open_init_open() 1 /*true*/, 0	/* max_fill_band */
/* Accessors for device procedures */
#define dev_proc(dev, p) ((dev)->procs.p)
#define set_dev_proc(dev, p, proc) ((dev)->procs.p = (proc))
#define fill_dev_proc(dev, p, dproc)\
  if ( dev_proc(dev, p) == 0 ) set_dev_proc(dev, p, dproc)
#define assign_dev_procs(todev, fromdev)\
  ((todev)->procs = (fromdev)->procs)

/* The bit fields used to filter objects out. If any bit field is set
 * then objects of that type will not be rendered/output.
 */
typedef enum FILTER_FLAGS {
    FILTERIMAGE = 1,
    FILTERTEXT = 2,
    FILTERVECTOR = 4
} OBJECT_FILTER_FLAGS;

/* ---------------- Device procedures ---------------- */

/*
 * Definition of device procedures.
 * Note that the gx_device * argument is not declared const,
 * because many drivers maintain dynamic state in the device structure.
 * Note also that the structure is defined as a template, so that
 * we can instantiate it with device subclasses.
 * Because C doesn't have real templates, we must do this with macros.
 */

/* Define macros for declaring device procedures. */

#define dev_t_proc_initialize_device(proc, dev_t)\
  int proc(dev_t *dev)
#define dev_proc_initialize_device(proc)\
  dev_t_proc_initialize_device(proc, gx_device)

#define dev_t_proc_open_device(proc, dev_t)\
  int proc(dev_t *dev)
#define dev_proc_open_device(proc)\
  dev_t_proc_open_device(proc, gx_device)

#define dev_t_proc_get_initial_matrix(proc, dev_t)\
  void proc(dev_t *dev, gs_matrix *pmat)
#define dev_proc_get_initial_matrix(proc)\
  dev_t_proc_get_initial_matrix(proc, gx_device)

#define dev_t_proc_sync_output(proc, dev_t)\
  int proc(dev_t *dev)
#define dev_proc_sync_output(proc)\
  dev_t_proc_sync_output(proc, gx_device)

#define dev_t_proc_output_page(proc, dev_t)\
  int proc(dev_t *dev, int num_copies, int flush)
#define dev_proc_output_page(proc)\
  dev_t_proc_output_page(proc, gx_device)

#define dev_t_proc_close_device(proc, dev_t)\
  int proc(dev_t *dev)
#define dev_proc_close_device(proc)\
  dev_t_proc_close_device(proc, gx_device)

#define dev_t_proc_map_rgb_color(proc, dev_t)\
  gx_color_index proc(dev_t *dev, const gx_color_value cv[])
#define dev_proc_map_rgb_color(proc)\
  dev_t_proc_map_rgb_color(proc, gx_device)

#define dev_t_proc_map_color_rgb(proc, dev_t)\
  int proc(dev_t *dev,\
    gx_color_index color, gx_color_value rgb[3])
#define dev_proc_map_color_rgb(proc)\
  dev_t_proc_map_color_rgb(proc, gx_device)

#define dev_t_proc_fill_rectangle(proc, dev_t)\
  int proc(dev_t *dev,\
    int x, int y, int width, int height, gx_color_index color)
#define dev_proc_fill_rectangle(proc)\
  dev_t_proc_fill_rectangle(proc, gx_device)

#define dev_t_proc_copy_mono(proc, dev_t)\
  int proc(dev_t *dev,\
    const byte *data, int data_x, int raster, gx_bitmap_id id,\
    int x, int y, int width, int height,\
    gx_color_index color0, gx_color_index color1)
#define dev_proc_copy_mono(proc)\
  dev_t_proc_copy_mono(proc, gx_device)

#define dev_t_proc_copy_color(proc, dev_t)\
  int proc(dev_t *dev,\
    const byte *data, int data_x, int raster, gx_bitmap_id id,\
    int x, int y, int width, int height)
#define dev_proc_copy_color(proc)\
  dev_t_proc_copy_color(proc, gx_device)

                /* Added in release 2.4, changed in 2.8, */
                /* renamed in 2.9.6 */

#define dev_t_proc_get_params(proc, dev_t)\
  int proc(dev_t *dev, gs_param_list *plist)
#define dev_proc_get_params(proc)\
  dev_t_proc_get_params(proc, gx_device)

#define dev_t_proc_put_params(proc, dev_t)\
  int proc(dev_t *dev, gs_param_list *plist)
#define dev_proc_put_params(proc)\
  dev_t_proc_put_params(proc, gx_device)

                /* Added in release 2.6 */

#define dev_t_proc_map_cmyk_color(proc, dev_t)\
  gx_color_index proc(dev_t *dev, const gx_color_value cv[])
#define dev_proc_map_cmyk_color(proc)\
  dev_t_proc_map_cmyk_color(proc, gx_device)

                /* Added in release 2.8.1 */

#define dev_t_proc_get_page_device(proc, dev_t)\
  gx_device *proc(dev_t *dev)
#define dev_proc_get_page_device(proc)\
  dev_t_proc_get_page_device(proc, gx_device)

                /* Added in release 3.20, OBSOLETED in 5.65 */
                /* 'Unobsoleted' in 9.55. */

#define dev_t_proc_get_alpha_bits(proc, dev_t)\
  int proc(dev_t *dev, graphics_object_type type)
#define dev_proc_get_alpha_bits(proc)\
  dev_t_proc_get_alpha_bits(proc, gx_device)

                /* Added in release 3.20 */

#define dev_t_proc_copy_alpha(proc, dev_t)\
  int proc(dev_t *dev, const byte *data, int data_x,\
    int raster, gx_bitmap_id id, int x, int y, int width, int height,\
    gx_color_index color, int depth)
#define dev_proc_copy_alpha(proc)\
  dev_t_proc_copy_alpha(proc, gx_device)

                /* Added in release 3.60, changed in 3.68. */

#define dev_t_proc_fill_path(proc, dev_t)\
  int proc(dev_t *dev,\
    const gs_gstate *pgs, gx_path *ppath,\
    const gx_fill_params *params,\
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
#define dev_proc_fill_path(proc)\
  dev_t_proc_fill_path(proc, gx_device)

#define dev_t_proc_stroke_path(proc, dev_t)\
  int proc(dev_t *dev,\
    const gs_gstate *pgs, gx_path *ppath,\
    const gx_stroke_params *params,\
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
#define dev_proc_stroke_path(proc)\
  dev_t_proc_stroke_path(proc, gx_device)

                /* Added in release 9.22 */

#define dev_t_proc_fill_stroke_path(proc, dev_t)\
  int proc(dev_t *dev,\
    const gs_gstate *pgs, gx_path *ppath,\
    const gx_fill_params *fill_params,\
    const gx_drawing_color *pdcolor_fill,\
    const gx_stroke_params *stroke_params,\
    const gx_drawing_color *pdcolor_stroke,\
    const gx_clip_path *pcpath)
#define dev_proc_fill_stroke_path(proc)\
  dev_t_proc_fill_stroke_path(proc, gx_device)

                /* Added in release 9.57 */

#define dev_t_proc_lock_pattern(proc, dev_t)\
  int proc(dev_t *dev,\
           gs_gstate *pgs,\
           gs_id pattern_id,\
           int lock)
#define dev_proc_lock_pattern(proc)\
  dev_t_proc_lock_pattern(proc, gx_device)

                /* Added in release 3.60 */

#define dev_t_proc_fill_mask(proc, dev_t)\
  int proc(dev_t *dev,\
    const byte *data, int data_x, int raster, gx_bitmap_id id,\
    int x, int y, int width, int height,\
    const gx_drawing_color *pdcolor, int depth,\
    gs_logical_operation_t lop, const gx_clip_path *pcpath)
#define dev_proc_fill_mask(proc)\
  dev_t_proc_fill_mask(proc, gx_device)

                /* Added in release 3.66, changed in 3.69 */

#define dev_t_proc_fill_trapezoid(proc, dev_t)\
  int proc(dev_t *dev,\
    const gs_fixed_edge *left, const gs_fixed_edge *right,\
    fixed ybot, fixed ytop, bool swap_axes,\
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
#define dev_proc_fill_trapezoid(proc)\
  dev_t_proc_fill_trapezoid(proc, gx_device)

#define dev_t_proc_fill_parallelogram(proc, dev_t)\
  int proc(dev_t *dev,\
    fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,\
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
#define dev_proc_fill_parallelogram(proc)\
  dev_t_proc_fill_parallelogram(proc, gx_device)

#define dev_t_proc_fill_triangle(proc, dev_t)\
  int proc(dev_t *dev,\
    fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,\
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
#define dev_proc_fill_triangle(proc)\
  dev_t_proc_fill_triangle(proc, gx_device)

                /* adjustx and adjusty were added in 8.71 to get around a
                 * problem with PCL (Bug 691030). In the fullness of time
                 * hopefully PCL can be fixed to not need them and they can
                 * be removed again. */

#define dev_t_proc_draw_thin_line(proc, dev_t)\
  int proc(dev_t *dev,\
    fixed fx0, fixed fy0, fixed fx1, fixed fy1,\
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop,\
    fixed adjustx, fixed adjusty)
#define dev_proc_draw_thin_line(proc)\
  dev_t_proc_draw_thin_line(proc, gx_device)

                /* Added in release 3.68 */

#define dev_t_proc_strip_tile_rectangle(proc, dev_t)\
  int proc(dev_t *dev,\
    const gx_strip_bitmap *tiles, int x, int y, int width, int height,\
    gx_color_index color0, gx_color_index color1,\
    int phase_x, int phase_y)
#define dev_proc_strip_tile_rectangle(proc)\
  dev_t_proc_strip_tile_rectangle(proc, gx_device)

                /* Added in release 4.20 */

#define dev_t_proc_get_clipping_box(proc, dev_t)\
  void proc(dev_t *dev, gs_fixed_rect *pbox)
#define dev_proc_get_clipping_box(proc)\
  dev_t_proc_get_clipping_box(proc, gx_device)

                /* Added in release 5.20, changed in 5.23 */

#define dev_t_proc_begin_typed_image(proc, dev_t)\
  int proc(dev_t *dev,\
    const gs_gstate *pgs, const gs_matrix *pmat,\
    const gs_image_common_t *pim, const gs_int_rect *prect,\
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,\
    gs_memory_t *memory, gx_image_enum_common_t **pinfo)
#define dev_proc_begin_typed_image(proc)\
  dev_t_proc_begin_typed_image(proc, gx_device)

                /* Added in release 5.20 */

#define dev_t_proc_get_bits_rectangle(proc, dev_t)\
  int proc(dev_t *dev, const gs_int_rect *prect,\
    gs_get_bits_params_t *params)
#define dev_proc_get_bits_rectangle(proc)\
  dev_t_proc_get_bits_rectangle(proc, gx_device)

#define dev_t_proc_composite(proc, dev_t)\
  int proc(dev_t *dev,\
    gx_device **pcdev, const gs_composite_t *pcte,\
    gs_gstate *pgs, gs_memory_t *memory, gx_device *cdev)
#define dev_proc_composite(proc)\
  dev_t_proc_composite(proc, gx_device)\

                /* Added in release 5.23 */

#define dev_t_proc_get_hardware_params(proc, dev_t)\
  int proc(dev_t *dev, gs_param_list *plist)
#define dev_proc_get_hardware_params(proc)\
  dev_t_proc_get_hardware_params(proc, gx_device)

                /* Added in release 5.24 */

     /* ... text_begin ... see gstext.h for definition */

                /* Added in release 6.61 (raph) */

/*
  This area of the transparency facilities is in flux.  Here is a proposal
  for extending the driver interface.
*/

/*
  Push the current transparency state (*ppts) onto the associated stack,
  and set *ppts to a new transparency state of the given dimension.  The
  transparency state may copy some or all of the gs_gstate, such as the
  current alpha and/or transparency mask values, and definitely copies the
  parameters.
*/
#define dev_t_proc_begin_transparency_group(proc, dev_t)\
  int proc(gx_device *dev,\
    const gs_transparency_group_params_t *ptgp,\
    const gs_rect *pbbox,\
    gs_gstate *pgs,\
    gs_memory_t *mem)
#define dev_proc_begin_transparency_group(proc)\
  dev_t_proc_begin_transparency_group(proc, gx_device)

/*
  End a transparency group: blend the top element of the transparency
  stack, which must be a group, into the next-to-top element, popping the
  stack.  If the stack only had a single element, blend into the device
  output.  Set *ppts to 0 iff the stack is now empty.  If end_group fails,
  the stack is *not* popped.
*/
#define dev_t_proc_end_transparency_group(proc, dev_t)\
  int proc(gx_device *dev,\
    gs_gstate *pgs)
#define dev_proc_end_transparency_group(proc)\
  dev_t_proc_end_transparency_group(proc, gx_device)

/*
  Push the transparency state and prepare to render a transparency mask.
  This is similar to begin_transparency_group except that it only
  accumulates coverage values, not full pixel values.
*/
#define dev_t_proc_begin_transparency_mask(proc, dev_t)\
  int proc(gx_device *dev,\
    const gx_transparency_mask_params_t *ptmp,\
    const gs_rect *pbbox,\
    gs_gstate *pgs,\
    gs_memory_t *mem)
#define dev_proc_begin_transparency_mask(proc)\
  dev_t_proc_begin_transparency_mask(proc, gx_device)

/*
  Store a pointer to the rendered transparency mask into *pptm, popping the
  stack like end_group.  Normally, the client will follow this by using
  rc_assign to store the rendered mask into pgs->{opacity,shape}.mask.  If
  end_mask fails, the stack is *not* popped.
*/
#define dev_t_proc_end_transparency_mask(proc, dev_t)\
  int proc(gx_device *dev,\
    gs_gstate *pgs)
#define dev_proc_end_transparency_mask(proc)\
  dev_t_proc_end_transparency_mask(proc, gx_device)

/*
  This will clean up the entire device allocations as something went
  wrong in the middle of reading in the source content while we are dealing with
  a transparency device.
*/
#define dev_t_proc_discard_transparency_layer(proc, dev_t)\
  int proc(gx_device *dev,\
    gs_gstate *pgs)
#define dev_proc_discard_transparency_layer(proc)\
  dev_t_proc_discard_transparency_layer(proc, gx_device)

     /* (end of transparency driver interface extensions) */

     /* (start of DeviceN color support) */
/*
 * The following macros are defined in gxcmap.h
 *
 * dev_t_proc_get_color_mapping_procs
 * dev_proc_get_color_mapping_procs
 * dev_t_proc_get_color_comp_index
 * dev_proc_get_color_comp_index
 * dev_t_proc_encode_color
 * dev_proc_encode_color
 * dev_t_proc_decode_color
 * dev_proc_decode_color
 */
     /* (end of DeviceN color support) */

/*
  Fill rectangle with a high level color.
  Return rangecheck, if the device can't handle the high level color.

  The graphics library calls this function with degenerate (widths=0)
  rectangles, to know whether the device can handle a rectangle with
  the high level color. The device should skip such rectangles returning
  a proper code.

  Currently this function is used with gs_rectfill and gs_fillpage.  It is
  also used for the handling of the devn color type for supporting
  large number of spot colorants to planar separation devices.

*/

#define dev_t_proc_fill_rectangle_hl_color(proc, dev_t)\
  int proc(dev_t *dev, const gs_fixed_rect *rect, \
        const gs_gstate *pgs, const gx_drawing_color *pdcolor, \
        const gx_clip_path *pcpath)
#define dev_proc_fill_rectangle_hl_color(proc)\
  dev_t_proc_fill_rectangle_hl_color(proc, gx_device)

/*
  Include a color space into the output.
  This function is used to include DefaultGray, DefaultRGB,
  DefaultCMYK into PDF, PS, EPS output.
  Low level devices should ignore this call.
*/

#define dev_t_proc_include_color_space(proc, dev_t)\
  int proc(dev_t *dev, gs_color_space *cspace, const byte *res_name, int name_length)
#define dev_proc_include_color_space(proc)\
  dev_t_proc_include_color_space(proc, gx_device)

                /* Shading support. */

typedef struct gs_fill_attributes_s {
      const gs_fixed_rect *clip;
      bool swap_axes;
      const gx_device_halftone *ht; /* Reserved for possible use in future. */
      gs_logical_operation_t lop; /* Reserved for possible use in future. */
      fixed ystart, yend; /* Only for X-independent gradients. Base coordinates of the gradient. */
      patch_fill_state_t *pfs; /* For gx_fill_triangle_small. Clients must not change. */
} gs_fill_attributes;

/* Fill a linear color scanline. */

#define dev_t_proc_fill_linear_color_scanline(proc, dev_t)\
  int proc(dev_t *dev, const gs_fill_attributes *fa,\
        int i, int j, int w, /* scanline coordinates and width */\
        const frac31 *c0, /* initial color for the pixel (i,j), the integer part */\
        const int32_t *c0_f, /* initial color for the pixel (i,j), the fraction part numerator */\
        const int32_t *cg_num, /* color gradient numerator */\
        int32_t cg_den /* color gradient denominator */)
#define dev_proc_fill_linear_color_scanline(proc)\
  dev_t_proc_fill_linear_color_scanline(proc, gx_device)

/* Fill a linear color trapezoid. */
/* The server assumes a strongly linear color,
   i.e. it can ignore any of c0, c1, c2, c3. */
/* [p0 : p1] - left edge, from bottom to top.
   [p2 : p3] - right edge, from bottom to top.
   The filled area is within Y-spans of both edges. */
/* Either (c0 and c1) or (c2 and c3) may be NULL.
   In this case the color doesn't depend on X (on Y if fa->swap_axes).
   In this case the base coordinates for the color gradient
   may be unequal to p0, p1, p2, p3, and must be provided/taken
   in/from fa->ystart, fa->yend.
   The return value 0 is not allowed in this case. */
/* Return values :
  1 - success;
  0 - Too big. The area isn't filled. The client must decompose the area.
  <0 - error.
 */

#define dev_t_proc_fill_linear_color_trapezoid(proc, dev_t)\
  int proc(dev_t *dev, const gs_fill_attributes *fa,\
        const gs_fixed_point *p0, const gs_fixed_point *p1,\
        const gs_fixed_point *p2, const gs_fixed_point *p3,\
        const frac31 *c0, const frac31 *c1,\
        const frac31 *c2, const frac31 *c3)
#define dev_proc_fill_linear_color_trapezoid(proc)\
  dev_t_proc_fill_linear_color_trapezoid(proc, gx_device)

/* Fill a linear color triangle. */
/* Return values :
  1 - success;
  0 - Too big. The area isn't filled. The client must decompose the area.
  <0 - error.
 */

#define dev_t_proc_fill_linear_color_triangle(proc, dev_t)\
  int proc(dev_t *dev, const gs_fill_attributes *fa,\
        const gs_fixed_point *p0, const gs_fixed_point *p1,\
        const gs_fixed_point *p2,\
        const frac31 *c0, const frac31 *c1, const frac31 *c2)
#define dev_proc_fill_linear_color_triangle(proc)\
  dev_t_proc_fill_linear_color_triangle(proc, gx_device)

/*
 * Update the equivalent colors for spot colors in a color space.  The default
 * procedure does nothing.  However this routine provides a method for devices
 * to determine an equivalent color for a spot color.  See comments at the
 * start of src/gsequivc.c.
 */
#define dev_t_proc_update_spot_equivalent_colors(proc, dev_t)\
  int proc(dev_t *dev, const gs_gstate * pgs, const gs_color_space *pcs)
#define dev_proc_update_spot_equivalent_colors(proc)\
  dev_t_proc_update_spot_equivalent_colors(proc, gx_device)

/*
 * return a pointer to the devn_params section of a device.  Return NULL
 * if this field is not present within the device.
 */
typedef struct gs_devn_params_s gs_devn_params;

#define dev_t_proc_ret_devn_params(proc, dev_t)\
  gs_devn_params * proc(dev_t *dev)
#define dev_proc_ret_devn_params(proc)\
  dev_t_proc_ret_devn_params(proc, gx_device)
#define dev_proc_ret_devn_params_const(proc)\
  const dev_t_proc_ret_devn_params(proc, const gx_device)

/*
 * Erase page.
 */

#define dev_t_proc_fillpage(proc, dev_t)\
  int proc(gx_device *dev, gs_gstate * pgs, gx_device_color *pdevc)
#define dev_proc_fillpage(proc)\
  dev_t_proc_fillpage(proc, gx_device)

#define dev_t_proc_push_transparency_state(proc, dev_t)\
  int proc(gx_device *dev,\
    gs_gstate *pgs)
#define dev_proc_push_transparency_state(proc)\
  dev_t_proc_push_transparency_state(proc, gx_device)

#define dev_t_proc_pop_transparency_state(proc, dev_t)\
  int proc(gx_device *dev,\
    gs_gstate *pgs)
#define dev_proc_pop_transparency_state(proc)\
  dev_t_proc_pop_transparency_state(proc, gx_device)

#define dev_t_proc_put_image(proc, dev_t)\
  int proc(gx_device *dev, gx_device *mdev,  const byte **buffers, int num_chan, int x, int y,\
            int width, int height, int row_stride,\
            int alpha_plane_index, int tag_plane_index)
#define dev_proc_put_image(proc)\
  dev_t_proc_put_image(proc, gx_device)

#define dev_t_proc_dev_spec_op(proc, dev_t)\
  int proc(gx_device *dev, int op, void *data, int datasize)
#define dev_proc_dev_spec_op(proc)\
  dev_t_proc_dev_spec_op(proc, gx_device)

#define dev_t_proc_copy_planes(proc, dev_t)\
  int proc(dev_t *dev,\
    const byte *data, int data_x, int raster, gx_bitmap_id id,\
    int x, int y, int width, int height, int plane_height)
#define dev_proc_copy_planes(proc)\
  dev_t_proc_copy_planes(proc, gx_device)

#define dev_t_proc_get_profile(proc, dev_t)\
  int proc(const dev_t *dev, cmm_dev_profile_t **dev_profile)
#define dev_proc_get_profile(proc)\
  dev_t_proc_get_profile(proc, gx_device)

#define dev_t_proc_set_graphics_type_tag(proc, dev_t)\
  void proc(dev_t *dev, gs_graphics_type_tag_t)
#define dev_proc_set_graphics_type_tag(proc)\
  dev_t_proc_set_graphics_type_tag(proc, gx_device)

#define dev_t_proc_strip_copy_rop2(proc, dev_t)\
  int proc(dev_t *dev,\
    const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,\
    const gx_color_index *scolors,\
    const gx_strip_bitmap *textures, const gx_color_index *tcolors,\
    int x, int y, int width, int height,\
    int phase_x, int phase_y, gs_logical_operation_t lop,\
    uint planar_height)
#define dev_proc_strip_copy_rop2(proc)\
  dev_t_proc_strip_copy_rop2(proc, gx_device)

#define dev_t_proc_strip_tile_rect_devn(proc, dev_t)\
  int proc(dev_t *dev,\
    const gx_strip_bitmap *tiles, int x, int y, int width, int height,\
    const gx_drawing_color *pdcolor0, const gx_drawing_color *pdcolor1,\
    int phase_x, int phase_y)
#define dev_proc_strip_tile_rect_devn(proc)\
  dev_t_proc_strip_tile_rect_devn(proc, gx_device)

#define dev_t_proc_copy_alpha_hl_color(proc, dev_t)\
  int proc(dev_t *dev, const byte *data, int data_x,\
    int raster, gx_bitmap_id id, int x, int y, int width, int height,\
    const gx_drawing_color *pdcolor, int depth)
#define dev_proc_copy_alpha_hl_color(proc)\
  dev_t_proc_copy_alpha_hl_color(proc, gx_device)

typedef struct gx_process_page_options_s gx_process_page_options_t;

struct gx_process_page_options_s
{
    int (*init_buffer_fn)(void *arg, gx_device *dev, gs_memory_t *memory, int w, int h, void **buffer);
    void (*free_buffer_fn)(void *arg, gx_device *dev, gs_memory_t *memory, void *buffer);
    int (*process_fn)(void *arg, gx_device *dev, gx_device *bdev, const gs_int_rect *rect, void *buffer);
    int (*output_fn)(void *arg, gx_device *dev, void *buffer);
    void *arg;
    int options; /* A mask of GX_PROCPAGE_... options bits */
};

/* If GX_PROCPAGE_BOTTOM_UP, then we run from band n-1 to band 0, rather than
 * 0 to n-1. */
#define GX_PROCPAGE_BOTTOM_UP 1

#define dev_t_proc_process_page(proc, dev_t)\
  int proc(dev_t *dev, gx_process_page_options_t *options)
#define dev_proc_process_page(proc)\
  dev_t_proc_process_page(proc, gx_device)

typedef enum  {
    transform_pixel_region_begin = 0,
    transform_pixel_region_data_needed = 1,
    transform_pixel_region_process_data = 2,
    transform_pixel_region_end = 3
} transform_pixel_region_reason;

typedef struct {
    void *state;
    union {
        struct {
            const gs_int_rect *clip;
            int w; /* source width */
            int h; /* source height */
            int spp;
            const gx_dda_fixed_point *pixels; /* DDA to enumerate the destination positions of pixels across a row */
            const gx_dda_fixed_point *rows; /* DDA to enumerate the starting position of each row */
            gs_logical_operation_t lop;
        } init;
        struct {
            const unsigned char *buffer[GX_DEVICE_COLOR_MAX_COMPONENTS];
            int data_x;
            gx_cmapper_t *cmapper;
            const gs_gstate *pgs;
        } process_data;
    } u;
} transform_pixel_region_data;

#define dev_t_proc_transform_pixel_region(proc, dev_t)\
  int proc(dev_t *dev, transform_pixel_region_reason reason, transform_pixel_region_data *data)
#define dev_proc_transform_pixel_region(proc)\
  dev_t_proc_transform_pixel_region(proc, gx_device)


/* Define the device procedure vector template proper. */

#define gx_device_proc_struct(dev_t)\
{\
        dev_t_proc_initialize_device((*initialize_device), dev_t);\
        dev_t_proc_open_device((*open_device), dev_t);\
        dev_t_proc_get_initial_matrix((*get_initial_matrix), dev_t);\
        dev_t_proc_sync_output((*sync_output), dev_t);\
        dev_t_proc_output_page((*output_page), dev_t);\
        dev_t_proc_close_device((*close_device), dev_t);\
        dev_t_proc_map_rgb_color((*map_rgb_color), dev_t);\
        dev_t_proc_map_color_rgb((*map_color_rgb), dev_t);\
        dev_t_proc_fill_rectangle((*fill_rectangle), dev_t);\
        dev_t_proc_copy_mono((*copy_mono), dev_t);\
        dev_t_proc_copy_color((*copy_color), dev_t);\
        dev_t_proc_get_params((*get_params), dev_t);\
        dev_t_proc_put_params((*put_params), dev_t);\
        dev_t_proc_map_cmyk_color((*map_cmyk_color), dev_t);\
        dev_t_proc_get_page_device((*get_page_device), dev_t);\
        dev_t_proc_get_alpha_bits((*get_alpha_bits), dev_t);\
        dev_t_proc_copy_alpha((*copy_alpha), dev_t);\
        dev_t_proc_fill_path((*fill_path), dev_t);\
        dev_t_proc_stroke_path((*stroke_path), dev_t);\
        dev_t_proc_fill_mask((*fill_mask), dev_t);\
        dev_t_proc_fill_trapezoid((*fill_trapezoid), dev_t);\
        dev_t_proc_fill_parallelogram((*fill_parallelogram), dev_t);\
        dev_t_proc_fill_triangle((*fill_triangle), dev_t);\
        dev_t_proc_draw_thin_line((*draw_thin_line), dev_t);\
        dev_t_proc_strip_tile_rectangle((*strip_tile_rectangle), dev_t);\
        dev_t_proc_get_clipping_box((*get_clipping_box), dev_t);\
        dev_t_proc_begin_typed_image((*begin_typed_image), dev_t);\
        dev_t_proc_get_bits_rectangle((*get_bits_rectangle), dev_t);\
        dev_t_proc_composite((*composite), dev_t);\
        dev_t_proc_get_hardware_params((*get_hardware_params), dev_t);\
        dev_t_proc_text_begin((*text_begin), dev_t);\
        dev_t_proc_begin_transparency_group((*begin_transparency_group), dev_t);\
        dev_t_proc_end_transparency_group((*end_transparency_group), dev_t);\
        dev_t_proc_begin_transparency_mask((*begin_transparency_mask), dev_t);\
        dev_t_proc_end_transparency_mask((*end_transparency_mask), dev_t);\
        dev_t_proc_discard_transparency_layer((*discard_transparency_layer), dev_t);\
        dev_t_proc_get_color_mapping_procs((*get_color_mapping_procs), dev_t); \
        dev_t_proc_get_color_comp_index((*get_color_comp_index), dev_t); \
        dev_t_proc_encode_color((*encode_color), dev_t); \
        dev_t_proc_decode_color((*decode_color), dev_t); \
        dev_t_proc_fill_rectangle_hl_color((*fill_rectangle_hl_color), dev_t); \
        dev_t_proc_include_color_space((*include_color_space), dev_t); \
        dev_t_proc_fill_linear_color_scanline((*fill_linear_color_scanline), dev_t); \
        dev_t_proc_fill_linear_color_trapezoid((*fill_linear_color_trapezoid), dev_t); \
        dev_t_proc_fill_linear_color_triangle((*fill_linear_color_triangle), dev_t); \
        dev_t_proc_update_spot_equivalent_colors((*update_spot_equivalent_colors), dev_t); \
        dev_t_proc_ret_devn_params((*ret_devn_params), dev_t); \
        dev_t_proc_fillpage((*fillpage), dev_t); \
        dev_t_proc_push_transparency_state((*push_transparency_state), dev_t); \
        dev_t_proc_pop_transparency_state((*pop_transparency_state), dev_t); \
        dev_t_proc_put_image((*put_image), dev_t); \
        dev_t_proc_dev_spec_op((*dev_spec_op), dev_t); \
        dev_t_proc_copy_planes((*copy_planes), dev_t); \
        dev_t_proc_get_profile((*get_profile), dev_t); \
        dev_t_proc_set_graphics_type_tag((*set_graphics_type_tag), dev_t); \
        dev_t_proc_strip_copy_rop2((*strip_copy_rop2), dev_t);\
        dev_t_proc_strip_tile_rect_devn((*strip_tile_rect_devn), dev_t);\
        dev_t_proc_copy_alpha_hl_color((*copy_alpha_hl_color), dev_t);\
        dev_t_proc_process_page((*process_page), dev_t);\
        dev_t_proc_transform_pixel_region((*transform_pixel_region), dev_t);\
        dev_t_proc_fill_stroke_path((*fill_stroke_path), dev_t);\
        dev_t_proc_lock_pattern((*lock_pattern), dev_t);\
}

/*
 * Provide procedures for passing image data.  image_data and end_image
 * are the equivalents of the obsolete driver procedures.  image_plane_data
 * was originally planned as a driver procedure, but is now associated with
 * the image enumerator, like the other two.
 */

typedef struct gx_image_plane_s {
    const byte *data;
    int data_x;
    uint raster;
} gx_image_plane_t;

#define gx_device_begin_typed_image(dev, pgs, pmat, pim, prect, pdcolor, pcpath, memory, pinfo)\
  ((*dev_proc(dev, begin_typed_image))\
   (dev, pgs, pmat, pim, prect, pdcolor, pcpath, memory, pinfo))

int gx_image_data(gx_image_enum_common_t *info, const byte **planes,
                  int data_x, uint raster, int height);
/*
 * Solely for backward compatibility, gx_image_plane_data doesn't return
 * rows_used.
 */
int gx_image_plane_data(gx_image_enum_common_t *info,
                        const gx_image_plane_t *planes, int height);
int gx_image_plane_data_rows(gx_image_enum_common_t *info,
                             const gx_image_plane_t *planes, int height,
                             int *rows_used);
int gx_image_flush(gx_image_enum_common_t *info);
bool gx_image_planes_wanted(const gx_image_enum_common_t *info, byte *wanted);
int gx_image_end(gx_image_enum_common_t *info, bool draw_last);

/* A generic device procedure record. */
struct gx_device_procs_s gx_device_proc_struct(gx_device);

/*
 * Define unaligned analogues of the copy_xxx procedures.
 * These are slower than the standard procedures, which require
 * aligned bitmaps, and also are not portable to non-byte-addressed machines.
 *
 * We allow both unaligned data and unaligned scan line widths;
 * however, we do require that both of these be aligned modulo the largest
 * power of 2 bytes that divides the data depth, i.e.:
 *      depth   alignment
 *      <= 8    1
 *      16      2
 *      24      1
 *      32      4
 */
dev_proc_copy_mono(gx_copy_mono_unaligned);
dev_proc_copy_color(gx_copy_color_unaligned);
dev_proc_copy_alpha(gx_copy_alpha_unaligned);

/* A generic device */
struct gx_device_s {
    gx_device_common;
};

extern_st(st_device);
struct_proc_finalize(gx_device_finalize);	/* public for subclasses */
/* We use vacuous enum/reloc procedures, rather than 0, so that */
/* gx_device can have subclasses. */
#define public_st_device()	/* in gsdevice.c */\
  gs_public_st_complex_only(st_device, gx_device, "gx_device",\
    0, device_enum_ptrs, device_reloc_ptrs, gx_device_finalize)
#define st_device_max_ptrs 2

/* Enumerate or relocate a pointer to a device. */
/* These take the containing space into account properly. */
gx_device *gx_device_enum_ptr(gx_device *);
gx_device *gx_device_reloc_ptr(gx_device *, gc_state_t *);

/* Define typedefs for some of the device procedures, because */
/* ansi2knr can't handle dev_proc_xxx((*xxx)) in a formal argument list. */
typedef dev_proc_map_rgb_color((*dev_proc_map_rgb_color_t));
typedef dev_proc_map_color_rgb((*dev_proc_map_color_rgb_t));

/*
 * A forwarding device forwards all non-display operations, and possibly
 * some imaging operations (possibly transformed in some way), to another
 * device called the "target".  This is used for many different purposes
 * internally, including clipping, banding, image and pattern accumulation,
 * compositing, halftoning, and the null device.
 */
#define gx_device_forward_common\
        gx_device_common;\
        gx_device *target
/* A generic forwarding device. */
typedef struct gx_device_forward_s {
    gx_device_forward_common;
} gx_device_forward;

extern_st(st_device_forward);
#define public_st_device_forward()	/* in gsdevice.c */\
  gs_public_st_complex_only(st_device_forward, gx_device_forward,\
    "gx_device_forward", 0, device_forward_enum_ptrs,\
    device_forward_reloc_ptrs, gx_device_finalize)
#define st_device_forward_max_ptrs (st_device_max_ptrs + 1)

/* Test to see if the device wants to use tags */
static inline bool device_encodes_tags(const gx_device *dev)
{
    return (dev->graphics_type_tag & GS_DEVICE_ENCODES_TAGS) != 0;
}

static inline gs_graphics_type_tag_t device_current_tag(const gx_device *dev)
{
    gs_graphics_type_tag_t tag = dev->graphics_type_tag;
    return (tag & GS_DEVICE_ENCODES_TAGS) ? (tag & ~GS_DEVICE_ENCODES_TAGS) : 0;
}

static inline bool device_is_deep(const gx_device *dev)
{
    bool has_tags = device_encodes_tags(dev);
    int bits_per_comp = ((dev->color_info.depth - has_tags*8) /
                         dev->color_info.num_components);
    if (bits_per_comp > 16)
        return 1;
    if (bits_per_comp == 16 && dev->color_info.num_components > 1)
        return 1;
    if (bits_per_comp == 8)
        return 0;
    return (dev->color_info.max_color > 255 ||
            dev->color_info.max_gray > 255);
}

/* A null device.  This is used to temporarily disable output. */
struct gx_device_null_s {
    gx_device_forward_common;
};
extern const gx_device_null gs_null_device;

#define gx_device_is_null(dev)\
  ((dev)->dname == gs_null_device.dname)
extern_st(st_device_null);
#define public_st_device_null()	/* in gsdevice.c */\
  gs_public_st_complex_only(st_device_null, gx_device_null,\
    "gx_device_null", 0, device_forward_enum_ptrs,\
    device_forward_reloc_ptrs, gx_device_finalize)
#define st_device_null_max_ptrs st_device_forward_max_ptrs

/*
 * Initialize a just-allocated device from a prototype.  If internal =
 * false, the device is marked retained; if internal = true, the device is
 * not marked retained.  See the beginning of this file for more information
 * about what this means.  Normally, devices created for temporary use have
 * internal = true (retained = false).
 */
int gx_device_init(gx_device * dev, const gx_device * proto,
                   gs_memory_t * mem, bool internal);

/*
 * Identical to gx_device_init, except that the reference counting is set
 * up so that it doesn't attempt to free the device structure when the last
 * instance is removed, and the device is always internal (never retained).
 *
 * If the device uses an initialize proc (and it should!) it can never
 * fail.
 */
void gx_device_init_on_stack(gx_device * dev, const gx_device * proto,
                             gs_memory_t * mem);

/* Make a null device. */
/* The gs_memory_t argument is 0 if the device is temporary and local, */
/* or the allocator that was used to allocate it if it is a real object. */
void gs_make_null_device(gx_device_null *dev_null, gx_device *target,
                         gs_memory_t *mem);
/* Is a null device ? */
bool gs_is_null_device(gx_device *dev);

/* Set the target of a (forwarding) device. */
void gx_device_set_target(gx_device_forward *fdev, gx_device *target);

/* Mark a device as retained or not retained. */
void gx_device_retain(gx_device *dev, bool retained);

/* Calculate the raster (number of bytes in a scan line), */
/* with byte or device padding. */
uint gx_device_raster(const gx_device * dev, bool pad_to_word);

/* Calculate the raster (number of bytes in a scan line), */
/* with byte or device padding forcing chunky format. */
uint gx_device_raster_chunky(const gx_device * dev, bool pad);

/* Calculate the raster (with device padding) optionally for a given
 * render_plane (may be NULL). */
uint gx_device_raster_plane(const gx_device * dev, const gx_render_plane_t *render_plane);

/* Adjust the resolution for devices that only have a fixed set of */
/* geometries, so that the apparent size in inches remains constant. */
/* If fit=1, the resolution is adjusted so that the entire image fits; */
/* if fit=0, one dimension fits, but the other one is clipped. */
int gx_device_adjust_resolution(gx_device * dev, int actual_width, int actual_height, int fit);

/* Set the HWMargins to values defined in inches. */
/* If move_origin is true, also reset the Margins. */
void gx_device_set_margins(gx_device * dev, const float *margins /*[4] */ ,
                           bool move_origin);

/* Set the width and height (in pixels), updating MediaSize. */
void gx_device_set_width_height(gx_device * dev, int width, int height);

/* Set the resolution (in pixels per inch), updating width and height. */
void gx_device_set_resolution(gx_device * dev, double x_dpi, double y_dpi);

/* Set the MediaSize (in 1/72" units), updating width and height. */
void gx_device_set_media_size(gx_device * dev, double media_width, double media_height);

/****** BACKWARD COMPATIBILITY ******/
#define gx_device_set_page_size(dev, w, h)\
  gx_device_set_media_size(dev, w, h)

/*
 * Temporarily install a null device, or a special device such as
 * a clipping or cache device.
 */
void gx_set_device_only(gs_gstate *, gx_device *);

/* Close a device. */
int gs_closedevice(gx_device *);

/* "Free" a device locally allocated on the stack, by finalizing it. */
void gx_device_free_local(gx_device *);

/* ------ Device types (an unused concept right now) ------ */

#define dev_type_proc_initialize(proc)\
  int proc(gx_device *)

typedef struct gx_device_type_s {
    gs_memory_type_ptr_t stype;
    dev_type_proc_initialize((*initialize));
} gx_device_type;

#define device_type(dtname, stype, initproc)\
static dev_type_proc_initialize(initproc);\
const gx_device_type dtname = { &stype, initproc }

/*dev_type_proc_initialize(gdev_initialize); */

#ifdef DEBUG
void gx_device_dump(gx_device *dev, const char *text);
#endif

/* Compare color information structures */
bool gx_color_info_equal(const gx_device_color_info *p1, const gx_device_color_info *p2);

/* Perform a callout to registered handlers from the device. */
int gx_callout(gx_device *dev, int id, int size, void *data);

static inline
gx_cm_opmsupported_t gx_get_opmsupported(gx_device *dev)
{
    if (dev->color_info.opmsupported != GX_CINFO_OPMSUPPORTED_UNKNOWN)
        return dev->color_info.opmsupported;

    check_opmsupported(dev);

    return dev->color_info.opmsupported;
}

static inline
gx_color_index gx_get_process_comps(gx_device *dev)
{
    if (dev->color_info.opmsupported != GX_CINFO_OPMSUPPORTED_UNKNOWN)
        return dev->color_info.process_comps;

    return check_cmyk_color_model_comps(dev);
}


#endif /* gxdevcli_INCLUDED */
