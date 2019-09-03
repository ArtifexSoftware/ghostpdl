/* Copyright (C) 2001-2018 Artifex Software, Inc.
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

/* Routines for dealing with devices */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_device.h"

static int pdfi_device_check_param(gx_device *dev, const char *param, gs_c_param_list *list)
{
    dev_param_req_t request;
    int code;

    gs_c_param_list_write(list, dev->memory);
    /* Stuff the data into a structure for passing to the spec_op */
    request.Param = (char *)param;
    request.list = list;
    code = dev_proc(dev, dev_spec_op)(dev, gxdso_get_dev_param, &request, sizeof(dev_param_req_t));
    if (code < 0) {
        gs_c_param_list_release(list);
        return code;
    }
    return 0;
}

bool pdfi_device_check_param_bool(gx_device *dev, const char *param)
{
    int code;
    gs_c_param_list list;
    int value;

    code = pdfi_device_check_param(dev, param, &list);
    if (code < 0)
        return false;
    gs_c_param_list_read(&list);
    code = param_read_bool((gs_param_list *)&list,
                           param,
                           &value);
    gs_c_param_list_release(&list);
    return value;
}
