/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* Device-related operators */
#include "string_.h"
#include "ghost.h"
#include "oper.h"
#include "ialloc.h"
#include "idict.h"
#include "igstate.h"
#include "imain.h"
#include "imemory.h"
#include "iname.h"
#include "interp.h"
#include "iparam.h"
#include "ivmspace.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gxdevice.h"
#include "gxalloc.h"
#include "gxgetbit.h"
#include "store.h"
#include "gsicc_manage.h"
#include "gxdevsop.h"

/* <device> <keep_open> .copydevice2 <newdevice> */
static int
zcopydevice2(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gx_device *new_dev;
    int code;

    check_read_type(op[-1], t_device);
    check_type(*op, t_boolean);
    if (op[-1].value.pdevice == NULL)
        /* This can happen if we invalidated devices on the stack by calling nulldevice after they were pushed */
        return_error(gs_error_undefined);

    code = gs_copydevice2(&new_dev, op[-1].value.pdevice, op->value.boolval,
                          imemory);
    if (code < 0)
        return code;
    new_dev->memory = imemory;
    make_tav(op - 1, t_device, icurrent_space | a_all, pdevice, new_dev);
    pop(1);
    return 0;
}

/* - currentdevice <device> */
/* Returns the current device in the graphics state */
int
zcurrentdevice(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gx_device *dev = gs_currentdevice(igs);
    gs_ref_memory_t *mem = (gs_ref_memory_t *) dev->memory;

    push(1);
    make_tav(op, t_device,
             (mem == 0 ? avm_foreign : imemory_space(mem)) | a_all,
             pdevice, dev);
    return 0;
}

/* - .currentoutputdevice <device> */
/* Returns the *output* device - which will often
   be the same as above, but not always: if a compositor
   or other forwarding device, or subclassing device is
   in force, that will be referenced by the graphics state
   rather than the output device.
   This is equivalent of currentdevice device, but returns
   the *device* object, rather than the dictionary describing
   the device and device state.
 */
int
zcurrentoutputdevice(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gx_device *odev = NULL, *dev = gs_currentdevice(igs);
    gs_ref_memory_t *mem = (gs_ref_memory_t *) dev->memory;
    int code = dev_proc(dev, dev_spec_op)(dev,
                        gxdso_current_output_device, (void *)&odev, 0);
    if (code < 0)
        return code;

    push(1);
    make_tav(op, t_device,
             (mem == 0 ? avm_foreign : imemory_space(mem)) | a_all,
             pdevice, odev);
    return 0;
}

/* <device> .devicename <string> */
static int
zdevicename(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    const char *dname;

    check_read_type(*op, t_device);
    if (op->value.pdevice == NULL)
        /* This can happen if we invalidated devices on the stack by calling nulldevice after they were pushed */
        return_error(gs_error_undefined);

    dname = op->value.pdevice->dname;
    make_const_string(op, avm_foreign | a_readonly, strlen(dname),
                      (const byte *)dname);
    return 0;
}

/* - .doneshowpage - */
static int
zdoneshowpage(i_ctx_t *i_ctx_p)
{
    gx_device *dev = gs_currentdevice(igs);
    gx_device *tdev = (*dev_proc(dev, get_page_device)) (dev);

    if (tdev != 0)
        tdev->ShowpageCount++;
    return 0;
}

/* - flushpage - */
int
zflushpage(i_ctx_t *i_ctx_p)
{
    return gs_flushpage(igs);
}

