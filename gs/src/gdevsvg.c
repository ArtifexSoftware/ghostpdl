/* Copyright (C) 2007 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id: $ */
/* SVG (Scalable Vector Graphics) output device */

#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gdevvec.h"
#include "stream.h"

/* SVG data constants */

#define XML_DECL    "<?xml version=\"1.0\" standalone=\"no\"?>"
#define SVG_DOCTYPE "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \n\
         \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">"
#define SVG_XMLNS   "http://www.w3.org/2000/svg"
#define SVG_VERSION "1.1"

/* default resolution. */
#ifndef X_DPI
#  define X_DPI 300
#endif
#ifndef Y_DPI
#  define Y_DPI 300
#endif

/* ---------------- Device definition ---------------- */

typedef struct gx_device_svg_s {
    /* superclass state */
    gx_device_vector_common;
    /* local state*/
    int header;
    int mark;
    int page_count;
    char *strokecolor, *fillcolor;
} gx_device_svg;

#define svg_device_body(dname, depth)\
  std_device_dci_type_body(gx_device_svg, 0, dname, &st_device_svg, \
			   DEFAULT_WIDTH_10THS * X_DPI / 10, \
			   DEFAULT_HEIGHT_10THS * Y_DPI / 10, \
			   X_DPI, Y_DPI, \
			   (depth > 8 ? 3 : 1), depth, \
			   (depth > 1 ? 255 : 1), (depth > 8 ? 255 : 0), \
			   (depth > 1 ? 256 : 2), (depth > 8 ? 256 : 1))

static dev_proc_open_device(svg_open_device);
static dev_proc_output_page(svg_output_page);
static dev_proc_close_device(svg_close_device);

static dev_proc_get_params(svg_get_params);
static dev_proc_put_params(svg_put_params);

#define svg_device_procs \
{ \
	svg_open_device, \
        NULL,                   /* get_initial_matrix */\
        NULL,                   /* sync_output */\
        svg_output_page,\
        svg_close_device,\
        gx_default_rgb_map_rgb_color,\
        gx_default_rgb_map_color_rgb,\
        gdev_vector_fill_rectangle,\
        NULL,                   /* tile_rectangle */\
        NULL,			/* copy_mono */\
        NULL,			/* copy_color */\
        NULL,                   /* draw_line */\
        NULL,                   /* get_bits */\
        svg_get_params,\
        svg_put_params,\
        NULL,                   /* map_cmyk_color */\
        NULL,                   /* get_xfont_procs */\
        NULL,                   /* get_xfont_device */\
        NULL,                   /* map_rgb_alpha_color */\
        gx_page_device_get_page_device,\
        NULL,                   /* get_alpha_bits */\
        NULL,                   /* copy_alpha */\
        NULL,                   /* get_band */\
        NULL,                   /* copy_rop */\
        gdev_vector_fill_path,\
        gdev_vector_stroke_path,\
        NULL,			/* fill_mask */\
        gdev_vector_fill_trapezoid,\
        gdev_vector_fill_parallelogram,\
        gdev_vector_fill_triangle,\
        NULL,			/* draw_thin_line */\
        NULL,			/* begin_image */\
        NULL,                   /* image_data */\
        NULL,                   /* end_image */\
        NULL,                   /* strip_tile_rectangle */\
        NULL			/* strip_copy_rop */\
}

gs_public_st_suffix_add0_final(st_device_svg, gx_device_svg,
                               "gx_device_svg",
                               device_svg_enum_ptrs, 
			       device_svg_reloc_ptrs,
                               gx_device_finalize, 
			       st_device_vector);

const gx_device_svg gs_svg_device = {
    svg_device_body("svg", 24),
    svg_device_procs
};

/* Vector device procedures */

static int
svg_beginpage(gx_device_vector *vdev);
static int
svg_setlinewidth(gx_device_vector *vdev, floatp width);
static int
svg_setlinecap(gx_device_vector *vdev, gs_line_cap cap);
static int
svg_setlinejoin(gx_device_vector *vdev, gs_line_join join);
static int
svg_setmiterlimit(gx_device_vector *vdev, floatp limit);
static int
svg_setdash(gx_device_vector *vdev, const float *pattern,
	    uint count, floatp offset);
static int
svg_setlogop(gx_device_vector *vdev, gs_logical_operation_t lop,
	     gs_logical_operation_t diff);

static int 
svg_can_handle_hl_color(gx_device_vector *vdev, const gs_imager_state *pis,
			const gx_drawing_color * pdc);
static int
svg_setfillcolor(gx_device_vector *vdev, const gs_imager_state *pis,
                 const gx_drawing_color *pdc);
static int
svg_setstrokecolor(gx_device_vector *vdev, const gs_imager_state *pis,
		   const gx_drawing_color *pdc);

