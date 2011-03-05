/*
 * "$Id$"
 *
 *   GNU Ghostscript raster output driver for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1993-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org/
 *
 *   This code and any derivative of it may be used and distributed
 *   freely under the terms of the GNU General Public License when
 *   used with GNU Ghostscript or its derivatives.  Use of the code
 *   (or any derivative of it) with software other than GNU
 *   GhostScript (or its derivatives) is governed by the CUPS license
 *   agreement.
 *
 * Contents:
 *
 *   cups_close()            - Close the output file.
 *   cups_decode_color()     - Decode a color value.
 *   cups_encode_color()     - Encode a color value.
 *   cups_get_color_comp_index()
 *                           - Color component to index
 *   cups_get_color_mapping_procs()
 *                           - Get the list of color mapping procedures.
 *   cups_get_matrix()       - Generate the default page matrix.
 *   cups_get_params()       - Get pagedevice parameters.
 *   cups_get_space_params() - Get space parameters from the RIP_CACHE env var.
 *   cups_map_cielab()       - Map CIE Lab transformation...
 *   cups_map_cmyk()         - Map a CMYK color value to device colors.
 *   cups_map_gray()         - Map a grayscale value to device colors.
 *   cups_map_rgb()          - Map a RGB color value to device colors.
 *   cups_map_cmyk_color()   - Map a CMYK color to a color index.
 *   cups_map_color_rgb()    - Map a color index to an RGB color.
 *   cups_map_rgb_color()    - Map an RGB color to a color index.  We map the
 *                             RGB color to the output colorspace & bits (we
 *                             figure out the format when we output a page).
 *   cups_open()             - Open the output file and initialize things.
 *   cups_print_pages()      - Send one or more pages to the output file.
 *   cups_put_params()       - Set pagedevice parameters.
 *   cups_set_color_info()   - Set the color information structure based on
 *                             the required output.
 *   cups_sync_output()      - Keep the user informed of our status...
 *   cups_print_chunked()    - Print a page of chunked pixels.
 *   cups_print_banded()     - Print a page of banded pixels.
 *   cups_print_planar()     - Print a page of planar pixels.
 */

/*
 * Include necessary headers...
 */

#include "std.h"                /* to stop stdlib.h redefining types */
#include "gdevprn.h"
#include "gsparam.h"
#include "arch.h"

#include <stdlib.h>
#include <ctype.h>
#include <cups/raster.h>
#include <cups/ppd.h>
#include <math.h>

#undef private
#define private

#ifdef WIN32
#define cbrt(arg) pow(arg, 1.0/3)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

/* This should go into gdevprn.h, or, better yet, gdevprn should
   acquire an API for changing resolution. */
int gdev_prn_maybe_realloc_memory(gx_device_printer *pdev,
                                  gdev_prn_space_params *old_space,
                                  int old_width, int old_height,
                                  bool old_page_uses_transparency);

/*
 * Check if we are compiling against CUPS 1.2.  If so, enable
 * certain extended attributes and use a different page header
 * structure and write function...
 */

#ifdef CUPS_RASTER_SYNCv1
#  define cups_page_header_t cups_page_header2_t
#  define cupsRasterWriteHeader cupsRasterWriteHeader2
#else
/* The RGBW colorspace is not defined until CUPS 1.2... */
#  define CUPS_CSPACE_RGBW 17
#endif /* CUPS_RASTER_SYNCv1 */


/*
 * CIE XYZ color constants...
 */

#define D65_X	(0.412453 + 0.357580 + 0.180423)
#define D65_Y	(0.212671 + 0.715160 + 0.072169)
#define D65_Z	(0.019334 + 0.119193 + 0.950227)


/*
 * Size of a tile in pixels...
 */

#define CUPS_TILE_SIZE	256


/*
 * Size of profile LUTs...
 */

#ifdef dev_t_proc_encode_color
#  define CUPS_MAX_VALUE	frac_1
#else
#  define CUPS_MAX_VALUE	gx_max_color_value
#endif /* dev_t_proc_encode_color */


/*
 * Macros...
 */

#define x_dpi		(pdev->HWResolution[0])
#define y_dpi		(pdev->HWResolution[1])
#define cups		((gx_device_cups *)pdev)

/*
 * Macros from <macros.h>; we can't include <macros.h> because it also
 * defines DEBUG, one of our flags to insert various debugging code.
 */

#ifndef max
#  define max(a,b)	((a)<(b) ? (b) : (a))
#endif /* !max */

#ifndef min
#  define min(a,b)	((a)>(b) ? (b) : (a))
#endif /* !min */

#ifndef abs
#  define abs(x)	((x)>=0 ? (x) : -(x))
#endif /* !abs */


/*
 * Procedures
 */

private dev_proc_close_device(cups_close);
private dev_proc_get_initial_matrix(cups_get_matrix);
private int cups_get_params(gx_device *, gs_param_list *);
private dev_proc_open_device(cups_open);
private int cups_print_pages(gx_device_printer *, FILE *, int);
private int cups_put_params(gx_device *, gs_param_list *);
private void cups_set_color_info(gx_device *);
private dev_proc_sync_output(cups_sync_output);
private prn_dev_proc_get_space_params(cups_get_space_params);

#ifdef dev_t_proc_encode_color
private cm_map_proc_gray(cups_map_gray);
private cm_map_proc_rgb(cups_map_rgb);
private cm_map_proc_cmyk(cups_map_cmyk);
private dev_proc_decode_color(cups_decode_color);
private dev_proc_encode_color(cups_encode_color);
private dev_proc_get_color_comp_index(cups_get_color_comp_index);
private dev_proc_get_color_mapping_procs(cups_get_color_mapping_procs);

static const gx_cm_color_map_procs cups_color_mapping_procs =
{
  cups_map_gray,
  cups_map_rgb,
  cups_map_cmyk
};
#else
private dev_proc_map_cmyk_color(cups_map_cmyk_color);
private dev_proc_map_color_rgb(cups_map_color_rgb);
private dev_proc_map_rgb_color(cups_map_rgb_color);
#endif /* dev_t_proc_encode_color */


/*
 * The device descriptors...
 */

typedef struct gx_device_cups_s
{
  gx_device_common;			/* Standard GhostScript device stuff */
  gx_prn_device_common;			/* Standard printer device stuff */
  int			page;		/* Page number */
  cups_raster_t		*stream;	/* Raster stream */
  cups_page_header_t	header;		/* PostScript page device info */
  int			landscape;	/* Non-zero if this is landscape */
  int			lastpage;
  int			HaveProfile;	/* Has a color profile been defined? */
  char			*Profile;	/* Current simple color profile string */
  ppd_file_t		*PPD;		/* PPD file for this device */
  unsigned char		RevLower1[16];	/* Lower 1-bit reversal table */
  unsigned char 	RevUpper1[16];	/* Upper 1-bit reversal table */
  unsigned char		RevLower2[16];	/* Lower 2-bit reversal table */
  unsigned char		RevUpper2[16];	/* Upper 2-bit reversal table */
#ifdef GX_COLOR_INDEX_TYPE
  gx_color_value	DecodeLUT[65536];/* Output color to RGB value LUT */
#else
  gx_color_value	DecodeLUT[256];	/* Output color to RGB value LUT */
#endif /* GX_COLOR_INDEX_TYPE */
  unsigned short	EncodeLUT[gx_max_color_value + 1];/* RGB value to output color LUT */
  int			Density[CUPS_MAX_VALUE + 1];/* Density LUT */
  int			Matrix[3][3][CUPS_MAX_VALUE + 1];/* Color transform matrix LUT */
  int                   cupsRasterVersion;

  /* Used by cups_put_params(): */
} gx_device_cups;

private gx_device_procs	cups_procs =
{
   cups_open,
   cups_get_matrix,
   cups_sync_output,
   gdev_prn_output_page,
   cups_close,
#ifdef dev_t_proc_encode_color
   NULL,				/* map_rgb_color */
   NULL,				/* map_color_rgb */
#else
   cups_map_rgb_color,
   cups_map_color_rgb,
#endif /* dev_t_proc_encode_color */
   NULL,				/* fill_rectangle */
   NULL,				/* tile_rectangle */
   NULL,				/* copy_mono */
   NULL,				/* copy_color */
   NULL,				/* draw_line */
   gx_default_get_bits,
   cups_get_params,
   cups_put_params,
#ifdef dev_t_proc_encode_color
   NULL,				/* map_cmyk_color */
#else
   cups_map_cmyk_color,
#endif /* dev_t_proc_encode_color */
   NULL,				/* get_xfont_procs */
   NULL,				/* get_xfont_device */
   NULL,				/* map_rgb_alpha_color */
   gx_page_device_get_page_device,
   NULL,				/* get_alpha_bits */
   NULL,				/* copy_alpha */
   NULL,				/* get_band */
   NULL,				/* copy_rop */
   NULL,				/* fill_path */
   NULL,				/* stroke_path */
   NULL,				/* fill_mask */
   NULL,				/* fill_trapezoid */
   NULL,				/* fill_parallelogram */
   NULL,				/* fill_triangle */
   NULL,				/* draw_thin_line */
   NULL,				/* begin_image */
   NULL,				/* image_data */
   NULL,				/* end_image */
   NULL,				/* strip_tile_rectangle */
   NULL,				/* strip_copy_rop */
   NULL,				/* get_clipping_box */
   NULL,				/* begin_typed_image */
   NULL,				/* get_bits_rectangle */
   NULL,				/* map_color_rgb_alpha */
   NULL,				/* create_compositor */
   NULL,				/* get_hardware_params */
   NULL,				/* text_begin */
   NULL,				/* finish_copydevice */
   NULL,				/* begin_transparency_group */
   NULL,				/* end_transparency_group */
   NULL,				/* begin_transparency_mask */
   NULL,				/* end_transparency_mask */
   NULL,				/* discard_transparency_layer */
#ifdef dev_t_proc_encode_color
   cups_get_color_mapping_procs,
   cups_get_color_comp_index,
   cups_encode_color,
   cups_decode_color,
#else
   NULL,                                /* get_color_mapping_procs */
   NULL,                                /* get_color_comp_index */
   NULL,                                /* encode_color */
   NULL,                                /* decode_color */
#endif /* dev_t_proc_encode_color */
   NULL,				/* pattern_manage */
   NULL,				/* fill_rectangle_hl_color */
   NULL,				/* include_color_space */
   NULL,				/* fill_linear_color_scanline */
   NULL,				/* fill_linear_color_trapezoid */
   NULL,				/* fill_linear_color_triangle */
   NULL,				/* update_spot_equivalent_colors */
   NULL,				/* ret_devn_params */
   NULL,				/* fillpage */
   NULL,				/* push_transparency_state */
   NULL,				/* pop_transparency_state */
   NULL,                                /* put_image */

};

#define prn_device_body_copies(dtype, procs, dname, w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, ncomp, depth, mg, mc, dg, dc, print_pages)\
	std_device_full_body_type(dtype, &procs, dname, &st_device_printer,\
	  (int)((long)(w10) * (xdpi) / 10),\
	  (int)((long)(h10) * (ydpi) / 10),\
	  xdpi, ydpi,\
	  ncomp, depth, mg, mc, dg, dc,\
	  -(lo) * (xdpi), -(to) * (ydpi),\
	  (lm) * 72.0, (bm) * 72.0,\
	  (rm) * 72.0, (tm) * 72.0\
	),\
	prn_device_body_copies_rest_(print_pages)

