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

/* April 2021 - March 2025, from across The Emerald City
 * ghostscript Apple Dot Matrix Printer / ImageWriter driver, version ][
 *
 * By: Josh Moyer, Mike Galatean, Scott Barker, Jonathan Luckey,
 * Mark Wedel and the authors of the epson and other drivers.
 *
 * This is version ][ (2.0) of the Apple dot-matrix printers driver for
 * ghostscript.  Please see the docs and release notes in the Devices
 * section of the ghostscript documentation.
 *
 * The devices:
 *      appledmp:  120dpi x  72dpi - Dot Matrix Printer / C. Itoh 8510
 *      iwlo:      160dpi x  72dpi - ImageWriter
.*      iwlow:     160dpi x  72dpi - ImageWriter 15" (new in version ][)
 *      iwhi:      160dpi x 144dpi - ImageWriter II
 *      iwhic:     160dpi x 144dpi - ImageWriter II color (new in version ][)
 *      iwlq:      320dpi x 216dpi - ImageWriter LQ
 *      iwlqc      320dpi x 216dpi - ImageWriter LQ color (new in version ][)
 *
 * Thanks for support and inspiration to the folks on the ghostscript
 * developers list, including at least (in no particular order):
 * Chris, Ray, Ken, Robin and William!
 * 
 * Thanks to Chris M. for making available certain documentation
 * for the printers.  Thanks also to Petar P.
 * 
 * And, finally, I d like to thank Microsoft for the decade/quarter
 * century that I ve had the privilege of being able to serve them
 * over and also for enabling me to do more by indirectly supporting
 * this project with the food that they placed on my table -- in addition
 * to the skills that I developed while working for them.
 *
 * This release represents a major feature upgrade and overhaul as compared
 * to the earlier drivers, while retaining as much of the original code as
 * practical.  Please see the Devices section of the ghostscript documentation
 * for a full description of features and instructions.
 *
 * Notable changes and new features include:
 * + Color support for the ImageWriter II and ImageWriter LQ!
 * + -dUNIDIRECTIONAL -- bidirectional by default.
 * + Improved paper alignment, handling and feeding.
 * + Support for multiple paper bins (ImageWriter LQ only.)
 * + Better safety for wide printouts.
 * + Safer margins.
 * + Better compliance with Apple's documentation.
 * + Better error handling.
 * + Improved print parameter checks.
 * + Better and more use of ghostscript APIs, including modern memory
 *   management.
 * + Improved comments and printer command macros in source.
 * + Other minor and miscellaneous changes.
 *
 * Known issues include:
 * + Rarely, some input files produce blank output unexpectedly.  I only ever
 *   saw it once in the course of many varied test prints, so probably an
 *   issue with the input file, rather than the driver.  Please e-mail me if
 *   you see this.
 * + Source lines marked with the word "bug" in them below.  Hopefully, these
 *   will be fixed in the planned and upcoming ][+ release.
 *
 *    _____    Josh Moyer (he/him) <JMoyer@NODOMAIN.NET>
 *   | * * |   http://jmoyer.nodomain.net/
 *   |*(*)*|   http://www.nodomain.net/
 *    \ - /
 *     \//     Love, Responsibility, Justice
 *             Liebe, Verantwortung, Gerechtigkeit
 * 
 *             Please don't eat the animals.
 *             Thanks.
 */

 /* Oct 2019 - April 2021, maintained by Mike Galatean */

 /*
 * Apple DMP / Imagewriter driver
 *
 * This is a modification of Mark Wedel's Apple DMP and
 * Jonathan Luckey's Imagewriter II driver to
 * support the Imagewriter LQ's higher resolution (320x216):
 *      appledmp:  120dpi x  72dpi is still supported (yuck)
 *	iwlo:	   160dpi x  72dpi
 *	iwhi:	   160dpi x 144dpi
 *      iwlq:      320dpi x 216dpi
 *
 * This is also my first attempt to work with gs. I have not included the LQ's
 * ability to print in colour. Perhaps at a later date I will tackle that.
 *
 * BTW, to get your Imagewriter LQ serial printer to work with a PC, attach it
 * with a nullmodem serial cable.
 *
 * Scott Barker (barkers@cuug.ab.ca)
 */

/*
 * This is a modification of Mark Wedel's Apple DMP driver to
 * support 2 higher resolutions:
 *      appledmp:  120dpi x  72dpi is still supported (yuck)
 *	iwlo:	   160dpi x  72dpi
 *	iwhi:	   160dpi x 144dpi
 *
 * The Imagewriter II is a bit odd.  In pinfeed mode, it thinks its
 * First line is 1 inch from the top of the page. If you set the top
 * form so that it starts printing at the top of the page, and print
 * to near the bottom, it thinks it has run onto the next page and
 * the formfeed will skip a whole page.  As a work around, I reverse
 * the paper about a 1.5 inches at the end of the page before the
 * formfeed to make it think its on the 'right' page.  bah. hack!
 *
 * This is  my first attempt to work with gs, so your milage may vary
 *
 * Jonathan Luckey (luckey@rtfm.mlb.fl.us)
 */

