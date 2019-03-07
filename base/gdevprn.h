/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Common header file for memory-buffered printers */

#ifndef gdevprn_INCLUDED
#  define gdevprn_INCLUDED

#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gp.h"			/* for gp_file_name_sizeof */
#include "gserrors.h"
#include "gsmatrix.h"		/* for gxdevice.h */
#include "gsutil.h"		/* for memflip8x8 */
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxclist.h"
#include "gxclthrd.h"		/* for background printing */
#include "gxclpage.h"		/* for saved_pages */
#include "gxrplane.h"
#include "gsparam.h"

/*
 * Define the parameters for the printer rendering method.
 * If the entire bitmap fits in MAX_BITMAP, and there is at least
 * PRN_MIN_MEMORY_LEFT memory left after allocating it, render in RAM,
 * otherwise use a command list with a size of PRN_BUFFER_SPACE.
 * (These are parameters that can be changed by a client program.)
 */
#define PRN_MAX_BITMAP MAX_BITMAP	/* see gxdevice.h */
#define PRN_BUFFER_SPACE BUFFER_SPACE	/* see gxdevice.h */
#define PRN_MIN_MEMORY_LEFT MIN_MEMORY_LEFT	/* see gxdevice.h */
#define PRN_MIN_BUFFER_SPACE MIN_BUFFER_SPACE	/* see gxdevice.h */

/* Define the abstract type for parameters describing buffer space. */
typedef struct gdev_space_params_s gdev_prn_space_params;

/* Define the abstract type for parameters describing buffer space. */
typedef struct gdev_banding_type gdev_prn_banding_type;

/* Define the abstract type for some band device procedures' arguments. */
typedef struct gdev_prn_start_render_params_s gdev_prn_start_render_params;

/*
 * Define the special procedures for band devices.
 */
typedef struct gx_printer_device_procs_s {

    /*
     * Print the page on the output file.  Required only for devices
     * where output_page is gdev_prn_output_page or gdev_prn_bg_output_page
     * ignored for other devices (unless their output_page calls those).
     */

#define prn_dev_proc_print_page(proc)\
  int proc(gx_device_printer *, gp_file *)
    prn_dev_proc_print_page((*print_page));
/* BACKWARD COMPATIBILITY */
#define dev_proc_print_page(proc) prn_dev_proc_print_page(proc)

    /* Print the page on the output file, with a given # of copies. */

#define prn_dev_proc_print_page_copies(proc)\
  int proc(gx_device_printer *, gp_file *, int)
    prn_dev_proc_print_page_copies((*print_page_copies));
/* BACKWARD COMPATIBILITY */
#define dev_proc_print_page_copies(proc) prn_dev_proc_print_page_copies(proc)

    /*
     * Define buffer device management procedures.
     * See gxdevcli.h for the definitions.
     */

    gx_device_buf_procs_t buf_procs;

    /*
     * Compute effective space params. These results effectively override
     * the space_params in the device, but does not replace them; that is to
     * say that computed space params are temps used for computation.
     * Procedure must fill in only those space_params that it wishes to
     * override, using curr width, height, margins, etc.
     *
     * Caller is gdevprn.open & gdevprn.put_params, calls driver or
     * default.
     */

#define prn_dev_proc_get_space_params(proc)\
  void proc(const gx_device_printer *, gdev_prn_space_params *)
    prn_dev_proc_get_space_params((*get_space_params));

} gx_printer_device_procs;

/* ------ Printer device definition ------ */

/* Structure for generic printer devices. */
/* This must be preceded by gx_device_common. */
/* Printer devices are actually a union of a memory device */
/* and a clist device, plus some additional state. */

#define prn_fname_sizeof gp_file_name_sizeof

typedef struct bg_print_s {
    gx_semaphore_t *sema;		/* used by foreground to wait */
    gx_device *device;			/* printer/clist device for bg printing */
    gp_thread_id thread_id;
    int num_copies;
    int return_code;			/* result from background print thread */
    char *ocfname;	                /* command file name */
    clist_file_ptr ocfile;	        /* command file, normally 0 */
    char *obfname;	                /* block file name */
    clist_file_ptr obfile;	/* block file, normally 0 */
    const clist_io_procs_t *oio_procs;
} bg_print_t;