gx_device_cups	gs_cups_device =
{
  prn_device_body_copies(gx_device_cups,/* type */
                         cups_procs,	/* procedures */
			 "cups",	/* device name */
			 85,		/* initial width */
			 110,		/* initial height */
			 100,		/* initial x resolution */
			 100,		/* initial y resolution */
                         0,		/* initial left offset */
			 0,		/* initial top offset */
			 0,		/* initial left margin */
			 0,		/* initial bottom margin */
			 0,		/* initial right margin */
			 0,		/* initial top margin */
			 1,		/* number of color components */
			 1,		/* number of color bits */
			 1,		/* maximum gray value */
			 0,		/* maximum color value */
			 2,		/* number of gray values */
			 0,		/* number of color values */
			 cups_print_pages),
					/* print procedure */
  0,					/* page */
  NULL,					/* stream */
  {					/* header */
    "",					/* MediaClass */
    "",					/* MediaColor */
    "",					/* MediaType */
    "",					/* OutputType */
    0,					/* AdvanceDistance */
    CUPS_ADVANCE_NONE,			/* AdvanceMedia */
    CUPS_FALSE,				/* Collate */
    CUPS_CUT_NONE,			/* CutMedia */
    CUPS_FALSE,				/* Duplex */
    { 100, 100 },			/* HWResolution */
    { 0, 0, 612, 792 },			/* ImagingBoundingBox */
    CUPS_FALSE,				/* InsertSheet */
    CUPS_JOG_NONE,			/* Jog */
    CUPS_EDGE_TOP,			/* LeadingEdge */
    { 0, 0 },				/* Margins */
    CUPS_FALSE,				/* ManualFeed */
    0,					/* MediaPosition */
    0,					/* MediaWeight */
    CUPS_FALSE,				/* MirrorPrint */
    CUPS_FALSE,				/* NegativePrint */
    1,					/* NumCopies */
    CUPS_ORIENT_0,			/* Orientation */
    CUPS_FALSE,				/* OutputFaceUp */
    { 612, 792 },			/* PageSize */
    CUPS_FALSE,				/* Separations */
    CUPS_FALSE,				/* TraySwitch */
    CUPS_FALSE,				/* Tumble */
    850,				/* cupsWidth */
    1100,				/* cupsHeight */
    0,					/* cupsMediaType */
    1,					/* cupsBitsPerColor */
    1,					/* cupsBitsPerPixel */
    107,				/* cupsBytesPerLine */
    CUPS_ORDER_CHUNKED,			/* cupsColorOrder */
    CUPS_CSPACE_K,			/* cupsColorSpace */
    0,					/* cupsCompression */
    0,					/* cupsRowCount */
    0,					/* cupsRowFeed */
    0					/* cupsRowStep */
#ifdef CUPS_RASTER_SYNCv1
    ,
    1,                                  /* cupsNumColors */
    1.0,                                /* cupsBorderlessScalingFactor */
    { 612.0, 792.0 },                   /* cupsPageSize */
    { 0.0, 0.0, 612.0, 792.0 },         /* cupsImagingBBox */
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, /* cupsInteger */
    { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
      0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, /* cupsReal */
    { "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "" },
                                        /* cupsString */
    "",                                 /* cupsMarkerType */
    "",                                 /* cupsRenderingIntent */
    ""                                  /* cupsPageSizeName */
#endif /* CUPS_RASTER_SYNCv1 */
  },
  0,                                    /* landscape */
  0,                                    /* lastpage */
  0,                                    /* HaveProfile */
  NULL,                                 /* Profile */
  NULL,                                 /* PPD */
  { 0x00, 0x08, 0x04, 0x0c, 0x02, 0x0a, 0x06, 0x0e,
    0x01, 0x09, 0x05, 0x0d, 0x03, 0x0b, 0x07, 0x0f },/* RevLower1 */
  { 0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0 },/* RevUpper1 */
  { 0x00, 0x04, 0x08, 0x0c, 0x01, 0x05, 0x09, 0x0d,
    0x02, 0x06, 0x0a, 0x0e, 0x03, 0x07, 0x0b, 0x0f },/* RevLower2 */
  { 0x00, 0x40, 0x80, 0xc0, 0x10, 0x50, 0x90, 0xd0,
    0x20, 0x60, 0xa0, 0xe0, 0x30, 0x70, 0xb0, 0xf0 },/* RevUpper2 */
  {0x00},                                  /* DecodeLUT */
  {0x00},                                  /* EncodeLUT */
  {0x00},                                  /* Density */
  {0x00},                                  /* Matrix */
  3                                     /* cupsRasterVersion */
};

/*
 * Local functions...
 */

static double	cups_map_cielab(double, double);
static int	cups_print_chunked(gx_device_printer *, unsigned char *,
		                   unsigned char *, int);
static int	cups_print_banded(gx_device_printer *, unsigned char *,
		                  unsigned char *, int);
static int	cups_print_planar(gx_device_printer *, unsigned char *,
		                  unsigned char *, int);

/*static void	cups_set_margins(gx_device *);*/


/*
 * 'cups_close()' - Close the output file.
 */

private int
cups_close(gx_device *pdev)		/* I - Device info */
{
#ifdef DEBUG
  dprintf1("DEBUG2: cups_close(%p)\n", pdev);
#endif /* DEBUG */

  dprintf("INFO: Rendering completed\n");

  if (cups->stream != NULL)
  {
    cupsRasterClose(cups->stream);
    close(fileno(cups->file));
    cups->stream = NULL;
    cups->file = NULL;
  }

#if 0 /* Can't do this here because put_params() might close the device */
  if (cups->PPD != NULL)
  {
    ppdClose(cups->PPD);
    cups->PPD = NULL;
  }

  if (cups->Profile != NULL)
  {
    free(cups->Profile);
    cups->Profile = NULL;
  }
#endif /* 0 */

  return (gdev_prn_close(pdev));
}


#ifdef dev_t_proc_encode_color
/*
 * 'cups_decode_color()' - Decode a color value.
 */

private int				/* O - Status (0 = OK) */
cups_decode_color(gx_device      *pdev,	/* I - Device info */
                  gx_color_index ci,	/* I - Color index */
                  gx_color_value *cv)	/* O - Colors */
{
  int			i;		/* Looping var */
  int			shift;		/* Bits to shift */
  int			mask;		/* Bits to mask */


  if (cups->header.cupsColorSpace == CUPS_CSPACE_KCMYcm &&
      cups->header.cupsBitsPerColor == 1)
  {
   /*
    * KCMYcm data is represented internally by Ghostscript as CMYK...
    */

    cv[0] = (ci & 0x20) ? frac_1 : frac_0;
    cv[1] = (ci & 0x12) ? frac_1 : frac_0;
    cv[2] = (ci & 0x09) ? frac_1 : frac_0;
    cv[3] = (ci & 0x04) ? frac_1 : frac_0;
  }
  else
  {
    shift = cups->header.cupsBitsPerColor;
    mask  = (1 << shift) - 1;

    for (i = cups->color_info.num_components - 1; i > 0; i --, ci >>= shift)
      cv[i] = cups->DecodeLUT[ci & mask];

    cv[0] = cups->DecodeLUT[ci & mask];
  }

  return (0);
}


/*
 * 'cups_encode_color()' - Encode a color value.
 */

private gx_color_index			/* O - Color index */
cups_encode_color(gx_device            *pdev,
					/* I - Device info */
                  const gx_color_value *cv)
					/* I - Colors */
{
  int			i;		/* Looping var */
  gx_color_index	ci;		/* Color index */
  int			shift;		/* Bits to shift */


 /*
  * Encode the color index...
  */

  shift = cups->header.cupsBitsPerColor;

  for (ci = cups->EncodeLUT[cv[0]], i = 1;
       i < cups->color_info.num_components;
       i ++)
    ci = (ci << shift) | cups->EncodeLUT[cv[i]];

#ifdef DEBUG
  dprintf2("DEBUG2: cv[0]=%d -> %llx\n", cv[0], ci);
#endif /* DEBUG */

 /*
  * Handle 6-color output...
  */

  if (cups->header.cupsColorSpace == CUPS_CSPACE_KCMYcm &&
      cups->header.cupsBitsPerColor == 1)
  {
   /*
    * Welcome to hackville, where we map CMYK data to the
    * light inks in draft mode...  Map blue to light magenta and
    * cyan and green to light cyan and yellow...
    */

    ci <<= 2;				/* Leave room for light inks */

    if (ci == 0x18)			/* Blue */
      ci = 0x11;			/* == cyan + light magenta */
    else if (ci == 0x14)		/* Green */
      ci = 0x06;			/* == light cyan + yellow */
  }

 /*
  * Range check the return value...
  */

  if (ci == gx_no_color_index)
    ci --;

 /*
  * Return the color index...
  */

  return (ci);
}

/*
 * 'cups_get_color_comp_index()' - Color component to index
 */

#define compare_color_names(pname, name_size, name_str) \
    (name_size == (int)strlen(name_str) && strncasecmp(pname, name_str, name_size) == 0)

int                                     /* O - Index of the named color in
					   the color space */
cups_get_color_comp_index(gx_device * pdev, const char * pname,
			  int name_size, int component_type)
{
  switch (cups->header.cupsColorSpace)
  {
    case CUPS_CSPACE_K :
        if (compare_color_names(pname, name_size, "Black") ||
	    compare_color_names(pname, name_size, "Gray") ||
	    compare_color_names(pname, name_size, "Grey"))
	    return 0;
	else
	    return -1; /* Indicate that the component name is "unknown" */
        break;
    case CUPS_CSPACE_W :
    case CUPS_CSPACE_WHITE :
        if (compare_color_names(pname, name_size, "White") ||
	    compare_color_names(pname, name_size, "Luminance") ||
	    compare_color_names(pname, name_size, "Gray") ||
	    compare_color_names(pname, name_size, "Grey"))
	    return 0;
	else
	    return -1;
        break;
    case CUPS_CSPACE_RGBA :
        if (compare_color_names(pname, name_size, "Alpha") ||
	    compare_color_names(pname, name_size, "Transparent") ||
	    compare_color_names(pname, name_size, "Transparency"))
	    return 3;
    case CUPS_CSPACE_RGBW :
        if (compare_color_names(pname, name_size, "White"))
	    return 3;
    case CUPS_CSPACE_RGB :
        if (compare_color_names(pname, name_size, "Red"))
	    return 0;
	if (compare_color_names(pname, name_size, "Green"))
	    return 1;
	if (compare_color_names(pname, name_size, "Blue"))
            return 2;
	else
	    return -1;
        break;
    case CUPS_CSPACE_CMYK :
#  ifdef CUPS_RASTER_HAVE_COLORIMETRIC
    case CUPS_CSPACE_CIEXYZ :
    case CUPS_CSPACE_CIELab :
    case CUPS_CSPACE_ICC1 :
    case CUPS_CSPACE_ICC2 :
    case CUPS_CSPACE_ICC3 :
    case CUPS_CSPACE_ICC4 :
    case CUPS_CSPACE_ICC5 :
    case CUPS_CSPACE_ICC6 :
    case CUPS_CSPACE_ICC7 :
    case CUPS_CSPACE_ICC8 :
    case CUPS_CSPACE_ICC9 :
    case CUPS_CSPACE_ICCA :
    case CUPS_CSPACE_ICCB :
    case CUPS_CSPACE_ICCC :
    case CUPS_CSPACE_ICCD :
    case CUPS_CSPACE_ICCE :
    case CUPS_CSPACE_ICCF :
#  endif /* CUPS_RASTER_HAVE_COLORIMETRIC */
        if (compare_color_names(pname, name_size, "Black"))
	    return 3;
    case CUPS_CSPACE_CMY :
        if (compare_color_names(pname, name_size, "Cyan"))
	    return 0;
	if (compare_color_names(pname, name_size, "Magenta"))
	    return 1;
	if (compare_color_names(pname, name_size, "Yellow"))
	    return 2;
	else
	    return -1;
        break;
    case CUPS_CSPACE_GMCS :
        if (compare_color_names(pname, name_size, "Silver") ||
	    compare_color_names(pname, name_size, "Silver Foil"))
	    return 3;
    case CUPS_CSPACE_GMCK :
        if (compare_color_names(pname, name_size, "Gold") ||
	    compare_color_names(pname, name_size, "Gold Foil"))
	    return 0;
    case CUPS_CSPACE_YMCK :
        if (compare_color_names(pname, name_size, "Black"))
	    return 3;
    case CUPS_CSPACE_YMC :
	if (compare_color_names(pname, name_size, "Yellow"))
	    return 0;
	if (compare_color_names(pname, name_size, "Magenta"))
	    return 1;
        if (compare_color_names(pname, name_size, "Cyan"))
	    return 2;
	else
	    return -1;
        break;
    case CUPS_CSPACE_KCMYcm :
        if (compare_color_names(pname, name_size, "Light Cyan") ||
	    compare_color_names(pname, name_size, "Photo Cyan"))
	    return 4;
        if (compare_color_names(pname, name_size, "Light Magenta") ||
	    compare_color_names(pname, name_size, "Photo Magenta"))
	    return 5;
    case CUPS_CSPACE_KCMY :
        if (compare_color_names(pname, name_size, "Black"))
	    return 0;
        if (compare_color_names(pname, name_size, "Cyan"))
	    return 1;
	if (compare_color_names(pname, name_size, "Magenta"))
	    return 2;
	if (compare_color_names(pname, name_size, "Yellow"))
	    return 3;
	else
	    return -1;
        break;
    case CUPS_CSPACE_GOLD :
        if (compare_color_names(pname, name_size, "Gold") ||
	    compare_color_names(pname, name_size, "Gold Foil"))
	    return 0;
	else
	    return -1;
        break;
    case CUPS_CSPACE_SILVER :
        if (compare_color_names(pname, name_size, "Silver") ||
	    compare_color_names(pname, name_size, "Silver Foil"))
	    return 0;
	else
	    return -1;
        break;
  }
  return -1;
}

/*
 * 'cups_get_color_mapping_procs()' - Get the list of color mapping procedures.
 */

private const gx_cm_color_map_procs *	/* O - List of device procedures */
cups_get_color_mapping_procs(const gx_device *pdev)
					/* I - Device info */
{
  return (&cups_color_mapping_procs);
}
#endif /* dev_t_proc_encode_color */


/*
 * 'cups_get_matrix()' - Generate the default page matrix.
 */

private void
cups_get_matrix(gx_device *pdev,	/* I - Device info */
                gs_matrix *pmat)	/* O - Physical transform matrix */
{
#ifdef DEBUG
  dprintf2("DEBUG2: cups_get_matrix(%p, %p)\n", pdev, pmat);
#endif /* DEBUG */

 /*
  * Set the raster width and height...
  */

  cups->header.cupsWidth  = cups->width;
  cups->header.cupsHeight = cups->height;

 /*
  * Set the transform matrix...
  */

  if (cups->landscape)
  {
   /*
    * Do landscape orientation...
    */
#ifdef DEBUG
    dprintf("DEBUG2: Landscape matrix: XX=0 XY=+1 YX=+1 YY=0\n");
#endif /* DEBUG */
    pmat->xx = 0.0;
    pmat->xy = (float)cups->header.HWResolution[1] / 72.0;
    pmat->yx = (float)cups->header.HWResolution[0] / 72.0;
    pmat->yy = 0.0;
    pmat->tx = -(float)cups->header.HWResolution[0] * pdev->HWMargins[1] / 72.0;
    pmat->ty = -(float)cups->header.HWResolution[1] * pdev->HWMargins[0] / 72.0;
  }
  else
  {
   /*
    * Do portrait orientation...
    */
#ifdef DEBUG
    dprintf("DEBUG2: Portrait matrix: XX=+1 XY=0 YX=0 YY=-1\n");
#endif /* DEBUG */
    pmat->xx = (float)cups->header.HWResolution[0] / 72.0;
    pmat->xy = 0.0;
    pmat->yx = 0.0;
    pmat->yy = -(float)cups->header.HWResolution[1] / 72.0;
    pmat->tx = -(float)cups->header.HWResolution[0] * pdev->HWMargins[0] / 72.0;
    pmat->ty = (float)cups->header.HWResolution[1] *
               ((float)cups->header.PageSize[1] - pdev->HWMargins[3]) / 72.0;
  }

#ifdef CUPS_RASTER_SYNCv1
  if (cups->header.cupsBorderlessScalingFactor > 1.0)
  {
    pmat->xx *= cups->header.cupsBorderlessScalingFactor;
    pmat->xy *= cups->header.cupsBorderlessScalingFactor;
    pmat->yx *= cups->header.cupsBorderlessScalingFactor;
    pmat->yy *= cups->header.cupsBorderlessScalingFactor;
    pmat->tx *= cups->header.cupsBorderlessScalingFactor;
    pmat->ty *= cups->header.cupsBorderlessScalingFactor;
  }
#endif /* CUPS_RASTER_SYNCv1 */

#ifdef DEBUG
  dprintf2("DEBUG2: width = %d, height = %d\n", cups->header.cupsWidth,
	   cups->header.cupsHeight);
  dprintf4("DEBUG2: PageSize = [ %d %d ], HWResolution = [ %d %d ]\n",
	   cups->header.PageSize[0], cups->header.PageSize[1],
	   cups->header.HWResolution[0], cups->header.HWResolution[1]);
  if (size_set)
    dprintf4("DEBUG2: HWMargins = [ %.3f %.3f %.3f %.3f ]\n",
	     pdev->HWMargins[0], pdev->HWMargins[1], pdev->HWMargins[2],
	     pdev->HWMargins[3]);
  dprintf6("DEBUG2: matrix = [ %.3f %.3f %.3f %.3f %.3f %.3f ]\n",
	   pmat->xx, pmat->xy, pmat->yx, pmat->yy, pmat->tx, pmat->ty);
#endif /* DEBUG */
}


/*
 * 'cups_get_params()' - Get pagedevice parameters.
 */

private int				/* O - Error status */
cups_get_params(gx_device     *pdev,	/* I - Device info */
                gs_param_list *plist)	/* I - Parameter list */
{
#ifdef CUPS_RASTER_SYNCv1
  int			i;		/* Looping var */
  char			name[255];	/* Attribute name */
#endif /* CUPS_RASTER_SYNCv1 */
  int			code;		/* Return code */
  gs_param_string	s;		/* Temporary string value */
  bool			b;		/* Temporary boolean value */


#ifdef DEBUG
  dprintf2("DEBUG2: cups_get_params(%p, %p)\n", pdev, plist);
#endif /* DEBUG */

 /*
  * First process the "standard" page device parameters...
  */

#ifdef DEBUG
  dprintf("DEBUG2: before gdev_prn_get_params()\n");
#endif /* DEBUG */

  if ((code = gdev_prn_get_params(pdev, plist)) < 0)
    return (code);

#ifdef DEBUG
  dprintf("DEBUG2: after gdev_prn_get_params()\n");
#endif /* DEBUG */

 /*
  * Then write the CUPS parameters...
  */

  param_string_from_string(s, cups->header.MediaClass);
  if ((code = param_write_string(plist, "MediaClass", &s)) < 0)
    return (code);

  param_string_from_string(s, cups->header.MediaColor);
  if ((code = param_write_string(plist, "MediaColor", &s)) < 0)
    return (code);

  param_string_from_string(s, cups->header.MediaType);
  if ((code = param_write_string(plist, "MediaType", &s)) < 0)
    return (code);

  param_string_from_string(s, cups->header.OutputType);
  if ((code = param_write_string(plist, "OutputType", &s)) < 0)
    return (code);

  if ((code = param_write_int(plist, "AdvanceDistance",
                              (int *)&(cups->header.AdvanceDistance))) < 0)
    return (code);

  if ((code = param_write_int(plist, "AdvanceMedia",
                              (int *)&(cups->header.AdvanceMedia))) < 0)
    return (code);

  b = cups->header.Collate;
  if ((code = param_write_bool(plist, "Collate", &b)) < 0)
    return (code);

  if ((code = param_write_int(plist, "CutMedia",
                              (int *)&(cups->header.CutMedia))) < 0)
    return (code);

  b = cups->header.Duplex;
  if ((code = param_write_bool(plist, "Duplex", &b)) < 0)
    return (code);

  b = cups->header.InsertSheet;
  if ((code = param_write_bool(plist, "InsertSheet", &b)) < 0)
    return (code);

  if ((code = param_write_int(plist, "Jog",
                              (int *)&(cups->header.Jog))) < 0)
    return (code);

  if ((code = param_write_int(plist, "LeadingEdge",
                              (int *)&(cups->header.LeadingEdge))) < 0)
    return (code);

  b = cups->header.ManualFeed;
  if ((code = param_write_bool(plist, "ManualFeed", &b)) < 0)
    return (code);

  if ((code = param_write_int(plist, "MediaPosition",
                              (int *)&(cups->header.MediaPosition))) < 0)
    return (code);

  if ((code = param_write_int(plist, "MediaWeight",
                              (int *)&(cups->header.MediaWeight))) < 0)
    return (code);

  b = cups->header.MirrorPrint;
  if ((code = param_write_bool(plist, "MirrorPrint", &b)) < 0)
    return (code);

  b = cups->header.NegativePrint;
  if ((code = param_write_bool(plist, "NegativePrint", &b)) < 0)
    return (code);

  b = cups->header.OutputFaceUp;
  if ((code = param_write_bool(plist, "OutputFaceUp", &b)) < 0)
    return (code);

  b = cups->header.Separations;
  if ((code = param_write_bool(plist, "Separations", &b)) < 0)
    return (code);

  b = cups->header.TraySwitch;
  if ((code = param_write_bool(plist, "TraySwitch", &b)) < 0)
    return (code);

  b = cups->header.Tumble;
  if ((code = param_write_bool(plist, "Tumble", &b)) < 0)
    return (code);

#if 0 /* Don't include read-only parameters... */
  if ((code = param_write_int(plist, "cupsWidth",
                              (int *)&(cups->header.cupsWidth))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsHeight",
                              (int *)&(cups->header.cupsHeight))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsBitsPerPixel",
                              (int *)&(cups->header.cupsBitsPerPixel))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsBytesPerLine",
                              (int *)&(cups->header.cupsBytesPerLine))) < 0)
    return (code);
#endif /* 0 */

  if ((code = param_write_int(plist, "cupsMediaType",
                              (int *)&(cups->header.cupsMediaType))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsBitsPerColor",
                              (int *)&(cups->header.cupsBitsPerColor))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsColorOrder",
                              (int *)&(cups->header.cupsColorOrder))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsColorSpace",
                              (int *)&(cups->header.cupsColorSpace))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsCompression",
                              (int *)&(cups->header.cupsCompression))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsRowCount",
                              (int *)&(cups->header.cupsRowCount))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsRowFeed",
                              (int *)&(cups->header.cupsRowFeed))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsRowStep",
                              (int *)&(cups->header.cupsRowStep))) < 0)
    return (code);

#ifdef CUPS_RASTER_SYNCv1
#if 0 /* Don't include read-only parameters... */
  if ((code = param_write_int(plist, "cupsNumColors",
                              (int *)&(cups->header.cupsNumColors))) < 0)
    return (code);
#endif /* 0 */

  if ((code = param_write_float(plist, "cupsBorderlessScalingFactor",
                        	&(cups->header.cupsBorderlessScalingFactor))) < 0)
    return (code);

  for (i = 0; i < 16; i ++)
  {
    sprintf(name, "cupsInteger%d", i);
    if ((code = param_write_int(plist, strdup(name),
                        	(int *)(cups->header.cupsInteger + i))) < 0)
      return (code);
  }

  for (i = 0; i < 16; i ++)
  {
    sprintf(name, "cupsReal%d", i);
    if ((code = param_write_float(plist, strdup(name),
                        	  cups->header.cupsReal + i)) < 0)
      return (code);
  }

  for (i = 0; i < 16; i ++)
  {
    sprintf(name, "cupsString%d", i);
    param_string_from_string(s, cups->header.cupsString[i]);
    if ((code = param_write_string(plist, strdup(name), &s)) < 0)
      return (code);
  }

  param_string_from_string(s, cups->header.cupsMarkerType);
  if ((code = param_write_string(plist, "cupsMarkerType", &s)) < 0)
    return (code);

  param_string_from_string(s, cups->header.cupsRenderingIntent);
  if ((code = param_write_string(plist, "cupsRenderingIntent", &s)) < 0)
    return (code);

  param_string_from_string(s, cups->header.cupsPageSizeName);
  if ((code = param_write_string(plist, "cupsPageSizeName", &s)) < 0)
    return (code);
#endif /* CUPS_RASTER_SYNCv1 */

#ifdef DEBUG
  dprintf("DEBUG2: Leaving cups_get_params()\n");
#endif /* DEBUG */

  return (0);
}


/*
 * 'cups_get_space_params()' - Get space parameters from the RIP_CACHE env var.
 */

void
cups_get_space_params(const gx_device_printer *pdev,
					/* I - Printer device */
                      gdev_prn_space_params   *space_params)
					/* O - Space parameters */
{
  float	cache_size;			/* Size of tile cache in bytes */
  char	*cache_env,			/* Cache size environment variable */
	cache_units[255];		/* Cache size units */


#ifdef DEBUG
  dprintf2("DEBUG2: cups_get_space_params(%p, %p)\n", pdev, space_params);
#endif /* DEBUG */

  if ((cache_env = getenv("RIP_MAX_CACHE")) != NULL)
  {
    switch (sscanf(cache_env, "%f%254s", &cache_size, cache_units))
    {
      case 0 :
          return;
      case 1 :
          cache_size *= 4 * CUPS_TILE_SIZE * CUPS_TILE_SIZE;
	  break;
      case 2 :
          if (tolower(cache_units[0]) == 'g')
	    cache_size *= 1024 * 1024 * 1024;
          else if (tolower(cache_units[0]) == 'm')
	    cache_size *= 1024 * 1024;
	  else if (tolower(cache_units[0]) == 'k')
	    cache_size *= 1024;
	  else if (tolower(cache_units[0]) == 't')
	    cache_size *= 4 * CUPS_TILE_SIZE * CUPS_TILE_SIZE;
	  break;
    }
  }
  else
    return;

  if (cache_size == 0)
    return;

#ifdef DEBUG
  dprintf1("DEBUG2: cache_size = %.0f\n", cache_size);
#endif /* DEBUG */

  space_params->MaxBitmap   = (long)cache_size;
  space_params->BufferSpace = (long)cache_size;
}


/*
 * 'cups_map_cielab()' - Map CIE Lab transformation...
 */

static double				/* O - Adjusted color value */
cups_map_cielab(double x,		/* I - Raw color value */
                double xn)		/* I - Whitepoint color value */
{
  double x_xn;				/* Fraction of whitepoint */


  x_xn = x / xn;

  if (x_xn > 0.008856)
    return (cbrt(x_xn));
  else
    return (7.787 * x_xn + 16.0 / 116.0);
}


#ifdef dev_t_proc_encode_color
/*
 * 'cups_map_cmyk()' - Map a CMYK color value to device colors.
 */

