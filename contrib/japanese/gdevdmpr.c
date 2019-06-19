/* COPYRIGHT (C) 1990, 1992 Aladdin Enterprises.  All rights reserved.
   Distributed by Free Software Foundation, Inc.

   This file is part of Ghostscript.

   Ghostscript is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
   to anyone for the consequences of using it or for whether it serves any
   particular purpose or works at all, unless he says so in writing.  Refer
   to the Ghostscript General Public License for full details.

   Everyone is granted permission to copy, modify and redistribute
   Ghostscript, but only under the conditions described in the Ghostscript
   General Public License.  A copy of this license is supposed to have been
   given to you along with Ghostscript so you can know your rights and
   responsibilities.  It should be in a file named COPYING.  Among other
   things, the copyright notice and this notice must be preserved on all
   copies.  */

/* gdevdmpr.c
 *
 * Dot matrix printer driver for Ghostscript
 *
 * The first version written by ...                 K.Asayama  Nov 1992
 * NEC PC-H98 high resolution mode supported by ... OkI        Dec 1992
 * IBM PC AT/Compatible machines supported by ...   hero.h     Mar 1993
 * Modified for 386BSD by ...                       FKR        Mar 1993
 * Modified for Ghostscript version 2.5.2 by ...    K.Asayama  Mar 1993
 * Modified for Ghostscript version 2.6.1 by ...    K.Asayama  Oct 1993
 * Modified for Ghostscript version 4.03 by ...     K.Asayama  May 1997
 */

#include "gdevprn.h"
#include "gp.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gsstate.h"
#include "math_.h"
#include "malloc_.h"

/* include library header. */
#include "dviprlib.h"

#define LOCAL_DEBUG 0

#define DEVICE_NAME "dmprt"
#define DEVICE_VERSION 201  /* 2.01 */

typedef struct gx_device_dmprt_local_s gx_device_dmprt_local;
struct gx_device_dmprt_local_s {
  int orig_x_dpi;
  int orig_y_dpi;

  int x_offset;
  int y_offset;

  int spec_width;
  int spec_height;
  int max_width;
  int max_height;
  int dev_margin[4]; /* left bottom right top */

  int prtcfg_init_f;
  int verbose_f;
  int debug_f;
  dviprt_cfg_t prtcfg;
  dviprt_print prtinfo;
};

/* declaration of the structure to describe `DMPRT' device driver */
typedef struct gx_device_dmprt_s gx_device_dmprt;
struct gx_device_dmprt_s {
  gx_device_common;
  gx_prn_device_common;

  gx_device_dmprt_local dmprt;
};

/* declarations of main functions */
static dev_proc_get_params(gdev_dmprt_get_params);
static dev_proc_put_params(gdev_dmprt_put_params);
static dev_proc_get_initial_matrix(gdev_dmprt_get_initial_matrix);
static dev_proc_open_device(gdev_dmprt_open);
static dev_proc_print_page(gdev_dmprt_print_page);
static dev_proc_close_device(gdev_dmprt_close);

/* declarations of sub functions to get printer properties. */
static void gdev_dmprt_init_printer_props(gx_device_dmprt *);
static int gdev_dmprt_get_printer_props(gx_device_dmprt *,char *);
static int gdev_dmprt_check_code_props(byte * ,int );
static gp_file *gdev_dmprt_dviprt_lib_fopen(const gs_memory_t *mem,const char *,char *);

static int gdev_dmprt_error_no_dviprt_to_gs(int );

/* The device descriptor */
gx_device_procs prn_dmprt_procs = {
  gdev_dmprt_open,
  gdev_dmprt_get_initial_matrix,
  0, /* sync_output */
  gdev_prn_output_page,
  gdev_dmprt_close,
  gdev_prn_map_rgb_color,
  gdev_prn_map_color_rgb,
  0, /* fill_rectangle */
  0, /* tile_rectangle */
  0, /* copy_mono */
  0, /* copy_color */
  0, /* draw_line */
  0, /* get_bits */
  gdev_dmprt_get_params,
  gdev_dmprt_put_params,
  gx_default_map_cmyk_color,
  0,
};

#define DEFAULT_DPI   180
#define DEFAULT_WIDTH   8.3
#define DEFAULT_HEIGHT 11.7

