/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* Ghostscript language interpreter */
#include "memory_.h"
#include "string_.h"
#include "ghost.h"
#include "gsstruct.h"           /* for iastruct.h */
#include "gserrors.h"           /* for gpcheck.h */
#include "stream.h"
#include "ierrors.h"
#include "estack.h"
#include "ialloc.h"
#include "iastruct.h"
#include "icontext.h"
#include "icremap.h"
#include "idebug.h"
#include "igstate.h"            /* for handling gs_error_Remap_Color */
#include "inamedef.h"
#include "iname.h"              /* for the_name_table */
#include "interp.h"
#include "ipacked.h"
#include "ostack.h"             /* must precede iscan.h */
#include "strimpl.h"            /* for sfilter.h */
#include "sfilter.h"            /* for iscan.h */
#include "iscan.h"
#include "iddict.h"
#include "isave.h"
#include "istack.h"
#include "itoken.h"
#include "iutil.h"              /* for array_get */
#include "ivmspace.h"
#include "iinit.h"
#include "dstack.h"
#include "files.h"              /* for file_check_read */
#include "oper.h"
#include "store.h"
#include "gpcheck.h"
#define FORCE_ASSERT_CHECKING 1
#define DEBUG_TRACE_PS_OPERATORS 1
#include "assert_.h"

/*
 * We may or may not optimize the handling of the special fast operators
 * in packed arrays.  If we do this, they run much faster when packed, but
 * slightly slower when not packed.
 */
#define PACKED_SPECIAL_OPS 1

/*
 * Pseudo-operators (procedures of type t_oparray) record
 * the operand and dictionary stack pointers, and restore them if an error
 * occurs during the execution of the procedure and if the procedure hasn't
 * (net) decreased the depth of the stack.  While this obviously doesn't
 * do all the work of restoring the state if a pseudo-operator gets an
 * error, it's a big help.  The only downside is that pseudo-operators run
 * a little slower.
 */

/* GC descriptors for stacks */
extern_st(st_ref_stack);
public_st_dict_stack();
public_st_exec_stack();
public_st_op_stack();

/*
 * Apply an operator.  When debugging, we route all operator calls
 * through a procedure.
 */
#if defined(DEBUG_TRACE_PS_OPERATORS) || defined(DEBUG)
#define call_operator(proc, p) (*call_operator_fn)(proc, p)
static int
do_call_operator(op_proc_t op_proc, i_ctx_t *i_ctx_p)
{
    int code;
    assert(e_stack.p >= e_stack.bot - 1 && e_stack.p < e_stack.top + 1);
    assert(o_stack.p >= o_stack.bot - 1 && o_stack.p < o_stack.top + 1);
    code = op_proc(i_ctx_p);
    if (gs_debug_c(gs_debug_flag_validate_clumps))
        ivalidate_clean_spaces(i_ctx_p);
    assert(e_stack.p >= e_stack.bot - 1 && e_stack.p < e_stack.top + 1);
    assert(o_stack.p >= o_stack.bot - 1 && o_stack.p < o_stack.top + 1);
    return code; /* A good place for a conditional breakpoint. */
}
static int
do_call_operator_verbose(op_proc_t op_proc, i_ctx_t *i_ctx_p)
{
    int code;

#ifndef SHOW_STACK_DEPTHS
    if_debug1m('!', imemory, "[!]operator %s\n", op_get_name_string(op_proc));
#else
    if_debug3m('!', imemory, "[!][es=%d os=%d]operator %s\n",
            esp-i_ctx_p->exec_stack.stack.bot,
            osp-i_ctx_p->op_stack.stack.bot,
            op_get_name_string(op_proc));
#endif
    code = do_call_operator(op_proc, i_ctx_p);
    if (code < 0)
        if_debug1m('!', imemory, "[!]   error: %d\n", code);
#if defined(SHOW_STACK_DEPTHS)
    if_debug2m('!', imemory, "[!][es=%d os=%d]\n",
            esp-i_ctx_p->exec_stack.stack.bot,
            osp-i_ctx_p->op_stack.stack.bot);
#endif
    if (gs_debug_c(gs_debug_flag_validate_clumps))
        ivalidate_clean_spaces(i_ctx_p);
    return code; /* A good place for a conditional breakpoint. */
}
#else
#  define call_operator(proc, p) ((*(proc))(p))
#endif

/* Define debugging statistics (not threadsafe as uses globals) */
/* #define COLLECT_STATS_IDSTACK */

#ifdef COLLECT_STATS_INTERP
struct stats_interp_s {
    long top;
    long lit, lit_array, exec_array, exec_operator, exec_name;
    long x_add, x_def, x_dup, x_exch, x_if, x_ifelse,
        x_index, x_pop, x_roll, x_sub;
    long find_name, name_lit, name_proc, name_oparray, name_operator;
    long p_full, p_exec_operator, p_exec_oparray, p_exec_non_x_operator,
        p_integer, p_lit_name, p_exec_name;
    long p_find_name, p_name_lit, p_name_proc;
} stats_interp;
# define INCR(v) (++(stats_interp.v))
#else
# define INCR(v) DO_NOTHING
#endif

/* Forward references */
static int estack_underflow(i_ctx_t *);
static int interp(i_ctx_t **, const ref *, ref *);
static int interp_exit(i_ctx_t *);
static int zforceinterp_exit(i_ctx_t *i_ctx_p);
static void set_gc_signal(i_ctx_t *, int);
static int copy_stack(i_ctx_t *, const ref_stack_t *, int skip, ref *);
static int oparray_pop(i_ctx_t *);
static int oparray_cleanup(i_ctx_t *);
static int zerrorexec(i_ctx_t *);
static int zfinderrorobject(i_ctx_t *);
static int errorexec_pop(i_ctx_t *);
static int errorexec_cleanup(i_ctx_t *);
static int zsetstackprotect(i_ctx_t *);
static int zcurrentstackprotect(i_ctx_t *);
static int zactonuel(i_ctx_t *);

/* Stack sizes */

/* The maximum stack sizes may all be set in the makefile. */

/*
 * Define the initial maximum size of the operand stack (MaxOpStack
 * user parameter).
 */
#ifndef MAX_OSTACK
#  define MAX_OSTACK 800
#endif
/*
 * The minimum block size for extending the operand stack is the larger of:
 *      - the maximum number of parameters to an operator
 *      (currently setcolorscreen, with 12 parameters);
 *      - the maximum number of values pushed by an operator
 *      (currently setcolortransfer, which calls zcolor_remap_one 4 times
 *      and therefore pushes 16 values).
 */
#define MIN_BLOCK_OSTACK 16
const int gs_interp_max_op_num_args = MIN_BLOCK_OSTACK;         /* for iinit.c */

/*
 * Define the initial maximum size of the execution stack (MaxExecStack
 * user parameter).
 */
#ifndef MAX_ESTACK
#  define MAX_ESTACK 5000
#endif
/*
 * The minimum block size for extending the execution stack is the largest
 * size of a contiguous block surrounding an e-stack mark.  (At least,
 * that's what the minimum value would be if we supported multi-block
 * estacks, which we currently don't.)  Currently, the largest such block is
 * the one created for text processing, which is 8 (snumpush) slots.
 */
#define MIN_BLOCK_ESTACK 8
/*
 * If we get an e-stack overflow, we need to cut it back far enough to
 * have some headroom for executing the error procedure.
 */
#define ES_HEADROOM 20

/*
 * Define the initial maximum size of the dictionary stack (MaxDictStack
 * user parameter).  Again, this is also currently the block size for
 * extending the d-stack.
 */
#ifndef MAX_DSTACK
#  define MAX_DSTACK 20
#endif
/*
 * The minimum block size for extending the dictionary stack is the number
 * of permanent entries on the dictionary stack, currently 3.
 */
#define MIN_BLOCK_DSTACK 3

/* See estack.h for a description of the execution stack. */

/* The logic for managing icount and iref below assumes that */
/* there are no control operators which pop and then push */
/* information on the execution stack. */

/* Stacks */
extern_st(st_ref_stack);
#define OS_GUARD_UNDER 10
#define OS_GUARD_OVER 10
#define OS_REFS_SIZE(body_size)\
  (stack_block_refs + OS_GUARD_UNDER + (body_size) + OS_GUARD_OVER)

#define ES_GUARD_UNDER 1
#define ES_GUARD_OVER 10
#define ES_REFS_SIZE(body_size)\
  (stack_block_refs + ES_GUARD_UNDER + (body_size) + ES_GUARD_OVER)

#define DS_REFS_SIZE(body_size)\
  (stack_block_refs + (body_size))

/* Extended types.  The interpreter may replace the type of operators */
/* in procedures with these, to speed up the interpretation loop. */
/****** NOTE: If you add or change entries in this list, */
/****** you must change the three dispatches in the interpreter loop. */
/* The operator procedures are declared in opextern.h. */
#define tx_op t_next_index
typedef enum {
    tx_op_add = tx_op,
    tx_op_def,
    tx_op_dup,
    tx_op_exch,
    tx_op_if,
    tx_op_ifelse,
    tx_op_index,
    tx_op_pop,
    tx_op_roll,
    tx_op_sub,
    tx_next_op
} special_op_types;

#define num_special_ops ((int)tx_next_op - tx_op)
const int gs_interp_num_special_ops = num_special_ops;  /* for iinit.c */
const int tx_next_index = tx_next_op;

/*
 * NOTE: if the size of either table below ever exceeds 15 real entries, it
 * will have to be split.
 */
