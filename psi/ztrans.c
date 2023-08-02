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


/* Transparency operators */
#include "string_.h"
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gscspace.h"		/* for gscolor2.h */
#include "gscolor2.h"
#include "gsipar3x.h"
#include "gstrans.h"
#include "gxiparam.h"		/* for image enumerator */
#include "gxcspace.h"
#include "idict.h"
#include "idstack.h"
#include "idparam.h"
#include "ifunc.h"
#include "igstate.h"
#include "iimage.h"
#include "iname.h"
#include "store.h"
#include "gspaint.h"		/* gs_erasepage prototype */
#include "gdevdevn.h"
#include "gxdevsop.h"
#include "gxblend.h"
#include "gdevp14.h"
#include "gsicc_cms.h"

/* ------ Utilities ------ */


static int
current_float_value(i_ctx_t *i_ctx_p,
                    float (*current_value)(const gs_gstate *))
{
    os_ptr op = osp;

    push(1);
    make_real(op, current_value(igs));
    return 0;
}

static int
enum_param(const gs_memory_t *mem, const ref *pnref,
           const char *const names[])
{
    const char *const *p;
    ref nsref;

    check_type(*pnref, t_name);

    name_string_ref(mem, pnref, &nsref);
    for (p = names; *p; ++p)
        if (r_size(&nsref) == strlen(*p) &&
            !memcmp(*p, nsref.value.const_bytes, r_size(&nsref))
            )
            return p - names;
    return_error(gs_error_rangecheck);
}

/* ------ Graphics state operators ------ */

static const char *const blend_mode_names[] = {
    GS_BLEND_MODE_NAMES, 0
};

/* <modename> .setblendmode - */
static int
zsetblendmode(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code;

    check_op(1);
    check_type(*op, t_name);
    if ((code = enum_param(imemory, op, blend_mode_names)) < 0 ||
        (code = gs_setblendmode(igs, code)) < 0
        )
        return code;
    pop(1);
    return 0;
}

/* - .currentblendmode <modename> */
static int
zcurrentblendmode(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    const char *mode_name = blend_mode_names[gs_currentblendmode(igs)];
    ref nref;
    int code = name_enter_string(imemory, mode_name, &nref);

    if (code < 0)
        return code;
    push(1);
    *op = nref;
    return 0;
}

/* <bool> .settextknockout - */
static int
zsettextknockout(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_op(1);
    check_type(*op, t_boolean);
    gs_settextknockout(igs, op->value.boolval);
    pop(1);
    return 0;
}

/* - .currenttextknockout <bool> */
static int
zcurrenttextknockout(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_bool(op, gs_currenttextknockout(igs));
    return 0;
}

/* ------ Rendering stack operators ------ */

static int
rect_param(gs_rect *prect, os_ptr op)
{
    double coords[4];
    int code = num_params(op, 4, coords);

    if (code < 0)
        return code;
    prect->p.x = coords[0], prect->p.y = coords[1];
    prect->q.x = coords[2], prect->q.y = coords[3];
    return 0;
}

static int
mask_op(i_ctx_t *i_ctx_p,
        int (*mask_proc)(gs_gstate *, gs_transparency_channel_selector_t))
{
    int csel;
    int code = int_param(osp, 1, &csel);

    if (code < 0)
        return code;
    code = mask_proc(igs, csel);
    if (code >= 0)
        pop(1);
    return code;

}

