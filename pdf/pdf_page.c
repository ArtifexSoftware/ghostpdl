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

/* Page-level operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_page.h"
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_loop_detect.h"
#include "pdf_colour.h"
#include "pdf_trans.h"

#include "gsiparm4.h"
#include "gsiparm3.h"

#include "gstrans.h"

/* Find the default value for an ocdict, based on contents of OCProperties */
/* NOTE: the spec says that if BaseState is present, it won't be set to "OFF",
 * but this doesn't seem to be the case (Bug 691491).  Also, the spec
 * says the ON and OFF arrays are redundant in certain cases.  We just
 * look at everything anyway.
 * Default is going to be visible unless anything here indicates that it
 * should be turned off.
 */
static bool
pdfi_get_default_OCG_val(pdf_context *ctx, pdf_dict *ocdict)
{
    bool is_visible = true;
    pdf_dict *D = NULL;
    pdf_obj *BaseState = NULL;
    pdf_array *OFF = NULL;
    pdf_array *ON = NULL;
    int code;

    if (ctx->OCProperties == NULL)
        return is_visible;

    code = pdfi_dict_knownget_type(ctx, ctx->OCProperties, "D", PDF_DICT, (pdf_obj **)&D);
    if (code <= 0)
        goto cleanup;

    code = pdfi_dict_knownget_type(ctx, D, "BaseState", PDF_NAME, &BaseState);
    if (code < 0) {
        goto cleanup;
    }
    if (code > 0) {
        if (pdfi_name_is((pdf_name *)BaseState, "OFF")) {
            is_visible = false;
        }
    }

    if (!is_visible) {
        code = pdfi_dict_knownget_type(ctx, D, "ON", PDF_ARRAY, (pdf_obj **)&ON);
        if (code < 0) {
            goto cleanup;
        }
        if (code > 0) {
            if (pdfi_array_known(ON, (pdf_obj *)ocdict, NULL))
                is_visible = true;
        }
    }

    if (is_visible) {
        code = pdfi_dict_get_type(ctx, D, "OFF", PDF_ARRAY, (pdf_obj **)&OFF);
        if (code < 0) {
            goto cleanup;
        }
        if (code > 0) {
            if (pdfi_array_known(OFF, (pdf_obj *)ocdict, NULL))
                is_visible = false;
        }
    }


 cleanup:
    pdfi_countdown(BaseState);
    pdfi_countdown(D);
    pdfi_countdown(OFF);
    pdfi_countdown(ON);
    return is_visible;
}

/* Check Usage for an OCG */
static bool
pdfi_page_check_OCG_usage(pdf_context *ctx, pdf_dict *ocdict)
{
    bool is_visible = true;
    int code;
    pdf_dict *Usage = NULL;
    pdf_dict *dict = NULL;
    pdf_obj *name = NULL;

    /* Check Usage to see if it has additional info */
    code = pdfi_dict_knownget_type(ctx, ocdict, "Usage", PDF_DICT, (pdf_obj **)&Usage);
    if (code <= 0) {
        /* No Usage, so we're done */
        goto cleanup;
    }

    if (ctx->printed) {
        code = pdfi_dict_knownget_type(ctx, ocdict, "Print", PDF_DICT, (pdf_obj **)&dict);
        if (code <= 0)
            goto cleanup;
        code = pdfi_dict_knownget_type(ctx, dict, "PrintState", PDF_NAME, &name);
        if (code <= 0)
            goto cleanup;
    } else {
        code = pdfi_dict_knownget_type(ctx, ocdict, "View", PDF_DICT, (pdf_obj **)&dict);
        if (code <= 0)
            goto cleanup;
        code = pdfi_dict_knownget_type(ctx, dict, "ViewState", PDF_NAME, &name);
        if (code <= 0)
            goto cleanup;
    }
    if (pdfi_name_strcmp((pdf_name *)name, "OFF")) {
        is_visible = false;
    }

 cleanup:
    pdfi_countdown(Usage);
    pdfi_countdown(dict);
    pdfi_countdown(name);

    return is_visible;
}

typedef enum {
    P_AnyOn,
    P_AllOn,
    P_AllOff,
    P_AnyOff
} ocmd_p_type;

