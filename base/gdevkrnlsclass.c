/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

#include "gx.h"
#include "gxdcolor.h"
#include "gdevkrnlsclass.h" /* 'standard' built in subclasses, currently First/Last Page and object filter */

/* If set to '1' ths forces all devices to be loaded, even if they won't do anything.
 * This is useful for cluster testing that the very presence of a device doesn't
 * break anything.
 */
#define FORCE_TESTING_SUBCLASSING 0

int install_internal_subclass_devices(gx_device **ppdev, int *devices_loaded)
{
    int code = 0;
    gx_device *dev = (gx_device *)*ppdev, *saved;

#if FORCE_TESTING_SUBCLASSING
    if (!dev->PageHandlerPushed) {
#else
    if (!dev->PageHandlerPushed && (dev->FirstPage != 0 || dev->LastPage != 0 || dev->PageList != 0)) {
#endif
        code = gx_device_subclass(dev, (gx_device *)&gs_flp_device, sizeof(first_last_subclass_data));
        if (code < 0)
            return code;

        saved = dev = dev->child;

        /* Open all devices *after* the new current device */
        do {
            dev->is_open = true;
            dev = dev->child;
        }while(dev);

        dev = saved;

        /* Rewind to top device in chain */
        while(dev->parent)
            dev = dev->parent;

        /* Note in all devices in chain that we have loaded the PageHandler */
        do {
            dev->PageHandlerPushed = true;
            dev = dev->child;
        }while(dev);

        dev = saved;
        if (devices_loaded)
            *devices_loaded = true;
    }
#if FORCE_TESTING_SUBCLASSING
    if (!dev->ObjectHandlerPushed) {
#else
    if (!dev->ObjectHandlerPushed && dev->ObjectFilter != 0) {
#endif
        code = gx_device_subclass(dev, (gx_device *)&gs_obj_filter_device, sizeof(obj_filter_subclass_data));
        if (code < 0)
            return code;

        saved = dev = dev->child;

        /* Open all devices *after* the new current device */
        do {
            dev->is_open = true;
            dev = dev->child;
        }while(dev);

        dev = saved;

        /* Rewind to top device in chain */
        while(dev->parent)
            dev = dev->parent;

        /* Note in all devices in chain that we have loaded the ObjectHandler */
        do {
            dev->ObjectHandlerPushed = true;
            dev = dev->child;
        }while(dev);

        dev = saved;
        if (devices_loaded)
            *devices_loaded = true;
    }
    *ppdev = dev;
    return code;
}
