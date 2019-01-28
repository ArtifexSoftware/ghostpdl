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


/* Miscellaneous operators */

#include "errno_.h"
#include "memory_.h"
#include "string_.h"
#include "ghost.h"
#include "gp.h"
#include "oper.h"
#include "ialloc.h"
#include "idict.h"
#include "dstack.h"		/* for name lookup in bind */
#include "iname.h"
#include "ipacked.h"
#include "ivmspace.h"
#include "store.h"
#include "igstate.h"            /* for gs_currentcpsimode */
#include "memento.h"
#include "iscan.h"

/**********************************************************************/

/* <proc> bind <proc> */
static inline bool
r_is_ex_oper(const ref *rp)
{
    return (r_has_attr(rp, a_executable) &&
            (r_btype(rp) == t_operator || r_type(rp) == t_oparray));
}
static int
zbind(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    uint depth = 1;
    ref defn;
    register os_ptr bsp;

    switch (r_type(op)) {
        case t_array:
            if (!r_has_attr(op, a_write)) {
                return 0;	/* per PLRM3 */
            }
        case t_mixedarray:
        case t_shortarray:
            defn = *op;
            break;
        case t_oparray:
            defn = *op->value.const_refs;
            break;
        default:
            return_op_typecheck(op);
    }
    push(1);
    *op = defn;
    bsp = op;
    /*
     * We must not make the top-level procedure read-only,
     * but we must bind it even if it is read-only already.
     *
     * Here are the invariants for the following loop:
     *      `depth' elements have been pushed on the ostack;
     *      For i < depth, p = ref_stack_index(&o_stack, i):
     *        *p is an array (or packedarray) ref.
     */
    while (depth) {
        while (r_size(bsp)) {
            ref_packed *const tpp = (ref_packed *)bsp->value.packed; /* break const */

            r_dec_size(bsp, 1);
            if (r_is_packed(tpp)) {
                /* Check for a packed executable name */
                ushort elt = *tpp;

                if (r_packed_is_exec_name(&elt)) {
                    ref nref;
                    ref *pvalue;

                    name_index_ref(imemory, packed_name_index(&elt),
                                   &nref);
                    if ((pvalue = dict_find_name(&nref)) != 0 &&
                        r_is_ex_oper(pvalue)
                        ) {
                        store_check_dest(bsp, pvalue);
                        /*
                         * Always save the change, since this can only
                         * happen once.
                         */
                        ref_do_save(bsp, tpp, "bind");
                        *tpp = pt_tag(pt_executable_operator) +
                            op_index(pvalue);
                    }
                }
                bsp->value.packed = tpp + 1;
            } else {
                ref *const tp = bsp->value.refs++;

                switch (r_type(tp)) {
                    case t_name:	/* bind the name if an operator */
                        if (r_has_attr(tp, a_executable)) {
                            ref *pvalue;

                            if ((pvalue = dict_find_name(tp)) != 0 &&
                                r_is_ex_oper(pvalue)
                                ) {
                                store_check_dest(bsp, pvalue);
                                ref_assign_old(bsp, tp, pvalue, "bind");
                            }
                        }
                        break;
                    case t_array:	/* push into array if writable */
                        if (!r_has_attr(tp, a_write))
                            break;
                    case t_mixedarray:
                    case t_shortarray:
                        if (r_has_attr(tp, a_executable)) {
                            /* Make reference read-only */
                            r_clear_attrs(tp, a_write);
                            if (bsp >= ostop) {
                                /* Push a new stack block. */
                                ref temp;
                                int code;

                                temp = *tp;
                                osp = bsp;
                                code = ref_stack_push(&o_stack, 1);
                                if (code < 0) {
                                    ref_stack_pop(&o_stack, depth);
                                    return_error(code);
                                }
                                bsp = osp;
                                *bsp = temp;
                            } else
                                *++bsp = *tp;
                            depth++;
                        }
                }
            }
        }
        bsp--;
        depth--;
        if (bsp < osbot) {	/* Pop back to the previous stack block. */
            osp = bsp;
            ref_stack_pop_block(&o_stack);
            bsp = osp;
        }
    }
    osp = bsp;
    return 0;
}

/* - serialnumber <int> */
static int
zserialnumber(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_int(op, gp_serialnumber());
    return 0;
}