/* Define the extended-type operators per the list above. */
const op_def interp1_op_defs[] = {
    /*
     * The very first entry, which corresponds to operator index 0,
     * must not contain an actual operator.
     */
    op_def_begin_dict("systemdict"),
    {"2add", zadd},
    {"2def", zdef},
    {"1dup", zdup},
    {"2exch", zexch},
    {"2if", zif},
    {"3ifelse", zifelse},
    {"1index", zindex},
    {"1pop", zpop},
    {"2roll", zroll},
    {"2sub", zsub},
    op_def_end(0)
};
/* Define the internal interpreter operators. */
const op_def interp2_op_defs[] = {
    {"0.currentstackprotect", zcurrentstackprotect},
    {"1.setstackprotect", zsetstackprotect},
    {"2.errorexec", zerrorexec},
    {"0.finderrorobject", zfinderrorobject},
    {"0%interp_exit", interp_exit},
    {"0.forceinterp_exit", zforceinterp_exit},
    {"0%oparray_pop", oparray_pop},
    {"0%errorexec_pop", errorexec_pop},
    {"0.actonuel", zactonuel},
    op_def_end(0)
};

#define make_null_proc(pref)\
  make_empty_const_array(pref, a_executable + a_readonly)

/* Initialize the interpreter. */
int
gs_interp_init(i_ctx_t **pi_ctx_p, const ref *psystem_dict,
               gs_dual_memory_t *dmem)
{
    /* Create and initialize a context state. */
    gs_context_state_t *pcst = 0;
    int code = context_state_alloc(&pcst, psystem_dict, dmem);
    if (code >= 0) {
        code = context_state_load(pcst);
        if (code < 0) {
            context_state_free(pcst);
            pcst = NULL;
        }
    }

    if (code < 0)
        lprintf1("Fatal error %d in gs_interp_init!\n", code);
    *pi_ctx_p = pcst;

    return code;
}
/*
 * Create initial stacks for the interpreter.
 * We export this for creating new contexts.
 */
int
gs_interp_alloc_stacks(gs_ref_memory_t *mem, gs_context_state_t * pcst)
{
    int code;
    gs_ref_memory_t *smem =
        (gs_ref_memory_t *)gs_memory_stable((gs_memory_t *)mem);
    ref stk;

#define REFS_SIZE_OSTACK OS_REFS_SIZE(MAX_OSTACK)
#define REFS_SIZE_ESTACK ES_REFS_SIZE(MAX_ESTACK)
#define REFS_SIZE_DSTACK DS_REFS_SIZE(MAX_DSTACK)
    code = gs_alloc_ref_array(smem, &stk, 0,
                       REFS_SIZE_OSTACK + REFS_SIZE_ESTACK +
                       REFS_SIZE_DSTACK, "gs_interp_alloc_stacks");
    if (code < 0)
        return code;

    {
        ref_stack_t *pos = &pcst->op_stack.stack;

        r_set_size(&stk, REFS_SIZE_OSTACK);
        code = ref_stack_init(pos, &stk, OS_GUARD_UNDER, OS_GUARD_OVER, NULL,
                              smem, NULL);
	if (code < 0)
            return code;
        ref_stack_set_error_codes(pos, gs_error_stackunderflow, gs_error_stackoverflow);
        ref_stack_set_max_count(pos, MAX_OSTACK);
        stk.value.refs += REFS_SIZE_OSTACK;
    }

    {
        ref_stack_t *pes = &pcst->exec_stack.stack;
        ref euop;

        r_set_size(&stk, REFS_SIZE_ESTACK);
        make_oper(&euop, 0, estack_underflow);
        code = ref_stack_init(pes, &stk, ES_GUARD_UNDER, ES_GUARD_OVER, &euop,
                       smem, NULL);
	if (code < 0)
            return code;
        ref_stack_set_error_codes(pes, gs_error_ExecStackUnderflow,
                                  gs_error_execstackoverflow);
        /**************** E-STACK EXPANSION IS NYI. ****************/
        ref_stack_allow_expansion(pes, false);
        ref_stack_set_max_count(pes, MAX_ESTACK);
        stk.value.refs += REFS_SIZE_ESTACK;
    }

    {
        ref_stack_t *pds = &pcst->dict_stack.stack;

        r_set_size(&stk, REFS_SIZE_DSTACK);
        code = ref_stack_init(pds, &stk, 0, 0, NULL, smem, NULL);
        if (code < 0)
            return code;
        ref_stack_set_error_codes(pds, gs_error_dictstackunderflow,
                                  gs_error_dictstackoverflow);
        ref_stack_set_max_count(pds, MAX_DSTACK);
    }

#undef REFS_SIZE_OSTACK
#undef REFS_SIZE_ESTACK
#undef REFS_SIZE_DSTACK
    return 0;
}
/*
 * Free the stacks when destroying a context.  This is the inverse of
 * create_stacks.
 */
void
gs_interp_free_stacks(gs_ref_memory_t * smem, gs_context_state_t * pcst)
{
    /* Free the stacks in inverse order of allocation. */
    ref_stack_release(&pcst->dict_stack.stack);
    ref_stack_release(&pcst->exec_stack.stack);
    ref_stack_release(&pcst->op_stack.stack);
}
void
gs_interp_reset(i_ctx_t *i_ctx_p)
{   /* Reset the stacks. */
    ref_stack_clear(&o_stack);
    ref_stack_clear(&e_stack);
    esp++;
    make_oper(esp, 0, interp_exit);
    ref_stack_pop_to(&d_stack, min_dstack_size);
    dict_set_top();
}
/* Report an e-stack block underflow.  The bottom guard slots of */
/* e-stack blocks contain a pointer to this procedure. */
static int
estack_underflow(i_ctx_t *i_ctx_p)
{
    return gs_error_ExecStackUnderflow;
}

/*
 * Create an operator during initialization.
 * If operator is hard-coded into the interpreter,
 * assign it a special type and index.
 */
void
gs_interp_make_oper(ref * opref, op_proc_t proc, int idx)
{
    int i;

    for (i = num_special_ops; i > 0 && proc != interp1_op_defs[i].proc; --i)
        DO_NOTHING;
    if (i > 0)
        make_tasv(opref, tx_op + (i - 1), a_executable, i, opproc, proc);
    else
        make_tasv(opref, t_operator, a_executable, idx, opproc, proc);
}

/*
 * Call the garbage collector, updating the context pointer properly.
 */
int
interp_reclaim(i_ctx_t **pi_ctx_p, int space)
{
    i_ctx_t *i_ctx_p = *pi_ctx_p;
    gs_gc_root_t ctx_root, *r = &ctx_root;
    int code;

#ifdef DEBUG
    if (gs_debug_c(gs_debug_flag_gc_disable))
        return 0;
#endif

    gs_register_struct_root(imemory_system, &r,
                            (void **)pi_ctx_p, "interp_reclaim(pi_ctx_p)");
    code = (*idmemory->reclaim)(idmemory, space);
    i_ctx_p = *pi_ctx_p;        /* may have moved */
    gs_unregister_root(imemory_system, r, "interp_reclaim(pi_ctx_p)");
    return code;
}

/*
 * Invoke the interpreter.  If execution completes normally, return 0.
 * If an error occurs, the action depends on user_errors as follows:
 *    user_errors < 0: always return an error code.
 *    user_errors >= 0: let the PostScript machinery handle all errors.
 *      (This will eventually result in a fatal error if no 'stopped'
 *      is active.)
 * In case of a quit or a fatal error, also store the exit code.
 * Set *perror_object to null or the error object.
 */