#define gx_prn_device_common\
        byte skip[max(sizeof(gx_device_memory), sizeof(gx_device_clist)) -\
                  sizeof(gx_device) + sizeof(double) /* padding */];\
        gx_printer_device_procs printer_procs;\
                /* ------ Device parameters that must be set ------ */\
                /* ------ before calling the device open routine. ------ */\
        char fname[prn_fname_sizeof];	/* OutputFile */\
                /* ------ Other device parameters ------ */\
        bool OpenOutputFile;\
        bool ReopenPerPage;\
        bool Duplex;\
        int Duplex_set;		        /* -1 = not supported */\
                /* ------ End of parameters ------ */\
        bool file_is_new;		/* true iff file just opened */\
        gp_file *file;  		/* output file */\
        long buffer_space;	        /* amount of space for clist buffer, */\
                                        /* 0 means not using clist */\
        byte *buf;			/* buffer for rendering */\
        gs_memory_t *buffer_memory;	/* allocator for command list */\
        gs_memory_t *bandlist_memory;	/* allocator for bandlist files */\
        uint clist_disable_mask;	/* mask of clist options to disable */\
        bool bg_print_requested;	/* request background printing of page from clist */\
        bg_print_t bg_print;            /* background printing data shared with thread */\
        int num_render_threads_requested;	/* for multiple band rendering threads */\
        gx_saved_pages_list *saved_pages_list;	/* list when we are saving pages instead of printing */\
        gx_device_procs save_procs_while_delaying_erasepage;	/* save device procs while delaying erasepage. */\
        gx_device_procs orig_procs	/* original (std_)procs */

/* The device descriptor */
struct gx_device_printer_s {
    gx_device_common;
    gx_prn_device_common;
};

/* A useful check to determine if the page is being rendered as a clist */
#define PRINTER_IS_CLIST(pdev) ((gx_device_printer *)(pdev)->buffer_space != 0)

extern_st(st_device_printer);
#define public_st_device_printer()	/* in gdevprn.c */\
  gs_public_st_complex_only(st_device_printer, gx_device_printer,\
    "gx_device_printer", 0, device_printer_enum_ptrs,\
    device_printer_reloc_ptrs, gx_device_finalize)

/* Define a typedef for the sake of ansi2knr. */
typedef dev_proc_print_page((*dev_proc_print_page_t));

/* Standard device procedures for printers */
dev_proc_open_device(gdev_prn_open);
dev_proc_output_page(gdev_prn_output_page);
dev_proc_output_page(gdev_prn_output_page_seekable);
dev_proc_output_page(gdev_prn_bg_output_page);
dev_proc_output_page(gdev_prn_bg_output_page_seekable);
dev_proc_close_device(gdev_prn_close);
#define gdev_prn_map_rgb_color gx_default_b_w_map_rgb_color
#define gdev_prn_map_color_rgb gx_default_b_w_map_color_rgb
dev_proc_get_params(gdev_prn_get_params);
dev_proc_put_params(gdev_prn_put_params);
dev_proc_dev_spec_op(gdev_prn_forwarding_dev_spec_op);
dev_proc_dev_spec_op(gdev_prn_dev_spec_op);

void gdev_prn_finalize(gx_device *dev);		/* cleanup when device is freed */
int gdev_prn_get_param(gx_device *dev, char *Param, void *list);

/* Default printer-specific procedures */
/* VMS limits procedure names to 31 characters. */
prn_dev_proc_get_space_params(gx_default_get_space_params);
/* BACKWARD COMPATIBILITY */
#define gdev_prn_default_get_space_params gx_default_get_space_params

/* Macro for generating procedure table */
#define prn_procs(p_open, p_output_page, p_close)\
  prn_color_procs_enc_dec(p_open, p_output_page, p_close, gdev_prn_map_rgb_color, gdev_prn_map_color_rgb, gdev_prn_map_rgb_color, gdev_prn_map_color_rgb)
#define prn_params_procs(p_open, p_output_page, p_close, p_get_params, p_put_params)\
  prn_color_params_procs_enc_dec(p_open, p_output_page, p_close, gdev_prn_map_rgb_color, gdev_prn_map_color_rgb, p_get_params, p_put_params, gdev_prn_map_rgb_color, gdev_prn_map_color_rgb)
#define prn_color_procs(p_open, p_output_page, p_close, p_map_rgb_color, p_map_color_rgb)\
  prn_color_params_procs(p_open, p_output_page, p_close, p_map_rgb_color, p_map_color_rgb, gdev_prn_get_params, gdev_prn_put_params)