gx_device_dmprt gs_dmprt_device = {
  prn_device_body(gx_device_dmprt, prn_dmprt_procs, DEVICE_NAME,
      DEFAULT_WIDTH*10,       /* width 10'th */
      DEFAULT_HEIGHT*10,      /* height 10'th */
      DEFAULT_DPI,            /* X resolution */
      DEFAULT_DPI,            /* Y resolution */
      0,0,0,0,                /* margins (top,left,bottom,right) */
      1,1,1,1,2,2,            /* color info. */
      gdev_dmprt_print_page),

  {
    DEFAULT_DPI,  DEFAULT_DPI,
    0, 0, /* offset */
    DEFAULT_DPI*DEFAULT_WIDTH,  /* specified width */
    DEFAULT_DPI*DEFAULT_HEIGHT, /* specified height */
    -1,-1,			/* maximal width,height */
    { 0,0,0,0 },      /* margins */

  /* status of the device */
      0,    /* printer configuration is not initialized */
      0,    /* verbose mode */
      0,    /* debug flag */
  },
};

#define ppdev ((gx_device_printer *)pdev)
#define pddev ((gx_device_dmprt *)pdev)

typedef struct {
  char *name;
  int no;
} dmprt_encoding;
static dmprt_encoding gdev_dmprt_encode_list[] = {
  {"Null",CFG_ENCODE_NULL},
  {"AsciiHex",CFG_ENCODE_HEX },
  {"CCITTFax",CFG_ENCODE_FAX },
  {"PCLMode1Compress",CFG_ENCODE_PCL1 },
  {"PCLMode2Compress",CFG_ENCODE_PCL2 },
  {0},
};

/* --- Get properties of printer device. --- */
static int gdev_dmprt_get_dmprt_params(gx_device *pdev, gs_param_list *plist);
static int
gdev_dmprt_get_dviprt_params(gx_device *pdev, gs_param_list *plist);

static int
gdev_dmprt_get_params(gx_device *pdev, gs_param_list *plist)
{
  int code;
  gs_param_dict dict;
  const char *param_name;

  if (!pddev->dmprt.prtcfg_init_f)
    gdev_dmprt_init_printer_props(pddev);

  dict.size = 30;
  code = param_begin_write_dict(plist, (param_name = "DmprtParams"),
                                &dict, false);
  if (code < 0) return code;
  if (code == 0) {
    code = gdev_dmprt_get_dmprt_params(pdev, dict.list);
    param_end_write_dict(plist, param_name, &dict);
    if (code < 0) return code;
  }

  dict.size = 30;
  code = param_begin_write_dict(plist, (param_name = "DviprtParams"),
                                &dict, false);
  if (code < 0) return code;
  if (code == 0) {
    code = gdev_dmprt_get_dviprt_params(pdev, dict.list);
    param_end_write_dict(plist, param_name, &dict);
    if (code < 0) return code;
  }

  {
    int w = pddev->width;
    int h = pddev->height;
    pddev->width = pddev->dmprt.spec_width;
    pddev->height = pddev->dmprt.spec_height;
    code = gdev_prn_get_params(pdev, plist);
    pddev->width = w;
    pddev->height = h;
  }

  return code;
}

/* internal routines for get_params */
static int
gdev_dmprt_get_dmprt_params(gx_device *pdev, gs_param_list *plist)
{
  int code;
  long vlong;
  bool vbool;
  gs_param_int_array vaint;
  int int_data[4];

  vlong = DEVICE_VERSION;
  code = param_write_long(plist, "Version", &vlong);
  if (code < 0) return code;

  vbool = pddev->dmprt.debug_f;
  code = param_write_bool(plist, "Debug", &vbool);
  if (code < 0) return code;

  vbool = pddev->dmprt.verbose_f;
  code = param_write_bool(plist, "Verbose", &vbool);
  if (code < 0) return code;

  vaint.size = 2;
  vaint.data = int_data;
  vaint.persistent = false;
  int_data[0] = pddev->dmprt.max_width;
  int_data[1] = pddev->dmprt.max_height;
  code = param_write_int_array(plist, "MaxSize", &vaint);
  if (code < 0) return code;

  vaint.size = 2;
  vaint.data = int_data;
  vaint.persistent = false;
  int_data[0] = pddev->dmprt.x_offset;
  int_data[1] = pddev->dmprt.y_offset;
  code = param_write_int_array(plist, "Offsets", &vaint);
  if (code < 0) return code;

  vaint.size = 4;
  vaint.data = int_data;
  vaint.persistent = false;
  { int i;
    for (i=0;i<4;i++) int_data[i] = pddev->dmprt.dev_margin[i];
  }
  code = param_write_int_array(plist, "Margins", &vaint);
  if (code < 0) return code;

  return code;
}

