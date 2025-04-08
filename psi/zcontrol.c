/* Copyright (C) 2001-2025 Artifex Software, Inc.
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


/* Control operators */
#include "string_.h"
#include "ghost.h"
#include "stream.h"
#include "oper.h"
#include "estack.h"
#include "files.h"
#include "ipacked.h"
#include "iutil.h"
#include "store.h"
#include "interp.h"

/* Forward references */
static int check_for_exec(const_os_ptr);
static int no_cleanup(i_ctx_t *);
static uint count_exec_stack(i_ctx_t *, bool);
static uint count_to_stopped(i_ctx_t *, long);
static int unmatched_exit(os_ptr, op_proc_t);

/* See the comment in opdef.h for an invariant which allows */
/* more efficient implementation of for, loop, and repeat. */

/* <[test0 body0 ...]> .cond - */
static int cond_continue(i_ctx_t *);
static int
zcond(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    es_ptr ep = esp;

    check_op(1);
    /* Push the array on the e-stack and call the continuation. */
    if (!r_is_array(op))
        return_op_typecheck(op);
    check_execute(*op);
    if ((r_size(op) & 1) != 0)
        return_error(gs_error_rangecheck);
    if (r_size(op) == 0)
        return zpop(i_ctx_p);
    check_estack(3);
    esp = ep += 3;
    ref_assign(ep - 2, op);	/* the cond body */
    make_op_estack(ep - 1, cond_continue);
    array_get(imemory, op, 0L, ep);
    esfile_check_cache();
    pop(1);
    return o_push_estack;
}
static int
cond_continue(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    es_ptr ep = esp;
    int code;

    check_op(1);
    /* The top element of the e-stack is the remaining tail of */
    /* the cond body.  The top element of the o-stack should be */
    /* the (boolean) result of the test that is the first element */
    /* of the tail. */
    check_type(*op, t_boolean);
    if (op->value.boolval) {	/* true */
        array_get(imemory, ep, 1L, ep);
        esfile_check_cache();
        code = o_pop_estack;
    } else if (r_size(ep) > 2) {	/* false */
        const ref_packed *elts = ep->value.packed;

        check_estack(2);
        ep = esp;
        r_dec_size(ep, 2);
        elts = packed_next(elts);
        elts = packed_next(elts);
        ep->value.packed = elts;
        array_get(imemory, ep, 0L, ep + 2);
        make_op_estack(ep + 1, cond_continue);
        esp = ep + 2;
        esfile_check_cache();
        code = o_push_estack;
    } else {			/* fall off end of cond */
        esp = ep - 1;
        code = o_pop_estack;
    }
    pop(1);			/* get rid of the boolean */
    return code;
}

/* <obj> exec - */
int
zexec(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code;

    check_op(1);
    code = check_for_exec(op);
    if (code < 0) {
        return code;
    }
    if (!r_has_attr(op, a_executable)) {
        return 0;	/* shortcut, literal object just gets pushed back */
    }
    check_estack(1);
    ++esp;
    ref_assign(esp, op);
    esfile_check_cache();
    pop(1);
    return o_push_estack;
}

/* <obj1> ... <objn> <n> .execn - */
static int
zexecn(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    uint n, i;
    es_ptr esp_orig;

    check_op(1);
    check_int_leu(*op, max_uint - 1);
    n = (uint) op->value.intval;
    check_op(n + 1);
    check_estack(n);
    esp_orig = esp;
    for (i = 0; i < n; ++i) {
        const ref *rp = ref_stack_index(&o_stack, (long)(i + 1));

        if (rp == NULL)
            continue;

        /* Make sure this object is legal to execute. */
        if (ref_type_uses_access(r_type(rp))) {
            if (!r_has_attr(rp, a_execute) &&
                r_has_attr(rp, a_executable)
                ) {
                esp = esp_orig;
                return_error(gs_error_invalidaccess);
            }
        }
        /* Executable nulls have a special meaning on the e-stack, */
        /* so since they are no-ops, don't push them. */
        if (!r_has_type_attrs(rp, t_null, a_executable)) {
            ++esp;
            ref_assign(esp, rp);
        }
    }
    esfile_check_cache();
    pop(n + 1);
    return o_push_estack;
}