#define prn_color_procs_enc_dec(p_open, p_output_page, p_close, p_map_rgb_color, p_map_color_rgb, p_encode_color, p_decode_color)\
  prn_color_params_procs_enc_dec(p_open, p_output_page, p_close, p_map_rgb_color, p_map_color_rgb, gdev_prn_get_params, gdev_prn_put_params, p_encode_color, p_decode_color)
/* See gdev_prn_open for explanation of the NULLs below. */
#define prn_color_params_procs(p_open, p_output_page, p_close, p_map_rgb_color, p_map_color_rgb, p_get_params, p_put_params) \
  prn_color_params_procs_enc_dec(p_open, p_output_page, p_close, p_map_rgb_color, p_map_color_rgb, p_get_params, p_put_params, NULL, NULL)
#define prn_color_params_procs_enc_dec(p_open, p_output_page, p_close, p_map_rgb_color, p_map_color_rgb, p_get_params, p_put_params, p_encode_color, p_decode_color) {\
        p_open,\
        NULL,	/* get_initial_matrix */\
        NULL,	/* sync_output */\
        p_output_page,\
        p_close,\
        p_map_rgb_color,\
        p_map_color_rgb,\
        NULL,	/* fill_rectangle */\
        NULL,	/* tile_rectangle */\
        NULL,	/* copy_mono */\
        NULL,	/* copy_color */\
        NULL,	/* draw_line */\
        NULL,	/* get_bits */\
        p_get_params,\
        p_put_params,\
        NULL,	/* map_cmyk_color */\
        NULL,	/* get_xfont_procs */\
        NULL,	/* get_xfont_device */\
        NULL,	/* map_rgb_alpha_color */\
        gx_page_device_get_page_device,\
        NULL,	/* get_alpha_bits */\
        NULL,	/* copy_alpha */\
        NULL,	/* get_band */\
        NULL,	/* copy_rop */\
        NULL,	/* fill_path */\
        NULL,	/* stroke_path */\
        NULL,	/* fill_mask */\
        NULL,	/* fill_trapezoid */\
        NULL,	/* fill_parallelogram */\
        NULL,	/* fill_triangle */\
        NULL,	/* draw_thin_line */\
        NULL,	/* begin_image */\
        NULL,	/* image_data */\
        NULL,	/* end_image */\
        NULL,	/* strip_tile_rectangle */\
        NULL,	/* strip_copy_rop, */\
        NULL,	/* get_clipping_box */\
        NULL,	/* begin_typed_image */\
        NULL,	/* get_bits_rectangle */\
        NULL,	/* map_color_rgb_alpha */\
        NULL,	/* create_compositor */\
        NULL,	/* get_hardware_params */\
        NULL,	/* text_begin */\
        NULL,	/* finish_copydevice */\
        NULL,	/* begin_transparency_group */\
        NULL,	/* end_transparency_group */\
        NULL,	/* begin_transparency_mask */\
        NULL,	/* end_transparency_mask */\
        NULL,	/* discard_transparency_layer */\
        NULL,  /* get_color_mapping_procs */\
        NULL,  /* get_color_comp_index */\
        p_encode_color,	/* encode_color */\
        p_decode_color,	/* decode_color */\
        NULL,  /* pattern_manage */\
        NULL,  /* fill_rectangle_hl_color */\
        NULL,  /* include_color_space */\
        NULL,  /* fill_linear_color_scanline */\
        NULL,  /* fill_linear_color_trapezoid */\
        NULL,  /* fill_linear_color_triangle */\
        NULL,  /* update_spot_equivalent_colors */\
        NULL,  /* ret_devn_params */\
        NULL,  /* fillpage */\
        NULL,  /* push_transparency_state */\
        NULL,  /* pop_transparency_state */\
        NULL,  /* put_image */\
        gdev_prn_dev_spec_op,  /* dev_spec_op */\
        NULL,  /* copy plane */\
        gx_default_get_profile, /* get_profile */\
        gx_default_set_graphics_type_tag /* set_graphics_type_tag */\
}

/* The standard printer device procedures */
/* (using gdev_prn_open/output_page/close). */
extern const gx_device_procs prn_std_procs;
extern const gx_device_procs prn_bg_procs;