/* - realtime <int> */
static int
zrealtime(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    long secs_ns[2];
    gs_lib_ctx_t *libctx = gs_lib_ctx_get_interp_instance(imemory);

    gp_get_realtime(secs_ns);
    secs_ns[1] -= libctx->real_time_0[1];
    secs_ns[0] -= libctx->real_time_0[0];
    push(1);
    make_int(op, secs_ns[0] * 1000 + secs_ns[1] / 1000000);
    return 0;
}

/* - usertime <int> */
static int
zusertime(i_ctx_t *i_ctx_p)
{
    gs_context_state_t *current = (gs_context_state_t *)i_ctx_p;
    os_ptr op = osp;
    long secs_ns[2];

    gp_get_usertime(secs_ns);
    if (!current->usertime_inited) {
        current->usertime_inited = true;
        current->usertime_0[0] = secs_ns[0];
        current->usertime_0[1] = secs_ns[1];
    }
    secs_ns[0] -= current->usertime_0[0];
    secs_ns[1] -= current->usertime_0[1];
    push(1);
    make_int(op, secs_ns[0] * 1000 + secs_ns[1] / 1000000);
    return 0;
}

/* ---------------- Non-standard operators ---------------- */

/* <string> getenv <value_string> true */
/* <string> getenv false */
static int
zgetenv(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    char *str;
    byte *value;
    int len = 0;

    check_read_type(*op, t_string);
    str = ref_to_string(op, imemory, "getenv key");
    if (str == 0)
        return_error(gs_error_VMerror);
    if (gp_getenv(str, (char *)0, &len) > 0) {	/* key missing */
        ifree_string((byte *) str, r_size(op) + 1, "getenv key");
        make_false(op);
        return 0;
    }
    value = ialloc_string(len, "getenv value");
    if (value == 0) {
        ifree_string((byte *) str, r_size(op) + 1, "getenv key");
        return_error(gs_error_VMerror);
    }
    DISCARD(gp_getenv(str, (char *)value, &len));	/* can't fail */
    ifree_string((byte *) str, r_size(op) + 1, "getenv key");
    /* Delete the stupid C string terminator. */
    value = iresize_string(value, len, len - 1,
                           "getenv value");	/* can't fail */
    push(1);
    make_string(op - 1, a_all | icurrent_space, len - 1, value);
    make_true(op);
    return 0;
}

/* - .defaultpapersize <string> true */
/* - .defaultpapersize false */
static int
zdefaultpapersize(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    byte *value;
    int len = 0;

    if (gp_defaultpapersize((char *)0, &len) > 0) {
        /* no default paper size */
        push(1);
        make_false(op);
        return 0;
    }

    value = ialloc_string(len, "defaultpapersize value");
    if (value == 0) {
        return_error(gs_error_VMerror);
    }
    DISCARD(gp_defaultpapersize((char *)value, &len));	/* can't fail */
    /* Delete the stupid C string terminator. */
    value = iresize_string(value, len, len - 1,
                           "defaultpapersize value");	/* can't fail */
    push(2);
    make_string(op - 1, a_all | icurrent_space, len - 1, value);
    make_true(op);
    return 0;
}

/* <name> <proc> .makeoperator <oper> */
static int
zmakeoperator(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    op_array_table *opt;
    uint count;
    ref *tab;

    check_type(op[-1], t_name);
    check_proc(*op);
    switch (r_space(op)) {
        case avm_global:
            opt = &i_ctx_p->op_array_table_global;
            break;
        case avm_local:
            opt = &i_ctx_p->op_array_table_local;
            break;
        default:
            return_error(gs_error_invalidaccess);
    }
    count = opt->count;
    tab = opt->table.value.refs;
    /*
     * restore doesn't reset op_array_table.count, but it does
     * remove entries from op_array_table.table.  Since we fill
     * the table in order, we can detect that a restore has occurred
     * by checking whether what should be the most recent entry
     * is occupied.  If not, we scan backwards over the vacated entries
     * to find the true end of the table.
     */
    while (count > 0 && r_has_type(&tab[count - 1], t_null))
        --count;
    if (count == r_size(&opt->table))
        return_error(gs_error_limitcheck);
    ref_assign_old(&opt->table, &tab[count], op, "makeoperator");
    opt->nx_table[count] = name_index(imemory, op - 1);
    op_index_ref(imemory, opt->base_index + count, op - 1);
    opt->count = count + 1;
    pop(1);
    return 0;
}