/* <array> <executable> .runandhide <obj>				*/
/* 	before executing  <executable>, <array> is been removed from	*/
/*	the operand stack and placed on the execstack with attributes	*/
/* 	changed to 'noaccess'.						*/
/* 	After execution, the array will be placed on  the top of the	*/
/*	operand stack (on top of any elemetns pushed by <executable>	*/
/*	for both the normal case and for the error case.		*/
static int end_runandhide(i_ctx_t *);
static int err_end_runandhide(i_ctx_t *);
static int
zrunandhide(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    es_ptr ep;

    check_op(2);
    if (!r_is_array(op - 1))
        return_op_typecheck(op);
    if (!r_has_attr(op, a_executable))
        return 0;		/* literal object just gets pushed back */
    check_estack(5);
    ep = esp += 5;
    make_mark_estack(ep - 4, es_other, err_end_runandhide); /* error case */
    make_op_estack(ep - 1,  end_runandhide); /* normal case */
    ref_assign(ep, op);
    /* Store the object we are hiding  and it's current tas.type_attrs */
    /* on the exec stack then change to 'noaccess' */
    make_int(ep - 3, (int)op[-1].tas.type_attrs);
    ref_assign(ep - 2, op - 1);
    r_clear_attrs(ep - 2, a_all);
    /* replace the array with a special kind of mark that has a_read access */
    esfile_check_cache();
    pop(2);
    return o_push_estack;
}
static int
runandhide_restore_hidden(i_ctx_t *i_ctx_p, ref *obj, ref *attrs)
{
    os_ptr op = osp;

    push(1);
    /* restore the hidden_object and its type_attrs */
    ref_assign(op, obj);
    r_clear_attrs(op, a_all);
    r_set_attrs(op, attrs->value.intval);
    return 0;
}

/* - %end_runandhide hiddenobject */
static int
end_runandhide(i_ctx_t *i_ctx_p)
{
    int code;

    if ((code = runandhide_restore_hidden(i_ctx_p, esp, esp - 1)) < 0) {
        esp -= 2;
        return code;
    }
    esp -= 2;		/* pop the hidden value and its atributes */
    return o_pop_estack;
}

/* restore hidden object for error returns */
static int
err_end_runandhide(i_ctx_t *i_ctx_p)
{
    int code;

    if ((code = runandhide_restore_hidden(i_ctx_p, esp + 3, esp + 2)) < 0)
        return code;
    return 0;
}

/* <bool> <proc> if - */
int
zif(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_op(2);
    check_proc(*op);
    check_type(op[-1], t_boolean);
    if (op[-1].value.boolval) {
        check_estack(1);
        ++esp;
        ref_assign(esp, op);
        esfile_check_cache();
    }
    pop(2);
    return o_push_estack;
}

/* <bool> <proc_true> <proc_false> ifelse - */
int
zifelse(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_op(3);
    check_proc(*op);
    check_proc(op[-1]);
    check_type(op[-2], t_boolean);
    check_estack(1);
    ++esp;
    if (op[-2].value.boolval) {
        ref_assign(esp, op - 1);
    } else {
        ref_assign(esp, op);
    }
    esfile_check_cache();
    pop(3);
    return o_push_estack;
}

/* <init> <step> <limit> <proc> for - */
static int
    for_pos_int_continue(i_ctx_t *),
    for_neg_int_continue(i_ctx_t *),
    for_real_continue(i_ctx_t *);