static int gs_call_interp(i_ctx_t **, ref *, int, int *, ref *);
int
gs_interpret(i_ctx_t **pi_ctx_p, ref * pref, int user_errors, int *pexit_code,
             ref * perror_object)
{
    i_ctx_t *i_ctx_p = *pi_ctx_p;
    gs_gc_root_t error_root, *r = &error_root;
    int code;

    gs_register_ref_root(imemory_system, &r,
                         (void **)&perror_object, "gs_interpret");
    code = gs_call_interp(pi_ctx_p, pref, user_errors, pexit_code,
                          perror_object);
    i_ctx_p = *pi_ctx_p;
    gs_unregister_root(imemory_system, &error_root, "gs_interpret");
    /* Avoid a dangling reference to the lib context GC signal. */
    set_gc_signal(i_ctx_p, 0);
    return code;
}
static int
gs_call_interp(i_ctx_t **pi_ctx_p, ref * pref, int user_errors,
               int *pexit_code, ref * perror_object)
{
    ref *epref = pref;
    ref doref;
    ref *perrordict;
    ref error_name;
    int code, ccode;
    ref saref;
    i_ctx_t *i_ctx_p = *pi_ctx_p;
    int *gc_signal = &imemory_system->gs_lib_ctx->gcsignal;

    *pexit_code = 0;
    *gc_signal = 0;

    /* This avoids a valgrind error */
    doref.tas.type_attrs = error_name.tas.type_attrs = saref.tas.type_attrs = 0;

    ialloc_reset_requested(idmemory);
again:
    /* Avoid a dangling error object that might get traced by a future GC. */
    make_null(perror_object);
    o_stack.requested = e_stack.requested = d_stack.requested = 0;
    while (*gc_signal) { /* Some routine below triggered a GC. */
        gs_gc_root_t epref_root, *r = &epref_root;

        *gc_signal = 0;
        /* Make sure that doref will get relocated properly if */
        /* a garbage collection happens with epref == &doref. */
        gs_register_ref_root(imemory_system, &r,
                             (void **)&epref, "gs_call_interp(epref)");
        code = interp_reclaim(pi_ctx_p, -1);
        i_ctx_p = *pi_ctx_p;
        gs_unregister_root(imemory_system, &epref_root,
                           "gs_call_interp(epref)");
        if (code < 0)
            return code;
    }
    code = interp(pi_ctx_p, epref, perror_object);
    i_ctx_p = *pi_ctx_p;
    if (!r_has_type(&i_ctx_p->error_object, t__invalid)) {
        *perror_object = i_ctx_p->error_object;
        make_t(&i_ctx_p->error_object, t__invalid);
    }
    /* Prevent a dangling reference to the GC signal in ticks_left */
    /* in the frame of interp, but be prepared to do a GC if */
    /* an allocation in this routine asks for it. */
    *gc_signal = 0;
    set_gc_signal(i_ctx_p, 1);
    if (esp < esbot)            /* popped guard entry */
        esp = esbot;
    switch (code) {
        case gs_error_Fatal:
            *pexit_code = 255;
            return code;
        case gs_error_Quit:
            *perror_object = osp[-1];
            *pexit_code = code = osp->value.intval;
            osp -= 2;
            return
                (code == 0 ? gs_error_Quit :
                 code < 0 && code > -100 ? code : gs_error_Fatal);
        case gs_error_InterpreterExit:
            return 0;
        case gs_error_ExecStackUnderflow:
/****** WRONG -- must keep mark blocks intact ******/
            ref_stack_pop_block(&e_stack);
            doref = *perror_object;
            epref = &doref;
            goto again;
        case gs_error_VMreclaim:
            /* Do the GC and continue. */
            /* We ignore the return value here, if it fails here
             * we'll call it again having jumped to the "again" label.
             * Where, assuming it fails again, we'll handle the error.
             */
            (void)interp_reclaim(pi_ctx_p,
                                  (osp->value.intval == 2 ?
                                   avm_global : avm_local));
            i_ctx_p = *pi_ctx_p;
            make_oper(&doref, 0, zpop);
            epref = &doref;
            goto again;
        case gs_error_NeedInput:
        case gs_error_interrupt:
            return code;
    }
    /* Adjust osp in case of operand stack underflow */
    if (osp < osbot - 1)
        osp = osbot - 1;
    /* We have to handle stack over/underflow specially, because */
    /* we might be able to recover by adding or removing a block. */
    switch (code) {
        case gs_error_dictstackoverflow:
            /* We don't have to handle this specially: */
            /* The only places that could generate it */
            /* use check_dstack, which does a ref_stack_extend, */
            /* so if` we get this error, it's a real one. */
            if (osp >= ostop) {
                if ((ccode = ref_stack_extend(&o_stack, 1)) < 0)
                    return ccode;
            }
            /* Skip system dictionaries for CET 20-02-02 */
            ccode = copy_stack(i_ctx_p, &d_stack, min_dstack_size, &saref);
            if (ccode < 0)
                return ccode;
            ref_stack_pop_to(&d_stack, min_dstack_size);
            dict_set_top();
            *++osp = saref;
            break;
        case gs_error_dictstackunderflow:
            if (ref_stack_pop_block(&d_stack) >= 0) {
                dict_set_top();
                doref = *perror_object;
                epref = &doref;
                goto again;
            }
            break;
        case gs_error_execstackoverflow:
            /* We don't have to handle this specially: */
            /* The only places that could generate it */
            /* use check_estack, which does a ref_stack_extend, */
            /* so if we get this error, it's a real one. */
            if (osp >= ostop) {
                if ((ccode = ref_stack_extend(&o_stack, 1)) < 0)
                    return ccode;
            }
            ccode = copy_stack(i_ctx_p, &e_stack, 0, &saref);
            if (ccode < 0)
                return ccode;
            {
                uint count = ref_stack_count(&e_stack);
                uint limit = ref_stack_max_count(&e_stack) - ES_HEADROOM;

                if (count > limit) {
                    /*
                     * If there is an e-stack mark within MIN_BLOCK_ESTACK of
                     * the new top, cut the stack back to remove the mark.
                     */
                    int skip = count - limit;
                    int i;

                    for (i = skip; i < skip + MIN_BLOCK_ESTACK; ++i) {
                        const ref *ep = ref_stack_index(&e_stack, i);

                        if (ep == NULL)
                            continue;

                        if (r_has_type_attrs(ep, t_null, a_executable)) {
                            skip = i + 1;
                            break;
                        }
                    }
                    pop_estack(i_ctx_p, skip);
                }
            }
            *++osp = saref;
            break;
        case gs_error_stackoverflow:
            if (ref_stack_extend(&o_stack, o_stack.requested) >= 0) {   /* We can't just re-execute the object, because */
                /* it might be a procedure being pushed as a */
                /* literal.  We check for this case specially. */
                doref = *perror_object;
                if (r_is_proc(&doref)) {
                    *++osp = doref;
                    make_null_proc(&doref);
                }
                epref = &doref;
                goto again;
            }
            ccode = copy_stack(i_ctx_p, &o_stack, 0, &saref);
            if (ccode < 0)
                return ccode;
            ref_stack_clear(&o_stack);
            *++osp = saref;
            break;
        case gs_error_stackunderflow:
            if (ref_stack_pop_block(&o_stack) >= 0) {
                doref = *perror_object;
                epref = &doref;
                goto again;
            }
            break;
    }
    if (user_errors < 0)
        return code;
    if (gs_errorname(i_ctx_p, code, &error_name) < 0)
        return code;            /* out-of-range error code! */

    /*  We refer to gserrordict first, which is not accessible to Postcript jobs
     *  If we're running with SAFERERRORS all the handlers are copied to gserrordict
     *  so we'll always find the default one. If not SAFERERRORS, only gs specific
     *  errors are in gserrordict.
     */
    if ((dict_find_string(systemdict, "gserrordict", &perrordict) <= 0 ||
        !r_has_type(perrordict, t_dictionary)                          ||
        dict_find(perrordict, &error_name, &epref) <= 0)               &&
       (dict_find_string(systemdict, "errordict", &perrordict) <= 0    ||
        !r_has_type(perrordict, t_dictionary)                          ||
        dict_find(perrordict, &error_name, &epref) <= 0))
        return code;            /* error name not in errordict??? */

    if (code == gs_error_execstackoverflow
        && obj_eq(imemory, &doref, epref)) {
        /* This strongly suggests we're in an error handler that
           calls itself infinitely, so Postscript is done, return
           to the caller.
         */
         ref_stack_clear(&e_stack);
         *pexit_code = gs_error_execstackoverflow;
         return_error(gs_error_execstackoverflow);
    }

    if (!r_is_proc(epref)){
        *pexit_code = gs_error_Fatal;
        return_error(gs_error_Fatal);
    }

    doref = *epref;
    epref = &doref;
    /* Push the error object on the operand stack if appropriate. */
    if (!GS_ERROR_IS_INTERRUPT(code)) {
        byte buf[260], *bufptr;
        uint rlen;
        /* Replace the error object if within an oparray or .errorexec. */
        osp++;
        if (osp >= ostop) {
            *pexit_code = gs_error_Fatal;
            return_error(gs_error_Fatal);
        }
        *osp = *perror_object;
        errorexec_find(i_ctx_p, osp);

        if (!r_has_type(osp, t_string) && !r_has_type(osp, t_name)) {
            code = obj_cvs(imemory, osp, buf + 2, 256, &rlen, (const byte **)&bufptr);
            if (code < 0) {
                const char *unknownstr = "--unknown--";
                rlen = strlen(unknownstr);
                memcpy(buf, unknownstr, rlen);
                bufptr = buf;
            }
            else {
                ref *tobj;
                bufptr[rlen] = '\0';
                /* Only pass a name object if the operator doesn't exist in systemdict
                 * i.e. it's an internal operator we have hidden
                 */
                code = dict_find_string(systemdict, (const char *)bufptr, &tobj);
                if (code <= 0) {
                    buf[0] = buf[1] = buf[rlen + 2] = buf[rlen + 3] = '-';
                    rlen += 4;
                    bufptr = buf;
                }
                else {
                    bufptr = NULL;
                }
            }
            if (bufptr) {
                code = name_ref(imemory, buf, rlen, osp, 1);
                if (code < 0)
                    make_null(osp);
            }
        }
    }
    goto again;
}
static int
interp_exit(i_ctx_t *i_ctx_p)
{
    return gs_error_InterpreterExit;
}

/* Only used (currently) with language switching:
 * allows the PS interpreter to co-exist with the
 * PJL interpreter.
 */
static int
zforceinterp_exit(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    stream *s;

    check_file(s, op);
    i_ctx_p->uel_position = stell(s)-1;
    /* resetfile */
    if (file_is_valid(s, op))
        sreset(s);

    if (!gs_lib_ctx_get_act_on_uel((gs_memory_t *)(i_ctx_p->memory.current)))
        return 0;

    gs_interp_reset(i_ctx_p);
    /* gs_interp_reset() actually leaves the op stack one entry below
     * the bottom of the stack, and that can cause problems depending
     * on the interpreter state at the end of the job.
     * So push a null object, and the return code before continuing.
     */
    push(2);
    op = osp;
    make_null(op - 1);
    make_int(op, gs_error_InterpreterExit);
    return_error(gs_error_Quit);
}

