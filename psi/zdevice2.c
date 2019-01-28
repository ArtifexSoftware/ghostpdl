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


/* Level 2 device operators */
#include "math_.h"
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "dstack.h"		/* for dict_find_name */
#include "estack.h"
#include "idict.h"
#include "idparam.h"
#include "igstate.h"
#include "iname.h"
#include "iutil.h"
#include "isave.h"
#include "store.h"
#include "gxdevice.h"
#include "gsstate.h"

/* Exported for zfunc4.c */
int z2copy(i_ctx_t *);

/* Forward references */
static int z2copy_gstate(i_ctx_t *);
static int push_callout(i_ctx_t *, const char *);

/* Extend the `copy' operator to deal with gstates. */
/* This is done with a hack -- we know that gstates are the only */
/* t_astruct subtype that implements copy. */
/* We export this for recognition in FunctionType 4 functions. */
int
z2copy(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code = zcopy(i_ctx_p);

    if (code >= 0)
        return code;
    if (!r_has_type(op, t_astruct))
        return code;
    return z2copy_gstate(i_ctx_p);
}

/* - .currentshowpagecount <count> true */
/* - .currentshowpagecount false */
static int
zcurrentshowpagecount(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gx_device *dev1, *dev = gs_currentdevice(igs);

    if ((*dev_proc(dev, get_page_device))(dev) == 0) {
        push(1);
        make_false(op);
    } else {
        dev1 = (*dev_proc(dev, get_page_device))(dev);
        push(2);
        make_int(op - 1, dev1->ShowpageCount);
        make_true(op);
    }
    return 0;
}

/* - .currentpagedevice <dict> <bool> */
static int
zcurrentpagedevice(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gx_device *dev = gs_currentdevice(igs);

    push(2);
    if ((*dev_proc(dev, get_page_device))(dev) != 0) {
        op[-1] = istate->pagedevice;
        make_true(op);
    } else {
        make_null(op - 1);
        make_false(op);
    }
    return 0;
}

/* <local_dict|null> .setpagedevice - */
static int
zsetpagedevice(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code;

/******
    if ( igs->in_cachedevice )
        return_error(gs_error_undefined);
 ******/
    if (r_has_type(op, t_dictionary)) {
        check_dict_read(*op);
#if 0	/****************/
        /*
         * In order to avoid invalidaccess errors on setpagedevice,
         * the dictionary must be allocated in local VM.
         */
        if (!(r_is_local(op)))
            return_error(gs_error_invalidaccess);
#endif	/****************/
        /* Make the dictionary read-only. */
        code = zreadonly(i_ctx_p);
        if (code < 0)
            return code;
    } else {
        check_type(*op, t_null);
    }
    istate->pagedevice = *op;
    pop(1);
    return 0;
}

/* Default Install/BeginPage/EndPage procedures */
/* that just call the procedure in the device. */

/* - .callinstall - */
static int
zcallinstall(i_ctx_t *i_ctx_p)
{
    gx_device *dev = gs_currentdevice(igs);

    if ((dev = (*dev_proc(dev, get_page_device))(dev)) != 0) {
        int code = (*dev->page_procs.install) (dev, igs);

        if (code < 0)
            return code;
    }
    return 0;
}

/* <showpage_count> .callbeginpage - */
static int
zcallbeginpage(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gx_device *dev = gs_currentdevice(igs);

    check_type(*op, t_integer);
    if ((dev = (*dev_proc(dev, get_page_device))(dev)) != 0) {
        int code = (*dev->page_procs.begin_page)(dev, igs);

        if (code < 0)
            return code;
    }
    pop(1);
    return 0;
}

/* <showpage_count> <reason_int> .callendpage <flush_bool> */
static int
zcallendpage(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gx_device *dev = gs_currentdevice(igs);
    int code;

    check_type(op[-1], t_integer);
    check_type(*op, t_integer);
    if ((dev = (*dev_proc(dev, get_page_device))(dev)) != 0) {
        code = (*dev->page_procs.end_page)(dev, (int)op->value.intval, igs);
        if (code < 0)
            return code;
        if (code > 1)
            return_error(gs_error_rangecheck);
    } else {
        code = (op->value.intval == 2 ? 0 : 1);
    }
    make_bool(op - 1, code);
    pop(1);
    return 0;
}

/* ------ Wrappers for operators that save the graphics state. ------ */

/* When saving the state with the current device a page device, */
/* we need to make sure that the page device dictionary exists */
/* so that grestore can use it to reset the device parameters. */
/* This may have significant performance consequences, but we don't see */
/* any way around it. */