/* This is a bare bones driver I developed for my apple Dot Matrix Printer.
 * This code originally was from the epson driver, but I removed a lot
 * of stuff that was not needed.
 *
 * The Dot Matrix Printer was a predecessor to the apple Imagewriter.  Its
 * main difference being that it was parallel.
 *
 * This code should work fine on Imagewriters, as they have a superset
 * of commands compared to the DMP printer.
 *
 * This driver does not produce the smalles output files possible.  To
 * do that, it should look through the output strings and find repeat
 * occurances of characters, and use the escape sequence that allows
 * printing repeat sequences.  However, as I see it, this the limiting
 * factor in printing is not transmission speed to the printer itself,
 * but rather, how fast the print head can move.  This is assuming the
 * printer is set up with a reasonable speed (9600 bps)
 *
 * WHAT THE CODE DOES AND DOES NOT DO:
 *
 * To print out images, it sets the printer for unidirection printing
 * and 15 cpi (120 dpi). IT sets line feed to 1/9 of an inch (72 dpi).
 * When finished, it sets things back to bidirection print, 1/8" line
 * feeds, and 12 cpi.  There does not appear to be a way to reset
 * things to initial values.
 *
 * This code does not set for 8 bit characters (which is required). It
 * also assumes that carriage return/newline is needed, and not just
 * carriage return.  These are all switch settings on the DMP, and
 * I have configured them for 8 bit data and cr only.
 *
 * You can search for the strings Init and Reset to find the strings
 * that set up the printer and clear things when finished, and change
 * them to meet your needs.
 *
 * Also, you need to make sure that the printer daemon (assuming unix)
 * doesn't change the data as it is being printed.  I have set my
 * printcap file (sunos 4.1.1) with the string:
 * ms=pass8,-opost
 * and it works fine.
 *
 * Feel free to improve this code if you want.  However, please make
 * sure that the old DMP will still be supported by any changes.  This
 * may mean making an imagewriter device, and just copying this file
 * to something like gdevimage.c.
 *
 * The limiting factor of the DMP is the vertical resolution.  However, I
 * see no way to do anything about this.  Horizontal resolution could
 * be increased by using 17 cpi (136 dpi).  I believe the Imagewriter
 * supports 24 cpi (192 dpi).  However, the higher dpi, the slower
 * the printing.
 *
 * Dot Matrix Code by Mark Wedel (master@cats.ucsc.edu)
 *
 */

#include "gdevprn.h"

/* Device type macros */
#define DMP 0
#define IWLO 1
#define IWHI 2
#define IWLQ 4

/* Device platen/max line width (in inches) macros */
#define STANDARD 8.5
#define WIDE 13.6

/* Device margin minimum (in points) macro */
#define MARGINMIN 18

/* Device resolution macros */
#define H120V72 8
#define H160V72 16
#define H160V144 32
#define H320V216 64

/* Device rendering mode macros */
#define GPCSL 0 /* gdev_prn_copy_scan_lines */
/* get_bits_rectangle mode died of excess complexity, RIP */
#define GPGL 2 /* gdev_prn_get_lines */
#define GPGLSC 4 /* gdev_prn_get_lines, sans copy */

/* Device command macros */
#define ESC "\033"
#define CR "\r"
#define LF "\n"

#define BIN "@"

#define ELITE "E"
#define ELITEPROPORTIONAL "P"
#define CONDENSED "q"
#define LQPROPORTIONAL "a3"

#define BIDIRECTIONAL "<"
#define UNIDIRECTIONAL ">"

#define LINEHEIGHT "T"
#define LINE6PI "A"

#define BITMAPLO "G"
#define BITMAPLORPT "V"
#define BITMAPHI "C"
#define BITMAPHIRPT "U"

#define COLORSELECT "K"
#define YELLOW "1"      /* (in a recommended order of printing for */
#define CYAN "3"       /* minimizing cross-band color contamination) */
#define MAGENTA "2"
#define BLACK "0"

/* Driver error string macros */
#define DRIVERNAME "gdevadmp"
#define ERRADVNLQ DRIVERNAME ": Near letter quality vertical advance failure.\n"
#define ERRALLOC DRIVERNAME ": Memory allocation failed.\n"
#define ERRBINCMD DRIVERNAME ": Bin command write failed.\n"
#define ERRBINSELECT DRIVERNAME ": Bin out of range: %d.\n"
#define ERRCLEANING DRIVERNAME ": Fatal error, cleaning up.\n" DRIVERNAME ": Please write Josh Moyer <JMoyer@NODOMAIN.NET> for help.\n"
#define ERRCMD DRIVERNAME ": Command failure.\n"
#define ERRCMDLQ DRIVERNAME ": Letter quality command failure.\n"
#define ERRCMDNLQ DRIVERNAME ": Near letter quality command failure.\n"
#define ERRCOLORSELECT DRIVERNAME ": Printhead color selection failed.\n"
#define ERRCR DRIVERNAME ": Carriage return failed.\n"
#define ERRCRLF DRIVERNAME ": Carriage return and line feed failed.\n"
#define ERRDAT DRIVERNAME ": Data failure.\n"
#define ERRDATLQ DRIVERNAME ": Letter quality data failure.\n"
#define ERRDATNLQ DRIVERNAME ": Near letter quality data failure.\n"
#define ERRDEVTYPE DRIVERNAME ": Driver selection failed.\n"
#define ERRDIRSELECT DRIVERNAME ": Setting directionality failed.\n"
#define ERREXITING DRIVERNAME ": Output is likely corrupt -- delete the file or reset your printer.\n" DRIVERNAME ": Exiting.\n"
#define ERRGPCSL DRIVERNAME ": gdev_prn_copy_scan_lines returned an unexpected values:\n" DRIVERNAME ":(code=%d,line_size=%d)\n"
#define ERRGPGL DRIVERNAME ": gdev_prn_get_lines returned an unexpected value: %d\n"
#define ERRHWMARGINS DRIVERNAME ": HWMargins must not be less than 18pt on any side.\n"
#define ERRLHNLQ DRIVERNAME ": Near letter quality line height failure.\n"
#define ERRNUMCOMP DRIVERNAME ": color_info.num_components out of range.\n"
#define ERRPLANEINIT DRIVERNAME ": gx_render_plane_init returned an unexpected value: %d\n"
#define ERRPOS DRIVERNAME ": Positioning failure.\n"
#define ERRPOSLQ DRIVERNAME ": Letter quality positioning failure.\n"
#define ERRPOSNLQ DRIVERNAME ": Near letter quality positioning failure.\n"
#define ERRRENDERMODE DRIVERNAME ": Invalid RENDERMODE specified: %d\n"
#define ERRRESET DRIVERNAME ": Reset failure.\n"
#define ERRREZSELECT DRIVERNAME ": Resolution select failed.\n"
#define ERRUNSAFEMARGINS DRIVERNAME ": -dUNSAFEMARGINS detected -- printer jams or damage may occur.\n"
#define ERRWIDTH DRIVERNAME ": Image too wide for printer: %d of %.0f maximum.\n"