/* Set the GC signal for all VMs. */
static void
set_gc_signal(i_ctx_t *i_ctx_p, int value)
{
    gs_memory_gc_status_t stat;
    int i;

    for (i = 0; i < countof(idmemory->spaces_indexed); i++) {
        gs_ref_memory_t *mem = idmemory->spaces_indexed[i];
        gs_ref_memory_t *mem_stable;

        if (mem == 0)
            continue;
        for (;; mem = mem_stable) {
            mem_stable = (gs_ref_memory_t *)
                gs_memory_stable((gs_memory_t *)mem);
            gs_memory_gc_status(mem, &stat);
            stat.signal_value = value;
            gs_memory_set_gc_status(mem, &stat);
            if (mem_stable == mem)
                break;
        }
    }
}

/* Create a printable string ref (or null) from an arbitrary ref.
 * For the purpose this is used here, it cannot fail, any
 * error in the process results in a null object, instead
 * of the string.
 */
static void obj_cvs_ref(i_ctx_t *i_ctx_p, const ref *in, ref *out)
{
    uint rlen;
    int code;
    byte sbuf[65], *buf = sbuf;
    uint len = sizeof(sbuf) - 1;

    code = obj_cvs(imemory, in, buf, len, &rlen, NULL);
    if (code == gs_error_rangecheck) {
        len = rlen;
        buf = gs_alloc_bytes(imemory, len + 1, "obj_cvs_ref");
        if (!buf)
            code = -1;
        else
            code = obj_cvs(imemory, in, buf, len, &rlen, NULL);
    }
    if (code < 0) {
        make_null(out);
    }
    else {
        buf[rlen] = '\0';
        code = string_to_ref((const char *)buf, out, iimemory, "obj_cvs_ref");
        if (code < 0)
            make_null(out);
    }
    if (buf != sbuf)
        gs_free_object(imemory, buf, "obj_cvs_ref");
    return;
}

/* Copy top elements of an overflowed stack into a (local) array. */
/* Adobe copies only 500 top elements, we copy up to 65535 top elements */
/* for better debugging, PLRM compliance, and backward compatibility. */
static int
copy_stack(i_ctx_t *i_ctx_p, const ref_stack_t * pstack, int skip, ref * arr)
{
    uint size = ref_stack_count(pstack) - skip;
    uint save_space = ialloc_space(idmemory);
    int code, i;
    ref *safety, *safe;

    if (size > 65535)
        size = 65535;
    ialloc_set_space(idmemory, avm_local);
    code = ialloc_ref_array(arr, a_all, size, "copy_stack");
    if (code >= 0)
        code = ref_stack_store(pstack, arr, size, 0, 1, true, idmemory,
                               "copy_stack");
    /* If we are copying the exec stack, try to replace any oparrays with
     * the operator that references them
     * We also replace any internal objects (t_struct and t_astruct) with
     * string representations, since these can contain references to objects
     * with uncertain lifespans, it is safer not to risk them persisting.
     * Since we basically did this later on for the error handler, it isn't
     * a significant speed hit.
     */
    if (pstack == &e_stack) {
        for (i = 0; i < size; i++) {
            if (errorexec_find(i_ctx_p, &arr->value.refs[i]) < 0)
                make_null(&arr->value.refs[i]);
            else if (r_has_type(&arr->value.refs[i], t_struct)
                  || r_has_type(&arr->value.refs[i], t_astruct)) {
                ref r;
                obj_cvs_ref(i_ctx_p, (const ref *)&arr->value.refs[i], &r);
                ref_assign(&arr->value.refs[i], &r);
            }
        }
    }
    if (pstack == &o_stack && dict_find_string(systemdict, "SAFETY", &safety) > 0 &&
        dict_find_string(safety, "safe", &safe) > 0 && r_has_type(safe, t_boolean) &&
        safe->value.boolval == true) {
        code = ref_stack_array_sanitize(i_ctx_p, arr, arr, 0);
        if (code < 0)
            return code;
    }
    ialloc_set_space(idmemory, save_space);
    return code;
}

/* Get the name corresponding to an error number. */
int
gs_errorname(i_ctx_t *i_ctx_p, int code, ref * perror_name)
{
    ref *perrordict, *pErrorNames;

    if (dict_find_string(systemdict, "errordict", &perrordict) <= 0 ||
        dict_find_string(systemdict, "ErrorNames", &pErrorNames) <= 0
        )
        return_error(gs_error_undefined);      /* errordict or ErrorNames not found?! */
    return array_get(imemory, pErrorNames, (long)(-code - 1), perror_name);
}

/* Store an error string in $error.errorinfo. */
/* This routine is here because of the proximity to the error handler. */
int
gs_errorinfo_put_string(i_ctx_t *i_ctx_p, const char *str)
{
    ref rstr;
    ref *pderror;
    int code = string_to_ref(str, &rstr, iimemory, "gs_errorinfo_put_string");

    if (code < 0)
        return code;
    if (dict_find_string(systemdict, "$error", &pderror) <= 0 ||
        !r_has_type(pderror, t_dictionary) ||
        idict_put_string(pderror, "errorinfo", &rstr) < 0
        )
        return_error(gs_error_Fatal);
    return 0;
}