static int
svg_dorect(gx_device_vector *vdev, fixed x0, fixed y0,
	   fixed x1, fixed y1, gx_path_type_t type);
static int
svg_beginpath(gx_device_vector *vdev, gx_path_type_t type);

static int
svg_moveto(gx_device_vector *vdev, floatp x0, floatp y0,
	   floatp x, floatp y, gx_path_type_t type);
static int
svg_lineto(gx_device_vector *vdev, floatp x0, floatp y0,
	   floatp x, floatp y, gx_path_type_t type);
static int
svg_curveto(gx_device_vector *vdev, floatp x0, floatp y0,
	    floatp x1, floatp y1, floatp x2, floatp y2,
	    floatp x3, floatp y3, gx_path_type_t type);
static int
svg_closepath(gx_device_vector *vdev, floatp x, floatp y,
	      floatp x_start, floatp y_start, gx_path_type_t type);
static int
svg_endpath(gx_device_vector *vdev, gx_path_type_t type);

/* Vector device function table */

static const gx_device_vector_procs svg_vector_procs = {
        /* Page management */
    svg_beginpage,
        /* Imager state */
    svg_setlinewidth,
    svg_setlinecap,
    svg_setlinejoin,
    svg_setmiterlimit,
    svg_setdash,
    gdev_vector_setflat,
    svg_setlogop,
        /* Other state */
    svg_can_handle_hl_color,
    svg_setfillcolor,
    svg_setstrokecolor,
        /* Paths */
    gdev_vector_dopath,
    svg_dorect,
    svg_beginpath,
    svg_moveto,
    svg_lineto,
    svg_curveto,
    svg_closepath,
    svg_endpath
};

/* local utility prototypes */

static int svg_write_bytes(gx_device_svg *svg, 
		const char *string, uint length);
static int svg_write(gx_device_svg *svg, const char *string);

static int svg_write_header(gx_device_svg *svg);

/* Driver procedure implementation */

/* Open the device */
static int
svg_open_device(gx_device *dev)
{
    gx_device_vector *const vdev = (gx_device_vector*)dev;
    gx_device_svg *const svg = (gx_device_svg*)dev;
    int code = 0;

    vdev->v_memory = dev->memory;
    vdev->vec_procs = &svg_vector_procs;
    gdev_vector_init(vdev);
    code = gdev_vector_open_file_options(vdev, 512,
	VECTOR_OPEN_FILE_SEQUENTIAL);
    if (code < 0) return code;

    /* svg-specific initialization goes here */
    svg->header = 0;	/* file header hasn't been written */
    svg->mark = 0;	/* have we drawn something? */
    svg->page_count = 0;/* how many output_page calls we've seen */
    svg->strokecolor = NULL; 
    svg->fillcolor = NULL;

    return code;
}

/* Complete a page */
static int
svg_output_page(gx_device *dev, int num_copies, int flush)
{
    gx_device_svg *const svg = (gx_device_svg*)dev;

    svg->page_count++;
    
    svg_write(svg, "\n<!-- svg_output_page -->\n");
    if (ferror(svg->file)) return_error(gs_error_ioerror);

    return gx_finish_output_page(dev, num_copies, flush);
}

/* Close the device */
static int
svg_close_device(gx_device *dev)
{
    gx_device_svg *const svg = (gx_device_svg*)dev;

    svg_write(svg, "\n<!-- svg_close_device -->\n");
    if (svg->mark) {
      svg_write(svg, "</g>\n");
      svg->mark = 0;
    }
    if (svg->fillcolor) gs_free_string(svg->memory, svg->fillcolor, 8,
	"svg_close_device");
    if (svg->strokecolor) gs_free_string(svg->memory, svg->strokecolor, 8,
	"svg_close_device");
    svg_write(svg, "</g>\n");
    if (svg->header) {
      svg_write(svg, "</svg>\n");
      svg->header = 0;
    }

    if (ferror(svg->file)) return_error(gs_error_ioerror);

    return gdev_vector_close_file((gx_device_vector*)dev);
}

/* Respond to a device parameter query from the client */
static int
svg_get_params(gx_device *dev, gs_param_list *plist)
{
    int code = 0;

    dprintf("svg_get_params\n");

    /* call our superclass to add its standard set */
    code = gdev_vector_get_params(dev, plist);
    if (code < 0) return code;

    /* svg specific parameters are added to plist here */

    return code;
}

/* Read the device parameters passed to us by the client */
static int
svg_put_params(gx_device *dev, gs_param_list *plist)
{
    int code = 0;

    dprintf("svg_put_params\n");

    /* svg specific parameters are parsed here */

    /* call our superclass to get its parameters, like OutputFile */
    code = gdev_vector_put_params(dev, plist);
    if (code < 0) return code;

    return code;
}