/*
 * Define macros for generating the device structure,
 * analogous to the std_device_body macros in gxdevice.h
 * Note that the macros are broken up so as to be usable for devices that
 * add further initialized state to the printer device.
 *
 * The 'margin' values provided here specify the unimageable region
 * around the edges of the page (in inches), and the left and top margins
 * also specify the displacement of the device (0,0) point from the
 * upper left corner.  We should provide macros that allow specifying
 * all 6 values independently, but we don't yet.
 *
 * Note that print_page and print_page_copies must not both be defaulted.
 */
#define prn_device_body_rest2_(print_page, print_page_copies, duplex_set)\
         { 0 },		/* std_procs */\
         { 0 },		/* skip */\
         { print_page,\
           print_page_copies,\
           { gx_default_create_buf_device,\
             gx_default_size_buf_device,\
             gx_default_setup_buf_device,\
             gx_default_destroy_buf_device\
           },\
           gdev_prn_default_get_space_params\
         },\
         { 0 },		/* fname */\
        0/*false*/,	/* OpenOutputFile */\
        0/*false*/,	/* ReopenPerPage */\
        0/*false*/, duplex_set,	/* Duplex[_set] */\
        0/*false*/,	/* file_is_new */\
        0,	        /* *file */\
        0,		/* buffer_space */\
        0,		/* *buf */\
        0,		/* *buffer_memory */\
        0,		/* *bandlist_memory */\
        0,		/* clist_disable_mask */\
        0/*false*/,	/* bg_print_requested */\
        {  0/*sema*/, 0/*device*/, 0/*thread_id*/, 0/*num_copies*/, 0/*return_code*/ }, /* bg_print */\
        0, 		/* num_render_threads_requested */\
        0,              /* saved_pages_list */\
        { 0 },	/* save_procs_while_delaying_erasepage */\
        { 0 }	/* ... orig_procs */
#define prn_device_body_rest_(print_page)\
  prn_device_body_rest2_(print_page, gx_default_print_page_copies, -1)
#define prn_device_body_copies_rest_(print_page_copies)\
  prn_device_body_rest2_(gx_print_page_single_copy, print_page_copies, -1)

/* The Sun cc compiler won't allow \ within a macro argument list. */
/* This accounts for the short parameter names here and below. */
#define prn_device_margins_body(dtype, procs, dname, w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, ncomp, depth, mg, mc, dg, dc, print_page)\
        std_device_full_body_type(dtype, &procs, dname, &st_device_printer,\
          (int)((float)(w10) * (xdpi) / 10 + 0.5),\
          (int)((float)(h10) * (ydpi) / 10 + 0.5),\
          xdpi, ydpi,\
          ncomp, depth, mg, mc, dg, dc,\
          (float)(-(lo) * (xdpi)), (float)(-(to) * (ydpi)),\
          (float)((lm) * 72.0), (float)((bm) * 72.0),\
          (float)((rm) * 72.0), (float)((tm) * 72.0)\
        ),\
        prn_device_body_rest_(print_page)
#define prn_device_margins_stype_body(dtype, procs, dname, stype, w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, ncomp, depth, mg, mc, dg, dc, print_page)\
        std_device_full_body_type(dtype, &procs, dname, stype,\
          (int)((float)(w10) * (xdpi) / 10 + 0.5),\
          (int)((float)(h10) * (ydpi) / 10 + 0.5),\
          xdpi, ydpi,\
          ncomp, depth, mg, mc, dg, dc,\
          (float)(-(lo) * (xdpi)), (float)(-(to) * (ydpi)),\
          (float)((lm) * 72.0), (float)((bm) * 72.0),\
          (float)((rm) * 72.0), (float)((tm) * 72.0)\
        ),\
        prn_device_body_rest_(print_page)

#define prn_device_body(dtype, procs, dname, w10, h10, xdpi, ydpi, lm, bm, rm, tm, ncomp, depth, mg, mc, dg, dc, print_page)\
  prn_device_margins_body(dtype, procs, dname, w10, h10, xdpi, ydpi,\
    lm, tm, lm, bm, rm, tm, ncomp, depth, mg, mc, dg, dc, print_page)
#define prn_device_stype_body(dtype, procs, dname, stype, w10, h10, xdpi, ydpi, lm, bm, rm, tm, ncomp, depth, mg, mc, dg, dc, print_page)\
  prn_device_margins_stype_body(dtype, procs, dname, stype, w10, h10, xdpi, ydpi,\
    lm, tm, lm, bm, rm, tm, ncomp, depth, mg, mc, dg, dc, print_page)