/* Main interpreter. */
/* If execution terminates normally, return gs_error_InterpreterExit. */
/* If an error occurs, leave the current object in *perror_object */
/* and return a (negative) error code. */
static int
interp(/* lgtm [cpp/use-of-goto] */
       i_ctx_t **pi_ctx_p /* context for execution, updated if resched */,
       const ref * pref /* object to interpret */,
       ref * perror_object)
{
    i_ctx_t *i_ctx_p = *pi_ctx_p;
    /*
     * Note that iref may actually be either a ref * or a ref_packed *.
     * Certain DEC compilers assume that a ref * is ref-aligned even if it
     * is cast to a short *, and generate code on this assumption, leading
     * to "unaligned access" errors.  For this reason, we declare
     * iref_packed, and use a macro to cast it to the more aligned type
     * where necessary (which is almost everywhere it is used).  This may
     * lead to compiler warnings about "cast increases alignment
     * requirements", but this is less harmful than expensive traps at run
     * time.
     */
    register const ref_packed *iref_packed = (const ref_packed *)pref;
    /*
     * To make matters worse, some versions of gcc/egcs have a bug that
     * leads them to assume that if iref_packed is EVER cast to a ref *,
     * it is ALWAYS ref-aligned.  We detect this in stdpre.h and provide
     * the following workaround:
     */
#ifdef ALIGNMENT_ALIASING_BUG
    const ref *iref_temp;
#  define IREF (iref_temp = (const ref *)iref_packed, iref_temp)
#else
#  define IREF ((const ref *)iref_packed)
#endif
#define SET_IREF(rp) (iref_packed = (const ref_packed *)(rp))
    register int icount = 0;    /* # of consecutive tokens at iref */
    register os_ptr iosp = osp; /* private copy of osp */
    register es_ptr iesp = esp; /* private copy of esp */
    int code;
    ref token;                  /* token read from file or string, */
                                /* must be declared in this scope */
    ref *pvalue;
    ref refnull;
    uint opindex;               /* needed for oparrays */
    os_ptr whichp;

    /*
     * We have to make the error information into a struct;
     * otherwise, the Watcom compiler will assign it to registers
     * strictly on the basis of textual frequency.
     * We also have to use ref_assign_inline everywhere, and
     * avoid direct assignments of refs, so that esi and edi
     * will remain available on Intel processors.
     */
    struct interp_error_s {
        int code;
        int line;
        const ref *obj;
        ref full;
    } ierror;

    /*
     * Get a pointer to the name table so that we can use the
     * inline version of name_index_ref.
     */
    const name_table *const int_nt = imemory->gs_lib_ctx->gs_name_table;

#define set_error(ecode)\
  { ierror.code = ecode; ierror.line = __LINE__; }
#define return_with_error(ecode, objp)\
  { set_error(ecode); ierror.obj = objp; goto rwe; }
#define return_with_error_iref(ecode)\
  { set_error(ecode); goto rwei; }
#define return_with_code_iref()\
  { ierror.line = __LINE__; goto rweci; }
#define return_with_stackoverflow(objp)\
  { o_stack.requested = 1; return_with_error(gs_error_stackoverflow, objp); }
#define return_with_stackoverflow_iref()\
  { o_stack.requested = 1; return_with_error_iref(gs_error_stackoverflow); }
/*
 * If control reaches the special operators (x_add, etc.) as a result of
 * interpreting an executable name, iref points to the name, not the
 * operator, so the name rather than the operator becomes the error object,
 * which is wrong.  We detect and handle this case explicitly when an error
 * occurs, so as not to slow down the non-error case.
 */
#define return_with_error_tx_op(err_code)\
  { if (r_has_type(IREF, t_name)) {\
        return_with_error(err_code, pvalue);\
    } else {\
        return_with_error_iref(err_code);\
    }\
  }

    int *ticks_left = &imemory_system->gs_lib_ctx->gcsignal;

#if defined(DEBUG_TRACE_PS_OPERATORS) || defined(DEBUG)
    int (*call_operator_fn)(op_proc_t, i_ctx_t *) = do_call_operator;

    if (gs_debug_c('!'))
        call_operator_fn = do_call_operator_verbose;
#endif

    *ticks_left = i_ctx_p->time_slice_ticks;

     make_null(&ierror.full);
     ierror.obj = &ierror.full;
     make_null(&refnull);
     refnull.value.intval = 0;
     pvalue = &refnull;

    /*
     * If we exceed the VMThreshold, set *ticks_left to -100
     * to alert the interpreter that we need to garbage collect.
     */
    set_gc_signal(i_ctx_p, -100);

    esfile_clear_cache();
    /*
     * From here on, if icount > 0, iref and icount correspond
     * to the top entry on the execution stack: icount is the count
     * of sequential entries remaining AFTER the current one.
     */
#define IREF_NEXT(ip)\
  ((const ref_packed *)((const ref *)(ip) + 1))
#define IREF_NEXT_EITHER(ip)\
  ( r_is_packed(ip) ? (ip) + 1 : IREF_NEXT(ip) )
#define store_state(ep)\
  ( icount > 0 ? (ep->value.const_refs = IREF + 1, r_set_size(ep, icount)) : 0 )
#define store_state_short(ep)\
  ( icount > 0 ? (ep->value.packed = iref_packed + 1, r_set_size(ep, icount)) : 0 )
#define store_state_either(ep)\
  ( icount > 0 ? (ep->value.packed = IREF_NEXT_EITHER(iref_packed), r_set_size(ep, icount)) : 0 )
#define next()\
  if ( --icount > 0 ) { iref_packed = IREF_NEXT(iref_packed); goto top; } else goto out
#define next_short()\
  if ( --icount <= 0 ) { if ( icount < 0 ) goto up; iesp--; }\
  ++iref_packed; goto top
#define next_either()\
  if ( --icount <= 0 ) { if ( icount < 0 ) goto up; iesp--; }\
  iref_packed = IREF_NEXT_EITHER(iref_packed); goto top

#if !PACKED_SPECIAL_OPS
#  undef next_either
#  define next_either() next()
#  undef store_state_either
#  define store_state_either(ep) store_state(ep)
#endif

    /* We want to recognize executable arrays here, */
    /* so we push the argument on the estack and enter */
    /* the loop at the bottom. */
    if (iesp >= estop)
        return_with_error(gs_error_execstackoverflow, pref);
    ++iesp;
    ref_assign_inline(iesp, pref);
    goto bot;
  top:
        /*
         * This is the top of the interpreter loop.
         * iref points to the ref being interpreted.
         * Note that this might be an element of a packed array,
         * not a real ref: we carefully arranged the first 16 bits of
         * a ref and of a packed array element so they could be distinguished
         * from each other.  (See ghost.h and packed.h for more detail.)
         */
    INCR(top);
#ifdef DEBUG
    /* Do a little validation on the top o-stack entry. */
    if (iosp >= osbot &&
        (r_type(iosp) == t__invalid || r_type(iosp) >= tx_next_op)
        ) {
        mlprintf(imemory, "Invalid value on o-stack!\n");
        return_with_error_iref(gs_error_Fatal);
    }
    if (gs_debug['I'] ||
        (gs_debug['i'] &&
         (r_is_packed(iref_packed) ?
          r_packed_is_name(iref_packed) :
          r_has_type(IREF, t_name)))
        ) {
        os_ptr save_osp = osp;  /* avoid side-effects */
        es_ptr save_esp = esp;

        osp = iosp;
        esp = iesp;
        dmlprintf5(imemory, "d%u,e%u<%u>"PRI_INTPTR"(%d): ",
                  ref_stack_count(&d_stack), ref_stack_count(&e_stack),
                  ref_stack_count(&o_stack), (intptr_t)IREF, icount);
        debug_print_ref(imemory, IREF);
        if (iosp >= osbot) {
            dmputs(imemory, " // ");
            debug_print_ref(imemory, iosp);
        }
        dmputc(imemory, '\n');
        osp = save_osp;
        esp = save_esp;
        dmflush(imemory);
    }
#endif
/* Objects that have attributes (arrays, dictionaries, files, and strings) */
/* use lit and exec; other objects use plain and plain_exec. */
#define lit(t) type_xe_value(t, a_execute)
#define exec(t) type_xe_value(t, a_execute + a_executable)
#define nox(t) type_xe_value(t, 0)
#define nox_exec(t) type_xe_value(t, a_executable)
#define plain(t) type_xe_value(t, 0)
#define plain_exec(t) type_xe_value(t, a_executable)
    /*
     * We have to populate enough cases of the switch statement to force
     * some compilers to use a dispatch rather than a testing loop.
     * What a nuisance!
     */
    switch (r_type_xe(iref_packed)) {
            /* Access errors. */
#define cases_invalid()\
  case plain(t__invalid): case plain_exec(t__invalid)
          cases_invalid():
            return_with_error_iref(gs_error_Fatal);
#define cases_nox()\
  case nox_exec(t_array): case nox_exec(t_dictionary):\
  case nox_exec(t_file): case nox_exec(t_string):\
  case nox_exec(t_mixedarray): case nox_exec(t_shortarray)
          cases_nox():
            return_with_error_iref(gs_error_invalidaccess);
            /*
             * Literal objects.  We have to enumerate all the types.
             * In fact, we have to include some extra plain_exec entries
             * just to populate the switch.  We break them up into groups
             * to avoid overflowing some preprocessors.
             */
#define cases_lit_1()\
  case lit(t_array): case nox(t_array):\
  case plain(t_boolean): case plain_exec(t_boolean):\
  case lit(t_dictionary): case nox(t_dictionary)
#define cases_lit_2()\
  case lit(t_file): case nox(t_file):\
  case plain(t_fontID): case plain_exec(t_fontID):\
  case plain(t_integer): case plain_exec(t_integer):\
  case plain(t_mark): case plain_exec(t_mark)
#define cases_lit_3()\
  case plain(t_name):\
  case plain(t_null):\
  case plain(t_oparray):\
  case plain(t_operator)
#define cases_lit_4()\
  case plain(t_real): case plain_exec(t_real):\
  case plain(t_save): case plain_exec(t_save):\
  case lit(t_string): case nox(t_string)
#define cases_lit_5()\
  case lit(t_mixedarray): case nox(t_mixedarray):\
  case lit(t_shortarray): case nox(t_shortarray):\
  case plain(t_device): case plain_exec(t_device):\
  case plain(t_struct): case plain_exec(t_struct):\
  case plain(t_astruct): case plain_exec(t_astruct):\
  case plain(t_pdfctx): case plain_exec(t_pdfctx)
            /* Executable arrays are treated as literals in direct execution. */
#define cases_lit_array()\
  case exec(t_array): case exec(t_mixedarray): case exec(t_shortarray)
          cases_lit_1():
          cases_lit_2():
          cases_lit_3():
          cases_lit_4():
          cases_lit_5():
            INCR(lit);
            break;
          cases_lit_array():
            INCR(lit_array);
            break;
            /* Special operators. */
        case plain_exec(tx_op_add):
x_add:      INCR(x_add);
            osp = iosp; /* sync o_stack */
            if ((code = zop_add(i_ctx_p)) < 0)
                return_with_error_tx_op(code);
            iosp--;
            next_either();
        case plain_exec(tx_op_def):
x_def:      INCR(x_def);
            osp = iosp; /* sync o_stack */
            if ((code = zop_def(i_ctx_p)) < 0)
                return_with_error_tx_op(code);
            iosp -= 2;
            next_either();
        case plain_exec(tx_op_dup):
x_dup:      INCR(x_dup);
            if (iosp < osbot)
                return_with_error_tx_op(gs_error_stackunderflow);
            if (iosp >= ostop) {
                o_stack.requested = 1;
                return_with_error_tx_op(gs_error_stackoverflow);
            }
            iosp++;
            ref_assign_inline(iosp, iosp - 1);
            next_either();
        case plain_exec(tx_op_exch):
x_exch:     INCR(x_exch);
            if (iosp <= osbot)
                return_with_error_tx_op(gs_error_stackunderflow);
            ref_assign_inline(&token, iosp);
            ref_assign_inline(iosp, iosp - 1);
            ref_assign_inline(iosp - 1, &token);
            next_either();
        case plain_exec(tx_op_if):
x_if:       INCR(x_if);
            if (!r_is_proc(iosp))
                return_with_error_tx_op(check_proc_failed(iosp));
            if (!r_has_type(iosp - 1, t_boolean))
                return_with_error_tx_op((iosp <= osbot ?
                                        gs_error_stackunderflow : gs_error_typecheck));
            if (!iosp[-1].value.boolval) {
                iosp -= 2;
                next_either();
            }
            if (iesp >= estop)
                return_with_error_tx_op(gs_error_execstackoverflow);
            store_state_either(iesp);
            whichp = iosp;
            iosp -= 2;
            goto ifup;
        case plain_exec(tx_op_ifelse):
x_ifelse:   INCR(x_ifelse);
            if (!r_is_proc(iosp))
                return_with_error_tx_op(check_proc_failed(iosp));
            if (!r_is_proc(iosp - 1))
                return_with_error_tx_op(check_proc_failed(iosp - 1));
            if (!r_has_type(iosp - 2, t_boolean))
                return_with_error_tx_op((iosp < osbot + 2 ?
                                        gs_error_stackunderflow : gs_error_typecheck));
            if (iesp >= estop)
                return_with_error_tx_op(gs_error_execstackoverflow);
            store_state_either(iesp);
            whichp = (iosp[-2].value.boolval ? iosp - 1 : iosp);
            iosp -= 3;
            /* Open code "up" for the array case(s) */
          ifup:if ((icount = r_size(whichp) - 1) <= 0) {
                if (icount < 0)
                    goto up;    /* 0-element proc */
                SET_IREF(whichp->value.refs);   /* 1-element proc */
                if (--(*ticks_left) > 0)
                    goto top;
            }
            ++iesp;
            /* Do a ref_assign, but also set iref. */
            iesp->tas = whichp->tas;
            SET_IREF(iesp->value.refs = whichp->value.refs);
            if (--(*ticks_left) > 0)
                goto top;
            goto slice;
        case plain_exec(tx_op_index):
x_index:    INCR(x_index);
            osp = iosp; /* zindex references o_stack */
            if ((code = zindex(i_ctx_p)) < 0)
                return_with_error_tx_op(code);
            next_either();
        case plain_exec(tx_op_pop):
x_pop:      INCR(x_pop);
            if (iosp < osbot)
                return_with_error_tx_op(gs_error_stackunderflow);
            iosp--;
            next_either();
        case plain_exec(tx_op_roll):
x_roll:     INCR(x_roll);
            osp = iosp; /* zroll references o_stack */
            if ((code = zroll(i_ctx_p)) < 0)
                return_with_error_tx_op(code);
            iosp -= 2;
            next_either();
        case plain_exec(tx_op_sub):
x_sub:      INCR(x_sub);
            osp = iosp; /* sync o_stack */
            if ((code = zop_sub(i_ctx_p)) < 0)
                return_with_error_tx_op(code);
            iosp--;
            next_either();
            /* Executable types. */
        case plain_exec(t_null):
            goto bot;
        case plain_exec(t_oparray):
            /* Replace with the definition and go again. */
            INCR(exec_array);
            opindex = op_index(IREF);
            pvalue = (ref *)IREF->value.const_refs;
          opst:         /* Prepare to call a t_oparray procedure in *pvalue. */
            store_state(iesp);
          oppr:         /* Record the stack depths in case of failure. */
            if (iesp >= estop - 4)
                return_with_error_iref(gs_error_execstackoverflow);
            iesp += 5;
            osp = iosp;         /* ref_stack_count_inline needs this */
            make_mark_estack(iesp - 4, es_other, oparray_cleanup);
            make_int(iesp - 3, opindex); /* for .errorexec effect */
            make_int(iesp - 2, ref_stack_count_inline(&o_stack));
            make_int(iesp - 1, ref_stack_count_inline(&d_stack));
            make_op_estack(iesp, oparray_pop);
            goto pr;
          prst:         /* Prepare to call the procedure (array) in *pvalue. */
            store_state(iesp);
          pr:                   /* Call the array in *pvalue.  State has been stored. */
            /* We want to do this check before assigning icount so icount is correct
             * in the event of a gs_error_execstackoverflow
             */
            if (iesp >= estop) {
                return_with_error_iref(gs_error_execstackoverflow);
            }
            if ((icount = r_size(pvalue) - 1) <= 0) {
                if (icount < 0)
                    goto up;    /* 0-element proc */
                SET_IREF(pvalue->value.refs);   /* 1-element proc */
                if (--(*ticks_left) > 0)
                    goto top;
            }
            ++iesp;
            /* Do a ref_assign, but also set iref. */
            iesp->tas = pvalue->tas;
            SET_IREF(iesp->value.refs = pvalue->value.refs);
            if (--(*ticks_left) > 0)
                goto top;
            goto slice;
        case plain_exec(t_operator):
            INCR(exec_operator);
            if (--(*ticks_left) <= 0) {    /* The following doesn't work, */
                /* and I can't figure out why. */
/****** goto sst; ******/
            }
            esp = iesp;         /* save for operator */
            osp = iosp;         /* ditto */
            /* Operator routines take osp as an argument. */
            /* This is just a convenience, since they adjust */
            /* osp themselves to reflect the results. */
            /* Operators that (net) push information on the */
            /* operand stack must check for overflow: */
            /* this normally happens automatically through */
            /* the push macro (in oper.h). */
            /* Operators that do not typecheck their operands, */
            /* or take a variable number of arguments, */
            /* must check explicitly for stack underflow. */
            /* (See oper.h for more detail.) */
            /* Note that each case must set iosp = osp: */
            /* this is so we can switch on code without having to */
            /* store it and reload it (for dumb compilers). */
            switch (code = call_operator(real_opproc(IREF), i_ctx_p)) {
                case 0: /* normal case */
                case 1: /* alternative success case */
                    iosp = osp;
                    next();
                case o_push_estack:     /* store the state and go to up */
                    store_state(iesp);
                  opush:iosp = osp;
                    iesp = esp;
                    if (--(*ticks_left) > 0)
                        goto up;
                    goto slice;
                case o_pop_estack:      /* just go to up */
                  opop:iosp = osp;
                    if (esp == iesp)
                        goto bot;
                    iesp = esp;
                    goto up;
                case gs_error_Remap_Color:
oe_remap:           store_state(iesp);
remap:              if (iesp + 2 >= estop) {
                        esp = iesp;
                        code = ref_stack_extend(&e_stack, 2);
                        if (code < 0)
                            return_with_error_iref(code);
                        iesp = esp;
                    }
                    packed_get(imemory, iref_packed, iesp + 1);
                    make_oper(iesp + 2, 0,
                              r_ptr(&istate->remap_color_info,
                                    int_remap_color_info_t)->proc);
                    iesp += 2;
                    goto up;
            }
            iosp = osp;
            iesp = esp;
            return_with_code_iref();
        case plain_exec(t_name):
            INCR(exec_name);
            pvalue = IREF->value.pname->pvalue;
            if (!pv_valid(pvalue)) {
                uint nidx = names_index(int_nt, IREF);
                uint htemp = 0;

                INCR(find_name);
                if ((pvalue = dict_find_name_by_index_inline(nidx, htemp)) == 0)
                    return_with_error_iref(gs_error_undefined);
            }
            /* Dispatch on the type of the value. */
            /* Again, we have to over-populate the switch. */
            switch (r_type_xe(pvalue)) {
                  cases_invalid():
                    return_with_error_iref(gs_error_Fatal);
                  cases_nox():  /* access errors */
                    return_with_error_iref(gs_error_invalidaccess);
                  cases_lit_1():
                  cases_lit_2():
                  cases_lit_3():
                  cases_lit_4():
                  cases_lit_5():
                      INCR(name_lit);
                    /* Just push the value */
                    if (iosp >= ostop)
                        return_with_stackoverflow(pvalue);
                    ++iosp;
                    ref_assign_inline(iosp, pvalue);
                    next();
                case exec(t_array):
                case exec(t_mixedarray):
                case exec(t_shortarray):
                    INCR(name_proc);
                    /* This is an executable procedure, execute it. */
                    goto prst;
                case plain_exec(tx_op_add):
                    goto x_add;
                case plain_exec(tx_op_def):
                    goto x_def;
                case plain_exec(tx_op_dup):
                    goto x_dup;
                case plain_exec(tx_op_exch):
                    goto x_exch;
                case plain_exec(tx_op_if):
                    goto x_if;
                case plain_exec(tx_op_ifelse):
                    goto x_ifelse;
                case plain_exec(tx_op_index):
                    goto x_index;
                case plain_exec(tx_op_pop):
                    goto x_pop;
                case plain_exec(tx_op_roll):
                    goto x_roll;
                case plain_exec(tx_op_sub):
                    goto x_sub;
                case plain_exec(t_null):
                    goto bot;
                case plain_exec(t_oparray):
                    INCR(name_oparray);
                    opindex = op_index(pvalue);
                    pvalue = (ref *)pvalue->value.const_refs;
                    goto opst;
                case plain_exec(t_operator):
                    INCR(name_operator);
                    {           /* Shortcut for operators. */
                        /* See above for the logic. */
                        if (--(*ticks_left) <= 0) {        /* The following doesn't work, */
                            /* and I can't figure out why. */
/****** goto sst; ******/
                        }
                        esp = iesp;
                        osp = iosp;
                        switch (code = call_operator(real_opproc(pvalue),
                                                     i_ctx_p)
                                ) {
                            case 0:     /* normal case */
                            case 1:     /* alternative success case */
                                iosp = osp;
                                next();
                            case o_push_estack:
                                store_state(iesp);
                                goto opush;
                            case o_pop_estack:
                                goto opop;
                            case gs_error_Remap_Color:
                                goto oe_remap;
                        }
                        iosp = osp;
                        iesp = esp;
                        return_with_error(code, pvalue);
                    }
                case plain_exec(t_name):
                case exec(t_file):
                case exec(t_string):
                default:
                    /* Not a procedure, reinterpret it. */
                    store_state(iesp);
                    icount = 0;
                    SET_IREF(pvalue);
                    goto top;
            }
        case exec(t_file):
            {   /* Executable file.  Read the next token and interpret it. */
                stream *s;
                scanner_state sstate;

                check_read_known_file(i_ctx_p, s, IREF, return_with_error_iref);
            rt:
                if (iosp >= ostop)      /* check early */
                    return_with_stackoverflow_iref();
                osp = iosp;     /* gs_scan_token uses ostack */
                gs_scanner_init_options(&sstate, IREF, i_ctx_p->scanner_options);
            again:
                code = gs_scan_token(i_ctx_p, &token, &sstate);
                iosp = osp;     /* ditto */
                switch (code) {
                    case 0:     /* read a token */
                        /* It's worth checking for literals, which make up */
                        /* the majority of input tokens, before storing the */
                        /* state on the e-stack.  Note that because of //, */
                        /* the token may have *any* type and attributes. */
                        /* Note also that executable arrays aren't executed */
                        /* at the top level -- they're treated as literals. */
                        if (!r_has_attr(&token, a_executable) ||
                            r_is_array(&token)
                            ) { /* If gs_scan_token used the o-stack, */
                            /* we know we can do a push now; if not, */
                            /* the pre-check is still valid. */
                            iosp++;
                            ref_assign_inline(iosp, &token);
                            /* With a construct like /f currentfile def //f we can
                               end up here with IREF == &token which can go badly wrong,
                               so find the current file we're interpeting on the estack
                               and have IREF point to that ref, rather than "token"
                             */
                            if (IREF == &token) {
                                ref *st;
                                int code2 = z_current_file(i_ctx_p, &st);
                                if (code2 < 0 || st == NULL) {
                                    ierror.code = gs_error_Fatal;
                                    goto rweci;
                                }
                                SET_IREF(st);
                            }
                            goto rt;
                        }
                        store_state(iesp);
                        /* Push the file on the e-stack */
                        if (iesp >= estop)
                            return_with_error_iref(gs_error_execstackoverflow);
                        esfile_set_cache(++iesp);
                        ref_assign_inline(iesp, IREF);
                        SET_IREF(&token);
                        icount = 0;
                        goto top;
                    case gs_error_undefined:   /* //name undefined */
                        gs_scanner_error_object(i_ctx_p, &sstate, &token);
                        return_with_error(code, &token);
                    case scan_EOF:      /* end of file */
                        esfile_clear_cache();
                        goto bot;
                    case scan_BOS:
                        /* Binary object sequences */
                        /* ARE executed at the top level. */
                        store_state(iesp);
                        /* Push the file on the e-stack */
                        if (iesp >= estop)
                            return_with_error_iref(gs_error_execstackoverflow);
                        esfile_set_cache(++iesp);
                        ref_assign_inline(iesp, IREF);
                        pvalue = &token;
                        goto pr;
                    case scan_Refill:
                        store_state(iesp);
                        /* iref may point into the exec stack; */
                        /* save its referent now. */
                        ref_assign_inline(&token, IREF);
                        /* Push the file on the e-stack */
                        if (iesp >= estop)
                            return_with_error_iref(gs_error_execstackoverflow);
                        ++iesp;
                        ref_assign_inline(iesp, &token);
                        esp = iesp;
                        osp = iosp;
                        code = gs_scan_handle_refill(i_ctx_p, &sstate, true,
                                                     ztokenexec_continue);
                scan_cont:
                        iosp = osp;
                        iesp = esp;
                        switch (code) {
                            case 0:
                                iesp--;         /* don't push the file */
                                goto again;     /* stacks are unchanged */
                            case o_push_estack:
                                esfile_clear_cache();
                                if (--(*ticks_left) > 0)
                                    goto up;
                                goto slice;
                        }
                        /* must be an error */
                        iesp--; /* don't push the file */
                        return_with_code_iref();
                    case scan_Comment:
                    case scan_DSC_Comment: {
                        /* See scan_Refill above for comments. */
                        ref file_token;

                        store_state(iesp);
                        ref_assign_inline(&file_token, IREF);
                        if (iesp >= estop)
                            return_with_error_iref(gs_error_execstackoverflow);
                        ++iesp;
                        ref_assign_inline(iesp, &file_token);
                        esp = iesp;
                        osp = iosp;
                        code = ztoken_handle_comment(i_ctx_p,
                                                     &sstate, &token,
                                                     code, true, true,
                                                     ztokenexec_continue);
                    }
                        goto scan_cont;
                    default:    /* error */
                        ref_assign_inline(&token, IREF);
                        gs_scanner_error_object(i_ctx_p, &sstate, &token);
                        return_with_error(code, &token);
                }
            }
        case exec(t_string):
            {                   /* Executable string.  Read a token and interpret it. */
                stream ss;
                scanner_state sstate;

                s_init(&ss, NULL);
                sread_string(&ss, IREF->value.bytes, r_size(IREF));
                gs_scanner_init_stream_options(&sstate, &ss, SCAN_FROM_STRING);
                osp = iosp;     /* gs_scan_token uses ostack */
                code = gs_scan_token(i_ctx_p, &token, &sstate);
                iosp = osp;     /* ditto */
                switch (code) {
                    case 0:     /* read a token */
                    case scan_BOS:      /* binary object sequence */
                        store_state(iesp);
                        /* If the updated string isn't empty, push it back */
                        /* on the e-stack. */
                        {
                            /* This is just the available buffer size, so
                               a signed int is plenty big
                             */
                            int size = sbufavailable(&ss);

                            if (size > 0) {
                                if (iesp >= estop)
                                    return_with_error_iref(gs_error_execstackoverflow);
                                ++iesp;
                                iesp->tas.type_attrs = IREF->tas.type_attrs;
                                iesp->value.const_bytes = sbufptr(&ss);
                                r_set_size(iesp, size);
                            }
                        }
                        if (code == 0) {
                            SET_IREF(&token);
                            icount = 0;
                            goto top;
                        }
                        /* Handle BOS specially */
                        pvalue = &token;
                        goto pr;
                    case scan_EOF:      /* end of string */
                        goto bot;
                    case scan_Refill:   /* error */
                        code = gs_note_error(gs_error_syntaxerror);
                        /* fall through */
                    default:    /* error */
                        ref_assign_inline(&token, IREF);
                        gs_scanner_error_object(i_ctx_p, &sstate, &token);
                        return_with_error(code, &token);
                }
            }
            /* Handle packed arrays here by re-dispatching. */
            /* This also picks up some anomalous cases of non-packed arrays. */
        default:
            {
                uint index;

                switch (*iref_packed >> r_packed_type_shift) {
                    case pt_full_ref:
                    case pt_full_ref + 1:
                        INCR(p_full);
                        if (iosp >= ostop)
                            return_with_stackoverflow_iref();
                        /* We know this can't be an executable object */
                        /* requiring special handling, so we just push it. */
                        ++iosp;
                        /* We know that refs are properly aligned: */
                        /* see packed.h for details. */
                        ref_assign_inline(iosp, IREF);
                        next();
                    case pt_executable_operator:
                        index = *iref_packed & packed_value_mask;
                        if (--(*ticks_left) <= 0) {        /* The following doesn't work, */
                            /* and I can't figure out why. */
/****** goto sst_short; ******/
                        }
                        if (!op_index_is_operator(index)) {
                            INCR(p_exec_oparray);
                            store_state_short(iesp);
                            opindex = index;
                            /* Call the operator procedure. */
                            index -= op_def_count;
                            pvalue = (ref *)
                                (index < r_size(&i_ctx_p->op_array_table_global.table) ?
                                 i_ctx_p->op_array_table_global.table.value.const_refs +
                                 index :
                                 i_ctx_p->op_array_table_local.table.value.const_refs +
                                 (index - r_size(&i_ctx_p->op_array_table_global.table)));
                            goto oppr;
                        }
                        INCR(p_exec_operator);
                        /* See the main plain_exec(t_operator) case */
                        /* for details of what happens here. */
#if PACKED_SPECIAL_OPS
                        /*
                         * We arranged in iinit.c that the special ops
                         * have operator indices starting at 1.
                         *
                         * The (int) cast in the next line is required
                         * because some compilers don't allow arithmetic
                         * involving two different enumerated types.
                         */
#  define case_xop(xop) case xop - (int)tx_op + 1
                        switch (index) {
                              case_xop(tx_op_add):goto x_add;
                              case_xop(tx_op_def):goto x_def;
                              case_xop(tx_op_dup):goto x_dup;
                              case_xop(tx_op_exch):goto x_exch;
                              case_xop(tx_op_if):goto x_if;
                              case_xop(tx_op_ifelse):goto x_ifelse;
                              case_xop(tx_op_index):goto x_index;
                              case_xop(tx_op_pop):goto x_pop;
                              case_xop(tx_op_roll):goto x_roll;
                              case_xop(tx_op_sub):goto x_sub;
                            case 0:     /* for dumb compilers */
                            default:
                                ;
                        }
#  undef case_xop
#endif
                        INCR(p_exec_non_x_operator);
                        esp = iesp;
                        osp = iosp;
                        switch (code = call_operator(op_index_proc(index), i_ctx_p)) {
                            case 0:
                            case 1:
                                iosp = osp;
                                next_short();
                            case o_push_estack:
                                store_state_short(iesp);
                                goto opush;
                            case o_pop_estack:
                                iosp = osp;
                                if (esp == iesp) {
                                    next_short();
                                }
                                iesp = esp;
                                goto up;
                            case gs_error_Remap_Color:
                                store_state_short(iesp);
                                goto remap;
                        }
                        iosp = osp;
                        iesp = esp;
                        return_with_code_iref();
                    case pt_integer:
                        INCR(p_integer);
                        if (iosp >= ostop)
                            return_with_stackoverflow_iref();
                        ++iosp;
                        make_int(iosp,
                                 ((int)*iref_packed & packed_int_mask) +
                                 packed_min_intval);
                        next_short();
                    case pt_literal_name:
                        INCR(p_lit_name);
                        {
                            uint nidx = *iref_packed & packed_value_mask;

                            if (iosp >= ostop)
                                return_with_stackoverflow_iref();
                            ++iosp;
                            name_index_ref_inline(int_nt, nidx, iosp);
                            next_short();
                        }
                    case pt_executable_name:
                        INCR(p_exec_name);
                        {
                            uint nidx = *iref_packed & packed_value_mask;

                            pvalue = name_index_ptr_inline(int_nt, nidx)->pvalue;
                            if (!pv_valid(pvalue)) {
                                uint htemp = 0;

                                INCR(p_find_name);
                                if ((pvalue = dict_find_name_by_index_inline(nidx, htemp)) == 0) {
                                    names_index_ref(int_nt, nidx, &token);
                                    return_with_error(gs_error_undefined, &token);
                                }
                            }
                            if (r_has_masked_attrs(pvalue, a_execute, a_execute + a_executable)) {      /* Literal, push it. */
                                INCR(p_name_lit);
                                if (iosp >= ostop)
                                    return_with_stackoverflow_iref();
                                ++iosp;
                                ref_assign_inline(iosp, pvalue);
                                next_short();
                            }
                            if (r_is_proc(pvalue)) {    /* This is an executable procedure, */
                                /* execute it. */
                                INCR(p_name_proc);
                                store_state_short(iesp);
                                goto pr;
                            }
                            /* Not a literal or procedure, reinterpret it. */
                            store_state_short(iesp);
                            icount = 0;
                            SET_IREF(pvalue);
                            goto top;
                        }
                        /* default can't happen here */
                }
            }
    }
    /* Literal type, just push it. */
    if (iosp >= ostop)
        return_with_stackoverflow_iref();
    ++iosp;
    ref_assign_inline(iosp, IREF);
  bot:next();
  out:                          /* At most 1 more token in the current procedure. */
    /* (We already decremented icount.) */
    if (!icount) {
        /* Pop the execution stack for tail recursion. */
        iesp--;
        iref_packed = IREF_NEXT(iref_packed);
        goto top;
    }
  up:if (--(*ticks_left) < 0)
        goto slice;
    /* See if there is anything left on the execution stack. */
    if (!r_is_proc(iesp)) {
        SET_IREF(iesp--);
        icount = 0;
        goto top;
    }
    SET_IREF(iesp->value.refs); /* next element of array */
    icount = r_size(iesp) - 1;
    if (icount <= 0) {          /* <= 1 more elements */
        iesp--;                 /* pop, or tail recursion */
        if (icount < 0)
            goto up;
    }
    goto top;
  sched:                        /* We've just called a scheduling procedure. */
    /* The interpreter state is in memory; iref is not current. */
    if (code < 0) {
        set_error(code);
        /*
         * We need a real object to return as the error object.
         * (It only has to last long enough to store in
         * *perror_object.)
         */
        make_null_proc(&ierror.full);
        SET_IREF(ierror.obj = &ierror.full);
        goto error_exit;
    }
    /* Reload state information from memory. */
    iosp = osp;
    iesp = esp;
    goto up;
#if 0                           /****** ****** ***** */
  sst:                          /* Time-slice, but push the current object first. */
    store_state(iesp);
    if (iesp >= estop)
        return_with_error_iref(gs_error_execstackoverflow);
    iesp++;
    ref_assign_inline(iesp, iref);
#endif /****** ****** ***** */
  slice:                        /* It's time to time-slice or garbage collect. */
    /* iref is not live, so we don't need to do a store_state. */
    osp = iosp;
    esp = iesp;
    /* If *ticks_left <= -100, we need to GC now. */
    if ((*ticks_left) <= -100) {   /* We need to garbage collect now. */
        *pi_ctx_p = i_ctx_p;
        code = interp_reclaim(pi_ctx_p, -1);
        i_ctx_p = *pi_ctx_p;
    } else
        code = 0;
    *ticks_left = i_ctx_p->time_slice_ticks;
    set_code_on_interrupt(imemory, &code);
    goto sched;

    /* Error exits. */

  rweci:
    ierror.code = code;
  rwei:
    ierror.obj = IREF;
  rwe:
    if (!r_is_packed(iref_packed))
        store_state(iesp);
    else {
        /*
         * We need a real object to return as the error object.
         * (It only has to last long enough to store in *perror_object.)
         */
        packed_get(imemory, (const ref_packed *)ierror.obj, &ierror.full);
        store_state_short(iesp);
        if (IREF == ierror.obj)
            SET_IREF(&ierror.full);
        ierror.obj = &ierror.full;
    }
  error_exit:
    if (GS_ERROR_IS_INTERRUPT(ierror.code)) {      /* We must push the current object being interpreted */
        /* back on the e-stack so it will be re-executed. */
        /* Currently, this is always an executable operator, */
        /* but it might be something else someday if we check */
        /* for interrupts in the interpreter loop itself. */
        if (iesp >= estop)
            ierror.code = gs_error_execstackoverflow;
        else {
            iesp++;
            ref_assign_inline(iesp, IREF);
        }
    }
    esp = iesp;
    osp = iosp;
    ref_assign_inline(perror_object, ierror.obj);
#ifdef DEBUG
    if (ierror.code == gs_error_InterpreterExit) {
        /* Do not call gs_log_error to reduce the noise. */
        return gs_error_InterpreterExit;
    }
#endif
    return gs_log_error(ierror.code, __FILE__, ierror.line);
}