/* Driver parameters macro */
/* unsafemargins off by default */
#define UM 0

/* Device structure */
struct gx_device_admp_s {
        gx_device_common;
        gx_prn_device_common;
        bool unidirectional;
        char bin;
        int rendermode;
        char devtype;
        bool unsafemargins;
        float platen; /* MS compiler warns of truncation with a float,
                        but a double doesn't make sense here, it seems */
};

typedef struct gx_device_admp_s gx_device_admp;

/* Device procedure initialization */
static dev_proc_put_params(admp_put_params);
static dev_proc_get_params(admp_get_params);
static dev_proc_print_page(admp_print_page);

static void
admp_initialize_device_procs(gx_device* pdev)
{
        gdev_prn_initialize_device_procs_mono_bg(pdev);

        set_dev_proc(pdev, get_params, admp_get_params);
        set_dev_proc(pdev, put_params, admp_put_params);
}

static void
admp_initialize_device_procs_color(gx_device* pdev)
{
        gdev_prn_initialize_device_procs_cmyk1_bg(pdev);

        set_dev_proc(pdev, get_params, admp_get_params);
        set_dev_proc(pdev, put_params, admp_put_params);
}

/* Dot Matrix Printer device descriptor */
const gx_device_admp far_data gs_appledmp_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs, "appledmp",
        DEFAULT_WIDTH_10THS_US_LETTER,         /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER,        /* height_10ths, 11" */
        120, 72,                               /* X_DPI, Y_DPI */
        0.25, 0, 0.25, 0.25, 0.25, 0.25,       /* origin and margins */
        1, 1, 1, 0, 2, 1,                      /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        0, -1, GPGL, DMP, UM, STANDARD };      /* unidirectional, bin, rendermode, device type, unsafemargins, platen */

/* ImageWriter device descriptor */
const gx_device_admp far_data gs_iwlo_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs, "iwlo",
        DEFAULT_WIDTH_10THS_US_LETTER,         /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER,        /* height_10ths, 11" */
        160, 72,                               /* X_DPI, Y_DPI */
        0.25, 0, 0.25, 0.25, 0.25, 0.25,       /* origin and margins */
        1, 1, 1, 0, 2, 1,                      /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        0, -1, GPGL, IWLO, UM, STANDARD };     /* unidirectional, bin, rendermode, device type, unsafemargins, platen */

/* ImageWriter 15" device descriptor */
const gx_device_admp far_data gs_iwlow_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs, "iwlow",
        DEFAULT_WIDTH_10THS_US_LETTER,         /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER,        /* height_10ths, 11" */
        160, 72,                               /* X_DPI, Y_DPI */
        0.25, 0, 0.25, 0.25, 0.25, 0.25,       /* origin and margins */
        1, 1, 1, 0, 2, 1,                      /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        0, -1, GPGL, IWLO, UM, WIDE };         /* unidirectional, bin, rendermode, device type, unsafemargins, platen */

/* ImageWriter II device descriptor */
const gx_device_admp far_data gs_iwhi_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs, "iwhi",
        DEFAULT_WIDTH_10THS_US_LETTER,         /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER,        /* height_10ths, 11" */
        160, 144,                              /* X_DPI, Y_DPI */
        0.25, 0, 0.25, 0.25, 0.25, 0.25,       /* origin and margins */
        1, 1, 1, 0, 2, 1,                      /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        0, -1, GPGL, IWHI, UM, STANDARD };     /* unidirectional, bin, rendermode, device type, unsafemargins, platen */

/* ImageWriter II color device descriptor */
const gx_device_admp far_data gs_iwhic_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs_color, "iwhic",
        DEFAULT_WIDTH_10THS_US_LETTER,         /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER,        /* height_10ths, 11" */
        160, 144,                              /* X_DPI, Y_DPI */
        0.25, 0, 0.25, 0.25, 0.25, 0.25,       /* origin and margins */
        4, 4, 1, 1, 2, 2,                      /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        0, -1, GPGL, IWHI, UM, STANDARD };     /* unidirectional, bin, rendermode, device type, unsafemargins, platen */

/* ImageWriter LQ device descriptor */
const gx_device_admp far_data gs_iwlq_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs, "iwlq",
        DEFAULT_WIDTH_10THS_US_LETTER,         /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER,        /* height_10ths, 11" */
        320, 216,                              /* X_DPI, Y_DPI */
        0.25, 0, 0.25, 0.25, 0.25, 0.25,       /* origin and margins */
        1, 1, 1, 0, 2, 1,                      /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        0, 0, GPGL, IWLQ, UM, WIDE };          /* unidirectional, bin, rendermode, device type, unsafemargins, platen */

/* ImageWriter LQ color device descriptor */
const gx_device_admp far_data gs_iwlqc_device = {
prn_device_margins_body(gx_device_admp, admp_initialize_device_procs_color, "iwlqc",
        DEFAULT_WIDTH_10THS_US_LETTER,         /* width_10ths, 8.5" */
        DEFAULT_HEIGHT_10THS_US_LETTER,        /* height_10ths, 11" */
        320, 216,                              /* X_DPI, Y_DPI */
        0.25, 0, 0.25, 0.25, 0.25, 0.25,       /* origin and margins */
        4, 4, 1, 1, 2, 2,                      /* maxcomp, depth, maxgray, maxcolor, numgray, numcolor */
        admp_print_page),
        0, 0, GPGL, IWLQ, UM, WIDE };          /* unidirectional, bin, rendermode, device type, unsafemargins, platen */

static int
admp_get_params(gx_device* pdev, gs_param_list* plist)
{
        /* Init */
        int code = gdev_prn_get_params(pdev, plist);

        /* Write params */
        if (code < 0 ||
                (code = param_write_bool(plist, "UNIDIRECTIONAL", &((gx_device_admp*)pdev)->unidirectional)) < 0 ||
                (code = param_write_bool(plist, "UNSAFEMARGINS", &((gx_device_admp*)pdev)->unsafemargins)) < 0 ||
                (code = param_write_int(plist, "RENDERMODE", &((gx_device_admp*)pdev)->rendermode)) < 0 )
                return code;

        return code;
}

