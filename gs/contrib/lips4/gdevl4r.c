/* Copyright (C) 1998, 1999 Norihito Ohmori.

   Ghostscript printer driver
   for Canon LBP, BJC-680J and BJC-880J printers (LIPS II+/III/IVc/IV)

   This software is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
   to anyone for the consequences of using it or for whether it serves any
   particular purpose or works at all, unless he says so in writing.  Refer
   to the GNU General Public License for full details.

   Everyone is granted permission to copy, modify and redistribute
   this software, but only under the conditions described in the GNU
   General Public License.  A copy of this license is supposed to have been
   given to you along with this software so you can know your rights and
   responsibilities.  It should be in a file named COPYING.  Among other
   things, the copyright notice and this notice must be preserved on all
   copies.
 */

/*$Id: gdevl4r.c,v 1.5 2002/10/12 23:24:34 tillkamppeter Exp $ */
/* Raster Version of LIPS driver */

#include "gdevlprn.h"
#include "gdevlips.h"

/* The device descriptors */
static dev_proc_open_device(lips2p_open);
static dev_proc_open_device(lips3_open);
static dev_proc_open_device(bjc880j_open);
static dev_proc_open_device(lips4_open);

static dev_proc_close_device(lips_close);

static dev_proc_print_page_copies(lips2p_print_page_copies);
static dev_proc_print_page_copies(lips3_print_page_copies);
static dev_proc_print_page_copies(bjc880j_print_page_copies);
static dev_proc_print_page_copies(lips4_print_page_copies);

static dev_proc_get_params(lips_get_params);
static dev_proc_get_params(lips4_get_params);

static dev_proc_put_params(lips_put_params);
static dev_proc_put_params(lips4_put_params);

#if 0
static dev_proc_image_out(lips_image_out);

#endif
static dev_proc_image_out(lips2p_image_out);
static dev_proc_image_out(lips4_image_out);

#define lips_device(dtype, procs, dname, xdpi, ydpi, lm, bm, rm, tm, color_bits,\
		    print_page_copies, image_out, cassetFeed, username)\
{        std_device_std_color_full_body(dtype, &procs, dname,\
          (int)((long)(DEFAULT_WIDTH_10THS) * (xdpi) / 10),\
          (int)((long)(DEFAULT_HEIGHT_10THS) * (ydpi) / 10),\
          xdpi, ydpi, color_bits,\
          -(lm) * (xdpi), -(tm) * (ydpi),\
          (lm) * 72.0, (bm) * 72.0,\
          (rm) * 72.0, (tm) * 72.0\
        ),\
       lp_device_body_rest_(print_page_copies, image_out),\
	   cassetFeed, username, LIPS_PJL_DEFAULT,\
	   0, 0, 0, 0, 0, 0, 0, -1\
}

#define lips4_device(dtype, procs, dname, xdpi, ydpi, lm, bm, rm, tm, color_bits,\
		    print_page_copies, image_out, cassetFeed, username)\
{        std_device_std_color_full_body(dtype, &procs, dname,\
          (int)((long)(DEFAULT_WIDTH_10THS) * (xdpi) / 10),\
          (int)((long)(DEFAULT_HEIGHT_10THS) * (ydpi) / 10),\
          xdpi, ydpi, color_bits,\
          -(lm) * (xdpi), -(tm) * (ydpi),\
          (lm) * 72.0, (bm) * 72.0,\
          (rm) * 72.0, (tm) * 72.0\
        ),\
       lp_duplex_device_body_rest_(print_page_copies, image_out),\
  cassetFeed,\
  username, LIPS_PJL_DEFAULT, 0, 0, 0, 0, 0, 0, 0, -1,\
  0, LIPS_NUP_DEFAULT, LIPS_FACEUP_DEFAULT,\
  LIPS_MEDIATYPE_DEFAULT \
}

typedef struct gx_device_lips_s gx_device_lips;
struct gx_device_lips_s {
    gx_device_common;
    gx_prn_device_common;
    gx_lprn_device_common;
    lips_params_common;
};

typedef struct gx_device_lips4_s gx_device_lips4;
struct gx_device_lips4_s {
    gx_device_common;
    gx_prn_device_common;
    gx_lprn_device_common;
    lips_params_common;
    lips4_params_common;
};

static gx_device_procs lips2p_prn_procs =
prn_params_procs(lips2p_open, gdev_prn_output_page, lips_close,
		 lips_get_params, lips_put_params);

static gx_device_procs lips3_prn_procs =
prn_params_procs(lips3_open, gdev_prn_output_page, lips_close,
		 lips_get_params, lips_put_params);

static gx_device_procs bjc880j_prn_color_procs =
prn_params_procs(bjc880j_open, gdev_prn_output_page, lips_close,
		       lips4_get_params, lips4_put_params);

static gx_device_procs lips4_prn_procs =
prn_params_procs(lips4_open, gdev_prn_output_page, lips_close,
		       lips4_get_params, lips4_put_params);

gx_device_lips far_data gs_lips2p_device =
lips_device(gx_device_lips, lips2p_prn_procs, "lips2p",
	    LIPS2P_DPI_DEFAULT,
	    LIPS2P_DPI_DEFAULT,
	    LIPS2P_LEFT_MARGIN_DEFAULT,
	    LIPS2P_BOTTOM_MARGIN_DEFAULT,
	    LIPS2P_RIGHT_MARGIN_DEFAULT,
	    LIPS2P_TOP_MARGIN_DEFAULT,
	    1, lips2p_print_page_copies, lips2p_image_out,
	    LIPS_CASSETFEED_DEFAULT,
	    LIPS_USERNAME_DEFAULT);

gx_device_lips far_data gs_lips3_device =
lips_device(gx_device_lips, lips3_prn_procs, "lips3",
	    LIPS3_DPI_DEFAULT,
	    LIPS3_DPI_DEFAULT,
	    LIPS3_LEFT_MARGIN_DEFAULT,
	    LIPS3_BOTTOM_MARGIN_DEFAULT,
	    LIPS3_RIGHT_MARGIN_DEFAULT,
	    LIPS3_TOP_MARGIN_DEFAULT,
	    1, lips3_print_page_copies, lips2p_image_out,
	    LIPS_CASSETFEED_DEFAULT,
	    LIPS_USERNAME_DEFAULT);

gx_device_lips4 far_data gs_bjc880j_device =
lips4_device(gx_device_lips4, bjc880j_prn_color_procs, "bjc880j",
	     BJC880J_DPI_DEFAULT,
	     BJC880J_DPI_DEFAULT,
	     BJC880J_LEFT_MARGIN_DEFAULT,
	     BJC880J_BOTTOM_MARGIN_DEFAULT,
	     BJC880J_RIGHT_MARGIN_DEFAULT,
	     BJC880J_TOP_MARGIN_DEFAULT,
	     1, bjc880j_print_page_copies, lips4_image_out,
	     LIPS_CASSETFEED_DEFAULT,
	     LIPS_USERNAME_DEFAULT);