/* Pop the bookkeeping information for a normal exit from a t_oparray. */
static int
oparray_pop(i_ctx_t *i_ctx_p)
{
    esp -= 4;
    return o_pop_estack;
}

/* Restore the stack pointers after an error inside a t_oparray procedure. */
/* This procedure is called only from pop_estack. */
static int
oparray_cleanup(i_ctx_t *i_ctx_p)
{                               /* esp points just below the cleanup procedure. */
    es_ptr ep = esp;
    uint ocount_old = (uint) ep[3].value.intval;
    uint dcount_old = (uint) ep[4].value.intval;
    uint ocount = ref_stack_count(&o_stack);
    uint dcount = ref_stack_count(&d_stack);

    if (ocount > ocount_old)
        ref_stack_pop(&o_stack, ocount - ocount_old);
    if (dcount > dcount_old) {
        ref_stack_pop(&d_stack, dcount - dcount_old);
        dict_set_top();
    }
    return 0;
}

/* Don't restore the stack pointers. */
static int
oparray_no_cleanup(i_ctx_t *i_ctx_p)
{
    return 0;
}

/* Find the innermost oparray. */
static ref *
oparray_find(i_ctx_t *i_ctx_p)
{
    long i;
    ref *ep;

    for (i = 0; (ep = ref_stack_index(&e_stack, i)) != 0; ++i) {
        if (r_is_estack_mark(ep) &&
            (ep->value.opproc == oparray_cleanup ||
             ep->value.opproc == oparray_no_cleanup)
            )
            return ep;
    }
    return 0;
}