static int
gdev_dmprt_get_dviprt_params(gx_device *pdev, gs_param_list *plist)
{
  dviprt_cfg_t *pprt = &pddev->dmprt.prtcfg;
  long vlong;
  bool vbool;
  gs_param_string vstr;
  const char *vchar;
  int code;
  int i;

  vlong = pprt->integer[CFG_PINS] * 8;
  code = param_write_long(plist,"Pins", &vlong);
  if (code < 0) return code;
  code = param_write_long(plist, "MinimalUnit",
                          pprt->integer+CFG_MINIMAL_UNIT);
  if (code < 0) return code;
  code = param_write_long(plist,"MaximalUnit",
                          pprt->integer+CFG_MAXIMAL_UNIT);
  if (code < 0) return code;
  code = param_write_int(plist,"HDpi" , &pddev->dmprt.orig_x_dpi);
  if (code < 0) return code;
  code = param_write_int(plist,"VDpi", &pddev->dmprt.orig_y_dpi);
  if (code < 0) return code;
  code = param_write_long(plist,"Constant", pprt->integer+CFG_CONSTANT);
  if (code < 0) return code;

  vbool = pprt->integer[CFG_UPPER_POS] & CFG_NON_TRANSPOSE_BIT ? 0 : 1;
  code = param_write_bool(plist,"Transpose", &vbool);
  if (code < 0) return code;
  vbool = pprt->integer[CFG_UPPER_POS] & CFG_REVERSE_BIT ? 1 : 0;
  code = param_write_bool(plist,"Reverse", &vbool);
  if (code < 0) return code;
  vbool = (pprt->integer[CFG_UPPER_POS] & CFG_NON_MOVING) ? 1 : 0;
  code = param_write_bool(plist,"NonMoving", &vbool);
  if (code < 0) return code;

  vchar = pprt->strings[CFG_NAME] ? (const char*)pprt->strings[CFG_NAME] : "";
  param_string_from_string(vstr, vchar);
  code = param_write_string(plist, "Name", &vstr);
  if (code < 0) return code;

  for (i=0;gdev_dmprt_encode_list[i].name;i++) {
    if (pprt->integer[CFG_ENCODE] == gdev_dmprt_encode_list[i].no)
      break;
  }
  if (gdev_dmprt_encode_list[i].name == 0) i = 0;
  param_string_from_string(vstr, gdev_dmprt_encode_list[i].name);
  code = param_write_string(plist, "Encoding", &vstr);
  if (code < 0) return code;

#define param_string_from_prt(ps, pprt, n) \
  ((ps).data = pprt->prtcode[n] ? pprt->prtcode[n] : (const byte*)"", \
  (ps).size = pprt->prtcode[n] ? pprt->prtcode_size[n] : 0, \
  (ps).persistent = true)
#define param_write_prt(plist, name, pprt, n) \
  (param_string_from_prt(vstr, pprt, n), \
  param_write_string(plist, name, &vstr))

  code = param_write_prt(plist, "BitImageMode",
                         pprt, CFG_BIT_IMAGE_MODE);
  if (code < 0) return code;
  code = param_write_prt(plist, "SendBitImage",
                         pprt, CFG_SEND_BIT_IMAGE);
  if (code < 0) return code;
  code = param_write_prt(plist, "BitRowHeader",
                         pprt, CFG_BIT_ROW_HEADER);
  if (code < 0) return code;
  code = param_write_prt(plist, "AfterBitImage",
                         pprt, CFG_AFTER_BIT_IMAGE);
  if (code < 0) return code;
  code = param_write_prt(plist, "LineFeed", pprt, CFG_LINE_FEED);
  if (code < 0) return code;
  code = param_write_prt(plist, "FormFeed", pprt, CFG_FORM_FEED);
  if (code < 0) return code;
  code = param_write_prt(plist, "NormalMode", pprt, CFG_NORMAL_MODE);
  if (code < 0) return code;
  code = param_write_prt(plist, "SkipSpaces", pprt, CFG_SKIP_SPACES);
  if (code < 0) return code;

  return code;
}
/* end of internal routines for get_params */