gx_device_lips4 far_data gs_lips4_device =
lips4_device(gx_device_lips4, lips4_prn_procs, "lips4",
	     LIPS4_DPI_DEFAULT,
	     LIPS4_DPI_DEFAULT,
	     LIPS4_LEFT_MARGIN_DEFAULT,
	     LIPS4_BOTTOM_MARGIN_DEFAULT,
	     LIPS4_RIGHT_MARGIN_DEFAULT,
	     LIPS4_TOP_MARGIN_DEFAULT,
	     1, lips4_print_page_copies, lips4_image_out,
	     LIPS_CASSETFEED_DEFAULT,
	     LIPS_USERNAME_DEFAULT);

/* Printer types */
typedef enum {
    LIPS2P,
    LIPS3,
    BJC880J,
    LIPS4
} lips_printer_type;

/* Forward references */
static void lips_job_start(gx_device_printer * dev, lips_printer_type ptype, FILE * fp, int num_copies);
static void lips_job_end(gx_device_printer * pdev, FILE * fp);
static int lips_open(gx_device * pdev, lips_printer_type ptype);
static int lips4c_output_page(gx_device_printer * pdev, FILE * prn_stream);
static int lips_delta_encode(byte * inBuff, byte * prevBuff, byte * outBuff, byte * diffBuff, int Length);
static int lips_byte_cat(byte * TotalBuff, byte * Buff, int TotalLen, int Len);
static int lips_print_page_copies(gx_device_printer * pdev, FILE * prn_stream, lips_printer_type ptype, int numcopies);
#if GS_VERSION_MAJOR >= 8
static int lips_print_page_copies(gx_device_printer * pdev, FILE * prn_stream, lips_printer_type ptype, int numcopies);
static int lips4type_print_page_copies(gx_device_printer * pdev, FILE * prn_stream, int num_copies, int ptype);
#else
static int lips_print_page_copies(P4(gx_device_printer * pdev, FILE * prn_stream, lips_printer_type ptype, int numcopies));
static int lips_print_page_copies(P4(gx_device_printer * pdev, FILE * prn_stream, lips_printer_type ptype, int numcopies));
#endif
static int
lips2p_open(gx_device * pdev)
{
    return lips_open(pdev, LIPS2P);
}

static int
lips3_open(gx_device * pdev)
{
    return lips_open(pdev, LIPS3);
}

static int
bjc880j_open(gx_device * pdev)
{
    return lips_open(pdev, BJC880J);
}

static int
lips4_open(gx_device * pdev)
{
    return lips_open(pdev, LIPS4);
}

/* Open the printer, adjusting the margins if necessary. */
static int
lips_open(gx_device * pdev, lips_printer_type ptype)
{
    int width = pdev->MediaSize[0];
    int height = pdev->MediaSize[1];
    int xdpi = pdev->x_pixels_per_inch;
    int ydpi = pdev->y_pixels_per_inch;

    /* Paper Size Check */
    if (width <= height) {	/* portrait */
	if ((width < LIPS_WIDTH_MIN || width > LIPS_WIDTH_MAX ||
	     height < LIPS_HEIGHT_MIN || height > LIPS_HEIGHT_MAX) &&
	    !(width == LIPS_LEDGER_WIDTH && height == LIPS_LEDGER_HEIGHT))
	    return_error(gs_error_rangecheck);
    } else {			/* landscape */
	if ((width < LIPS_HEIGHT_MIN || width > LIPS_HEIGHT_MAX ||
	     height < LIPS_WIDTH_MIN || height > LIPS_WIDTH_MAX) &&
	    !(width == LIPS_LEDGER_HEIGHT && height == LIPS_LEDGER_WIDTH))
	    return_error(gs_error_rangecheck);
    }

    /* Resolution Check */
    if (xdpi != ydpi)
	return_error(gs_error_rangecheck);
    else if (ptype == LIPS2P) {
	/* LIPS II+ support DPI is 240x240 */
	if (xdpi != LIPS2P_DPI_MAX)
	    return_error(gs_error_rangecheck);
    } else if (ptype == LIPS3) {
	/* LIPS III supports DPI is 300x300 */
	if (xdpi != LIPS3_DPI_MAX)
	    return_error(gs_error_rangecheck);
    } else if (ptype == BJC880J) {
	if (xdpi < LIPS_DPI_MIN || xdpi > BJC880J_DPI_MAX)
	    return_error(gs_error_rangecheck);
    } else {			/* LIPS4 supprts DPI is 60x60 - 600x600 and 1200x1200 */
	if ((xdpi < LIPS_DPI_MIN || xdpi > LIPS4_DPI_MAX) && xdpi != LIPS4_DPI_SUPERFINE)
	    return_error(gs_error_rangecheck);
    }

    return gdev_prn_open(pdev);
}

static int
lips_close(gx_device * pdev)
{
    gx_device_printer *const ppdev = (gx_device_printer *) pdev;
    gx_device_lips *const lips = (gx_device_lips *) pdev;

    gdev_prn_open_printer(pdev, 1);

    fprintf(ppdev->file, "%c0J%c", LIPS_DCS, LIPS_ST);
    if (lips->pjl)
	fprintf(ppdev->file,
		"%c%%-12345X"
		"@PJL SET LPARM : LIPS SW2 = OFF\n"
		"@PJL EOJ\n"
		"%c%%-12345X", LIPS_ESC, LIPS_ESC);

    return gdev_prn_close(pdev);
}

/* Get properties for the lips drivers. */
static int
lips_get_params(gx_device * pdev, gs_param_list * plist)
{
    gx_device_lips *const lips = (gx_device_lips *) pdev;
    int code = lprn_get_params(pdev, plist);
    int ncode;
    gs_param_string usern;

    if (code < 0)
	return code;

    if ((ncode = param_write_int(plist, LIPS_OPTION_CASSETFEED,
				 &lips->cassetFeed)) < 0)
	code = ncode;

    if ((ncode = param_write_bool(plist, LIPS_OPTION_PJL,
				  &lips->pjl)) < 0)
	code = ncode;

    if ((ncode = param_write_int(plist, LIPS_OPTION_TONERDENSITY,
				 &lips->toner_density)) < 0)
	code = ncode;

    if (lips->toner_saving_set >= 0 &&
	(code = (lips->toner_saving_set ?
     param_write_bool(plist, LIPS_OPTION_TONERSAVING, &lips->toner_saving) :
		 param_write_null(plist, LIPS_OPTION_TONERSAVING))) < 0)
	code = ncode;

    if (code < 0)
	return code;

    usern.data = (const byte *)lips->Username,
	usern.size = strlen(lips->Username),
	usern.persistent = false;

    return param_write_string(plist, LIPS_OPTION_USER_NAME, &usern);
}