static bool
pdfi_page_check_OCMD_array(pdf_context *ctx, pdf_array *array, ocmd_p_type type)
{
    bool is_visible;
    uint64_t i;
    int code;

    /* Setup default */
    switch (type) {
    case P_AnyOn:
    case P_AnyOff:
        is_visible = false;
        break;
    case P_AllOn:
    case P_AllOff:
        is_visible = true;
        break;
    }

    for (i=0; i<pdfi_array_size(array); i++) {
        bool vis;
        pdf_obj *val = NULL;

        code = pdfi_array_peek(ctx, array, i, &val);
        /* The spec says it could be PDF_DICT or PDF_NULL,
         * But not flagging any errors here, in spirit of PDF
         * being permissive.  Just quietly skip things that
         * don't make sense.
         */
        if (code < 0) continue;
        if (val->type != PDF_DICT) continue;

        vis = pdfi_get_default_OCG_val(ctx, (pdf_dict *)val);
        switch (type) {
        case P_AnyOn:
            /* visible if any is on */
            if (vis) {
                is_visible = true;
                goto cleanup;
            }
            break;
        case P_AllOn:
            /* visible if all on */
            if (!vis) {
                is_visible = false;
                goto cleanup;
            }
            break;
        case P_AllOff:
            /* visible if all are off */
            if (vis) {
                is_visible = false;
                goto cleanup;
            }
            break;
        case P_AnyOff:
            /* visible if any is off */
            if (!vis) {
                is_visible = true;
                goto cleanup;
            }
            break;
        }
    }

 cleanup:
    return is_visible;
}

static bool
pdfi_page_check_OCMD(pdf_context *ctx, pdf_dict *ocdict)
{
    bool is_visible = true;
    int code;
    pdf_obj *VE = NULL;
    pdf_obj *obj = NULL;
    pdf_obj *Pname = NULL;
    pdf_dict *OCGs_dict = NULL; /* alias, don't need to free */
    pdf_array *OCGs_array = NULL; /* alias, don't need to free */
    ocmd_p_type Ptype = P_AnyOn;

    /* TODO: We don't support this, so log a warning and ignore */
    code = pdfi_dict_knownget_type(ctx, ocdict, "VE", PDF_ARRAY, &VE);
    if (code > 0) {
        dmprintf(ctx->memory, "WARNING: OCMD contains VE, which is not supported (ignoring)\n");
    }

    code = pdfi_dict_knownget(ctx, ocdict, "OCGs", &obj);
    if (code <= 0)
        goto cleanup;
    if (obj->type == PDF_ARRAY) {
        OCGs_array = (pdf_array *)obj;
    } else if (obj->type == PDF_DICT) {
        OCGs_dict = (pdf_dict *)obj;
    } else {
        goto cleanup;
    }

    code = pdfi_dict_knownget_type(ctx, ocdict, "P", PDF_NAME, &Pname);
    if (code < 0)
        goto cleanup;
    if (code == 0 || pdfi_name_strcmp((pdf_name *)Pname, "AnyOn")) {
        Ptype = P_AnyOn;
    } else if (pdfi_name_strcmp((pdf_name *)Pname, "AllOn")) {
        Ptype = P_AllOn;
    } else if (pdfi_name_strcmp((pdf_name *)Pname, "AnyOff")) {
        Ptype = P_AnyOff;
    } else if (pdfi_name_strcmp((pdf_name *)Pname, "AllOff")) {
        Ptype = P_AllOff;
    } else {
        Ptype = P_AnyOn;
    }

    if (OCGs_dict) {
        switch (Ptype) {
        case P_AnyOn:
        case P_AllOn:
            is_visible = pdfi_get_default_OCG_val(ctx, OCGs_dict);
            break;
        case P_AllOff:
        case P_AnyOff:
            is_visible = !pdfi_get_default_OCG_val(ctx, OCGs_dict);
            break;
        }
    } else {
        is_visible = pdfi_page_check_OCMD_array(ctx, OCGs_array, Ptype);
    }

 cleanup:
    pdfi_countdown(VE);
    pdfi_countdown(obj);
    pdfi_countdown(Pname);

    return is_visible;
}

/* Check if an OCG or OCMD is visible, passing in OC dict */
bool
pdfi_page_is_ocg_visible(pdf_context *ctx, pdf_dict *ocdict)
{
    pdf_obj *type = NULL;
    bool is_visible = true;
    int code;

    /* Type can be either OCMD or OCG.
     * PS implementation only checked for OCMD and assumed missing or non-OCMD means OCG.
     */
    code = pdfi_dict_knownget_type(ctx, ocdict, "Type", PDF_NAME, &type);
    if (code <= 0)
        goto cleanup;

    if (pdfi_name_is((pdf_name *)type, "OCMD")) {
        is_visible = pdfi_page_check_OCMD(ctx, ocdict);
    } else {
        is_visible = pdfi_get_default_OCG_val(ctx, ocdict);
        if (is_visible)
            is_visible = pdfi_page_check_OCG_usage(ctx, ocdict);
    }

 cleanup:
    pdfi_countdown(type);

    return is_visible;
}