int
zfor(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    register es_ptr ep;
    int code;
    float params[3];

    check_op(4);
        /* Mostly undocumented, and somewhat bizarre Adobe behavior discovered	*/
        /* with the CET (28-05) and FTS (124-01) is that the proc is not run	*/
        /* if BOTH the initial value and increment are zero.			*/
    if ((code = float_params(op - 1, 3, params)) < 0)
        return code;
    if ( params[0] == 0.0 && params[1] == 0.0 ) {
        pop(4);		/* don't run the proc */
        return 0;
    }
    check_estack(7);
    ep = esp + 6;
    check_proc(*op);
    /* Push a mark, the control variable set to the initial value, */
    /* the increment, the limit, and the procedure, */
    /* and invoke the continuation operator. */
    if (r_has_type(op - 3, t_integer) &&
        r_has_type(op - 2, t_integer)
        ) {
        make_int(ep - 4, op[-3].value.intval);
        make_int(ep - 3, op[-2].value.intval);
        switch (r_type(op - 1)) {
            case t_integer:
                make_int(ep - 2, op[-1].value.intval);
                break;
            case t_real:
                make_int(ep - 2, (ps_int)op[-1].value.realval);
                break;
            default:
                return_op_typecheck(op - 1);
        }
        if (ep[-3].value.intval >= 0)
            make_op_estack(ep, for_pos_int_continue);
        else
            make_op_estack(ep, for_neg_int_continue);
    } else {
        make_real(ep - 4, params[0]);
        make_real(ep - 3, params[1]);
        make_real(ep - 2, params[2]);
        make_op_estack(ep, for_real_continue);
    }
    make_mark_estack(ep - 5, es_for, no_cleanup);
    ref_assign(ep - 1, op);
    esp = ep;
    pop(4);
    return o_push_estack;
}
/* Continuation operators for for, separate for positive integer, */
/* negative integer, and real. */
/* Execution stack contains mark, control variable, increment, */
/* limit, and procedure (procedure is topmost.) */
/* Continuation operator for positive integers. */
static int
for_pos_int_continue(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    register es_ptr ep = esp;
    ps_int var = ep[-3].value.intval;

    if (var > ep[-1].value.intval) {
        esp -= 5;		/* pop everything */
        return o_pop_estack;
    }
    push(1);
    make_int(op, var);
    ep[-3].value.intval = var + ep[-2].value.intval;
    ref_assign_inline(ep + 2, ep);	/* saved proc */
    esp = ep + 2;
    return o_push_estack;
}
/* Continuation operator for negative integers. */
static int
for_neg_int_continue(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    register es_ptr ep = esp;
    ps_int var = ep[-3].value.intval;

    if (var < ep[-1].value.intval) {
        esp -= 5;		/* pop everything */
        return o_pop_estack;
    }
    push(1);
    make_int(op, var);
    ep[-3].value.intval = var + ep[-2].value.intval;
    ref_assign(ep + 2, ep);	/* saved proc */
    esp = ep + 2;
    return o_push_estack;
}
/* Continuation operator for reals. */
static int
for_real_continue(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    es_ptr ep = esp;
    float var = ep[-3].value.realval;
    float incr = ep[-2].value.realval;

    if (incr >= 0 ? (var > ep[-1].value.realval) :
        (var < ep[-1].value.realval)
        ) {
        esp -= 5;		/* pop everything */
        return o_pop_estack;
    }
    push(1);
    ref_assign(op, ep - 3);
    ep[-3].value.realval = var + incr;
    esp = ep + 2;
    ref_assign(ep + 2, ep);	/* saved proc */
    return o_push_estack;
}

/*
 * Here we provide an internal variant of 'for' that enumerates the values
 * A, ((N-1)*A+1*B)/N, ((N-2)*A+2*B)/N, ..., B precisely.  The arguments are
 * A (real), N (integer), and B (real).  We need this for loading caches such
 * as the transfer function cache.
 *
 * NOTE: This computation must match the SAMPLE_LOOP_VALUE macro in gscie.h.
 */