static int
lips4_get_params(gx_device * pdev, gs_param_list * plist)
{
    gx_device_lips4 *const lips4 = (gx_device_lips4 *) pdev;
    int code = lips_get_params(pdev, plist);
    int ncode;
    gs_param_string pmedia;

    if (code < 0)
	return code;

    if ((ncode = param_write_int(plist, LIPS_OPTION_NUP,
				 &lips4->nup)) < 0)
	code = ncode;

    if ((ncode = param_write_bool(plist, LIPS_OPTION_FACEUP,
				  &lips4->faceup)) < 0)
	code = ncode;

    if (code < 0)
	return code;

    pmedia.data = (const byte *)lips4->mediaType,
	pmedia.size = strlen(lips4->mediaType),
	pmedia.persistent = false;

    return param_write_string(plist, LIPS_OPTION_MEDIATYPE, &pmedia);
}

/* Put properties for the lips drivers. */
static int
lips_put_params(gx_device * pdev, gs_param_list * plist)
{
    gx_device_lips *const lips = (gx_device_lips *) pdev;
    int ecode = 0;
    int code;
    gs_param_name param_name;
    int cass = lips->cassetFeed;
    bool pjl = lips->pjl;
    int toner_density = lips->toner_density;
    bool toner_saving = lips->toner_saving;
    bool toner_saving_set = lips->toner_saving_set;
    gs_param_string usern;

    switch (code = param_read_int(plist,
				  (param_name = LIPS_OPTION_CASSETFEED),
				  &cass)) {
	case 0:
	    if (cass < -1 || cass > 17 || (cass > 3 && cass < 10))
		ecode = gs_error_rangecheck;
	    else
		break;
	    goto casse;
	default:
	    ecode = code;
	  casse:param_signal_error(plist, param_name, ecode);
	case 1:
	    break;
    }

    if ((code = param_read_bool(plist,
				(param_name = LIPS_OPTION_PJL),
				&pjl)) < 0)
	param_signal_error(plist, param_name, ecode = code);

    switch (code = param_read_int(plist,
				  (param_name = LIPS_OPTION_TONERDENSITY),
				  &toner_density)) {
	case 0:
	    if (toner_density < 0 || toner_density > 8)
		ecode = gs_error_rangecheck;
	    else
		break;
	    goto tden;
	default:
	    ecode = code;
	  tden:param_signal_error(plist, param_name, ecode);
	case 1:
	    break;
    }

    if (lips->toner_saving_set >= 0)
	switch (code = param_read_bool(plist, (param_name = LIPS_OPTION_TONERSAVING),
				       &toner_saving)) {
	    case 0:
		toner_saving_set = 1;
		break;
	    default:
		if ((code = param_read_null(plist, param_name)) == 0) {
		    toner_saving_set = 0;
		    break;
		}
		ecode = code;
		param_signal_error(plist, param_name, ecode);
	    case 1:
		break;
	}
    switch (code = param_read_string(plist,
				     (param_name = LIPS_OPTION_USER_NAME),
				     &usern)) {
	case 0:
	    if (usern.size > LIPS_USERNAME_MAX) {
		ecode = gs_error_limitcheck;
                goto userne;
	    } else {		/* Check the validity of ``User Name'' characters */
		int i;

		for (i = 0; i < usern.size; i++)
		    if (usern.data[i] < 0x20 ||
			usern.data[i] > 0x7e
		    /*
		       && usern.data[i] < 0xa0) ||
		       usern.data[i] > 0xfe
		     */
			) {
			ecode = gs_error_rangecheck;
			goto userne;
		    }
	    }
	    break;
	default:
	    ecode = code;
	  userne:param_signal_error(plist, param_name, ecode);
	case 1:
	    usern.data = 0;
	    break;
    }

    if (ecode < 0)
	return ecode;
    code = lprn_put_params(pdev, plist);
    if (code < 0)
	return code;

    lips->cassetFeed = cass;
    lips->pjl = pjl;
    lips->toner_density = toner_density;
    lips->toner_saving = toner_saving;
    lips->toner_saving_set = toner_saving_set;

    if (usern.data != 0 &&
	bytes_compare(usern.data, usern.size,
		      (const byte *)lips->Username, strlen(lips->Username))
	) {
	memcpy(lips->Username, usern.data, usern.size);
	lips->Username[usern.size] = 0;
    }
    return 0;
}

static int
lips4_put_params(gx_device * pdev, gs_param_list * plist)
{
    gx_device_lips4 *const lips4 = (gx_device_lips4 *) pdev;
    int ecode = 0;
    int code;
    gs_param_name param_name;
    gs_param_string pmedia;
    bool nup = lips4->nup;
    bool faceup = lips4->faceup;
    int old_bpp = pdev->color_info.depth;
    int bpp = 0;

    switch (code = param_read_int(plist,
				  (param_name = LIPS_OPTION_NUP),
				  &nup)) {
	case 0:
	    if (nup != 1 && nup != 2 && nup != 4)
		ecode = gs_error_rangecheck;
	    else
		break;
	    goto nupe;
	default:
	    ecode = code;
	  nupe:param_signal_error(plist, param_name, ecode);
	case 1:
	    break;
    }

    if ((code = param_read_bool(plist,
				(param_name = LIPS_OPTION_FACEUP),
				&faceup)) < 0)
	param_signal_error(plist, param_name, ecode = code);

    switch (code = param_read_string(plist,
				     (param_name = LIPS_OPTION_MEDIATYPE),
				     &pmedia)) {
	case 0:
	    if (pmedia.size > LIPS_MEDIACHAR_MAX) {
		ecode = gs_error_limitcheck;
	        goto pmediae;
	    } else {   /* Check the validity of ``MediaType'' characters */
		if (strcmp(pmedia.data, "PlainPaper") != 0 &&
		    strcmp(pmedia.data, "OHP") != 0 &&
		    strcmp(pmedia.data, "TransparencyFilm") != 0 &&	/* same as OHP */
		    strcmp(pmedia.data, "GlossyFilm") != 0 &&
		    strcmp(pmedia.data, "CardBoard") != 0
		    ) {
		    ecode = gs_error_rangecheck;
		    goto pmediae;
		}
	    }
	    break;
	default:
	    ecode = code;
	  pmediae:param_signal_error(plist, param_name, ecode);
	case 1:
	    pmedia.data = 0;
	    break;
    }

    switch (code = param_read_int(plist,
				  (param_name = "BitsPerPixel"),
				  &bpp)) {
	case 0:
	    if (bpp != 1 && bpp != 24)
		ecode = gs_error_rangecheck;
	    else
		break;
	    goto bppe;
	default:
	    ecode = code;
	  bppe:param_signal_error(plist, param_name, ecode);
	case 1:
	    break;
    }

    if (bpp != 0)
      {
	pdev->color_info.depth = bpp;
	pdev->color_info.num_components = ((bpp == 1) ? 1 : 3);
	pdev->color_info.max_gray = (bpp >= 8 ? 255 : 1);
	pdev->color_info.max_color = (bpp >= 8 ? 255 : bpp > 1 ? 1 : 0);
	pdev->color_info.dither_grays = (bpp >= 8 ? 5 : 2);
	pdev->color_info.dither_colors = (bpp >= 8 ? 5 : bpp > 1 ? 2 : 0);
	dev_proc(pdev, map_rgb_color) = ((bpp == 1) ? gdev_prn_map_rgb_color : gx_default_rgb_map_rgb_color);
      }

    if (ecode < 0)
	return ecode;
    code = lips_put_params(pdev, plist);
    if (code < 0)
	return code;

    lips4->nup = nup;
    lips4->faceup = faceup;

    if (pmedia.data != 0 &&
	bytes_compare(pmedia.data, pmedia.size,
		   (const byte *)lips4->mediaType, strlen(lips4->mediaType))
	) {
	memcpy(lips4->mediaType, pmedia.data, pmedia.size);
	lips4->mediaType[pmedia.size] = 0;
    }
    if (bpp != 0 && bpp != old_bpp && pdev->is_open)
	return gs_closedevice(pdev);
    return 0;
}


