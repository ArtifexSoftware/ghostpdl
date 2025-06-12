/* BJC-210/240/250/265/1000 Bubble Jet Printer driver for GhostScript
 * main subroutines for GS
 *
 * Copyright 2000, 2001, 2002 Gergely Sz�sz (Gergely Sza'sz)
 * mailto://szaszg@hu.inter.net http://bjc250gs.sourceforge.net
 *
 *   This program may be distributed and/or modified under the terms of
 *   the GNU General Public License as published by the Free Software
 *   Foundation (the "GPL"); either version 2 of the GPL, or (at your option)
 *   any later version.
 *
 *   When distributed under the terms of the GPL, this program is distributed
 *   in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 *   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *   PURPOSE.  See the GPL for more details.
 *
 *   If this program is being distributed under the terms of the GPL, you
 *   should have received a copy of the GPL along with this program, normally
 *   in a plain ASCII text file named COPYING; if not, write to the Free
 *   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111
 *   U.S.A.
 */

/* Copyright (C) 1989, 2000 Aladdin Enterprises.  All rights reserved.

   This program may also be distributed as part of AFPL Ghostscript, under the
   terms of the Aladdin Free Public License (the "License").

   AFPL Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No
   author or distributor accepts any responsibility for the consequences of
   using it, or for whether it serves any particular purpose or works at
   all, unless he or she says so in writing.  Refer to the License for full
   details.

   Every copy of AFPL Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute AFPL Ghostscript, but only
   under certain conditions described in the License.  Among other things,
   the License requires that the copyright notice and this notice be
   preserved on all copies.
*/

/* BJC printers drivers */
#include "gdevprn.h"
#include "gsparam.h"
#include "gdevbjc_.h"

/* ------ The device descriptors ------ */
static dev_proc_print_page(bjc_print_page_mono);
static dev_proc_print_page(bjc_print_page_gray);
static dev_proc_print_page(bjc_print_page_cmyk);
static dev_proc_print_page(bjc_print_page_color);
static dev_proc_put_params(gdev_bjc_put_params);
static dev_proc_get_params(gdev_bjc_get_params);
const stringParamDescription *
 paramValueToParam(const stringParamDescription *, int);
const stringParamDescription *
 paramStringToParam(const stringParamDescription *, const char *, uint);

BJL_command BJL_command_set[] = {
    { "@Cleaning=1",            BJC_BJL_CLEANING,      11},
    { "@PowerOff",              BJC_BJL_POWER_OFF,      9},
    { "@RollerCleaning",        BJC_BJL_ROCLEANING,    15},
    { "@TestPrint=NozzleCheck", BJC_BJL_NOZZLECHK,     22},
    { "@TestPrint=A",           BJC_BJL_TESTA,         12},
    { "@TestPrint=B",           BJC_BJL_TESTB,         12},
    { "@TestPrint=C",           BJC_BJL_TESTC,         12},
    { "@TestPrint=DemoPrint",   BJC_BJL_DEMO,          20},

#define BJL_CMC_AP  "ControlMode=Common\012AutoPower"  /*+28*/

    { BJL_CMC_AP "On=Enable",   BJC_BJL_ON_ENABLE,     37},
    { BJL_CMC_AP "On=Disable",  BJC_BJL_ON_DISABLE,    38},
    { BJL_CMC_AP "Off=1",       BJC_BJL_OFF_1MIN,      33},
    { BJL_CMC_AP "Off=10",      BJC_BJL_OFF_10MIN,     34},
    { BJL_CMC_AP "Off=30",      BJC_BJL_OFF_30MIN,     34},
    { BJL_CMC_AP "Off=60",      BJC_BJL_OFF_60MIN,     34},
    { BJL_CMC_AP "Off=Disable", BJC_BJL_OFF_DISABLE,   39},
    { NULL }
};

/* String parameter definitions */

stringParamDescription strPrinterType[] = {
    { { (const byte *)"BJC-250",    7, false },   	0 },
    { { (const byte *)"BJC-250ex",  9, false }, 	1 },
    { { (const byte *)"BJC-1000",   8, false },  	2 },

    { { (const byte *)"250",   3, false },       	0 },
    { { (const byte *)"250ex", 5, false },     	1 },
    { { (const byte *)"1000",  4, false },      	2 },
    { {0} }
};

stringParamDescription strFeeder[] = {
    { { (const byte *)"Manual",	6, false },    	0x11 },
    { { (const byte *)"Auto",        	4, false },    	0x10 },

    { { (const byte *)"m",		1, false },    	0x11 },
    { { (const byte *)"a",		1, false },    	0x10 },
    { {0} }
};

stringParamDescription strQuality[] = {
    { { (const byte *)"Normal",	6, false },    	0 },
    { { (const byte *)"High",        	4, false },    	1 },
    { { (const byte *)"Draft",        5, false },    	2 },
    { { (const byte *)"NonBleed",   	8, false },    	8 },

    { { (const byte *)"n",		1, false },    	0 },
    { { (const byte *)"h",		1, false },    	1 },
    { { (const byte *)"d",		1, false },    	2 },
    { { (const byte *)"b",		1, false },    	8 },
    { {0} }
};