/* <device> <x> <y> <width> <max_height> <alpha?> <std_depth|null> <string> */
/*   .getbitsrect <height> <substring> */
static int
zgetbitsrect(i_ctx_t *i_ctx_p)
{	/*
         * alpha? is 0 for no alpha, -1 for alpha first, 1 for alpha last.
         * std_depth is null for native pixels, depth/component for
         * standard color space.
         */
    os_ptr op = osp;
    gx_device *dev;
    gs_int_rect rect;
    gs_get_bits_params_t params;
    int w, h;
    gs_get_bits_options_t options =
        GB_ALIGN_ANY | GB_RETURN_COPY | GB_OFFSET_0 | GB_RASTER_STANDARD |
        GB_PACKING_CHUNKY;
    int depth;
    uint raster;
    int num_rows;
    int code;

    check_read_type(op[-7], t_device);
    dev = op[-7].value.pdevice;
    if (dev == NULL)
        /* This can happen if we invalidated devices on the stack by calling nulldevice after they were pushed */
        return_error(gs_error_undefined);

    check_int_leu(op[-6], dev->width);
    rect.p.x = op[-6].value.intval;
    check_int_leu(op[-5], dev->height);
    rect.p.y = op[-5].value.intval;
    check_int_leu(op[-4], dev->width);
    w = op[-4].value.intval;
    check_int_leu(op[-3], dev->height);
    h = op[-3].value.intval;
    check_type(op[-2], t_integer);
    /*
     * We use if/else rather than switch because the value is long,
     * which is not supported as a switch value in pre-ANSI C.
     */
    if (op[-2].value.intval == -1)
        options |= GB_ALPHA_FIRST;
    else if (op[-2].value.intval == 0)
        options |= GB_ALPHA_NONE;
    else if (op[-2].value.intval == 1)
        options |= GB_ALPHA_LAST;
    else
        return_error(gs_error_rangecheck);
    if (r_has_type(op - 1, t_null)) {
        options |= GB_COLORS_NATIVE;
        depth = dev->color_info.depth;
    } else {
        static const gs_get_bits_options_t depths[17] = {
            0, GB_DEPTH_1, GB_DEPTH_2, 0, GB_DEPTH_4, 0, 0, 0, GB_DEPTH_8,
            0, 0, 0, GB_DEPTH_12, 0, 0, 0, GB_DEPTH_16
        };
        gs_get_bits_options_t depth_option;
        int std_depth;

        check_int_leu(op[-1], 16);
        std_depth = (int)op[-1].value.intval;
        depth_option = depths[std_depth];
        if (depth_option == 0)
            return_error(gs_error_rangecheck);
        options |= depth_option | GB_COLORS_NATIVE;
        depth = (dev->color_info.num_components +
                 (options & GB_ALPHA_NONE ? 0 : 1)) * std_depth;
    }
    if (w == 0)
        return_error(gs_error_rangecheck);
    raster = (w * depth + 7) >> 3;
    check_write_type(*op, t_string);
    num_rows = r_size(op) / raster;
    h = min(h, num_rows);
    if (h == 0)
        return_error(gs_error_rangecheck);
    rect.q.x = rect.p.x + w;
    rect.q.y = rect.p.y + h;
    params.options = options;
    params.data[0] = op->value.bytes;
    code = (*dev_proc(dev, get_bits_rectangle))(dev, &rect, &params, NULL);
    if (code < 0)
        return code;
    make_int(op - 7, h);
    op[-6] = *op;
    r_set_size(op - 6, h * raster);
    pop(6);
    return 0;
}

/* <int> .getdevice <device> */
static int
zgetdevice(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    const gx_device *dev;

    check_type(*op, t_integer);
    if (op->value.intval != (int)(op->value.intval))
        return_error(gs_error_rangecheck);	/* won't fit in an int */
    dev = gs_getdevice((int)(op->value.intval));
    if (dev == 0)		/* index out of range */
        return_error(gs_error_rangecheck);
    /* Device prototypes are read-only; */
    /* the cast is logically unnecessary. */
    make_tav(op, t_device, avm_foreign | a_readonly, pdevice,
             (gx_device *) dev);
    return 0;
}

/* - .getdefaultdevice <device> */
static int
zgetdefaultdevice(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    const gx_device *dev;

    dev = gs_getdefaultlibdevice(imemory);
    if (dev == 0) /* couldn't find a default device */
        return_error(gs_error_unknownerror);
    push(1);
    make_tav(op, t_device, avm_foreign | a_readonly, pdevice,
                (gx_device *) dev);
    return 0;
}