/* ------ Internal routines ------ */

static int
lips2p_print_page_copies(gx_device_printer * pdev, FILE * prn_stream, int num_copies)
{
    return lips_print_page_copies(pdev, prn_stream, LIPS2P, num_copies);
}

static int
lips3_print_page_copies(gx_device_printer * pdev, FILE * prn_stream, int num_copies)
{
    return lips_print_page_copies(pdev, prn_stream, LIPS3, num_copies);
}

#define NUM_LINES 24		/* raster height */
#define NUM_LINES_4C 256

static int
lips_print_page_copies(gx_device_printer * pdev, FILE * prn_stream, lips_printer_type ptype, int num_copies)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    int code = 0;
    int bpl = gdev_mem_bytes_per_scan_line(pdev);
    int maxY = lprn->BlockLine / lprn->nBh * lprn->nBh;

    /* Initialize printer. */
    lips_job_start(pdev, ptype, prn_stream, num_copies);

    if (!(lprn->CompBuf = gs_malloc(gs_lib_ctx_get_non_gc_memory_t(), bpl * 3 / 2 + 1, maxY, "(CompBuf)")))
	return_error(gs_error_VMerror);


    lprn->NegativePrint = false; /* not support */
    lprn->prev_x = lprn->prev_y = 0;
    code = lprn_print_image(pdev, prn_stream);
    if (code < 0)
	return code;

    gs_free(gs_lib_ctx_get_non_gc_memory_t(), lprn->CompBuf, bpl * 3 / 2 + 1, maxY, "(CompBuf)");

    /* eject page */
    lips_job_end(pdev, prn_stream);


    return 0;
}

static int
bjc880j_print_page_copies(gx_device_printer * pdev, FILE * prn_stream, int num_copies)
{
  return lips4type_print_page_copies(pdev, prn_stream, num_copies, BJC880J);
}

static int
lips4_print_page_copies(gx_device_printer * pdev, FILE * prn_stream, int num_copies)
{
  return lips4type_print_page_copies(pdev, prn_stream, num_copies, LIPS4);
}

static int
lips4type_print_page_copies(gx_device_printer * pdev, FILE * prn_stream, int num_copies, int ptype)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    int code = 0;
    int bpl = gdev_mem_bytes_per_scan_line(pdev);
    int maxY = lprn->BlockLine / lprn->nBh * lprn->nBh;

    /* Initialize printer. */
    lips_job_start(pdev, ptype, prn_stream, num_copies);

    if (pdev->color_info.depth == 1)
      {
	if (!(lprn->CompBuf = gs_malloc(gs_lib_ctx_get_non_gc_memory_t(), bpl * 3 / 2 + 1, maxY, "(CompBuf)")))
	  return_error(gs_error_VMerror);
	if (!(lprn->CompBuf2 = gs_malloc(gs_lib_ctx_get_non_gc_memory_t(), bpl * 3 / 2 + 1, maxY, "(CompBuf2)")))
	  return_error(gs_error_VMerror);

	if (lprn->NegativePrint) {
	  int rm = pdev->width - (dev_l_margin(pdev) + dev_r_margin(pdev)) * pdev->x_pixels_per_inch;
	  int bm = pdev->height - (dev_t_margin(pdev) + dev_b_margin(pdev)) * pdev->y_pixels_per_inch;
	  /* Draw black rectangle */
	  fprintf(prn_stream,
		  "%c{%c%da%c%de%c;;;3}",
		  LIPS_CSI, LIPS_CSI, rm, LIPS_CSI, bm, LIPS_CSI);
	  fprintf(prn_stream, "%c%dj%c%dk",
		  LIPS_CSI, rm, LIPS_CSI, bm);
	}


	lprn->prev_x = lprn->prev_y = 0;
	code = lprn_print_image(pdev, prn_stream);
	if (code < 0)
	  return code;
	
	gs_free(gs_lib_ctx_get_non_gc_memory_t(), lprn->CompBuf, bpl * 3 / 2 + 1, maxY, "(CompBuf)");
	gs_free(gs_lib_ctx_get_non_gc_memory_t(), lprn->CompBuf2, bpl * 3 / 2 + 1, maxY, "(CompBuf2)");
      }
    else
      {
	code = lips4c_output_page(pdev, prn_stream);
	
	if (code < 0)
	  return code;
      }

    /* eject page */
    lips_job_end(pdev, prn_stream);


    return 0;
}

#if 0
/* Send the page to the printer.  */
static int
lips4c_print_page_copies(gx_device_printer * pdev, FILE * prn_stream, int num_copies)
{
    int code;

    lips_job_start(pdev, BJC880J, prn_stream, num_copies);

    /* make output data */
    code = lips4c_output_page(pdev, prn_stream);

    if (code < 0)
	return code;

    /* eject page */
    lips_job_end(pdev, prn_stream);

    return 0;
}
#endif

static void
move_cap(gx_device_printer * pdev, FILE * prn_stream, int x, int y)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;

    if (x != lprn->prev_x) {
	if (x > lprn->prev_x)
	    fprintf(prn_stream, "%c%da", LIPS_CSI, x - lprn->prev_x);
	else
	    fprintf(prn_stream, "%c%dj", LIPS_CSI, lprn->prev_x - x);

	lprn->prev_x = x;
    }
    if (y != lprn->prev_y) {
	if (y > lprn->prev_y)
	    fprintf(prn_stream, "%c%de", LIPS_CSI, y - lprn->prev_y);
	else
	    fprintf(prn_stream, "%c%dk", LIPS_CSI, lprn->prev_y - y);

	lprn->prev_y = y;
    }
}

static void
draw_bubble(FILE * prn_stream, int width, int height)
{
    /* Draw a rectangle */
    fprintf(prn_stream,
	    "%c{%c%da%c%de%c}",
	    LIPS_CSI, LIPS_CSI, width, LIPS_CSI, height, LIPS_CSI);
    fprintf(prn_stream, "%c%dj%c%dk",
	    LIPS_CSI, width, LIPS_CSI, height);
}