/* --- Put properties of printer device. --- */
static int gdev_dmprt_put_dmprt_params(gx_device *pdev, gs_param_list *plist);
static int
gdev_dmprt_put_dviprt_params(gx_device *pdev, gs_param_list *plist);
static int gdev_dmprt_put_prt_code_param(gs_param_list *plist,
                                          dviprt_cfg_t *pprt,
                                          const char* name, int idx);
static int
gdev_dmprt_put_prt_string_param(gs_param_list *plist,
                                dviprt_cfg_t *pprt, const char* name, int idx);

static int
gdev_dmprt_put_params(gx_device *pdev, gs_param_list *plist)
{
  int code = 0;
  const char *param_name;
  gs_param_dict dict;

  if (!pddev->dmprt.prtcfg_init_f)
    gdev_dmprt_init_printer_props(pddev);

  /* dmprt parameters */
  code = param_begin_read_dict(plist, (param_name = "DmprtParams"),
                               &dict, false);
  if (code < 0) return code;
  if (code == 0) {
    code = gdev_dmprt_put_dmprt_params(pdev, dict.list);
    param_end_read_dict(plist, param_name, &dict);
    if (code < 0) return code;
  }

  /* dviprt parameters */
  code = param_begin_read_dict(plist, (param_name = "DviprtParams"),
                                &dict, false);
  if (code < 0) return code;
  if (code == 0) {
    code = gdev_dmprt_put_dviprt_params(pdev, dict.list);
    param_end_read_dict(plist, param_name, &dict);
    if (code < 0) return code;
  }

  if (pdev->is_open && code) {
    int ccode = gs_closedevice(pdev);
    if (ccode < 0) return ccode;
  }

  pddev->width = pddev->dmprt.spec_width;
  pddev->height = pddev->dmprt.spec_height;
  code = gdev_prn_put_params(pdev, plist);
  pddev->dmprt.spec_width = pddev->width;
  pddev->dmprt.spec_height = pddev->height;
  pddev->width -= (pddev->dmprt.dev_margin[0] + pddev->dmprt.dev_margin[2]);
  pddev->height -= (pddev->dmprt.dev_margin[1] + pddev->dmprt.dev_margin[3]);
  if (code < 0) return code;

  if (pddev->dmprt.max_width>0 && pddev->dmprt.max_width<pddev->width)
    pddev->width = pddev->dmprt.max_width;
  if (pddev->dmprt.max_height>0 && pddev->dmprt.max_height<pddev->height)
    pddev->height = pddev->dmprt.max_height;

  dviprt_setmessagestream(pddev->dmprt.debug_f ? stderr : NULL);

  return code;
}

/* internal routines for put_params */

static int
gdev_dmprt_put_dmprt_params(gx_device *pdev, gs_param_list *plist)
{
  int code;
  long vlong;
  bool vbool;
  gs_param_int_array vaint;

  /* debug flag */
  code = param_read_bool(plist, "Debug", &vbool);
  if (code < 0) return code;
  if (code == 0) pddev->dmprt.debug_f = vbool;

  dviprt_setmessagestream(pddev->dmprt.debug_f ? stderr : NULL);

  code = param_read_bool(plist, "Verbose", &vbool);
  if (code < 0) return code;
  pddev->dmprt.verbose_f = vbool;

  /* dummy */
  code = param_read_long(plist, "Version", &vlong);
  if (code < 0) return code;

  code = param_read_int_array(plist, "MaxSize", &vaint);
  if (code < 0) return code;
  if (code == 0) {
    if (vaint.size != 2) return_error(gs_error_typecheck);
    pddev->dmprt.max_width = vaint.data[0];
    pddev->dmprt.max_height = vaint.data[1];
  }

  code = param_read_int_array(plist, "Offsets", &vaint);
  if (code < 0) return code;
  if (code == 0) {
    if (vaint.size != 2) return_error(gs_error_typecheck);
    pddev->dmprt.x_offset = vaint.data[0];
    pddev->dmprt.y_offset = vaint.data[1];
  }

  code = param_read_int_array(plist, "Margins", &vaint);
  if (code < 0) return code;
  if (code == 0) {
    int i;
    if (vaint.size != 4) return_error(gs_error_typecheck);
    for (i=0;i<4;i++) pddev->dmprt.dev_margin[i] = vaint.data[i];
  }

  return code;
}