/* - .oserrno <int> */
static int
zoserrno(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_int(op, errno);
    return 0;
}

/* <int> .setoserrno - */
static int
zsetoserrno(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_type(*op, t_integer);
    errno = op->value.intval;
    pop(1);
    return 0;
}

/* <int> .oserrorstring <string> true */
/* <int> .oserrorstring false */
static int
zoserrorstring(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    const char *str;
    int code;
    uint len;
    byte ch;

    check_type(*op, t_integer);
    str = gp_strerror((int)op->value.intval);
    if (str == 0 || (len = strlen(str)) == 0) {
        make_false(op);
        return 0;
    }
    check_ostack(1);
    code = string_to_ref(str, op, iimemory, ".oserrorstring");
    if (code < 0)
        return code;
    /* Strip trailing end-of-line characters. */
    while ((len = r_size(op)) != 0 &&
           ((ch = op->value.bytes[--len]) == '\r' || ch == '\n')
        )
        r_dec_size(op, 1);
    push(1);
    make_true(op);
    return 0;
}

/* <string> <bool> .setdebug - */
static int
zsetdebug(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    check_read_type(op[-1], t_string);
    check_type(*op, t_boolean);
    {
        int i;

        for (i = 0; i < r_size(op - 1); i++)
            gs_debug[op[-1].value.bytes[i] & 127] =
                op->value.boolval;
    }
    pop(2);
    return 0;
}

/* .mementolistnew - */
static int
zmementolistnewblocks(i_ctx_t *i_ctx_p)
{
    Memento_listNewBlocks();
    return 0;
}

/* There are a few cases where a customer/user might want CPSI behavior
 * instead of the GS default behavior. cmyk_to_rgb and Type 1 char fill
 * method are two that have come up so far. This operator allows a PS
 * program to control the behavior without needing to recompile.
 */
/* <bool> .setCPSImode - */
static int
zsetCPSImode(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    check_type(*op, t_boolean);
    gs_setcpsimode(imemory, op->value.boolval);
    if (op->value.boolval) {
        i_ctx_p->scanner_options |= SCAN_CPSI_MODE;
    }
    else {
        i_ctx_p->scanner_options &= ~(int)SCAN_CPSI_MODE;
    }
    pop(1);
    return 0;
}

/* - .getCPSImode <bool> */
static int
zgetCPSImode(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_bool(op, gs_currentcpsimode(imemory));
    return 0;
}

/* <int> .setscanconverter - */
static int
zsetscanconverter(i_ctx_t *i_ctx_p)
{
    int val;

    os_ptr op = osp;
    if (r_has_type(op, t_boolean))
        val = (int)op->value.boolval;
    else if (r_has_type(op, t_integer))
        val = op->value.intval;
    else
        return_op_typecheck(op);

    gs_setscanconverter(igs, val);
    pop(1);
    return 0;
}

/* - .getscanconverter <int> */
static int
zgetscanconverter(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_int(op, gs_getscanconverter(imemory));
    return 0;
}
/* ------ Initialization procedure ------ */

const op_def zmisc_a_op_defs[] =
{
    {"1bind", zbind},
    {"1getenv", zgetenv},
    {"0.defaultpapersize", zdefaultpapersize},
    {"2.makeoperator", zmakeoperator},
    {"0.oserrno", zoserrno},
    {"1.oserrorstring", zoserrorstring},
    {"0realtime", zrealtime},
    {"0serialnumber", zserialnumber},
    {"2.setdebug", zsetdebug},
    {"0.mementolistnewblocks", zmementolistnewblocks},
    {"1.setoserrno", zsetoserrno},
    {"0usertime", zusertime},
    op_def_end(0)
};

const op_def zmisc_b_op_defs[] =
{
    {"1.setCPSImode", zsetCPSImode},
    {"0.getCPSImode", zgetCPSImode},
    {"1.setscanconverter", zsetscanconverter},
    {"0.getscanconverter", zgetscanconverter},
    op_def_end(0)
};