#if 0
/* Non Compression Version of image_out */
static void
lips_image_out(gx_device_printer * pdev, FILE * prn_stream, int x, int y, int width, int height)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;

    int i, j;
    byte *p;
    int maxY = lprn->BlockLine / lprn->nBh * lprn->nBh;

    move_cap(pdev, prn_stream, x, y);

    fprintf(prn_stream, "%c%d;%d;%d.r", LIPS_CSI,
	    width / 8 * height, width / 8, (int)pdev->x_pixels_per_inch);

    for (i = 0; i < height; i++) {
	p = lprn->ImageBuf + ((i + y) % maxY) * raster;
	for (j = 0; j < width / 8; j++) {
	    fputc(p[j + data_x], prn_stream);
	}
    }

    if (lprn->ShowBubble)
	draw_bubble(prn_stream, width, height);
}
#endif

static void
lips2p_image_out(gx_device_printer * pdev, FILE * prn_stream, int x, int y, int width, int height)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    int Len;
    char raw_str[32];		/* LIPS command header (uncompress) */
    char comp_str[32];		/* LIPS command header (compress) */

    move_cap(pdev, prn_stream, x, y);

    Len = lips_mode3format_encode(lprn->TmpBuf, lprn->CompBuf, width / 8 * height);
    sprintf(raw_str, "%c%d;%d;%d.r", LIPS_CSI,
	    width / 8 * height, width / 8, (int)pdev->x_pixels_per_inch);
    sprintf(comp_str, "%c%d;%d;%d;9;%d.r", LIPS_CSI,
	    Len, width / 8, (int)pdev->x_pixels_per_inch, height);

    if (Len < width / 8 * height - strlen(comp_str) + strlen(raw_str)) {
	fprintf(prn_stream, "%s", comp_str);
	fwrite(lprn->CompBuf, 1, Len, prn_stream);
    } else {
	/* compression result is bad. */
	fprintf(prn_stream, "%s", raw_str);
	fwrite(lprn->TmpBuf, 1, width / 8 * height, prn_stream);
    }

    if (lprn->ShowBubble)
	draw_bubble(prn_stream, width, height);
}

static void
lips4_image_out(gx_device_printer * pdev, FILE * prn_stream, int x, int y, int width, int height)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    int Len, Len_rle;
    char raw_str[32];		/* LIPS command header (uncompress) */
    char comp_str[32];		/* LIPS command header (compress) */

    move_cap(pdev, prn_stream, x, y);

    Len = lips_packbits_encode(lprn->TmpBuf, lprn->CompBuf, width / 8 * height);
    Len_rle = lips_rle_encode(lprn->TmpBuf, lprn->CompBuf2, width / 8 * height);

    sprintf(raw_str, "%c%d;%d;%d.r", LIPS_CSI,
	    width / 8 * height, width / 8, (int)pdev->x_pixels_per_inch);

    if (Len < Len_rle) {
	sprintf(comp_str, "%c%d;%d;%d;11;%d.r", LIPS_CSI,
		Len, width / 8, (int)pdev->x_pixels_per_inch, height);
	if (Len < width / 8 * height - strlen(comp_str) + strlen(raw_str)) {
	    fprintf(prn_stream, "%s", comp_str);
	    fwrite(lprn->CompBuf, 1, Len, prn_stream);
	} else {
	    /* compression result is bad. */
	    fprintf(prn_stream, "%s", raw_str);
	    fwrite(lprn->TmpBuf, 1, width / 8 * height, prn_stream);
	}
    } else {
	sprintf(comp_str, "%c%d;%d;%d;10;%d.r", LIPS_CSI,
		Len, width / 8, (int)pdev->x_pixels_per_inch, height);
	if (Len_rle < width / 8 * height - strlen(comp_str) + strlen(raw_str)) {
	    fprintf(prn_stream, "%s", comp_str);
	    fwrite(lprn->CompBuf2, 1, Len, prn_stream);
	} else {
	    /* compression result is bad. */
	    fprintf(prn_stream, "%s", raw_str);
	    fwrite(lprn->TmpBuf, 1, width / 8 * height, prn_stream);
	}
    }

    if (lprn->ShowBubble)
	draw_bubble(prn_stream, width, height);
}

static int
lips4c_write_raster(gx_device_printer * pdev, FILE * prn_stream, byte * pBuff, byte * prevBuff, byte * ComBuff, byte * TotalBuff, byte * diffBuff, int lnum, int raster_height)
{
    int bits_per_pixel = pdev->color_info.depth;
    int num_components = bits_per_pixel > 8 ? 3 : 1;
    int nBytesPerLine = gdev_prn_raster(pdev);
    int Xpixel = nBytesPerLine / num_components;
    int TotalLen = 0;
    int num_zerobyte = 0;
    bool zerobyte_flag = false;
    int i, y, Len;

    for (i = 0; i < nBytesPerLine; i++) {
	*(prevBuff + i) = 0x00;	/* initialize */
    }

    for (y = lnum; y < lnum + raster_height; y++) {
	gdev_prn_copy_scan_lines(pdev, y, pBuff, nBytesPerLine);

	Len = lips_delta_encode(pBuff,
				prevBuff, ComBuff, diffBuff,
				Xpixel * num_components);

	if (Len == 2 && *ComBuff == 0x01) {
	    if (zerobyte_flag == false) {
		zerobyte_flag = true;
		TotalLen = lips_byte_cat(TotalBuff, ComBuff, TotalLen, Len);
	    } else {
		if (num_zerobyte > 255) {
		    TotalLen = lips_byte_cat(TotalBuff, ComBuff, TotalLen, Len);
		} else {
		    *(TotalBuff + TotalLen - 1) = num_zerobyte;
		}
		num_zerobyte++;
	    }
	} else {
	    TotalLen = lips_byte_cat(TotalBuff, ComBuff, TotalLen, Len);
	    zerobyte_flag = false;
	    num_zerobyte = 0;
	}
    }

    fprintf(prn_stream, "%c%d;%d;%d;12;%d;;%d;%d;;1.r", LIPS_CSI,
	    TotalLen, Xpixel, (int)pdev->x_pixels_per_inch,
	    raster_height,
	    bits_per_pixel / num_components,
	    num_components < 3 ? 0 : 10);
    fwrite(TotalBuff, 1, TotalLen, prn_stream);
    fputc(0x85, prn_stream);	/* CR + LF */

    return 0;
}