stringParamDescription strInk[] = {
    { { (const byte *)"Black",	5, false },    	8 },
    { { (const byte *)"Cyan",        	4, false },    	1 },
    { { (const byte *)"Magenta",      7, false },    	2 },
    { { (const byte *)"Yellow",   	6, false },    	4 },
    { { (const byte *)"Red",        	3, false },    	6 },
    { { (const byte *)"Green",        5, false },    	5 },
    { { (const byte *)"Blue",   	4, false },    	3 },

    { { (const byte *)"K",		1, false },    	8 },
    { { (const byte *)"C",		1, false },    	1 },
    { { (const byte *)"M",		1, false },    	2 },
    { { (const byte *)"Y",		1, false },    	4 },
    { { (const byte *)"R",		1, false },    	6 },
    { { (const byte *)"G",		1, false },    	5 },
    { { (const byte *)"B",		1, false },    	3 },
    { { (const byte *)"CK",		2, false },    	9 },
    { { (const byte *)"MK",		2, false },    10 },
    { { (const byte *)"YK",		2, false },    12 },
    { { (const byte *)"RK",		2, false },    14 },
    { { (const byte *)"GK",		2, false },    13 },
    { { (const byte *)"BK",		2, false },    11 },
    { { (const byte *)"CMY",		3, false },     7 },
    { { (const byte *)"CMYK",		4, false },    15 },
    { {0} }
};

static stringParamDescription strMedia[] = {
  {{(const byte *)"PlainPaper", 10, false}, 0},
  {{(const byte *)"CoatedPaper", 11, false}, 1},
  {{(const byte *)"TransparencyFilm", 16, false}, 2},
  {{(const byte *)"BackprintFilm", 13, false}, 3},
  {{(const byte *)"T-ShirtTransfer", 15, false}, 3},
  {{(const byte *)"FabricSheet", 11, false}, 4},
  {{(const byte *)"GlossyPaper", 11, false}, 5},
  {{(const byte *)"GlossyPhotoPaper", 16, false}, 5},
  {{(const byte *)"HighGlossPaper", 14, false}, 5},
  {{(const byte *)"HighGlossyFilm", 14, false}, 6},
  {{(const byte *)"Envelope", 8, false}, 7},
  {{(const byte *)"OtherPaper", 10, false}, 8},
  {{(const byte *)"HighResolutionPaper", 19, false}, 9},
/*    { { (const byte *)"HighResPaper",		12, false },    11 }, */
  {{(const byte *)"GlossyPhotoCard", 15, false}, 10},
/*    { { (const byte *)"FullBleed",		 9, false },    12 }, */
  {{(const byte *)"Banner", 6, false}, 11},

  {{(const byte *)"Plain", 5, false}, 0},
  {{(const byte *)"Coated", 6, false}, 1},
  {{(const byte *)"Trans", 5, false}, 2},
  {{(const byte *)"Back", 4, false}, 3},
  {{(const byte *)"Shirt", 4, false}, 3},
/*    { { (const byte *)"Lead",			 4, false },    4 }, */
  {{(const byte *)"Fabric", 6, false}, 4},
  {{(const byte *)"Glossy", 6, false}, 5},
  {{(const byte *)"HGloss", 6, false}, 6},
  {{(const byte *)"Env", 3, false}, 7},
  {{(const byte *)"Oth", 3, false}, 8},
  {{(const byte *)"HiRes", 5, false}, 9},
/*    { { (const byte *)"Bleed",		 5, false },    12 }, */
  {{(const byte *)"Card", 4, false}, 10},
  {{(const byte *)"Ban", 3, false}, 11},

  {{(const byte *)"p", 1, false}, 0},
  {{(const byte *)"c", 1, false}, 1},
  {{(const byte *)"t", 1, false}, 2},
  {{(const byte *)"b", 1, false}, 3},
  {{(const byte *)"s", 1, false}, 3},
  {{(const byte *)"f", 1, false}, 4},
  {{(const byte *)"g", 1, false}, 5},
  {{(const byte *)"F", 1, false}, 6},
  {{(const byte *)"e", 1, false}, 7},
  {{(const byte *)"o", 1, false}, 8},
  {{(const byte *)"h", 1, false}, 9},
  {{(const byte *)"C", 1, false}, 10},
  {{(const byte *)"B", 1, false}, 11},
  {{0}}
};

static media_t media_codes[] = {
  {0x00, 0x00},			/* Plain paper */
  {0x10, 0x10},			/* Coated paper */
  {0x20, 0x20},			/* Transp */
  {0x30, 0x30},			/* Backprint or T-shirt */
  {0x50, 0x40},			/* Fabric */
  {0x60, 0x50},			/* Glossy (Photo) Paper */
  {0x70, 0x60},			/* High gloss film */
  {0x80, 0x00},			/* Envelope */
  {0x90, 0x10},			/* Other */
  {0xb0, 0x70},			/* Hi Res */
  {0xc0, 0x50},			/* Glossy Photo Cards */
  {0xd0, 0x00},			/* Banner */
};

/***************************************************************************/
/* ------------------------- 1 bit Monochrome ---------------------------- */
/***************************************************************************/

static void
bjcmono_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, get_params, gdev_bjc_get_params);
    set_dev_proc(dev, put_params, gdev_bjc_put_params);
    set_dev_proc(dev, map_color_rgb, gx_default_w_b_map_color_rgb);
    set_dev_proc(dev, map_rgb_color, gx_default_gray_fast_encode);
    set_dev_proc(dev, encode_color, gx_default_gray_fast_encode);
    set_dev_proc(dev, decode_color, gx_default_w_b_map_color_rgb);
}


const gx_device_bjc_printer gs_bjcmono_device =
bjc_device(bjcmono_initialize_device_procs, "bjcmono",
           DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
           X_DPI, Y_DPI,
           (float)(3.4 / 25.4), (float)(7.0 / 25.4),
           (float)(3.4 / 25.4), (float)(3.0 / 25.4),
           1,                   /* num components */
           1,                   /* depth  */
           1,                   /* max gray */
           0,                   /* max color */
           2,                   /* dither gray */
           0,                   /* dither color */
           bjc_print_page_mono, INK_K);  /* printer rutin, default ink */

