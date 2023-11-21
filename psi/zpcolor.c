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


/* Pattern color */
#include "ghost.h"
#include "oper.h"
#include "gscolor.h"
#include "gsmatrix.h"
#include "gsstruct.h"
#include "gscoord.h"
#include "gxcspace.h"
#include "gxfixed.h"		/* for gxcolor2.h */
#include "gxcolor2.h"
#include "gxdcolor.h"		/* for gxpcolor.h */
#include "gxdevice.h"
#include "gxdevmem.h"		/* for gxpcolor.h */
#include "gxpcolor.h"
#include "gxpath.h"
#include "estack.h"
#include "ialloc.h"
#include "icremap.h"
#include "istruct.h"
#include "idict.h"
#include "idparam.h"
#include "igstate.h"
#include "ipcolor.h"
#include "store.h"
#include "gzstate.h"
#include "memory_.h"
#include "gdevp14.h"
#include "gxdevsop.h"

/* Imported from gspcolor.c */
extern const gs_color_space_type gs_color_space_type_Pattern;

/* Forward references */
static int zPaintProc(const gs_client_color *, gs_gstate *);
static int pattern_paint_prepare(i_ctx_t *);
static int pattern_paint_finish(i_ctx_t *);

/* GC descriptors */
private_st_int_pattern();

/* Initialize the Pattern cache. */
static int
zpcolor_init(i_ctx_t *i_ctx_p)
{
    gx_pattern_cache *pc = gx_pattern_alloc_cache(imemory_system,
                                                  gx_pat_cache_default_tiles(),
                                                  gx_pat_cache_default_bits());
    if (pc == NULL)
	return_error(gs_error_VMerror);
    gstate_set_pattern_cache(igs, pc);
    return 0;
}

/* Create an interpreter pattern structure. */
int
int_pattern_alloc(int_pattern **ppdata, const ref *op, gs_memory_t *mem)
{
    int_pattern *pdata =
        gs_alloc_struct(mem, int_pattern, &st_int_pattern, "int_pattern");

    if (pdata == 0)
        return_error(gs_error_VMerror);
    pdata->dict = *op;
    *ppdata = pdata;
    return 0;
}

/* <pattern> <matrix> .buildpattern1 <pattern> <instance> */
static int
zbuildpattern1(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    os_ptr op1 = op - 1;
    int code;
    gs_matrix mat;
    float BBox[4];
    gs_client_pattern templat;
    int_pattern *pdata;
    gs_client_color cc_instance;
    ref *pPaintProc;

    check_op(2);
    code = read_matrix(imemory, op, &mat);
    if (code < 0)
        return code;
    check_type(*op1, t_dictionary);
    check_dict_read(*op1);
    gs_pattern1_init(&templat);

    code = dict_uid_param(op1, &templat.uid, 1, imemory, i_ctx_p);
    if (code < 0)
        return code;
    if (code != 1)
        return_error(gs_error_rangecheck);

    code = dict_int_param(op1, "PaintType", 1, 2, 0, &templat.PaintType);
    if (code < 0)
        return code;

    code = dict_int_param(op1, "TilingType", 1, 3, 0, &templat.TilingType);
    if (code < 0)
        return code;

    code = dict_bool_param(op1, ".pattern_uses_transparency", 0, &templat.uses_transparency);
    if (code < 0)
        return code;

    code = dict_floats_param(imemory, op1, "BBox", 4, BBox, NULL);
    if (code < 0)
        return code;
    if (code == 0)
       return_error(gs_error_undefined);

    code = dict_float_param(op1, "XStep", 0.0, &templat.XStep);
    if (code < 0)
        return code;
    if (code == 1)
       return_error(gs_error_undefined);

    code = dict_float_param(op1, "YStep", 0.0, &templat.YStep);
    if (code < 0)
        return code;
    if (code == 1)
       return_error(gs_error_undefined);

    code = dict_find_string(op1, "PaintProc", &pPaintProc);
    if (code < 0)
        return code;
    if (code == 0)
       return_error(gs_error_undefined);

    check_proc(*pPaintProc);

    if (mat.xx * mat.yy == mat.xy * mat.yx)
        return_error(gs_error_undefinedresult);
    if (BBox[0] >= BBox[2] ||  BBox[1] >= BBox[3])
        return_error(gs_error_rangecheck);

    templat.BBox.p.x = BBox[0];
    templat.BBox.p.y = BBox[1];
    templat.BBox.q.x = BBox[2];
    templat.BBox.q.y = BBox[3];
    templat.PaintProc = zPaintProc;
    code = int_pattern_alloc(&pdata, op1, imemory);
    if (code < 0)
        return code;
    code = gs_makepattern(&cc_instance, &templat, &mat, igs, imemory);
    if (code < 0) {
        ifree_object(pdata, "int_pattern");
        return code;
    }
    cc_instance.pattern->client_data = pdata;
    make_istruct(op, a_readonly, cc_instance.pattern);
    return code;
}

