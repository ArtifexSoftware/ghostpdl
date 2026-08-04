/* Copyright (C) 2001-2026 Artifex Software, Inc.
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


/* Procedure-based filter stream support */
#include "memory_.h"
#include "stat_.h" /* get system header early to avoid name clash on Cygwin */
#include "ghost.h"
#include "oper.h"		/* for ifilter.h */
#include "estack.h"
#include "gsstruct.h"
#include "ialloc.h"
#include "istruct.h"		/* for RELOC_REF_VAR */
#include "stream.h"
#include "strimpl.h"
#include "ifilter.h"
#include "files.h"
#include "store.h"

#include "isave.h"
#include "isstate.h"
/* ---------------- Generic ---------------- */

/* GC procedures */
static
CLEAR_MARKS_PROC(sproc_clear_marks)
{
    stream_proc_state *const pptr = vptr;

    r_clear_attrs(&pptr->proc, l_mark);
    r_clear_attrs(&pptr->data, l_mark);
}
static
ENUM_PTRS_WITH(sproc_enum_ptrs, stream_proc_state *pptr) return 0;
case 0:
ENUM_RETURN_REF(&pptr->proc);
case 1:
ENUM_RETURN_REF(&pptr->data);
ENUM_PTRS_END
static RELOC_PTRS_WITH(sproc_reloc_ptrs, stream_proc_state *pptr);
RELOC_REF_VAR(pptr->proc);
r_clear_attrs(&pptr->proc, l_mark);
RELOC_REF_VAR(pptr->data);
r_clear_attrs(&pptr->data, l_mark);
RELOC_PTRS_END

/* Structure type for procedure-based streams. */
private_st_stream_proc_state();

/* Allocate and open a procedure-based filter. */
/* The caller must have checked that *sop is a procedure. */
static int
s_proc_init(ref * sop, stream ** psstrm, uint mode,
            const stream_template * temp, const stream_procs * procs,
            gs_ref_memory_t *imem)
{
    gs_memory_t *const mem = (gs_memory_t *)imem;
    stream *sstrm = file_alloc_stream(mem, "s_proc_init(stream)");
    stream_proc_state *state = (stream_proc_state *)
        s_alloc_state(mem, &st_sproc_state, "s_proc_init(state)");

    if (sstrm == 0 || state == 0) {
        gs_free_object(mem, state, "s_proc_init(state)");
        /*gs_free_object(mem, sstrm, "s_proc_init(stream)"); *//* just leave it on the file list */
        return_error(gs_error_VMerror);
    }
    s_std_init(sstrm, NULL, 0, procs, mode);
    sstrm->procs.process = temp->process;
    state->templat = temp;
    state->memory = mem;
    state->eof = 0;
    state->proc = *sop;
    make_empty_string(&state->data, a_all);
    state->data_memory = NULL;
    state->data_save_id = 0;
    state->index = 0;
    sstrm->state = (stream_state *) state;
    *psstrm = sstrm;
    return 0;
}

/*
 * Note where the data string just stored in ss->data was allocated, and
 * which save was innermost at that time (if the space the string was created
 * in was local VM). The procedure may hand us a string allocated after an
 *  enclosing 'save' while the stream state itself predates that save;
 * if that save is restored, the string is freed but the stream survives,
 * so we must be able to detect this before using the string again.
 * The PLRM is explicit that PostScript program should not do this, and the
 * result is 'unpredictable'.
 */
static void
s_proc_record_data(i_ctx_t *i_ctx_p, stream_proc_state *ss, const ref *pdata)
{
    int space_index = r_space_index(pdata);
    gs_ref_memory_t *mem;

    ss->data_memory = NULL;
    ss->data_save_id = 0;
    if (space_index != i_vm_local)
        return;			/* global, foreign or system VM: not restorable */
    mem = idmemory->spaces_indexed[space_index];
    if (mem == NULL || mem->saved == NULL)
        return;			/* no save active: string cannot be restored away */
    ss->data_memory = mem;
    /* Save ids are unique and never reused; 0 means 'invisible' save. */
    ss->data_save_id = (mem->saved->id != 0 ? mem->saved->id : (ulong)-1);
}