static int common_transparency_group(i_ctx_t *i_ctx_p, pdf14_compositor_operations group_type)
{
    os_ptr op = osp;
    os_ptr dop = op - 4;
    gs_transparency_group_params_t params;
    gs_rect bbox;
    ref *dummy;
    int code;

    check_op(5);
    check_type(*dop, t_dictionary);
    check_dict_read(*dop);
    gs_trans_group_params_init(&params, 1.0);
    if ((code = dict_bool_param(dop, "Isolated", false, &params.Isolated)) < 0 ||
        (code = dict_bool_param(dop, "Knockout", false, &params.Knockout)) < 0 ||
        (code = dict_bool_param(dop, ".image_with_SMask", false, &params.image_with_SMask)) < 0
        )
        return code;
    code = rect_param(&bbox, op);
    if (code < 0)
        return code;
    /* If the CS is not given in the transparency group dict, set to NULL   */
    /* so that the transparency code knows to inherit from the parent layer */
    if (dict_find_string(dop, "CS", &dummy) <= 0) {
        params.ColorSpace = NULL;
    } else {
        /* the PDF interpreter sets the colorspace, so use it */
        params.ColorSpace = gs_currentcolorspace(igs);
        /* Lets make sure that it is not an ICC color space that came from
           a PS CIE color space or a PS color space. These are 1-way color
           spaces and cannot be used for group color spaces */
        if (gs_color_space_is_PSCIE(params.ColorSpace))
            params.ColorSpace = NULL;
        else if (gs_color_space_is_ICC(params.ColorSpace) &&
            params.ColorSpace->cmm_icc_profile_data != NULL &&
            params.ColorSpace->cmm_icc_profile_data->profile_handle != NULL) {
            if (gscms_is_input(params.ColorSpace->cmm_icc_profile_data->profile_handle,
                params.ColorSpace->cmm_icc_profile_data->memory))
                params.ColorSpace = NULL;
        }
    }

    if (gs_getalphaisshape(igs)) {
        params.group_shape = gs_getfillconstantalpha(igs);
        params.group_opacity = 1.0;
    } else {
        params.group_opacity = gs_getfillconstantalpha(igs);
        params.group_shape = 1.0;
    }

    code = gs_begin_transparency_group(igs, &params, &bbox, group_type);
    if (code < 0)
        return code;
    pop(5);
    return code;
}

/* <paramdict> <llx> <lly> <urx> <ury> .beginpagegroup - */
static int
zbegintransparencypagegroup(i_ctx_t *i_ctx_p)
{
    return common_transparency_group(i_ctx_p, PDF14_BEGIN_TRANS_PAGE_GROUP);
}

/* <paramdict> <llx> <lly> <urx> <ury> .begintransparencygroup - */
static int
zbegintransparencygroup(i_ctx_t *i_ctx_p)
{
    return common_transparency_group(i_ctx_p, PDF14_BEGIN_TRANS_GROUP);
}

/* - .endtransparencygroup - */
static int
zendtransparencygroup(i_ctx_t *i_ctx_p)
{
    return gs_end_transparency_group(igs);
}

/* - .endtransparencytextgroup - */
static int
zendtransparencytextgroup(i_ctx_t *i_ctx_p)
{
    return gs_end_transparency_text_group(igs);
}

/* - .begintransparencytextgroup - */
static int
zbegintransparencytextgroup(i_ctx_t *i_ctx_p)
{
    return gs_begin_transparency_text_group(igs);
}

/* <cs_set?> <paramdict> <llx> <lly> <urx> <ury> .begintransparencymaskgroup -	*/
/*             cs_set == false if we are inheriting the colorspace		*/
static int tf_using_function(double, float *, void *);
static int
zbegintransparencymaskgroup(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    os_ptr dop = op - 4;
    gs_transparency_mask_params_t params;
    ref *pparam;
    gs_rect bbox;
    int code;
    static const char *const subtype_names[] = {
        GS_TRANSPARENCY_MASK_SUBTYPE_NAMES, 0
    };

    check_op(6);
    check_type(*dop, t_dictionary);
    check_dict_read(*dop);
    if (dict_find_string(dop, "Subtype", &pparam) <= 0)
        return_error(gs_error_rangecheck);
    if ((code = enum_param(imemory, pparam, subtype_names)) < 0)
        return code;
    gs_trans_mask_params_init(&params, code);
    params.replacing = true;
    if ((code = dict_floats_param(imemory, dop, "Background",
                    cs_num_components(gs_currentcolorspace(i_ctx_p->pgs)),
                                  params.Background, NULL)) < 0)
        return code;
    else if (code > 0)
        params.Background_components = code;

    if ((code = dict_floats_param(imemory, dop, "GrayBackground",
                    1, &params.GrayBackground, NULL)) < 0)
        return code;
    if (dict_find_string(dop, "TransferFunction", &pparam) > 0) {
        gs_function_t *pfn = ref_function(pparam);

        if (pfn == 0 || pfn->params.m != 1 || pfn->params.n != 1)
            return_error(gs_error_rangecheck);
        params.TransferFunction = tf_using_function;
        params.TransferFunction_data = pfn;
    }
    code = rect_param(&bbox, op);
    if (code < 0)
        return code;
    check_type(op[-5], t_boolean);

    /* Is the colorspace set for this mask ? */
    if (op[-5].value.boolval) {
                params.ColorSpace = gs_currentcolorspace(igs);
                /* Lets make sure that it is not an ICC color space that came from
                a PS CIE color space or a PS color space. These are 1-way color
                spaces and cannot be used for group color spaces */
                if (gs_color_space_is_PSCIE(params.ColorSpace))
                    params.ColorSpace = NULL;
                else if (gs_color_space_is_ICC(params.ColorSpace) &&
                    params.ColorSpace->cmm_icc_profile_data != NULL &&
                    params.ColorSpace->cmm_icc_profile_data->profile_handle != NULL) {
                    if (gscms_is_input(params.ColorSpace->cmm_icc_profile_data->profile_handle,
                        params.ColorSpace->cmm_icc_profile_data->memory))
                        params.ColorSpace = NULL;
                }
    } else {
        params.ColorSpace = NULL;
    }
    code = gs_begin_transparency_mask(igs, &params, &bbox, false);
    if (code < 0)
        return code;
    pop(6);
    return code;
}