private void
cups_map_cmyk(gx_device *pdev,		/* I - Device info */
              frac      c,		/* I - Cyan value */
	      frac      m,		/* I - Magenta value */
	      frac      y,		/* I - Yellow value */
	      frac      k,		/* I - Black value */
	      frac      *out)		/* O - Device colors */
{
  int	c0 = 0, c1 = 0,
        c2 = 0, c3 = 0;			/* Temporary color values */
  float	rr, rg, rb,			/* Real RGB colors */
	ciex, ciey, ciez,		/* CIE XYZ colors */
	ciey_yn,			/* Normalized luminance */
	ciel, ciea, cieb;		/* CIE Lab colors */


#ifdef DEBUG
  dprintf6("DEBUG2: cups_map_cmyk(%p, %d, %d, %d, %d, %p)\n",
          pdev, c, m, y, k, out);
#endif /* DEBUG */

 /*
  * Convert the CMYK color to the destination colorspace...
  */

  switch (cups->header.cupsColorSpace)
  {
    case CUPS_CSPACE_W :
        c0 = (c * 31 + m * 61 + y * 8) / 100 + k;
	
	if (c0 < 0)
	  c0 = 0;
	else if (c0 > frac_1)
	  c0 = frac_1;
	out[0] = frac_1 - (frac)cups->Density[c0];
        break;

    case CUPS_CSPACE_RGBA :
        out[3] = frac_1;

    case CUPS_CSPACE_RGB :
    case CUPS_CSPACE_RGBW :
        if (cups->header.cupsColorSpace == CUPS_CSPACE_RGBW) {
	  c0 = c;
	  c1 = m;
	  c2 = y;
	  c3 = k;
	} else {
	  c0 = c + k;
	  c1 = m + k;
	  c2 = y + k;
	}

        if (c0 < 0)
	  c0 = 0;
	else if (c0 > frac_1)
	  c0 = frac_1;
	out[0] = frac_1 - (frac)cups->Density[c0];

        if (c1 < 0)
	  c1 = 0;
	else if (c1 > frac_1)
	  c1 = frac_1;
	out[1] = frac_1 - (frac)cups->Density[c1];

        if (c2 < 0)
	  c2 = 0;
	else if (c2 > frac_1)
	  c2 = frac_1;
	out[2] = frac_1 - (frac)cups->Density[c2];

        if (cups->header.cupsColorSpace == CUPS_CSPACE_RGBW) {
	  if (c3 < 0)
	    c3 = 0;
	  else if (c3 > frac_1)
	    c3 = frac_1;
	  out[3] = frac_1 - (frac)cups->Density[c3];
	}
        break;

    default :
    case CUPS_CSPACE_K :
        c0 = (c * 31 + m * 61 + y * 8) / 100 + k;

	if (c0 < 0)
	  out[0] = 0;
	else if (c0 > frac_1)
	  out[0] = (frac)cups->Density[frac_1];
	else
	  out[0] = (frac)cups->Density[c0];
        break;

    case CUPS_CSPACE_CMY :
        c0 = c + k;
	c1 = m + k;
	c2 = y + k;

        if (c0 < 0)
	  out[0] = 0;
	else if (c0 > frac_1)
	  out[0] = (frac)cups->Density[frac_1];
	else
	  out[0] = (frac)cups->Density[c0];

        if (c1 < 0)
	  out[1] = 0;
	else if (c1 > frac_1)
	  out[1] = (frac)cups->Density[frac_1];
	else
	  out[1] = (frac)cups->Density[c1];

        if (c2 < 0)
	  out[2] = 0;
	else if (c2 > frac_1)
	  out[2] = (frac)cups->Density[frac_1];
	else
	  out[2] = (frac)cups->Density[c2];
        break;

    case CUPS_CSPACE_YMC :
        c0 = y + k;
	c1 = m + k;
	c2 = c + k;

        if (c0 < 0)
	  out[0] = 0;
	else if (c0 > frac_1)
	  out[0] = (frac)cups->Density[frac_1];
	else
	  out[0] = (frac)cups->Density[c0];

        if (c1 < 0)
	  out[1] = 0;
	else if (c1 > frac_1)
	  out[1] = (frac)cups->Density[frac_1];
	else
	  out[1] = (frac)cups->Density[c1];

        if (c2 < 0)
	  out[2] = 0;
	else if (c2 > frac_1)
	  out[2] = (frac)cups->Density[frac_1];
	else
	  out[2] = (frac)cups->Density[c2];
        break;

    case CUPS_CSPACE_CMYK :
        if (c < 0)
	  out[0] = 0;
	else if (c > frac_1)
	  out[0] = (frac)cups->Density[frac_1];
	else
	  out[0] = (frac)cups->Density[c];

        if (m < 0)
	  out[1] = 0;
	else if (m > frac_1)
	  out[1] = (frac)cups->Density[frac_1];
	else
	  out[1] = (frac)cups->Density[m];

        if (y < 0)
	  out[2] = 0;
	else if (y > frac_1)
	  out[2] = (frac)cups->Density[frac_1];
	else
	  out[2] = (frac)cups->Density[y];

        if (k < 0)
	  out[3] = 0;
	else if (k > frac_1)
	  out[3] = (frac)cups->Density[frac_1];
	else
	  out[3] = (frac)cups->Density[k];
        break;

    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
        if (y < 0)
	  out[0] = 0;
	else if (y > frac_1)
	  out[0] = (frac)cups->Density[frac_1];
	else
	  out[0] = (frac)cups->Density[y];

        if (m < 0)
	  out[1] = 0;
	else if (m > frac_1)
	  out[1] = (frac)cups->Density[frac_1];
	else
	  out[1] = (frac)cups->Density[m];

        if (c < 0)
	  out[2] = 0;
	else if (c > frac_1)
	  out[2] = (frac)cups->Density[frac_1];
	else
	  out[2] = (frac)cups->Density[c];

        if (k < 0)
	  out[3] = 0;
	else if (k > frac_1)
	  out[3] = (frac)cups->Density[frac_1];
	else
	  out[3] = (frac)cups->Density[k];
        break;

    case CUPS_CSPACE_KCMYcm :
    case CUPS_CSPACE_KCMY :
        if (k < 0)
	  out[0] = 0;
	else if (k > frac_1)
	  out[0] = (frac)cups->Density[frac_1];
	else
	  out[0] = (frac)cups->Density[k];

        if (c < 0)
	  out[1] = 0;
	else if (c > frac_1)
	  out[1] = (frac)cups->Density[frac_1];
	else
	  out[1] = (frac)cups->Density[c];

        if (m < 0)
	  out[2] = 0;
	else if (m > frac_1)
	  out[2] = (frac)cups->Density[frac_1];
	else
	  out[2] = (frac)cups->Density[m];

        if (y < 0)
	  out[3] = 0;
	else if (y > frac_1)
	  out[3] = (frac)cups->Density[frac_1];
	else
	  out[3] = (frac)cups->Density[y];
        break;

#  ifdef CUPS_RASTER_HAVE_COLORIMETRIC
    case CUPS_CSPACE_CIEXYZ :
    case CUPS_CSPACE_CIELab :
    case CUPS_CSPACE_ICC1 :
    case CUPS_CSPACE_ICC2 :
    case CUPS_CSPACE_ICC3 :
    case CUPS_CSPACE_ICC4 :
    case CUPS_CSPACE_ICC5 :
    case CUPS_CSPACE_ICC6 :
    case CUPS_CSPACE_ICC7 :
    case CUPS_CSPACE_ICC8 :
    case CUPS_CSPACE_ICC9 :
    case CUPS_CSPACE_ICCA :
    case CUPS_CSPACE_ICCB :
    case CUPS_CSPACE_ICCC :
    case CUPS_CSPACE_ICCD :
    case CUPS_CSPACE_ICCE :
    case CUPS_CSPACE_ICCF :
       /*
        * Convert CMYK to sRGB...
	*/

        c0 = frac_1 - c - k;
	c1 = frac_1 - m - k;
	c2 = frac_1 - y - k;

        if (c0 < 0)
	  c0 = 0;

        if (c1 < 0)
	  c1 = 0;

        if (c2 < 0)
	  c2 = 0;

       /*
        * Convert sRGB to linear RGB...
	*/

	rr = pow(((double)c0 / (double)frac_1 + 0.055) / 1.055, 2.4);
	rg = pow(((double)c1 / (double)frac_1 + 0.055) / 1.055, 2.4);
	rb = pow(((double)c2 / (double)frac_1 + 0.055) / 1.055, 2.4);

       /*
        * Convert to CIE XYZ...
	*/

	ciex = 0.412453 * rr + 0.357580 * rg + 0.180423 * rb;
	ciey = 0.212671 * rr + 0.715160 * rg + 0.072169 * rb;
	ciez = 0.019334 * rr + 0.119193 * rg + 0.950227 * rb;

        if (cups->header.cupsColorSpace == CUPS_CSPACE_CIEXYZ)
	{
	 /*
	  * Convert to an integer XYZ color value...
	  */

          if (cups->header.cupsBitsPerColor == 8)
	  {
	    if (ciex <= 0.0f)
	      c0 = 0;
	    else if (ciex < 1.1)
	      c0 = (int)(ciex * 231.8181 + 0.5);
	    else
	      c0 = 255;

	    if (ciey <= 0.0f)
	      c1 = 0;
	    else if (ciey < 1.1)
	      c1 = (int)(ciey * 231.8181 + 0.5);
	    else
	      c1 = 255;

	    if (ciez <= 0.0f)
	      c2 = 0;
	    else if (ciez < 1.1)
	      c2 = (int)(ciez * 231.8181 + 0.5);
	    else
	      c2 = 255;
	  }
	  else
	  {
	    if (ciex <= 0.0f)
	      c0 = 0;
	    else if (ciex < 1.1)
	      c0 = (int)(ciex * 59577.2727 + 0.5);
	    else
	      c0 = 65535;

	    if (ciey <= 0.0f)
	      c1 = 0;
	    else if (ciey < 1.1)
	      c1 = (int)(ciey * 59577.2727 + 0.5);
	    else
	      c1 = 65535;

	    if (ciez <= 0.0f)
	      c2 = 0;
	    else if (ciez < 1.1)
	      c2 = (int)(ciez * 59577.2727 + 0.5);
	    else
	      c2 = 65535;
	  }
	}
	else
	{
	 /*
	  * Convert CIE XYZ to Lab...
	  */

	  ciey_yn = ciey / D65_Y;

	  if (ciey_yn > 0.008856)
	    ciel = 116 * cbrt(ciey_yn) - 16;
	  else
	    ciel = 903.3 * ciey_yn;

	  ciea = 500 * (cups_map_cielab(ciex, D65_X) -
	                cups_map_cielab(ciey, D65_Y));
	  cieb = 200 * (cups_map_cielab(ciey, D65_Y) -
	                cups_map_cielab(ciez, D65_Z));

          if (cups->header.cupsBitsPerColor == 8)
	  {
           /*
	    * Scale the L value and bias the a and b values by 128
	    * so that all values are in the range of 0 to 255.
	    */

	    ciel = ciel * 2.55 + 0.5;
	    ciea += 128.5;
	    cieb += 128.5;

	    if (ciel <= 0.0)
	      c0 = 0;
	    else if (ciel < 255.0)
	      c0 = (int)ciel;
	    else
	      c0 = 255;

	    if (ciea <= 0.0)
	      c1 = 0;
	    else if (ciea < 255.0)
	      c1 = (int)ciea;
	    else
	      c1 = 255;

	    if (cieb <= 0.0)
	      c2 = 0;
	    else if (cieb < 255.0)
	      c2 = (int)cieb;
	    else
	      c2 = 255;
          }
	  else
	  {
	   /*
	    * Scale the L value and bias the a and b values by 128 so that all
	    * numbers are from 0 to 65535.
	    */

	    ciel = ciel * 655.35 + 0.5;
	    ciea = (ciea + 128.0) * 256.0 + 0.5;
	    cieb = (cieb + 128.0) * 256.0 + 0.5;

	   /*
	    * Output 16-bit values...
	    */

	    if (ciel <= 0.0)
	      c0 = 0;
	    else if (ciel < 65535.0)
	      c0 = (int)ciel;
	    else
	      c0 = 65535;

	    if (ciea <= 0.0)
	      c1 = 0;
	    else if (ciea < 65535.0)
	      c1 = (int)ciea;
	    else
	      c1 = 65535;

	    if (cieb <= 0.0)
	      c2 = 0;
	    else if (cieb < 65535.0)
	      c2 = (int)cieb;
	    else
	      c2 = 65535;
	  }
	}

        out[0] = cups->DecodeLUT[c0];
        out[1] = cups->DecodeLUT[c1];
        out[2] = cups->DecodeLUT[c2];
        break;
#  endif /* CUPS_RASTER_HAVE_COLORIMETRIC */
  }

#ifdef DEBUG
  switch (cups->color_info.num_components)
  {
    default :
    case 1 :
        dprintf1("DEBUG2:   \\=== COLOR %d\n", out[0]);
	break;

    case 3 :
        dprintf3("DEBUG2:   \\=== COLOR %d, %d, %d\n",
	        out[0], out[1], out[2]);
	break;

    case 4 :
        dprintf4("DEBUG2:   \\=== COLOR %d, %d, %d, %d\n",
	        out[0], out[1], out[2], out[3]);
	break;
  }
#endif /* DEBUG */
}


/*
 * 'cups_map_gray()' - Map a grayscale value to device colors.
 */

private void
cups_map_gray(gx_device *pdev,		/* I - Device info */
              frac      g,		/* I - Grayscale value */
	      frac      *out)		/* O - Device colors */
{
#ifdef DEBUG2
  dprintf3("DEBUG2: cups_map_gray(%p, %d, %p)\n",
	   pdev, g, out);
#endif /* DEBUG2 */

 /*
  * Just use the CMYK mapper...
  */

  cups_map_cmyk(pdev, 0, 0, 0, frac_1 - g, out);
}


/*
 * 'cups_map_rgb()' - Map a RGB color value to device colors.
 */

private void
cups_map_rgb(gx_device             *pdev,
					/* I - Device info */
             const gs_imager_state *pis,/* I - Device state */
             frac                  r,	/* I - Red value */
	     frac                  g,	/* I - Green value */
	     frac                  b,	/* I - Blue value */
	     frac                  *out)/* O - Device colors */
{
  frac		c, m, y, k;		/* CMYK values */
  frac		mk;			/* Maximum K value */
  int		tc, tm, ty;		/* Temporary color values */


#ifdef DEBUG
  dprintf6("DEBUG2: cups_map_rgb(%p, %p, %d, %d, %d, %p)\n",
	   pdev, pis, r, g, b, out);
#endif /* DEBUG */

 /*
  * Compute CMYK values...
  */

  c = frac_1 - r;
  m = frac_1 - g;
  y = frac_1 - b;
  k = min(c, min(m, y));

  if ((mk = max(c, max(m, y))) > k)
    k = (int)((float)k * (float)k * (float)k / ((float)mk * (float)mk));

  c -= k;
  m -= k;
  y -= k;

 /*
  * Do color correction as needed...
  */

  if (cups->HaveProfile)
  {
   /*
    * Color correct CMY...
    */

    tc = cups->Matrix[0][0][c] +
         cups->Matrix[0][1][m] +
	 cups->Matrix[0][2][y];
    tm = cups->Matrix[1][0][c] +
         cups->Matrix[1][1][m] +
	 cups->Matrix[1][2][y];
    ty = cups->Matrix[2][0][c] +
         cups->Matrix[2][1][m] +
	 cups->Matrix[2][2][y];

    if (tc < 0)
      c = 0;
    else if (tc > frac_1)
      c = frac_1;
    else
      c = (frac)tc;

    if (tm < 0)
      m = 0;
    else if (tm > frac_1)
      m = frac_1;
    else
      m = (frac)tm;

    if (ty < 0)
      y = 0;
    else if (ty > frac_1)
      y = frac_1;
    else
      y = (frac)ty;
  }

 /*
  * Use the CMYK mapping function to produce the device colors...
  */

  cups_map_cmyk(pdev, c, m, y, k, out);
}
#else
/*
 * 'cups_map_cmyk_color()' - Map a CMYK color to a color index.
 *
 * This function is only called when a 4 or 6 color colorspace is
 * selected for output.  CMYK colors are *not* corrected but *are*
 * density adjusted.
 */

private gx_color_index			/* O - Color index */
cups_map_cmyk_color(gx_device      *pdev,
					/* I - Device info */
                    const gx_color_value cv[4])/* I - CMYK color values */
{
  gx_color_index	i;		/* Temporary index */
  gx_color_value	c, m, y, k;
  gx_color_value	ic, im, iy, ik;	/* Integral CMYK values */

  c = cv[0];
  m = cv[1];
  y = cv[2];
  k = cv[3];

#ifdef DEBUG
  dprintf5("DEBUG2: cups_map_cmyk_color(%p, %d, %d, %d, %d)\n", pdev,
	   c, m, y, k);
#endif /* DEBUG */

 /*
  * Setup the color info data as needed...
  */

  if (pdev->color_info.num_components == 0)
    cups_set_color_info(pdev);

 /*
  * Density correct...
  */

  if (cups->HaveProfile)
  {
    c = cups->Density[c];
    m = cups->Density[m];
    y = cups->Density[y];
    k = cups->Density[k];
  }

  ic = cups->EncodeLUT[c];
  im = cups->EncodeLUT[m];
  iy = cups->EncodeLUT[y];
  ik = cups->EncodeLUT[k];

 /*
  * Convert the CMYK color to a color index...
  */

  switch (cups->header.cupsColorSpace)
  {
    default :
        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((((ic << 1) | im) << 1) | iy) << 1) | ik;
              break;
          case 2 :
              i = (((((ic << 2) | im) << 2) | iy) << 2) | ik;
              break;
          case 4 :
              i = (((((ic << 4) | im) << 4) | iy) << 4) | ik;
              break;
          case 8 :
              i = (((((ic << 8) | im) << 8) | iy) << 8) | ik;
              break;
#ifdef GX_COLOR_INDEX_TYPE
	  case 16 :
	      i = (((((ic << 16) | im) << 16) | iy) << 16) | ik;
	      break;
#endif /* GX_COLOR_INDEX_TYPE */
        }
        break;

    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((((iy << 1) | im) << 1) | ic) << 1) | ik;
              break;
          case 2 :
              i = (((((iy << 2) | im) << 2) | ic) << 2) | ik;
              break;
          case 4 :
              i = (((((iy << 4) | im) << 4) | ic) << 4) | ik;
              break;
          case 8 :
              i = (((((iy << 8) | im) << 8) | ic) << 8) | ik;
              break;
#ifdef GX_COLOR_INDEX_TYPE
	  case 16 :
	      i = (((((iy << 16) | im) << 16) | ic) << 16) | ik;
	      break;
#endif /* GX_COLOR_INDEX_TYPE */
        }
        break;

    case CUPS_CSPACE_KCMYcm :
        if (cups->header.cupsBitsPerColor == 1)
	{
	  if (ik)
	    i = 32;
	  else
	    i = 0;

	  if (ic && im)
	    i |= 17;
	  else if (ic && iy)
	    i |= 6;
	  else if (im && iy)
	    i |= 12;
	  else if (ic)
	    i |= 16;
	  else if (im)
	    i |= 8;
	  else if (iy)
	    i |= 4;
	  break;
	}

    case CUPS_CSPACE_KCMY :
        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((((ik << 1) | ic) << 1) | im) << 1) | iy;
              break;
          case 2 :
              i = (((((ik << 2) | ic) << 2) | im) << 2) | iy;
              break;
          case 4 :
              i = (((((ik << 4) | ic) << 4) | im) << 4) | iy;
              break;
          case 8 :
              i = (((((ik << 8) | ic) << 8) | im) << 8) | iy;
              break;
#ifdef GX_COLOR_INDEX_TYPE
	  case 16 :
	      i = (((((ik << 16) | ic) << 16) | im) << 16) | iy;
	      break;
#endif /* GX_COLOR_INDEX_TYPE */
        }
        break;
  }

#ifdef DEBUG
  dprintf9("DEBUG2: CMYK (%d,%d,%d,%d) -> CMYK %08x (%d,%d,%d,%d)\n",
	   c, m, y, k, (unsigned)i, ic, im, iy, ik);
#endif /* DEBUG */

 /*
  * Make sure we don't get a CMYK color of 255, 255, 255, 255...
  */

  if (i == gx_no_color_index)
    i --;

  return (i);
}


/*
 * 'cups_map_color_rgb()' - Map a color index to an RGB color.
 */

private int
cups_map_color_rgb(gx_device      *pdev,/* I - Device info */
                   gx_color_index color,/* I - Color index */
		   gx_color_value prgb[3])
					/* O - RGB values */
{
  unsigned char		c0, c1, c2, c3;	/* Color index components */
  gx_color_value	c, m, y, k, divk; /* Colors, Black & divisor */


#ifdef DEBUG
  dprintf3("DEBUG2: cups_map_color_rgb(%p, %d, %p)\n", pdev,
	   (unsigned)color, prgb);
#endif /* DEBUG */

 /*
  * Setup the color info data as needed...
  */

  if (pdev->color_info.num_components == 0)
    cups_set_color_info(pdev);

#ifdef DEBUG
  dprintf1("DEBUG2: COLOR %08x = ", (unsigned)color);
#endif /* DEBUG */

 /*
  * Extract the color components from the color index...
  */

  switch (cups->header.cupsBitsPerColor)
  {
    default :
        c3 = color & 1;
        color >>= 1;
        c2 = color & 1;
        color >>= 1;
        c1 = color & 1;
        color >>= 1;
        c0 = color;
        break;
    case 2 :
        c3 = color & 3;
        color >>= 2;
        c2 = color & 3;
        color >>= 2;
        c1 = color & 3;
        color >>= 2;
        c0 = color;
        break;
    case 4 :
        c3 = color & 15;
        color >>= 4;
        c2 = color & 15;
        color >>= 4;
        c1 = color & 15;
        color >>= 4;
        c0 = color;
        break;
    case 8 :
        c3 = color & 255;
        color >>= 8;
        c2 = color & 255;
        color >>= 8;
        c1 = color & 255;
        color >>= 8;
        c0 = color;
        break;
#ifdef GX_COLOR_INDEX_TYPE
    case 16 :
        c3 = color & 0xffff;
        color >>= 16;
        c2 = color & 0xffff;
        color >>= 16;
        c1 = color & 0xffff;
        color >>= 16;
        c0 = color;
        break;
#endif /* GX_COLOR_INDEX_TYPE */
  }

 /*
  * Convert the color components to RGB...
  */

  switch (cups->header.cupsColorSpace)
  {
    case CUPS_CSPACE_K :
    case CUPS_CSPACE_WHITE :
    case CUPS_CSPACE_GOLD :
    case CUPS_CSPACE_SILVER :
        prgb[0] =
        prgb[1] =
        prgb[2] = cups->DecodeLUT[c3];
        break;

    case CUPS_CSPACE_W :
        prgb[0] =
        prgb[1] =
        prgb[2] = cups->DecodeLUT[c3];
        break;

    case CUPS_CSPACE_RGB :
        prgb[0] = cups->DecodeLUT[c1];
        prgb[1] = cups->DecodeLUT[c2];
        prgb[2] = cups->DecodeLUT[c3];
        break;

    case CUPS_CSPACE_RGBA :
        prgb[0] = cups->DecodeLUT[c0];
        prgb[1] = cups->DecodeLUT[c1];
        prgb[2] = cups->DecodeLUT[c2];
        break;

    case CUPS_CSPACE_CMY :
        prgb[0] = cups->DecodeLUT[c1];
        prgb[1] = cups->DecodeLUT[c2];
        prgb[2] = cups->DecodeLUT[c3];
        break;

    case CUPS_CSPACE_YMC :
        prgb[0] = cups->DecodeLUT[c3];
        prgb[1] = cups->DecodeLUT[c2];
        prgb[2] = cups->DecodeLUT[c1];
        break;

    case CUPS_CSPACE_KCMY :
    case CUPS_CSPACE_KCMYcm :
        k    = cups->DecodeLUT[c0];
        divk = gx_max_color_value - k;
        if (divk == 0)
        {
          prgb[0] = 0;
          prgb[1] = 0;
          prgb[2] = 0;
        }
        else
        {
          prgb[0] = gx_max_color_value + divk -
                    gx_max_color_value * c1 / divk;
          prgb[1] = gx_max_color_value + divk -
                    gx_max_color_value * c2 / divk;
          prgb[2] = gx_max_color_value + divk -
                    gx_max_color_value * c3 / divk;
        }
        break;

    case CUPS_CSPACE_RGBW :
       /*
        * cups->DecodeLUT actually maps to RGBW, not CMYK...
	*/

        k = cups->DecodeLUT[c3];
        c = cups->DecodeLUT[c0] + k;
        m = cups->DecodeLUT[c1] + k;
        y = cups->DecodeLUT[c2] + k;

        if (c > gx_max_color_value)
	  prgb[0] = gx_max_color_value;
	else
	  prgb[0] = c;

        if (m > gx_max_color_value)
	  prgb[1] = gx_max_color_value;
	else
	  prgb[1] = m;

        if (y > gx_max_color_value)
	  prgb[2] = gx_max_color_value;
	else
	  prgb[2] = y;
        break;

    case CUPS_CSPACE_CMYK :
        k    = cups->DecodeLUT[c3];
        divk = gx_max_color_value - k;
        if (divk == 0)
        {
          prgb[0] = 0;
          prgb[1] = 0;
          prgb[2] = 0;
        }
        else
        {
          prgb[0] = gx_max_color_value + divk -
                    gx_max_color_value * c0 / divk;
          prgb[1] = gx_max_color_value + divk -
                    gx_max_color_value * c1 / divk;
          prgb[2] = gx_max_color_value + divk -
                    gx_max_color_value * c2 / divk;
        }
        break;

    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
        k    = cups->DecodeLUT[c3];
        divk = gx_max_color_value - k;
        if (divk == 0)
        {
          prgb[0] = 0;
          prgb[1] = 0;
          prgb[2] = 0;
        }
        else
        {
          prgb[0] = gx_max_color_value + divk -
                    gx_max_color_value * c2 / divk;
          prgb[1] = gx_max_color_value + divk -
                    gx_max_color_value * c1 / divk;
          prgb[2] = gx_max_color_value + divk -
                    gx_max_color_value * c0 / divk;
        }
        break;

#  ifdef CUPS_RASTER_HAVE_COLORIMETRIC
    case CUPS_CSPACE_CIEXYZ :
    case CUPS_CSPACE_CIELab :
    case CUPS_CSPACE_ICC1 :
    case CUPS_CSPACE_ICC2 :
    case CUPS_CSPACE_ICC3 :
    case CUPS_CSPACE_ICC4 :
    case CUPS_CSPACE_ICC5 :
    case CUPS_CSPACE_ICC6 :
    case CUPS_CSPACE_ICC7 :
    case CUPS_CSPACE_ICC8 :
    case CUPS_CSPACE_ICC9 :
    case CUPS_CSPACE_ICCA :
    case CUPS_CSPACE_ICCB :
    case CUPS_CSPACE_ICCC :
    case CUPS_CSPACE_ICCD :
    case CUPS_CSPACE_ICCE :
    case CUPS_CSPACE_ICCF :
        break;
#  endif /* CUPS_RASTER_HAVE_COLORIMETRIC */
  }

#ifdef DEBUG
  dprintf3("DEBUG2: RGB values: %d,%d,%d\n", prgb[0], prgb[1], prgb[2]);
#endif /* DEBUG */

  return (0);
}


