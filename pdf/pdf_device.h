/* Copyright (C) 2019-2023 Artifex Software, Inc.
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

#ifndef PDF_DEVICE
#define PDF_DEVICE

int pdfi_device_check_param(gx_device *dev, const char *param, gs_c_param_list *list);
bool pdfi_device_check_param_bool(gx_device *dev, const char *param);
bool pdfi_device_check_param_exists(gx_device *dev, const char *param);
int pdfi_device_set_param_string(gx_device *dev, const char *paramname, const char *value);
int pdfi_device_set_param_bool(gx_device *dev, const char *param, bool value);
int pdfi_device_set_param_float(gx_device *dev, const char *param, float value);
void pdfi_device_set_flags(pdf_context *ctx);
int pdfi_device_misc_config(pdf_context *ctx);

#endif
