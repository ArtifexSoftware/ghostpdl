/* Copyright (C) 2001-2025 Artifex Software, Inc.
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


/* Definitions for implementors of image types */

#ifndef gxiparam_INCLUDED
#  define gxiparam_INCLUDED

#include "gsstype.h"		/* for extern_st */
#include "gxdevcli.h"
#include "scommon.h"

/* ---------------- Image types ---------------- */

/* Define the structure for image types. */

struct gx_image_type_s {

    /*
     * Define the storage type for this type of image.
     */
    gs_memory_type_ptr_t stype;

    /*
     * Provide the default implementation of begin_typed_image for this
     * type.
     */
    dev_proc_begin_typed_image((*begin_typed_image));

    /*
     * Put image parameters on a stream.  Currently this is used
     * only for banding.  If the parameters include a color space,
     * store it in *ppcs.
     */
#define image_proc_sput(proc)\
  int proc(const gs_image_common_t *pic, stream *s,\
    const gs_color_space **ppcs)

    image_proc_sput((*sput));

    /*
     * Get image parameters from a stream.  Currently this is used
     * only for banding.  If the parameters include a color space,
     * use pcs.
     */
#define image_proc_sget(proc)\
  int proc(gs_image_common_t *pic, stream *s, gs_color_space *pcs)

    image_proc_sget((*sget));

    /*
     * Release any parameters allocated by sget.
     * Currently this only frees the parameter structure itself.
     */
#define image_proc_release(proc)\
  void proc(gs_image_common_t *pic, gs_memory_t *mem)

    image_proc_release((*release));

    /*
     * We put index last so that if we add more procedures and some
     * implementor fails to initialize them, we'll get a type error.
     */
    int index;		/* PostScript ImageType */
};

/*
 * Define dummy sput/sget/release procedures for image types that don't
 * implement these functions.
 */
image_proc_sput(gx_image_no_sput); /* rangecheck error */
image_proc_sget(gx_image_no_sget); /* rangecheck error */
image_proc_release(gx_image_default_release); /* just free the params */
/*
 * Define sput/sget/release procedures for generic pixel images.
 * Note that these procedures take different parameters.
 */
int gx_pixel_image_sput(const gs_pixel_image_t *pic, stream *s,
                        const gs_color_space **ppcs, int extra);
int gx_pixel_image_sget(gs_pixel_image_t *pic, stream *s,
                        gs_color_space *pcs);
void gx_pixel_image_release(gs_pixel_image_t *pic, gs_memory_t *mem);

/* Internal procedures for use in sput/sget implementations. */
bool gx_image_matrix_is_default(const gs_data_image_t *pid);
void gx_image_matrix_set_default(gs_data_image_t *pid);
void sput_variable_uint(stream *s, uint w);
int sget_variable_uint(stream *s, uint *pw);
#define DECODE_DEFAULT(i, dd1)\
  ((i) == 1 ? dd1 : (i) & 1)

/* ---------------- Image enumerators ---------------- */

/*
 * Define the procedures associated with an image enumerator.
 */