static int for_samples_continue(i_ctx_t *);
/* <first> <count> <last> <proc> %for_samples - */
int
zfor_samples(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    es_ptr ep;

    check_op(4);
    check_type(op[-3], t_real);
    check_type(op[-2], t_integer);
    check_type(op[-1], t_real);
    check_proc(*op);
    check_estack(8);
    ep = esp + 7;
    make_mark_estack(ep - 6, es_for, no_cleanup);
    make_int(ep - 5, 0);
    memcpy(ep - 4, op - 3, 3 * sizeof(ref));
    ref_assign(ep - 1, op);
    make_op_estack(ep, for_samples_continue);
    esp = ep;
    pop(4);
    return o_push_estack;
}
/* Continuation procedure */
static int
for_samples_continue(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    es_ptr ep = esp;
    int var = ep[-4].value.intval;
    float a = ep[-3].value.realval;
    int n = ep[-2].value.intval;
    float b = ep[-1].value.realval;

    if (var > n) {
        esp -= 6;		/* pop everything */
        return o_pop_estack;
    }
    push(1);
    make_real(op, ((n - var) * a + var * b) / n);
    ep[-4].value.intval = var + 1;
    ref_assign_inline(ep + 2, ep);	/* saved proc */
    esp = ep + 2;
    return o_push_estack;
}

/* <int> <proc> repeat - */
static int repeat_continue(i_ctx_t *);
int
zrepeat(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_op(2);
    check_proc(*op);
    check_type(op[-1], t_integer);
    if (op[-1].value.intval < 0)
        return_error(gs_error_rangecheck);
    check_estack(5);
    /* Push a mark, the count, and the procedure, and invoke */
    /* the continuation operator. */
    push_mark_estack(es_for, no_cleanup);
    *++esp = op[-1];
    *++esp = *op;
    make_op_estack(esp + 1, repeat_continue);
    pop(2);
    return repeat_continue(i_ctx_p);
}
/* Continuation operator for repeat */
static int
repeat_continue(i_ctx_t *i_ctx_p)
{
    es_ptr ep = esp;		/* saved proc */

    if (--(ep[-1].value.intval) >= 0) {		/* continue */
        esp += 2;
        ref_assign(esp, ep);
        return o_push_estack;
    } else {			/* done */
        esp -= 3;		/* pop mark, count, proc */
        return o_pop_estack;
    }
}

/* <proc> loop */
static int loop_continue(i_ctx_t *);
static int
zloop(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_op(1);
    check_proc(*op);
    check_estack(4);
    /* Push a mark and the procedure, and invoke */
    /* the continuation operator. */
    push_mark_estack(es_for, no_cleanup);
    *++esp = *op;
    make_op_estack(esp + 1, loop_continue);
    pop(1);
    return loop_continue(i_ctx_p);
}
/* Continuation operator for loop */
static int
loop_continue(i_ctx_t *i_ctx_p)
{
    register es_ptr ep = esp;	/* saved proc */

    ref_assign(ep + 2, ep);
    esp = ep + 2;
    return o_push_estack;
}

/* - exit - */
static int
zexit(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    ref_stack_enum_t rsenum;
    uint scanned = 0;

    ref_stack_enum_begin(&rsenum, &e_stack);
    do {
        uint used = rsenum.size;
        es_ptr ep = rsenum.ptr + used - 1;
        uint count = used;

        for (; count; count--, ep--)
            if (r_is_estack_mark(ep))
                switch (estack_mark_index(ep)) {
                    case es_for:
                        pop_estack(i_ctx_p, scanned + (used - count + 1));
                        return o_pop_estack;
                    case es_stopped:
                        return_error(gs_error_invalidexit);	/* not a loop */
                }
        scanned += used;
    } while (ref_stack_enum_next(&rsenum));
    /* No mark, quit.  (per Adobe documentation) */
    push(2);
    return unmatched_exit(op, zexit);
}