/* <paramdict> .begintransparencymaskimage <paramdict> */
static int
zbegintransparencymaskimage(i_ctx_t *i_ctx_p)
{
    os_ptr dop = osp;
    os_ptr op = osp;
    gs_transparency_mask_params_t params;
    gs_rect bbox = { { 0, 0} , { 1, 1} };
    int code;
    gs_color_space *gray_cs = gs_cspace_new_DeviceGray(imemory);

    check_op(1);
    check_type(*dop, t_dictionary);
    check_dict_read(*dop);
    if (!gray_cs)
        return_error(gs_error_VMerror);
    gs_trans_mask_params_init(&params, TRANSPARENCY_MASK_Luminosity);
    if ((code = dict_float_array_check_param(imemory, dop, "Matte",
                                  GS_CLIENT_COLOR_MAX_COMPONENTS,
                                  params.Matte, NULL, 0,
                                  gs_error_rangecheck)) < 0)
        return code;
    else if (code > 0)
        params.Matte_components = code;
    code = gs_begin_transparency_mask(igs, &params, &bbox, true);
    if (code < 0)
        return code;
    rc_decrement_cs(gray_cs, "zbegintransparencymaskimage");
    return code;
}

/* Implement the TransferFunction using a Function. */
static int
tf_using_function(double in_val, float *out, void *proc_data)
{
    float in = in_val;
    gs_function_t *const pfn = proc_data;

    return gs_function_evaluate(pfn, &in, out);
}

/* <mask#> .endtransparencymask - */
static int
zendtransparencymask(i_ctx_t *i_ctx_p)
{
    return mask_op(i_ctx_p, gs_end_transparency_mask);
}

/* ------ Soft-mask images ------ */

/* <dict> .image3x - */
static int mask_dict_param(const gs_memory_t *mem, os_ptr,
                            image_params *, const char *, int,
                            gs_image3x_mask_t *);
static int
zimage3x(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_image3x_t image;
    ref *pDataDict;
    image_params ip_data;
    int num_components =
        gs_color_space_num_components(gs_currentcolorspace(igs));
    int ignored;
    int code;

    check_type(*op, t_dictionary);
    check_dict_read(*op);
    memset(&image, 0, sizeof(gs_image3x_t));
    gs_image3x_t_init(&image, NULL);
    if (dict_find_string(op, "DataDict", &pDataDict) <= 0)
        return_error(gs_error_rangecheck);
    check_type(*pDataDict, t_dictionary);
    if ((code = pixel_image_params(i_ctx_p, pDataDict,
                   (gs_pixel_image_t *)&image, &ip_data,
                   16, gs_currentcolorspace(igs))) < 0 ||
        (code = dict_int_param(pDataDict, "ImageType", 1, 1, 0, &ignored)) < 0
        )
        return code;
    /*
     * We have to process the masks in the reverse order, because they
     * insert their DataSource before the one(s) for the DataDict.
     */
    if ((code = mask_dict_param(imemory, op, &ip_data,
                                "ShapeMaskDict", num_components,
                                &image.Shape)) < 0 ||
        (code = mask_dict_param(imemory, op, &ip_data,
                                "OpacityMaskDict", num_components,
                                &image.Opacity)) < 0
        )
        return code;
    return zimage_setup(i_ctx_p, (gs_pixel_image_t *)&image,
                        &ip_data.DataSource[0],
                        image.CombineWithColor, 1);
}