/* Common functionality of zgethardwareparms & zgetdeviceparams */
static int
zget_device_params(i_ctx_t *i_ctx_p, bool is_hardware)
{
    os_ptr op = osp;
    ref rkeys;
    gx_device *dev;
    stack_param_list list;
    int code;
    ref *pmark;

    check_read_type(op[-1], t_device);

    if(!r_has_type(op, t_null)) {
        check_type(*op, t_dictionary);
    }
    rkeys = *op;
    dev = op[-1].value.pdevice;
    if (op[-1].value.pdevice == NULL)
        /* This can happen if we invalidated devices on the stack by calling nulldevice after they were pushed */
        return_error(gs_error_undefined);
    pop(1);
    stack_param_list_write(&list, &o_stack, &rkeys, iimemory);
    code = gs_get_device_or_hardware_params(dev, (gs_param_list *) & list,
                                            is_hardware);
    if (code < 0) {
        /* We have to put back the top argument. */
        if (list.count > 0)
            ref_stack_pop(&o_stack, list.count * 2 - 1);
        else {
            code = ref_stack_push(&o_stack, 1);
            if (code < 0)
                return code;
        }
        *osp = rkeys;
        return code;
    }
    pmark = ref_stack_index(&o_stack, list.count * 2);
    make_mark(pmark);
    return 0;
}
/* <device> <key_dict|null> .getdeviceparams <mark> <name> <value> ... */
static int
zgetdeviceparams(i_ctx_t *i_ctx_p)
{
    return zget_device_params(i_ctx_p, false);
}
/* <device> <key_dict|null> .gethardwareparams <mark> <name> <value> ... */
static int
zgethardwareparams(i_ctx_t *i_ctx_p)
{
    return zget_device_params(i_ctx_p, true);
}

/* <matrix> <width> <height> <palette> <word?> makewordimagedevice <device> */
static int
zmakewordimagedevice(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    os_ptr op1 = op - 1;
    gs_matrix imat;
    gx_device *new_dev;
    const byte *colors;
    int colors_size;
    int code;

    check_int_leu(op[-3], max_uint >> 1);	/* width */
    check_int_leu(op[-2], max_uint >> 1);	/* height */
    check_type(*op, t_boolean);
    if (r_has_type(op1, t_null)) {	/* true color */
        colors = 0;
        colors_size = -24;	/* 24-bit true color */
    } else if (r_has_type(op1, t_integer)) {
        /*
         * We use if/else rather than switch because the value is long,
         * which is not supported as a switch value in pre-ANSI C.
         */
        if (op1->value.intval != 16 && op1->value.intval != 24 &&
            op1->value.intval != 32
            )
            return_error(gs_error_rangecheck);
        colors = 0;
        colors_size = -op1->value.intval;
    } else {
        check_type(*op1, t_string);	/* palette */
        if (r_size(op1) > 3 * 256)
            return_error(gs_error_rangecheck);
        colors = op1->value.bytes;
        colors_size = r_size(op1);
    }
    if ((code = read_matrix(imemory, op - 4, &imat)) < 0)
        return code;
    /* Everything OK, create device */
    code = gs_makewordimagedevice(&new_dev, &imat,
                                  (int)op[-3].value.intval,
                                  (int)op[-2].value.intval,
                                  colors, colors_size,
                                  op->value.boolval, true, imemory);
    if (code == 0) {
        new_dev->memory = imemory;
        make_tav(op - 4, t_device, imemory_space(iimemory) | a_all,
                 pdevice, new_dev);
        pop(4);
    }
    return code;
}

static void invalidate_stack_devices(i_ctx_t *i_ctx_p)
{
    os_ptr op = osbot;
    while (op != ostop) {
        if (r_has_type(op, t_device))
            op->value.pdevice = 0;
        op++;
    }
}

/* - nulldevice - */
/* Note that nulldevice clears the current pagedevice. */
static int
znulldevice(i_ctx_t *i_ctx_p)
{
    int code = gs_nulldevice(igs);
    invalidate_stack_devices(i_ctx_p);
    clear_pagedevice(istate);
    return code;
}

extern void print_resource_usage(const gs_main_instance *, gs_dual_memory_t *,
                     const char *);

