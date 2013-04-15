/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


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

/* internal line buffer */
#define SVG_LINESIZE 100

/* default constants */
#define SVG_DEFAULT_LINEWIDTH	1.0
#define SVG_DEFAULT_LINECAP	gs_cap_butt
#define SVG_DEFAULT_LINEJOIN	gs_join_miter
#define SVG_DEFAULT_MITERLIMIT	4.0

/* ---------------- Device definition ---------------- */

typedef struct gx_device_svg_s {
    /* superclass state */
    gx_device_vector_common;
    /* local state */
    int header;		/* whether we've written the file header */
    int dirty;		/* whether we need to rewrite the <g> element */
    int mark;		/* <g> nesting level */
    int page_count;	/* how many output_page calls we've seen */
    gx_color_index strokecolor, fillcolor;
    double linewidth;
    gs_line_cap linecap;
    gs_line_join linejoin;
    double miterlimit;
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
                               device_svg_enum_ptrs, device_svg_reloc_ptrs,
                               gx_device_finalize, st_device_vector);

/* The output device is named 'svg' but we're referred to as the
   'svgwrite' device by the build system to avoid conflicts with
   the svg interpreter */
const gx_device_svg gs_svgwrite_device = {
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
    if (code < 0)
      return gs_rethrow_code(code);

    /* svg-specific initialization goes here */
    svg->header = 0;
    svg->dirty = 0;
    svg->mark = 0;
    svg->page_count = 0;
    svg->strokecolor = gx_no_color_index;
    svg->fillcolor = gx_no_color_index;
    /* these should be the graphics library defaults instead? */
    svg->linewidth = SVG_DEFAULT_LINEWIDTH;
    svg->linecap = SVG_DEFAULT_LINECAP;
    svg->linejoin = SVG_DEFAULT_LINEJOIN;
    svg->miterlimit = SVG_DEFAULT_MITERLIMIT;

    return code;
}

/* Complete a page */
static int
svg_output_page(gx_device *dev, int num_copies, int flush)
{
    gx_device_svg *const svg = (gx_device_svg*)dev;
    int code;

    svg->page_count++;

    svg_write(svg, "\n<!-- svg_output_page -->\n");
    if (ferror(svg->file))
      return gs_throw_code(gs_error_ioerror);

    if ((code=gx_finish_output_page(dev, num_copies, flush)) < 0)
        return code;
    /* Check if we need to change the output file for separate pages */
    if (gx_outputfile_is_separate_pages(((gx_device_vector *)dev)->fname, dev->memory)) {
        if ((code = svg_close_device(dev)) < 0)
            return code;
        code = svg_open_device(dev);
    }
    return code;
}

/* Close the device */
static int
svg_close_device(gx_device *dev)
{
    gx_device_svg *const svg = (gx_device_svg*)dev;

    svg_write(svg, "\n<!-- svg_close_device -->\n");
    /* close any open group elements */
    while (svg->mark > 0) {
      svg_write(svg, "</g>\n");
      svg->mark--;
    }
    if (svg->header) {
      svg_write(svg, "</svg>\n");
      svg->header = 0;
    }

    if (ferror(svg->file))
      return gs_throw_code(gs_error_ioerror);

    return gdev_vector_close_file((gx_device_vector*)dev);
}

/* Respond to a device parameter query from the client */
static int
svg_get_params(gx_device *dev, gs_param_list *plist)
{
    int code = 0;

    if_debug0m('_', dev->memory, "svg_get_params\n");

    /* call our superclass to add its standard set */
    code = gdev_vector_get_params(dev, plist);
    if (code < 0)
      return gs_rethrow_code(code);

    /* svg specific parameters are added to plist here */

    return code;
}