/***************************************************************************/
/* -------------------------- 8 bit Grayscale ---------------------------- */
/***************************************************************************/

static void
bjcgray_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray(dev);

    set_dev_proc(dev, get_params, gdev_bjc_get_params);
    set_dev_proc(dev, put_params, gdev_bjc_put_params);

    /* The prn macros used in previous versions of the code leave
     * encode_color and decode_color set to NULL (which are then rewritten
     * by the system to the default. For compatibility we do the same. */
    set_dev_proc(dev, encode_color, NULL);
    set_dev_proc(dev, decode_color, NULL);
}

const gx_device_bjc_printer gs_bjcgray_device =
bjc_device(bjcgray_initialize_device_procs, "bjcgray",
           DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
           X_DPI, Y_DPI,
           (float)(3.4 / 25.4), (float)(7.0 / 25.4),
           (float)(3.4 / 25.4), (float)(3.0 / 25.4),
           1,                   /* num components */
           8,                   /* depth  */
           255,                 /* max gray */
           0,                   /* max color */
           256,                 /* dither gray */
           0,                   /* dither color */
           bjc_print_page_gray, INK_K);

/***************************************************************************/
/* --------------------------- 3 bit CMYK Color -------------------------- */
/***************************************************************************/

static void
bjccmyk_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_cmyk1(dev);

    set_dev_proc(dev, get_params, gdev_bjc_get_params);
    set_dev_proc(dev, put_params, gdev_bjc_put_params);

    /* The prn macros used in previous versions of the code leave
     * encode_color and decode_color set to NULL (which are then rewritten
     * by the system to the default. For compatibility we do the same. */
    set_dev_proc(dev, encode_color, NULL);
    set_dev_proc(dev, decode_color, NULL);
}

const gx_device_bjc_printer gs_bjccmyk_device =
bjc_device(bjccmyk_initialize_device_procs, "bjccmyk",
           DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
           X_DPI, Y_DPI,
           (float)(3.4 / 25.4), (float)(7.0 / 25.4),
           (float)(3.4 / 25.4), (float)(3.0 / 25.4),
           4,                   /* num components */
           4,                   /* depth  */
           1,                   /* max gray */
           1,                   /* max color */
           2,                   /* dither gray */
           2,                   /* dither color */
           bjc_print_page_cmyk, (INK_K|INK_C|INK_M|INK_Y));

/***************************************************************************/
/* --------------------------- 24 bit TrueColor -------------------------- */
/***************************************************************************/

static void
bjc_truecolor_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_cmyk8(dev);

    set_dev_proc(dev, get_params, gdev_bjc_get_params);
    set_dev_proc(dev, put_params, gdev_bjc_put_params);

    /* The prn macros used in previous versions of the code leave
     * encode_color and decode_color set to NULL (which are then rewritten
     * by the system to the default. For compatibility we do the same. */
    set_dev_proc(dev, encode_color, NULL);
    set_dev_proc(dev, decode_color, NULL);
}

const gx_device_bjc_printer gs_bjccolor_device =
bjc_device(bjc_truecolor_initialize_device_procs, "bjccolor",
           DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
           X_DPI, Y_DPI,
           (float)(3.4 / 25.4), (float)(7.0 / 25.4),
           (float)(3.4 / 25.4), (float)(3.0 / 25.4),
           4,                   /* num components */
           32,                  /* depth  */
           255,                 /* max gray */
           255,                 /* max color */
           256,                 /* dither gray */
           256,                 /* dither color */
           bjc_print_page_color, (INK_K|INK_C|INK_M|INK_Y));

/***************************************************************************/
/* ---------------------------Print a page routine------------------------ */
/***************************************************************************/

/*
static int
bjc_print_page(gx_device_printer * pdev, gp_file * file)
{
#define ppdev ((gx_device_bjc_printer *) pdev)
#define prdev ((gx_device_printer *) pdev)

    uint raster = gdev_prn_raster(pdev);
    uint bjc_raster = raster + (-raster & 3);
    byte *row = gs_alloc_bytes(pdev->memory, bjc_raster, "bjc file buffer");
    int y;
    int code;
    char alma[512];

    sprintf(alma, "\nNumC: %d, Depth: %d, Mgray: %d, Mrgb: %d\n"
            "Dgray: %d, Drgb: %d", pdev->color_info.num_components,
            pdev->color_info.depth,
            pdev->color_info.max_gray,
            pdev->color_info.max_color,
            pdev->color_info.dither_grays,
            pdev->color_info.dither_colors
           );

    if (row == 0)
        return_error(gs_error_VMerror);

done:
    gs_free_object(pdev->memory, row, "bjc file buffer");

    gp_fwrite((const char *) alma, 512, 1, file);
    return code;

#undef ppdev
}
*/

void
bjc_put_bjl_command(gp_file * file, int bjl_command)
{
    BJL_command *command = BJL_command_set;
    for( ; command->string; command++)
        if(command->numeric == bjl_command) break;
    if(command->string) {
      gp_fwrite((const char *)"\033[K\002\000\000\037BJLSTART\012", 16, 1, file);
      gp_fwrite(command->string, command->length, 1, file);
      gp_fwrite((const char *)"\012BJLEND\012", 8, 1, file); }
}

/* ------ Get/put parameters ------ */