/* <num_copies> <flush_bool> .outputpage - */
static int
zoutputpage(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code;

    check_type(op[-1], t_integer);
    check_type(*op, t_boolean);
    if (gs_debug[':']) {
        gs_main_instance *minst = get_minst_from_memory((gs_memory_t *)i_ctx_p->memory.current->non_gc_memory);

        print_resource_usage(minst, &(i_ctx_p->memory), "Outputpage start");
    }
    code = gs_output_page(igs, (int)op[-1].value.intval,
                          op->value.boolval);
    if (code < 0)
        return code;
    pop(2);
    if (gs_debug[':']) {
        gs_main_instance *minst = get_minst_from_memory((gs_memory_t *)i_ctx_p->memory.current->non_gc_memory);

        print_resource_usage(minst, &(i_ctx_p->memory), "Outputpage end");
    }
    return 0;
}

/* <device> <policy_dict|null> <require_all> <mark> <name> <value> ... */
/*      .putdeviceparams */
/*   (on success) <device> <eraseflag> */
/*   (on failure) <device> <policy_dict|null> <require_all> <mark> */
/*       <name1> <error1> ... */
/* For a key that simply was not recognized, if require_all is true, */
/* the result will be an /undefined error; if require_all is false, */
/* the key will be ignored. */
/* Note that .putdeviceparams clears the current pagedevice. */
static int
zputdeviceparams(i_ctx_t *i_ctx_p)
{
    uint count = ref_stack_counttomark(&o_stack);
    ref *prequire_all;
    ref *ppolicy;
    ref *pdev;
    gx_device *dev;
    stack_param_list list;
    int code;
    int old_width, old_height;
    int i, dest;

    if (count == 0)
        return_error(gs_error_unmatchedmark);
    prequire_all = ref_stack_index(&o_stack, count);
    ppolicy = ref_stack_index(&o_stack, count + 1);
    pdev = ref_stack_index(&o_stack, count + 2);
    if (pdev == 0)
        return_error(gs_error_stackunderflow);
    check_type_only(*prequire_all, t_boolean);
    check_write_type_only(*pdev, t_device);
    dev = pdev->value.pdevice;
    if (dev == NULL)
        /* This can happen if we invalidated devices on the stack by calling nulldevice after they were pushed */
        return_error(gs_error_undefined);
    code = stack_param_list_read(&list, &o_stack, 0, ppolicy,
                                 prequire_all->value.boolval, iimemory);
    if (code < 0)
        return code;
    old_width = dev->width;
    old_height = dev->height;
    code = gs_putdeviceparams(dev, (gs_param_list *) & list);
    /* Check for names that were undefined or caused errors. */
    for (dest = count - 2, i = 0; i < count >> 1; i++)
        if (list.results[i] < 0) {
            *ref_stack_index(&o_stack, dest) =
                *ref_stack_index(&o_stack, count - (i << 1) - 2);
            gs_errorname(i_ctx_p, list.results[i],
                         ref_stack_index(&o_stack, dest - 1));
            dest -= 2;
        }
    iparam_list_release(&list);
    if (code < 0) {		/* There were errors reported. */
        ref_stack_pop(&o_stack, dest + 1);
        return (code == gs_error_Fatal) ? code : 0;	/* cannot continue from Fatal */
    }
    if (code > 0 || (code == 0 && (dev->width != old_width || dev->height != old_height))) {
        /*
         * The device was open and is now closed, or its dimensions have
         * changed.  If it was the current device, call setdevice to
         * reinstall it and erase the page.
         */
        /****** DOESN'T FIND ALL THE GSTATES THAT REFERENCE THE DEVICE. ******/
        if (gs_currentdevice(igs) == dev) {
            bool was_open = dev->is_open;

            code = gs_setdevice_no_erase(igs, dev);
            /* If the device wasn't closed, setdevice won't erase the page. */
            if (was_open && code >= 0)
                code = 1;
        }
    }
    if (code < 0)
        return code;
    ref_stack_pop(&o_stack, count + 1);
    make_bool(osp, code);
    clear_pagedevice(istate);
    return 0;
}