typedef struct gx_image_enum_procs_s {

    /*
     * Pass the next batch of data for processing.  *rows_used is set
     * even in the case of an error.
     */

#define image_enum_proc_plane_data(proc)\
  int proc(gx_image_enum_common_t *info, const gx_image_plane_t *planes,\
           int height, int *rows_used)

    image_enum_proc_plane_data((*plane_data));

    /*
     * End processing an image, freeing the enumerator.  We keep this
     * procedure as the last required one, so that we can detect obsolete
     * static initializers.
     */

#define image_enum_proc_end_image(proc)\
  int proc(gx_image_enum_common_t *info, bool draw_last)

    image_enum_proc_end_image((*end_image));

    /*
     * Flush any intermediate buffers to the target device.
     * We need this for situations where two images interact
     * (currently, only the mask and the data of ImageType 3).
     *
     * This procedure is optional (may be 0).
     */

#define image_enum_proc_flush(proc)\
  int proc(gx_image_enum_common_t *info)

    image_enum_proc_flush((*flush));

    /*
     * Determine which data planes should be passed on the next call to the
     * plane_data procedure, by filling wanted[0 .. num_planes - 1] with 0
     * for unwanted planes and non-0 for wanted planes.  This procedure may
     * also change the plane_depths[] and/or plane_widths[] values.  The
     * procedure returns true if the returned vector will always be the same
     * *and* the plane depths and widths remain constant, false if the
     * wanted planes *or* plane depths or widths may vary over the course of
     * the image.  Note also that the only time a plane's status can change
     * from wanted to not wanted, and the only time a wanted plane's depth
     * or width can change, is after a call of plane_data that actually
     * provides data for that plane.
     *
     * By default, all data planes are always wanted; however, ImageType 3
     * images with separate mask and image data sources may want mask data
     * before image data or vice versa.
     *
     * This procedure is optional (may be 0).
     */

#define image_enum_proc_planes_wanted(proc)\
  bool proc(const gx_image_enum_common_t *info, byte *wanted)

    image_enum_proc_planes_wanted((*planes_wanted));

} gx_image_enum_procs_t;

/*
 * Define the common prefix of the image enumerator structure.  All
 * implementations of begin[_typed]_image must initialize all of the members
 * of this structure, by calling gx_image_enum_common_init and then filling
 * in whatever else they need to.
 *
 * Note that the structure includes a unique ID, so that the banding
 * machinery could in principle keep track of multiple enumerations that may
 * be in progress simultaneously.
 */
#define gx_image_enum_common\
        const gx_image_type_t *image_type;\
        const gx_image_enum_procs_t *procs;\
        gx_device *dev;\
        gs_memory_t *memory;	/* from begin_image */\
        const gs_gstate *pgs; \
        int pgs_level;         /* Used for sanity checking */\
        gs_id id;\
        bool skipping; /* don't render, just consume image streams. */\
        int num_planes;\
        char *OC;     /* Only used currently for pdfwrite with image type 3x images because we need to know and set this for the type 3 image, but not the mask */\
        int plane_depths[GS_IMAGE_MAX_COMPONENTS]; /* [num_planes] */\
        int plane_widths[GS_IMAGE_MAX_COMPONENTS]  /* [num_planes] */
struct gx_image_enum_common_s {
    gx_image_enum_common;
};

extern_st(st_gx_image_enum_common);
#define public_st_gx_image_enum_common()	/* in gdevddrw.c */\
  gs_public_st_composite(st_gx_image_enum_common, gx_image_enum_common_t,\
    "gx_image_enum_common_t",\
     image_enum_common_enum_ptrs, image_enum_common_reloc_ptrs)

/*
 * Initialize the common part of an image enumerator.
 */
int gx_image_enum_common_init(gx_image_enum_common_t * piec,
                              const gs_data_image_t * pic,
                              const gx_image_enum_procs_t * piep,
                              gx_device * dev, int num_components,
                              gs_image_format_t format);

/*
 * Define image_plane_data and end_image procedures for image types that
 * don't have any source data (ImageType 2 and composite images).
 */
image_enum_proc_plane_data(gx_no_plane_data);
image_enum_proc_end_image(gx_ignore_end_image);
/*
 * Define the procedures and type data for ImageType 1 images,
 * which are always included and which are shared with ImageType 4.
 */
dev_proc_begin_typed_image(gx_begin_image1);
image_enum_proc_plane_data(gx_image1_plane_data);
image_enum_proc_end_image(gx_image1_end_image);
image_enum_proc_flush(gx_image1_flush);

/* Free the image enumerator. */
void gx_image_free_enum(gx_image_enum_common_t **ppenum);

/* Needed in clist image (high level) color monitoring set up */
void image_init_map(byte * map, int map_size, const float *decode);

#endif /* gxiparam_INCLUDED */