/* Functions for manipulation params strings */

const stringParamDescription *
paramValueToParam(const stringParamDescription * params, int value)
{

    for (; params->p_string.data; ++params) {
        if (params->p_value == value) {
            return params;
        }
    }

    return (stringParamDescription *) NULL;
}

const stringParamDescription *
paramStringToParam(const stringParamDescription * params,
                   const char * name, uint len)
{
    for (; params->p_string.data; ++params) {
        if (len == params->p_string.size)
            if (!(strncmp((const char *)params->p_string.data,
                          name, len))) {
                return params;
            }
    }

    return (stringParamDescription *) NULL;
}

/* Get parameters.  BJC printer devices add several more parameters */
/* to the default set. */
static int
gdev_bjc_get_params(gx_device * pdev, gs_param_list * plist)
{
    const gx_device_bjc_printer * ppdev = (gx_device_bjc_printer *)pdev;

    int code;
    (void) (
        (code = gdev_prn_get_params(pdev, plist)) < 0 ||
        (code = param_write_string(plist, "PrinterType",
                 &paramValueToParam(strPrinterType, ppdev->printerType)->p_string)) < 0 ||
        (code = param_write_string(plist, "Feeder",
                 &paramValueToParam(strFeeder, ppdev->feeder)->p_string)) < 0 ||
        (code = param_write_string(plist, "Media",
                 &paramValueToParam(strMedia, ppdev->mediaType)->p_string)) < 0 ||
        (code = param_write_string(plist, "Quality",
                 &paramValueToParam(strQuality, ppdev->quality)->p_string)) < 0 ||
        (code = param_write_string(plist, "InkColor",
                 &paramValueToParam(strInk, ppdev->ink)->p_string)) < 0 ||

        (code = param_write_bool(plist, "Inverse", &ppdev->inverse)) < 0 ||
        (code = param_write_bool(plist, "Smooth", &ppdev->smooth)) < 0 ||
        (code = param_write_bool(plist, "Compress", &ppdev->compress)) < 0 ||
        (code = param_write_bool(plist, "LimitCheck", &ppdev->limit)) < 0 ||
        (code = param_write_bool(plist, "DecomposeK", &ppdev->compose)) < 0 ||

        (code = param_write_int(plist, "PaperRed", &ppdev->paperColor.red)) < 0 ||
        (code = param_write_int(plist, "PaperGreen", &ppdev->paperColor.green)) < 0 ||
        (code = param_write_int(plist, "PaperBlue", &ppdev->paperColor.blue)) < 0 ||
        (code = param_write_int(plist, "Random", &ppdev->rnd)) < 0 ||

        (code = param_write_float(plist, "Gamma", &ppdev->gamma)) < 0 ||
        (code = param_write_float(plist, "RedGamma", &ppdev->redGamma)) < 0 ||
        (code = param_write_float(plist, "GreenGamma", &ppdev->greenGamma)) < 0 ||
        (code = param_write_float(plist, "BlueGamma", &ppdev->blueGamma)) < 0
        );
    return code;

}

