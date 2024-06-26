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


/* Device and page control API */

#ifndef gsdevice_INCLUDED
#  define gsdevice_INCLUDED

#include "std.h"
#include "gsgstate.h"
#include "gsmatrix.h"
#include "gsparam.h"

/* We maintain the #ifdef gx_device_DEFINED dance, even though
 * this is now the primary place in the source that defines it,
 * to allow iapi.h to remain standalone. */
#ifndef gx_device_DEFINED
#  define gx_device_DEFINED
typedef struct gx_device_s gx_device;
#endif

typedef struct gx_device_memory_s gx_device_memory;

/* Device procedures not involving a graphics state. */

const gx_device *gs_getdevice(int);

/****** DEPRECATED: use  gs_getdefaultlibdevice() *********/
const gx_device *gs_getdefaultdevice(void);

const gx_device *
gs_getdefaultlibdevice(gs_memory_t *mem);

int gs_opendevice(gx_device *);
int gs_copyscanlines(gx_device *, int, byte *, uint, int *, uint *);
int gs_copydevice(gx_device **, const gx_device *, gs_memory_t *);
/*
 * If keep_open is true and dev->is_open is true, the copy *may* have
 * is_open = true; otherwise, the copy will have is_open = false.
 * copydevice is equivalent to copydevice2 with keep_open = false.
 */
int gs_copydevice2(gx_device **pnew_dev, const gx_device *dev,
                   bool keep_open, gs_memory_t *mem);

#define gs_makeimagedevice(pdev, pmat, w, h, colors, colors_size, mem)\
  gs_makewordimagedevice(pdev, pmat, w, h, colors, colors_size, false, true, mem)
int gs_makewordimagedevice(gx_device ** pnew_dev, const gs_matrix * pmat,
                           uint width, uint height,
                           const byte * colors, int num_colors,
                           bool word_oriented, bool page_device,
                           gs_memory_t * mem);

#define gs_initialize_imagedevice(mdev, pmat, w, h, colors, colors_size, mem)\
  gs_initialize_wordimagedevice(mdev, pmat, w, h, colors, color_size, false, true, mem)
int gs_initialize_wordimagedevice(gx_device_memory * new_dev,
                                  const gs_matrix * pmat,
                                  uint width, uint height,
                                  const byte * colors, int colors_size,
                                  bool word_oriented, bool page_device,
                                  gs_memory_t * mem);
const char *gs_devicename(const gx_device *);
void gs_deviceinitialmatrix(gx_device *, gs_matrix *);

/* VMS limits identifiers to 31 characters. */
int gs_get_device_or_hw_params(gx_device *, gs_param_list *, bool);
#define gs_getdeviceparams(dev, plist)\
  gs_get_device_or_hw_params(dev, plist, false)
#define gs_gethardwareparams(dev, plist)\
  gs_get_device_or_hw_params(dev, plist, true)
/* BACKWARD COMPATIBILITY */
#define gs_get_device_or_hardware_params gs_get_device_or_hw_params

int gs_putdeviceparams(gx_device *, gs_param_list *);
int gs_closedevice(gx_device *);

/* Device procedures involving an gs_gstate. */

int gs_gstate_putdeviceparams(gs_gstate *pgs, gx_device *dev,
gs_param_list *plist);

int gs_flushpage(gs_gstate *);
int gs_copypage(gs_gstate *);
int gs_output_page(gs_gstate *, int, int);
int gs_nulldevice(gs_gstate *);
int gs_setdevice(gs_gstate *, gx_device *);
int gs_setdevice_no_erase(gs_gstate *, gx_device *);		/* returns 1 */
                                                /* if erasepage required */
int gs_setdevice_no_init(gs_gstate *, gx_device *);
gx_device *gs_currentdevice(const gs_gstate *);

/* gzstate.h redefines the following: */
#ifndef gs_currentdevice_inline
#  define gs_currentdevice_inline(pgs) gs_currentdevice(pgs)
#endif

#endif /* gsdevice_INCLUDED */