#define prn_device_margins_body_extended(dtype, procs, dname, w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, mcomp, ncomp, pol, depth, gi, mg, mc, dg, dc, ef, cn, print_page)\
        std_device_full_body_type_extended(dtype, &procs, dname, &st_device_printer,\
          (int)((long)(w10) * (xdpi) / 10),\
          (int)((long)(h10) * (ydpi) / 10),\
          xdpi, ydpi,\
          mcomp, ncomp, pol, depth, gi, mg, mc, dg, dc, ef, cn,\
          -(lo) * (xdpi), -(to) * (ydpi),\
          (lm) * 72.0, (bm) * 72.0,\
          (rm) * 72.0, (tm) * 72.0\
        ),\
        prn_device_body_rest_(print_page)

#define prn_device_body_extended(dtype, procs, dname, w10, h10, xdpi, ydpi, lm, bm, rm, tm, mcomp, ncomp, pol, depth, gi, mg, mc, dg, dc, ef, cn, print_page)\
  prn_device_margins_body_extended(dtype, procs, dname, w10, h10, xdpi, ydpi,\
    lm, tm, lm, bm, rm, tm, mcomp, ncomp, pol, depth, gi, mg, mc, dg, dc, ef, cn, print_page)

#define prn_device_std_margins_body(dtype, procs, dname, w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, color_bits, print_page)\
        std_device_std_color_full_body_type(dtype, &procs, dname, &st_device_printer,\
          (int)((float)(w10) * (xdpi) / 10 + 0.5),\
          (int)((float)(h10) * (ydpi) / 10 + 0.5),\
          xdpi, ydpi, color_bits,\
          (float)(-(lo) * (xdpi)), (float)(-(to) * (ydpi)),\
          (float)((lm) * 72.0), (float)((bm) * 72.0),\
          (float)((rm) * 72.0), (float)((tm) * 72.0)\
        ),\
        prn_device_body_rest_(print_page)

#define prn_device_std_body(dtype, procs, dname, w10, h10, xdpi, ydpi, lm, bm, rm, tm, color_bits, print_page)\
  prn_device_std_margins_body(dtype, procs, dname, w10, h10, xdpi, ydpi,\
    lm, tm, lm, bm, rm, tm, color_bits, print_page)

#define prn_device_std_margins_body_copies(dtype, procs, dname, w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, color_bits, print_page_copies)\
        std_device_std_color_full_body_type(dtype, &procs, dname, &st_device_printer,\
          (int)((float)(w10) * (xdpi) / 10 + 0.5),\
          (int)((float)(h10) * (ydpi) / 10 + 0.5),\
          xdpi, ydpi, color_bits,\
          (float)(-(lo) * (xdpi)), (float)(-(to) * (ydpi)),\
          (float)((lm) * 72.0), (float)((bm) * 72.0),\
          (float)((rm) * 72.0), (float)((tm) * 72.0)\
        ),\
        prn_device_body_copies_rest_(print_page_copies)

#define prn_device_std_body_copies(dtype, procs, dname, w10, h10, xdpi, ydpi, lm, bm, rm, tm, color_bits, print_page_copies)\
  prn_device_std_margins_body_copies(dtype, procs, dname, w10, h10, xdpi, ydpi,\
    lm, tm, lm, bm, rm, tm, color_bits, print_page_copies)

     /* Note that the following macros add { } around the data. */

#define prn_device_margins(procs, dname, w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, color_bits, print_page)\
{ prn_device_std_margins_body(gx_device_printer, procs, dname,\
    w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, color_bits, print_page)\
}

#define prn_device(procs, dname, w10, h10, xdpi, ydpi, lm, bm, rm, tm, color_bits, print_page)\
  prn_device_margins(procs, dname, w10, h10, xdpi, ydpi,\
    lm, tm, lm, bm, rm, tm, color_bits, print_page)

#define prn_device_margins_copies(procs, dname, w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, color_bits, print_page_copies)\
{ prn_device_std_margins_body_copies(gx_device_printer, procs, dname,\
    w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, color_bits, print_page_copies)\
}

#define prn_device_copies(procs, dname, w10, h10, xdpi, ydpi, lm, bm, rm, tm, color_bits, print_page_copies)\
  prn_device_margins_copies(procs, dname, w10, h10, xdpi, ydpi,\
    lm, tm, lm, bm, rm, tm, color_bits, print_page_copies)