/* Put parameters. */
static int
gdev_bjc_put_params(gx_device * pdev, gs_param_list * plist)
{
    int code, ecode = 0;
    const char *param_name;
    gs_param_string tmppar;
    uint parsize;
    const stringParamDescription *tmpstr=NULL;

#   define ppdev ((gx_device_bjc_printer *)pdev)

#define CHECK_PARAM_CASES(good, label)			\
    case 1:						\
        break;						\
    case 0:						\
        if ( good ) break;				\
        ecode = gs_error_rangecheck; goto label;	\
    default:						\
        ecode = code;					\
label:							\
        param_signal_error(plist, param_name, ecode)

#define CHECK_str_PARAM_CASES(set, str, label)     	\
    case 1:						\
        break;						\
    case 0:						\
        parsize = tmppar.size;                          \
        tmpstr = paramStringToParam(str,                \
                         (const char *)tmppar.data,     \
                          parsize);                     \
        if ( tmpstr ) { set = tmpstr->p_value; break;}	\
        ecode = gs_error_rangecheck; goto label;	\
    default:						\
        ecode = code;					\
label:							\
        param_signal_error(plist, param_name, ecode)

    switch ( code = param_read_string(plist, (param_name = "PrinterType"),
                                     &tmppar)) {
        CHECK_str_PARAM_CASES(ppdev->printerType, strPrinterType, label_Type);
    }
    switch (code = param_read_string(plist, (param_name = "Feeder"),
                                  &tmppar)) {
        CHECK_str_PARAM_CASES(ppdev->feeder, strFeeder, label_Feeder);
    }
    switch (code = param_read_string(plist, (param_name = "Media"),
                                  &tmppar)) {
        CHECK_str_PARAM_CASES(ppdev->mediaType, strMedia, label_Paper);
    }
    switch (code = param_read_string(plist, (param_name = "Quality"),
                                  &tmppar)) {
        CHECK_str_PARAM_CASES(ppdev->quality, strQuality, label_Quality);
    }
    switch (code = param_read_string(plist, (param_name = "InkColor"),
                                  &tmppar)) {
        CHECK_str_PARAM_CASES(ppdev->ink, strInk, label_Ink);
    }

    switch (code = param_read_bool(plist, (param_name = "Inverse"),
                                  &ppdev->inverse)) {
        CHECK_PARAM_CASES( ppdev->inverse == true ||
                           ppdev->inverse == false , label_Inverse);
    }
    switch (code = param_read_bool(plist, (param_name = "Compress"),
                                  &ppdev->compress)) {
        CHECK_PARAM_CASES( ppdev->compress == true ||
                           ppdev->compress == false , label_Compress);
    }
    switch (code = param_read_bool(plist, (param_name = "Smooth"),
                                  &ppdev->smooth)) {
        CHECK_PARAM_CASES( ppdev->smooth == true ||
                           ppdev->smooth == false , label_Smooth);
    }

    switch (code = param_read_bool(plist, (param_name = "LimitCheck"),
                                  &ppdev->limit)) {
        CHECK_PARAM_CASES( ppdev->limit == true ||
                           ppdev->limit == false , label_Limit);
    }

    switch (code = param_read_bool(plist, (param_name = "DecomposeK"),
                                  &ppdev->compose)) {
        CHECK_PARAM_CASES( ppdev->compose == true ||
                           ppdev->compose == false , label_Compose);
    }

    switch (code = param_read_int(plist, (param_name = "PaperRed"),
                                  &ppdev->paperColor.red)) {
        CHECK_PARAM_CASES( ppdev->paperColor.red >= 0 &&
                           ppdev->paperColor.red <= 255 , label_Red);
    }
    switch (code = param_read_int(plist, (param_name = "PaperGreen"),
                                  &ppdev->paperColor.green)) {
        CHECK_PARAM_CASES(ppdev->paperColor.green >= 0 &&
                          ppdev->paperColor.green <= 255 , label_Green);
    }
    switch (code = param_read_int(plist, (param_name = "PaperBlue"),
                                  &ppdev->paperColor.blue)) {
        CHECK_PARAM_CASES(ppdev->paperColor.blue >= 0 &&
                          ppdev->paperColor.blue <= 255  , label_Blue);
    }
    switch (code = param_read_int(plist, (param_name = "Random"),
                                  &ppdev->rnd)) {
        CHECK_PARAM_CASES(ppdev->rnd >= 0 &&
                          ppdev->rnd <= 100  , label_Random);
    }
    switch (code = param_read_float(plist, (param_name = "Gamma"),
                                  &ppdev->gamma)) {
        CHECK_PARAM_CASES(ppdev->gamma >= 0.0 &&
                          ppdev->gamma <= 10.0  , label_Gamma);
    }
    switch (code = param_read_float(plist, (param_name = "RedGamma"),
                                  &ppdev->redGamma)) {
        CHECK_PARAM_CASES(ppdev->redGamma >= 0.0 &&
                          ppdev->redGamma <= 10.0  , label_Rgamma);
    }
    switch (code = param_read_float(plist, (param_name = "GreenGamma"),
                                  &ppdev->greenGamma)) {
        CHECK_PARAM_CASES(ppdev->greenGamma >= 0.0 &&
                          ppdev->greenGamma <= 10.0  , label_Ggamma);
    }
    switch (code = param_read_float(plist, (param_name = "BlueGamma"),
                                  &ppdev->blueGamma)) {
        CHECK_PARAM_CASES(ppdev->blueGamma >= 0.0 &&
                          ppdev->blueGamma <= 10.0  , label_Bgamma);
    }
    if (ecode < 0) return ecode;

    return gdev_prn_put_params(pdev, plist);
#undef ppdev
}

static int
bjc_print_page_mono(gx_device_printer * pdev, gp_file * file)
{
#define ppdev ((gx_device_bjc_printer *) pdev)
#define prdev ((gx_device_printer *) pdev)

    uint raster = gdev_prn_raster(pdev);
    uint cmplen;
    byte *row = gs_alloc_bytes(pdev->memory, raster, "bjc mono file buffer");
    byte *cmp = gs_alloc_bytes(pdev->memory, (raster << 1) + 1,
                               "bjc mono comp buffer"); /*worst case */
    byte *outrow; /* misc variable for send a row */
    int y;
    int skip;    /* empty raster lines */
    char color = (ppdev->smooth == true ? 0x12 :       /* smooted black */
                  ((ppdev->ink & INK_K) ? 0x11: 0x10)); /* black or color */
    char ink   = 0x01; /* regular ink type */
    char compress = (ppdev->compress == true ? 0x01 : 0x00); /* compression or not */
    int x_resolution = (int)pdev->HWResolution[0];
    int y_resolution = (int)pdev->HWResolution[1];
    int length = 0/*x71*/, lm = 0/*x01*/, rm = 0/*x01*/, top = 0/*x50*/;
    byte inkc = ppdev->ink;
    byte mask_array[] = { 0xff, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe };
    byte lastmask = mask_array[pdev->width % 8];

    if (row == 0 || cmp == 0)		/* can't allocate row buffer */
        return_error(gs_error_VMerror);

    /* Write the setup data. */

    bjc_put_set_initial (file);   /* start printing */
    bjc_put_print_method(file, color, media_codes[ppdev->mediaType].c, ppdev->quality, 0);
    bjc_put_media_supply(file, ppdev->feeder, media_codes[ppdev->mediaType].l);
    bjc_put_raster_resolution(file, x_resolution, y_resolution);
    bjc_put_page_margins(file, length, lm, rm, top);
    bjc_put_set_compression(file, compress);
    bjc_put_image_format(file, 0, 0, ink);   /* normal ink */

    /* Write the contents of the image. */
    skip = 0;
    for (y = 0; y < pdev->height ; y++) {
      gdev_prn_copy_scan_lines(pdev, y, row, raster);
      if (bjc_invert_bytes(row, raster, ppdev->inverse, lastmask)) /* black -> K and check empty line*/
       {  /* empty line raster */
        if (skip) bjc_put_raster_skip(file, skip);
        skip = 1;
        if(compress) cmplen = bjc_compress(row, raster, cmp), outrow = cmp;
        else outrow = row, cmplen = raster;  /* compress or not */
        if(inkc & INK_K) bjc_put_cmyk_image(file, CMYK_K, outrow, cmplen),
                bjc_put_CR(file);
        if(inkc & INK_C) bjc_put_cmyk_image(file, CMYK_C, outrow, cmplen),
                bjc_put_CR(file);
        if(inkc & INK_M) bjc_put_cmyk_image(file, CMYK_M, outrow, cmplen),
                bjc_put_CR(file);
        if(inkc & INK_Y) bjc_put_cmyk_image(file, CMYK_Y, outrow, cmplen),
                bjc_put_CR(file); /* use the needed ink(s) */
       }
      else skip++; /* +1 empty line */
    }
    if (skip) bjc_put_raster_skip(file, skip);
    bjc_put_FF(file);            /* eject a page */
    bjc_put_initialize (file);

    gs_free_object(pdev->memory, cmp, "bjc mono comp buffer");
    gs_free_object(pdev->memory, row, "bjc mono file buffer");

    return 0;

#undef ppdev
}

