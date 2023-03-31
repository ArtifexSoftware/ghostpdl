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


/* Common extern definitions for the "standard" subclass devices */

#ifndef gdev_subclass_dev_INCLUDED
#  define gdev_subclass_dev_INCLUDED

#include "gdevnup.h"
#include "gdevflp.h"
#include "gdevoflt.h"

extern gx_device_obj_filter  gs_obj_filter_device;

extern gx_device_flp  gs_flp_device;

extern gx_device_nup gs_nup_device;

int install_internal_subclass_devices(gx_device **ppdev, int *devices_loaded);

#endif /* gdev_subclass_dev_INCLUDED */