static int
lips4c_output_page(gx_device_printer * pdev, FILE * prn_stream)
{
    byte *pBuff, *ComBuff, *prevBuff, *TotalBuff, *diffBuff;
    int bits_per_pixel = pdev->color_info.depth;
    int num_components = bits_per_pixel > 8 ? 3 : 1;
    int nBytesPerLine = gdev_prn_raster(pdev);
    int Xpixel = nBytesPerLine / num_components;
    int lnum = 0;

    /* Memory Allocate */
    if (!(pBuff = (byte *) gs_malloc(gs_lib_ctx_get_non_gc_memory_t(), nBytesPerLine, sizeof(byte), "lips4c_compress_output_page(pBuff)")))
	return_error(gs_error_VMerror);
    if (!(prevBuff = (byte *) gs_malloc(gs_lib_ctx_get_non_gc_memory_t(), nBytesPerLine, sizeof(byte), "lips4c_compress_output_page(prevBuff)")))
	return_error(gs_error_VMerror);
    if (!(ComBuff = (byte *) gs_malloc(gs_lib_ctx_get_non_gc_memory_t(), Xpixel * num_components + (Xpixel * num_components + 127) * 129 / 128, sizeof(byte), "lips4c_compress_output_page(ComBuff)")))
	return_error(gs_error_VMerror);
    if (!(TotalBuff = (byte *) gs_malloc(gs_lib_ctx_get_non_gc_memory_t(), (Xpixel * num_components + (Xpixel * num_components + 127) * 129 / 128) * NUM_LINES_4C, sizeof(byte), "lips4c_compress_output_page(TotalBuff)")))
	return_error(gs_error_VMerror);
    if (!(diffBuff = (byte *) gs_malloc(gs_lib_ctx_get_non_gc_memory_t(), Xpixel * num_components * 2, sizeof(byte), "lips_print_page")))
	return_error(gs_error_VMerror);

    /* make output data */
    while (lnum < pdev->height) {
	lips4c_write_raster(pdev, prn_stream, pBuff, prevBuff, ComBuff,
			    TotalBuff, diffBuff, lnum, NUM_LINES_4C);
	lnum += NUM_LINES_4C;
    }

    if (pdev->height - (lnum - NUM_LINES_4C) > 0) {
	lips4c_write_raster(pdev, prn_stream, pBuff, prevBuff, ComBuff,
			    TotalBuff, diffBuff, lnum - NUM_LINES_4C,
			    pdev->height - (lnum - NUM_LINES_4C));
    }
    /* Free Memory */
    gs_free(gs_lib_ctx_get_non_gc_memory_t(), pBuff, nBytesPerLine, sizeof(byte), "lips4c_compress_output_page(pBuff)");
    gs_free(gs_lib_ctx_get_non_gc_memory_t(), prevBuff, nBytesPerLine, sizeof(byte), "lips4c_compress_output_page(prevBuff)");
    gs_free(gs_lib_ctx_get_non_gc_memory_t(), ComBuff, Xpixel * num_components + (Xpixel * num_components + 127) * 129 / 128, sizeof(byte), "lips4c_compress_output_page(ComBuff)");
    gs_free(gs_lib_ctx_get_non_gc_memory_t(), TotalBuff, (Xpixel * num_components + (Xpixel * num_components + 127) * 129 / 128) * NUM_LINES_4C, sizeof(byte), "lips4c_compress_output_page(TotalBuff)");
    gs_free(gs_lib_ctx_get_non_gc_memory_t(), diffBuff, Xpixel * num_components * 2, sizeof(byte), "lips_print_page");

    return 0;
}