/*
 * .stopped pushes the following on the e-stack:
 *      - A mark with type = es_stopped and procedure = no_cleanup.
 *      - The result to be pushed on a normal return.
 *      - The signal mask for .stop.
 *      - The procedure %stopped_push, to handle the normal return case.
 */

/* In the normal (no-error) case, pop the mask from the e-stack, */
/* and move the result to the o-stack. */
static int
stopped_push(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    *op = esp[-1];
    esp -= 3;
    return o_pop_estack;
}

/* - stop - */
/* Equivalent to true 1 .stop. */
/* This is implemented in C because if were a pseudo-operator, */
/* the stacks would get restored in case of an error. */
static int
zstop(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    uint count = count_to_stopped(i_ctx_p, 1L);

    if (count) {
        /*
         * If there are any t_oparrays on the e-stack, they will pop
         * any new items from the o-stack.  Wait to push the 'true'
         * until we have run all the unwind procedures.
         */
        check_ostack(2);
        pop_estack(i_ctx_p, count);
        op = osp;
        push(1);
        make_true(op);
        return o_pop_estack;
    }
    /* No mark, quit.  (per Adobe documentation) */
    push(2);
    return unmatched_exit(op, zstop);
}

/* <result> <mask> .stop - */
static int
zzstop(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    uint count;

    check_op(2);
    check_type(*op, t_integer);
    count = count_to_stopped(i_ctx_p, op->value.intval);
    if (count) {
        /*
         * If there are any t_oparrays on the e-stack, they will pop
         * any new items from the o-stack.  Wait to push the result
         * until we have run all the unwind procedures.
         */
        ref save_result;

        check_op(2);
        save_result = op[-1];
        pop(2);
        pop_estack(i_ctx_p, count);
        op = osp;
        push(1);
        *op = save_result;
        return o_pop_estack;
    }
    /* No mark, quit.  (per Adobe documentation) */
    return unmatched_exit(op, zzstop);
}

/* <obj> stopped <stopped> */
/* Equivalent to false 1 .stopped. */
/* This is implemented in C because if were a pseudo-operator, */
/* the stacks would get restored in case of an error. */
static int
zstopped(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_op(1);
    /* Mark the execution stack, and push the default result */
    /* in case control returns normally. */
    check_estack(5);
    push_mark_estack(es_stopped, no_cleanup);
    ++esp;
    make_false(esp);		/* save the result */
    ++esp;
    make_int(esp, 1);		/* save the signal mask */
    push_op_estack(stopped_push);
    push_op_estack(zexec);	/* execute the operand */
    return o_push_estack;
}

/* <obj> <result> <mask> .stopped <result> */
static int
zzstopped(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    check_type(*op, t_integer);
    check_op(3);
    /* Mark the execution stack, and push the default result */
    /* in case control returns normally. */
    check_estack(5);
    push_mark_estack(es_stopped, no_cleanup);
    *++esp = op[-1];		/* save the result */
    *++esp = *op;		/* save the signal mask */
    push_op_estack(stopped_push);
    push_op_estack(zexec);	/* execute the operand */
    pop(2);
    return o_push_estack;
}

/* <mask> .instopped false */
/* <mask> .instopped <result> true */
static int
zinstopped(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    uint count;

    check_type(*op, t_integer);
    count = count_to_stopped(i_ctx_p, op->value.intval);
    if (count) {
        push(1);
        op[-1] = *ref_stack_index(&e_stack, count - 2);		/* default result */
        make_true(op);
    } else
        make_false(op);
    return 0;
}

/* <include_marks> .countexecstack <int> */
/* - countexecstack <int> */
/* countexecstack is an operator solely for the sake of the Genoa tests. */
static int
zcountexecstack(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_int(op, count_exec_stack(i_ctx_p, false));
    return 0;
}
static int
zcountexecstack1(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_type(*op, t_boolean);
    make_int(op, count_exec_stack(i_ctx_p, op->value.boolval));
    return 0;
}