static int
admp_put_params(gx_device* pdev, gs_param_list* plist)
{
        /* Init */
        int code = 0, ecode = 0;
        bool unidirectional = ((gx_device_admp*)pdev)->unidirectional;
        bool unsafemargins = ((gx_device_admp*)pdev)->unsafemargins;
        int rendermode = ((gx_device_admp*)pdev)->rendermode;
        int bin = ((gx_device_admp*)pdev)->bin;
        gs_param_name param_name;

        /* Read params */
        if ((code = param_read_bool(plist,
                (param_name = "UNIDIRECTIONAL"),
                &unidirectional)) < 0) {
                param_signal_error(plist, param_name, ecode = code);
        }
        if (ecode < 0)
                return code;
        ((gx_device_admp*)pdev)->unidirectional = unidirectional;

        if ((code = param_read_bool(plist,
                (param_name = "UNSAFEMARGINS"),
                &unsafemargins)) < 0) {
                param_signal_error(plist, param_name, ecode = code);
        }
        if (ecode < 0)
                return code;
        ((gx_device_admp*)pdev)->unsafemargins = unsafemargins;

        if ((code = param_read_int(plist,
                (param_name = "RENDERMODE"),
                &rendermode)) < 0) {
                param_signal_error(plist, param_name, ecode = code);
        }
        if (ecode < 0)
                return code;
        ((gx_device_admp*)pdev)->rendermode = rendermode;

        if ((code = param_read_int(plist,
                (param_name = "%MediaSource"),
                &bin)) < 0) {
                param_signal_error(plist, param_name, ecode = code);
        }
        if (ecode < 0)
                return code;
        ((gx_device_admp*)pdev)->bin = bin;

        /* Check and fix-up Margin[s], see */
        /* https://bugs.ghostscript.com/show_bug.cgi?id=707616 */
        pdev->Margins[1] = 0;

        switch (((gx_device_admp*)pdev)->devtype)
        {
        case IWLQ:
                if (pdev->HWResolution[0] == 320)
                        pdev->Margins[0] = -80;
                if (pdev->HWResolution[0] == 160)
                        pdev->Margins[0] = -40;
                if (pdev->HWResolution[0] == 120)
                        pdev->Margins[0] = -30;
                break;
        case IWHI:
                if (pdev->HWResolution[0] == 160)
                        pdev->Margins[0] = -40;
                if (pdev->HWResolution[0] == 120)
                        pdev->Margins[0] = -30;
                break;
        case IWLO:
                if (pdev->HWResolution[0] == 160)
                        pdev->Margins[0] = -40;
                if (pdev->HWResolution[0] == 120)
                        pdev->Margins[0] = -30;
                break;
        default:
                if (pdev->HWResolution[0] == 120)
                        pdev->Margins[0] = -30;
                break;
        }

        return gdev_prn_put_params(pdev, plist);
}