static int
gdev_dmprt_put_dviprt_params(gx_device *pdev, gs_param_list *plist)
{
  int code;
  dviprt_cfg_t *pprt = &pddev->dmprt.prtcfg;
  gs_param_string vstr;
  long vlong;
  bool vbool;

  /* load .cfg/.src file */
  code = param_read_string(plist, "FileName", &vstr);
  if (code < 0) return code;
  if (code == 0) {
    char *filename = gs_malloc(pdev->memory->non_gc_memory, vstr.size + 1, 1,
                               "gdev_dmprt_put_props(filename)");
    int ccode;
    if (filename == 0) return_error(gs_error_VMerror);
    strncpy(filename, (const char*)vstr.data, vstr.size);
    filename[vstr.size] = '\0';
    ccode = gdev_dmprt_get_printer_props(pddev,filename);
    gs_free(pdev->memory->non_gc_memory, filename, vstr.size+1, 1, "gdev_dmprt_put_props(filename)");
    if (ccode < 0) return ccode;
  }

  code = param_read_long(plist, "Pins", &vlong);
  if (code < 0) return code;
  if (code == 0) pprt->integer[CFG_PINS] = vlong / 8;

  code = param_read_long(plist, "MinimalUnit", &vlong);
  if (code < 0) return code;
  if (code == 0) pprt->integer[CFG_MINIMAL_UNIT] = vlong;

  code = param_read_long(plist, "MaximalUnit", &vlong);
  if (code < 0) return code;
  if (code == 0) pprt->integer[CFG_MAXIMAL_UNIT] = vlong;

  code = param_read_long(plist, "HDpi", &vlong);
  if (code < 0) return code;
  if (code == 0) pddev->dmprt.orig_x_dpi = vlong;

  code = param_read_long(plist, "VDpi", &vlong);
  if (code < 0) return code;
  if (code == 0) pddev->dmprt.orig_y_dpi = vlong;

  code = param_read_long(plist, "Constant", &vlong);
  if (code < 0) return code;
  if (code == 0) pprt->integer[CFG_CONSTANT] = vlong;

  {
    long tr = pprt->integer[CFG_UPPER_POS] & CFG_NON_TRANSPOSE_BIT;
    long rv = pprt->integer[CFG_UPPER_POS] & CFG_REVERSE_BIT;
    long nm = pprt->integer[CFG_UPPER_POS] & CFG_NON_MOVING;
    param_read_bool(plist,"Transpose", &vbool);
    if (code < 0) return code;
    if (code == 0) tr = vbool ? 0 : CFG_NON_TRANSPOSE_BIT;
    param_read_bool(plist,"Reverse", &vbool);
    if (code < 0) return code;
    if (code == 0) rv = vbool ? CFG_REVERSE_BIT : 0;
    param_read_bool(plist,"NonMoving", &vbool);
    if (code < 0) return code;
    if (code == 0) nm = vbool ? CFG_NON_MOVING : 0;
    pprt->integer[CFG_UPPER_POS] = tr | rv | nm;
  }

  code = gdev_dmprt_put_prt_code_param(plist, pprt, "BitImageMode",
                                       CFG_BIT_IMAGE_MODE);
  if (code < 0) return code;
  code = gdev_dmprt_put_prt_code_param(plist, pprt, "SendBitImage",
                                       CFG_SEND_BIT_IMAGE);
  if (code < 0) return code;
  code = gdev_dmprt_put_prt_code_param(plist, pprt, "BitRowHeader",
                                       CFG_BIT_ROW_HEADER);
  if (code < 0) return code;
  code = gdev_dmprt_put_prt_code_param(plist, pprt, "AfterBitImage",
                                       CFG_AFTER_BIT_IMAGE);
  if (code < 0) return code;
  code = gdev_dmprt_put_prt_code_param(plist, pprt, "LineFeed",
                                       CFG_LINE_FEED);
  if (code < 0) return code;
  code = gdev_dmprt_put_prt_code_param(plist, pprt, "FormFeed",
                                       CFG_FORM_FEED);
  if (code < 0) return code;
  code = gdev_dmprt_put_prt_code_param(plist, pprt, "NormalMode",
                                       CFG_NORMAL_MODE);
  if (code < 0) return code;
  code = gdev_dmprt_put_prt_code_param(plist, pprt, "SkipSpaces",
                                       CFG_SKIP_SPACES);
  if (code < 0) return code;

  code = gdev_dmprt_put_prt_string_param(plist, pprt, "Name",
                                         CFG_NAME);
  if (code < 0) return code;

  code = param_read_string(plist, "Encoding", &vstr);
  if (code < 0) return code;
  if (code == 0) {
    int i;
    for (i=0; gdev_dmprt_encode_list[i].name ; i++) {
      if (strlen(gdev_dmprt_encode_list[i].name) == vstr.size) {
        if (strncmp(gdev_dmprt_encode_list[i].name,
                    vstr.data, vstr.size) == 0) {
          pprt->integer[CFG_ENCODE] = gdev_dmprt_encode_list[i].no;
          break;
        }
      }
    }
    if (gdev_dmprt_encode_list[i].name == 0)
      return_error(gs_error_rangecheck);
  }

  return code;
}