/*
 * 'cups_map_rgb_color()' - Map an RGB color to a color index.  We map the
 *                          RGB color to the output colorspace & bits (we
 *                          figure out the format when we output a page).
 */

private gx_color_index			/* O - Color index */
cups_map_rgb_color(gx_device      *pdev,/* I - Device info */
                   const gx_color_value cv[3])/* I - RGB color values */
{
  gx_color_index	i;		/* Temporary index */
  gx_color_value        r, g, b;
  gx_color_value	ic, im, iy, ik;	/* Integral CMYK values */
  gx_color_value	mk;		/* Maximum K value */
  int			tc, tm, ty;	/* Temporary color values */
  float			rr, rg, rb,	/* Real RGB colors */
			ciex, ciey, ciez,
					/* CIE XYZ colors */
			ciey_yn,	/* Normalized luminance */
			ciel, ciea, cieb;
					/* CIE Lab colors */

  r = cv[0];
  g = cv[1];
  b = cv[2];

#ifdef DEBUG
  dprintf4("DEBUG2: cups_map_rgb_color(%p, %d, %d, %d)\n", pdev, r, g, b);
#endif /* DEBUG */

 /*
  * Setup the color info data as needed...
  */

  if (pdev->color_info.num_components == 0)
    cups_set_color_info(pdev);

 /*
  * Do color correction as needed...
  */

  if (cups->HaveProfile)
  {
   /*
    * Compute CMYK values...
    */

    ic = gx_max_color_value - r;
    im = gx_max_color_value - g;
    iy = gx_max_color_value - b;
    ik = min(ic, min(im, iy));

    if ((mk = max(ic, max(im, iy))) > ik)
      ik = (int)((float)ik * (float)ik * (float)ik / ((float)mk * (float)mk));

    ic -= ik;
    im -= ik;
    iy -= ik;

   /*
    * Color correct CMY...
    */

    tc = cups->Matrix[0][0][ic] +
         cups->Matrix[0][1][im] +
	 cups->Matrix[0][2][iy] +
	 ik;
    tm = cups->Matrix[1][0][ic] +
         cups->Matrix[1][1][im] +
	 cups->Matrix[1][2][iy] +
	 ik;
    ty = cups->Matrix[2][0][ic] +
         cups->Matrix[2][1][im] +
	 cups->Matrix[2][2][iy] +
	 ik;

   /*
    * Density correct combined CMYK...
    */

    if (tc < 0)
      r = gx_max_color_value;
    else if (tc > gx_max_color_value)
      r = gx_max_color_value - cups->Density[gx_max_color_value];
    else
      r = gx_max_color_value - cups->Density[tc];

    if (tm < 0)
      g = gx_max_color_value;
    else if (tm > gx_max_color_value)
      g = gx_max_color_value - cups->Density[gx_max_color_value];
    else
      g = gx_max_color_value - cups->Density[tm];

    if (ty < 0)
      b = gx_max_color_value;
    else if (ty > gx_max_color_value)
      b = gx_max_color_value - cups->Density[gx_max_color_value];
    else
      b = gx_max_color_value - cups->Density[ty];
  }

 /*
  * Convert the RGB color to a color index...
  */

  switch (cups->header.cupsColorSpace)
  {
    case CUPS_CSPACE_W :
        i = cups->EncodeLUT[(r * 31 + g * 61 + b * 8) / 100];
        break;

    case CUPS_CSPACE_RGB :
        ic = cups->EncodeLUT[r];
        im = cups->EncodeLUT[g];
        iy = cups->EncodeLUT[b];

        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((ic << 1) | im) << 1) | iy;
              break;
          case 2 :
              i = (((ic << 2) | im) << 2) | iy;
              break;
          case 4 :
              i = (((ic << 4) | im) << 4) | iy;
              break;
          case 8 :
              i = (((ic << 8) | im) << 8) | iy;
              break;
#ifdef GX_COLOR_INDEX_TYPE
	  case 16 :
	      i = (((ic << 16) | im) << 16) | iy;
	      break;
#endif /* GX_COLOR_INDEX_TYPE */
        }
        break;

    case CUPS_CSPACE_RGBW :
        if (!r && !g && !b)
	{
	 /*
	  * Map black to W...
	  */

          switch (cups->header.cupsBitsPerColor)
          {
            default :
        	i = 0x0e;
        	break;
            case 2 :
        	i = 0xfc;
        	break;
            case 4 :
        	i = 0xfff0;
        	break;
            case 8 :
        	i = 0xffffff00;
        	break;
#ifdef GX_COLOR_INDEX_TYPE
	    case 16 :
		i = 0xffffffffffff0000;
		break;
#endif /* GX_COLOR_INDEX_TYPE */
          }
	  break;
	}

    case CUPS_CSPACE_RGBA :
        ic = cups->EncodeLUT[r];
        im = cups->EncodeLUT[g];
        iy = cups->EncodeLUT[b];

        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((((ic << 1) | im) << 1) | iy) << 1) | 0x01;
              break;
          case 2 :
              i = (((((ic << 2) | im) << 2) | iy) << 2) | 0x03;
              break;
          case 4 :
              i = (((((ic << 4) | im) << 4) | iy) << 4) | 0x0f;
              break;
          case 8 :
              i = (((((ic << 8) | im) << 8) | iy) << 8) | 0xff;
              break;
#ifdef GX_COLOR_INDEX_TYPE
	  case 16 :
	      i = (((((ic << 16) | im) << 16) | iy) << 16) | 0xffff;
	      break;
#endif /* GX_COLOR_INDEX_TYPE */
        }
        break;

    default :
        i = cups->EncodeLUT[gx_max_color_value - (r * 31 + g * 61 + b * 8) / 100];
        break;

    case CUPS_CSPACE_CMY :
        ic = cups->EncodeLUT[gx_max_color_value - r];
        im = cups->EncodeLUT[gx_max_color_value - g];
        iy = cups->EncodeLUT[gx_max_color_value - b];

        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((ic << 1) | im) << 1) | iy;
              break;
          case 2 :
              i = (((ic << 2) | im) << 2) | iy;
              break;
          case 4 :
              i = (((ic << 4) | im) << 4) | iy;
              break;
          case 8 :
              i = (((ic << 8) | im) << 8) | iy;
              break;
#ifdef GX_COLOR_INDEX_TYPE
	  case 16 :
	      i = (((ic << 16) | im) << 16) | iy;
	      break;
#endif /* GX_COLOR_INDEX_TYPE */
        }
        break;

    case CUPS_CSPACE_YMC :
        ic = cups->EncodeLUT[gx_max_color_value - r];
        im = cups->EncodeLUT[gx_max_color_value - g];
        iy = cups->EncodeLUT[gx_max_color_value - b];

        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((iy << 1) | im) << 1) | ic;
              break;
          case 2 :
              i = (((iy << 2) | im) << 2) | ic;
              break;
          case 4 :
              i = (((iy << 4) | im) << 4) | ic;
              break;
          case 8 :
              i = (((iy << 8) | im) << 8) | ic;
              break;
        }
        break;

    case CUPS_CSPACE_CMYK :
	ic = gx_max_color_value - r;
	im = gx_max_color_value - g;
	iy = gx_max_color_value - b;
        ik = min(ic, min(im, iy));

	if ((mk = max(ic, max(im, iy))) > ik)
	  ik = (int)((float)ik * (float)ik * (float)ik /
	             ((float)mk * (float)mk));

        ic = cups->EncodeLUT[ic - ik];
        im = cups->EncodeLUT[im - ik];
        iy = cups->EncodeLUT[iy - ik];
        ik = cups->EncodeLUT[ik];

        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((((ic << 1) | im) << 1) | iy) << 1) | ik;
              break;
          case 2 :
              i = (((((ic << 2) | im) << 2) | iy) << 2) | ik;
              break;
          case 4 :
              i = (((((ic << 4) | im) << 4) | iy) << 4) | ik;
              break;
          case 8 :
              i = (((((ic << 8) | im) << 8) | iy) << 8) | ik;
              break;
#ifdef GX_COLOR_INDEX_TYPE
	  case 16 :
	      i = (((((ic << 16) | im) << 16) | iy) << 16) | ik;
	      break;
#endif /* GX_COLOR_INDEX_TYPE */
        }

#ifdef DEBUG
	dprintf8("DEBUG2: CMY (%d,%d,%d) -> CMYK %08x (%d,%d,%d,%d)\n",
		 r, g, b, (unsigned)i, ic, im, iy, ik);
#endif /* DEBUG */
        break;

    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
	ic = gx_max_color_value - r;
	im = gx_max_color_value - g;
	iy = gx_max_color_value - b;
        ik = min(ic, min(im, iy));

	if ((mk = max(ic, max(im, iy))) > ik)
	  ik = (int)((float)ik * (float)ik * (float)ik /
	             ((float)mk * (float)mk));

        ic = cups->EncodeLUT[ic - ik];
        im = cups->EncodeLUT[im - ik];
        iy = cups->EncodeLUT[iy - ik];
        ik = cups->EncodeLUT[ik];

        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((((iy << 1) | im) << 1) | ic) << 1) | ik;
              break;
          case 2 :
              i = (((((iy << 2) | im) << 2) | ic) << 2) | ik;
              break;
          case 4 :
              i = (((((iy << 4) | im) << 4) | ic) << 4) | ik;
              break;
          case 8 :
              i = (((((iy << 8) | im) << 8) | ic) << 8) | ik;
              break;
#ifdef GX_COLOR_INDEX_TYPE
	  case 16 :
	      i = (((((iy << 16) | im) << 16) | ic) << 16) | ik;
	      break;
#endif /* GX_COLOR_INDEX_TYPE */
        }
        break;

    case CUPS_CSPACE_KCMYcm :
        if (cups->header.cupsBitsPerColor == 1)
	{
	  ic = gx_max_color_value - r;
	  im = gx_max_color_value - g;
	  iy = gx_max_color_value - b;
          ik = min(ic, min(im, iy));

	  if ((mk = max(ic, max(im, iy))) > ik)
	    ik = (int)((float)ik * (float)ik * (float)ik /
	               ((float)mk * (float)mk));

          ic = cups->EncodeLUT[ic - ik];
          im = cups->EncodeLUT[im - ik];
          iy = cups->EncodeLUT[iy - ik];
          ik = cups->EncodeLUT[ik];
	  if (ik)
	    i = 32;
	  else if (ic && im)
	    i = 17;
	  else if (ic && iy)
	    i = 6;
	  else if (im && iy)
	    i = 12;
	  else if (ic)
	    i = 16;
	  else if (im)
	    i = 8;
	  else if (iy)
	    i = 4;
	  else
	    i = 0;
	  break;
	}

    case CUPS_CSPACE_KCMY :
	ic = gx_max_color_value - r;
	im = gx_max_color_value - g;
	iy = gx_max_color_value - b;
        ik = min(ic, min(im, iy));

	if ((mk = max(ic, max(im, iy))) > ik)
	  ik = (int)((float)ik * (float)ik * (float)ik /
	             ((float)mk * (float)mk));

        ic = cups->EncodeLUT[ic - ik];
        im = cups->EncodeLUT[im - ik];
        iy = cups->EncodeLUT[iy - ik];
        ik = cups->EncodeLUT[ik];

        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((((ik << 1) | ic) << 1) | im) << 1) | iy;
              break;
          case 2 :
              i = (((((ik << 2) | ic) << 2) | im) << 2) | iy;
              break;
          case 4 :
              i = (((((ik << 4) | ic) << 4) | im) << 4) | iy;
              break;
          case 8 :
              i = (((((ik << 8) | ic) << 8) | im) << 8) | iy;
              break;
#ifdef GX_COLOR_INDEX_TYPE
	  case 16 :
	      i = (((((ik << 16) | ic) << 16) | im) << 16) | iy;
	      break;
#endif /* GX_COLOR_INDEX_TYPE */
        }
        break;

#  ifdef CUPS_RASTER_HAVE_COLORIMETRIC
    case CUPS_CSPACE_CIEXYZ :
    case CUPS_CSPACE_CIELab :
    case CUPS_CSPACE_ICC1 :
    case CUPS_CSPACE_ICC2 :
    case CUPS_CSPACE_ICC3 :
    case CUPS_CSPACE_ICC4 :
    case CUPS_CSPACE_ICC5 :
    case CUPS_CSPACE_ICC6 :
    case CUPS_CSPACE_ICC7 :
    case CUPS_CSPACE_ICC8 :
    case CUPS_CSPACE_ICC9 :
    case CUPS_CSPACE_ICCA :
    case CUPS_CSPACE_ICCB :
    case CUPS_CSPACE_ICCC :
    case CUPS_CSPACE_ICCD :
    case CUPS_CSPACE_ICCE :
    case CUPS_CSPACE_ICCF :
       /*
        * Convert sRGB to linear RGB...
	*/

	rr = pow(((double)r / (double)gx_max_color_value + 0.055) / 1.055, 2.4);
	rg = pow(((double)g / (double)gx_max_color_value + 0.055) / 1.055, 2.4);
	rb = pow(((double)b / (double)gx_max_color_value + 0.055) / 1.055, 2.4);

       /*
        * Convert to CIE XYZ...
	*/

	ciex = 0.412453 * rr + 0.357580 * rg + 0.180423 * rb;
	ciey = 0.212671 * rr + 0.715160 * rg + 0.072169 * rb;
	ciez = 0.019334 * rr + 0.119193 * rg + 0.950227 * rb;

        if (cups->header.cupsColorSpace == CUPS_CSPACE_CIEXYZ)
	{
	 /*
	  * Convert to an integer XYZ color value...
	  */

          if (ciex > 1.1)
	    ic = 255;
	  else if (ciex > 0.0)
	    ic = (int)(ciex / 1.1 * 255.0 + 0.5);
	  else
	    ic = 0;

          if (ciey > 1.1)
	    im = 255;
	  else if (ciey > 0.0)
	    im = (int)(ciey / 1.1 * 255.0 + 0.5);
	  else
	    im = 0;

          if (ciez > 1.1)
	    iy = 255;
	  else if (ciez > 0.0)
	    iy = (int)(ciez / 1.1 * 255.0 + 0.5);
	  else
	    iy = 0;
	}
	else
	{
	 /*
	  * Convert CIE XYZ to Lab...
	  */

	  ciey_yn = ciey / D65_Y;

	  if (ciey_yn > 0.008856)
	    ciel = 116 * cbrt(ciey_yn) - 16;
	  else
	    ciel = 903.3 * ciey_yn;

	  ciea = 500 * (cups_map_cielab(ciex, D65_X) -
	                cups_map_cielab(ciey, D65_Y));
	  cieb = 200 * (cups_map_cielab(ciey, D65_Y) -
	                cups_map_cielab(ciez, D65_Z));

         /*
	  * Scale the L value and bias the a and b values by 128
	  * so that all values are in the range of 0 to 255.
	  */

	  ciel = ciel * 2.55 + 0.5;
	  ciea += 128.5;
	  cieb += 128.5;

         /*
	  * Convert to 8-bit values...
	  */

          if (ciel < 0.0)
	    ic = 0;
	  else if (ciel < 255.0)
	    ic = (int)ciel;
	  else
	    ic = 255;

          if (ciea < 0.0)
	    im = 0;
	  else if (ciea < 255.0)
	    im = (int)ciea;
	  else
	    im = 255;

          if (cieb < 0.0)
	    iy = 0;
	  else if (cieb < 255.0)
	    iy = (int)cieb;
	  else
	    iy = 255;
	}

       /*
        * Put the final color value together...
	*/

        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((ic << 1) | im) << 1) | iy;
              break;
          case 2 :
              i = (((ic << 2) | im) << 2) | iy;
              break;
          case 4 :
              i = (((ic << 4) | im) << 4) | iy;
              break;
          case 8 :
              i = (((ic << 8) | im) << 8) | iy;
              break;
#ifdef GX_COLOR_INDEX_TYPE
	  case 16 :
	      i = (((((ic << 16) | im) << 16) | iy) << 16) | ik;
	      break;
#endif /* GX_COLOR_INDEX_TYPE */
        }
        break;
#  endif /* CUPS_RASTER_HAVE_COLORIMETRIC */
  }

#ifdef DEBUG
  dprintf4("DEBUG2: RGB %d,%d,%d = %08x\n", r, g, b, (unsigned)i);
#endif /* DEBUG */

  return (i);
}
#endif /* dev_t_proc_encode_color */


/*
 * 'cups_open()' - Open the output file and initialize things.
 */

private int				/* O - Error status */
cups_open(gx_device *pdev)		/* I - Device info */
{
  int	code;				/* Return status */

#ifdef DEBUG
  dprintf1("DEBUG2: cups_open(%p)\n", pdev);
#endif /* DEBUG */

  dprintf("INFO: Start rendering...\n");
  cups->printer_procs.get_space_params = cups_get_space_params;

  if (cups->page == 0)
  {
    dprintf("INFO: Processing page 1...\n");
    cups->page = 1;
  }

  cups_set_color_info(pdev);

  if ((code = gdev_prn_open(pdev)) != 0)
    return (code);

  if (cups->PPD == NULL)
    cups->PPD = ppdOpenFile(getenv("PPD"));

  return (0);
}


/*
 * 'cups_print_pages()' - Send one or more pages to the output file.
 */