static int
/* Send the page to the printer output file. */
admp_print_page(gx_device_printer* pdev, gp_file* gprn_stream)
{
        /* Init */
        int code, dev_rez, lnum;
        unsigned char color = 0;
        char pcmd[255];
        int line_size = gdev_mem_bytes_per_scan_line((gx_device*)pdev);
        int in_size = line_size * 8; /* Note that in_size is a multiple of 8 dots in height. */
        unsigned char color_order[4] = { -1, -1, -1, -1 };
        gx_render_plane_t render_planes[4] = {{ -1, -1, -1}, { -1, -1, -1}, { -1, -1, -1}, { -1, -1, -1}};
        byte* buf_in = NULL;
        byte* buf_out = NULL;
        byte* row = NULL;
        byte* prn = NULL;
        byte* in, * out;

        if (line_size > max_int / 24)
            return_error(gs_error_rangecheck);

        /* Allocate memory */
        row = (byte*)gs_alloc_bytes(pdev->memory, line_size, "admp_print_page(row)");
        buf_in = (byte*)gs_alloc_bytes(pdev->memory, in_size, "admp_print_page(buf_in)");
        buf_out = (byte*)gs_alloc_bytes(pdev->memory, in_size, "admp_print_page(buf_out)");
        prn = (byte*)gs_alloc_bytes(pdev->memory, in_size * 3, "admp_print_page(prn)");

        if (row == NULL ||
                buf_in == NULL ||
                buf_out == NULL ||
                prn == NULL)
        {
                (void)errprintf(pdev->memory, ERRALLOC);
                code = gs_note_error(gs_error_VMerror);
                goto xit;
        }

        /* Set-up for color or monochrome */
        switch (pdev->color_info.num_components)
        {
        case 4:
                /* Use the YCMK order in an attempt to reduce
                 * cross-band contamination on the ribbon
                 */
                color_order[0] = 2;
                color_order[1] = 0;
                color_order[2] = 1;
                color_order[3] = 3;
                break;
        case 1:
                color_order[0] = 0;
                break;
        default:
                (void)errprintf(pdev->memory, ERRNUMCOMP);
                code = gs_note_error(gs_error_rangecheck);
                goto xit;
        }

        /* Set up some pointers */
        in = buf_in;
        out = buf_out;

        /* Init */
        lnum = 0;

        /* Check that the image is not too wide for the printer */
        if (pdev->width > ((gx_device_admp*)pdev)->platen * pdev->HWResolution[0])
        {
                (void)errprintf(pdev->memory, ERRWIDTH, pdev->width, ((gx_device_admp*)pdev)->platen * pdev->HWResolution[0]);
                code = gs_note_error(gs_error_rangecheck);
                goto xit;
        }

        /* Check HWMargins */
        if (((gx_device_admp*)pdev)->unsafemargins == 0 &&
                (pdev->HWMargins[0] < MARGINMIN ||
                        pdev->HWMargins[1] < MARGINMIN ||
                        pdev->HWMargins[2] < MARGINMIN ||
                        pdev->HWMargins[3] < MARGINMIN))
        {
                (void)errprintf(pdev->memory, ERRHWMARGINS);
                code = gs_note_error(gs_error_rangecheck);
                goto xit;
        }

        /* Notify user if -dUNSAFEMARGINS */
        if (((gx_device_admp*)pdev)->unsafemargins == 1)
                (void)errprintf(pdev->memory, ERRUNSAFEMARGINS);

        /* Select paper bin (ImageWriter LQ only) */
        if (((gx_device_admp*)pdev)->devtype == IWLQ)
        {
                switch (((gx_device_admp*)pdev)->bin)
                {
                case 0:
                        (void)strcpy(pcmd, ESC BIN "0");
                        break;
                case 1:
                        (void)strcpy(pcmd, ESC BIN "1");
                        break;
                case 2:
                        (void)strcpy(pcmd, ESC BIN "2");
                        break;
                default:
                        (void)errprintf(pdev->memory, ERRBINSELECT, ((gx_device_admp*)pdev)->bin);
                        code = gs_note_error(gs_error_rangecheck);
                        goto xit;
                }

                code = gp_fputs(pcmd, gprn_stream);

                if (code != strlen(pcmd))
                {
                        (void)errprintf(pdev->memory, ERRBINCMD);
                        code = gs_note_error(gs_error_ioerror);
                        goto xit;
                }
        }

        /* Initialize the printer by setting line-height and
         * print-head direction.
         */
        if (((gx_device_admp*)pdev)->unidirectional)
        {
                (void)strcpy(pcmd, ESC UNIDIRECTIONAL ESC LINEHEIGHT "16");
                code = gp_fputs(pcmd, gprn_stream);
        }
        else
        {
                (void)strcpy(pcmd, ESC BIDIRECTIONAL ESC LINEHEIGHT "16");
                code = gp_fputs(pcmd, gprn_stream);
        }

        if (code != strlen(pcmd))
        {
                (void)errprintf(pdev->memory, ERRDIRSELECT);
                code = gs_note_error(gs_error_ioerror);
                goto xit;
        }

        /* Configure resolution */
        switch ((int)pdev->y_pixels_per_inch)
        {
        case 216:
                if (pdev->x_pixels_per_inch == 320 &&
                        ((gx_device_admp*)pdev)->devtype == IWLQ)
                {
                        dev_rez = H320V216;
                        break;
                }
        case 144:
                if (pdev->x_pixels_per_inch == 160 &&
                        ((gx_device_admp*)pdev)->devtype >= IWHI)
                {
                        dev_rez = H160V144;
                        break;
                }
        case 72:
                if (pdev->x_pixels_per_inch == 160 &&
                        ((gx_device_admp*)pdev)->devtype >= IWLO)
                {
                        dev_rez = H160V72;
                        break;
                }

                if (pdev->x_pixels_per_inch == 120 &&
                        ((gx_device_admp*)pdev)->devtype >= DMP)
                {
                        dev_rez = H120V72;
                        break;
                }
        default:
                (void)errprintf(pdev->memory, ERRREZSELECT);
                code = gs_note_error(gs_error_rangecheck);
                goto xit;
        }

        /* Write resolution/character pitch to output file */
        switch (dev_rez)
        {
        case H320V216:
                (void)strcpy(pcmd, ESC ELITEPROPORTIONAL ESC LQPROPORTIONAL);
                code = gp_fputs(pcmd, gprn_stream);
                break;
        case H160V144: /* falls through */
        case H160V72:
                (void)strcpy(pcmd, ESC ELITEPROPORTIONAL);
                code = gp_fputs(pcmd, gprn_stream);
                break;
        case H120V72: /* falls through */
        default:
                (void)strcpy(pcmd, ESC CONDENSED);
                code = gp_fputs(pcmd, gprn_stream);
                break;
        }

        if (code != strlen(pcmd))
        {
                (void)errprintf(pdev->memory, ERRREZSELECT);
                code = gs_note_error(gs_error_ioerror);
                goto xit;
        }

        /* Pre-initialize the render planes */
        for (color = 0; color < pdev->color_info.num_components; color++)
        {
                code = gx_render_plane_init(&render_planes[color_order[color]], (gx_device*)pdev, color_order[color]);

                if (code != 0)
                {
                        (void)errprintf(pdev->memory, ERRPLANEINIT, code);
                        code = gs_note_error(gs_error_rangecheck);
                        goto xit;
                }
        }

        /* Bug: Zero the scanline buffer because the output file fills
         * with patterns of 0xFF and 0x00 otherwise.  Additionally,
         * not zeroing the row causes the 320x216 color IWLQ test
         * case to hit ERRCMDLQ.  (When using new GPGL rendermode.)
         */
        (void)memset(row, 0, line_size);

        /* For each scan line in the main buffer... */
        while (lnum < pdev->height)
        {
                byte* actual_row, * inp, * in_end, * out_end, * prn_blk, * prn_end, * prn_tmp;
                int count, lcnt, ltmp, passes;
                uint actual_line_size;

                /* 8 raster tall bands per pass */
                switch (dev_rez)
                {
                case H320V216: passes = 3; break;
                case H160V144: passes = 2; break;
                case H160V72: /* falls through */
                case H120V72: /* falls through */
                default: passes = 1; break;
                }

                /* For each colorant, copy, transpose, copy and write a band of dots
                 * Bug: Too many copies in here...
                 */
                for (color = 0; color < pdev->color_info.num_components; ++color)
                {
                        /* Get rendered scanlines from each plane and
                         * assemble into 8, 16, or 24-dot high bands
                         */
                        for (count = 0; count < passes; count++)
                        {
                                /* For each line in an 8-dot high band*/
                                for (lcnt = 0; lcnt < 8; lcnt++)
                                {
                                        /* Select band count/height */
                                        switch (dev_rez)
                                        {
                                        case H320V216: ltmp = lcnt + 8 * count; break;
                                        case H160V144: ltmp = 2 * lcnt + count; break;
                                        case H160V72: /* falls through */
                                        case H120V72: /* falls through */
                                        default: ltmp = lcnt; break;
                                        }

                                        if ((lnum + ltmp) > pdev->height)
                                                /* If we're past end of page,
                                                 * write a blank line, ignoring
                                                 * the return value.
                                                 */
                                                (void)memset(in + lcnt * line_size, 0, line_size);
                                        else /* otherwise, get scan lines */
                                        {
                                                /* The Apple printers seem to be odd in that the bit order on
                                                 * each line is reverse what might be expected.  Meaning, an
                                                 * underscore would be done as a series of 0x80, while on overscore
                                                 * would be done as a series of 0x01.  So we get each
                                                 * scan line in reverse order.
                                                 */

                                                 /* Declare in_start here because the macOS 10.13.6 compiler doesn't
                                                  * seem to like variable declarations inside switch statements.
                                                  */
                                                byte* in_start;

                                                /* copy a scanline to the buffer using the given rendermode */
                                                switch (((gx_device_admp*)pdev)->rendermode)
                                                {
                                                case GPGL: /* gdev_prn_get_lines */
                                                        /* Bug: Here, we retrieve the scanlines with y=1 and then a copy.
                                                         * It would be better to avoid the copy and set y=8 (or maybe 24
                                                         * for the LQ.)  However, this code works.
                                                         */
                                                        code = gdev_prn_get_lines(pdev,
                                                                lnum + ltmp,
                                                                1,
                                                                row,
                                                                line_size,
                                                                &actual_row,
                                                                &actual_line_size,
                                                                &render_planes[color_order[color]]);

                                                        if (code != 0 || actual_line_size != line_size)
                                                        {
                                                                (void)errprintf(pdev->memory, ERRGPGL, code);
                                                                code = gs_note_error(gs_error_rangecheck);
                                                                goto xit;
                                                        }

                                                        /* Compute input buffer offset */
                                                        in_start = in + line_size * (7 - lcnt);

                                                        /* Copy row to input buffer */
                                                        (void)memcpy(in_start, row, line_size);

                                                        break;

                                                case GPGLSC: /* gdev_prn_get_lines. sans copy */
                                                        /* Here, we try to avoid the copy and use the
                                                         * original method of allocating a large buffer and
                                                         * copying into it in reverse order, but the in
                                                         * buffer gets filled with garbage, for reasons
                                                         * still unknown.  Maybe we'll fix this in release ][+
                                                         */
                                                        code = gdev_prn_get_lines(pdev,
                                                                lnum + ltmp,
                                                                1,
                                                                in + line_size * (7 - lcnt),
                                                                line_size,
                                                                &actual_row,
                                                                &actual_line_size,
                                                                &render_planes[color_order[color]]);

                                                        if (code != 0 || actual_line_size != line_size)
                                                        {
                                                                (void)errprintf(pdev->memory, ERRGPGL, code);
                                                                code = gs_note_error(gs_error_rangecheck);
                                                                goto xit;
                                                        }

                                                        break;

                                                case GPCSL: /* gdev_prn_copy_scan_lines - legacy code path, monochrome only */
                                                        code = gdev_prn_copy_scan_lines(
                                                                pdev,
                                                                (lnum + ltmp),
                                                                in + line_size * (7 - lcnt),
                                                                line_size);

                                                        if (code != 1)
                                                        {
                                                                (void)errprintf(pdev->memory, ERRGPCSL, code, line_size);
                                                                code = gs_note_error(gs_error_rangecheck);
                                                                goto xit;
                                                        }
                                                        break;

                                                default: /* Invalid rendering mode */
                                                        (void)errprintf(pdev->memory, ERRRENDERMODE, ((gx_device_admp*)pdev)->rendermode);
                                                        code = gs_note_error(gs_error_rangecheck);
                                                        goto xit;
                                                        break;
                                                }
                                        } /* else */
                                } /* for lcnt */

                                /* Here is the full input buffer.  It requires transposition
                                 * in order to conform with the printer's native graphics format.
                                 */
                                out_end = out;
                                inp = in;
                                in_end = inp + line_size;
                                for (; inp < in_end; inp++, out_end += 8)
                                {
                                        /* gdev_prn_transpose(...) returns void */
                                        gdev_prn_transpose_8x8(inp, line_size, out_end, 1);
                                }

                                /* Here is the output buffer, fully transposed */
                                out_end = out;

                                /* Determine the size of the final print buffer. */
                                switch (dev_rez)
                                {
                                case H320V216: prn_end = prn + count; break;
                                case H160V144: prn_end = prn + in_size * count; break;
                                case H160V72: /* falls through */
                                case H120V72: /* falls through */
                                default: prn_end = prn; break;
                                }

                                /* Copy the output buffer to the final print buffer,
                                 * but don't use memcpy, possibly for some reason
                                 * that a different author did not elaborate on.
                                 */
                                while ((int)(out_end - out) < in_size)
                                {
                                        *prn_end = *(out_end++);
                                        if ((dev_rez) == H320V216) prn_end += 3;
                                        else prn_end++;
                                }
                        } /* for count */

                        /* Select ribbon color */
                        if (pdev->color_info.num_components == 4)
                        {
                                switch (color_order[color])
                                {
                                case 3: (void)strcpy(pcmd, ESC COLORSELECT BLACK); break;
                                case 2: (void)strcpy(pcmd, ESC COLORSELECT YELLOW); break;
                                case 1: (void)strcpy(pcmd, ESC COLORSELECT MAGENTA); break;
                                case 0: (void)strcpy(pcmd, ESC COLORSELECT CYAN); break;
                                }

                                code = gp_fputs(pcmd, gprn_stream);

                                if (code != strlen(pcmd))
                                {
                                        (void)errprintf(pdev->memory, ERRCOLORSELECT);
                                        code = gs_note_error(gs_error_ioerror);
                                        goto xit;
                                }
                        }

                        /* Send bitmaps to printer, based on resolution setting

                        Note: each of the three vertical resolutions has a
                        unique and separate implementation, but each also
                        follows a similar pattern.
                        */

                        switch (dev_rez)
                        {
                        case H320V216:
                                /* Set start of prn_blk */
                                prn_blk = prn;
                                /* calculate end */
                                prn_end = prn_blk + in_size * 3;

                                /* Determine right edge of bitmap data,
                                 * advancing 3 bytes at a time
                                 */
                                while (prn_end > prn && prn_end[-1] == 0 &&
                                        prn_end[-2] == 0 && prn_end[-3] == 0)
                                        prn_end -= 3;

                                /* Determine left edge of bitmap data,
                                 * advancing 3 bytes at a time
                                 */
                                while (prn_blk < prn_end && prn_blk[0] == 0 &&
                                        prn_blk[1] == 0 && prn_blk[2] == 0)
                                        prn_blk += 3;

                                /* Send bitmaps to printer, if any */
                                if (prn_end != prn_blk)
                                {
                                        /* if at least 8 bytes of bits */
                                        if ((prn_blk - prn) > 7)
                                        {
                                                /* Set print-head position by sending a repeated all-zeros hirez bitmap */
                                                code = gp_fprintf(gprn_stream,
                                                        ESC BITMAPHIRPT "%04d%c%c%c",
                                                        (int)((prn_blk - prn) / 3),
                                                        0, 0, 0);

                                                if (code != 9)
                                                {
                                                        (void)errprintf(pdev->memory, ERRPOSLQ);
                                                        code = gs_note_error(gs_error_ioerror);
                                                        goto xit;
                                                }
                                        }
                                        else
                                                prn_blk = prn;

                                        /* Send hirez bitmapped graphics mode command (with length parameter) */
                                        code = gp_fprintf(gprn_stream,
                                                ESC BITMAPHI "%04d",
                                                (int)((prn_end - prn_blk) / 3));

                                        if (code != 6)
                                        {
                                                (void)errprintf(pdev->memory, ERRCMDLQ);
                                                code = gs_note_error(gs_error_ioerror);
                                                goto xit;
                                        }

                                        /* Send actual bitmap data */
                                        code = gp_fwrite(prn_blk,
                                                1,
                                                (int)(prn_end - prn_blk),
                                                gprn_stream);

                                        if (code != (int)(prn_end - prn_blk))
                                        {
                                                (void)errprintf(pdev->memory, ERRDATLQ);
                                                code = gs_note_error(gs_error_ioerror);
                                                goto xit;
                                        }
                                }
                                break;
                        case H160V144:
                                /* Alternate even and odd rows */
                                for (count = 0; count < 2; count++)
                                {
                                        /* Set start of prn_blk and prn_tmp */
                                        prn_blk = prn_tmp = prn + in_size * count;
                                        /* Calculate end */
                                        prn_end = prn_blk + in_size;

                                        /* Determine right edge of bitmap data,
                                         * advancing 1 byte at a time
                                         */
                                        while (prn_end > prn_blk && prn_end[-1] == 0)
                                                prn_end--;

                                        /* Determine left edge of bitmap data,
                                         * advancing 1 byte at a time
                                         */
                                        while (prn_blk < prn_end && prn_blk[0] == 0)
                                                prn_blk++;

                                        /* Send bitmaps to printer, if any */
                                        if (prn_end != prn_blk)
                                        {
                                                /* if at least 8 bytes of bits */
                                                if ((prn_blk - prn_tmp) > 7)
                                                {
                                                        /* Set print-head position by sending a repeated all-zeros lorez bitmap */
                                                        code = gp_fprintf(gprn_stream,
                                                                ESC BITMAPLORPT "%04d%c",
                                                                (int)(prn_blk - prn_tmp),
                                                                0);

                                                        if (code != 7)
                                                        {
                                                                (void)errprintf(pdev->memory, ERRPOSNLQ);
                                                                code = gs_note_error(gs_error_ioerror);
                                                                goto xit;
                                                        }
                                                }
                                                else
                                                        prn_blk = prn_tmp;

                                                /* Send lorez bitmapped graphics mode command (with length parameter) */
                                                code = gp_fprintf(gprn_stream,
                                                        ESC BITMAPLO "%04d",
                                                        (int)(prn_end - prn_blk));

                                                if (code != 6)
                                                {
                                                        (void)errprintf(pdev->memory, ERRCMDNLQ);
                                                        code = gs_note_error(gs_error_ioerror);
                                                        goto xit;
                                                }

                                                /* Send actual bitmap data */
                                                code = gp_fwrite(prn_blk,
                                                        1,
                                                        (int)(prn_end - prn_blk),
                                                        gprn_stream);

                                                if (code != (int)(prn_end - prn_blk))
                                                {
                                                        (void)errprintf(pdev->memory, ERRDATNLQ);
                                                        code = gs_note_error(gs_error_ioerror);
                                                        goto xit;
                                                }
                                        } /* if prn_end != prn_blk */

                                        /* Issue a 1/144th" line feed on every
                                         * other line for double-density graphics
                                         */
                                        if (!count)
                                        {
                                                /* For V144, advance the page
                                                 * 1/144th of an inch for the
                                                 * first NLQ pass, but only if
                                                 * the current color is black.
                                                 */
                                                if (pdev->color_info.num_components == 4 && color < 3)
                                                        code = gp_fputs(CR,
                                                                gprn_stream);
                                                else
                                                        code = gp_fputs(ESC LINEHEIGHT "01" LF,
                                                                gprn_stream);

                                                if (code != 1 && code != 5)
                                                {
                                                        (void)errprintf(pdev->memory, ERRADVNLQ);
                                                        code = gs_note_error(gs_error_ioerror);
                                                        goto xit;
                                                }
                                        }
                                } /* for count = 0,... */

                                /* Then set the line-height for the remaining
                                 * 15/144ths to finish the 2nd NLQ pass.
                                 */
                                if (pdev->color_info.num_components == 4 && color < 3)
                                {
                                        // do nothing
                                }
                                else
                                {
                                        code = gp_fputs(ESC LINEHEIGHT "15",
                                                gprn_stream);

                                        if (code != 4)
                                        {
                                                (void)errprintf(pdev->memory, ERRLHNLQ);
                                                code = gs_note_error(gs_error_ioerror);
                                                goto xit;
                                        }
                                }
                                break;
                        case H160V72:
                        case H120V72:
                        default:
                                /* Set start of prn_blk */
                                prn_blk = prn;
                                /* calculate end */
                                prn_end = prn_blk + in_size;

                                /* Determine right edge of bitmap data,
                                 * advancing 1 byte at a time
                                 */
                                while (prn_end > prn_blk && prn_end[-1] == 0)
                                        prn_end--;

                                /* Determine left edge of bitmap data,
                                 * advancing 1 byte at a time
                                 */
                                while (prn_blk < prn_end && prn_blk[0] == 0)
                                        prn_blk++;

                                /* Send bitmaps to printer, if any */
                                if (prn_end != prn_blk)
                                {
                                        /* if at least 8 bytes of bits */
                                        if ((prn_blk - prn) > 7)
                                        {
                                                /* Set print-head position by sending a repeated all-zeros lorez bitmap */
                                                code = gp_fprintf(gprn_stream,
                                                        ESC BITMAPLORPT "%04d%c",
                                                        (int)(prn_blk - prn),
                                                        0);

                                                if (code != 7)
                                                {
                                                        (void)errprintf(pdev->memory, ERRPOS);
                                                        code = gs_note_error(gs_error_ioerror);
                                                        goto xit;
                                                }
                                        }
                                        else
                                                prn_blk = prn;

                                        /* Send lorez bitmapped graphics mode command (with length parameter) */
                                        code = gp_fprintf(gprn_stream,
                                                ESC BITMAPLO "%04d",
                                                (int)(prn_end - prn_blk));

                                        if (code != 6)
                                        {
                                                (void)errprintf(pdev->memory, ERRCMD);
                                                code = gs_note_error(gs_error_ioerror);
                                                goto xit;
                                        }

                                        /* Send actual bitmap data */
                                        code = gp_fwrite(prn_blk,
                                                1,
                                                (int)(prn_end - prn_blk),
                                                gprn_stream);

                                        if (code != (int)(prn_end - prn_blk))
                                        {
                                                (void)errprintf(pdev->memory, ERRDAT);
                                                code = gs_note_error(gs_error_ioerror);
                                                goto xit;
                                        }
                                } /* if prn_end != prn blk */
                                break;
                        } /* switch dev_rez */

                        /* if color and the color is not black */
                        if (pdev->color_info.num_components == 4 && color < 3)
                        {
                                /* Reset the print-head position for the next band */
                                code = gp_fputs(CR, gprn_stream);
                                if (code != 1)
                                {
                                        (void)errprintf(pdev->memory, ERRCR);
                                        code = gs_note_error(gs_error_ioerror);
                                        goto xit;
                                }
                        }
                        else /* Color is black */
                        {
                                /* Reset the print-head position and advance the page */
                                code = gp_fputs(CR LF, gprn_stream);
                                if (code != 2)
                                {
                                        (void)errprintf(pdev->memory, ERRCRLF);
                                        code = gs_note_error(gs_error_ioerror);
                                        goto xit;
                                }
                        }
                } /* for color */

                /* Set lnum to reflect the number of 1-dot tall bands printed */
                switch (dev_rez)
                {
                case H320V216: lnum += 24; break;
                case H160V144: lnum += 16; break;
                case H160V72: /* falls through */
                case H120V72: /* falls through */
                default: lnum += 8; break;
                }
        } /* while lnum < pdev->height */

        /* Page should auto-eject since there were line-feeds for every
         * band -- no need for a form-feed.
         *
         * Reset printer with factory defaults and elite character pitch.
         */
        code = gp_fputs(ESC BIDIRECTIONAL ESC LINE6PI ESC ELITE,
                gprn_stream);

        if (code != 6)
        {
                (void)errprintf(pdev->memory, ERRRESET);
                code = gs_note_error(gs_error_ioerror);
                goto xit;
        }

        /* gp_fflush(...) returns void */
        (void)gp_fflush(gprn_stream);

        /* everything was OK! */
        code = gs_error_ok;

        /* Free memory, not checking retvals because gs_free_object is a macro */
        gs_free_object(pdev->memory, prn, "admp_print_page(prn)");
        gs_free_object(pdev->memory, buf_out, "admp_print_page(buf_out)");
        gs_free_object(pdev->memory, buf_in, "admp_print_page(buf_in)");
        gs_free_object(pdev->memory, row, "admp_print_page(row)");

        /* Exit normally */
        return code;

        /* Error exit path */
xit:
        /* Tell the user that we're cleaning up. */
        (void)errprintf(pdev->memory, ERRCLEANING);

        /* Free memory, not checking retvals because gs_free_object is a macro */
        gs_free_object(pdev->memory, prn, "admp_print_page(prn)");
        gs_free_object(pdev->memory, buf_out, "admp_print_page(buf_out)");
        gs_free_object(pdev->memory, buf_in, "admp_print_page(buf_in)");
        gs_free_object(pdev->memory, row, "admp_print_page(row)");

        /* Don't check return values here,
         * just close the device */
        (void)gs_closedevice((gx_device*)pdev);

        /* Here, it would be very Apple-ish to delete the
         * output file, since it's in an incomplete state.
         * However, the commented out solution seems to have
         * the potential to delete /dev/tty or similar, if
         * the user is directly printing to the port.  So,
         * just tell the user not to print the file in
         * ERREXITING.
         */
         /* (void)unlink(pdev->fname); */

         /* Tell the user we're exiting. */
        (void)errprintf(pdev->memory, ERREXITING);

        /* Exit abnormally */
        exit(code);
}