static void
lips_job_start(gx_device_printer * pdev, lips_printer_type ptype, FILE * prn_stream, int num_copies)
{
    gx_device_lips *const lips = (gx_device_lips *) pdev;
    gx_device_lips4 *const lips4 = (gx_device_lips4 *) pdev;
    int prev_paper_size, prev_paper_width, prev_paper_height, paper_size;
    int width = pdev->MediaSize[0];
    int height = pdev->MediaSize[1];
    int tm, lm, rm, bm;

    if (pdev->PageCount == 0) {
	if (lips->pjl) {
	    fprintf(prn_stream,
		    "%c%%-12345X@PJL CJLMODE\n"
		    "@PJL JOB\n", LIPS_ESC);
	    if (ptype == LIPS4) {
		fprintf(prn_stream,
			"%c%%-12345X@PJL CJLMODE\n", LIPS_ESC);
		if ((int)pdev->x_pixels_per_inch == 1200)
		    fprintf(prn_stream, "@PJL SET RESOLUTION = SUPERFINE\n");
		else if ((int)pdev->x_pixels_per_inch == 600)
		    fprintf(prn_stream, "@PJL SET RESOLUTION = FINE\n");
		else if ((int)pdev->x_pixels_per_inch == 300)
		    fprintf(prn_stream, "@PJL SET RESOLUTION = QUICK\n");
	    }
	    if (lips->toner_density)
		fprintf(prn_stream, "@PJL SET TONER-DENSITY=%d\n",
			lips->toner_density);
	    if (lips->toner_saving_set) {
		fprintf(prn_stream, "@PJL SET TONER-SAVING=");
		if (lips->toner_saving)
		    fprintf(prn_stream, "ON\n");
		else
		    fprintf(prn_stream, "OFF\n");
	    }
	    fprintf(prn_stream,
		    "@PJL SET LPARM : LIPS SW2 = ON\n"
		    "@PJL ENTER LANGUAGE = LIPS\n");
	}
	fprintf(prn_stream, "%c%%@", LIPS_ESC);
	if (ptype == LIPS2P)
	    fprintf(prn_stream, "%c21;%d;0J" LIPS2P_STRING LIPS_VERSION "%c",
		    LIPS_DCS, (int)pdev->x_pixels_per_inch, LIPS_ST);
	else if (ptype == LIPS3)
	    fprintf(prn_stream, "%c31;%d;0J" LIPS3_STRING LIPS_VERSION "%c",
		    LIPS_DCS, (int)pdev->x_pixels_per_inch, LIPS_ST);
	else if (ptype == LIPS4)
	    fprintf(prn_stream, "%c41;%d;0J" LIPS4_STRING LIPS_VERSION "%c",
		    LIPS_DCS, (int)pdev->x_pixels_per_inch, LIPS_ST);
	else if (ptype == BJC880J)
	    fprintf(prn_stream, "%c41;%d;0J" BJC880J_STRING LIPS_VERSION "%c",
		    LIPS_DCS, (int)pdev->x_pixels_per_inch, LIPS_ST);

	if (ptype == LIPS4 || ptype == BJC880J)
	  {
	    if (pdev->color_info.depth == 24)
	      fprintf(prn_stream, "%c1\"p", LIPS_CSI);
	    else
	      fprintf(prn_stream, "%c0\"p", LIPS_CSI);
	  }
	fprintf(prn_stream, "%c<", LIPS_ESC);
	fprintf(prn_stream, "%c11h", LIPS_CSI);	/* Size Unit Mode */
    }
    /*                           */
    /* Print Environment Setting */
    /*                           */
    /* Media Selection */
    paper_size = lips_media_selection(width, height);

    if (ptype == BJC880J) {
	/* Paint Memory Mode Setting */
	/* for BJC-680J/BJC-880J */
	if (paper_size == B4_SIZE ||
	    paper_size == B4_SIZE + LANDSCAPE ||
	    paper_size == LEGAL_SIZE ||
	    paper_size == LEGAL_SIZE + LANDSCAPE)
	    /* for BJC-880J */
	    fprintf(prn_stream, "%c3&z", LIPS_CSI);
	else if (paper_size == A3_SIZE ||
		 paper_size == A3_SIZE + LANDSCAPE ||
		 paper_size == LEDGER_SIZE ||
		 paper_size == LEDGER_SIZE + LANDSCAPE)
	    /* for BJC-880J */
	    fprintf(prn_stream, "%c4&z", LIPS_CSI);
	else
	    fprintf(prn_stream, "%c2&z", LIPS_CSI);
    }
    if (ptype == LIPS4) {
	if (strcmp(lips4->mediaType, "PlainPaper") == 0)
	    fprintf(prn_stream, "%c20\'t", LIPS_CSI);
	else if (strcmp(lips4->mediaType, "OHP") == 0 ||
		 strcmp(lips4->mediaType, "TransparencyFilm") == 0)
	    fprintf(prn_stream, "%c40\'t", LIPS_CSI);	/* OHP mode (for LBP-2160) */
	else if (strcmp(lips4->mediaType, "CardBoard") == 0)
	    fprintf(prn_stream, "%c30\'t", LIPS_CSI);	/* CardBoard mode (for LBP-2160) */
	else if (strcmp(lips4->mediaType, "GlossyFilm") == 0)
	    fprintf(prn_stream, "%c41\'t", LIPS_CSI);	/* GlossyFile mode (for LBP-2160) */
    }
    if (ptype == LIPS4 || ptype == BJC880J) {
	if (lips4->ManualFeed ||
	    (strcmp(lips4->mediaType, "PlainPaper") != 0 && strcmp(lips4->mediaType, LIPS_MEDIATYPE_DEFAULT) != 0)) {
	    if (lips->prev_feed_mode != 10)
		fprintf(prn_stream, "%c10q", LIPS_CSI);
	    lips->prev_feed_mode = 10;
	} else {
	    if (lips->prev_feed_mode != lips->cassetFeed)
		fprintf(prn_stream, "%c%dq", LIPS_CSI, lips->cassetFeed);
	    lips->prev_feed_mode = lips->cassetFeed;
	}
    } else if (lips->ManualFeed) {	/* Use ManualFeed */
	if (lips->prev_feed_mode != 1)
	    fprintf(prn_stream, "%c1q", LIPS_CSI);
	lips->prev_feed_mode = 1;
    } else {
	if (lips->prev_feed_mode != lips->cassetFeed)
	    fprintf(prn_stream, "%c%dq", LIPS_CSI, lips->cassetFeed);
	lips->prev_feed_mode = lips->cassetFeed;
    }

    /* Use Verious Paper Size */
    prev_paper_size = lips->prev_paper_size;
    prev_paper_width = lips->prev_paper_width;
    prev_paper_height = lips->prev_paper_height;

    if (prev_paper_size != paper_size) {
	if (paper_size == USER_SIZE) {
	    fprintf(prn_stream, "%c2 I", LIPS_CSI);
	    fprintf(prn_stream, "%c80;%d;%dp", LIPS_CSI,
		    width * 10, height * 10);
	} else if (paper_size == USER_SIZE + LANDSCAPE) {
	    fprintf(prn_stream, "%c2 I", LIPS_CSI);
	    fprintf(prn_stream, "%c81;%d;%dp", LIPS_CSI,
		    height * 10, width * 10);
	} else {
	    fprintf(prn_stream, "%c%dp", LIPS_CSI, paper_size);
	}
    } else if (paper_size == USER_SIZE) {
	if (prev_paper_width != width ||
	    prev_paper_height != height) {
	    fprintf(prn_stream, "%c2 I", LIPS_CSI);
	    fprintf(prn_stream, "%c80;%d;%dp", LIPS_CSI,
		    width * 10, height * 10);
	}
    } else if (paper_size == USER_SIZE + LANDSCAPE) {
	if (prev_paper_width != width ||
	    prev_paper_height != height) {
	    fprintf(prn_stream, "%c2 I", LIPS_CSI);
	    fprintf(prn_stream, "%c81;%d;%dp", LIPS_CSI,
		    height * 10, width * 10);
	}
    }
    /* desired number of copies */
    if (num_copies > 255)
	num_copies = 255;
    if (lips->prev_num_copies != num_copies) {
	fprintf(prn_stream, "%c%dv", LIPS_CSI, num_copies);
	lips->prev_num_copies = num_copies;
    }
    if (ptype == LIPS4) {
	if (lips4->faceup)
	    fprintf(prn_stream, "%c11;12;12~", LIPS_CSI);
    }
    if (ptype == LIPS4) {

	if (pdev->PageCount == 0) {
	    /* N-up Printing */
	    if (lips4->nup != 1) {
		fprintf(prn_stream, "%c%d1;;%do", LIPS_CSI, lips4->nup, paper_size);
	    }
	}
	/* Duplex mode */
	{
	    bool dup = lips4->Duplex;
	    int dupset = lips4->Duplex_set;
	    bool tum = lips4->Tumble;

	    if (dupset && dup) {
		if (lips4->prev_duplex_mode == 0 ||
		    lips4->prev_duplex_mode == 1)
		    fprintf(prn_stream, "%c2;#x", LIPS_CSI);	/* duplex */
		if (!tum) {
		    /* long edge binding */
		    if (lips4->prev_duplex_mode != 2)
			fprintf(prn_stream, "%c0;#w", LIPS_CSI);
		    lips4->prev_duplex_mode = 2;
		} else {
		    /* short edge binding */
		    if (lips4->prev_duplex_mode != 3)
			fprintf(prn_stream, "%c2;#w", LIPS_CSI);
		    lips4->prev_duplex_mode = 3;
		}
	    } else if (dupset && !dup) {
		if (lips4->prev_duplex_mode != 1)
		    fprintf(prn_stream, "%c0;#x", LIPS_CSI);	/* simplex */
		lips4->prev_duplex_mode = 1;
	    }
	}
    }
    if (pdev->PageCount == 0) {
	/* Display text on printer panel */
	fprintf(prn_stream, "%c2y%s%c", LIPS_DCS, lips->Username, LIPS_ST);

	fprintf(prn_stream, "%c11h", LIPS_CSI);	/* Size Unit Mode */

	fprintf(prn_stream, "%c?2;3h", LIPS_CSI);
	/* 2: Disable Auto FF */
	/* 3: Disable Auto CAP Movement */

	fprintf(prn_stream, "%c?1;4;5;6l", LIPS_CSI);
	/* 1: Disable Auto NF */
	/* 4: Disable Auto LF at CR */
	/* 5: Disable Auto CR at LF */
	/* 6: Disable Auto CR at FF */
    }
    if (prev_paper_size != paper_size || paper_size == USER_SIZE ||
	paper_size == USER_SIZE + LANDSCAPE) {
	if (ptype == LIPS4 || ptype == BJC880J) {
	    fprintf(prn_stream, "%c?7;%d I", LIPS_CSI,
		    (int)pdev->x_pixels_per_inch);	/* SelectSizeUnit */
	} else {
	    fprintf(prn_stream, "%c7 I", LIPS_CSI);	/* SelectSizeUnit */
	}

	if (ptype == LIPS4 || ptype == BJC880J)
	  {
	    if (pdev->color_info.depth == 24)
	      fprintf(prn_stream, "%c%d G", LIPS_CSI, NUM_LINES_4C);		/* VMI (dots) */
	    else
	      fprintf(prn_stream, "%c%d G", LIPS_CSI, NUM_LINES);	/* VMI (dots) */
	  }
    }
    if (prev_paper_size != paper_size) {
	/* Top Margin: 63/300 inch + 5 mm */
	tm = (63. / 300. + 5. / MMETER_PER_INCH - dev_t_margin(pdev)) * pdev->x_pixels_per_inch;
	if (tm > 0)
	    fprintf(prn_stream, "%c%dk", LIPS_CSI, tm);
	if (tm < 0)
	    fprintf(prn_stream, "%c%de", LIPS_CSI, -tm);

	/* Left Margin: 5 mm left */
	lm = (5. / MMETER_PER_INCH - dev_l_margin(pdev)) * pdev->x_pixels_per_inch;
	if (lm > 0)
	    fprintf(prn_stream, "%c%dj", LIPS_CSI, lm);
	if (lm < 0)
	    fprintf(prn_stream, "%c%da", LIPS_CSI, -lm);

	/* Set Top/Left Margins */
	fprintf(prn_stream, "%c0;2t", LIPS_CSI);

	/* Bottom Margin: height */
	bm = pdev->height - (dev_t_margin(pdev) + dev_b_margin(pdev)) * pdev->y_pixels_per_inch;
	fprintf(prn_stream, "%c%de", LIPS_CSI, bm);
	/* Right Margin: width */
	rm = pdev->width - (dev_l_margin(pdev) + dev_r_margin(pdev)) * pdev->x_pixels_per_inch;
	fprintf(prn_stream, "%c%da", LIPS_CSI, rm);
	fprintf(prn_stream, "%c1;3t", LIPS_CSI);

	/* move CAP to (0, 0) */
	fprintf(prn_stream, "%c%dk\r", LIPS_CSI, bm);
    }
    lips->prev_paper_size = paper_size;
    lips->prev_paper_width = width;
    lips->prev_paper_height = height;
}