static int
gdev_dmprt_put_prt_code_param(gs_param_list *plist,
                              dviprt_cfg_t *pprt, const char* name, int idx)
{
  gs_param_string vstr;

  int code = param_read_string(plist, name, &vstr);
  if (code == 0) {
    int ccode = gdev_dmprt_check_code_props(vstr.data, vstr.size);
    byte *pbyte;
    if (ccode < 0) return_error(gs_error_rangecheck);
    pbyte = (byte *)malloc(vstr.size+1);
    if (pbyte == 0) return_error(gs_error_VMerror);
    memcpy(pbyte, vstr.data, vstr.size);
    pbyte[vstr.size] = 0;
    pprt->prtcode[idx] = pbyte;
    pprt->prtcode_size[idx] = vstr.size;
    if (code == 0) code = 1;
  }
  return code;
}

static int
gdev_dmprt_put_prt_string_param(gs_param_list *plist,
                                dviprt_cfg_t *pprt, const char* name, int idx)
{
  gs_param_string vstr;

  int code = param_read_string(plist, name, &vstr);
  if (code == 0) {
    byte *pbyte;
    pbyte = (byte *)malloc(vstr.size+1);
    if (pbyte == 0) return_error(gs_error_VMerror);
    memcpy(pbyte, vstr.data, vstr.size);
    pbyte[vstr.size] = 0;
    pprt->strings[idx] = pbyte;
    if (code == 0) code = 1;
  }
  return code;
}
/* end of internal routines for put_params */

/* --- Get initial matrix. --- */
static void
gdev_dmprt_get_initial_matrix(gx_device *pdev, gs_matrix *pmat)
{
  gx_default_get_initial_matrix(pdev,pmat);
  pmat->tx += (pddev->dmprt.x_offset - pddev->dmprt.dev_margin[0]);
  pmat->ty += (pddev->dmprt.y_offset + pddev->dmprt.dev_margin[3]);
}

/* --- Open printer device. --- */
static int
gdev_dmprt_open(gx_device *pdev)
{
  int code;
  dviprt_cfg_t *pcfg;
  dviprt_print *pprint;

  pprint = &pddev->dmprt.prtinfo;
  pcfg = &pddev->dmprt.prtcfg;

  if ((code = gdev_prn_open(pdev)) < 0)
    return code;
  pcfg->integer[CFG_DPI] = (int)ppdev->x_pixels_per_inch;
  pcfg->integer[CFG_Y_DPI] = (int)ppdev->y_pixels_per_inch;
  code = dviprt_initlibrary(pprint,pcfg,gdev_prn_raster(pdev),pdev->height);
  if (code < 0) return gdev_dmprt_error_no_dviprt_to_gs(code);
  code = dviprt_setbuffer(pprint,NULL);
  if (code < 0) return gdev_dmprt_error_no_dviprt_to_gs(code);

  return 0;
}