/* <array> <include_marks> .execstack <subarray> */
/* <array> execstack <subarray> */
/* execstack is an operator solely for the sake of the Genoa tests. */
static int execstack_continue(i_ctx_t *);
static int execstack2_continue(i_ctx_t *);
static int
push_execstack(i_ctx_t *i_ctx_p, os_ptr op1, bool include_marks,
               op_proc_t cont)
{
    uint size;
    /*
     * We can't do this directly, because the interpreter
     * might have cached some state.  To force the interpreter
     * to update the stored state, we push a continuation on
     * the exec stack; the continuation is executed immediately,
     * and does the actual transfer.
     */
    uint depth;

    if (!r_is_array(op1))
        return_op_typecheck(op1);
    /* Check the length before the write access per CET 28-03 */
    size = r_size(op1);
    depth = count_exec_stack(i_ctx_p, include_marks);
    if (depth > size)
        return_error(gs_error_rangecheck);
    check_write(*op1);
    {
        int code = ref_stack_store_check(&e_stack, op1, size, 0);

        if (code < 0)
            return code;
    }
    check_estack(1);
    r_set_size(op1, depth);
    push_op_estack(cont);
    return o_push_estack;
}
static int
zexecstack(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    return push_execstack(i_ctx_p, op, false, execstack_continue);
}
static int
zexecstack2(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_type(*op, t_boolean);
    return push_execstack(i_ctx_p, op - 1, op->value.boolval, execstack2_continue);
}
/* Continuation operator to do the actual transfer. */
/* r_size(op1) was set just above. */
static int
do_execstack(i_ctx_t *i_ctx_p, bool include_marks, bool include_oparrays, os_ptr op1)
{
    os_ptr op = osp;
    ref *arefs = op1->value.refs;
    uint asize = r_size(op1);
    uint i;
    ref *rq;

    /*
     * Copy elements from the stack to the array,
     * optionally skipping executable nulls.
     * Clear the executable bit in any internal operators, and
     * convert t_structs and t_astructs (which can only appear
     * in connection with stack marks, which means that they will
     * probably be freed when unwinding) to something harmless.
     */
    for (i = 0, rq = arefs + asize; rq != arefs; ++i) {
        const ref *rp = ref_stack_index(&e_stack, (long)i);

        if (rp == NULL)
            continue;
        if (r_has_type_attrs(rp, t_null, a_executable) && !include_marks)
            continue;
        --rq;
        ref_assign_old(op1, rq, rp, "execstack");
        switch (r_type(rq)) {
            case t_operator: {
                uint opidx = op_index(rq);

                if (opidx == 0 || op_def_is_internal(op_index_def(opidx)))
                    r_clear_attrs(rq, a_executable);
                break;
            }
            case t_struct:
            case t_astruct: {
                const char *tname = rq->value.pstruct ?
                    gs_struct_type_name_string(
                                gs_object_type(imemory, rq->value.pstruct))
                    : "NULL";

                make_const_string(rq, a_readonly | avm_foreign,
                                  strlen(tname), (const byte *)tname);
                break;
            }
            case t_array:
            case t_shortarray:
            case t_mixedarray:
                if (!include_oparrays && errorexec_find(i_ctx_p, rq) < 0)
                    make_null(rq);
                break;
            default:
                ;
        }
    }
    pop(op - op1);
    return 0;
}
static int
execstack_continue(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    return do_execstack(i_ctx_p, false, false, op);
}
static int
execstack2_continue(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    return do_execstack(i_ctx_p, op->value.boolval, true, op - 1);
}

/* - .needinput - */
static int
zneedinput(i_ctx_t *i_ctx_p)
{
    return gs_error_NeedInput;		/* interpreter will exit to caller */
}