static void
lips_job_end(gx_device_printer * pdev, FILE * prn_stream)
{
    /* Paper eject command */
    fprintf(prn_stream, "\r%c", LIPS_FF);
}

static int lips_delta_compress(byte * inBuff, byte * prevBuff, byte * diffBuff, int Length);

static int
lips_delta_encode(byte * inBuff, byte * prevBuff, byte * outBuff, byte * diffBuff, int Length)
{
    int i, j, k, com_size;



    com_size = lips_delta_compress(inBuff, prevBuff, diffBuff, Length);
    if (com_size < 0) {		/* data is white raster */
	*outBuff = 0x01;
	*(outBuff + 1) = 0000;
	for (k = 0; k < Length; k++)
	    *(prevBuff + k) = 0000;
	return 2;
    } else if (com_size == 0) {	/* data is the same raster */
	*outBuff = 0000;
	return 1;
    }
    for (i = 0; i < com_size / 255; i++) {
	*(outBuff + i) = 0377;
    }

    *(outBuff + i) = (byte) (com_size % 255);

    for (j = 0; j < com_size; j++) {
	*(outBuff + i + j + 1) = *(diffBuff + j);
    }

    for (k = 0; k < Length; k++)
	*(prevBuff + k) = *(inBuff + k);


    return i + j + 1;
}

static int
lips_delta_compress(byte * inBuff, byte * prevBuff, byte * diffBuff, int Length)
{
    int i, j;
    bool zero_flag = TRUE;
    bool same_flag = TRUE;
    int num_bytes = 0;
    int num_commandbyte = 0;
    int size = 0;
    int offset = 0;

    for (i = 0; i < Length; i++) {
	if (*(inBuff + i) != 0x00)
	    zero_flag = FALSE;

	/* Compare Buffer */
	if (*(inBuff + i) != *(prevBuff + i)) {
	    num_bytes++;

	    if (same_flag == TRUE) {
		/* first byte is offset */
		if (offset > 31)
		    *(diffBuff + size) = 0037;
		else
		    *(diffBuff + size) = offset;

		size++;
		num_commandbyte++;

		for (j = 0; j < (offset - 31) / 255; j++) {
		    *(diffBuff + size) = 0377;
		    size++;
		    num_commandbyte++;
		}

		if ((offset - 31) % 255 >= 0) {
		    *(diffBuff + size) = (offset - 31) % 255;
		    size++;
		    num_commandbyte++;
		}
		same_flag = FALSE;

	    }
	} else {
	    same_flag = TRUE;
	    offset++;
	}

	if (num_bytes > 8) {
	    /* write number of data for replace */
	    *(diffBuff + size - num_commandbyte)
		= *(diffBuff + size - num_commandbyte) | 0340;

	    for (j = 0; j < 8; j++, size++) {
		*(diffBuff + size) = *(inBuff + i + j - 8);
	    }

	    /* offset is 0 */
	    *(diffBuff + size) = 0000;
	    size++;

	    num_bytes = 1;
	    same_flag = FALSE;
	    num_commandbyte = 1;
	} else if (same_flag == true && num_bytes > 0) {
	    offset = 1;

	    /* write number of data for replace */
	    *(diffBuff + size - num_commandbyte)
		= *(diffBuff + size - num_commandbyte) | ((num_bytes - 1) << 5);

	    /* write a different bytes */
	    for (j = 0; j < num_bytes; j++, size++) {
		*(diffBuff + size) = *(inBuff + i + j - num_bytes);
	    }
	    num_bytes = 0;
	    num_commandbyte = 0;
	}
    }

    if (num_bytes > 0) {
	/* write number of data for replace */
	*(diffBuff + size - num_commandbyte)
	    = *(diffBuff + size - num_commandbyte) | ((num_bytes - 1) << 5);

	for (j = 0; j < num_bytes; j++, size++) {
	    *(diffBuff + size) = *(inBuff + i + j - num_bytes);
	}
    }
    if (zero_flag)
	return -1;

    return size;
}

/* This routine work like ``strcat'' */
static int
lips_byte_cat(byte * TotalBuff, byte * Buff, int TotalLen, int Len)
{
    int i;

    for (i = 0; i < Len; i++)
	*(TotalBuff + TotalLen + i) = *(Buff + i);

    return TotalLen + Len;
}