/* Read the device parameters passed to us by the client */
static int
svg_put_params(gx_device *dev, gs_param_list *plist)
{
    int code = 0;

    if_debug0m('_', dev->memory, "svg_put_params\n");

    /* svg specific parameters are parsed here */

    /* call our superclass to get its parameters, like OutputFile */
    code = gdev_vector_put_params(dev, plist);
    if (code < 0)
      return gs_rethrow_code(code);

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

    if_debug0m('_', svg->memory, "svg_write_header\n");

    /* only write the header once */
    if (svg->header)
      return 1;

    /* write the initial boilerplate */
    gs_sprintf(line, "%s\n", XML_DECL);
    /* svg_write(svg, line); */
    sputs(s, (byte *)line, strlen(line), &used);
    gs_sprintf(line, "%s\n", SVG_DOCTYPE);
    /* svg_write(svg, line); */
    sputs(s, (byte *)line, strlen(line), &used);
    gs_sprintf(line, "<svg xmlns='%s' version='%s'",
        SVG_XMLNS, SVG_VERSION);
    /* svg_write(svg, line); */
    sputs(s, (byte *)line, strlen(line), &used);
    gs_sprintf(line, "\n\twidth='%dpt' height='%dpt'>\n",
        (int)svg->MediaSize[0], (int)svg->MediaSize[1]);
    sputs(s, (byte *)line, strlen(line), &used);

    /* Scale drawing so our coordinates are in pixels */
    gs_sprintf(line, "<g transform='scale(%lf,%lf)'>\n",
        72.0 / svg->HWResolution[0],
        72.0 / svg->HWResolution[1]);
    /* svg_write(svg, line); */
    sputs(s, (byte *)line, strlen(line), &used);
    svg->mark++;

    /* mark that we've been called */
    svg->header = 1;

    return 0;
}

static gx_color_index
svg_get_color(gx_device_svg *svg, const gx_drawing_color *pdc)
{

    gx_color_index color = gx_no_color_index;

    if (gx_dc_is_pure(pdc))
	color = gx_dc_pure_color(pdc);
    return color;
}

static int
svg_write_state(gx_device_svg *svg)
{
    char line[SVG_LINESIZE];

    /* has anything changed? */
    if (!svg->dirty)
      return 0;

    /* close the current graphics state element, if any */
    if (svg->mark > 1) {
      svg_write(svg, "</g>\n");
      svg->mark--;
    }
    /* write out the new current state */
    svg_write(svg, "<g ");
    if (svg->strokecolor != gx_no_color_index) {
        gs_sprintf(line, " stroke='#%06x'", svg->strokecolor & 0xffffffL);
        svg_write(svg, line);
    } else {
        svg_write(svg, " stroke='none'");
    }
    if (svg->fillcolor != gx_no_color_index) {
        gs_sprintf(line, " fill='#%06x'", svg->fillcolor & 0xffffffL);
        svg_write(svg, line);
    } else {
      svg_write(svg, " fill='none'");
    }
    if (svg->linewidth != 1.0) {
      gs_sprintf(line, " stroke-width='%lf'", svg->linewidth);
      svg_write(svg, line);
    }
    if (svg->linecap != SVG_DEFAULT_LINECAP) {
        switch (svg->linecap) {
          case gs_cap_round:
            svg_write(svg, " stroke-linecap='round'");
            break;
          case gs_cap_square:
            svg_write(svg, " stroke-linecap='square'");
            break;
          case gs_cap_butt:
          default:
            /* treat all the other options as the default */
            svg_write(svg, " stroke-linecap='butt'");
            break;
        }
    }
    if (svg->linejoin != SVG_DEFAULT_LINEJOIN) {
        switch (svg->linejoin) {
          case gs_join_round:
            svg_write(svg, " stroke-linejoin='round'");
            break;
          case gs_join_bevel:
            svg_write(svg, " stroke-linejoin='bevel'");
            break;
          case gs_join_miter:
          default:
            /* SVG doesn't support any other variants */
            svg_write(svg, " stroke-linejoin='miter'");
            break;
        }
    }
    if (svg->miterlimit != SVG_DEFAULT_MITERLIMIT) {
        gs_sprintf(line, " stroke-miterlimit='%lf'", svg->miterlimit);
        svg_write(svg, line);
    }
    svg_write(svg, ">\n");
    svg->mark++;

    svg->dirty = 0;
    return 0;
}

/* vector device implementation */

        /* Page management */
static int
svg_beginpage(gx_device_vector *vdev)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;

    svg_write_header(svg);

    if_debug1m('_', svg->memory, "svg_beginpage (page count %d)\n", svg->page_count);
    return 0;
}

        /* Imager state */