private int				/* O - 0 if everything is OK */
cups_print_pages(gx_device_printer *pdev,
					/* I - Device info */
                 FILE              *fp,	/* I - Output file */
		 int               num_copies)
					/* I - Number of copies */
{
  int		code = 0;		/* Error code */
  int		copy;			/* Copy number */
  int		srcbytes;		/* Byte width of scanline */
  unsigned char	*src,			/* Scanline data */
		*dst;			/* Bitmap data */
  ppd_attr_t    *RasterVersion = NULL;  /* CUPS Raster version read from PPD
					   file */

  (void)fp; /* reference unused file pointer to prevent compiler warning */

#ifdef DEBUG
  dprintf3("DEBUG2: cups_print_pages(%p, %p, %d)\n", pdev, fp,
          num_copies);
#endif /* DEBUG */

 /*
  * Figure out the number of bytes per line...
  */

  switch (cups->header.cupsColorOrder)
  {
    case CUPS_ORDER_CHUNKED :
        cups->header.cupsBytesPerLine = (cups->header.cupsBitsPerPixel *
	                                 cups->header.cupsWidth + 7) / 8;
        break;

    case CUPS_ORDER_BANDED :
        if (cups->header.cupsColorSpace == CUPS_CSPACE_KCMYcm &&
	    cups->header.cupsBitsPerColor == 1)
          cups->header.cupsBytesPerLine = (cups->header.cupsBitsPerColor *
                                           cups->header.cupsWidth + 7) / 8 * 6;
        else
          cups->header.cupsBytesPerLine = (cups->header.cupsBitsPerColor *
                                           cups->header.cupsWidth + 7) / 8 *
				          cups->color_info.num_components;
        break;

    case CUPS_ORDER_PLANAR :
        cups->header.cupsBytesPerLine = (cups->header.cupsBitsPerColor *
	                                 cups->header.cupsWidth + 7) / 8;
        break;
  }

 /*
  * Compute the width of a scanline and allocate input/output buffers...
  */

  srcbytes = gdev_prn_raster(pdev);

#ifdef DEBUG
  dprintf4("DEBUG2: cupsBitsPerPixel = %d, cupsWidth = %d, cupsBytesPerLine = %d, srcbytes = %d\n",
	   cups->header.cupsBitsPerPixel, cups->header.cupsWidth,
	   cups->header.cupsBytesPerLine, srcbytes);
#endif /* DEBUG */

  src = (unsigned char *)gs_malloc(pdev->memory->non_gc_memory, srcbytes, 1, "cups_print_pages");

  if (src == NULL)	/* can't allocate input buffer */
    return_error(gs_error_VMerror);

  memset(src, 0, srcbytes);

 /*
  * Need an output buffer, too...
  */

  dst = (unsigned char *)gs_malloc(pdev->memory->non_gc_memory, cups->header.cupsBytesPerLine, 2,
                                   "cups_print_pages");

  if (dst == NULL)	/* can't allocate working area */
    return_error(gs_error_VMerror);

  memset(dst, 0, 2 * cups->header.cupsBytesPerLine);

 /*
  * See if the stream has been initialized yet...
  */

  if (cups->stream == NULL)
  {
    RasterVersion = ppdFindAttr(cups->PPD, "cupsRasterVersion", NULL); 
    if (RasterVersion) {
#ifdef DEBUG
      dprintf1("DEBUG2: cupsRasterVersion = %s\n", RasterVersion->value);
#endif /* DEBUG */
      cups->cupsRasterVersion = atoi(RasterVersion->value);
      if ((cups->cupsRasterVersion != 2) &&
	  (cups->cupsRasterVersion != 3)) {
	dprintf1("ERROR: Unsupported CUPS Raster Version: %s",
		 RasterVersion->value);
	return_error(gs_error_unknownerror);
      }
    }
    if ((cups->stream = cupsRasterOpen(fileno(cups->file),
                                       (cups->cupsRasterVersion == 3 ?
					CUPS_RASTER_WRITE :
					CUPS_RASTER_WRITE_COMPRESSED))) == NULL)
    {
      perror("ERROR: Unable to open raster stream - ");
      return_error(gs_error_ioerror);
    }
  }

 /*
  * Output a page of graphics...
  */

  if (num_copies < 1)
    num_copies = 1;

  if (cups->PPD != NULL && !cups->PPD->manual_copies)
  {
    cups->header.NumCopies = num_copies;
    num_copies = 1;
  }

#ifdef DEBUG
  dprintf3("DEBUG2: cupsWidth = %d, cupsHeight = %d, cupsBytesPerLine = %d\n",
	   cups->header.cupsWidth, cups->header.cupsHeight,
	   cups->header.cupsBytesPerLine);
#endif /* DEBUG */

  for (copy = num_copies; copy > 0; copy --)
  {
    cupsRasterWriteHeader(cups->stream, &(cups->header));

    if (pdev->color_info.num_components == 1)
      code = cups_print_chunked(pdev, src, dst, srcbytes);
    else
      switch (cups->header.cupsColorOrder)
      {
	case CUPS_ORDER_CHUNKED :
            code = cups_print_chunked(pdev, src, dst, srcbytes);
	    break;
	case CUPS_ORDER_BANDED :
            code = cups_print_banded(pdev, src, dst, srcbytes);
	    break;
	case CUPS_ORDER_PLANAR :
            code = cups_print_planar(pdev, src, dst, srcbytes);
	    break;
      }
    if (code < 0)
      break;
  }

 /*
  * Free temporary storage and return...
  */

  gs_free(pdev->memory->non_gc_memory, (char *)src, srcbytes, 1, "cups_print_pages");
  gs_free(pdev->memory->non_gc_memory, (char *)dst, cups->header.cupsBytesPerLine, 1, "cups_print_pages");

  if (code < 0)
    return (code);
 
  cups->page ++;
  dprintf1("INFO: Processing page %d...\n", cups->page);

  return (0);
}


/*
 * 'cups_put_params()' - Set pagedevice parameters.
 */

private int				/* O - Error status */
cups_put_params(gx_device     *pdev,	/* I - Device info */
                gs_param_list *plist)	/* I - Parameter list */
{
  int			i;		/* Looping var */
#ifdef CUPS_RASTER_SYNCv1
  char			name[255];	/* Name of attribute */
  float			sf;		/* cupsBorderlessScalingFactor */
#endif /* CUPS_RASTER_SYNCv1 */
  float			margins[4];	/* Physical margins of print */
  ppd_size_t		*size;		/* Page size */
  int			code;		/* Error code */
  int			intval;		/* Integer value */
  bool			boolval;	/* Boolean value */
  float			floatval;	/* Floating point value */
  gs_param_string	stringval;	/* String value */
  gs_param_float_array	arrayval;	/* Float array value */
  int			margins_set;	/* Were the margins set? */
  int			size_set;	/* Was the size set? */
  int			color_set;	/* Were the color attrs set? */
  gdev_prn_space_params	sp_old;	        /* Space parameter data */
  int			width,		/* New width of page */
                        height,		/* New height of page */
                        width_old = 0,  /* Previous width of page */
                        height_old = 0; /* Previous height of page */
  bool                  transp_old = 0; /* Previous transparency usage state */
  ppd_attr_t            *backside = NULL,
                        *backsiderequiresflippedmargins = NULL;
  float                 swap;
  int                   xflip = 0,
                        yflip = 0;
  int                   found = 0;

#ifdef DEBUG
  dprintf2("DEBUG2: cups_put_params(%p, %p)\n", pdev, plist);
#endif /* DEBUG */

 /*
  * Process other options for CUPS...
  */

#define stringoption(name, sname) \
  if ((code = param_read_string(plist, sname, &stringval)) < 0) \
  { \
    dprintf1("ERROR: Error setting %s...\n", sname);	      \
    param_signal_error(plist, sname, code); \
    return (code); \
  } \
  else if (code == 0) \
  { \
    strncpy(cups->header.name, (const char *)(stringval.data),	\
            stringval.size); \
    cups->header.name[stringval.size] = '\0'; \
  }

#define intoption(name, sname, type) \
  if ((code = param_read_int(plist, sname, &intval)) < 0) \
  { \
    dprintf1("ERROR: Error setting %s ...\n", sname); \
    param_signal_error(plist, sname, code); \
    return (code); \
  } \
  else if (code == 0) \
  { \
    cups->header.name = (type)intval; \
  }

#define floatoption(name, sname) \
  if ((code = param_read_float(plist, sname, &floatval)) < 0) \
  { \
    dprintf1("ERROR: Error setting %s ...\n", sname); \
    param_signal_error(plist, sname, code); \
    return (code); \
  } \
  else if (code == 0) \
  { \
    cups->header.name = (float)floatval; \
  }

#define booloption(name, sname) \
  if ((code = param_read_bool(plist, sname, &boolval)) < 0) \
  { \
    if ((code = param_read_null(plist, sname)) < 0) \
    { \
      dprintf1("ERROR: Error setting %s ...\n", sname); \
      param_signal_error(plist, sname, code); \
      return (code); \
    } \
    if (code == 0) \
      cups->header.name = CUPS_FALSE; \
  } \
  else if (code == 0) \
  { \
    cups->header.name = (cups_bool_t)boolval; \
  }

#define arrayoption(name, sname, count) \
  if ((code = param_read_float_array(plist, sname, &arrayval)) < 0) \
  { \
    if ((code = param_read_null(plist, sname)) < 0) \
    { \
      dprintf1("ERROR: Error setting %s...\n", sname); \
      param_signal_error(plist, sname, code); \
      return (code); \
    } \
    if (code == 0) \
    { \
      for (i = 0; i < count; i ++) \
	cups->header.name[i] = 0; \
    } \
  } \
  else if (code == 0) \
  { \
    for (i = 0; i < count; i ++) { \
      cups->header.name[i] = (unsigned)(arrayval.data[i]); \
    } \
  }

  sp_old = ((gx_device_printer *)pdev)->space_params;
  width_old = pdev->width;
  height_old = pdev->height;
  transp_old = cups->page_uses_transparency;
  size_set    = param_read_float_array(plist, ".MediaSize", &arrayval) == 0 ||
                param_read_float_array(plist, "PageSize", &arrayval) == 0;
  margins_set = param_read_float_array(plist, "Margins", &arrayval) == 0;
  color_set   = param_read_int(plist, "cupsColorSpace", &intval) == 0 ||
                param_read_int(plist, "cupsBitsPerColor", &intval) == 0;
  /* We set the old dimensions to 1 if we have a color depth change, so
     that memory reallocation gets forced. This is perhaps not the correct
     approach to prevent crashes like in bug 690435. We keep it for the
     time being until we decide finally */
  if (color_set) {
    width_old = 1;
    height_old = 1;
  }
  /* We also recompute page size and margins if we simply get onto a new
     page without necessarily having a page size change in the PostScript
     code, as for some printers margins have to be flipped on the back sides of
     the sheets (even pages) when printing duplex */
  if (cups->page != cups->lastpage) {
    size_set = 1;
    cups->lastpage = cups->page;
  }

  stringoption(MediaClass, "MediaClass")
  stringoption(MediaColor, "MediaColor")
  stringoption(MediaType, "MediaType")
  stringoption(OutputType, "OutputType")
  intoption(AdvanceDistance, "AdvanceDistance", unsigned)
  intoption(AdvanceMedia, "AdvanceMedia", cups_adv_t)
  booloption(Collate, "Collate")
  intoption(CutMedia, "CutMedia", cups_cut_t)
  booloption(Duplex, "Duplex")
  arrayoption(ImagingBoundingBox, "ImagingBoundingBox", 4)
  booloption(InsertSheet, "InsertSheet")
  intoption(Jog, "Jog", cups_jog_t)
  intoption(LeadingEdge, "LeadingEdge", cups_edge_t)
  arrayoption(Margins, "Margins", 2)
  booloption(ManualFeed, "ManualFeed")
  intoption(MediaPosition, "cupsMediaPosition", unsigned) /* Compatibility */
  intoption(MediaPosition, "MediaPosition", unsigned)
  intoption(MediaWeight, "MediaWeight", unsigned)
  booloption(MirrorPrint, "MirrorPrint")
  booloption(NegativePrint, "NegativePrint")
  intoption(Orientation, "Orientation", cups_orient_t)
  booloption(OutputFaceUp, "OutputFaceUp")
  booloption(Separations, "Separations")
  booloption(TraySwitch, "TraySwitch")
  booloption(Tumble, "Tumble")
  intoption(cupsMediaType, "cupsMediaType", unsigned)
  intoption(cupsBitsPerColor, "cupsBitsPerColor", unsigned)
  intoption(cupsColorOrder, "cupsColorOrder", cups_order_t)
  intoption(cupsColorSpace, "cupsColorSpace", cups_cspace_t)
  intoption(cupsCompression, "cupsCompression", unsigned)
  intoption(cupsRowCount, "cupsRowCount", unsigned)
  intoption(cupsRowFeed, "cupsRowFeed", unsigned)
  intoption(cupsRowStep, "cupsRowStep", unsigned)

#ifdef GX_COLOR_INDEX_TYPE
 /*
  * Support cupsPreferredBitsPerColor - basically, allows you to
  * request 16-bits per color in a backwards-compatible way...
  */

  if (!param_read_int(plist, "cupsPreferredBitsPerColor", &intval))
    if (intval > cups->header.cupsBitsPerColor && intval <= 16)
      cups->header.cupsBitsPerColor = intval;
#endif /* GX_COLOR_INDEX_TYPE */

#ifdef CUPS_RASTER_SYNCv1
  floatoption(cupsBorderlessScalingFactor, "cupsBorderlessScalingFactor");

  for (i = 0; i < 16; i ++)
  {
    sprintf(name, "cupsInteger%d", i);
    intoption(cupsInteger[i],strdup(name), unsigned)
  }

  for (i = 0; i < 16; i ++)
  {
    sprintf(name, "cupsReal%d", i);
    floatoption(cupsReal[i], strdup(name))
  }

  for (i = 0; i < 16; i ++)
  {
    sprintf(name, "cupsString%d", i);
    stringoption(cupsString[i], strdup(name))
  }

  stringoption(cupsMarkerType, "cupsMarkerType");
  stringoption(cupsRenderingIntent, "cupsRenderingIntent");
  stringoption(cupsPageSizeName, "cupsPageSizeName");
#endif /* CUPS_RASTER_SYNCv1 */

  if ((code = param_read_string(plist, "cupsProfile", &stringval)) < 0)
  {
    param_signal_error(plist, "cupsProfile", code);
    return (code);
  }
  else if (code == 0)
  {
    if (cups->Profile != NULL)
      free(cups->Profile);

    cups->Profile = strdup((char *)stringval.data);
  }

  cups_set_color_info(pdev);

  /*
  * Then process standard page device options...
  */

  if ((code = gdev_prn_put_params(pdev, plist)) < 0)
    return (code);

 /*
  * Update margins/sizes as needed...
  */

  if (size_set)
  {
   /*
    * Compute the page margins...
    */

#ifdef DEBUG
    dprintf2("DEBUG: Updating PageSize to [%.0f %.0f]...\n",
	     cups->MediaSize[0], cups->MediaSize[1]);
#endif /* DEBUG */

    memset(margins, 0, sizeof(margins));

    cups->landscape = 0;

    if (cups->PPD != NULL)
    {
#ifdef DEBUG
      dprintf1("DEBUG2: cups->header.Duplex = %d\n", cups->header.Duplex);
      dprintf1("DEBUG2: cups->header.Tumble = %d\n", cups->header.Tumble);
      dprintf1("DEBUG2: cups->page = %d\n", cups->page);
      dprintf1("DEBUG2: cups->PPD = %p\n", cups->PPD);
#endif /* DEBUG */

      backside = ppdFindAttr(cups->PPD, "cupsBackSide", NULL); 
      if (backside) {
#ifdef DEBUG
	dprintf1("DEBUG2: cupsBackSide = %s\n", backside->value);
#endif /* DEBUG */
	cups->PPD->flip_duplex = 0;
      }
#ifdef DEBUG
      dprintf1("DEBUG2: cups->PPD->flip_duplex = %d\n", cups->PPD->flip_duplex);
#endif /* DEBUG */

      backsiderequiresflippedmargins =
	ppdFindAttr(cups->PPD, "APDuplexRequiresFlippedMargin", NULL);
#ifdef DEBUG
      if (backsiderequiresflippedmargins)
	dprintf1("DEBUG2: APDuplexRequiresFlippedMargin = %s\n",
		 backsiderequiresflippedmargins->value);
#endif /* DEBUG */

      if (cups->header.Duplex &&
	  (cups->header.Tumble &&
	   (backside && !strcasecmp(backside->value, "Flipped"))) &&
	  !(cups->page & 1))
      {
	xflip = 1;
	if (backsiderequiresflippedmargins &&
	    !strcasecmp(backsiderequiresflippedmargins->value, "False")) {
#ifdef DEBUG
	  dprintf("DEBUG2: (1) Flip: X=1 Y=0\n");
#endif /* DEBUG */
	  yflip = 0;
	} else {
#ifdef DEBUG
	  dprintf("DEBUG2: (1) Flip: X=1 Y=1\n");
#endif /* DEBUG */
	  yflip = 1;
	}
      }
      else if (cups->header.Duplex &&
	       (!cups->header.Tumble &&
		(backside && !strcasecmp(backside->value, "Flipped"))) &&
	       !(cups->page & 1))
      {
	xflip = 0;
	if (backsiderequiresflippedmargins &&
	    !strcasecmp(backsiderequiresflippedmargins->value, "False")) {
#ifdef DEBUG
	  dprintf("DEBUG2: (2) Flip: X=0 Y=1\n");
#endif /* DEBUG */
	  yflip = 1;
	} else {
#ifdef DEBUG
	  dprintf("DEBUG2: (2) Flip: X=0 Y=0\n");
#endif /* DEBUG */
	  yflip = 0;
	}
      }
      else if (cups->header.Duplex &&
	       ((!cups->header.Tumble &&
		 (cups->PPD->flip_duplex ||
		  (backside && !strcasecmp(backside->value, "Rotated")))) ||
		(cups->header.Tumble &&
		 (backside && !strcasecmp(backside->value, "ManualTumble")))) &&
	       !(cups->page & 1))
      { 
	xflip = 1;
	if (backsiderequiresflippedmargins &&
	    !strcasecmp(backsiderequiresflippedmargins->value, "True")) {
#ifdef DEBUG
	  dprintf("DEBUG2: (3) Flip: X=1 Y=0\n");
#endif /* DEBUG */
	  yflip = 0;
	} else {
#ifdef DEBUG
	  dprintf("DEBUG2: (3) Flip: X=1 Y=1\n");
#endif /* DEBUG */
	  yflip = 1;
	}
      }
      else
      {
#ifdef DEBUG
	dprintf("DEBUG2: (4) Flip: X=0 Y=0\n");
#endif /* DEBUG */
	xflip = 0;
	yflip = 0;
      }

#ifdef CUPS_RASTER_SYNCv1
     /*
      * Chack whether cupsPageSizeName has a valid value
      */

      if (strlen(cups->header.cupsPageSizeName) != 0) {
	found = 0;
	for (i = cups->PPD->num_sizes, size = cups->PPD->sizes;
	     i > 0;
	     i --, size ++)
	  if (strcasecmp(cups->header.cupsPageSizeName, size->name) == 0) {
	    found = 1;
	    break;
	  }
	if (found == 0) cups->header.cupsPageSizeName[0] = '\0';
      }
#ifdef DEBUG
      dprintf1("DEBUG2: cups->header.cupsPageSizeName = %s\n", cups->header.cupsPageSizeName);
#endif /* DEBUG */
#endif /* CUPS_RASTER_SYNCv1 */

     /*
      * Find the matching page size...
      */

      for (i = cups->PPD->num_sizes, size = cups->PPD->sizes;
           i > 0;
           i --, size ++)
	if (fabs(cups->MediaSize[1] - size->length) < 5.0 &&
            fabs(cups->MediaSize[0] - size->width) < 5.0 &&
#ifdef CUPS_RASTER_SYNCv1
	    ((strlen(cups->header.cupsPageSizeName) == 0) ||
	     (strcasecmp(cups->header.cupsPageSizeName, size->name) == 0)) &&
#endif
	    /* We check whether all 4 margins match with the margin info
	       of the page size in the PPD. Here we check also for swapped
	       left/right and top/bottom margins as the cups->HWMargins
	       info can be from the previous page and there the margins
	       can be swapped due to duplex printing requirements */
	    (!margins_set ||
	     (((fabs(cups->HWMargins[0] - size->left) < 1.0 &&
		fabs(cups->HWMargins[2] - size->width + size->right) < 1.0) ||
	       (fabs(cups->HWMargins[0] - size->width + size->right) < 1.0 &&
		fabs(cups->HWMargins[2] - size->left) < 1.0)) &&
	      ((fabs(cups->HWMargins[1] - size->bottom) < 1.0 &&
		fabs(cups->HWMargins[3] - size->length + size->top) < 1.0) ||
	       (fabs(cups->HWMargins[1] - size->length + size->top) < 1.0 &&
		fabs(cups->HWMargins[3] - size->bottom) < 1.0)))))
	  break;

      if (i > 0)
      {
       /*
	* Standard size...
	*/

#ifdef DEBUG
	dprintf1("DEBUG: size = %s\n", size->name);
#endif /* DEBUG */

	gx_device_set_media_size(pdev, size->width, size->length);

	cups->landscape = 0;

	margins[0] = size->left / 72.0;
	margins[1] = size->bottom / 72.0;
	margins[2] = (size->width - size->right) / 72.0;
	margins[3] = (size->length - size->top) / 72.0;
	if (xflip == 1)
	{
	  swap = margins[0]; margins[0] = margins[2]; margins[2] = swap;
	}
	if (yflip == 1)
	{
	  swap = margins[1]; margins[1] = margins[3]; margins[3] = swap;
	}
      }
      else
      {
       /*
	* No matching portrait size; look for a matching size in
	* landscape orientation...
	*/

	for (i = cups->PPD->num_sizes, size = cups->PPD->sizes;
             i > 0;
             i --, size ++)
	  if (fabs(cups->MediaSize[0] - size->length) < 5.0 &&
              fabs(cups->MediaSize[1] - size->width) < 5.0 &&
#ifdef CUPS_RASTER_SYNCv1
	      ((strlen(cups->header.cupsPageSizeName) == 0) ||
	       (strcasecmp(cups->header.cupsPageSizeName, size->name) == 0)) &&
#endif
	      /* We check whether all 4 margins match with the margin info
		 of the page size in the PPD. Here we check also for swapped
		 left/right and top/bottom margins as the cups->HWMargins
		 info can be from the previous page and there the margins
		 can be swapped due to duplex printing requirements */
	      (!margins_set ||
	       (((fabs(cups->HWMargins[1] - size->left) < 1.0 &&
		  fabs(cups->HWMargins[3] - size->width + size->right) < 1.0) ||
		 (fabs(cups->HWMargins[1] - size->width + size->right) < 1.0 &&
		  fabs(cups->HWMargins[3] - size->left) < 1.0)) &&
		((fabs(cups->HWMargins[0] - size->bottom) < 1.0 &&
		  fabs(cups->HWMargins[2] - size->length + size->top) < 1.0) ||
		 (fabs(cups->HWMargins[0] - size->length + size->top) < 1.0 &&
		  fabs(cups->HWMargins[2] - size->bottom) < 1.0)))))
	    break;

	if (i > 0)
	{
	 /*
	  * Standard size in landscape orientation...
	  */

#ifdef DEBUG
	  dprintf1("DEBUG: landscape size = %s\n", size->name);
#endif /* DEBUG */

	  gx_device_set_media_size(pdev, size->length, size->width);

          cups->landscape = 1;

	  margins[0] = (size->length - size->top) / 72.0;
	  margins[1] = size->left / 72.0;
	  margins[2] = size->bottom / 72.0;
	  margins[3] = (size->width - size->right) / 72.0;
	  if (xflip == 1)
	  {
	    swap = margins[1]; margins[1] = margins[3]; margins[3] = swap;
	  }
	  if (yflip == 1)
	  {
	    swap = margins[0]; margins[0] = margins[2]; margins[2] = swap;
	  }
	}
	else
	{
	 /*
	  * Custom size...
	  */

#ifdef DEBUG
	  dprintf("DEBUG: size = Custom\n");
#endif /* DEBUG */

          cups->landscape = 0;

	  for (i = 0; i < 4; i ++)
            margins[i] = cups->PPD->custom_margins[i] / 72.0;
	  if (xflip == 1)
	  {
	    swap = margins[0]; margins[0] = margins[2]; margins[2] = swap;
	  }
	  if (yflip == 1)
	  {
	    swap = margins[1]; margins[1] = margins[3]; margins[3] = swap;
	  }
	}
      }

#ifdef DEBUG
      dprintf4("DEBUG: margins[] = [ %f %f %f %f ]\n",
	       margins[0], margins[1], margins[2], margins[3]);
#endif /* DEBUG */
    }

   /*
    * Set the margins to update the bitmap size...
    */

    gx_device_set_margins(pdev, margins, false);
  }

 /*
  * Reallocate memory if the size or color depth was changed...
  */

  if (color_set || size_set)
  {
   /*
    * Make sure the page image is the correct size - current Ghostscript
    * does not keep track of the margins in the bitmap size...
    */

    if (cups->landscape)
    {
      width  = (pdev->MediaSize[1] - pdev->HWMargins[1] - pdev->HWMargins[3]) *
               pdev->HWResolution[0] / 72.0f + 0.499f;
      height = (pdev->MediaSize[0] - pdev->HWMargins[0] - pdev->HWMargins[2]) *
               pdev->HWResolution[1] / 72.0f + 0.499f;
    }
    else
    {
      width  = (pdev->MediaSize[0] - pdev->HWMargins[0] - pdev->HWMargins[2]) *
               pdev->HWResolution[0] / 72.0f + 0.499f;
      height = (pdev->MediaSize[1] - pdev->HWMargins[1] - pdev->HWMargins[3]) *
               pdev->HWResolution[1] / 72.0f + 0.499f;
    }

#ifdef CUPS_RASTER_SYNCv1
    if (cups->header.cupsBorderlessScalingFactor > 1.0)
    {
      width  *= cups->header.cupsBorderlessScalingFactor;
      height *= cups->header.cupsBorderlessScalingFactor;
    }
#endif /* CUPS_RASTER_SYNCv1 */
    pdev->width  = width;
    pdev->height = height;

   /*
    * Don't reallocate memory unless the device has been opened...
    * Also reallocate only if the size has actually changed...
    */

    if (pdev->is_open)
    {

     /*
      * Device is open and size has changed, so reallocate...
      */

#ifdef DEBUG
      dprintf4("DEBUG2: Reallocating memory, [%.0f %.0f] = %dx%d pixels...\n",
	       pdev->MediaSize[0], pdev->MediaSize[1], width, height);
#endif /* DEBUG */

      if ((code = gdev_prn_maybe_realloc_memory((gx_device_printer *)pdev,
                                                &sp_old, 
						width_old, height_old,
						transp_old))
	  < 0)
	return (code);
#ifdef DEBUG
      dprintf4("DEBUG2: Reallocated memory, [%.0f %.0f] = %dx%d pixels...\n",
	       pdev->MediaSize[0], pdev->MediaSize[1], width, height);
#endif /* DEBUG */
    }
    else
    {
     /*
      * Device isn't yet open, so just save the new width and height...
      */

#ifdef DEBUG
      dprintf4("DEBUG: Setting initial media size, [%.0f %.0f] = %dx%d pixels...\n",
	       pdev->MediaSize[0], pdev->MediaSize[1], width, height);
#endif /* DEBUG */

      pdev->width  = width;
      pdev->height = height;
    }
  }

 /*
  * Set CUPS raster header values...
  */

  cups->header.HWResolution[0] = pdev->HWResolution[0];
  cups->header.HWResolution[1] = pdev->HWResolution[1];

#ifdef CUPS_RASTER_SYNCv1

  if (cups->landscape)
  {
    cups->header.cupsPageSize[0] = pdev->MediaSize[1];
    cups->header.cupsPageSize[1] = pdev->MediaSize[0];

    cups->header.cupsImagingBBox[0] = pdev->HWMargins[1];
    cups->header.cupsImagingBBox[1] = pdev->HWMargins[2];
    cups->header.cupsImagingBBox[2] = pdev->MediaSize[1] - pdev->HWMargins[3];
    cups->header.cupsImagingBBox[3] = pdev->MediaSize[0] - pdev->HWMargins[0];

    if ((sf = cups->header.cupsBorderlessScalingFactor) < 1.0)
      sf = 1.0;

    cups->header.Margins[0] = pdev->HWMargins[1] * sf;
    cups->header.Margins[1] = pdev->HWMargins[2] * sf;

    cups->header.PageSize[0] = pdev->MediaSize[1] * sf;
    cups->header.PageSize[1] = pdev->MediaSize[0] * sf;

    cups->header.ImagingBoundingBox[0] = pdev->HWMargins[1] * sf;
    cups->header.ImagingBoundingBox[1] = pdev->HWMargins[2] * sf;
    cups->header.ImagingBoundingBox[2] = (pdev->MediaSize[1] -
					  pdev->HWMargins[3]) * sf;
    cups->header.ImagingBoundingBox[3] = (pdev->MediaSize[0] -
					  pdev->HWMargins[0]) * sf;
  } 
  else
  {
    cups->header.cupsPageSize[0] = pdev->MediaSize[0];
    cups->header.cupsPageSize[1] = pdev->MediaSize[1];

    cups->header.cupsImagingBBox[0] = pdev->HWMargins[0];
    cups->header.cupsImagingBBox[1] = pdev->HWMargins[1];
    cups->header.cupsImagingBBox[2] = pdev->MediaSize[0] - pdev->HWMargins[2];
    cups->header.cupsImagingBBox[3] = pdev->MediaSize[1] - pdev->HWMargins[3];

    if ((sf = cups->header.cupsBorderlessScalingFactor) < 1.0)
      sf = 1.0;

    cups->header.Margins[0] = pdev->HWMargins[0] * sf;
    cups->header.Margins[1] = pdev->HWMargins[1] * sf;

    cups->header.PageSize[0] = pdev->MediaSize[0] * sf;
    cups->header.PageSize[1] = pdev->MediaSize[1] * sf;

    cups->header.ImagingBoundingBox[0] = pdev->HWMargins[0] * sf;
    cups->header.ImagingBoundingBox[1] = pdev->HWMargins[1] * sf;
    cups->header.ImagingBoundingBox[2] = (pdev->MediaSize[0] -
					  pdev->HWMargins[2]) * sf;
    cups->header.ImagingBoundingBox[3] = (pdev->MediaSize[1] -
					  pdev->HWMargins[3]) * sf;
  }

#else

  if (cups->landscape)
  {
    cups->header.Margins[0] = pdev->HWMargins[1];
    cups->header.Margins[1] = pdev->HWMargins[2];

    cups->header.PageSize[0] = pdev->MediaSize[1];
    cups->header.PageSize[1] = pdev->MediaSize[0];

    cups->header.ImagingBoundingBox[0] = pdev->HWMargins[1];
    cups->header.ImagingBoundingBox[1] = pdev->HWMargins[0];
    cups->header.ImagingBoundingBox[2] = pdev->MediaSize[1] - 
                                         pdev->HWMargins[3];
    cups->header.ImagingBoundingBox[3] = pdev->MediaSize[0] -
                                         pdev->HWMargins[2];
  } 
  else
  {
    cups->header.Margins[0] = pdev->HWMargins[0];
    cups->header.Margins[1] = pdev->HWMargins[1];

    cups->header.PageSize[0] = pdev->MediaSize[0];
    cups->header.PageSize[1] = pdev->MediaSize[1];

    cups->header.ImagingBoundingBox[0] = pdev->HWMargins[0];
    cups->header.ImagingBoundingBox[1] = pdev->HWMargins[3];
    cups->header.ImagingBoundingBox[2] = pdev->MediaSize[0] - 
                                         pdev->HWMargins[2];
    cups->header.ImagingBoundingBox[3] = pdev->MediaSize[1] -
                                         pdev->HWMargins[1];
  }

#endif /* CUPS_RASTER_SYNCv1 */
  cups->header.cupsWidth  = cups->width;
  cups->header.cupsHeight = cups->height;

#ifdef DEBUG
  dprintf1("DEBUG2: ppd = %p\n", cups->PPD);
  dprintf2("DEBUG2: PageSize = [ %.3f %.3f ]\n",
	   pdev->MediaSize[0], pdev->MediaSize[1]);
  if (size_set)
    dprintf4("DEBUG2: margins = [ %.3f %.3f %.3f %.3f ]\n",
	     margins[0], margins[1], margins[2], margins[3]);
  dprintf2("DEBUG2: HWResolution = [ %.3f %.3f ]\n",
	   pdev->HWResolution[0], pdev->HWResolution[1]);
  dprintf2("DEBUG2: width = %d, height = %d\n",
	   pdev->width, pdev->height);
  dprintf4("DEBUG2: HWMargins = [ %.3f %.3f %.3f %.3f ]\n",
	   pdev->HWMargins[0], pdev->HWMargins[1],
	   pdev->HWMargins[2], pdev->HWMargins[3]);
#endif /* DEBUG */

  return (0);
}


