/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.

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


/* Definitions for implementors of image types */

#ifndef gxiparam_INCLUDED
#  define gxiparam_INCLUDED

#include "gxdevcli.h"

/* ---------------- Image types ---------------- */

/* Define the structure for image types. */

#ifndef stream_DEFINED
#  define stream_DEFINED
typedef struct stream_s stream;
#endif

#ifndef gx_image_type_DEFINED
#  define gx_image_type_DEFINED
typedef struct gx_image_type_s gx_image_type_t;
#endif

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
     * Compute the width and height of the source data.  For images with
     * explicit data, this information is in the gs_data_image_t
     * structure, but ImageType 2 images must compute it.
     */
#define image_proc_source_size(proc)\
  int proc(P3(const gs_imager_state *pis, const gs_image_common_t *pic,\
    gs_int_point *psize))

    image_proc_source_size((*source_size));

    /*
     * Put image parameters on a stream.  Currently this is used
     * only for banding.  If the parameters include a color space,
     * store it in *ppcs.
     */
#define image_proc_sput(proc)\
  int proc(P3(const gs_image_common_t *pic, stream *s,\
	      const gs_color_space **ppcs))

    image_proc_sput((*sput));

    /*
     * Get image parameters from a stream.  Currently this is used
     * only for banding.  If the parameters include a color space,
     * use pcs.
     */
#define image_proc_sget(proc)\
  int proc(P3(gs_image_common_t *pic, stream *s, const gs_color_space *pcs))

    image_proc_sget((*sget));

    /*
     * Release any parameters allocated by sget.
     * Currently this only frees the parameter structure itself.
     */
#define image_proc_release(proc)\
  void proc(P2(gs_image_common_t *pic, gs_memory_t *mem))

    image_proc_release((*release));

    /*
     * We put index last so that if we add more procedures and some
     * implementor fails to initialize them, we'll get a type error.
     */
    int index;		/* PostScript ImageType */
};

/*
 * Define the procedure for getting the source size of an image with
 * explicit data.
 */
image_proc_source_size(gx_data_image_source_size);
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
int gx_pixel_image_sput(P4(const gs_pixel_image_t *pic, stream *s,
			   const gs_color_space **ppcs, int extra));
int gx_pixel_image_sget(P3(gs_pixel_image_t *pic, stream *s,
			   const gs_color_space *pcs));
void gx_pixel_image_release(P2(gs_pixel_image_t *pic, gs_memory_t *mem));

/* Internal procedures for use in sput/sget implementations. */
bool gx_image_matrix_is_default(P1(const gs_data_image_t *pid));
void gx_image_matrix_set_default(P1(gs_data_image_t *pid));
void sput_variable_uint(P2(stream *s, uint w));
int sget_variable_uint(P2(stream *s, uint *pw));
#define DECODE_DEFAULT(i, dd1)\
  ((i) == 1 ? dd1 : (i) & 1)

/* ---------------- Image enumerators ---------------- */

#ifndef gx_image_enum_common_t_DEFINED
#  define gx_image_enum_common_t_DEFINED
typedef struct gx_image_enum_common_s gx_image_enum_common_t;
#endif

/*
 * Define the procedures associated with an image enumerator.
 *
 * Note that image_plane_data and end_image used to be device procedures;
 * they still take the device argument first for compatibility.  However, in
 * order to make forwarding begin_image work, the intermediary routines
 * gx_image_[plane_]data and gx_image_end substitute the device from the
 * enumerator for the explicit device argument, which is ignored.
 * Eventually we should fix this by removing the device argument from
 * gx_device..., just as we have done for text enumeration; but this would
 * have caused major difficulties with 5.1x retrofitting of this code, and
 * it's too much work to fix right now.  ****** FIX THIS SOMEDAY ******
 */
typedef struct gx_image_enum_procs_s {

    /*
     * Pass the next batch of data for processing.
     * image_enum_proc_plane_data is defined in gxdevcli.h.
     */

    image_enum_proc_plane_data((*plane_data));

    /*
     * End processing an image.  We keep this procedure as the last one that
     * requires initialization, so that we can detect obsolete static
     * initializers.  dev_proc_end_image is defined in gxdevcli.h.
     */
#define image_enum_proc_end_image(proc)\
  dev_proc_end_image(proc)

    image_enum_proc_end_image((*end_image));

    /*
     * Flush any intermediate buffers to the target device.
     * We need this for situations where two images interact
     * (currently, only the mask and the data of ImageType 3).
     * This procedure is optional (may be 0).
     */
#define image_enum_proc_flush(proc)\
  int proc(P1(gx_image_enum_common_t *info))

    image_enum_proc_flush((*flush));

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
	gs_id id;\
	int num_planes;\
	int plane_depths[gs_image_max_planes]	/* [num_planes] */
struct gx_image_enum_common_s {
    gx_image_enum_common;
};

/*extern_st(st_gx_image_enum_common); */
#define public_st_gx_image_enum_common()	/* in gdevddrw.c */\
  gs_public_st_composite(st_gx_image_enum_common, gx_image_enum_common_t,\
    "gx_image_enum_common_t",\
     image_enum_common_enum_ptrs, image_enum_common_reloc_ptrs)

/*
 * Initialize the common part of an image enumerator.
 */
int gx_image_enum_common_init(P7(gx_image_enum_common_t * piec,
				 const gs_image_common_t * pic,
				 const gx_image_enum_procs_t * piep,
				 gx_device * dev,
				 int bits_per_component, int num_components,
				 gs_image_format_t format));

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

#endif /* gxiparam_INCLUDED */