int
zsetdevice_no_safer(i_ctx_t *i_ctx_p, gx_device *new_dev)
{
    gx_device *dev = gs_currentdevice(igs);
    int code;

    dev->ShowpageCount = 0;

    if (new_dev == NULL)
        return gs_note_error(gs_error_undefined);

    code = gs_setdevice_no_erase(igs, new_dev);
    if (code < 0)
        return code;

    invalidate_stack_devices(i_ctx_p);
    clear_pagedevice(istate);
    return code;
}

/* <device> .setdevice <eraseflag> */
/* Note that .setdevice clears the current pagedevice. */
int
zsetdevice(i_ctx_t *i_ctx_p)
{
    gx_device *odev = NULL, *dev = gs_currentdevice(igs);
    gx_device *ndev = NULL;
    os_ptr op = osp;
    int code = dev_proc(dev, dev_spec_op)(dev,
                        gxdso_current_output_device, (void *)&odev, 0);

    if (code < 0)
        return code;
    check_write_type(*op, t_device);

    if (op->value.pdevice == 0)
        return gs_note_error(gs_error_undefined);

    /* slightly icky special case: the new device may not have had
     * it's procs initialised, at this point - but we need to check
     * whether we're being asked to change the device here
     */
    if (dev_proc((op->value.pdevice), dev_spec_op) == NULL)
        ndev = op->value.pdevice;
    else
        code = dev_proc((op->value.pdevice), dev_spec_op)(op->value.pdevice,
                        gxdso_current_output_device, (void *)&ndev, 0);

    if (code < 0)
        return code;

    if (odev->LockSafetyParams) {	  /* do additional checking if locked  */
        if(ndev != odev) 	  /* don't allow a different device    */
            return_error(gs_error_invalidaccess);
    }
    code = zsetdevice_no_safer(i_ctx_p, op->value.pdevice);
    make_bool(op, code != 0);	/* erase page if 1 */
    return code;
}

/* Custom PostScript operator '.special_op' is used to implement
 * 'dev_spec_op' access from PostScript. Initially this is intended
 * to be used to recover individual device parameters from certain
 * devices (pdfwrite, ps2write etc). In the future we may choose to
 * increase the devices which can support this, and make more types
 * of 'spec_op' available from the PostScript world.
 */

/* We use this structure in a table below which allows us to add new
 * 'spec_op's with minimum fuss.
 */
typedef struct spec_op_s spec_op_t;
struct spec_op_s {
    char *name;					/* C string representing the name of the spec_op */
    int spec_op;                /* Integer used to switch on the name */
};

/* To add more spec_ops, put a key name (used to identify the specific
 * spec_op required) in this table, the integer is just used in the switch
 * in the main code to execute the required spec_op code.
 */
spec_op_t spec_op_defs[] = {
    {(char *)"GetDeviceParam", 0},
    {(char *)"EventInfo", 1},
};

/* <any> <any> .... /spec_op name .special_op <any> <any> .....
 * The special_op operator takes at a minimum the name of the spec_op to execute
 * and as many additional parameters as are required for the spec_op. It may
 * return as many additional parameters as required.
 */