/*
 * 'cups_set_color_info()' - Set the color information structure based on
 *                           the required output.
 */

private void
cups_set_color_info(gx_device *pdev)	/* I - Device info */
{
  int		i, j, k;		/* Looping vars */
  int		max_lut;		/* Maximum LUT value */
  float		d, g;			/* Density and gamma correction */
  float		m[3][3];		/* Color correction matrix */
  char		resolution[41];		/* Resolution string */
  ppd_profile_t	*profile;		/* Color profile information */


#ifdef DEBUG
  dprintf1("DEBUG2: cups_set_color_info(%p)\n", pdev);
#endif /* DEBUG */

#ifndef GX_COLOR_INDEX_TYPE
  if (cups->header.cupsBitsPerColor > 8)
    cups->header.cupsBitsPerColor = 8;
#endif /* !GX_COLOR_INDEX_TYPE */

  switch (cups->header.cupsColorSpace)
  {
    default :
    case CUPS_CSPACE_W :
    case CUPS_CSPACE_K :
    case CUPS_CSPACE_WHITE :
    case CUPS_CSPACE_GOLD :
    case CUPS_CSPACE_SILVER :
#ifdef CUPS_RASTER_SYNCv1
	cups->header.cupsNumColors      = 1;
#endif /* CUPS_RASTER_SYNCv1 */
        cups->header.cupsBitsPerPixel   = cups->header.cupsBitsPerColor;
        cups->color_info.depth          = cups->header.cupsBitsPerPixel;
        cups->color_info.num_components = 1;
        cups->color_info.dither_grays = 1L << cups->header.cupsBitsPerColor;
        cups->color_info.dither_colors = 1L << cups->header.cupsBitsPerColor;
        cups->color_info.max_gray = cups->color_info.dither_grays - 1;
        cups->color_info.max_color = cups->color_info.dither_grays - 1;
        break;

    case CUPS_CSPACE_CMY :
    case CUPS_CSPACE_YMC :
    case CUPS_CSPACE_RGB :
#ifdef CUPS_RASTER_SYNCv1
	cups->header.cupsNumColors      = 3;
#endif /* CUPS_RASTER_SYNCv1 */
        if (cups->header.cupsColorOrder != CUPS_ORDER_CHUNKED)
          cups->header.cupsBitsPerPixel = cups->header.cupsBitsPerColor;
	else if (cups->header.cupsBitsPerColor < 8)
	  cups->header.cupsBitsPerPixel = 4 * cups->header.cupsBitsPerColor;
	else
	  cups->header.cupsBitsPerPixel = 3 * cups->header.cupsBitsPerColor;

	if (cups->header.cupsBitsPerColor < 8)
	  cups->color_info.depth = 4 * cups->header.cupsBitsPerColor;
	else
	  cups->color_info.depth = 3 * cups->header.cupsBitsPerColor;

        cups->color_info.num_components = 3;
        break;

    case CUPS_CSPACE_KCMYcm :
        if (cups->header.cupsBitsPerColor == 1)
	{
#ifdef CUPS_RASTER_SYNCv1
	  cups->header.cupsNumColors      = 6;
#endif /* CUPS_RASTER_SYNCv1 */
	  cups->header.cupsBitsPerPixel   = 8;
	  cups->color_info.depth          = 8;
	  cups->color_info.num_components = 4;
	  break;
	}

    case CUPS_CSPACE_RGBA :
    case CUPS_CSPACE_RGBW :
    case CUPS_CSPACE_CMYK :
    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_KCMY :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
#ifdef CUPS_RASTER_SYNCv1
	cups->header.cupsNumColors = 4;
#endif /* CUPS_RASTER_SYNCv1 */
        if (cups->header.cupsColorOrder != CUPS_ORDER_CHUNKED)
          cups->header.cupsBitsPerPixel = cups->header.cupsBitsPerColor;
	else
	  cups->header.cupsBitsPerPixel = 4 * cups->header.cupsBitsPerColor;

        cups->color_info.depth          = 4 * cups->header.cupsBitsPerColor;
        cups->color_info.num_components = 4;
        break;

#ifdef CUPS_RASTER_HAVE_COLORIMETRIC
    case CUPS_CSPACE_CIEXYZ :
    case CUPS_CSPACE_CIELab :
    case CUPS_CSPACE_ICC1 :
    case CUPS_CSPACE_ICC2 :
    case CUPS_CSPACE_ICC3 :
    case CUPS_CSPACE_ICC4 :
    case CUPS_CSPACE_ICC5 :
    case CUPS_CSPACE_ICC6 :
    case CUPS_CSPACE_ICC7 :
    case CUPS_CSPACE_ICC8 :
    case CUPS_CSPACE_ICC9 :
    case CUPS_CSPACE_ICCA :
    case CUPS_CSPACE_ICCB :
    case CUPS_CSPACE_ICCC :
    case CUPS_CSPACE_ICCD :
    case CUPS_CSPACE_ICCE :
    case CUPS_CSPACE_ICCF :
       /*
	* Colorimetric color spaces currently are implemented as 24-bit
	* mapping to XYZ or Lab, which are then converted as needed to
	* the final representation...
	*
	* This code enforces a minimum output depth of 8 bits per
	* component...
	*/

#ifdef CUPS_RASTER_SYNCv1
	cups->header.cupsNumColors = 3;
#endif /* CUPS_RASTER_SYNCv1 */

	if (cups->header.cupsBitsPerColor < 8)
          cups->header.cupsBitsPerColor = 8;

	if (cups->header.cupsColorOrder != CUPS_ORDER_CHUNKED)
          cups->header.cupsBitsPerPixel = cups->header.cupsBitsPerColor;
	else
          cups->header.cupsBitsPerPixel = 3 * cups->header.cupsBitsPerColor;

	cups->color_info.depth          = 24;
	cups->color_info.num_components = 3;
	break;
#endif /* CUPS_RASTER_HAVE_COLORIMETRIC */
  }

#ifdef dev_t_proc_encode_color
  switch (cups->header.cupsColorSpace)
  {
    default :
        cups->color_info.gray_index = GX_CINFO_COMP_NO_INDEX;
	break;

    case CUPS_CSPACE_W :
    case CUPS_CSPACE_WHITE :
    case CUPS_CSPACE_K :
    case CUPS_CSPACE_GOLD :
    case CUPS_CSPACE_SILVER :
    case CUPS_CSPACE_KCMYcm :
    case CUPS_CSPACE_KCMY :
        cups->color_info.gray_index = 0;
	break;

    case CUPS_CSPACE_RGBW :
    case CUPS_CSPACE_CMYK :
    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
        cups->color_info.gray_index = 3;
	break;
  }

  switch (cups->header.cupsColorSpace)
  {
    default :
    case CUPS_CSPACE_W :
    case CUPS_CSPACE_WHITE :
    case CUPS_CSPACE_RGB :
    case CUPS_CSPACE_RGBA :
    case CUPS_CSPACE_RGBW :
#  ifdef CUPS_RASTER_HAVE_COLORIMETRIC
    case CUPS_CSPACE_CIEXYZ :
    case CUPS_CSPACE_CIELab :
    case CUPS_CSPACE_ICC1 :
    case CUPS_CSPACE_ICC2 :
    case CUPS_CSPACE_ICC3 :
    case CUPS_CSPACE_ICC4 :
    case CUPS_CSPACE_ICC5 :
    case CUPS_CSPACE_ICC6 :
    case CUPS_CSPACE_ICC7 :
    case CUPS_CSPACE_ICC8 :
    case CUPS_CSPACE_ICC9 :
    case CUPS_CSPACE_ICCA :
    case CUPS_CSPACE_ICCB :
    case CUPS_CSPACE_ICCC :
    case CUPS_CSPACE_ICCD :
    case CUPS_CSPACE_ICCE :
    case CUPS_CSPACE_ICCF :
#  endif /* CUPS_RASTER_HAVE_COLORIMETRIC */
        cups->color_info.polarity = GX_CINFO_POLARITY_ADDITIVE;
        break;

    case CUPS_CSPACE_K :
    case CUPS_CSPACE_GOLD :
    case CUPS_CSPACE_SILVER :
    case CUPS_CSPACE_CMY :
    case CUPS_CSPACE_YMC :
    case CUPS_CSPACE_KCMYcm :
    case CUPS_CSPACE_CMYK :
    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_KCMY :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
        cups->color_info.polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
        break;
  }

  cups->color_info.separable_and_linear = GX_CINFO_SEP_LIN_NONE;
#endif /* dev_t_proc_encode_color */

  i       = cups->header.cupsBitsPerColor;
  max_lut = (1 << i) - 1;

  switch (cups->color_info.num_components)
  {
    default :
    case 1 :
	cups->color_info.max_gray      = max_lut;
	cups->color_info.max_color     = 0;
	cups->color_info.dither_grays  = max_lut + 1;
	cups->color_info.dither_colors = 0;
        break;

    case 3 :
	cups->color_info.max_gray      = 0;
	cups->color_info.max_color     = max_lut;
	cups->color_info.dither_grays  = 0;
	cups->color_info.dither_colors = max_lut + 1;
	break;

    case 4 :
	cups->color_info.max_gray      = max_lut;
	cups->color_info.max_color     = max_lut;
	cups->color_info.dither_grays  = max_lut + 1;
	cups->color_info.dither_colors = max_lut + 1;
	break;
  }

 /*
  * Enable/disable CMYK color support...
  */

#ifdef dev_t_proc_encode_color
  cups->color_info.max_components = cups->color_info.num_components;
#endif /* dev_t_proc_encode_color */

 /*
  * Tell Ghostscript to forget any colors it has cached...
  */

  gx_device_decache_colors(pdev);

 /*
  * Compute the lookup tables...
  */

  for (i = 0; i <= gx_max_color_value; i ++)
  {
    j = (max_lut * i + gx_max_color_value / 2) / gx_max_color_value;

#if !ARCH_IS_BIG_ENDIAN
    if (max_lut > 255)
      j = ((j & 255) << 8) | ((j >> 8) & 255);
#endif /* !ARCH_IS_BIG_ENDIAN */

    cups->EncodeLUT[i] = j;

#ifdef DEBUG
    if (i == 0 || cups->EncodeLUT[i] != cups->EncodeLUT[i - 1])
      dprintf2("DEBUG2: cups->EncodeLUT[%d] = %d\n", i, (int)cups->EncodeLUT[i]);
#endif /* DEBUG */
  }

#ifdef DEBUG
  dprintf1("DEBUG2: cups->EncodeLUT[0] = %d\n", (int)cups->EncodeLUT[0]);
  dprintf2("DEBUG2: cups->EncodeLUT[%d] = %d\n", gx_max_color_value,
	   (int)cups->EncodeLUT[gx_max_color_value]);
#endif /* DEBUG */

  for (i = 0; i < cups->color_info.dither_grays; i ++)
    cups->DecodeLUT[i] = gx_max_color_value * i / max_lut;

#ifdef DEBUG
  dprintf2("DEBUG: num_components = %d, depth = %d\n",
	   cups->color_info.num_components, cups->color_info.depth);
  dprintf2("DEBUG: cupsColorSpace = %d, cupsColorOrder = %d\n",
	   cups->header.cupsColorSpace, cups->header.cupsColorOrder);
  dprintf2("DEBUG: cupsBitsPerPixel = %d, cupsBitsPerColor = %d\n",
	   cups->header.cupsBitsPerPixel, cups->header.cupsBitsPerColor);
  dprintf2("DEBUG: max_gray = %d, dither_grays = %d\n",
	   cups->color_info.max_gray, cups->color_info.dither_grays);
  dprintf2("DEBUG: max_color = %d, dither_colors = %d\n",
	   cups->color_info.max_color, cups->color_info.dither_colors);
#endif /* DEBUG */

 /*
  * Set the color profile as needed...
  */

  cups->HaveProfile = 0;

#ifdef dev_t_proc_encode_color
  if (cups->Profile)
#else
  if (cups->Profile && cups->header.cupsBitsPerColor == 8)
#endif /* dev_t_proc_encode_color */
  {
#ifdef DEBUG
    dprintf1("DEBUG: Using user-defined profile \"%s\"...\n", cups->Profile);
#endif /* DEBUG */

    if (sscanf(cups->Profile, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f", &d, &g,
               m[0] + 0, m[0] + 1, m[0] + 2,
               m[1] + 0, m[1] + 1, m[1] + 2,
               m[2] + 0, m[2] + 1, m[2] + 2) != 11)
      dprintf("ERROR: User-defined profile does not contain 11 integers!\n");
    else
    {
      cups->HaveProfile = 1;

      d       *= 0.001f;
      g       *= 0.001f;
      m[0][0] *= 0.001f;
      m[0][1] *= 0.001f;
      m[0][2] *= 0.001f;
      m[1][0] *= 0.001f;
      m[1][1] *= 0.001f;
      m[1][2] *= 0.001f;
      m[2][0] *= 0.001f;
      m[2][1] *= 0.001f;
      m[2][2] *= 0.001f;
    }
  }
#ifdef dev_t_proc_encode_color
  else if (cups->PPD)
#else
  else if (cups->PPD && cups->header.cupsBitsPerColor == 8)
#endif /* dev_t_proc_encode_color */
  {
   /*
    * Find the appropriate color profile...
    */

    if (pdev->HWResolution[0] != pdev->HWResolution[1])
      sprintf(resolution, "%.0fx%.0fdpi", pdev->HWResolution[0],
              pdev->HWResolution[1]);
    else
      sprintf(resolution, "%.0fdpi", pdev->HWResolution[0]);

    for (i = 0, profile = cups->PPD->profiles;
         i < cups->PPD->num_profiles;
	 i ++, profile ++)
      if ((strcmp(profile->resolution, resolution) == 0 ||
           profile->resolution[0] == '-') &&
          (strcmp(profile->media_type, cups->header.MediaType) == 0 ||
           profile->media_type[0] == '-'))
	break;

   /*
    * If we found a color profile, use it!
    */

    if (i < cups->PPD->num_profiles)
    {
#ifdef DEBUG
      dprintf("DEBUG: Using color profile in PPD file!\n");
#endif /* DEBUG */

      cups->HaveProfile = 1;

      d = profile->density;
      g = profile->gamma;

      memcpy(m, profile->matrix, sizeof(m));
    }
  }

  if (cups->HaveProfile)
  {
    for (i = 0; i < 3; i ++)
      for (j = 0; j < 3; j ++)
	for (k = 0; k <= CUPS_MAX_VALUE; k ++)
	{
          cups->Matrix[i][j][k] = (int)((float)k * m[i][j] + 0.5);

#ifdef DEBUG
          if ((k & 4095) == 0)
            dprintf4("DEBUG2: cups->Matrix[%d][%d][%d] = %d\n",
		     i, j, k, cups->Matrix[i][j][k]);
#endif /* DEBUG */
        }


    for (k = 0; k <= CUPS_MAX_VALUE; k ++)
    {
      cups->Density[k] = (int)((float)CUPS_MAX_VALUE * d *
	                     pow((float)k / (float)CUPS_MAX_VALUE, g) +
			     0.5);

#ifdef DEBUG
      if ((k & 4095) == 0)
        dprintf2("DEBUG2: cups->Density[%d] = %d\n", k, cups->Density[k]);
#endif /* DEBUG */
    }
  }
  else
  {
    for (k = 0; k <= CUPS_MAX_VALUE; k ++)
      cups->Density[k] = k;
  }
}


