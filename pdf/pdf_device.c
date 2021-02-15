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

/* Routines for dealing with devices */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_device.h"
#include "gdevvec.h" /* for gs_device_vector */

int pdfi_device_check_param(gx_device *dev, const char *param, gs_c_param_list *list)
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

/* Check value of boolean device parameter */
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

/* Set value of string device parameter */
int pdfi_device_set_param_string(gx_device *dev, const char *paramname, const char *value)
{
    int code;
    gs_c_param_list list;
    gs_param_string paramstring;

    paramstring.data = (byte *)value;
    paramstring.size = strlen(value);
    paramstring.persistent = 0;

    gs_c_param_list_write(&list, dev->memory);

    gs_param_list_set_persistent_keys((gs_param_list *) &list, false);
    code = param_write_string((gs_param_list *)&list, paramname, &paramstring);
    if (code < 0) goto exit;
    gs_c_param_list_read(&list);
    code = gs_putdeviceparams(dev, (gs_param_list *)&list);

 exit:
    gs_c_param_list_release(&list);
    return code;
}

/* Set value of boolean device parameter */
int pdfi_device_set_param_bool(gx_device *dev, const char *param, bool value)
{
    int code;
    gs_c_param_list list;
    bool paramval = value;

    gs_c_param_list_write(&list, dev->memory);

    code = param_write_bool((gs_param_list *)&list, param, &paramval);
    if (code < 0) goto exit;
    gs_c_param_list_read(&list);
    code = gs_putdeviceparams(dev, (gs_param_list *)&list);

 exit:
    gs_c_param_list_release(&list);
    return code;
}

/* Checks whether a parameter exists for the device */
bool pdfi_device_check_param_exists(gx_device *dev, const char *param)
{
    int code;
    gs_c_param_list list;

    code = pdfi_device_check_param(dev, param, &list);
    if (code < 0)
        return false;
    gs_c_param_list_release(&list);
    return true;
}

/* Config some device-related variables */
void pdfi_device_set_flags(pdf_context *ctx)
{
    bool has_pdfmark;
    bool has_ForOPDFRead;
    gx_device *dev = ctx->pgs->device;

    has_pdfmark = pdfi_device_check_param_exists(dev, "pdfmark");
    has_ForOPDFRead = pdfi_device_check_param_bool(dev, "ForOPDFRead");

    /* Cache these so they don't have to constantly be calculated */
    ctx->device_state.writepdfmarks = has_pdfmark || ctx->args.dopdfmarks;
    ctx->device_state.annotations_preserved = ctx->device_state.writepdfmarks && !has_ForOPDFRead;

    /* PreserveTrMode is for pdfwrite device */
    ctx->device_state.preserve_tr_mode = pdfi_device_check_param_bool(dev, "PreserveTrMode");
    ctx->device_state.preserve_smask = pdfi_device_check_param_bool(dev, "PreserveSMask");

    /* See if it is a DeviceN (spot capable) */
    ctx->device_state.spot_capable = dev_proc(dev, dev_spec_op)(dev, gxdso_supports_devn, NULL, 0);

    /* If multi-page output, can't do certain pdfmarks */
    if (ctx->device_state.writepdfmarks) {
        if (gx_outputfile_is_separate_pages(((gx_device_vector *)dev)->fname, dev->memory)) {
            ctx->args.no_pdfmark_outlines = true;
            ctx->args.no_pdfmark_dests = true;
        }
    }

#if DEBUG_DEVICE
    dbgmprintf2(ctx->memory, "Device writepdfmarks=%s, annotations_preserved=%s\n",
        ctx->writepdfmarks ? "TRUE" : "FALSE",
                ctx->annotations_preserved ? "TRUE" : "FALSE");
#endif
}

/* Config the output device
 * This will configure any special device parameters.
 * Right now it just sets up some stuff for pdfwrite.
 */
int pdfi_device_misc_config(pdf_context *ctx)
{
    bool has_pdfmark;
    int code;
    gx_device *dev = ctx->pgs->device;

    /* I am using pdfmark to identify the pdfwrite device */
    has_pdfmark = pdfi_device_check_param_bool(dev, "pdfmark");
    /* (only handling pdfwrite for now) */
    if (!has_pdfmark)
        return 0;

    /* TODO: I think the pdfwrite device should have these automatically set to true,
     * but that doesn't seem to be the case now.
     * See pdf_document_metadata()
     */
    code = pdfi_device_set_param_string(dev, "AutoRotatePages", "PageByPage");
    if (code < 0) goto exit;

 exit:
    return code;
}
