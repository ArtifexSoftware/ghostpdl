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

#include "gx.h"
#include "gxdcolor.h"
#include "gdevkrnlsclass.h" /* 'standard' built in subclasses, currently Page, Object, and Nup filter */

/* If set to '1' ths forces all devices to be loaded, even if they won't do anything.
 * This is useful for cluster testing that the very presence of a device doesn't
 * break anything. This requires that all of these devices pass through if the params
 * they use are 0/NULL.
 */
#define FORCE_TESTING_SUBCLASSING 0

/* This installs the 'kernel' device classes. If you add any devices here you should
 * almost certainly edit gdevp14.c, gs_pdf14_device_push() and add the new device to the list
 * of devices which the push of the compositor claims are already installed (to prevent
 * a second copy being installed by gdev_prn_open).
 */
int install_internal_subclass_devices(gx_device **ppdev, int *devices_loaded)
{
    int code = 0;
    gx_device *dev = (gx_device *)*ppdev, *saved;

    /*
     * NOTE: the Nup device should precede the PageHandler so the FirstPage, LastPage
     *       and PageList will filter pages out BEFORE they are seen by the nesting.
     */

#if FORCE_TESTING_SUBCLASSING
    if (!dev->NupHandlerPushed) {
#else
    if (!dev->NupHandlerPushed && dev->NupControl != 0) {
#endif
        code = gx_device_nup_device_install(dev);
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

        /* Note in all devices in chain that we have loaded the NupHandler */
        do {
            dev->NupHandlerPushed = true;
            dev = dev->child;
        }while(dev);

        dev = saved;
        if (devices_loaded)
            *devices_loaded = true;
    }
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
