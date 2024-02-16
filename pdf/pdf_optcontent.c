/* Copyright (C) 2019-2024 Artifex Software, Inc.
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

/* Optional Content routines */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_misc.h"
#include "pdf_font_types.h"
#include "pdf_gstate.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_doc.h"
#include "pdf_mark.h"
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

    if (ctx->args.printed) {
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

static bool pdfi_oc_default_visibility(pdf_context *ctx)
{
    int code;
    pdf_dict *D_dict = NULL;
    pdf_name *B = NULL;

    code = pdfi_dict_knownget_type(ctx, ctx->OCProperties, "D", PDF_DICT, (pdf_obj **)&D_dict);
    if (code < 0 || D_dict == NULL)
        return true;

    code = pdfi_dict_knownget_type(ctx, D_dict, "BaseState", PDF_NAME, (pdf_obj **)&B);
    (void)pdfi_countdown(D_dict);
    D_dict = NULL;
    if (code < 0 || B == NULL)
        return true;

    if (pdfi_name_is(B, "OFF")) {
        (void)pdfi_countdown(B);
        return false;
    }

    (void)pdfi_countdown(B);
    return true;
}

static bool
pdfi_oc_check_OCMD_array(pdf_context *ctx, pdf_array *array, ocmd_p_type type)
{
    bool is_visible, hit = false;
    uint64_t i;
    int code;
    pdf_obj *val = NULL;

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

        code = pdfi_array_get(ctx, array, i, &val);
        if (code < 0) continue;
        if (pdfi_type_of(val) != PDF_DICT) {
            dmprintf1(ctx->memory, "WARNING: OCMD array contains item type %d, expected PDF_DICT or PDF_NULL\n", pdfi_type_of(val));
            pdfi_countdown(val);
            val = NULL;
            continue;
        }

        hit = true;
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
        pdfi_countdown(val);
        val = NULL;
    }

    /* If the array was empty, or contained only null or deleted entries, then it has no effect
     * PDF Reference 1.7 p366, table 4.49, OCGs entry. I'm interpreting this to mean that we should use
     * the OCProperties 'D' dictionary, set the visibility state to the BaseState. Since this is an OCMD
     * I'm assuming that the ON and OFF arrays (which apply to Optional Content Groups) shouold not be
     * consulted. We need to cater for the fact that BaseState is optional (but default is ON). D is
     * specified as 'Required' but it seems best not to take any chances on that!
     */
    if (!hit)
        is_visible = pdfi_oc_default_visibility(ctx);

cleanup:
    pdfi_countdown(val);
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
    pdf_dict *UsageDict = NULL, *StateDict = NULL;
    pdf_obj *State = NULL;
    ocmd_p_type Ptype = P_AnyOn;

    /* TODO: We don't support this, so log a warning and ignore */
    code = pdfi_dict_knownget_type(ctx, ocdict, "VE", PDF_ARRAY, &VE);
    if (code > 0) {
        dmprintf(ctx->memory, "WARNING: OCMD contains VE, which is not supported (ignoring)\n");
    }

    code = pdfi_dict_knownget(ctx, ocdict, "OCGs", &obj);
    if (code <= 0)
        goto cleanup;
    if (pdfi_type_of(obj) == PDF_ARRAY) {
        OCGs_array = (pdf_array *)obj;
    } else if (pdfi_type_of(obj) == PDF_DICT) {
        OCGs_dict = (pdf_dict *)obj;
    } else {
        is_visible = pdfi_oc_default_visibility(ctx);
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

    if (OCGs_dict) {
        code = pdfi_dict_knownget_type(ctx, OCGs_dict, "Usage", PDF_DICT, (pdf_obj **)&UsageDict);
        if (code < 0)
            goto cleanup;

        if (UsageDict != NULL) {
            if (ctx->args.printed) {
                code = pdfi_dict_knownget_type(ctx, UsageDict, "Print", PDF_DICT, (pdf_obj **)&StateDict);
                if (code < 0)
                    goto cleanup;
                if (StateDict) {
                    code = pdfi_dict_knownget_type(ctx, StateDict, "PrintState", PDF_NAME, &State);
                    if (code < 0)
                        goto cleanup;
                }
            } else {
                code = pdfi_dict_knownget_type(ctx, UsageDict, "View", PDF_DICT, (pdf_obj **)&StateDict);
                if (code < 0)
                    goto cleanup;
                if (StateDict) {
                    code = pdfi_dict_knownget_type(ctx, StateDict, "ViewState", PDF_NAME, &State);
                    if (code < 0)
                        goto cleanup;
                }
            }
            if (State) {
                if (pdfi_name_is((const pdf_name *)State, "ON"))
                    is_visible = true;
                else
                    if (pdfi_name_is((const pdf_name *)State, "OFF"))
                        is_visible = false;
                    else
                        pdfi_set_error(ctx, 0, NULL, E_PDF_BAD_VALUE, "pdfi_oc_check_OCMD", "Usage Dictionary State is neither ON nor OFF");
            }
        }
    }

 cleanup:
    pdfi_countdown(State);
    pdfi_countdown(StateDict);
    pdfi_countdown(UsageDict);
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
        memcpy(str, (const char *)type->data, type->length < 100 ? type->length : 99);
        str[type->length < 100 ? type->length : 99] = '\0';
        dmprintf1(ctx->memory, "WARNING: OC dict type is %s, expected OCG or OCMD\n", str);
    }

 cleanup:
    pdfi_countdown(type);

    if (ctx->args.pdfdebug) {
        dmprintf2(ctx->memory, "OCG: OC Dict %d %s visible\n", ocdict->object_num,
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

    if (index > levels->max_flags - 1) {
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
    if (index > levels->max_flags - 1)
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

int pdfi_op_MP(pdf_context *ctx)
{
    pdf_obj *o = NULL;
    int code = 0;

    if (pdfi_count_stack(ctx) < 1)
        return_error(gs_error_stackunderflow);

    if (!ctx->device_state.writepdfmarks || !ctx->args.preservemarkedcontent) {
        pdfi_pop(ctx, 1);
        goto exit;
    }

    o = ctx->stack_top[-1];
    pdfi_countup(o);
    pdfi_pop(ctx, 1);

    if (pdfi_type_of(o) != PDF_NAME) {
        code = gs_note_error(gs_error_typecheck);
        goto exit;
    }

    code = pdfi_pdfmark_from_objarray(ctx, &o, 1, NULL, "MP");
    ctx->BMClevel ++;

exit:
    pdfi_countdown(o);
    return code;
}

int pdfi_op_DP(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_name *properties = NULL;
    int code = 0;
    pdf_obj **objarray = NULL, *o = NULL;

    if (pdfi_count_stack(ctx) < 2) {
        pdfi_clearstack(ctx);
        return gs_note_error(gs_error_stackunderflow);
    }

    if (!ctx->device_state.writepdfmarks || !ctx->args.preservemarkedcontent) {
        pdfi_pop(ctx, 2); /* pop args */
        goto exit;
    }

    if (pdfi_type_of(ctx->stack_top[-2]) != PDF_NAME) {
        pdfi_pop(ctx, 2); /* pop args */
        code = gs_note_error(gs_error_typecheck);
        goto exit;
    }

    objarray = (pdf_obj **)gs_alloc_bytes(ctx->memory, 2 * sizeof(pdf_obj *), "pdfi_op_DP");
    if (objarray == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto exit;
    }

    objarray[0] = ctx->stack_top[-2];
    pdfi_countup(objarray[0]);
    o = ctx->stack_top[-1];
    pdfi_countup(o);
    pdfi_pop(ctx, 2); /* pop args */

    switch (pdfi_type_of(o)) {
        case PDF_NAME:
            code = pdfi_find_resource(ctx, (unsigned char *)"Properties", (pdf_name *)o, stream_dict, page_dict, (pdf_obj **)&properties);
            if(code < 0)
                goto exit;
            if (pdfi_type_of(properties) != PDF_DICT) {
                code = gs_note_error(gs_error_typecheck);
                goto exit;
            }
            objarray[1] = (pdf_obj *)properties;
            break;
        case PDF_DICT:
            objarray[1] = o;
            break;
        default:
            code = gs_note_error(gs_error_VMerror);
            goto exit;
    }

    code = pdfi_pdfmark_from_objarray(ctx, objarray, 2, NULL, "DP");

 exit:
    if (objarray != NULL) {
        pdfi_countdown(objarray[0]);
        gs_free_object(ctx->memory, objarray, "free pdfi_op_DP");
    }
    pdfi_countdown(o);
    pdfi_countdown(properties);
    return code;
}

/* begin marked content sequence */
int pdfi_op_BMC(pdf_context *ctx)
{
    pdf_obj *o = NULL;
    int code = 0;

    /* This will also prevent us writing out an EMC if the BMC is in any way invalid */
    ctx->BDCWasOC = true;

    if (pdfi_count_stack(ctx) < 1)
        return_error(gs_error_stackunderflow);

    if (!ctx->device_state.writepdfmarks || !ctx->args.preservemarkedcontent) {
        /* Need to increment the BMCLevel anyway, as the EMC will count it down.
         * If we already have a BDC, then that effectively will turn it on.
         */
        ctx->BMClevel ++;
        pdfi_pop(ctx, 1);
        goto exit;
    }

    o = ctx->stack_top[-1];
    pdfi_countup(o);
    pdfi_pop(ctx, 1);

    if (pdfi_type_of(o) != PDF_NAME) {
        code = gs_note_error(gs_error_typecheck);
        goto exit;
    }

    ctx->BDCWasOC = false;
    code = pdfi_pdfmark_from_objarray(ctx, &o, 1, NULL, "BMC");
    ctx->BMClevel ++;

exit:
    pdfi_countdown(o);
    return code;
}

/* begin marked content sequence with property list */
int pdfi_op_BDC(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_name *tag = NULL;
    pdf_name *properties = NULL;
    pdf_dict *oc_dict = NULL;
    int code = 0;
    bool ocg_is_visible;
    pdf_obj **objarray = NULL, *o = NULL;
    pdf_indirect_ref *dictref = NULL;

    /* This will also prevent us writing out an EMC if the BDC is in any way invalid */
    ctx->BDCWasOC = true;

    if (pdfi_count_stack(ctx) < 2) {
        pdfi_clearstack(ctx);
        return gs_note_error(gs_error_stackunderflow);
    }

    ctx->BMClevel ++;

    tag = (pdf_name *)ctx->stack_top[-2];
    pdfi_countup(tag);
    o = ctx->stack_top[-1];
    pdfi_countup(o);
    pdfi_pop(ctx, 2);

    if (pdfi_type_of(tag) != PDF_NAME)
        goto exit;

    if (!pdfi_name_is(tag, "OC"))
        ctx->BDCWasOC = false;

    if (ctx->device_state.writepdfmarks && ctx->args.preservemarkedcontent) {
        objarray = (pdf_obj **)gs_alloc_bytes(ctx->memory, 2 * sizeof(pdf_obj *), "pdfi_op_BDC");
        if (objarray == NULL) {
            code = gs_note_error(gs_error_VMerror);
            goto exit;
        }

        objarray[0] = (pdf_obj *)tag;

        switch (pdfi_type_of(o)) {
            case PDF_NAME:
                code = pdfi_find_resource(ctx, (unsigned char *)"Properties", (pdf_name *)o, stream_dict, page_dict, (pdf_obj **)&oc_dict);
                if(code < 0)
                    goto exit;
                if (pdfi_type_of(oc_dict) != PDF_DICT) {
                    code = gs_note_error(gs_error_typecheck);
                    goto exit;
                }
                code = pdfi_pdfmark_dict(ctx, oc_dict);
                if (code < 0)
                    goto exit;

                /* Create an indirect ref for the dict */
                code = pdfi_object_alloc(ctx, PDF_INDIRECT, 0, (pdf_obj **)&dictref);
                if (code < 0) goto exit;
                pdfi_countup(dictref);
                dictref->ref_object_num = oc_dict->object_num;
                dictref->ref_generation_num = oc_dict->generation_num;
                dictref->is_marking = true;

                objarray[1] = (pdf_obj *)dictref;
                break;
            case PDF_DICT:
                objarray[1] = o;
                break;
            default:
                code = gs_note_error(gs_error_VMerror);
                goto exit;
        }

        code = pdfi_pdfmark_from_objarray(ctx, objarray, 2, NULL, "BDC");
        goto exit;
    }

    if (pdfi_name_is(tag, "OC")) {
        /* Check if first arg is a name and handle it if so */
        /* TODO: spec says it could also be an inline dict that we should be able to handle,
         * but I am just matching what gs does for now, and it doesn't handle that case.
         */
        properties = (pdf_name *)o;
        if (pdfi_type_of(properties) != PDF_NAME)
            goto exit;

        /* If it's a name, look it up in Properties */
        code = pdfi_find_resource(ctx, (unsigned char *)"Properties", properties,
                                  (pdf_dict *)stream_dict, page_dict, (pdf_obj **)&oc_dict);
        if (code != 0)
            goto exit;
        if (pdfi_type_of(oc_dict) != PDF_DICT)
            goto exit;

        ocg_is_visible = pdfi_oc_is_ocg_visible(ctx, oc_dict);
        if (!ocg_is_visible)
            code = pdfi_oc_levels_set(ctx, ctx->OFFlevels, ctx->BMClevel);

    }

exit:
    if (objarray != NULL)
        gs_free_object(ctx->memory, objarray, "free pdfi_op_BDC");
    pdfi_countdown(dictref);
    pdfi_countdown(o);
    pdfi_countdown(tag);
    pdfi_countdown(oc_dict);
    return code;
}

/* end marked content sequence */
int pdfi_op_EMC(pdf_context *ctx)
{
    int code, code1 = 0;

    if (ctx->device_state.writepdfmarks && ctx->args.preservemarkedcontent)
        code1 = pdfi_pdfmark_from_objarray(ctx, NULL, 0, NULL, "EMC");

    code = pdfi_oc_levels_clear(ctx, ctx->OFFlevels, ctx->BMClevel);
    if (code == 0)
        code = code1;

    /* TODO: Should we flag error on too many EMC? */
    if (ctx->BMClevel > 0)
        ctx->BMClevel --;
    return code;
}