/* <obj> <int> .quit - */
static int
zquit(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_op(2);
    check_type(*op, t_integer);
    return_error(gs_error_Quit);	/* Interpreter will do the exit */
}

/* Get the current file from which the interpreter is reading. */
static ref *
zget_current_file(i_ctx_t *i_ctx_p)
{
    ref_stack_enum_t rsenum;

    ref_stack_enum_begin(&rsenum, &e_stack);
    do {
        uint count = rsenum.size;
        es_ptr ep = rsenum.ptr + count - 1;

        for (; count; count--, ep--)
            if (r_has_type_attrs(ep, t_file, a_executable))
                return ep;
    } while (ref_stack_enum_next(&rsenum));
    return 0;
}

/* - currentfile <file> */
int
z_current_file(i_ctx_t *i_ctx_p, ref **s)
{
    ref *fp;
    /* Check the cache first */
    if (esfile != 0) {
#ifdef DEBUG
        /* Check that esfile is valid. */
        ref *efp = zget_current_file(i_ctx_p);

        if (esfile != efp) {
            lprintf2("currentfile: esfile="PRI_INTPTR", efp="PRI_INTPTR"\n",
                     (intptr_t) esfile, (intptr_t) efp);
            *s = efp;
        } else
#endif
            *s = esfile;
    } else if ((fp = zget_current_file(i_ctx_p)) == 0) {	/* Return an invalid file object. */
        *s = NULL;
    } else {
        *s = fp;
        esfile_set_cache(fp);
    }
    return 0;
}
static int
zcurrentfile(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    ref *s;
    int code;

    push(1);

    code = z_current_file(i_ctx_p, &s);
    if (code < 0 || s == NULL) {
        /* This doesn't make a lot of sense to me, */
        /* but it's what the PostScript manual specifies. */
        make_invalid_file(i_ctx_p, op);
    }
    else {
        ref_assign(op, s);
    }
    /* Make the returned value literal. */
    r_clear_attrs(op, a_executable);
    return code;
}
/* ------ Initialization procedure ------ */

/* We need to split the table because of the 16-element limit. */
const op_def zcontrol1_op_defs[] = {
    {"1.cond", zcond},
    {"0countexecstack", zcountexecstack},
    {"1.countexecstack", zcountexecstack1},
    {"0currentfile", zcurrentfile},
    {"1exec", zexec},
    {"1.execn", zexecn},
    {"1execstack", zexecstack},
    {"2.execstack", zexecstack2},
    {"0exit", zexit},
    {"2if", zif},
    {"3ifelse", zifelse},
    {"0.instopped", zinstopped},
    {"0.needinput", zneedinput},
    op_def_end(0)
};
const op_def zcontrol2_op_defs[] = {
    {"4for", zfor},
    {"1loop", zloop},
    {"2.quit", zquit},
    {"2repeat", zrepeat},
    {"0stop", zstop},
    {"1.stop", zzstop},
    {"1stopped", zstopped},
    {"2.stopped", zzstopped},
    op_def_end(0)
};
const op_def zcontrol3_op_defs[] = {
                /* Internal operators */
    {"1%cond_continue", cond_continue},
    {"1%execstack_continue", execstack_continue},
    {"2%execstack2_continue", execstack2_continue},
    {"0%for_pos_int_continue", for_pos_int_continue},
    {"0%for_neg_int_continue", for_neg_int_continue},
    {"0%for_real_continue", for_real_continue},
    {"4%for_samples", zfor_samples},
    {"0%for_samples_continue", for_samples_continue},
    {"0%loop_continue", loop_continue},
    {"0%repeat_continue", repeat_continue},
    {"0%stopped_push", stopped_push},
    {"2.runandhide", zrunandhide},
    {"0%end_runandhide", end_runandhide},
    op_def_end(0)
};

/* ------ Internal routines ------ */