/*
 * Check that the string in ss->data has not been freed by a restore,
 * i.e. that the save that was innermost when it was stored is still
 * active.  If it is not, the string may no longer exist: replace the
 * dangling reference with an empty string so it cannot be used.
 * Returns true if ss->data is safe to use.
 */
static bool
s_proc_data_valid(stream_proc_state *ss)
{
    const alloc_save_t *save;

    if (ss->data_memory == NULL)
        return true;
    for (save = ss->data_memory->saved; save != NULL;
        save = save->state.saved)
        if (save->id == ss->data_save_id)
            return true;
    make_empty_string(&ss->data, a_all);
    ss->index = 0;
    ss->data_memory = NULL;
    ss->data_save_id = 0;
    return false;
}

/* Handle an interrupt during a stream operation. */
/* This is logically unrelated to procedure streams, */
/* but it is also associated with the interpreter stream machinery. */
static int
s_handle_intc(i_ctx_t *i_ctx_p, const ref *pstate, int nstate,
              op_proc_t cont)
{
    int npush = nstate + 2;

    check_estack(npush);
    if (nstate)
        memcpy(esp + 2, pstate, nstate * sizeof(ref));
#if 0				/* **************** */
    {
        int code = gs_interpret_error(gs_error_interrupt, (ref *) (esp + npush));

        if (code < 0)
            return code;
    }
#else /* **************** */
    npush--;
#endif /* **************** */
    make_op_estack(esp + 1, cont);
    esp += npush;
    return o_push_estack;
}

/* Set default parameter values (actually, just clear pointers). */
static void
s_proc_set_defaults(stream_state * st)
{
    stream_proc_state *const ss = (stream_proc_state *) st;

    make_null(&ss->proc);
    make_null(&ss->data);
}

static int s_proc_copy_string(i_ctx_t * i_ctx_p, ref *dest, ref *src, uint mem)
{
    int code = 0;
    uint saved_space = avm_local;

    saved_space = imemory_space(iimemory);

    if (imemory_space(iimemory) != mem)
        ialloc_set_space(idmemory, mem);

    code = gs_alloc_string_ref(iimemory, dest, 0, r_size(src), "copy_cspace_string");

    if (imemory_space(iimemory) != saved_space)
        ialloc_set_space(idmemory, saved_space);

    if (code < 0)
        return code;

    r_copy_attrs(dest, a_all, src);

    memcpy(dest->value.bytes, src->value.bytes, r_size(src));
    return 0;
}

/* ---------------- Read streams ---------------- */

/* Forward references */
static stream_proc_process(s_proc_read_process);
static int s_proc_read_continue(i_ctx_t *);

/* Stream templates */
static const stream_template s_proc_read_template = {
    &st_sproc_state, NULL, s_proc_read_process, 1, 1,
    NULL, s_proc_set_defaults
};
static const stream_procs s_proc_read_procs = {
    s_std_noavailable, s_std_noseek, s_std_read_reset,
    s_std_read_flush, s_std_null, NULL
};

/* Allocate and open a procedure-based read stream. */
/* The caller must have checked that *sop is a procedure. */
int
sread_proc(ref * sop, stream ** psstrm, gs_ref_memory_t *imem)
{
    int code =
        s_proc_init(sop, psstrm, s_mode_read, &s_proc_read_template,
                    &s_proc_read_procs, imem);

    if (code < 0)
        return code;
    (*psstrm)->end_status = CALLC;
    return code;
}