/* Check whether we need to call out to create the page device dictionary. */
static bool
save_page_device(gs_gstate *pgs)
{
    return
        (r_has_type(&gs_int_gstate(pgs)->pagedevice, t_null) &&
         (*dev_proc(gs_currentdevice(pgs), get_page_device))(gs_currentdevice(pgs)) != 0);
}

/* - gsave - */
static int
z2gsave(i_ctx_t *i_ctx_p)
{
    if (!save_page_device(igs))
        return gs_gsave(igs);
    return push_callout(i_ctx_p, "%gsavepagedevice");
}

/* - save - */
static int
z2save(i_ctx_t *i_ctx_p)
{
    if (!save_page_device(igs))
        return zsave(i_ctx_p);
    return push_callout(i_ctx_p, "%savepagedevice");
}

/* - gstate <gstate> */
static int
z2gstate(i_ctx_t *i_ctx_p)
{
    if (!save_page_device(igs))
        return zgstate(i_ctx_p);
    return push_callout(i_ctx_p, "%gstatepagedevice");
}

/* <gstate1> <gstate2> copy <gstate2> */
static int
z2copy_gstate(i_ctx_t *i_ctx_p)
{
    if (!save_page_device(igs))
        return zcopy_gstate(i_ctx_p);
    return push_callout(i_ctx_p, "%copygstatepagedevice");
}

/* <gstate> currentgstate <gstate> */
static int
z2currentgstate(i_ctx_t *i_ctx_p)
{
    if (!save_page_device(igs))
        return zcurrentgstate(i_ctx_p);
    return push_callout(i_ctx_p, "%currentgstatepagedevice");
}

/* ------ Wrappers for operators that reset the graphics state. ------ */

/* Check whether we need to call out to restore the page device. */
static int
restore_page_device(i_ctx_t *i_ctx_p, const gs_gstate * pgs_old, const gs_gstate * pgs_new)
{
    gx_device *dev_old = gs_currentdevice(pgs_old);
    gx_device *dev_new;
    gx_device *dev_t1;
    gx_device *dev_t2;
    bool samepagedevice = obj_eq(dev_old->memory, &gs_int_gstate(pgs_old)->pagedevice,
        &gs_int_gstate(pgs_new)->pagedevice);
    bool LockSafetyParams = dev_old->LockSafetyParams;

    if ((dev_t1 = (*dev_proc(dev_old, get_page_device)) (dev_old)) == 0)
        return 0;
    /* If we are going to putdeviceparams in a callout, we need to */
    /* unlock temporarily.  The device will be re-locked as needed */
    /* by putdeviceparams from the pgs_old->pagedevice dict state. */
    if (!samepagedevice)
        dev_old->LockSafetyParams = false;
    dev_new = gs_currentdevice(pgs_new);
    if (dev_old != dev_new) {
        if ((dev_t2 = (*dev_proc(dev_new, get_page_device)) (dev_new)) == 0)
            samepagedevice = true;
        else if (dev_t1 != dev_t2)
            samepagedevice = false;
    }

    if (LockSafetyParams) {
        const int required_ops = 512;
        const int required_es = 32;

        /* The %grestorepagedevice must complete: the biggest danger
           is operand stack overflow. As we use get/putdeviceparams
           that means pushing all the device params onto the stack,
           pdfwrite having by far the largest number of parameters
           at (currently) 212 key/value pairs - thus needing (currently)
           424 entries on the op stack. Allowing for working stack
           space, and safety margin.....
         */
        if (required_ops + ref_stack_count(&o_stack) >= ref_stack_max_count(&o_stack)) {
           gs_currentdevice(pgs_old)->LockSafetyParams = LockSafetyParams;
           return_error(gs_error_stackoverflow);
        }
        /* We also want enough exec stack space - 32 is an overestimate of
           what we need to complete the Postscript call out.
         */
        if (required_es + ref_stack_count(&e_stack) >= ref_stack_max_count(&e_stack)) {
           gs_currentdevice(pgs_old)->LockSafetyParams = LockSafetyParams;
           return_error(gs_error_execstackoverflow);
        }
    }
    /*
     * The current implementation of setpagedevice just sets new
     * parameters in the same device object, so we have to check
     * whether the page device dictionaries are the same.
     */
    return samepagedevice ? 0 : 1;
}