/*
 * Check the operand of exec or stopped.  Return 0 if OK to execute, or a
 * negative error code.  We emulate an apparent bug in Adobe interpreters,
 * which cause an invalidaccess error when 'exec'ing a noaccess literal
 * (other than dictionaries).  We also match the Adobe interpreters in that
 * we catch noaccess executable objects here, rather than waiting for the
 * interpreter to catch them, so that we can signal the error with the
 * object still on the operand stack.
 */
static int
check_for_exec(const_os_ptr op)
{
    if (!r_has_attr(op, a_execute) && /* only true if noaccess */
        ref_type_uses_access(r_type(op)) &&
        (r_has_attr(op, a_executable) || !r_has_type(op, t_dictionary))
        ) {
        return_error(gs_error_invalidaccess);
    }
    return 0;
}

/* Vacuous cleanup routine */
static int
no_cleanup(i_ctx_t *i_ctx_p)
{
    return 0;
}

/*
 * Count the number of elements on the exec stack, with or without
 * the normally invisible elements (*op is a Boolean that indicates this).
 */
static uint
count_exec_stack(i_ctx_t *i_ctx_p, bool include_marks)
{
    uint count = ref_stack_count(&e_stack);

    if (!include_marks) {
        uint i;

        for (i = count; i--;) {
            ref *o;
            o = ref_stack_index(&e_stack, (long)i);
            if (o == NULL)
                continue;
            if (r_has_type_attrs(o, t_null, a_executable))
                --count;
        }
    }
    return count;
}

/*
 * Count the number of elements down to and including the first 'stopped'
 * mark on the e-stack with a given mask.  Return 0 if there is no 'stopped'
 * mark.
 */
static uint
count_to_stopped(i_ctx_t *i_ctx_p, long mask)
{
    ref_stack_enum_t rsenum;
    uint scanned = 0;

    ref_stack_enum_begin(&rsenum, &e_stack);
    do {
        uint used = rsenum.size;
        es_ptr ep = rsenum.ptr + used - 1;
        uint count = used;

        for (; count; count--, ep--) {
            if (r_is_estack_mark(ep)) {
                if (estack_mark_index(ep) == es_stopped &&
                  (ep[2].value.intval & mask) != 0)
                    return scanned + (used - count + 1);
            }
        }
        scanned += used;
    } while (ref_stack_enum_next(&rsenum));
    return 0;
}

/*
 * Pop the e-stack, executing cleanup procedures as needed.
 * We could make this more efficient using ref_stack_enum_*,
 * but it isn't used enough to make this worthwhile.
 */
void
pop_estack(i_ctx_t *i_ctx_p, uint count)
{
    uint idx = 0;
    uint popped = 0;

    esfile_clear_cache();
    for (; idx < count; idx++) {
        ref *ep = ref_stack_index(&e_stack, idx - popped);

        if (ep == NULL)
            continue;

        if (r_is_estack_mark(ep)) {
            /* This exec stack juggling is to cope with hitting
               exactly the bottom of a stack block. It is possible
               to end up with the book keeping at the bottom of
               one block, and the opproc at the top of the previous
               block. If we pop everything in one go, the book keeping
               entries disappear, so we pop to the start of the book
               keeping values, call the cleanup, then pop the final
               entry.
             */
            op_proc_t opproc = real_opproc(ep);
            ref_stack_pop(&e_stack, idx - popped);
            esp--;
            (*opproc) (i_ctx_p);
            esp++;
            ref_stack_pop(&e_stack, 1);
            popped = idx + 1;
        }
    }
    ref_stack_pop(&e_stack, count - popped);
}

/*
 * Execute a quit in the case of an exit or stop with no appropriate
 * enclosing control scope (loop or stopped).  The caller has already
 * ensured two free slots on the top of the o-stack.
 */
static int
unmatched_exit(os_ptr op, op_proc_t opproc)
{
    make_oper(op - 1, 0, opproc);
    make_int(op, gs_error_invalidexit);
    return_error(gs_error_Quit);
}