/* ------ Initialization procedure ------ */

const op_def zpcolor_l2_op_defs[] =
{
    op_def_begin_level2(),
    {"2.buildpattern1", zbuildpattern1},
    {"0%pattern_paint_prepare", pattern_paint_prepare},
    {"0%pattern_paint_finish", pattern_paint_finish},
    op_def_end(zpcolor_init)
};

/* ------ Internal procedures ------ */

/* Render the pattern by calling the PaintProc. */
static int pattern_paint_cleanup(i_ctx_t *);
static int pattern_paint_cleanup_core(i_ctx_t *, bool);
static int
zPaintProc(const gs_client_color * pcc, gs_gstate * pgs)
{
    /* Just schedule a call on the real PaintProc. */
    r_ptr(&gs_int_gstate(pgs)->remap_color_info,
          int_remap_color_info_t)->proc =
        pattern_paint_prepare;
    return_error(gs_error_Remap_Color);
}
/* Prepare to run the PaintProc. */
static int
pattern_paint_prepare(i_ctx_t *i_ctx_p)
{
    gs_gstate *pgs = igs;
    gs_pattern1_instance_t *pinst =
        (gs_pattern1_instance_t *)gs_currentcolor(pgs)->pattern;
    ref *pdict = &((int_pattern *) pinst->client_data)->dict;
    gx_device_forward *pdev = NULL;
    gx_device *cdev = gs_currentdevice_inline(igs), *new_dev = NULL;
    int code;
    ref *ppp;
    bool internal_accum = true;

    check_estack(8);
    if (pgs->have_pattern_streams) {
        code = dev_proc(cdev, dev_spec_op)(cdev, gxdso_pattern_can_accum,
                                pinst, pinst->id);
        if (code < 0)
            return code;
        internal_accum = (code == 0);
    }
    if (internal_accum) {
        gs_memory_t *storage_memory = gstate_pattern_cache(pgs)->memory;

        pdev = gx_pattern_accum_alloc(imemory, storage_memory, pinst, "pattern_paint_prepare");
        if (pdev == 0)
            return_error(gs_error_VMerror);
        code = (*dev_proc(pdev, open_device)) ((gx_device *) pdev);
        if (code < 0) {
            ifree_object(pdev, "pattern_paint_prepare");
            return code;
        }
    } else {
        code = gx_pattern_cache_add_dummy_entry(igs, pinst, cdev->color_info.depth);
        if (code < 0)
            return code;
    }
    code = gs_gsave(pgs);
    if (code < 0)
        return code;
    code = gs_setgstate(pgs, pinst->saved);
    if (code < 0) {
        gs_grestore(pgs);
        return code;
    }
    /* gx_set_device_only(pgs, (gx_device *) pdev); */
    if (internal_accum) {
        gs_setdevice_no_init(pgs, (gx_device *)pdev);
        if (pinst->templat.uses_transparency) {
            if_debug0m('v', imemory, "   pushing the pdf14 compositor device into this graphics state\n");
            if ((code = gs_push_pdf14trans_device(pgs, true, true, 0, 0)) < 0)  /* spot color count found from pdf14 target */
                return code;
        } else { /* not transparent */
            if (pinst->templat.PaintType == 1 && !(pinst->is_clist)
                && dev_proc(pinst->saved->device, dev_spec_op)(pinst->saved->device, gxdso_pattern_can_accum, NULL, 0) == 0)
                if ((code = gx_erase_colored_pattern(pgs)) < 0)
                    return code;
        }
    } else {
        gs_matrix m;
        gs_rect bbox;
        gs_fixed_rect clip_box;

        dev_proc(pgs->device, get_initial_matrix)(pgs->device, &m);
        gs_setmatrix(igs, &m);
        code = gs_bbox_transform(&pinst->templat.BBox, &ctm_only(pgs), &bbox);
        if (code < 0) {
            gs_grestore(pgs);
            return code;
        }
        clip_box.p.x = float2fixed(bbox.p.x);
        clip_box.p.y = float2fixed(bbox.p.y);
        clip_box.q.x = float2fixed(bbox.q.x);
        clip_box.q.y = float2fixed(bbox.q.y);
        code = gx_clip_to_rectangle(igs, &clip_box);
        if (code < 0) {
            gs_grestore(pgs);
            return code;
        }

        {
            pattern_accum_param_s param;
            param.pinst = (void *)pinst;
            param.interpreter_memory = imemory;
            param.graphics_state = (void *)pgs;
            param.pinst_id = pinst->id;

            code = (*dev_proc(pgs->device, dev_spec_op))((gx_device *)pgs->device,
                gxdso_pattern_start_accum, &param, sizeof(pattern_accum_param_s));
        }
        /* Previously the code here and in pattern_pain_cleanup() assumed that if the current
         * device could handle patterns it would do so itself, thus if we get to the cleanup
         * and the device stored on the exec stack (pdev) was NULL, we assumed that we could
         * simly tell the current device to finish the pattern. However, if the device handles
         * patterns by installing a new device, then that won't work. So here, after the
         * device has processed the pattern, we find the 'current' device. If the device
         * handles patterns itself then there is effectively no change, that device will be
         * informed by pattern_pain_cleanup() that the pattern has finished. However, if
         * the device has changed, then we assume here that the new current device is the
         * one that handles the pattern, and threfore the onw to inform when the pattern
         * terminates. We store the pattern o the exec stack, exactly like the pattern
         * instance etc.
         */
        new_dev = gs_currentdevice_inline(igs);

        if (code < 0) {
            gs_grestore(pgs);
            return code;
        }
    }
    push_mark_estack(es_other, pattern_paint_cleanup);
    ++esp;
    make_istruct(esp, 0, new_dev); /* see comment in pattern_paint_cleanup() */
    ++esp;
    make_istruct(esp, 0, pinst); /* see comment in pattern_paint_cleanup() */
    ++esp;
    make_istruct(esp, 0, pdev);
    ++esp;
    make_int(esp, imemory_save_level(iimemory));
    ++esp;
    make_int(esp, imemory_space(iimemory));
    ++esp;
    /* Save operator stack depth in case PaintProc leaves junk on ostack. */
    make_int(esp, ref_stack_count(&o_stack));
    push_op_estack(pattern_paint_finish);
    dict_find_string(pdict, "PaintProc", &ppp);		/* can't fail */
    *++esp = *ppp;
    *++esp = *pdict;		/* (push on ostack) */
    return o_push_estack;
}
/* Save the rendered pattern. */
static int
pattern_paint_finish(i_ctx_t *i_ctx_p)
{
    int o_stack_adjust = ref_stack_count(&o_stack) - esp->value.intval;
    gx_device_forward *pdev = r_ptr(esp - 3, gx_device_forward);
    gs_pattern1_instance_t *pinst =
        (gs_pattern1_instance_t *)gs_currentcolor(igs->saved)->pattern;
    gx_device_pattern_accum const *padev = (const gx_device_pattern_accum *) pdev;
    gs_pattern1_instance_t *pinst2 = r_ptr(esp - 4, gs_pattern1_instance_t);
    int orig_save_level = (esp - 2)->value.intval;
    uint vmspace = (uint)(esp - 1)->value.intval;

    if (vmspace !=  imemory_space(iimemory) || orig_save_level != imemory_save_level(iimemory_local)) {
        return_error(gs_error_undefinedresult);
    }

    if (pdev != NULL) {
        gx_color_tile *ctile;
        int code;
        gs_gstate *pgs = igs;
        /* If the PaintProc does one or more gsaves, then fails to do an equal numer of
         * grestores, we can get here with the graphics state stack not how we expect.
         * Hence we stored a reference to the pattern instance on the exec stack, and that
         * allows us to roll back the graphics states until we have the one we expect,
         * and pattern instance we expect
         */
        if (pinst != pinst2) {
            int i;
            for (i = 0; pgs->saved && pinst != pinst2; i++, pgs = pgs->saved) {
                pinst = (gs_pattern1_instance_t *)gs_currentcolor(pgs->saved)->pattern;
            }
            for (;i > 1; i--) {
                gs_grestore(igs);
            }
            pinst = (gs_pattern1_instance_t *)gs_currentcolor(igs->saved)->pattern;
            /* If pinst is NULL after all of that then we are not going to recover */
            if (pinst == NULL) {
                esp -= 5;
                return_error(gs_error_unknownerror);
            }
        }
        pgs = igs;

        if (pinst->templat.uses_transparency) {
            if (pinst->is_clist) {
                /* Send the compositor command to close the PDF14 device */
                code = gs_pop_pdf14trans_device(pgs, true);
                if (code < 0) {
                    esp -= 5;
                    return code;
                }
            } else {
                /* Not a clist, get PDF14 buffer information */
                code = pdf14_get_buffer_information(pgs->device,
                                                    padev->transbuff, pgs->memory,
                                                    true);
                /* PDF14 device (and buffer) is destroyed when pattern cache
                   entry is removed */
                if (code < 0) {
                    esp -= 5;
                    return code;
                }
            }
        }
        code = gx_pattern_cache_add_entry(igs, pdev, &ctile);
        if (code < 0)
            return code;
    }
    if (o_stack_adjust > 0) {
#if 0
        dmlprintf1(imemory, "PaintProc left %d extra on operator stack!\n", o_stack_adjust);
#endif
        /* Take care here: if anything is added after this that may access the op stack,
           this needs to be ref_stack_pop() rather than pop().
         */
        pop(o_stack_adjust);
    }
    esp -= 7;
    pattern_paint_cleanup_core(i_ctx_p, 0);
    return o_pop_estack;
}
/* Clean up after rendering a pattern.  Note that iff the rendering */
/* succeeded, closing the accumulator won't free the bits. */
static int
pattern_paint_cleanup(i_ctx_t *i_ctx_p)
{
    return pattern_paint_cleanup_core(i_ctx_p, 1);
}

