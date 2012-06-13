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

/* $Id: gslt.h 2713 2007-01-02 22:22:14Z henrys $ */
/* gslt "Ghostscript Lite" convenience header */

/*
 * The following typedefs will allow this file to be used
 * without including the regular gs headers.
 *
 * Always include this file *after* the regular gs headers.
 */

#ifndef gsmemory_INCLUDED
typedef struct gs_memory_s gs_memory_t;
#endif

#ifndef gxdevcli_INCLUDED
typedef struct gx_device_s gx_device;
#endif

#ifndef gsstate_INCLUDED
typedef struct gs_state_s gs_state;
#endif

#ifndef gs_matrix_DEFINED
typedef struct gs_matrix_s gs_matrix;
struct gs_matrix_s { float xx, xy, yx, yy, tx, ty; };
#endif

#ifndef gs_font_dir_DEFINED
typedef struct gs_font_dir_s gs_font_dir;
#endif

/*
 * A simple memory allocator, based on the the allocator for ghostpcl.
 */

gs_memory_t *gslt_alloc_init(void);

/*
 * Initialization helpers to set up the memory, device and gstate structs.
 */

gs_memory_t * gslt_init_library();
gx_device * gslt_init_device(gs_memory_t *mem, char *name);
gs_state *gslt_init_state(gs_memory_t *mem, gx_device *dev);
void gslt_free_state(gs_memory_t *mem, gs_state *pgs);
void gslt_free_device(gs_memory_t *mem, gx_device *dev);
void gslt_free_library(gs_memory_t *mem);

void gslt_get_device_param(gs_memory_t *mem, gx_device *dev, char *key);
void gslt_set_device_param(gs_memory_t *mem, gx_device *dev, char *key, char *val);