/* Get one soft-mask dictionary parameter. */
static int
mask_dict_param(const gs_memory_t *mem, os_ptr op,
image_params *pip_data, const char *dict_name,
                int num_components, gs_image3x_mask_t *pixm)
{
    ref *pMaskDict;
    image_params ip_mask;
    int ignored;
    int code, mcode;

    if (dict_find_string(op, dict_name, &pMaskDict) <= 0)
        return 1;
    if (!r_has_type(pMaskDict, t_dictionary))
        return gs_note_error(gs_error_typecheck);

    if ((mcode = code = data_image_params(mem, pMaskDict, &pixm->MaskDict,
                                          &ip_mask, false, 1, 16, false)) < 0 ||
        (code = dict_int_param(pMaskDict, "ImageType", 1, 1, 0, &ignored)) < 0 ||
        (code = dict_int_param(pMaskDict, "InterleaveType", 1, 3, -1,
                               &pixm->InterleaveType)) < 0 ||
        (code = dict_floats_param(mem, op, "Matte", num_components,
                                  pixm->Matte, NULL)) < 0
        )
        return code;
    pixm->has_Matte = code > 0;
    /*
     * The MaskDict must have a DataSource iff InterleaveType == 3.
     */
    if ((pip_data->MultipleDataSources && pixm->InterleaveType != 3) ||
        ip_mask.MultipleDataSources ||
        mcode != (pixm->InterleaveType != 3)
        )
        return_error(gs_error_rangecheck);
    if (pixm->InterleaveType == 3) {
        /* Insert the mask DataSource before the data DataSources. */
        memmove(&pip_data->DataSource[1], &pip_data->DataSource[0],
                (countof(pip_data->DataSource) - 1) *
                sizeof(pip_data->DataSource[0]));
        pip_data->DataSource[0] = ip_mask.DataSource[0];
    }
    return 0;
}

/* depth .pushpdf14devicefilter - */
/* this is a filter operator, but we include it here to maintain
   modularity of the pdf14 transparency support */
static int
zpushpdf14devicefilter(i_ctx_t *i_ctx_p)
{
    int code;
    int depth;
    int spot_color_count = -1;		/* default is 'unknown' spot color count */
    os_ptr op = osp;
    gx_device *cdev = gs_currentdevice_inline(igs);
    dict_stack_t *dstack = &(i_ctx_p->dict_stack);
    ref_stack_t *rdstack = &dstack->stack;
    const ref *puserdict = ref_stack_index(rdstack, ref_stack_count(rdstack) - 1 -
                            dstack->userdict_index);

    check_op(1);
    check_type(*op, t_integer);
    depth = (int)op->value.intval;

    if (dev_proc(cdev, dev_spec_op)(cdev, gxdso_is_pdf14_device, NULL, 0) > 0)
        return 0;		/* ignore push_device if already is pdf14 device */

    /* Bug 698087: In case some program uses our .pushpdf14devicefilter  make	*/
    /*             sure that the device knows that we are using the pdf14	*/
    /*             transparency. Note this will close and re-open the device	*/
    /*             and erase the page. This should not occur with PDF files.	*/
    /* We don't do this if this is a push for the overprint_sim mode		*/
    if (depth >= 0 && cdev->page_uses_transparency == 0) {
        gs_c_param_list list;
        bool bool_true = 1;

        gs_c_param_list_write(&list, imemory);
        code = param_write_bool((gs_param_list *)&list, "PageUsesTransparency", &bool_true);
        if ( code >= 0) {
            gs_c_param_list_read(&list);
            code = gs_gstate_putdeviceparams(igs, cdev, (gs_param_list *)&list);
        }
        gs_c_param_list_release(&list);
        if (code < 0)
            return code;
        if (cdev->is_open) {
            if ((code = gs_closedevice((gx_device *)cdev)) < 0)
                return code;
        }
        if ((code = gs_opendevice((gx_device *)cdev)) < 0)
            return code;
        if ((code = gs_erasepage(igs)) < 0)
            return code;
    }
    /* Get the PageSpotColors value from the userdict, if it is defined */
    code = dict_int_param(puserdict, "PageSpotColors", -1, max_int, -1, &spot_color_count);
    if (code < 0)
        return code;
    /* and finally actually push the compositor device */
    code = gs_push_pdf14trans_device(igs, false, true, depth, spot_color_count);
    if (code < 0)
        return code;
    pop(1);
    return 0;
}