/* write a length-limited char buffer */
static int
svg_write_bytes(gx_device_svg *svg, const char *string, uint length)
{
    /* calling the accessor ensures beginpage is called */
    stream *s = gdev_vector_stream((gx_device_vector*)svg);
    uint used;

    sputs(s, (const byte *)string, length, &used);

    return !(length == used);
}

/* write a null terminated string */
static int
svg_write(gx_device_svg *svg, const char *string)
{
    return svg_write_bytes(svg, string, strlen(string));
}

static int
svg_write_header(gx_device_svg *svg)
{
    /* we're called from beginpage, so we can't use 
       svg_write() which calls gdev_vector_stream()
       which calls beginpage! */
    stream *s = svg->strm;
    uint used;
    char line[300];

    dprintf("svg_write_header\n");

    /* only write the header once */
    if (svg->header) return 1;

    /* write the initial boilerplate */
    sprintf(line, "%s\n", XML_DECL);
    /* svg_write(svg, line); */
    sputs(s, line, strlen(line), &used);
    sprintf(line, "%s\n", SVG_DOCTYPE);
    /* svg_write(svg, line); */
    sputs(s, line, strlen(line), &used);
    sprintf(line, "<svg xmlns='%s' version='%s'>\n",
	SVG_XMLNS, SVG_VERSION);
    /* svg_write(svg, line); */
    sputs(s, line, strlen(line), &used);

    /* svg_write(svg, "<g transform='scale(0.3,0.3)'>\n"); */
    /* svg_write(svg, line); */
    sprintf(line, "<g transform='scale(0.3,0.3)'>\n");
    sputs(s, line, strlen(line), &used);

    /* mark that we've been called */
    svg->header = 1;

    return 0;
}

static const char *
svg_make_color(gx_device_svg *svg, gx_drawing_color *pdc)
{
    char *paint = gs_alloc_string(svg->memory, 8, "svg_make_color");

    if (!paint) {
      gs_note_error(gs_error_VMerror);
      return NULL;
    }

    if (gx_dc_is_pure(pdc)) {
      gx_color_index color = gx_dc_pure_color(pdc);
      sprintf(paint, "#%06x", color & 0xffffff);
    } else if (gx_dc_is_null(pdc)) {
      sprintf(paint, "None");
    } else {
      gs_free_string(svg->memory, paint, 8, "svg_make_color");
      gs_note_error(gs_error_rangecheck);
      return NULL;
    }

    return paint;
}

/* vector device implementation */

        /* Page management */
static int
svg_beginpage(gx_device_vector *vdev)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;

    svg_write_header(svg);

    dprintf1("svg_beginpage (page count %d)\n", svg->page_count);
    return 0;
}

        /* Imager state */
static int
svg_setlinewidth(gx_device_vector *vdev, floatp width)
{
    dprintf1("svg_setlinewidth(%lf)\n", width);
    return 0;
}
static int
svg_setlinecap(gx_device_vector *vdev, gs_line_cap cap)
{
    dprintf("svg_setlinecap\n");
    return 0;
}
static int
svg_setlinejoin(gx_device_vector *vdev, gs_line_join join)
{
    dprintf("svg_setlinejoin\n");
    return 0;
}
static int
svg_setmiterlimit(gx_device_vector *vdev, floatp limit)
{
    dprintf1("svg_setmiterlimit(%lf)\n", limit);
    return 0;
}
static int
svg_setdash(gx_device_vector *vdev, const float *pattern,
	    uint count, floatp offset)
{
    dprintf("svg_setdash\n");
    return 0;
}
static int
svg_setlogop(gx_device_vector *vdev, gs_logical_operation_t lop,
	     gs_logical_operation_t diff)
{
    dprintf("svg_setlogop\n");
    return 0;
}

        /* Other state */

static int 
svg_can_handle_hl_color(gx_device_vector *vdev, const gs_imager_state *pis,
			  const gx_drawing_color * pdc)
{
    dprintf("svg_can_handle_hl_color\n");
    return 0;
}

static int
svg_setfillcolor(gx_device_vector *vdev, const gs_imager_state *pis,
		 const gx_drawing_color *pdc)
{
    gx_device_svg *svg = (gx_device_svg*)vdev;
    char *fill;

    dprintf("svg_setfillcolor\n");

    fill = svg_make_color(svg, pdc);
    if (!fill) return 1;
    if (svg->fillcolor && !strcmp(fill, svg->fillcolor)) 
      return 0; /* not a new color */

    /* update and write a new group */
    if (svg->fillcolor) gs_free_string(svg->memory, svg->fillcolor, 8, 
	"svg_setfillcolor");
    svg->fillcolor = fill;
    if (svg->mark) {
      svg_write(svg, "</g>\n");
    }
    svg_write(svg, "<g fill='");
    svg_write(svg, fill);
    svg_write(svg, "'");
    if (svg->strokecolor) {
      svg_write(svg, " stroke='");
      svg_write(svg, svg->strokecolor);
      svg_write(svg, "'");
    }
    svg_write(svg, ">\n");
    svg->mark = 1;

    return 0;
}

