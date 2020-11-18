/* Copyright (C) 2001-2020 Artifex Software, Inc.
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

#ifndef PDF_DEVICE
#define PDF_DEVICE

int pdfi_device_check_param(gx_device *dev, const char *param, gs_c_param_list *list);
bool pdfi_device_check_param_bool(gx_device *dev, const char *param);
bool pdfi_device_check_param_exists(gx_device *dev, const char *param);
bool pdfi_device_set_param_bool(gx_device *dev, const char *param, bool value);
void pdfi_device_set_flags(pdf_context *ctx);
int pdfi_device_misc_config(pdf_context *ctx);

#endif