/*
 * 'cups_sync_output()' - Keep the user informed of our status...
 */

private int				/* O - Error status */
cups_sync_output(gx_device *pdev)	/* I - Device info */
{
  dprintf1("INFO: Processing page %d...\n", cups->page);

  return (0);
}


/*
 * 'cups_print_chunked()' - Print a page of chunked pixels.
 */

static int
cups_print_chunked(gx_device_printer *pdev,
					/* I - Printer device */
                   unsigned char     *src,
					/* I - Scanline buffer */
		   unsigned char     *dst,
					/* I - Bitmap buffer */
		   int               srcbytes)
					/* I - Number of bytes in src */
{
  int		y;			/* Looping var */
  unsigned char	*srcptr,		/* Pointer to data */
		*dstptr;		/* Pointer to bits */
  int		count;			/* Count for loop */
  int		xflip,			/* Flip scanline? */
                yflip,			/* Reverse scanline order? */
                ystart, yend, ystep;    /* Loop control for scanline order */   
  ppd_attr_t    *backside = NULL;

#ifdef DEBUG
  dprintf1("DEBUG2: cups->header.Duplex = %d\n", cups->header.Duplex);
  dprintf1("DEBUG2: cups->header.Tumble = %d\n", cups->header.Tumble);
  dprintf1("DEBUG2: cups->page = %d\n", cups->page);
  dprintf1("DEBUG2: cups->PPD = %p\n", cups->PPD);
#endif /* DEBUG */

  if (cups->PPD) {
    backside = ppdFindAttr(cups->PPD, "cupsBackSide", NULL);
    if (backside) {
#ifdef DEBUG
      dprintf1("DEBUG2: cupsBackSide = %s\n", backside->value);
#endif /* DEBUG */
      cups->PPD->flip_duplex = 0;
    }
  }
  if (cups->header.Duplex && cups->PPD &&
      ((!cups->header.Tumble &&
	(cups->PPD->flip_duplex ||
	 (backside && !strcasecmp(backside->value, "Rotated")))) ||
       (cups->header.Tumble &&
	(backside && (!strcasecmp(backside->value, "Flipped") ||
		      !strcasecmp(backside->value, "ManualTumble"))))) &&
      !(cups->page & 1))
    xflip = 1;
  else
    xflip = 0;
  if (cups->header.Duplex && cups->PPD &&
      ((!cups->header.Tumble &&
	(cups->PPD->flip_duplex ||
	 (backside && (!strcasecmp(backside->value, "Flipped") ||
		       !strcasecmp(backside->value, "Rotated"))))) ||
       (cups->header.Tumble &&
	(backside && !strcasecmp(backside->value, "ManualTumble")))) &&
      !(cups->page & 1)) {
    yflip = 1;
    ystart = cups->height - 1;
    yend = -1;
    ystep = -1;
  } else {
    yflip = 0;
    ystart = 0;
    yend = cups->height;
    ystep = 1;
  }

#ifdef DEBUG
  dprintf3("DEBUG: cups_print_chunked: xflip = %d, yflip = %d, height = %d\n",
	   xflip, yflip, cups->height);
#endif /* DEBUG */

 /*
  * Loop through the page bitmap and write chunked pixels, reversing as
  * needed...
  */
  for (y = ystart; y != yend; y += ystep)
  {
   /*
    * Grab the scanline data...
    */

    if (gdev_prn_get_bits((gx_device_printer *)pdev, y, src, &srcptr) < 0)
    {
      dprintf1("ERROR: Unable to get scanline %d!\n", y);
      return_error(gs_error_unknownerror);
    }

    if (xflip)
    {
     /*
      * Flip the raster data before writing it...
      */

      if (srcptr[0] == 0 && memcmp(srcptr, srcptr + 1, srcbytes - 1) == 0)
	memset(dst, 0, cups->header.cupsBytesPerLine);
      else
      {
        dstptr = dst;
	count  = srcbytes;

	switch (cups->color_info.depth)
	{
          case 1 : /* B&W bitmap */
	      for (srcptr += srcbytes - 1;
	           count > 0;
		   count --, srcptr --, dstptr ++)
	      {
	        *dstptr = cups->RevUpper1[*srcptr & 15] |
		          cups->RevLower1[*srcptr >> 4];
              }
	      break;

	  case 2 : /* 2-bit W/K image */
	      for (srcptr += srcbytes - 1;
	           count > 0;
		   count --, srcptr --, dstptr ++)
	      {
	        *dstptr = cups->RevUpper2[*srcptr & 15] |
		          cups->RevLower2[*srcptr >> 4];
              }
	      break;

	  case 4 : /* 1-bit RGB/CMY/CMYK bitmap or 4-bit W/K image */
	      for (srcptr += srcbytes - 1;
	           count > 0;
		   count --, srcptr --, dstptr ++)
	        *dstptr = (*srcptr >> 4) | (*srcptr << 4);
	      break;

          case 8 : /* 2-bit RGB/CMY/CMYK or 8-bit W/K image */
	      for (srcptr += srcbytes - 1;
	           count > 0;
		   count --, srcptr --, dstptr ++)
	        *dstptr = *srcptr;
	      break;

          case 16 : /* 4-bit RGB/CMY/CMYK or 16-bit W/K image */
	      for (srcptr += srcbytes - 2;
	           count > 0;
		   count -= 2, srcptr -= 2, dstptr += 2)
	      {
	        dstptr[0] = srcptr[0];
	        dstptr[1] = srcptr[1];
              }
	      break;

          case 24 : /* 8-bit RGB or CMY image */
	      for (srcptr += srcbytes - 3;
	           count > 0;
		   count -= 3, srcptr -= 3, dstptr += 3)
	      {
	        dstptr[0] = srcptr[0];
	        dstptr[1] = srcptr[1];
	        dstptr[2] = srcptr[2];
              }
	      break;

          case 32 : /* 8-bit CMYK image */
	      for (srcptr += srcbytes - 4;
	           count > 0;
		   count -= 4, srcptr -= 4, dstptr += 4)
	      {
	        dstptr[0] = srcptr[0];
	        dstptr[1] = srcptr[1];
	        dstptr[2] = srcptr[2];
	        dstptr[3] = srcptr[3];
              }
	      break;

          case 48 : /* 16-bit RGB or CMY image */
	      for (srcptr += srcbytes - 6;
	           count > 0;
		   count -= 6, srcptr -= 6, dstptr += 6)
	      {
	        dstptr[0] = srcptr[0];
	        dstptr[1] = srcptr[1];
	        dstptr[2] = srcptr[2];
	        dstptr[3] = srcptr[3];
	        dstptr[4] = srcptr[4];
	        dstptr[5] = srcptr[5];
              }
	      break;

          case 64 : /* 16-bit CMYK image */
	      for (srcptr += srcbytes - 8;
	           count > 0;
		   count -= 8, srcptr -= 8, dstptr += 8)
	      {
	        dstptr[0] = srcptr[0];
	        dstptr[1] = srcptr[1];
	        dstptr[2] = srcptr[2];
	        dstptr[3] = srcptr[3];
	        dstptr[4] = srcptr[4];
	        dstptr[5] = srcptr[5];
	        dstptr[6] = srcptr[6];
	        dstptr[7] = srcptr[7];
              }
	      break;
        }
      }

     /*
      * Write the bitmap data to the raster stream...
      */

      cupsRasterWritePixels(cups->stream, dst, cups->header.cupsBytesPerLine);
    }
    else
    {
     /*
      * Write the scanline data to the raster stream...
      */

      cupsRasterWritePixels(cups->stream, srcptr, cups->header.cupsBytesPerLine);
    }
  }
  return (0);
}


/*
 * 'cups_print_banded()' - Print a page of banded pixels.
 */