static int
svg_setlinewidth(gx_device_vector *vdev, floatp width)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;

    if_debug1m('_', svg->memory, "svg_setlinewidth(%lf)\n", width);

    svg->linewidth = width;
    svg->dirty++;

    return 0;
}
static int
svg_setlinecap(gx_device_vector *vdev, gs_line_cap cap)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;
    const char *linecap_names[] = {"butt", "round", "square",
        "triangle", "unknown"};

    if (cap < 0 || cap > gs_cap_unknown)
        return gs_throw_code(gs_error_rangecheck);
    if_debug1m('_', svg->memory, "svg_setlinecap(%s)\n", linecap_names[cap]);

    svg->linecap = cap;
    svg->dirty++;

    return 0;
}
static int
svg_setlinejoin(gx_device_vector *vdev, gs_line_join join)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;
    const char *linejoin_names[] = {"miter", "round", "bevel",
        "none", "triangle", "unknown"};

    if (join < 0 || join > gs_join_unknown)
        return gs_throw_code(gs_error_rangecheck);
    if_debug1m('_', svg->memory, "svg_setlinejoin(%s)\n", linejoin_names[join]);

    svg->linejoin = join;
    svg->dirty++;

    return 0;
}
static int
svg_setmiterlimit(gx_device_vector *vdev, floatp limit)
{
    if_debug1m('_', vdev->memory, "svg_setmiterlimit(%lf)\n", limit);
    return 0;
}
static int
svg_setdash(gx_device_vector *vdev, const float *pattern,
            uint count, floatp offset)
{
    if_debug0m('_', vdev->memory, "svg_setdash\n");
    return 0;
}
static int
svg_setlogop(gx_device_vector *vdev, gs_logical_operation_t lop,
             gs_logical_operation_t diff)
{
    if_debug2m('_', vdev->memory, "svg_setlogop(%u,%u) set logical operation\n",
        lop, diff);
    /* SVG can fake some simpler modes, but we ignore this for now. */
    return 0;
}

        /* Other state */

static int
svg_can_handle_hl_color(gx_device_vector *vdev, const gs_imager_state *pis,
                          const gx_drawing_color * pdc)
{
    if_debug0m('_', vdev->memory, "svg_can_handle_hl_color\n");
    return 0;
}

static int
svg_setfillcolor(gx_device_vector *vdev, const gs_imager_state *pis,
                 const gx_drawing_color *pdc)
{
    gx_device_svg *svg = (gx_device_svg*)vdev;
    gx_color_index fill = svg_get_color(svg, pdc);

    if_debug0m('_', svg->memory, "svg_setfillcolor\n");

    if (svg->fillcolor == fill)
      return 0; /* not a new color */
    /* update our state with the new color */
    svg->fillcolor = fill;
    /* request a new group element */
    svg->dirty++;
    return 0;
}

static int
svg_setstrokecolor(gx_device_vector *vdev, const gs_imager_state *pis,
                   const gx_drawing_color *pdc)
{
    gx_device_svg *svg = (gx_device_svg*)vdev;
    gx_color_index stroke = svg_get_color(svg, pdc);

    if_debug0m('_', svg->memory, "svg_setstrokecolor\n");

    if (svg->strokecolor == stroke)
      return 0; /* not a new color */

    /* update our state with the new color */
    svg->strokecolor = stroke;
    /* request a new group element */
    svg->dirty++;

    return 0;
}

        /* Paths */
/*    gdev_vector_dopath */

static int svg_print_path_type(gx_device_svg *svg, gx_path_type_t type)
{
    const char *path_type_names[] = {"winding number", "fill", "stroke",
        "fill and stroke", "clip"};

    if (type <= 4)
        if_debug2m('_', svg->memory, "type %d (%s)", type, path_type_names[type]);
    else
        if_debug1m('_', svg->memory, "type %d", type);

    return 0;
}

static int
svg_dorect(gx_device_vector *vdev, fixed x0, fixed y0,
           fixed x1, fixed y1, gx_path_type_t type)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;
    char line[300];

    /* hack single-page output */
    if (svg->page_count)
      return 0;

    if_debug0m('_', svg->memory, "svg_dorect");
    svg_print_path_type(svg, type);
    if_debug0m('_', svg->memory, "\n");

    svg_write_state(svg);

    if (type & gx_path_type_clip) {
        svg_write(svg, "<clipPath>\n");
    }

    gs_sprintf(line, "<rect x='%lf' y='%lf' width='%lf' height='%lf'",
        fixed2float(x0), fixed2float(y0),
        fixed2float(x1 - x0), fixed2float(y1 - y0));
    svg_write(svg, line);
    /* override the inherited stroke attribute if we're not stroking */
    if (!(type & gx_path_type_stroke) && (svg->strokecolor != gx_no_color_index))
        svg_write(svg, " stroke='none'");
    /* override the inherited fill attribute if we're not filling */
    if (!(type & gx_path_type_fill) && (svg->fillcolor != gx_no_color_index))
        svg_write(svg, " fill='none'");
    svg_write(svg, "/>\n");

    if (type & gx_path_type_clip) {
        svg_write(svg, "</clipPath>\n");
    }

    return 0;
}