/* this is a filter operator, but we include it here to maintain
   modularity of the pdf14 transparency support */
static int
zpoppdf14devicefilter(i_ctx_t *i_ctx_p)
{
    return gs_pop_pdf14trans_device(igs, false);
}

/* Something has gone terribly wrong */
static int
zabortpdf14devicefilter(i_ctx_t *i_ctx_p)
{
    return gs_abort_pdf14trans_device(igs);
}

/* This is used to communicate to the transparency compositor
   when a q (save extended graphic state) occurs.  Since
   the softmask is part of the graphic state we need to know
   this to handle clist processing properly */

static int
zpushextendedgstate(i_ctx_t *i_ctx_p)
{
    int code;
    code = gs_push_transparency_state(igs);
    return(code);
}

/* This is used to communicate to the transparency compositor
   when a Q (restore extended graphic state) occurs.  Since
   the softmask is part of the graphic state we need to know
   this to handle clist processing properly */

static int
zpopextendedgstate(i_ctx_t *i_ctx_p)
{
    int code;
    code = gs_pop_transparency_state(igs, false);
    return(code);
}

static int
zsetstrokeconstantalpha(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    double value;

    if (real_param(op, &value) < 0)
        return_op_typecheck(op);

    gs_setstrokeconstantalpha(igs, (float)value);
    pop(1);
    return 0;
}

static int
zgetstrokeconstantalpha(i_ctx_t *i_ctx_p)
{
    return current_float_value(i_ctx_p, gs_getstrokeconstantalpha);
}

static int
zsetfillconstantalpha(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    double value;

    if (real_param(op, &value) < 0)
        return_op_typecheck(op);

    gs_setfillconstantalpha(igs, (float)value);
    pop(1);
    return 0;
}

static int
zgetfillconstantalpha(i_ctx_t *i_ctx_p)
{
    return current_float_value(i_ctx_p, gs_getfillconstantalpha);
}

static int
zsetalphaisshape(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_type(*op, t_boolean);
    gs_setalphaisshape(igs, op->value.boolval);
    pop(1);

    return 0;
}

static int
zgetalphaisshape(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_bool(op, gs_getalphaisshape(igs));
    return 0;
}

static int
zsetSMask(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_op(1);

    istate->SMask = *op;
    pop(1);
    return 0;
}

static int
zcurrentSMask(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    *op = istate->SMask;
    return 0;
}

/* ------ Initialization procedure ------ */

/* We need to split the table because of the 16-element limit. */
const op_def ztrans1_op_defs[] = {
    {"1.setblendmode", zsetblendmode},
    {"0.currentblendmode", zcurrentblendmode},
    {"1.settextknockout", zsettextknockout},
    {"0.currenttextknockout", zcurrenttextknockout},
    {"0.pushextendedgstate", zpushextendedgstate},
    {"0.popextendedgstate", zpopextendedgstate},
    op_def_end(0)
};
const op_def ztrans2_op_defs[] = {
    {"5.begintransparencygroup", zbegintransparencygroup},
    {"5.begintransparencypagegroup", zbegintransparencypagegroup},
    {"0.endtransparencygroup", zendtransparencygroup},
    { "0.endtransparencytextgroup", zendtransparencytextgroup },
    { "0.begintransparencytextgroup", zbegintransparencytextgroup },
    {"5.begintransparencymaskgroup", zbegintransparencymaskgroup},
    {"1.begintransparencymaskimage", zbegintransparencymaskimage},
    {"1.endtransparencymask", zendtransparencymask},
    {"1.image3x", zimage3x},
    {"1.pushpdf14devicefilter", zpushpdf14devicefilter},
    {"0.poppdf14devicefilter", zpoppdf14devicefilter},
    {"0.abortpdf14devicefilter", zabortpdf14devicefilter},
    op_def_end(0)
};

const op_def ztrans3_op_defs[] = {
    {"1.setstrokeconstantalpha", zsetstrokeconstantalpha},
    {"0.currentstrokeconstantalpha", zgetstrokeconstantalpha},
    {"1.setfillconstantalpha", zsetfillconstantalpha},
    {"0.currentfillconstantalpha", zgetfillconstantalpha},
    {"1.setalphaisshape", zsetalphaisshape},
    {"0.currentalphaisshape", zgetalphaisshape},
    {"1.setSMask", zsetSMask},
    {"0.currentSMask", zcurrentSMask},
    op_def_end(0)
};