/* <errorobj> <obj> .errorexec ... */
/* Execute an object, substituting errorobj for the 'command' if an error */
/* occurs during the execution.  Cf .execfile (in zfile.c). */
static int
zerrorexec(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code;

    check_op(2);
    check_estack(4);            /* mark/cleanup, errobj, pop, obj */
    push_mark_estack(es_other, errorexec_cleanup);
    *++esp = op[-1];
    push_op_estack(errorexec_pop);
    code = zexec(i_ctx_p);
    if (code >= 0)
        pop(1);
    else
        esp -= 3;               /* undo our additions to estack */
    return code;
}

/* - .finderrorobject <errorobj> true */
/* - .finderrorobject false */
/* If we are within an .errorexec or oparray, return the error object */
/* and true, otherwise return false. */
static int
zfinderrorobject(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    ref errobj;

    if (errorexec_find(i_ctx_p, &errobj)) {
        push(2);
        op[-1] = errobj;
        make_true(op);
    } else {
        push(1);
        make_false(op);
    }
    return 0;
}

/*
 * Find the innermost .errorexec or oparray.  If there is an oparray, or a
 * .errorexec with errobj != null, store it in *perror_object and return 1,
 * otherwise return 0;
 */
int
errorexec_find(i_ctx_t *i_ctx_p, ref *perror_object)
{
    long i;
    const ref *ep;

    for (i = 0; (ep = ref_stack_index(&e_stack, i)) != 0; ++i) {
        if (r_is_estack_mark(ep)) {
            if (ep->value.opproc == oparray_cleanup) {
                /* See oppr: above. */
                uint opindex = (uint)ep[1].value.intval;
                if (opindex == 0) /* internal operator, ignore */
                    continue;
                op_index_ref(imemory, opindex, perror_object);
                return 1;
            }
            if (ep->value.opproc == oparray_no_cleanup)
                return 0;       /* protection disabled */
            if (ep->value.opproc == errorexec_cleanup) {
                if (r_has_type(ep + 1, t_null))
                    return 0;
                *perror_object = ep[1]; /* see .errorexec above */
                return 1;
            }
        }
    }
    return 0;
}