static int
cups_print_banded(gx_device_printer *pdev,
					/* I - Printer device */
                  unsigned char     *src,
					/* I - Scanline buffer */
		  unsigned char     *dst,
					/* I - Bitmap buffer */
		  int               srcbytes)
					/* I - Number of bytes in src */
{
  int		x;			/* Looping var */
  int		y;			/* Looping var */
  int		bandbytes;		/* Bytes per band */
  unsigned char	bit;			/* Current bit */
  unsigned char	temp;			/* Temporary variable */
  unsigned char	*srcptr;		/* Pointer to data */
  unsigned char	*cptr, *mptr, *yptr,	/* Pointer to components */
		*kptr, *lcptr, *lmptr;	/* ... */
  int		xflip,			/* Flip scanline? */
                yflip,			/* Reverse scanline order? */
                ystart, yend, ystep;    /* Loop control for scanline order */   
  ppd_attr_t    *backside = NULL;

#ifdef DEBUG
  dprintf1("DEBUG2: cups->header.Duplex = %d\n", cups->header.Duplex);
  dprintf1("DEBUG2: cups->header.Tumble = %d\n", cups->header.Tumble);
  dprintf1("DEBUG2: cups->page = %d\n", cups->page);
  dprintf1("DEBUG2: cups->PPD = %p\n", cups->PPD);
#endif /* DEBUG */

  if (cups->PPD) {
    backside = ppdFindAttr(cups->PPD, "cupsBackSide", NULL);
    if (backside) {
#ifdef DEBUG
      dprintf1("DEBUG2: cupsBackSide = %s\n", backside->value);
#endif /* DEBUG */
      cups->PPD->flip_duplex = 0;
    }
  }
  if (cups->header.Duplex && cups->PPD &&
      ((!cups->header.Tumble &&
	(cups->PPD->flip_duplex ||
	 (backside && !strcasecmp(backside->value, "Rotated")))) ||
       (cups->header.Tumble &&
	(backside && (!strcasecmp(backside->value, "Flipped") ||
		      !strcasecmp(backside->value, "ManualTumble"))))) &&
      !(cups->page & 1))
    xflip = 1;
  else
    xflip = 0;
  if (cups->header.Duplex && cups->PPD &&
      ((!cups->header.Tumble &&
	(cups->PPD->flip_duplex ||
	 (backside && (!strcasecmp(backside->value, "Flipped") ||
		       !strcasecmp(backside->value, "Rotated"))))) ||
       (cups->header.Tumble &&
	(backside && !strcasecmp(backside->value, "ManualTumble")))) &&
      !(cups->page & 1)) {
    yflip = 1;
    ystart = cups->height - 1;
    yend = -1;
    ystep = -1;
  } else {
    yflip = 0;
    ystart = 0;
    yend = cups->height;
    ystep = 1;
  }

#ifdef DEBUG
  dprintf3("DEBUG: cups_print_chunked: xflip = %d, yflip = %d, height = %d\n",
	   xflip, yflip, cups->height);
#endif /* DEBUG */

 /*
  * Loop through the page bitmap and write banded pixels...  We have
  * to separate each chunked color as needed...
  */

#ifdef CUPS_RASTER_SYNCv1
  bandbytes = cups->header.cupsBytesPerLine / cups->header.cupsNumColors;
#else
  if (cups->header.cupsColorSpace == CUPS_CSPACE_KCMYcm &&
      cups->header.cupsBitsPerColor == 1)
    bandbytes = cups->header.cupsBytesPerLine / 6;
  else
    bandbytes = cups->header.cupsBytesPerLine / cups->color_info.num_components;
#endif /* CUPS_RASTER_SYNCv1 */

  for (y = ystart; y != yend; y += ystep)
  {
   /*
    * Grab the scanline data...
    */

    if (gdev_prn_get_bits((gx_device_printer *)pdev, y, src, &srcptr) < 0)
    {
      dprintf1("ERROR: Unable to get scanline %d!\n", y);
      return_error(gs_error_unknownerror);
    }

   /*
    * Separate the chunked colors into their components...
    */

    if (srcptr[0] == 0 && memcmp(srcptr, srcptr + 1, srcbytes - 1) == 0)
      memset(dst, 0, cups->header.cupsBytesPerLine);
    else
    {
      if (xflip)
        cptr = dst + bandbytes - 1;
      else
        cptr = dst;

      mptr  = cptr + bandbytes;
      yptr  = mptr + bandbytes;
      kptr  = yptr + bandbytes;
      lcptr = kptr + bandbytes;
      lmptr = lcptr + bandbytes;

      switch (cups->header.cupsBitsPerColor)
      {
	default :
            memset(dst, 0, cups->header.cupsBytesPerLine);

            switch (cups->header.cupsColorSpace)
	    {
	      default :
	          for (x = cups->width, bit = xflip ? 1 << (x & 7) : 128;
		       x > 0;
		       x --, srcptr ++)
		  {
		    if (*srcptr & 0x40)
		      *cptr |= bit;
		    if (*srcptr & 0x20)
		      *mptr |= bit;
		    if (*srcptr & 0x10)
		      *yptr |= bit;

                    if (xflip)
		    {
		      if (bit < 128)
			bit <<= 1;
		      else
		      {
			cptr --;
			mptr --;
			yptr --;
			bit = 1;
		      }
		    }
		    else
		      bit >>= 1;

		    x --;
		    if (x == 0)
		      break;

		    if (*srcptr & 0x4)
		      *cptr |= bit;
		    if (*srcptr & 0x2)
		      *mptr |= bit;
		    if (*srcptr & 0x1)
		      *yptr |= bit;

                    if (xflip)
		    {
		      if (bit < 128)
			bit <<= 1;
		      else
		      {
			cptr --;
			mptr --;
			yptr --;
			bit = 1;
		      }
		    }
		    else if (bit > 1)
		      bit >>= 1;
		    else
		    {
		      cptr ++;
		      mptr ++;
		      yptr ++;
		      bit = 128;
		    }
		  }
	          break;
	      case CUPS_CSPACE_GMCK :
	      case CUPS_CSPACE_GMCS :
	      case CUPS_CSPACE_RGBA :
	      case CUPS_CSPACE_RGBW :
	      case CUPS_CSPACE_CMYK :
	      case CUPS_CSPACE_YMCK :
	      case CUPS_CSPACE_KCMY :
	          for (x = cups->width, bit = xflip ? 1 << (x & 7) : 128;
		       x > 0;
		       x --, srcptr ++)
		  {
		    if (*srcptr & 0x80)
		      *cptr |= bit;
		    if (*srcptr & 0x40)
		      *mptr |= bit;
		    if (*srcptr & 0x20)
		      *yptr |= bit;
		    if (*srcptr & 0x10)
		      *kptr |= bit;

                    if (xflip)
		    {
		      if (bit < 128)
			bit <<= 1;
		      else
		      {
			cptr --;
			mptr --;
			yptr --;
			kptr --;
			bit = 1;
		      }
		    }
		    else
		      bit >>= 1;

		    x --;
		    if (x == 0)
		      break;

		    if (*srcptr & 0x8)
		      *cptr |= bit;
		    if (*srcptr & 0x4)
		      *mptr |= bit;
		    if (*srcptr & 0x2)
		      *yptr |= bit;
		    if (*srcptr & 0x1)
		      *kptr |= bit;

                    if (xflip)
		    {
		      if (bit < 128)
			bit <<= 1;
		      else
		      {
			cptr --;
			mptr --;
			yptr --;
			kptr --;
			bit = 1;
		      }
		    }
		    else if (bit > 1)
		      bit >>= 1;
		    else
		    {
		      cptr ++;
		      mptr ++;
		      yptr ++;
		      kptr ++;
		      bit = 128;
		    }
		  }
	          break;
	      case CUPS_CSPACE_KCMYcm :
	          for (x = cups->width, bit = xflip ? 1 << (x & 7) : 128;
		       x > 0;
		       x --, srcptr ++)
		  {
                   /*
                    * Note: Because of the way the pointers are setup,
                    *       the following code is correct even though
                    *       the names don't match...
                    */

		    if (*srcptr & 0x20)
		      *cptr |= bit;
		    if (*srcptr & 0x10)
		      *mptr |= bit;
		    if (*srcptr & 0x08)
		      *yptr |= bit;
		    if (*srcptr & 0x04)
		      *kptr |= bit;
		    if (*srcptr & 0x02)
		      *lcptr |= bit;
		    if (*srcptr & 0x01)
		      *lmptr |= bit;

                    if (xflip)
		    {
		      if (bit < 128)
			bit <<= 1;
		      else
		      {
			cptr --;
			mptr --;
			yptr --;
			kptr --;
			lcptr --;
			lmptr --;
			bit = 1;
		      }
		    }
		    else if (bit > 1)
		      bit >>= 1;
		    else
		    {
		      cptr ++;
		      mptr ++;
		      yptr ++;
		      kptr ++;
		      lcptr ++;
		      lmptr ++;
		      bit = 128;
		    }
		  }
	          break;
	    }
            break;

	case 2 :
            memset(dst, 0, cups->header.cupsBytesPerLine);

            switch (cups->header.cupsColorSpace)
	    {
	      default :
	          for (x = cups->width, bit = xflip ? 3 << (2 * (x & 3)) : 0xc0;
		       x > 0;
		       x --, srcptr ++)
		    switch (bit)
		    {
		      case 0xc0 :
			  if ((temp = *srcptr & 0x30) != 0)
			    *cptr |= temp << 2;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *mptr |= temp << 4;
			  if ((temp = *srcptr & 0x03) != 0)
			    *yptr |= temp << 6;

                          if (xflip)
			  {
			    bit = 0x03;
			    cptr --;
			    mptr --;
			    yptr --;
			  }
			  else
			    bit = 0x30;
			  break;
		      case 0x30 :
			  if ((temp = *srcptr & 0x30) != 0)
			    *cptr |= temp;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *mptr |= temp << 2;
			  if ((temp = *srcptr & 0x03) != 0)
			    *yptr |= temp << 4;

			  if (xflip)
			    bit = 0xc0;
			  else
			    bit = 0x0c;
			  break;
		      case 0x0c :
			  if ((temp = *srcptr & 0x30) != 0)
			    *cptr |= temp >> 2;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *mptr |= temp;
			  if ((temp = *srcptr & 0x03) != 0)
			    *yptr |= temp << 2;

			  if (xflip)
			    bit = 0x30;
			  else
			    bit = 0x03;
			  break;
		      case 0x03 :
			  if ((temp = *srcptr & 0x30) != 0)
			    *cptr |= temp >> 4;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *mptr |= temp >> 2;
			  if ((temp = *srcptr & 0x03) != 0)
			    *yptr |= temp;

			  if (xflip)
			    bit = 0x0c;
			  else
			  {
			    bit = 0xc0;
			    cptr ++;
			    mptr ++;
			    yptr ++;
			  }
			  break;
                    }
	          break;
	      case CUPS_CSPACE_GMCK :
	      case CUPS_CSPACE_GMCS :
	      case CUPS_CSPACE_RGBA :
	      case CUPS_CSPACE_RGBW :
	      case CUPS_CSPACE_CMYK :
	      case CUPS_CSPACE_YMCK :
	      case CUPS_CSPACE_KCMY :
	      case CUPS_CSPACE_KCMYcm :
	          for (x = cups->width, bit = xflip ? 3 << (2 * (x & 3)) : 0xc0;
		       x > 0;
		       x --, srcptr ++)
		    switch (bit)
		    {
		      case 0xc0 :
		          if ((temp = *srcptr & 0xc0) != 0)
			    *cptr |= temp;
			  if ((temp = *srcptr & 0x30) != 0)
			    *mptr |= temp << 2;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *yptr |= temp << 4;
			  if ((temp = *srcptr & 0x03) != 0)
			    *kptr |= temp << 6;

                          if (xflip)
			  {
			    bit = 0x03;
			    cptr --;
			    mptr --;
			    yptr --;
			    kptr --;
			  }
			  else
			    bit = 0x30;
			  break;
		      case 0x30 :
		          if ((temp = *srcptr & 0xc0) != 0)
			    *cptr |= temp >> 2;
			  if ((temp = *srcptr & 0x30) != 0)
			    *mptr |= temp;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *yptr |= temp << 2;
			  if ((temp = *srcptr & 0x03) != 0)
			    *kptr |= temp << 4;

			  if (xflip)
			    bit = 0xc0;
			  else
			    bit = 0x0c;
			  break;
		      case 0x0c :
		          if ((temp = *srcptr & 0xc0) != 0)
			    *cptr |= temp >> 4;
			  if ((temp = *srcptr & 0x30) != 0)
			    *mptr |= temp >> 2;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *yptr |= temp;
			  if ((temp = *srcptr & 0x03) != 0)
			    *kptr |= temp << 2;

			  if (xflip)
			    bit = 0x30;
			  else
			    bit = 0x03;
			  break;
		      case 0x03 :
		          if ((temp = *srcptr & 0xc0) != 0)
			    *cptr |= temp >> 6;
			  if ((temp = *srcptr & 0x30) != 0)
			    *mptr |= temp >> 4;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *yptr |= temp >> 2;
			  if ((temp = *srcptr & 0x03) != 0)
			    *kptr |= temp;

			  if (xflip)
			    bit = 0x0c;
			  else
			  {
			    bit = 0xc0;
			    cptr ++;
			    mptr ++;
			    yptr ++;
			    kptr ++;
			  }
			  break;
                    }
	          break;
	    }
            break;

	case 4 :
            memset(dst, 0, cups->header.cupsBytesPerLine);

            switch (cups->header.cupsColorSpace)
	    {
	      default :
	          for (x = cups->width, bit = xflip && (x & 1) ? 0xf0 : 0x0f;
		       x > 0;
		       x --, srcptr += 2)
		    switch (bit)
		    {
		      case 0xf0 :
			  if ((temp = srcptr[0] & 0x0f) != 0)
			    *cptr |= temp << 4;
			  if ((temp = srcptr[1] & 0xf0) != 0)
			    *mptr |= temp;
			  if ((temp = srcptr[1] & 0x0f) != 0)
			    *yptr |= temp << 4;

			  bit = 0x0f;

                          if (xflip)
			  {
			    cptr --;
			    mptr --;
			    yptr --;
			  }
			  break;
		      case 0x0f :
			  if ((temp = srcptr[0] & 0x0f) != 0)
			    *cptr |= temp;
			  if ((temp = srcptr[1] & 0xf0) != 0)
			    *mptr |= temp >> 4;
			  if ((temp = srcptr[1] & 0x0f) != 0)
			    *yptr |= temp;

			  bit = 0xf0;

                          if (!xflip)
			  {
			    cptr ++;
			    mptr ++;
			    yptr ++;
			  }
			  break;
                    }
	          break;
	      case CUPS_CSPACE_GMCK :
	      case CUPS_CSPACE_GMCS :
	      case CUPS_CSPACE_RGBA :
	      case CUPS_CSPACE_RGBW :
	      case CUPS_CSPACE_CMYK :
	      case CUPS_CSPACE_YMCK :
	      case CUPS_CSPACE_KCMY :
	      case CUPS_CSPACE_KCMYcm :
	          for (x = cups->width, bit = xflip && (x & 1) ? 0xf0 : 0x0f;
		       x > 0;
		       x --, srcptr += 2)
		    switch (bit)
		    {
		      case 0xf0 :
		          if ((temp = srcptr[0] & 0xf0) != 0)
			    *cptr |= temp;
			  if ((temp = srcptr[0] & 0x0f) != 0)
			    *mptr |= temp << 4;
			  if ((temp = srcptr[1] & 0xf0) != 0)
			    *yptr |= temp;
			  if ((temp = srcptr[1] & 0x0f) != 0)
			    *kptr |= temp << 4;

			  bit = 0x0f;

                          if (xflip)
			  {
			    cptr --;
			    mptr --;
			    yptr --;
			    kptr --;
			  }
			  break;
		      case 0x0f :
		          if ((temp = srcptr[0] & 0xf0) != 0)
			    *cptr |= temp >> 4;
			  if ((temp = srcptr[0] & 0x0f) != 0)
			    *mptr |= temp;
			  if ((temp = srcptr[1] & 0xf0) != 0)
			    *yptr |= temp >> 4;
			  if ((temp = srcptr[1] & 0x0f) != 0)
			    *kptr |= temp;

			  bit = 0xf0;

                          if (!xflip)
			  {
			    cptr ++;
			    mptr ++;
			    yptr ++;
			    kptr ++;
			  }
			  break;
                    }
	          break;
	    }
            break;

	case 8 :
            switch (cups->header.cupsColorSpace)
	    {
	      default :
	          if (xflip)
	            for (x = cups->width; x > 0; x --)
		    {
		      *cptr-- = *srcptr++;
		      *mptr-- = *srcptr++;
		      *yptr-- = *srcptr++;
		    }
		  else
	            for (x = cups->width; x > 0; x --)
		    {
		      *cptr++ = *srcptr++;
		      *mptr++ = *srcptr++;
		      *yptr++ = *srcptr++;
		    }
	          break;
	      case CUPS_CSPACE_GMCK :
	      case CUPS_CSPACE_GMCS :
	      case CUPS_CSPACE_RGBA :
	      case CUPS_CSPACE_RGBW :
	      case CUPS_CSPACE_CMYK :
	      case CUPS_CSPACE_YMCK :
	      case CUPS_CSPACE_KCMY :
	      case CUPS_CSPACE_KCMYcm :
	          if (xflip)
	            for (x = cups->width; x > 0; x --)
		    {
		      *cptr-- = *srcptr++;
		      *mptr-- = *srcptr++;
		      *yptr-- = *srcptr++;
		      *kptr-- = *srcptr++;
		    }
		  else
	            for (x = cups->width; x > 0; x --)
		    {
		      *cptr++ = *srcptr++;
		      *mptr++ = *srcptr++;
		      *yptr++ = *srcptr++;
		      *kptr++ = *srcptr++;
		    }
	          break;
	    }
            break;

	case 16 :
            switch (cups->header.cupsColorSpace)
	    {
	      default :
	          if (xflip)
	            for (x = cups->width; x > 0; x --, srcptr += 6)
		    {
		      *cptr-- = srcptr[1];
		      *cptr-- = srcptr[0];
		      *mptr-- = srcptr[3];
		      *mptr-- = srcptr[2];
		      *yptr-- = srcptr[5];
		      *yptr-- = srcptr[4];
		    }
		  else
	            for (x = cups->width; x > 0; x --)
		    {
		      *cptr++ = *srcptr++;
		      *cptr++ = *srcptr++;
		      *mptr++ = *srcptr++;
		      *mptr++ = *srcptr++;
		      *yptr++ = *srcptr++;
		      *yptr++ = *srcptr++;
		    }
	          break;
	      case CUPS_CSPACE_GMCK :
	      case CUPS_CSPACE_GMCS :
	      case CUPS_CSPACE_RGBA :
	      case CUPS_CSPACE_RGBW :
	      case CUPS_CSPACE_CMYK :
	      case CUPS_CSPACE_YMCK :
	      case CUPS_CSPACE_KCMY :
	      case CUPS_CSPACE_KCMYcm :
	          if (xflip)
	            for (x = cups->width; x > 0; x --, srcptr += 8)
		    {
		      *cptr-- = srcptr[1];
		      *cptr-- = srcptr[0];
		      *mptr-- = srcptr[3];
		      *mptr-- = srcptr[2];
		      *yptr-- = srcptr[5];
		      *yptr-- = srcptr[4];
		      *kptr-- = srcptr[7];
		      *kptr-- = srcptr[6];
		    }
		  else
	            for (x = cups->width; x > 0; x --)
		    {
		      *cptr++ = *srcptr++;
		      *cptr++ = *srcptr++;
		      *mptr++ = *srcptr++;
		      *mptr++ = *srcptr++;
		      *yptr++ = *srcptr++;
		      *yptr++ = *srcptr++;
		      *kptr++ = *srcptr++;
		      *kptr++ = *srcptr++;
		    }
	          break;
	    }
            break;
      }
    }

   /*
    * Write the bitmap data to the raster stream...
    */

    cupsRasterWritePixels(cups->stream, dst, cups->header.cupsBytesPerLine);
  }
  return (0);
}


/*
 * 'cups_print_planar()' - Print a page of planar pixels.
 */

static int
cups_print_planar(gx_device_printer *pdev,
					/* I - Printer device */
                  unsigned char     *src,
					/* I - Scanline buffer */
		  unsigned char     *dst,
					/* I - Bitmap buffer */
		  int               srcbytes)
					/* I - Number of bytes in src */
{
  int		x;			/* Looping var */
  int		y;			/* Looping var */
  int		z;			/* Looping var */
  unsigned char	srcbit;			/* Current source bit */
  unsigned char	dstbit;			/* Current destination bit */
  unsigned char	temp;			/* Temporary variable */
  unsigned char	*srcptr;		/* Pointer to data */
  unsigned char	*dstptr;		/* Pointer to bitmap */


 /**** NOTE: Currently planar output doesn't support flipped duplex!!! ****/

 /*
  * Loop through the page bitmap and write planar pixels...  We have
  * to separate each chunked color as needed...
  */

  for (z = 0; z < pdev->color_info.num_components; z ++)
    for (y = 0; y < cups->height; y ++)
    {
     /*
      * Grab the scanline data...
      */

      if (gdev_prn_get_bits((gx_device_printer *)pdev, y, src, &srcptr) < 0)
      {
	dprintf1("ERROR: Unable to get scanline %d!\n", y);
	return_error(gs_error_unknownerror);
      }

     /*
      * Pull the individual color planes out of the pixels...
      */

      if (srcptr[0] == 0 && memcmp(srcptr, srcptr + 1, srcbytes - 1) == 0)
	memset(dst, 0, cups->header.cupsBytesPerLine);
      else
	switch (cups->header.cupsBitsPerColor)
	{
          default :
	      memset(dst, 0, cups->header.cupsBytesPerLine);

	      switch (cups->header.cupsColorSpace)
	      {
		default :
	            for (dstptr = dst, x = cups->width, srcbit = 64 >> z,
		             dstbit = 128;
			 x > 0;
			 x --)
		    {
		      if (*srcptr & srcbit)
			*dstptr |= dstbit;

                      if (srcbit >= 16)
			srcbit >>= 4;
		      else
		      {
			srcbit = 64 >> z;
			srcptr ++;
		      }

                      if (dstbit > 1)
			dstbit >>= 1;
		      else
		      {
			dstbit = 128;
			dstptr ++;
		      }
		    }
	            break;
		case CUPS_CSPACE_GMCK :
		case CUPS_CSPACE_GMCS :
		case CUPS_CSPACE_RGBA :
		case CUPS_CSPACE_RGBW :
		case CUPS_CSPACE_CMYK :
		case CUPS_CSPACE_YMCK :
		case CUPS_CSPACE_KCMY :
	            for (dstptr = dst, x = cups->width, srcbit = 128 >> z,
		             dstbit = 128;
			 x > 0;
			 x --)
		    {
		      if (*srcptr & srcbit)
			*dstptr |= dstbit;

                      if (srcbit >= 16)
			srcbit >>= 4;
		      else
		      {
			srcbit = 128 >> z;
			srcptr ++;
		      }

                      if (dstbit > 1)
			dstbit >>= 1;
		      else
		      {
			dstbit = 128;
			dstptr ++;
		      }
		    }
	            break;
		case CUPS_CSPACE_KCMYcm :
	            for (dstptr = dst, x = cups->width, srcbit = 32 >> z,
		             dstbit = 128;
			 x > 0;
			 x --, srcptr ++)
		    {
		      if (*srcptr & srcbit)
			*dstptr |= dstbit;

                      if (dstbit > 1)
			dstbit >>= 1;
		      else
		      {
			dstbit = 128;
			dstptr ++;
		      }
		    }
	            break;
              }
	      break;

	  case 2 :
	      memset(dst, 0, cups->header.cupsBytesPerLine);

	      switch (cups->header.cupsColorSpace)
	      {
		default :
	            for (dstptr = dst, x = cups->width, srcbit = 48 >> (z * 2),
		             dstbit = 0xc0;
			 x > 0;
			 x --, srcptr ++)
		    {
		      if ((temp = *srcptr & srcbit) != 0)
		      {
			if (srcbit == dstbit)
		          *dstptr |= temp;
	        	else
			{
		          switch (srcbit)
			  {
			    case 0x30 :
				temp >>= 4;
				break;
			    case 0x0c :
				temp >>= 2;
				break;
                          }

		          switch (dstbit)
			  {
			    case 0xc0 :
				*dstptr |= temp << 6;
				break;
			    case 0x30 :
				*dstptr |= temp << 4;
				break;
			    case 0x0c :
				*dstptr |= temp << 2;
				break;
			    case 0x03 :
				*dstptr |= temp;
				break;
                          }
			}
		      }

		      if (dstbit > 0x03)
			dstbit >>= 2;
		      else
		      {
			dstbit = 0xc0;
			dstptr ++;
		      }
		    }
	            break;
		case CUPS_CSPACE_GMCK :
		case CUPS_CSPACE_GMCS :
		case CUPS_CSPACE_RGBA :
		case CUPS_CSPACE_RGBW :
		case CUPS_CSPACE_CMYK :
		case CUPS_CSPACE_YMCK :
		case CUPS_CSPACE_KCMY :
		case CUPS_CSPACE_KCMYcm :
	            for (dstptr = dst, x = cups->width, srcbit = 192 >> (z * 2),
		             dstbit = 0xc0;
			 x > 0;
			 x --, srcptr ++)
		    {
		      if ((temp = *srcptr & srcbit) != 0)
		      {
			if (srcbit == dstbit)
		          *dstptr |= temp;
	        	else
			{
		          switch (srcbit)
			  {
			    case 0xc0 :
				temp >>= 6;
				break;
			    case 0x30 :
				temp >>= 4;
				break;
			    case 0x0c :
				temp >>= 2;
				break;
                          }

		          switch (dstbit)
			  {
			    case 0xc0 :
				*dstptr |= temp << 6;
				break;
			    case 0x30 :
				*dstptr |= temp << 4;
				break;
			    case 0x0c :
				*dstptr |= temp << 2;
				break;
			    case 0x03 :
				*dstptr |= temp;
				break;
                          }
			}
		      }

		      if (dstbit > 0x03)
			dstbit >>= 2;
		      else
		      {
			dstbit = 0xc0;
			dstptr ++;
		      }
		    }
	            break;
              }
	      break;

	  case 4 :
	      memset(dst, 0, cups->header.cupsBytesPerLine);

	      switch (cups->header.cupsColorSpace)
	      {
		default :
	            if (z > 0)
		      srcptr ++;

		    if (z == 1)
		      srcbit = 0xf0;
		    else
		      srcbit = 0x0f;

	            for (dstptr = dst, x = cups->width, dstbit = 0xf0;
			 x > 0;
			 x --, srcptr += 2)
		    {
		      if ((temp = *srcptr & srcbit) != 0)
		      {
			if (srcbit == dstbit)
		          *dstptr |= temp;
	        	else
			{
		          if (srcbit == 0xf0)
	                    temp >>= 4;

		          if (dstbit == 0xf0)
   			    *dstptr |= temp << 4;
			  else
			    *dstptr |= temp;
			}
		      }

		      if (dstbit == 0xf0)
			dstbit = 0x0f;
		      else
		      {
			dstbit = 0xf0;
			dstptr ++;
		      }
		    }
	            break;
		case CUPS_CSPACE_GMCK :
		case CUPS_CSPACE_GMCS :
		case CUPS_CSPACE_RGBA :
		case CUPS_CSPACE_RGBW :
		case CUPS_CSPACE_CMYK :
		case CUPS_CSPACE_YMCK :
		case CUPS_CSPACE_KCMY :
		case CUPS_CSPACE_KCMYcm :
	            if (z > 1)
		      srcptr ++;

		    if (z & 1)
		      srcbit = 0x0f;
		    else
		      srcbit = 0xf0;

	            for (dstptr = dst, x = cups->width, dstbit = 0xf0;
			 x > 0;
			 x --, srcptr += 2)
		    {
		      if ((temp = *srcptr & srcbit) != 0)
		      {
			if (srcbit == dstbit)
		          *dstptr |= temp;
	        	else
			{
		          if (srcbit == 0xf0)
	                    temp >>= 4;

		          if (dstbit == 0xf0)
   			    *dstptr |= temp << 4;
			  else
			    *dstptr |= temp;
			}
		      }

		      if (dstbit == 0xf0)
			dstbit = 0x0f;
		      else
		      {
			dstbit = 0xf0;
			dstptr ++;
		      }
		    }
	            break;
              }
	      break;

	  case 8 :
	      for (srcptr += z, dstptr = dst, x = cups->header.cupsBytesPerLine;
	           x > 0;
		   srcptr += pdev->color_info.num_components, x --)
		*dstptr++ = *srcptr;
	      break;

	  case 16 :
	      for (srcptr += 2 * z, dstptr = dst, x = cups->header.cupsBytesPerLine;
	           x > 0;
		   srcptr += 2 * pdev->color_info.num_components, x --)
              {
		*dstptr++ = srcptr[0];
		*dstptr++ = srcptr[1];
	      }
	      break;
	}

     /*
      * Write the bitmap data to the raster stream...
      */

      cupsRasterWritePixels(cups->stream, dst, cups->header.cupsBytesPerLine);
    }
  return (0);
}


/*
 * End of "$Id$".
 */