/* ------ Utilities ------ */
/* These are defined in gdevprn.c. */

/*
 * Open the printer's output file if necessary.
 */
/* VMS limits procedure names to 31 characters. */
int gdev_prn_open_printer_seekable(gx_device *dev, bool binary_mode,
                                   bool seekable);
/* BACKWARD COMPATIBILITY */
#define gdev_prn_open_printer_positionable gdev_prn_open_printer_seekable
/* open_printer defaults positionable = false */
int gdev_prn_open_printer(gx_device * dev, bool binary_mode);
/*
 * Test whether the printer's output file was just opened, i.e., whether
 * this is the first page being written to this file.  This is only valid
 * at the entry to a driver's print_page procedure.
 */
bool gdev_prn_file_is_new(const gx_device_printer *pdev);

/*
 * Determine the number of bytes required for one scan line of output to
 * a printer device, without any padding.
 */
#define gdev_prn_raster(pdev) gx_device_raster((gx_device *)(pdev), 0)

#define gdev_prn_raster_chunky(pdev) gx_device_raster_chunky((gx_device *)(pdev), 0)

/*
 * Determine (conservatively) what colors are used in a given range of scan
 * lines, and return the actual range of scan lines to which the result
 * applies.  (Currently, the result will always actually apply to one or
 * more full bands.)  In the non-banded case, this information is currently
 * not available, so we return all-1s (i.e., any color may appear) as the
 * 'or', and the entire page as the range; we may improve this someday.
 *
 * The return value is like get_band: the first Y value of the actual range
 * is stored in *range_start, and the height of the range is returned.
 * If the parameters are invalid, the procedure returns -1.
 */
int gdev_prn_color_usage(gx_device *dev, int y, int height,
                         gx_color_usage_t *color_usage,
                         int *range_start);
/*
 * Determine the colors used in a saved page.  We still need the device
 * in order to know the total page height. Saved pages are always
 * clist, so we will get this using clist_read_color_usage_array.
 */
int gx_page_info_color_usage(const gx_device *dev,
                             const gx_band_page_info_t *page_info,
                             int y, int height,
                             gx_color_usage_t *color_usage,
                             int *range_start);

/*
 * Render a subrectangle of the page into a target device provided by the
 * caller, optionally clearing the device before rendering.  The rectangle
 * need not coincide exactly with band boundaries, but it will be most
 * efficient if they do, since it is necessary to render every band that
 * even partly falls within the rectangle.  This is the lowest-level
 * rendering procedure: the other procedures for reading rasterized lines,
 * defined below, ultimately call this one.
 *
 * render_plane is used only for internal cache bookkeeping: it doesn't
 * affect what is passed to the buffer device.  It is the client's
 * responsibility to set up a plane extraction device, if required, to
 * select an individual plane for rendering.
 *
 * Note that it may be necessary to render more than one plane even if the
 * caller only requests a single plane.  Currently this only occurs for
 * non-trivial RasterOps on CMYK devices.  If this is the case, it is the
 * client's responsibility to provide a target device that accumulates full
 * pixels rather than a single plane: if the plane extraction device is
 * asked to execute an operation that requires full pixels, it will return
 * an error.
 */
int gdev_prn_render_rectangle(gx_device_printer *pdev,
                              const gs_int_rect *prect,
                              gx_device *target,
                              const gx_render_plane_t *render_plane,
                              bool clear);

/*
 * Read one or more rasterized scan lines for printing.
 * The procedure either copies the scan lines into the buffer and
 * sets *actual_buffer = buffer and *actual_bytes_per_line = bytes_per_line,
 * or sets *actual_buffer and *actual_bytes_per_line to reference
 * already-rasterized scan lines.
 *
 * For non-banded devices, copying is never necessary.  For banded devices,
 * if the client's buffer is at least as large as the single preallocated
 * one (if any), the band will be rasterized directly into the client's
 * buffer, again avoiding copying.  Alternatively, if there is a
 * preallocated buffer and the call asks for no more data than will fit
 * in that buffer, buffer may be NULL: any necessary rasterizing will
 * occur in the preallocated buffer, and a pointer into the buffer will be
 * returned.
 */