static int
svg_beginpath(gx_device_vector *vdev, gx_path_type_t type)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;

    /* hack single-page output */
    if (svg->page_count)
      return 0;

    /* skip non-drawing paths for now */
    if (!(type & gx_path_type_fill) && !(type & gx_path_type_stroke))
      return 0;

    if_debug0m('_', svg->memory, "svg_beginpath ");
    svg_print_path_type(svg, type);
    if_debug0m('_', svg->memory, "\n");

    svg_write_state(svg);
    svg_write(svg, "<path d='");

    return 0;
}

static int
svg_moveto(gx_device_vector *vdev, floatp x0, floatp y0,
           floatp x, floatp y, gx_path_type_t type)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;
    char line[SVG_LINESIZE];

    /* hack single-page output */
    if (svg->page_count)
      return 0;

    /* skip non-drawing paths for now */
    if (!(type & gx_path_type_fill) && !(type & gx_path_type_stroke))
      return 0;

    if_debug4m('_', svg->memory, "svg_moveto(%lf,%lf,%lf,%lf) ", x0, y0, x, y);
    svg_print_path_type(svg, type);
    if_debug0m('_', svg->memory, "\n");

    gs_sprintf(line, " M%lf,%lf", x, y);
    svg_write(svg, line);

    return 0;
}

static int
svg_lineto(gx_device_vector *vdev, floatp x0, floatp y0,
           floatp x, floatp y, gx_path_type_t type)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;
    char line[SVG_LINESIZE];

    /* hack single-page output */
    if (svg->page_count)
      return 0;

    /* skip non-drawing paths for now */
    if (!(type & gx_path_type_fill) && !(type & gx_path_type_stroke))
      return 0;

    if_debug4m('_', svg->memory, "svg_lineto(%lf,%lf,%lf,%lf) ", x0,y0, x,y);
    svg_print_path_type(svg, type);
    if_debug0m('_', svg->memory, "\n");

    gs_sprintf(line, " L%lf,%lf", x, y);
    svg_write(svg, line);

    return 0;
}

static int
svg_curveto(gx_device_vector *vdev, floatp x0, floatp y0,
            floatp x1, floatp y1, floatp x2, floatp y2,
            floatp x3, floatp y3, gx_path_type_t type)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;
    char line[SVG_LINESIZE];

    /* hack single-page output */
    if (svg->page_count)
      return 0;

    /* skip non-drawing paths for now */
    if (!(type & gx_path_type_fill) && !(type & gx_path_type_stroke))
      return 0;

    if_debug8m('_', svg->memory, "svg_curveto(%lf,%lf, %lf,%lf, %lf,%lf, %lf,%lf) ",
        x0,y0, x1,y1, x2,y2, x3,y3);
    svg_print_path_type(svg, type);
    if_debug0m('_', svg->memory, "\n");

    gs_sprintf(line, " C%lf,%lf %lf,%lf %lf,%lf", x1,y1, x2,y2, x3,y3);
    svg_write(svg, line);

    return 0;
}

static int
svg_closepath(gx_device_vector *vdev, floatp x, floatp y,
              floatp x_start, floatp y_start, gx_path_type_t type)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;

    /* hack single-page output */
    if (svg->page_count)
      return 0;

    /* skip non-drawing paths for now */
    if (!(type & gx_path_type_fill) && !(type & gx_path_type_stroke))
      return 0;

    if_debug0m('_', svg->memory, "svg_closepath ");
    svg_print_path_type(svg, type);
    if_debug0m('_', svg->memory, "\n");

    svg_write(svg, " z");

    return 0;
}

static int
svg_endpath(gx_device_vector *vdev, gx_path_type_t type)
{
    gx_device_svg *svg = (gx_device_svg *)vdev;

    /* hack single-page output */
    if (svg->page_count)
      return 0;

    /* skip non-drawing paths for now */
    if (!(type & gx_path_type_fill) && !(type & gx_path_type_stroke))
      return 0;

    if_debug0m('_', svg->memory, "svg_endpath ");
    svg_print_path_type(svg, type);
    if_debug0m('_', svg->memory, "\n");

    /* close the path data attribute */
    svg_write(svg, "'");

    /* override the inherited stroke attribute if we're not stroking */
    if (!(type & gx_path_type_stroke) && (svg->strokecolor != gx_no_color_index))
      svg_write(svg, " stroke='none'");

    /* override the inherited fill attribute if we're not filling */
    if (!(type & gx_path_type_fill) && (svg->fillcolor != gx_no_color_index))
      svg_write(svg, " fill='none'");

    svg_write(svg, "/>\n");

    return 0;
}