/* - grestore - */
static int
z2grestore(i_ctx_t *i_ctx_p)
{
    int code = restore_page_device(i_ctx_p, igs, gs_gstate_saved(igs));
    if (code < 0) return code;

    if (code == 0)
        return gs_grestore(igs);
    return push_callout(i_ctx_p, "%grestorepagedevice");
}

/* - grestoreall - */
static int
z2grestoreall(i_ctx_t *i_ctx_p)
{
    for (;;) {
        int code = restore_page_device(i_ctx_p, igs, gs_gstate_saved(igs));
        if (code < 0) return code;
        if (code == 0) {
            bool done = !gs_gstate_saved(gs_gstate_saved(igs));

            gs_grestore(igs);
            if (done)
                break;
        } else
            return push_callout(i_ctx_p, "%grestoreallpagedevice");
    }
    return 0;
}
/* This is the Level 2+ variant of restore - which adds restoring
   of the page device to the Level 1 variant in zvmem.c.
   Previous this restored the device state before calling zrestore.c
   which validated operands etc, meaning a restore could error out
   partially complete.
   The operand checking, and actual VM restore are now in two functions
   so they can called separately thus, here, we can do as much
   checking as possible, before embarking on actual changes
 */
/* <save> restore - */
static int
z2restore(i_ctx_t *i_ctx_p)
{
    alloc_save_t *asave;
    bool saveLockSafety = gs_currentdevice_inline(igs)->LockSafetyParams;
    int code = restore_check_save(i_ctx_p, &asave);

    if (code < 0) return code;

    while (gs_gstate_saved(gs_gstate_saved(igs))) {
        code = restore_page_device(i_ctx_p, igs, gs_gstate_saved(igs));
        if (code < 0) return code;
        if (code > 0)
            return push_callout(i_ctx_p, "%restore1pagedevice");
        gs_grestore(igs);
    }
    code = restore_page_device(i_ctx_p, igs, gs_gstate_saved(igs));
    if (code < 0) return code;
    if (code > 0)
        return push_callout(i_ctx_p, "%restorepagedevice");

    code = dorestore(i_ctx_p, asave);

    if (code < 0) {
        /* An error here is basically fatal, but....
           restore_page_device() has to set LockSafetyParams false so it can
           configure the restored device correctly - in normal operation, that
           gets reset by that configuration. If we hit an error, though, that
           may not happen -  at least ensure we keep the setting through the
           error.
         */
        gs_currentdevice_inline(igs)->LockSafetyParams = saveLockSafety;
    }
    return code;
}

/* <gstate> setgstate - */
static int
z2setgstate(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code;

    check_stype(*op, st_igstate_obj);
    code = restore_page_device(i_ctx_p, igs, igstate_ptr(op));
    if (code < 0) return code;
    if (code == 0)
        return zsetgstate(i_ctx_p);
    return push_callout(i_ctx_p, "%setgstatepagedevice");
}

/* ------ Initialization procedure ------ */

const op_def zdevice2_l2_op_defs[] =
{
    op_def_begin_level2(),
    {"0.currentshowpagecount", zcurrentshowpagecount},
    {"0.currentpagedevice", zcurrentpagedevice},
    {"1.setpagedevice", zsetpagedevice},
                /* Note that the following replace prior definitions */
                /* in the indicated files: */
    {"1copy", z2copy},		/* zdps1.c */
    {"0gsave", z2gsave},	/* zgstate.c */
    {"0save", z2save},		/* zvmem.c */
    {"0gstate", z2gstate},	/* zdps1.c */
    {"1currentgstate", z2currentgstate},	/* zdps1.c */
    {"0grestore", z2grestore},	/* zgstate.c */
    {"0grestoreall", z2grestoreall},	/* zgstate.c */
    {"1restore", z2restore},	/* zvmem.c */
    {"1setgstate", z2setgstate},	/* zdps1.c */
                /* Default Install/BeginPage/EndPage procedures */
                /* that just call the procedure in the device. */
    {"0.callinstall", zcallinstall},
    {"1.callbeginpage", zcallbeginpage},
    {"2.callendpage", zcallendpage},
    op_def_end(0)
};

/* ------ Internal routines ------ */

/* Call out to a PostScript procedure. */
static int
push_callout(i_ctx_t *i_ctx_p, const char *callout_name)
{
    int code;

    check_estack(1);
    code = name_enter_string(imemory, callout_name, esp + 1);
    if (code < 0)
        return code;
    ++esp;
    r_set_attrs(esp, a_executable);
    return o_push_estack;
}