/* --- Close printer device. --- */
static int
gdev_dmprt_close(gx_device *pdev)
{
  int code;
  dviprt_print *pprint;

  pprint = &pddev->dmprt.prtinfo;

  if (!strchr(pddev->fname,'%')) {
    code = dviprt_endbitmap(pprint);
    if (code < 0) return gdev_dmprt_error_no_dviprt_to_gs(code);
  }
  if (pddev->dmprt.verbose_f && pddev->PageCount>0) {
      emprintf2(pdev->memory,
              "%s: Total %lu bytes output.\n",
              pddev->dname,
              dviprt_getoutputbytes(pprint));
  }
  code = dviprt_unsetbuffer(pprint);
  if (code < 0) return gdev_dmprt_error_no_dviprt_to_gs(code);
  return gdev_prn_close(pdev);
}

/* Output the PAGE. */
static int
gdev_dmprt_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
  int code = 0;
  dviprt_print *pprint = &pddev->dmprt.prtinfo;
  int line_size = gdev_prn_raster((gx_device *)pdev);
  int pins = dviprt_getscanlines(pprint);
  int i_buf_size = pins * line_size;
  int lnum = 0;
  ulong prev_bytes;
  byte *in;

  /* get work buffer */
  in = (byte *)gs_malloc(pdev->memory->non_gc_memory, 1, i_buf_size ,"gdev_dmprt_print_page(in)");
  if ( in == 0 )
    return_error(gs_error_VMerror);

  /* Initialize this printer driver */
  if (pdev->file_is_new) {
    code = dviprt_setstream(pprint,NULL,prn_stream);
    if (code < 0) return gdev_dmprt_error_no_dviprt_to_gs(code);
  }
  if (pddev->dmprt.verbose_f) {
    if (pddev->PageCount == 1)
      emprintf2(pdev->memory,
                "%s: %s\n",
                pddev->dname,
                pddev->dmprt.prtcfg.strings[CFG_NAME]);
    emprintf2(pdev->memory,
              "%s: [%ld]",
              pddev->dname,
              pddev->PageCount);
  }
  prev_bytes = dviprt_getoutputbytes(pprint);
  code = dviprt_beginpage(pprint);
  if (code < 0) return gdev_dmprt_error_no_dviprt_to_gs(code);

  /* Transfer pixels to printer */
  while ( lnum < pdev->height ) {
    int num_lines;

    num_lines = pdev->height-lnum;
    if (num_lines > pins)
      num_lines = pins;

    code = gdev_prn_copy_scan_lines(pdev,lnum,in,line_size*num_lines);
    if (code < 0) goto error_ex;

    lnum += num_lines;
    if (num_lines < pins) {
      memset(in+line_size*num_lines,0,line_size*(pins-num_lines));
    }

    code = dviprt_outputscanlines(pprint,in);
    if (code < 0) {
      code = gdev_dmprt_error_no_dviprt_to_gs(code);
      goto error_ex;
    }
  }

  /* Eject the page and set printer normal mode. */
  if (strchr(pdev->fname,'%')) {
    code = dviprt_endbitmap(pprint);
    if (code < 0) return gdev_dmprt_error_no_dviprt_to_gs(code);
  }
  fflush(pddev->file);
  if (pddev->dmprt.verbose_f) {
    emprintf1(pdev->memory,
              " %lu bytes\n",
              dviprt_getoutputbytes(pprint)-prev_bytes);
  }
error_ex:
  gs_free(pdev->memory->non_gc_memory, (char *)in, 1, i_buf_size,"gdev_dmprt_print_page(in)");

  return code;
}

/************************** Internal Routines **************************/

/************************ Get printer properties. ***********************/
static int
gdev_dmprt_check_code_props(byte *str,int len)
{
  byte *end = str+len;
  while (str < end) {
    byte fmt = *str++;
    if (fmt & CFG_FMT_BIT) {
      int s = *str++;
      str += s;
      if (str > end) return_error(gs_error_rangecheck);
      if ((fmt & CFG_FMT_FORMAT_BIT) == CFG_FMT_STRINGS) {
        s = *str++;
        str += s;
        if (str > end) return_error(gs_error_rangecheck);
      }
    }
    else {
      str += fmt;
      if (str > end) return_error(gs_error_rangecheck);
    }
  }
  return str == end ? 0 : gs_error_rangecheck;
}