static int
pattern_paint_cleanup_core(i_ctx_t *i_ctx_p, bool is_error)
{
    gx_device_pattern_accum *const pdev =
        r_ptr(esp + 4, gx_device_pattern_accum);
        gs_pattern1_instance_t *pinst = (gs_pattern1_instance_t *)gs_currentcolor(igs->saved)->pattern;
    gs_pattern1_instance_t *pinst2 = r_ptr(esp + 3, gs_pattern1_instance_t);
    int code, i, ecode=0;
    int orig_save_level = (esp + 5)->value.intval;
    uint vmspace = (uint)(esp + 6)->value.intval;

    if (vmspace !=  imemory_space(iimemory) || orig_save_level != imemory_save_level(iimemory_local)) {
        return_error(gs_error_undefinedresult);
    }
    /* If the PaintProc does one or more gsaves, then encounters an error, we can get
     * here with the graphics state stack not how we expect.
     * Hence we stored a reference to the pattern instance on the exec stack, and that
     * allows us to roll back the graphics states until we have the one we expect,
     * and pattern instance we expect
     */
    if (pinst != pinst2) {
        gs_gstate *pgs = igs;
        for (i = 0; pgs->saved && pinst != pinst2; i++, pgs = pgs->saved) {
            pinst = (gs_pattern1_instance_t *)gs_currentcolor(pgs->saved)->pattern;
        }
        for (;i > 1; i--) {
            gs_grestore(igs);
        }
        pinst = (gs_pattern1_instance_t *)gs_currentcolor(igs->saved)->pattern;
    }

    if (pdev != NULL) {
        /* grestore will free the device, so close it first. */
        (*dev_proc(pdev, close_device)) ((gx_device *) pdev);
        /* The accumulator device is allocated in "system" memory
           but the target device may be allocated in the prevailing
           VM mode (local/global) of the input file, and thus potentially
           subject to save/restore - which may cause bad things if the
           accumator hangs around until the end of job restore, which can
           happen in the even of an error during the PaintProc, so
           null that pointer for that case.
         */
        if (is_error)
            pdev->target = NULL;
    }
    if (pdev == NULL) {
        gx_device *cdev = r_ptr(esp + 2, gx_device);
        pattern_accum_param_s param;

        param.pinst = (void *)pinst;
        param.graphics_state = (void *)igs;
        param.pinst_id = pinst->id;

        ecode = dev_proc(cdev, dev_spec_op)(cdev,
                        gxdso_pattern_finish_accum, &param, sizeof(pattern_accum_param_s));
    }
    code = gs_grestore(igs);
    gx_unset_dev_color(igs);	/* dev_color may need updating if GC ran */

    if (ecode < 0)
        return ecode;
    return code;
}