static int
bjc_print_page_gray(gx_device_printer * pdev, gp_file * file)
{
#define ppdev ((gx_device_bjc_printer *) pdev)
#define prdev ((gx_device_printer *) pdev)

    uint width =   pdev->width; /* Because grayscale */
    uint raster = (pdev->width >> 3) + ( (pdev->width % 8) ? 1:0);
    uint cmplen;
    byte *row = gs_alloc_bytes(pdev->memory, width, "bjc gray file buffer");
    byte *dit = gs_alloc_bytes(pdev->memory, raster, "bjc gray dither buffer");
    byte *cmp = gs_alloc_bytes(pdev->memory, (raster << 1) + 1,
                               "bjc gray comp buffer"); /*worst case */
    byte *out; /* misc variable for send a row */
    int y;
    int skip;    /* empty raster lines */
    char color = (ppdev->smooth == true ? 0x12 :       /* smooted black */
                  ((ppdev->ink & INK_K) ? 0x11: 0x10)); /* black or color */
    char ink   = 0x01; /* regular ink type */
    char compress = (ppdev->compress == true ? 0x01 : 0x00); /* compression or not */
    int x_resolution = (int)pdev->HWResolution[0];
    int y_resolution = (int)pdev->HWResolution[1];
    int length = 0/*x71*/, lm = 0/*x01*/, rm = 0/*x01*/, top = 0/*x50*/;
    byte inkc = ppdev->ink;
    byte mask_array[] = { 0xff, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe };
    byte lastmask = mask_array[pdev->width % 8];

    if (row == 0 || cmp == 0 ||
       dit == 0 )		/* can't allocate row buffers */
        return_error(gs_error_VMerror);

    /* Write the setup data. */

    bjc_build_gamma_table(ppdev, ppdev->gamma, CMYK_K); /* set up the gamma table */

    bjc_put_set_initial (file);   /* start printing */
    bjc_put_print_method(file, color, media_codes[ppdev->mediaType].c, ppdev->quality, 0);
    bjc_put_media_supply(file, ppdev->feeder, media_codes[ppdev->mediaType].l);
    bjc_put_raster_resolution(file, x_resolution, y_resolution);
    bjc_put_page_margins(file, length, lm, rm, top);
    bjc_put_set_compression(file, compress);
    bjc_put_image_format(file, 0, 0, ink);   /* normal ink */

    /* Write the contents of the image. */
    skip = 0;
    ppdev->bjc_j = 0;
    ppdev->bjc_k = 31;
    ppdev->FloydSteinbergDirectionForward=true;
    if(FloydSteinbergInitG(pdev) == -1)
                return_error(gs_error_VMerror);   /* initiate the dithering */

    for (y = 0; y < pdev->height ; y++) {
     gdev_prn_copy_scan_lines(pdev, y, row, width);   /* image -> row */
     FloydSteinbergDitheringG(ppdev, row, dit, width, raster, ppdev->limit); /* gray */
      if (bjc_invert_bytes(dit, raster, ppdev->inverse, lastmask)) /* black -> K and check empty line*/
       {  /* end of empty lines */
        if (skip) bjc_put_raster_skip(file, skip);
        skip = 1;

          if(compress) cmplen = bjc_compress(dit, raster, cmp), out = cmp;
          else cmplen = raster, out = dit;  /* compress or not */

          if(inkc & INK_K) bjc_put_cmyk_image(file, CMYK_K, out, cmplen),
                bjc_put_CR(file);
          if(inkc & INK_C) bjc_put_cmyk_image(file, CMYK_C, out, cmplen),
                bjc_put_CR(file);
          if(inkc & INK_M) bjc_put_cmyk_image(file, CMYK_M, out, cmplen),
                bjc_put_CR(file);
          if(inkc & INK_Y) bjc_put_cmyk_image(file, CMYK_Y, out, cmplen),
                bjc_put_CR(file); /* use the needed ink(s) */

       } else skip++; /* +1 empty line */
    }
    if (skip) bjc_put_raster_skip(file, skip);
    bjc_put_FF(file);            /* eject a page */
    bjc_put_initialize (file);

    FloydSteinbergCloseG(pdev);
    gs_free_object(pdev->memory, dit, "bjc gray dither buffer");
    gs_free_object(pdev->memory, cmp, "bjc gray comp buffer");
    gs_free_object(pdev->memory, row, "bjc gray file buffer");

    return 0;

#undef ppdev
}

