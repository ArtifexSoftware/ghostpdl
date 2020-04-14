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

/* Optional Content routines */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_misc.h"
#include "pdf_gstate.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_optcontent.h"


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
        if (code < 0)
            goto cleanup;
        if (code > 0) {
            if (pdfi_array_known(ctx, ON, (pdf_obj *)ocdict, NULL))
                is_visible = true;
        }
    }

    if (is_visible) {
        code = pdfi_dict_knownget_type(ctx, D, "OFF", PDF_ARRAY, (pdf_obj **)&OFF);
        if (code < 0)
            goto cleanup;
        if (code > 0) {
            if (pdfi_array_known(ctx, OFF, (pdf_obj *)ocdict, NULL))
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
pdfi_oc_check_OCG_usage(pdf_context *ctx, pdf_dict *ocdict)
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
pdfi_oc_check_OCMD_array(pdf_context *ctx, pdf_array *array, ocmd_p_type type)
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
        if (code < 0) continue;
        if (val->type != PDF_DICT) {
            dmprintf1(ctx->memory, "WARNING: OCMD array contains item type %d, expected PDF_DICT or PDF_NULL\n", val->type);
            continue;
        }

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
pdfi_oc_check_OCMD(pdf_context *ctx, pdf_dict *ocdict)
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
    if (code == 0 || pdfi_name_is((pdf_name *)Pname, "AnyOn")) {
        Ptype = P_AnyOn;
    } else if (pdfi_name_is((pdf_name *)Pname, "AllOn")) {
        Ptype = P_AllOn;
    } else if (pdfi_name_is((pdf_name *)Pname, "AnyOff")) {
        Ptype = P_AnyOff;
    } else if (pdfi_name_is((pdf_name *)Pname, "AllOff")) {
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
        is_visible = pdfi_oc_check_OCMD_array(ctx, OCGs_array, Ptype);
    }

 cleanup:
    pdfi_countdown(VE);
    pdfi_countdown(obj);
    pdfi_countdown(Pname);

    return is_visible;
}

/* Check if an OCG or OCMD is visible, passing in OC dict */
bool
pdfi_oc_is_ocg_visible(pdf_context *ctx, pdf_dict *ocdict)
{
    pdf_name *type = NULL;
    bool is_visible = true;
    int code;

    /* Type can be either OCMD or OCG.
     */
    code = pdfi_dict_knownget_type(ctx, ocdict, "Type", PDF_NAME, (pdf_obj **)&type);
    if (code <= 0)
        goto cleanup;

    if (pdfi_name_is(type, "OCMD")) {
        is_visible = pdfi_oc_check_OCMD(ctx, ocdict);
    } else if (pdfi_name_is(type, "OCG")) {
        is_visible = pdfi_get_default_OCG_val(ctx, ocdict);
        if (is_visible)
            is_visible = pdfi_oc_check_OCG_usage(ctx, ocdict);
    } else {
        char str[100];
        memcpy(str, (const char *)type->data, type->length);
        str[type->length] = '\0';
        dmprintf1(ctx->memory, "WARNING: OC dict type is %s, expected OCG or OCMD\n", str);
    }

 cleanup:
    pdfi_countdown(type);

    if (ctx->pdfdebug) {
        dmprintf2(ctx->memory, "OCG: OC Dict %ld %s visible\n", ocdict->object_num,
                  is_visible ? "IS" : "IS NOT");
    }
    return is_visible;
}

#define NUM_CONTENT_LEVELS 100
typedef struct {
    byte *flags;
    uint64_t num_off;
    uint64_t max_flags;
} pdfi_oc_levels_t;

static int pdfi_oc_levels_init(pdf_context *ctx, pdfi_oc_levels_t **levels)
{
    byte *data;
    pdfi_oc_levels_t *new;

    *levels = NULL;

    new = (pdfi_oc_levels_t *)gs_alloc_bytes(ctx->memory, sizeof(pdfi_oc_levels_t),
                                             "pdfi_oc_levels_init (levels)");
    if (!new)
        return_error(gs_error_VMerror);

    data = (byte *)gs_alloc_bytes(ctx->memory, NUM_CONTENT_LEVELS, "pdfi_oc_levels_init (data)");
    if (!data) {
        gs_free_object(ctx->memory, new, "pdfi_oc_levels_init (levels (error))");
        return_error(gs_error_VMerror);
    }
    memset(data, 0, NUM_CONTENT_LEVELS);

    new->flags = data;
    new->num_off = 0;
    new->max_flags = NUM_CONTENT_LEVELS;
    *levels = new;

    return 0;
}

static int pdfi_oc_levels_free(pdf_context *ctx, pdfi_oc_levels_t *levels)
{
    if (!levels)
        return 0;
    gs_free_object(ctx->memory, levels->flags, "pdfi_oc_levels_free (flags)");
    gs_free_object(ctx->memory, levels, "pdfi_oc_levels_free (levels)");

    return 0;
}

static int pdfi_oc_levels_set(pdf_context *ctx, pdfi_oc_levels_t *levels, uint64_t index)
{
    byte *new = NULL;
    uint64_t newmax;

    if (index > levels->max_flags) {
        /* Expand the flags buffer */
        newmax = levels->max_flags + NUM_CONTENT_LEVELS;
        if (index > newmax)
            return_error(gs_error_Fatal); /* shouldn't happen */
        new = gs_alloc_bytes(ctx->memory, newmax, "pdfi_oc_levels_set (new data)");
        if (!new)
            return_error(gs_error_VMerror);
        memset(new, 0, newmax);
        memcpy(new, levels->flags, levels->max_flags);
        gs_free_object(ctx->memory, levels->flags, "pdfi_oc_levels_set (old data)");
        levels->flags = new;
        levels->max_flags += NUM_CONTENT_LEVELS;
    }

    if (levels->flags[index] == 0)
        levels->num_off ++;
    levels->flags[index] = 1;
    return 0;
}

static int pdfi_oc_levels_clear(pdf_context *ctx, pdfi_oc_levels_t *levels, uint64_t index)
{
    if (index > levels->max_flags)
        return -1;
    if (levels->flags[index] != 0)
        levels->num_off --;
    levels->flags[index] = 0;
    return 0;
}


/* Test if content is turned off for this element.
 */
bool pdfi_oc_is_off(pdf_context *ctx)
{
    pdfi_oc_levels_t *levels = (pdfi_oc_levels_t *)ctx->OFFlevels;
    uint64_t num_off = levels->num_off;

    return (num_off != 0);
}

int pdfi_oc_init(pdf_context *ctx)
{
    int code;

    ctx->BMClevel = 0;
    if (ctx->OFFlevels) {
        pdfi_oc_levels_free(ctx, ctx->OFFlevels);
        ctx->OFFlevels = NULL;
    }
    code = pdfi_oc_levels_init(ctx, (pdfi_oc_levels_t **)&ctx->OFFlevels);
    if (code < 0)
        return code;

    return 0;
}

int pdfi_oc_free(pdf_context *ctx)
{
    int code;

    code = pdfi_oc_levels_free(ctx, (pdfi_oc_levels_t *)ctx->OFFlevels);
    ctx->OFFlevels = NULL;
    return code;
}

/* begin marked content sequence */
/* TODO: Incomplete implementation, it is ignoring the argument */
int pdfi_op_BMC(pdf_context *ctx)
{
    if (pdfi_count_stack(ctx) >= 1) {
        pdfi_pop(ctx, 1);
    } else
        pdfi_clearstack(ctx);
    ctx->BMClevel ++;
    return 0;
}

/* begin marked content sequence with property list */
/* TODO: Incomplete implementation, only tries to do something sensible for OC */
int pdfi_op_BDC(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_name *tag = NULL;
    pdf_name *properties = NULL;
    pdf_dict *oc_dict = NULL;
    int code = 0;
    bool ocg_is_visible;

    if (pdfi_count_stack(ctx) < 2) {
        /* TODO: Flag error? */
        pdfi_clearstack(ctx);
        return 0;
    }

    ctx->BMClevel ++;

    /* Check if second arg is OC and handle it if so */
    tag = (pdf_name *)ctx->stack_top[-2];
    if (tag->type != PDF_NAME)
        goto exit;
    if (!pdfi_name_is(tag, "OC"))
        goto exit;

    /* Check if first arg is a name and handle it if so */
    /* TODO: spec says it could also be an inline dict that we should be able to handle,
     * but I am just matching what gs does for now, and it doesn't handle that case.
     */
    properties = (pdf_name *)ctx->stack_top[-1];
    if (tag->type != PDF_NAME)
        goto exit;

    /* If it's a name, look it up in Properties */
    code = pdfi_find_resource(ctx, (unsigned char *)"Properties", properties,
                              stream_dict, page_dict, (pdf_obj **)&oc_dict);
    if (code != 0)
        goto exit;
    if (oc_dict->type != PDF_DICT)
        goto exit;

    /* Now we have an OC dict, see if it's visible */
    ocg_is_visible = pdfi_oc_is_ocg_visible(ctx, oc_dict);
    if (!ocg_is_visible)
        code = pdfi_oc_levels_set(ctx, ctx->OFFlevels, ctx->BMClevel);

 exit:
    pdfi_pop(ctx, 2); /* pop args */
    pdfi_countdown(oc_dict);
    return code;
}

/* end marked content sequence */
int pdfi_op_EMC(pdf_context *ctx)
{
    int code;

    code = pdfi_oc_levels_clear(ctx, ctx->OFFlevels, ctx->BMClevel);

    /* TODO: Should we flag error on too many EMC? */
    if (ctx->BMClevel > 0)
        ctx->BMClevel --;
    return code;
}