int gdev_prn_get_lines(gx_device_printer *pdev, int y, int height,
                       byte *buffer, uint bytes_per_line,
                       byte **actual_buffer, uint *actual_bytes_per_line,
                       const gx_render_plane_t *render_plane);

/*
 * Read a rasterized scan line for sending to the printer.
 * This is essentially a simplified form of gdev_prn_get_lines,
 * except that it also calls gdev_prn_clear_trailing_bits.
 */
int gdev_prn_get_bits(gx_device_printer *pdev, int y, byte *buffer,
                      byte **actual_buffer);

/*
 * Copy scan lines to send to the printer.  This is now DEPRECATED,
 * because it copies the data even if the data are already available in
 * the right form in the rasterizer buffer.  Use gdev_prn_get_bits
 * instead.  For documentation, see the implementation in gdevprn.c.
 */
int gdev_prn_copy_scan_lines(gx_device_printer *, int, byte *, uint);

/*
 * Clear any trailing bits in the last byte of one or more scan lines
 * returned from the calls for reading out rasterized data.  Note that
 * the device is only used to get the width and depth, and need not be
 * a printer device.
 */
void gdev_prn_clear_trailing_bits(byte *data, uint raster, int height,
                                  const gx_device *dev);

/*
 * Close the printer's output file.
 */
int gdev_prn_close_printer(gx_device *);

/* Print a single copy of a page by calling print_page_copies. */
prn_dev_proc_print_page(gx_print_page_single_copy);

/*
 * Define a default print_page_copies procedure just calls print_page
 * the given number of times.
 */
prn_dev_proc_print_page_copies(gx_default_print_page_copies);

/*
 * Determine the number of scan lines that should actually be passed
 * to the device.
 */
int gdev_prn_print_scan_lines(gx_device *);

/* Allocate / reallocate / free printer memory. */
int gdev_prn_allocate_memory(gx_device *pdev,
                             gdev_prn_space_params *space,
                             int new_width, int new_height);
int gdev_prn_reallocate_memory(gx_device *pdev,
                               gdev_prn_space_params *space,
                               int new_width, int new_height);
int gdev_prn_free_memory(gx_device *pdev);

/*
 * Create the buffer device for a printer device.  Clients should always
 * call this, and never call the device procedure directly.  The actual
 * create_buf_device procedure is passed as an argument because the banding
 * code needs this: normally it is dev_proc(some_dev, create_buf_device).
 */
typedef dev_proc_create_buf_device((*create_buf_device_proc_t));
int gdev_create_buf_device(create_buf_device_proc_t cbd_proc,
                           gx_device **pbdev, gx_device *target, int y,
                           const gx_render_plane_t *render_plane,
                           gs_memory_t *mem, gx_color_usage_t *color_usage);

/* BACKWARD COMPATIBILITY */
#define dev_print_scan_lines(dev)\
  gdev_prn_print_scan_lines((gx_device *)(dev))
#define gdev_mem_bytes_per_scan_line(dev)\
  gdev_prn_raster((gx_device_printer *)(dev))
#define gdev_prn_transpose_8x8(inp,ils,outp,ols)\
  memflip8x8(inp,ils,outp,ols)

/* ------ Printer device types ------ */
/**************** THE FOLLOWING CODE IS NOT USED YET. ****************/

#if 0				/**************** VMS linker gets upset *************** */
#endif
int gdev_prn_initialize(gx_device *, const char *, dev_proc_print_page((*)));
void gdev_prn_init_color(gx_device *, int, dev_proc_map_rgb_color((*)), dev_proc_map_color_rgb((*)));

#define prn_device_type(dtname, initproc, pageproc)\
static dev_proc_print_page(pageproc);\
device_type(dtname, st_prn_device, initproc)

#define prn_device_type_mono(dtname, dname, initproc, pageproc)\
static dev_proc_print_page(pageproc);\
static int \
initproc(gx_device *dev)\
{	return gdev_prn_initialize(dev, dname, pageproc);\
}\
device_type(dtname, st_prn_device, initproc)

#define prn_device_type_color(dtname, dname, depth, initproc, pageproc, rcproc, crproc)\
static dev_proc_print_page(pageproc);\
static int \
initproc(gx_device *dev)\
{	int code = gdev_prn_initialize(dev, dname, pageproc);\
        gdev_prn_init_color(dev, depth, rcproc, crproc);\
        return code;\
}\
device_type(dtname, st_prn_device, initproc)

#endif /* gdevprn_INCLUDED */