static int
bjc_print_page_cmyk(gx_device_printer * pdev, gp_file * file)
{
#define ppdev ((gx_device_bjc_printer *) pdev)
#define prdev ((gx_device_printer *) pdev)

    uint raster = bitmap_raster(pdev->width);
    uint a_raster;                             /* a tmp variable */
    uint cmplen;
    byte *row = gs_alloc_bytes(pdev->memory, raster*4,
                               "bjc cmyk file buffer"); /* one for each component */
    byte *cmp = gs_alloc_bytes(pdev->memory, (raster << 1) + 1,
                               "bjc cmyk comp buffer"); /*worst case */
    byte *rows[4];
    byte *outrow; /* misc variable for send a row */
    int y;
    int skip;    /* empty raster lines */
    char color = 0x10; /* color */
    char ink   = 0x01; /* regular ink type */
    char compress = (ppdev->compress == true ? 0x01 : 0x00); /* compression or not */
    int x_resolution = (int)pdev->HWResolution[0];
    int y_resolution = (int)pdev->HWResolution[1];
    int length = 0/*x71*/, lm = 0/*x01*/, rm = 0/*x01*/, top = 0/*x50*/;
    int plane, code = 0;
    byte mask_array[] = { 0xff, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe };
    byte lastmask = mask_array[pdev->width % 8];
    skip_t skipc;
    byte inkc = ppdev->ink;
    bool inverse = ppdev->inverse;
    gx_render_plane_t render_plane;

    if (row == 0 || cmp == 0)		/* can't allocate row buffer */
        return_error(gs_error_VMerror);

    /* Write the setup data. */

    bjc_put_set_initial (file);   /* start printing */
    bjc_put_print_method(file, color, media_codes[ppdev->mediaType].c, ppdev->quality, 0);
    bjc_put_media_supply(file, ppdev->feeder, media_codes[ppdev->mediaType].l);
    bjc_put_raster_resolution(file, x_resolution, y_resolution);
    bjc_put_page_margins(file, length, lm, rm, top);
    bjc_put_set_compression(file, compress);
    bjc_put_image_format(file, 0, 0, ink);   /* normal ink */

    /* Write the contents of the image. */
    skip = 0;

    for (y = 0; y < pdev->height ; y++) {

        for (plane = 0; plane < 4; plane++) {   /* print each color component */
            code = gx_render_plane_init(&render_plane, (gx_device *)pdev, plane);
            if (code < 0)
                goto error;
            code = gdev_prn_get_lines(pdev, y, 1, row + raster*plane, raster,
                               &rows[plane], &a_raster, &render_plane);
            if (code < 0)
                goto error;
        }

        {
            int i;
            byte *byteC=rows[0], *byteM=rows[1],
                *byteY=rows[2], *byteK=rows[3];
            for(i=0; i<raster; i++, byteC++, byteM++, byteY++, byteK++) {
                if (ppdev->compose) {
                    *byteK =  *byteC & *byteM & *byteY;
                    *byteC &= ~(*byteK);
                    *byteM &= ~(*byteK);
                    *byteY &= ~(*byteK);
                }
                else {
                    *byteC |= *byteK;
                    *byteM |= *byteK;
                    *byteY |= *byteK;
                    *byteK =  0;
                }
            }
        }

        if(bjc_invert_cmyk_bytes(rows[0], rows[1], rows[2], rows[3],
                                 raster, inverse, lastmask, &skipc)) {

            if (skip) bjc_put_raster_skip(file, skip);
            skip = 1;
            if(skipc.skipC && (inkc & INK_C)) {
                if(compress) cmplen = bjc_compress(rows[0], raster, cmp), outrow = cmp;
                else outrow = rows[0], cmplen = raster;  /* compress or not */
                bjc_put_cmyk_image(file, CMYK_C, outrow, cmplen), bjc_put_CR(file);
            }
            if(skipc.skipM && (inkc & INK_M)) {
                if(compress) cmplen = bjc_compress(rows[1], raster, cmp), outrow = cmp;
                else outrow = rows[1], cmplen = raster;  /* compress or not */
                bjc_put_cmyk_image(file, CMYK_M, outrow, cmplen), bjc_put_CR(file);
            }
            if(skipc.skipY && (inkc & INK_Y)) {
                if(compress) cmplen = bjc_compress(rows[2], raster, cmp), outrow = cmp;
                else outrow = rows[2], cmplen = raster;  /* compress or not */
                bjc_put_cmyk_image(file, CMYK_Y, outrow, cmplen), bjc_put_CR(file);
            }
            if(skipc.skipK && (inkc & INK_K)) {
                if(compress) cmplen = bjc_compress(rows[3], raster, cmp), outrow = cmp;
                else outrow = rows[3], cmplen = raster;  /* compress or not */
                bjc_put_cmyk_image(file, CMYK_K, outrow, cmplen), bjc_put_CR(file);
            }
        }
        else skip++; /* +1 empty line */
    }

    if (skip) bjc_put_raster_skip(file, skip);
    bjc_put_FF(file);            /* eject a page */
    bjc_put_initialize (file);

error:
    gs_free_object(pdev->memory, cmp, "bjc cmyk comp buffer");
    gs_free_object(pdev->memory, row, "bjc cmyk file buffer");

    return code;

#undef ppdev
}