int
zspec_op(i_ctx_t *i_ctx_p)
{
    os_ptr  op = osp;
    gx_device *dev = gs_currentdevice(igs);
    int i, nprocs = sizeof(spec_op_defs) / sizeof(spec_op_t), code, proc = -1;
    ref opname, nref, namestr;
    char *data;

    /* At the very minimum we need a name object telling us which sepc_op to perform */
    check_op(1);
    if (!r_has_type(op, t_name))
        return_error(gs_error_typecheck);

    ref_assign(&opname, op);

    /* Find the relevant spec_op name */
    for (i=0;i<nprocs;i++) {
        code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)spec_op_defs[i].name, strlen(spec_op_defs[i].name), &nref, 0);
        if (code < 0)
            return code;
        if (name_eq(&opname, &nref)) {
            proc = i;
            break;
        }
    }

    if (proc < 0)
        return_error(gs_error_undefined);

    pop(1);     /* We don't need the name of the spec_op any more */
    op = osp;

    switch(proc) {
        case 0:
            {
                stack_param_list list;
                dev_param_req_t request;
                ref rkeys;
                /* Get a single device parameter, we should be supplied with
                 * the name of the paramter, as a name object.
                 */
                check_op(1);
                if (!r_has_type(op, t_name))
                    return_error(gs_error_typecheck);

                ref_assign(&opname, op);
                name_string_ref(imemory, &opname, &namestr);

                data = (char *)gs_alloc_bytes(imemory, r_size(&namestr) + 1, "temporary special_op string");
                if (data == 0)
                    return_error(gs_error_VMerror);
                memset(data, 0x00, r_size(&namestr) + 1);
                memcpy(data, namestr.value.bytes, r_size(&namestr));

                /* Discard the parameter name now, we're done with it */
                pop (1);
                /* Make a null object so that the stack param list won't check for requests */
                make_null(&rkeys);
                stack_param_list_write(&list, &o_stack, &rkeys, iimemory);
                /* Stuff the data into a structure for passing to the spec_op */
                request.Param = data;
                request.list = &list;

                code = dev_proc(dev, dev_spec_op)(dev, gxdso_get_dev_param, &request, sizeof(dev_param_req_t));

                gs_free_object(imemory, data, "temporary special_op string");

                if (code < 0) {
                    if (code == gs_error_undefined) {
                        op = osp;
                        push(1);
                        make_bool(op, 0);
                    } else
                        return_error(code);
                } else {
                    op = osp;
                    push(1);
                    make_bool(op, 1);
                }
            }
            break;
        case 1:
            {
                stack_param_list list;
                dev_param_req_t request;
                ref rkeys;
                /* Get a single device parameter, we should be supplied with
                 * the name of the paramter, as a name object.
                 */
                check_op(1);
                if (!r_has_type(op, t_name))
                    return_error(gs_error_typecheck);

                ref_assign(&opname, op);
                name_string_ref(imemory, &opname, &namestr);

                data = (char *)gs_alloc_bytes(imemory, r_size(&namestr) + 1, "temporary special_op string");
                if (data == 0)
                    return_error(gs_error_VMerror);
                memset(data, 0x00, r_size(&namestr) + 1);
                memcpy(data, namestr.value.bytes, r_size(&namestr));

                /* Discard the parameter name now, we're done with it */
                pop (1);
                /* Make a null object so that the stack param list won't check for requests */
                make_null(&rkeys);
                stack_param_list_write(&list, &o_stack, &rkeys, iimemory);
                /* Stuff the data into a structure for passing to the spec_op */
                request.Param = data;
                request.list = &list;

                code = dev_proc(dev, dev_spec_op)(dev, gxdso_event_info, &request, sizeof(dev_param_req_t));

                gs_free_object(imemory, data, "temporary special_op string");

                if (code < 0) {
                    if (code == gs_error_undefined) {
                        op = osp;
                        push(1);
                        make_bool(op, 0);
                    } else
                        return_error(code);
                }
            }
            break;
        default:
            /* Belt and braces; it shold not be possible to get here, as the table
             * containing the names should mirror the entries in this switch. If we
             * found a name there should be a matching case here.
             */
            return_error(gs_error_undefined);
            break;
    }
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zdevice_op_defs[] =
{
    {"2.copydevice2", zcopydevice2},
    {"0currentdevice", zcurrentdevice},
    {"0.currentoutputdevice", zcurrentoutputdevice},
    {"1.devicename", zdevicename},
    {"0.doneshowpage", zdoneshowpage},
    {"0flushpage", zflushpage},
    {"7.getbitsrect", zgetbitsrect},
    {"1.getdevice", zgetdevice},
    {"0.getdefaultdevice", zgetdefaultdevice},
    {"2.getdeviceparams", zgetdeviceparams},
    {"2.gethardwareparams", zgethardwareparams},
    {"5makewordimagedevice", zmakewordimagedevice},
    {"0nulldevice", znulldevice},
    {"2.outputpage", zoutputpage},
    {"3.putdeviceparams", zputdeviceparams},
    {"1.setdevice", zsetdevice},
    op_def_end(0)
};

/* We need to split the table because of the 16-element limit. */
const op_def zdevice_ext_op_defs[] =
{
    {"0.special_op", zspec_op},
    op_def_end(0)
};