/* Pop the bookkeeping information on a normal exit from .errorexec. */
static int
errorexec_pop(i_ctx_t *i_ctx_p)
{
    esp -= 2;
    return o_pop_estack;
}

/* Clean up when unwinding the stack on an error.  (No action needed.) */
static int
errorexec_cleanup(i_ctx_t *i_ctx_p)
{
    return 0;
}

/* <bool> .setstackprotect - */
/* Set whether to protect the stack for the innermost oparray. */
static int
zsetstackprotect(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    ref *ep = oparray_find(i_ctx_p);

    check_op(1);
    check_type(*op, t_boolean);
    if (ep == 0)
        return_error(gs_error_rangecheck);
    ep->value.opproc =
        (op->value.boolval ? oparray_cleanup : oparray_no_cleanup);
    pop(1);
    return 0;
}

/* - .currentstackprotect <bool> */
/* Return the stack protection status. */
static int
zcurrentstackprotect(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    ref *ep = oparray_find(i_ctx_p);

    if (ep == 0)
        return_error(gs_error_rangecheck);
    push(1);
    make_bool(op, ep->value.opproc == oparray_cleanup);
    return 0;
}

static int
zactonuel(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_bool(op, !!gs_lib_ctx_get_act_on_uel((gs_memory_t *)(i_ctx_p->memory.current)));
    return 0;
}