static int
bjc_print_page_color(gx_device_printer * pdev, gp_file * file)
{
#define ppdev ((gx_device_bjc_printer *) pdev)
#define prdev ((gx_device_printer *) pdev)

    uint width =   pdev->width; /* Because grayscale */
    uint raster = (pdev->width >> 3) + ( (pdev->width % 8) ? 1:0);
    uint cmplen;
    byte *row = gs_alloc_bytes(pdev->memory, width*4,
                               "bjc true file buffer"); /* one for each component */
    byte *dit = gs_alloc_bytes(pdev->memory, raster*4,
                               "bjc true dither buffer");
    byte *cmp = gs_alloc_bytes(pdev->memory, (raster << 1) + 1,
                               "bjc true comp buffer"); /*worst case */
    byte *rowC = dit;                 /*C*/
    byte *rowM = dit + raster;        /*M*/
    byte *rowY = dit + 2*raster;      /*Y*/
    byte *rowK = dit + 3*raster;      /*K*/
    byte *outrow; /* misc variable for send a row */
    int y;
    int skip;    /* empty raster lines */
    char color = 0x10; /* color */
    char ink   = 0x01; /* regular ink type */
    char compress = (ppdev->compress == true ? 0x01 : 0x00); /* compression or not */
    int x_resolution = (int)pdev->HWResolution[0];
    int y_resolution = (int)pdev->HWResolution[1];
    int length = 0/*x71*/, lm = 0/*x01*/, rm = 0/*x01*/, top = 0/*x50*/;
    byte mask_array[] = { 0xff, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe };
    byte lastmask = mask_array[pdev->width % 8];
    skip_t skipc;
    byte inkc = ppdev->ink;
    float rgamma = ppdev->gamma != 1.0 ? ppdev->gamma:ppdev->redGamma;
    float ggamma = ppdev->gamma != 1.0 ? ppdev->gamma:ppdev->greenGamma;
    float bgamma = ppdev->gamma != 1.0 ? ppdev->gamma:ppdev->blueGamma;

    if (row == 0 || cmp == 0 || dit == 0)     /* can't allocate row buffer */
        return_error(gs_error_VMerror);

    bjc_build_gamma_table(ppdev, rgamma, CMYK_C); /* set up the gamma table */
    bjc_build_gamma_table(ppdev, ggamma, CMYK_M); /* set up the gamma table */
    bjc_build_gamma_table(ppdev, bgamma, CMYK_Y); /* set up the gamma table */

    /* Write the setup data. */

    bjc_put_set_initial (file);   /* start printing */
    bjc_put_print_method(file, color, media_codes[ppdev->mediaType].c, ppdev->quality, 0);
    bjc_put_media_supply(file, ppdev->feeder, media_codes[ppdev->mediaType].l);
    bjc_put_raster_resolution(file, x_resolution, y_resolution);
    bjc_put_page_margins(file, length, lm, rm, top);
    bjc_put_set_compression(file, compress);
    bjc_put_image_format(file, 0, 0, ink);   /* normal ink */

    /* Write the contents of the image. */
    skip = 0;

    ppdev->bjc_j = 0;
    ppdev->bjc_k = 31;
    ppdev->FloydSteinbergDirectionForward=true;
    if(FloydSteinbergInitC(pdev) == -1)
                return_error(gs_error_VMerror);   /* initiate the dithering */

    for (y = 0; y < pdev->height ; y++) {
        gdev_prn_copy_scan_lines(pdev, y, row, gdev_prn_raster(pdev));
        /* image -> row */
        FloydSteinbergDitheringC(ppdev, row, dit, width, raster, ppdev->limit,
                                 ppdev->compose);

        if(bjc_invert_cmyk_bytes(rowC, rowM, rowY, rowK,
                                 raster, ~ppdev->inverse, lastmask,
                                 &skipc)) {
            if (skip) bjc_put_raster_skip(file, skip);
            skip = 1;
            if(skipc.skipC && (inkc & INK_C) ) {
                if(compress) cmplen = bjc_compress(rowC, raster, cmp), outrow = cmp;
                else outrow = rowC, cmplen = raster;  /* compress or not */
                bjc_put_cmyk_image(file, CMYK_C, outrow, cmplen), bjc_put_CR(file);
            }
            if(skipc.skipM  && (inkc & INK_M) ) {
                if(compress) cmplen = bjc_compress(rowM, raster, cmp), outrow = cmp;
                else outrow = rowM, cmplen = raster;  /* compress or not */
                bjc_put_cmyk_image(file, CMYK_M, outrow, cmplen), bjc_put_CR(file);
            }
            if(skipc.skipY  && (inkc & INK_Y) ) {
                if(compress) cmplen = bjc_compress(rowY, raster, cmp), outrow = cmp;
                else outrow = rowY, cmplen = raster;  /* compress or not */
                bjc_put_cmyk_image(file, CMYK_Y, outrow, cmplen), bjc_put_CR(file);
            }
            if(skipc.skipK  && (inkc & INK_K) ) {
                if(compress) cmplen = bjc_compress(rowK, raster, cmp), outrow = cmp;
                else outrow = rowK, cmplen = raster;  /* compress or not */
                bjc_put_cmyk_image(file, CMYK_K, outrow, cmplen), bjc_put_CR(file);
            }
        }
        else skip++; /* +1 empty line */
    }

    if (skip) bjc_put_raster_skip(file, skip);
    bjc_put_FF(file);            /* eject a page */
    bjc_put_initialize (file);

    FloydSteinbergCloseC(pdev);
    gs_free_object(pdev->memory, cmp, "bjc true comp buffer");
    gs_free_object(pdev->memory, dit, "bjc true dither buffer");
    gs_free_object(pdev->memory, row, "bjc true file buffer");

    return 0;

#undef ppdev
}
