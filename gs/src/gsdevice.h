/* Copyright (C) 1994, 1995, 1997, 1998 Aladdin Enterprises.  All rights reserved.

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


/* Device and page control API */

#ifndef gsdevice_INCLUDED
#  define gsdevice_INCLUDED

#ifndef gx_device_DEFINED
#  define gx_device_DEFINED
typedef struct gx_device_s gx_device;
#endif

#ifndef gx_device_memory_DEFINED
#  define gx_device_memory_DEFINED
typedef struct gx_device_memory_s gx_device_memory;
#endif

#ifndef gs_matrix_DEFINED
#  define gs_matrix_DEFINED
typedef struct gs_matrix_s gs_matrix;
#endif

#ifndef gs_param_list_DEFINED
#  define gs_param_list_DEFINED
typedef struct gs_param_list_s gs_param_list;
#endif

#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s gs_state;
#endif

int gs_flushpage(P1(gs_state *));
int gs_copypage(P1(gs_state *));
int gs_output_page(P3(gs_state *, int, int));
int gs_copyscanlines(P6(gx_device *, int, byte *, uint, int *, uint *));
const gx_device *gs_getdevice(P1(int));
int gs_copydevice(P3(gx_device **, const gx_device *, gs_memory_t *));

#define gs_makeimagedevice(pdev, pmat, w, h, colors, colors_size, mem)\
  gs_makewordimagedevice(pdev, pmat, w, h, colors, colors_size, false, true, mem)
int gs_makewordimagedevice(P9(gx_device ** pnew_dev, const gs_matrix * pmat,
			      uint width, uint height,
			      const byte * colors, int num_colors,
			      bool word_oriented, bool page_device,
			      gs_memory_t * mem));

#define gs_initialize_imagedevice(mdev, pmat, w, h, colors, colors_size, mem)\
  gs_initialize_wordimagedevice(mdev, pmat, w, h, colors, color_size, false, true, mem)
int gs_initialize_wordimagedevice(P9(gx_device_memory * new_dev,
				     const gs_matrix * pmat,
				     uint width, uint height,
				     const byte * colors, int colors_size,
				     bool word_oriented, bool page_device,
				     gs_memory_t * mem));

int gs_nulldevice(P1(gs_state *));
int gs_setdevice(P2(gs_state *, gx_device *));
int gs_setdevice_no_erase(P2(gs_state *, gx_device *));		/* returns 1 */
						/* if erasepage required */
int gs_setdevice_no_init(P2(gs_state *, gx_device *));
gx_device *gs_currentdevice(P1(const gs_state *));

/* gzstate.h redefines the following: */
#ifndef gs_currentdevice_inline
#  define gs_currentdevice_inline(pgs) gs_currentdevice(pgs)
#endif
const char *gs_devicename(P1(const gx_device *));
void gs_deviceinitialmatrix(P2(gx_device *, gs_matrix *));

int gs_get_device_or_hardware_params(P3(gx_device *, gs_param_list *, bool));
#define gs_getdeviceparams(dev, plist)\
  gs_get_device_or_hardware_params(dev, plist, false)
#define gs_gethardwareparams(dev, plist)\
  gs_get_device_or_hardware_params(dev, plist, true)

int gs_putdeviceparams(P2(gx_device *, gs_param_list *));
int gs_closedevice(P1(gx_device *));

#endif /* gsdevice_INCLUDED */