static void
gdev_dmprt_init_printer_props(gx_device_dmprt *pdev)
{
  dviprt_cfg_t *pprt;
  int i;

  pprt = &pdev->dmprt.prtcfg;

  for (i=0;i<CFG_INTEGER_TYPE_COUNT;i++)
    pprt->integer[i] = 0;
  for (i=0;i<CFG_STRINGS_TYPE_COUNT;i++)
    pprt->strings[i] = 0;
  for (i=0;i<CFG_PRTCODE_TYPE_COUNT;i++) {
    pprt->prtcode[i] = 0;
    pprt->prtcode_size[i] = 0;
  }
  pdev->dmprt.prtcfg_init_f = 1;
}

static int
gdev_dmprt_get_printer_props(gx_device_dmprt *pdev,char *fnamebase)
{
  int code;
  gp_file *fp;
  dviprt_cfg_t cfg;
  char *fname;

  fname = gs_malloc(pdev->memory->non_gc_memory, 256,1,"dviprt_lib_fname");
  if (fname == NULL) return_error(gs_error_VMerror);

  fp = gdev_dmprt_dviprt_lib_fopen(pdev->memory,fnamebase,fname);
  if (fp == NULL) {
    return_error(gs_error_undefinedfilename);
  }
  if (fseek(fp,18,0) < 0)
    return_error(gs_error_ioerror);
  code = fgetc(fp);
  fclose(fp);

  if (code == EOF)
    code = gs_error_ioerror;
  else if (code == 0xff) {
    code = dviprt_readcfg(pdev->memory,fname,&cfg,NULL,0,NULL,0);
  }
  else {
    code = dviprt_readsrc(pdev->memory,fname,&cfg,NULL,0,NULL,0);
  }

  if (code < 0) {
    code = gdev_dmprt_error_no_dviprt_to_gs(code);
  }
  else { /* success */
    memcpy(&pdev->dmprt.prtcfg,&cfg,sizeof(dviprt_cfg_t));
    pddev->dmprt.orig_x_dpi = cfg.integer[CFG_DPI];
    pddev->dmprt.orig_y_dpi =
      cfg.integer[CFG_Y_DPI] > 0 ? cfg.integer[CFG_Y_DPI] : pddev->dmprt.orig_x_dpi;
  }

  gs_free(pdev->memory->non_gc_memory, fname,256,1,"dviprt_lib_fname");

  return code;
}

static const char * gp_file_name_concat_string(const char *, unsigned);
static gp_file *
gdev_dmprt_dviprt_lib_fopen(const gs_memory_t *mem,const char *fnamebase,char *fname)
{
  gp_file *fp = NULL;
  char *env;

  strcpy(fname,fnamebase);
  /* lib_fopen is no longer called like this. Use gp_fopen
   * instead. */
  /* fp = lib_fopen(fname); */
  gp = gp_fopen(mem, fname, gp_fmode_rb);
  if (fp == NULL)
  {
    env = getenv("TEXCFG");
    if (env) {
      strcpy(fname,env);
      strcat(fname, gp_file_name_concat_string(env,strlen(env)));
      strcat(fname,fnamebase);
      fp = gp_fopen(mem,fname,gp_fmode_rb);
    }
  }
  return fp;
}

/* Misc. */
static int
gdev_dmprt_error_no_dviprt_to_gs(int code)
{
  switch (code) {
  case CFG_ERROR_MEMORY:
    return_error(gs_error_VMerror);
  case CFG_ERROR_FILE_OPEN:
  case CFG_ERROR_OUTPUT:
    return gs_errpr_ioerror;
  default:
    return -1;
  }
}

/* Answer the string to be used for combining a directory/device prefix */
/* with a base file name. The prefix directory/device is examined to    */
/* determine if a separator is needed and may return an empty string    */
static const char *
gp_file_name_concat_string(const char *prefix, unsigned plen)
{
    if (plen > 0 && prefix[plen - 1] == '/')
        return "";
    return "/";
}