/* Handle an input request. */
static int
s_proc_read_process(stream_state * st, stream_cursor_read * ignore_pr,
                    stream_cursor_write * pw, bool last)
{
    /* Move data from the string returned by the procedure */
    /* into the stream buffer, or ask for a callback. */
    stream_proc_state *const ss = (stream_proc_state *) st;
    uint count;

    if (!s_proc_data_valid(ss))
        return_error(gs_error_ioerror);
    count = r_size(&ss->data) - ss->index;

    if (count > 0) {
        uint wcount = pw->limit - pw->ptr;

        if (wcount < count)
            count = wcount;
        memcpy(pw->ptr + 1, ss->data.value.bytes + ss->index, count);
        pw->ptr += count;
        ss->index += count;
        return 1;
    }
    return (ss->eof ? EOFC : CALLC);
}

/* Handle an exception (INTC or CALLC) from a read stream */
/* whose buffer is empty. */
int
s_handle_read_exception(i_ctx_t *i_ctx_p, int status, const ref * fop,
                        const ref * pstate, int nstate, op_proc_t cont)
{
    int npush = nstate + 4;
    stream *ps;

    switch (status) {
        case INTC:
            return s_handle_intc(i_ctx_p, pstate, nstate, cont);
        case CALLC:
            break;
        default:
            return_error(gs_error_ioerror);
    }
    /* Find the stream whose buffer needs refilling. */
    for (ps = fptr(fop); ps->strm != 0;)
        ps = ps->strm;
    check_estack(npush);
    if (nstate)
        memcpy(esp + 2, pstate, nstate * sizeof(ref));
    make_op_estack(esp + 1, cont);
    esp += npush;
    make_op_estack(esp - 2, s_proc_read_continue);
    esp[-1] = *fop;
    r_clear_attrs(esp - 1, a_executable);
    *esp = ((stream_proc_state *) ps->state)->proc;
    return o_push_estack;
}
/* Continue a read operation after returning from a procedure callout. */
/* osp[0] contains the file (pushed on the e-stack by handle_read_status); */
/* osp[-1] contains the new data string (pushed by the procedure). */
/* The top of the e-stack contains the real continuation. */
static int
s_proc_read_continue(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    os_ptr opbuf = op - 1;
    stream *ps;
    stream_proc_state *ss;
    uint s1, s2;

    check_file(ps, op);
    check_read_type(*opbuf, t_string);
    while ((ps->end_status = 0, ps->strm) != 0)
        ps = ps->strm;
    s1 = r_space(op);
    s2 = r_space(opbuf);
    if (!r_is_local(op) && r_is_local(opbuf)) {
        ref copy;
        int code = 0;

        code = s_proc_copy_string(i_ctx_p, &copy, opbuf, r_space(op));
        if (code < 0)
            return code;
        *opbuf = copy;
    }
    ss = (stream_proc_state *) ps->state;
    ss->data = *opbuf;
    s_proc_record_data(i_ctx_p, ss, opbuf);
    ss->index = 0;
    if (r_size(opbuf) == 0)
        ss->eof = true;
    pop(2);
    return 0;
}

/* ---------------- Write streams ---------------- */

/* Forward references */
static stream_proc_flush(s_proc_write_flush);
static stream_proc_process(s_proc_write_process);
static int s_proc_write_continue(i_ctx_t *);

/* Stream templates */
static const stream_template s_proc_write_template = {
    &st_sproc_state, NULL, s_proc_write_process, 1, 1,
    NULL, s_proc_set_defaults
};
static const stream_procs s_proc_write_procs = {
    s_std_noavailable, s_std_noseek, s_std_write_reset,
    s_proc_write_flush, s_std_null, NULL
};

/* Allocate and open a procedure-based write stream. */
/* The caller must have checked that *sop is a procedure. */
int
swrite_proc(ref * sop, stream ** psstrm, gs_ref_memory_t *imem)
{
    return s_proc_init(sop, psstrm, s_mode_write, &s_proc_write_template,
                       &s_proc_write_procs, imem);
}