static int
svg_setstrokecolor(gx_device_vector *vdev, const gs_imager_state *pis,
		   const gx_drawing_color *pdc)
{
    gx_device_svg *svg = (gx_device_svg*)vdev;
    char *stroke;

    dprintf("svg_setstrokecolor\n");

    stroke = svg_make_color(svg, pdc);
    if (!stroke) return 1;
    if (svg->strokecolor && !strcmp(stroke, svg->strokecolor)) 
      return 0; /* not a new color */

    /* update and write a new group */
    if (svg->strokecolor) gs_free_string(svg->memory, svg->strokecolor, 8,
	"svg_setstrokecolor");
    svg->strokecolor = stroke;
    if (svg->mark) {
      svg_write(svg, "</g>\n");
    }
    svg_write(svg, "<g ");
    if (svg->fillcolor) {
      svg_write(svg, " fill='");
      svg_write(svg, svg->fillcolor);
      svg_write(svg, "'");
    }
    svg_write(svg, " stroke='");
    svg_write(svg, stroke);
    svg_write(svg, "'");
    svg_write(svg, ">\n");
    svg->mark = 1;

    return 0;
}

	/* Paths */
/*    gdev_vector_dopath */

static int
svg_dorect(gx_device_vector *vdev, fixed x0, fixed y0,
	   fixed x1, fixed y1, gx_path_type_t type)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;
    char line[300];

    if (svg->page_count) return 0; /* hack single-page output */
    
    dprintf1("svg_dorect (type %d)\n", type);
#if 0 /* dorect seems to be a duplicate? */
    sprintf(line, "<rect x='%lf' y='%lf' width='%lf' height='%lf'/>\n",
	fixed2float(x0), fixed2float(y0),
	fixed2float(x1 - x0), fixed2float(y1 - y0));
    svg_write(svg, line);
#endif    
    return 0;
}

static int
svg_beginpath(gx_device_vector *vdev, gx_path_type_t type)    
{
    gx_device_svg *svg = (gx_device_svg *)vdev;

    if (svg->page_count) return 0; /* hack single-page output */
    
    dprintf("svg_beginpath\n");
    svg_write(svg, "<path d='");

    return 0;
}

static int
svg_moveto(gx_device_vector *vdev, floatp x0, floatp y0,
	   floatp x, floatp y, gx_path_type_t type)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;
    char line[100];

    if (svg->page_count) return 0; /* hack single-page output */
    
    dprintf4("svg_moveto(%lf,%lf,%lf,%lf)\n", x0, y0, x, y);

    sprintf(line, " M%lf,%lf", x, y);
    svg_write(svg, line);

    return 0;
}

static int
svg_lineto(gx_device_vector *vdev, floatp x0, floatp y0,
	   floatp x, floatp y, gx_path_type_t type)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;
    char line[100];

    if (svg->page_count) return 0; /* hack single-page output */
    
    dprintf4("svg_lineto(%lf,%lf,%lf,%lf)\n", x0,y0, x,y);

    sprintf(line, " L%lf,%lf", x, y);
    svg_write(svg, line);

    return 0;
}

static int
svg_curveto(gx_device_vector *vdev, floatp x0, floatp y0,
	    floatp x1, floatp y1, floatp x2, floatp y2,
	    floatp x3, floatp y3, gx_path_type_t type)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;
    char line[100];

    if (svg->page_count) return 0; /* hack single-page output */
    
    dprintf8("svg_curveto(%lf,%lf, %lf,%lf, %lf,%lf, %lf,%lf)\n",
	x0,y0, x1,y1, x2,y2, x3,y3);

    sprintf(line, " C%lf,%lf %lf,%lf %lf,%lf", x1,y2, x2,y2, x3,y3);
    svg_write(svg, line);

    return 0;
}

static int
svg_closepath(gx_device_vector *vdev, floatp x, floatp y,
	      floatp x_start, floatp y_start, gx_path_type_t type)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;

    if (svg->page_count) return 0; /* hack single-page output */
    
    dprintf("svg_closepath\n");

    svg_write(svg, " z");

    return 0;
}

static int
svg_endpath(gx_device_vector *vdev, gx_path_type_t type)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;

    if (svg->page_count) return 0; /* hack single-page output */
    
    dprintf("svg_endpath\n");

    svg_write(svg, "'/>\n");

    return 0;
}