/* Handle an output request. */
static int
s_proc_write_process(stream_state * st, stream_cursor_read * pr,
                     stream_cursor_write * ignore_pw, bool last)
{
    /* Move data from the stream buffer to the string */
    /* returned by the procedure, or ask for a callback. */
    stream_proc_state *const ss = (stream_proc_state *) st;
    uint rcount = pr->limit - pr->ptr;

    if (!s_proc_data_valid(ss))
        return_error(gs_error_ioerror);
    /* if 'last' return CALLC even when rcount == 0. ss->eof terminates */
    if (rcount > 0 || (last && !ss->eof)) {
        uint wcount = r_size(&ss->data) - ss->index;
        uint count = min(rcount, wcount);

        memcpy(ss->data.value.bytes + ss->index, pr->ptr + 1, count);
        pr->ptr += count;
        ss->index += count;
        if (rcount > wcount)
            return CALLC;
        else if (last) {
            ss->eof = true;
            return CALLC;
        } else
            return 0;
    }
    return ((ss->eof = last) ? EOFC : 0);
}

/* Flush the output.  This is non-standard because it must call the */
/* procedure. */
static int
s_proc_write_flush(stream *s)
{
    int result = s_process_write_buf(s, false);
    stream_proc_state *const ss = (stream_proc_state *)s->state;

    return (result < 0 || ss->index == 0 ? result : CALLC);
}

/* Handle an exception (INTC or CALLC) from a write stream */
/* whose buffer is full. */
int
s_handle_write_exception(i_ctx_t *i_ctx_p, int status, const ref * fop,
                         const ref * pstate, int nstate, op_proc_t cont)
{
    stream *ps;
    stream_proc_state *psst;

    switch (status) {
        case INTC:
            return s_handle_intc(i_ctx_p, pstate, nstate, cont);
        case CALLC:
            break;
        default:
            return_error(gs_error_ioerror);
    }
    /* Find the stream whose buffer needs emptying. */
    for (ps = fptr(fop); ps->strm != 0;)
        ps = ps->strm;
    psst = (stream_proc_state *) ps->state;
    if (!s_proc_data_valid(psst))
        return_error(gs_error_ioerror);
    {
        int npush = nstate + 6;

        check_estack(npush);
        if (nstate)
            memcpy(esp + 2, pstate, nstate * sizeof(ref));
        make_op_estack(esp + 1, cont);
        esp += npush;
        make_op_estack(esp - 4, s_proc_write_continue);
        esp[-3] = *fop;
        r_clear_attrs(esp - 3, a_executable);
        make_bool(esp - 1, !psst->eof);
    }
    esp[-2] = psst->proc;
    *esp = psst->data;
    r_set_size(esp, psst->index);
    return o_push_estack;
}
/* Continue a write operation after returning from a procedure callout. */
/* osp[0] contains the file (pushed on the e-stack by handle_write_status); */
/* osp[-1] contains the new buffer string (pushed by the procedure). */
/* The top of the e-stack contains the real continuation. */
static int
s_proc_write_continue(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    os_ptr opbuf = op - 1;
    stream *ps;
    stream_proc_state *ss;

    check_file(ps, op);
    check_write_type(*opbuf, t_string);
    while (ps->strm != 0) {
        if (ps->end_status == CALLC)
            ps->end_status = 0;
        ps = ps->strm;
    }
    ps->end_status = 0;
    if (!r_is_local(op) && r_is_local(opbuf)) {
        ref copy;
        int code = 0;

        code = s_proc_copy_string(i_ctx_p, &copy, opbuf, r_space(op));
        if (code < 0)
            return code;
        *opbuf = copy;
    }
    ss = (stream_proc_state *) ps->state;
    ss->data = *opbuf;
    s_proc_record_data(i_ctx_p, ss, opbuf);
    ss->index = 0;
    pop(2);
    return 0;
}

/* ------ More generic ------ */

/* Test whether a stream is procedure-based. */
bool
s_is_proc(const stream *s)
{
    return (s->procs.process == s_proc_read_process ||
            s->procs.process == s_proc_write_process);
}

/* ------ Initialization procedure ------ */

const op_def zfproc_op_defs[] =
{
                /* Internal operators */
    {"2%s_proc_read_continue", s_proc_read_continue},
    {"2%s_proc_write_continue", s_proc_write_continue},
    op_def_end(0)
};
