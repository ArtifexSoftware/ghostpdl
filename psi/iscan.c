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


/* Token scanner for Ghostscript interpreter */
#include "ghost.h"
#include "memory_.h"
#include "string_.h"
#include "stream.h"
#include "ierrors.h"
#include "btoken.h"             /* for ref_binary_object_format */
#include "files.h"              /* for fptr */
#include "ialloc.h"
#include "idict.h"              /* for //name lookup */
#include "dstack.h"             /* ditto */
#include "ilevel.h"
#include "iname.h"
#include "ipacked.h"
#include "iparray.h"
#include "strimpl.h"            /* for string decoding */
#include "sa85d.h"              /* ditto */
#include "sfilter.h"            /* ditto */
#include "ostack.h"             /* for accumulating proc bodies; */
                                        /* must precede iscan.h */
#include "iscan.h"              /* defines interface */
#include "iscanbin.h"
#include "iscannum.h"
#include "istream.h"
#include "istruct.h"            /* for RELOC_REF_VAR */
#include "iutil.h"
#include "ivmspace.h"
#include "store.h"
#include "scanchar.h"

/* Procedure for handling DSC comments if desired. */
/* Set at initialization if a DSC handling module is included. */
int (*gs_scan_dsc_proc) (const byte *, uint) = NULL;

/* Procedure for handling all comments if desired. */
/* Set at initialization if a comment handling module is included. */
/* If both gs_scan_comment_proc and gs_scan_dsc_proc are set, */
/* scan_comment_proc is called only for non-DSC comments. */
int (*gs_scan_comment_proc) (const byte *, uint) = NULL;

/*
 * Level 2 includes some changes in the scanner:
 *      - \ is always recognized in strings, regardless of the data source;
 *      - << and >> are legal tokens;
 *      - <~ introduces an ASCII85 encoded string (terminated by ~>);
 *      - Character codes above 127 introduce binary objects.
 * We explicitly enable or disable these changes based on level2_enabled.
 */

/* ------ Dynamic strings ------ */

/* Begin collecting a dynamically allocated string. */
static inline void
dynamic_init(da_ptr pda, gs_memory_t *mem)
{
    pda->is_dynamic = false;
    pda->limit = pda->buf + sizeof(pda->buf);
    pda->next = pda->base = pda->buf;
    pda->memory = mem;
}

/* Free a dynamic string. */
static void
dynamic_free(da_ptr pda)
{
    if (pda->is_dynamic)
        gs_free_string(pda->memory, pda->base, da_size(pda), "scanner");
}

/* Resize a dynamic string. */
/* If the allocation fails, return gs_error_VMerror; otherwise, return 0. */
static int
dynamic_resize(da_ptr pda, uint new_size)
{
    uint old_size = da_size(pda);
    uint pos = pda->next - pda->base;
    gs_memory_t *mem = pda->memory;
    byte *base;

    if (pda->is_dynamic) {
        base = gs_resize_string(mem, pda->base, old_size,
                                new_size, "scanner");
        if (base == 0)
            return_error(gs_error_VMerror);
    } else {                    /* switching from static to dynamic */
        base = gs_alloc_string(mem, new_size, "scanner");
        if (base == 0)
            return_error(gs_error_VMerror);
        memcpy(base, pda->base, min(old_size, new_size));
        pda->is_dynamic = true;
    }
    pda->base = base;
    pda->next = base + pos;
    pda->limit = base + new_size;
    return 0;
}

/* Grow a dynamic string. */
/* Return 0 if the allocation failed, the new 'next' ptr if OK. */
/* Return 0 or an error code, updating pda->next to point to the first */
/* available byte after growing. */
static int
dynamic_grow(da_ptr pda, byte * next, uint max_size)
{
    uint old_size = da_size(pda);
    uint new_size = (old_size < 10 ? 20 :
                     old_size >= (max_size >> 1) ? max_size :
                     old_size << 1);
    int code;

    pda->next = next;
    if (old_size >= max_size)
        return_error(gs_error_limitcheck);
    while ((code = dynamic_resize(pda, new_size)) < 0 &&
           new_size > old_size
        ) {                     /* Try trimming down the requested new size. */
        new_size -= (new_size - old_size + 1) >> 1;
    }
    return code;
}

/* Ensure that a dynamic string is either on the heap or in the */
/* private buffer. */
static void
dynamic_save(da_ptr pda)
{
    if (!pda->is_dynamic && pda->base != pda->buf) {
        int len = da_size(pda);

        if (len > sizeof(pda->buf))
            len = sizeof(pda->buf);
        memcpy(pda->buf, pda->base, len);
        pda->next = pda->buf + len;
        pda->base = pda->buf;
    }
}

/* Finish collecting a dynamic string. */
static int
dynamic_make_string(i_ctx_t *i_ctx_p, ref * pref, da_ptr pda, byte * next)
{
    uint size = (pda->next = next) - pda->base;
    int code = dynamic_resize(pda, size);

    if (code < 0)
        return code;
    make_tasv_new(pref, t_string,
                  a_all | imemory_space((gs_ref_memory_t *) pda->memory),
                  size, bytes, pda->base);
    return 0;
}

/* ------ Main scanner ------ */

/* GC procedures */
static
CLEAR_MARKS_PROC(scanner_clear_marks)
{
    scanner_state *const ssptr = vptr;

    r_clear_attrs(&ssptr->s_file, l_mark);
    r_clear_attrs(&ssptr->s_ss.binary.bin_array, l_mark);
    r_clear_attrs(&ssptr->s_error.object, l_mark);
}
static
ENUM_PTRS_WITH(scanner_enum_ptrs, scanner_state *ssptr) return 0;
case 0:
    ENUM_RETURN_REF(&ssptr->s_file);
case 1:
    ENUM_RETURN_REF(&ssptr->s_error.object);
case 2:
    if (ssptr->s_scan_type == scanning_none ||
        !ssptr->s_da.is_dynamic
        )
        ENUM_RETURN(0);
    return ENUM_STRING2(ssptr->s_da.base, da_size(&ssptr->s_da));
case 3:
    if (ssptr->s_scan_type != scanning_binary)
        return 0;
    ENUM_RETURN_REF(&ssptr->s_ss.binary.bin_array);
ENUM_PTRS_END
static RELOC_PTRS_WITH(scanner_reloc_ptrs, scanner_state *ssptr)
{
    RELOC_REF_VAR(ssptr->s_file);
    r_clear_attrs(&ssptr->s_file, l_mark);
    if (ssptr->s_scan_type != scanning_none && ssptr->s_da.is_dynamic) {
        gs_string sda;

        sda.data = ssptr->s_da.base;
        sda.size = da_size(&ssptr->s_da);
        RELOC_STRING_VAR(sda);
        ssptr->s_da.limit = sda.data + sda.size;
        ssptr->s_da.next = sda.data + (ssptr->s_da.next - ssptr->s_da.base);
        ssptr->s_da.base = sda.data;
    }
    if (ssptr->s_scan_type == scanning_binary) {
        RELOC_REF_VAR(ssptr->s_ss.binary.bin_array);
        r_clear_attrs(&ssptr->s_ss.binary.bin_array, l_mark);
    }
    RELOC_REF_VAR(ssptr->s_error.object);
    r_clear_attrs(&ssptr->s_error.object, l_mark);
}
RELOC_PTRS_END
/* Structure type */
public_st_scanner_state_dynamic();

/* Initialize a scanner. */
void
gs_scanner_init_options(scanner_state *sstate, const ref *fop, int options)
{
    ref_assign(&sstate->s_file, fop);
    sstate->s_scan_type = scanning_none;
    sstate->s_pstack = 0;
    sstate->s_options = options;
    SCAN_INIT_ERROR(sstate);
}
void gs_scanner_init_stream_options(scanner_state *sstate, stream *s,
                                 int options)
{
    /*
     * The file 'object' will never be accessed, but it must be in correct
     * form for the GC.
     */
    ref fobj;

    make_file(&fobj, a_read, 0, s);
    gs_scanner_init_options(sstate, &fobj, options);
}

/*
 * Return the "error object" to be stored in $error.command instead of
 * --token--, if any, or <0 if no special error object is available.
 */
int
gs_scanner_error_object(i_ctx_t *i_ctx_p, const scanner_state *pstate,
                     ref *pseo)
{
    if (!r_has_type(&pstate->s_error.object, t__invalid)) {
        ref_assign(pseo, &pstate->s_error.object);
        return 0;
    }
    if (pstate->s_error.string[0]) {
        int len = strlen(pstate->s_error.string);

        if (pstate->s_error.is_name) {
            int code = name_ref(imemory, (const byte *)pstate->s_error.string, len, pseo, 1);

            if (code < 0)
                return code;
            r_set_attrs(pseo, a_executable); /* Adobe compatibility */
            return 0;
        } else {
            byte *estr = ialloc_string(len, "gs_scanner_error_object");

            if (estr == 0)
                return -1;              /* VMerror */
            memcpy(estr, (const byte *)pstate->s_error.string, len);
            make_string(pseo, a_all | icurrent_space, len, estr);
            return 0;
        }
    }
    return -1;                  /* no error object */
}

/* Handle a scan_Refill return from gs_scan_token. */
/* This may return o_push_estack, 0 (meaning just call gs_scan_token */
/* again), or an error code. */
int
gs_scan_handle_refill(i_ctx_t *i_ctx_p, scanner_state * sstate,
                   bool save, op_proc_t cont)
{
    const ref *const fop = &sstate->s_file;
    stream *s = fptr(fop);
    uint avail = sbufavailable(s);
    int status;

    if (s->end_status == EOFC) {
        /* More data needed, but none available, so this is a syntax error. */
        return_error(gs_error_syntaxerror);
    }
    status = s_process_read_buf(s);
    if (sbufavailable(s) > avail)
        return 0;
    if (status == 0)
        status = s->end_status;
    switch (status) {
        case EOFC:
            /* We just discovered that we're at EOF. */
            /* Let the caller find this out. */
            return 0;
        case ERRC:
            return_error(gs_error_ioerror);
        case INTC:
        case CALLC:
            {
                ref rstate[1];
                scanner_state *pstate;

                if (save) {
                    pstate = (scanner_state *)
                        ialloc_struct(scanner_state_dynamic, &st_scanner_state_dynamic,
                                      "gs_scan_handle_refill");
                    if (pstate == 0)
                        return_error(gs_error_VMerror);
                    ((scanner_state_dynamic *)pstate)->mem = imemory;
                    *pstate = *sstate;
                } else
                    pstate = sstate;
                make_istruct(&rstate[0], 0, pstate);
                return s_handle_read_exception(i_ctx_p, status, fop,
                                               rstate, 1, cont);
            }
    }
    /* No more data available, but no exception. */
    /* A filter is consuming headers but returns nothing. */
    return 0;
}

/*
 * Handle a comment.  The 'saved' argument is needed only for
 * tracing printout.
 */
static int
scan_comment(i_ctx_t *i_ctx_p, ref *pref, scanner_state *pstate,
             const byte * base, const byte * end, bool saved)
{
    uint len = (uint) (end - base);
    int code;
#ifdef DEBUG
    const char *sstr = (saved ? ">" : "");
#endif

    if (len > 1 && (base[1] == '%' || base[1] == '!')) {
        /* Process as a DSC comment if requested. */
#ifdef DEBUG
        if (gs_debug_c('%')) {
            dmlprintf2(imemory, "[%%%%%s%c]", sstr, (len >= 3 ? '+' : '-'));
            debug_print_string(imemory, base, len);
            dmputs(imemory, "\n");
        }
#endif
        if (gs_scan_dsc_proc != NULL) {
            code = gs_scan_dsc_proc(base, len);
            return (code < 0 ? code : 0);
        }
        if (pstate->s_options & SCAN_PROCESS_DSC_COMMENTS) {
            code = scan_DSC_Comment;
            goto comment;
        }
        /* Treat as an ordinary comment. */
    }
#ifdef DEBUG
    else {
        if (gs_debug_c('%')) {
            dmlprintf2(imemory, "[%% %s%c]", sstr, (len >= 2 ? '+' : '-'));
            debug_print_string(imemory, base, len);
            dmputs(imemory, "\n");
        }
    }
#endif
    if (gs_scan_comment_proc != NULL) {
        code = gs_scan_comment_proc(base, len);
        return (code < 0 ? code : 0);
    }
    if (pstate->s_options & SCAN_PROCESS_COMMENTS) {
        code = scan_Comment;
        goto comment;
    }
    return 0;
 comment:
    {
        byte *cstr = ialloc_string(len, "scan_comment");

        if (cstr == 0)
            return_error(gs_error_VMerror);
        memcpy(cstr, base, len);
        make_string(pref, a_all | icurrent_space, len, cstr);
    }
    return code;
}

/* Read a token from a string. */
/* Update the string if succesful. */
/* Store the error object in i_ctx_p->error_object if not. */
int
gs_scan_string_token_options(i_ctx_t *i_ctx_p, ref * pstr, ref * pref,
                             int options)
{
    stream st;
    stream *s = &st;
    scanner_state state;
    int code;

    if (!r_has_attr(pstr, a_read))
        return_error(gs_error_invalidaccess);
    s_init(s, NULL);
    sread_string(s, pstr->value.bytes, r_size(pstr));
    gs_scanner_init_stream_options(&state, s, options | SCAN_FROM_STRING);
    switch (code = gs_scan_token(i_ctx_p, pref, &state)) {
        default:                /* error or comment */
            if (code < 0)
                break;
            /* falls through */
        case 0:         /* read a token */
        case scan_BOS:
            {
                uint pos = stell(s);

                pstr->value.bytes += pos;
                r_dec_size(pstr, pos);
            }
            break;
        case scan_Refill:       /* error */
            code = gs_note_error(gs_error_syntaxerror);
        case scan_EOF:
            break;
    }
    if (code < 0)
        gs_scanner_error_object(i_ctx_p, &state, &i_ctx_p->error_object);
    return code;
}

/*
 * Read a token from a stream.  Return 0 if an ordinary token was read,
 * >0 for special situations (see iscan.h).
 * If the token required a terminating character (i.e., was a name or
 * number) and the next character was whitespace, read and discard
 * that character.  Note that the state is relevant for gs_error_VMerror
 * as well as for scan_Refill.
 */
int
gs_scan_token(i_ctx_t *i_ctx_p, ref * pref, scanner_state * pstate)
{
    stream *const s = pstate->s_file.value.pfile;
    ref *myref = pref;
    int retcode = 0;
    int c;

    s_declare_inline(s, sptr, endptr);
    const byte *newptr;
    byte *daptr;

#define sreturn(code)\
  { retcode = gs_note_error(code); goto sret; }
#define if_not_spush1()\
  if ( osp < ostop ) osp++;\
  else if ( (retcode = ref_stack_push(&o_stack, 1)) >= 0 )\
    ;\
  else
#define spop1()\
  if ( osp >= osbot ) osp--;\
  else ref_stack_pop(&o_stack, 1)
    int max_name_ctype =
        ((ref_binary_object_format.value.intval != 0 && level2_enabled)? ctype_name : ctype_btoken);

#define scan_sign(sign, ptr)\
  switch ( *ptr ) {\
    case '-': sign = -1; ptr++; break;\
    case '+': sign = 1; ptr++; break;\
    default: sign = 0;\
  }
#define refill2_back(styp,nback)\
  BEGIN sptr -= nback; sstate.s_scan_type = styp; goto pause; END
#define ensure2_back(styp,nback)\
  if ( sptr >= endptr ) refill2_back(styp,nback)
#define ensure2(styp) ensure2_back(styp, 1)
#define refill2(styp) refill2_back(styp, 1)
    byte s1[2];
    const byte *const decoder = scan_char_decoder;
    int status;
    int sign;
    const bool check_only = (pstate->s_options & SCAN_CHECK_ONLY) != 0;
    const bool PDFScanRules = (i_ctx_p->scanner_options & SCAN_PDF_RULES) != 0;
    /*
     * The following is a hack so that ^D will be self-delimiting in PS files
     * (to compensate for bugs in some PostScript-generating applications)
     * but not in strings (to match CPSI on the CET) or PDF.
     */
    const int ctrld = (pstate->s_options & SCAN_FROM_STRING ||
                      PDFScanRules ? 0x04 : 0xffff);
    scanner_state sstate;

    sptr = endptr = NULL; /* Quiet compiler */
    if (pstate->s_pstack != 0) {
        if_not_spush1()
            return retcode;
        myref = osp;
    }
    /* Check whether we are resuming after an interruption. */
    if (pstate->s_scan_type != scanning_none) {
        sstate = *pstate;
        if (!sstate.s_da.is_dynamic && sstate.s_da.base != sstate.s_da.buf) {
            /* The sstate.s_da contains some self-referencing pointers. */
            /* Fix them up now. */
            uint next = sstate.s_da.next - sstate.s_da.base;
            uint limit = sstate.s_da.limit - sstate.s_da.base;

            sstate.s_da.base = sstate.s_da.buf;
            sstate.s_da.next = sstate.s_da.buf + next;
            sstate.s_da.limit = sstate.s_da.buf + limit;
        }
        daptr = sstate.s_da.next;
        switch (sstate.s_scan_type) {
            case scanning_binary:
                retcode = (*sstate.s_ss.binary.cont)
                    (i_ctx_p, myref, &sstate);
                s_begin_inline(s, sptr, endptr);
                if (retcode == scan_Refill)
                    goto pause;
                goto sret;
            case scanning_comment:
                s_begin_inline(s, sptr, endptr);
                goto cont_comment;
            case scanning_name:
                goto cont_name;
            case scanning_string:
                goto cont_string;
            default:
                return_error(gs_error_Fatal);
        }
    }
    /* Fetch any state variables that are relevant even if */
    /* sstate.s_scan_type == scanning_none. */
    sstate.s_pstack = pstate->s_pstack;
    sstate.s_pdepth = pstate->s_pdepth;
    ref_assign(&sstate.s_file, &pstate->s_file);
    sstate.s_options = pstate->s_options;
    SCAN_INIT_ERROR(&sstate);
    s_begin_inline(s, sptr, endptr);
    /*
     * Loop invariants:
     *      If sstate.s_pstack != 0, myref = osp, and *osp is a valid slot.
     */
  top:c = sgetc_inline(s, sptr, endptr);
    if_debug1m('S', imemory, (c >= 32 && c <= 126 ? "`%c'" : c >= 0 ? "`\\%03o'" : "`%d'"), c);
    switch (c) {
        case ' ':
        case '\f':
        case '\t':
        case char_CR:
        case char_EOL:
        case char_NULL:
            goto top;
        case 0x04:              /* see ctrld above */
            if (c == ctrld)     /* treat as ordinary name char */
                goto begin_name;
            /* fall through */
        case '[':
        case ']':
            s1[0] = (byte) c;
            retcode = name_ref(imemory, s1, 1, myref, 1);       /* can't fail */
            r_set_attrs(myref, a_executable);
            break;
        case '<':
            if (level2_enabled) {
                ensure2(scanning_none);
                c = sgetc_inline(s, sptr, endptr);
                switch (c) {
                    case '<':
                        sputback_inline(s, sptr, endptr);
                        sstate.s_ss.s_name.s_name_type = 0;
                        sstate.s_ss.s_name.s_try_number = false;
                        goto try_funny_name;
                    case '~':
                        s_A85D_init_inline(&sstate.s_ss.a85d);
                        sstate.s_ss.st.templat = &s_A85D_template;
                        sstate.s_ss.a85d.require_eod = true;
                        goto str;
                }
                sputback_inline(s, sptr, endptr);
            }
            (void)s_AXD_init_inline(&sstate.s_ss.axd);
            sstate.s_ss.st.templat = &s_AXD_template;
          str:s_end_inline(s, sptr, endptr);
            dynamic_init(&sstate.s_da, imemory);
          cont_string:for (;;) {
                stream_cursor_write w;

                w.ptr = sstate.s_da.next - 1;
                w.limit = sstate.s_da.limit - 1;
                status = (*sstate.s_ss.st.templat->process)
                    (&sstate.s_ss.st, &s->cursor.r, &w,
                     s->end_status == EOFC);
                if (!check_only)
                    sstate.s_da.next = w.ptr + 1;
                switch (status) {
                    case 0:
                        status = s->end_status;
                        if (status < 0) {
                            if (status == EOFC) {
                                if (check_only) {
                                    retcode = scan_Refill;
                                    sstate.s_scan_type = scanning_string;
                                    goto suspend;
                                } else
                                    sreturn(gs_error_syntaxerror);
                            }
                            break;
                        }
                        s_process_read_buf(s);
                        continue;
                    case 1:
                        if (!check_only) {
                            retcode = dynamic_grow(&sstate.s_da, sstate.s_da.next, max_string_size);
                            if (retcode == gs_error_VMerror) {
                                sstate.s_scan_type = scanning_string;
                                goto suspend;
                            } else if (retcode < 0)
                                sreturn(retcode);
                        }
                        continue;
                }
                break;
            }
            s_begin_inline(s, sptr, endptr);
            switch (status) {
                default:
                    /*case ERRC: */
                    sreturn(gs_error_syntaxerror);
                case INTC:
                case CALLC:
                    sstate.s_scan_type = scanning_string;
                    goto pause;
                case EOFC:
                    ;
            }
            retcode = dynamic_make_string(i_ctx_p, myref, &sstate.s_da, sstate.s_da.next);
            if (retcode < 0) {  /* VMerror */
                sputback(s);    /* rescan ) */
                sstate.s_scan_type = scanning_string;
                goto suspend;
            }
            break;
        case '(':
            sstate.s_ss.pssd.from_string =
                ((pstate->s_options & SCAN_FROM_STRING) != 0) &&
                !level2_enabled;
            s_PSSD_partially_init_inline(&sstate.s_ss.pssd);
            sstate.s_ss.st.templat = &s_PSSD_template;
            goto str;
        case '{':
            if (sstate.s_pstack == 0) {  /* outermost procedure */
                if_not_spush1() {
                    sputback_inline(s, sptr, endptr);
                    sstate.s_scan_type = scanning_none;
                    goto pause_ret;
                }
                sstate.s_pdepth = ref_stack_count_inline(&o_stack);
            }
            make_int(osp, sstate.s_pstack);
            sstate.s_pstack = ref_stack_count_inline(&o_stack);
            if_debug3m('S', imemory, "[S{]d=%d, s=%d->%d\n",
                       sstate.s_pdepth, (int)osp->value.intval, sstate.s_pstack);
            goto snext;
        case '>':
            if (level2_enabled) {
                ensure2(scanning_none);
                sstate.s_ss.s_name.s_name_type = 0;
                sstate.s_ss.s_name.s_try_number = false;
                goto try_funny_name;
            }
            /* falls through */
        case ')':
            sreturn(gs_error_syntaxerror);
        case '}':
            if (sstate.s_pstack == 0)
                sreturn(gs_error_syntaxerror);
            osp--;
            {
                uint size = ref_stack_count_inline(&o_stack) - sstate.s_pstack;
                ref arr;

                if_debug4m('S', imemory, "[S}]d=%"PRIu32", s=%"PRIu32"->%"PRIpsint", c=%"PRIu32"\n",
                           sstate.s_pdepth, sstate.s_pstack,
                           (sstate.s_pstack == sstate.s_pdepth ? 0 :
                           ref_stack_index(&o_stack, size)->value.intval),
                           size + sstate.s_pstack);
                if (size > max_array_size)
                    sreturn(gs_error_limitcheck);
                myref = (sstate.s_pstack == sstate.s_pdepth ? pref : &arr);
                if (check_only) {
                    make_empty_array(myref, 0);
                    ref_stack_pop(&o_stack, size);
                } else if (ref_array_packing.value.boolval) {
                    retcode = make_packed_array(myref, &o_stack, size,
                                                idmemory, "scanner(packed)");
                    if (retcode < 0) {  /* must be VMerror */
                        osp++;
                        sputback_inline(s, sptr, endptr);
                        sstate.s_scan_type = scanning_none;
                        goto pause_ret;
                    }
                    r_set_attrs(myref, a_executable);
                } else {
                    retcode = ialloc_ref_array(myref,
                                               a_executable + a_all, size,
                                               "scanner(proc)");
                    if (retcode < 0) {  /* must be VMerror */
                        osp++;
                        sputback_inline(s, sptr, endptr);
                        sstate.s_scan_type = scanning_none;
                        goto pause_ret;
                    }
                    retcode = ref_stack_store(&o_stack, myref, size, 0, 1,
                                              false, idmemory, "scanner");
                    if (retcode < 0) {
                        ifree_ref_array(myref, "scanner(proc)");
                        sreturn(retcode);
                    }
                    ref_stack_pop(&o_stack, size);
                }
                if (sstate.s_pstack == sstate.s_pdepth) {         /* This was the top-level procedure. */
                    spop1();
                    sstate.s_pstack = 0;
                } else {
                    if (osp < osbot)
                        ref_stack_pop_block(&o_stack);
                    sstate.s_pstack = osp->value.intval;
                    *osp = arr;
                    goto snext;
                }
            }
            break;
        case '/':
            /*
             * If the last thing in the input is a '/', don't try to read
             * any more data.
             */
            if (sptr >= endptr && s->end_status != EOFC) {
                refill2(scanning_none);
            }
            c = sgetc_inline(s, sptr, endptr);
            if (!PDFScanRules && (c == '/')) {
                sstate.s_ss.s_name.s_name_type = 2;
                c = sgetc_inline(s, sptr, endptr);
            } else
                sstate.s_ss.s_name.s_name_type = 1;
            sstate.s_ss.s_name.s_try_number = false;
            switch (decoder[c]) {
                case ctype_name:
                default:
                    goto do_name;
                case ctype_btoken:
                    if (!(ref_binary_object_format.value.intval != 0 && level2_enabled))
                        goto do_name;
                    /* otherwise, an empty name */
                case ctype_exception:
                case ctype_space:
                    /*
                     * Amazingly enough, the Adobe implementations don't accept
                     * / or // followed by [, ], <<, or >>, so we do the same.
                     * (Older versions of our code had a ctype_other case here
                     * that handled these specially.)
                     */
                case ctype_other:
                    if (c == ctrld) /* see above */
                        goto do_name;
                    sstate.s_da.base = sstate.s_da.limit = daptr = 0;
                    sstate.s_da.is_dynamic = false;
                    goto nx;
            }
        case '%':
            {                   /* Scan as much as possible within the buffer. */
                const byte *base = sptr;
                const byte *end;

                while (++sptr < endptr)         /* stop 1 char early */
                    switch (*sptr) {
                        case char_CR:
                            end = sptr;
                            if (sptr[1] == char_EOL)
                                sptr++;
                          cend: /* Check for externally processed comments. */
                            retcode = scan_comment(i_ctx_p, myref, &sstate,
                                                   base, end, false);
                            if (retcode != 0)
                                goto comment;
                            goto top;
                        case char_EOL:
                        case '\f':
                            end = sptr;
                            goto cend;
                    }
                /*
                 * We got to the end of the buffer while inside a comment.
                 * If there is a possibility that we must pass the comment
                 * to an external procedure, move what we have collected
                 * so far into a private buffer now.
                 */
                --sptr;
                sstate.s_da.buf[1] = 0;
                {
                    /* Could be an externally processable comment. */
                    uint len = sptr + 1 - base;
                    if (len > sizeof(sstate.s_da.buf))
                        len = sizeof(sstate.s_da.buf);

                    memcpy(sstate.s_da.buf, base, len);
                    daptr = sstate.s_da.buf + len;
                }
                sstate.s_da.base = sstate.s_da.buf;
                sstate.s_da.is_dynamic = false;
            }
            /* Enter here to continue scanning a comment. */
            /* daptr must be set. */
          cont_comment:for (;;) {
                switch ((c = sgetc_inline(s, sptr, endptr))) {
                    default:
                        if (c < 0)
                            switch (c) {
                                case INTC:
                                case CALLC:
                                    sstate.s_da.next = daptr;
                                    sstate.s_scan_type = scanning_comment;
                                    goto pause;
                                case EOFC:
                                    /*
                                     * One would think that an EOF in a comment
                                     * should be a syntax error, but there are
                                     * quite a number of files that end that way.
                                     */
                                    goto end_comment;
                                default:
                                    sreturn(gs_error_syntaxerror);
                            }
                        if (daptr < sstate.s_da.buf + max_comment_line)
                            *daptr++ = c;
                        continue;
                    case char_CR:
                    case char_EOL:
                    case '\f':
                      end_comment:
                        retcode = scan_comment(i_ctx_p, myref, &sstate,
                                               sstate.s_da.buf, daptr, true);
                        if (retcode != 0)
                            goto comment;
                        goto top;
                }
            }
            /*NOTREACHED */
        case EOFC:
            if (sstate.s_pstack != 0) {
                if (check_only)
                    goto pause;
                sreturn(gs_error_syntaxerror);
            }
            retcode = scan_EOF;
            break;
        case ERRC:
            sreturn(gs_error_ioerror);

            /* Check for a Level 2 funny name (<< or >>). */
            /* c is '<' or '>'.  We already did an ensure2. */
          try_funny_name:
            {
                int c1 = sgetc_inline(s, sptr, endptr);

                if (c1 == c) {
                    s1[0] = s1[1] = c;
                    name_ref(imemory, s1, 2, myref, 1); /* can't fail */
                    goto have_name;
                }
                sputback_inline(s, sptr, endptr);
            }
            sreturn(gs_error_syntaxerror);

            /* Handle separately the names that might be a number. */
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '.':
            sign = 0;
    nr:     /*
             * Skip a leading sign, if any, by conditionally passing
             * sptr + 1 rather than sptr.  Also, if the last character
             * in the buffer is a CR, we must stop the scan 1 character
             * early, to be sure that we can test for CR+LF within the
             * buffer, by passing endptr rather than endptr + 1.
             */
            retcode = scan_number(sptr + (sign & 1),
                    endptr /*(*endptr == char_CR ? endptr : endptr + 1) */ ,
                                  sign, myref, &newptr, i_ctx_p->scanner_options);
            if (retcode == 1 && decoder[newptr[-1]] == ctype_space) {
                sptr = newptr - 1;
                if (*sptr == char_CR && sptr[1] == char_EOL)
                    sptr++;
                retcode = 0;
                ref_mark_new(myref);
                break;
            }
            sstate.s_ss.s_name.s_name_type = 0;
            sstate.s_ss.s_name.s_try_number = true;
            goto do_name;
        case '+':
            sign = 1;
            goto nr;
        case '-':
            sign = -1;
            if(i_ctx_p->scanner_options & SCAN_PDF_INV_NUM) {
                do {
                    if (*(sptr + 1) == '-') {
                        sptr++;
                    } else
                        break;
                } while (1);
            }
            goto nr;

            /* Check for a binary object */
          case 128: case 129: case 130: case 131: case 132: case 133: case 134: case 135:
          case 136: case 137: case 138: case 139: case 140: case 141: case 142: case 143:
          case 144: case 145: case 146: case 147: case 148: case 149: case 150: case 151:
          case 152: case 153: case 154: case 155: case 156: case 157: case 158: case 159:
            if ((ref_binary_object_format.value.intval != 0 && level2_enabled)) {
                s_end_inline(s, sptr, endptr);
                retcode = scan_binary_token(i_ctx_p, myref, &sstate);
                s_begin_inline(s, sptr, endptr);
                if (retcode == scan_Refill)
                    goto pause;
                break;
            }
            /* Not a binary object, fall through. */

            /* The default is a name. */
        default:
            if (c < 0) {
                dynamic_init(&sstate.s_da, name_memory(imemory));        /* sstate.s_da state must be clean */
                sstate.s_scan_type = scanning_none;
                goto pause;
            }
            /* Populate the switch with enough cases to force */
            /* simple compilers to use a dispatch rather than tests. */
        case '!':
        case '"':
        case '#':
        case '$':
        case '&':
        case '\'':
        case '*':
        case ',':
        case '=':
        case ':':
        case ';':
        case '?':
        case '@':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case 'G':
        case 'H':
        case 'I':
        case 'J':
        case 'K':
        case 'L':
        case 'M':
        case 'N':
        case 'O':
        case 'P':
        case 'Q':
        case 'R':
        case 'S':
        case 'T':
        case 'U':
        case 'V':
        case 'W':
        case 'X':
        case 'Y':
        case 'Z':
        case '\\':
        case '^':
        case '_':
        case '`':
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h':
        case 'i':
        case 'j':
        case 'k':
        case 'l':
        case 'm':
        case 'n':
        case 'o':
        case 'p':
        case 'q':
        case 'r':
        case 's':
        case 't':
        case 'u':
        case 'v':
        case 'w':
        case 'x':
        case 'y':
        case 'z':
        case '|':
        case '~':
          begin_name:
            /* Common code for scanning a name. */
            /* sstate.s_ss.s_name.s_try_number and sstate.s_ss.s_name.s_name_type are already set. */
            /* We know c has ctype_name (or maybe ctype_btoken, */
            /* or is ^D) or is a digit. */
            sstate.s_ss.s_name.s_name_type = 0;
            sstate.s_ss.s_name.s_try_number = false;
          do_name:
            /* Try to scan entirely within the stream buffer. */
            /* We stop 1 character early, so we don't switch buffers */
            /* looking ahead if the name is terminated by \r\n. */
            sstate.s_da.base = (byte *) sptr;
            sstate.s_da.is_dynamic = false;
            {
                const byte *endp1 = endptr - 1;

                do {
                    if (sptr >= endp1)  /* stop 1 early! */
                        goto dyn_name;
                }
                while (decoder[*++sptr] <= max_name_ctype || *sptr == ctrld);   /* digit or name */
            }
            /* Name ended within the buffer. */
            daptr = (byte *) sptr;
            c = *sptr;
            goto nx;
          dyn_name:             /* Name extended past end of buffer. */
            s_end_inline(s, sptr, endptr);
            /* Initialize the dynamic area. */
            /* We have to do this before the next */
            /* sgetc, which will overwrite the buffer. */
            sstate.s_da.limit = (byte *)++ sptr;
            sstate.s_da.memory = name_memory(imemory);
            retcode = dynamic_grow(&sstate.s_da, sstate.s_da.limit, name_max_string);
            if (retcode < 0) {
                dynamic_save(&sstate.s_da);
                if (retcode != gs_error_VMerror)
                    sreturn(retcode);
                sstate.s_scan_type = scanning_name;
                goto pause_ret;
            }
            daptr = sstate.s_da.next;
            /* Enter here to continue scanning a name. */
            /* daptr must be set. */
          cont_name:s_begin_inline(s, sptr, endptr);
            while (decoder[c = sgetc_inline(s, sptr, endptr)] <= max_name_ctype || c == ctrld) {
                if (daptr == sstate.s_da.limit) {
                    retcode = dynamic_grow(&sstate.s_da, daptr,
                                           name_max_string);
                    if (retcode < 0) {
                        dynamic_save(&sstate.s_da);
                        if (retcode != gs_error_VMerror)
                            sreturn(retcode);
                        sputback_inline(s, sptr, endptr);
                        sstate.s_scan_type = scanning_name;
                        goto pause_ret;
                    }
                    daptr = sstate.s_da.next;
                }
                *daptr++ = c;
            }
          nx:switch (decoder[c]) {
                case ctype_other:
                    if (c == ctrld) /* see above */
                        break;
                case ctype_btoken:
                    sputback_inline(s, sptr, endptr);
                    break;
                case ctype_space:
                    /* Check for \r\n */
                    if (c == char_CR) {
                        if (sptr >= endptr) {   /* ensure2 *//* We have to check specially for */
                            /* the case where the very last */
                            /* character of a file is a CR. */
                            if (s->end_status != EOFC) {
                                sptr--;
                                goto pause_name;
                            }
                        } else if (sptr[1] == char_EOL)
                            sptr++;
                    }
                    break;
                case ctype_exception:
                    switch (c) {
                        case INTC:
                        case CALLC:
                            goto pause_name;
                        case ERRC:
                            sreturn(gs_error_ioerror);
                        case EOFC:
                            break;
                    }
            }
            /* Check for a number */
            if (sstate.s_ss.s_name.s_try_number) {
                const byte *base = sstate.s_da.base;

                scan_sign(sign, base);
                retcode = scan_number(base, daptr, sign, myref, &newptr, i_ctx_p->scanner_options);
                if (retcode == 1) {
                    ref_mark_new(myref);
                    retcode = 0;
                } else if (retcode != gs_error_syntaxerror) {
                    dynamic_free(&sstate.s_da);
                    if (sstate.s_ss.s_name.s_name_type == 2)
                        sreturn(gs_error_syntaxerror);
                    break;      /* might be gs_error_limitcheck */
                }
            }
            if (sstate.s_da.is_dynamic) {        /* We've already allocated the string on the heap. */
                uint size = daptr - sstate.s_da.base;

                retcode = name_ref(imemory, sstate.s_da.base, size, myref, -1);
                if (retcode >= 0) {
                    dynamic_free(&sstate.s_da);
                } else {
                    retcode = dynamic_resize(&sstate.s_da, size);
                    if (retcode < 0) {  /* VMerror */
                        if (c != EOFC)
                            sputback_inline(s, sptr, endptr);
                        sstate.s_scan_type = scanning_name;
                        goto pause_ret;
                    }
                    retcode = name_ref(imemory, sstate.s_da.base, size, myref, 2);
                }
            } else {
                retcode = name_ref(imemory, sstate.s_da.base, (uint) (daptr - sstate.s_da.base),
                                   myref, !s->foreign);
            }
            /* Done scanning.  Check for preceding /'s. */
            if (retcode < 0) {
                if (retcode != gs_error_VMerror)
                    sreturn(retcode);
                if (!sstate.s_da.is_dynamic) {
                    sstate.s_da.next = daptr;
                    dynamic_save(&sstate.s_da);
                }
                if (c != EOFC)
                    sputback_inline(s, sptr, endptr);
                sstate.s_scan_type = scanning_name;
                goto pause_ret;
            }
          have_name:switch (sstate.s_ss.s_name.s_name_type) {
                case 0: /* ordinary executable name */
                    if (r_has_type(myref, t_name))      /* i.e., not a number */
                        r_set_attrs(myref, a_executable);
                case 1: /* quoted name */
                    break;
                case 2: /* immediate lookup */
                    {
                        ref *pvalue;

                        if (!r_has_type(myref, t_name) ||
                            (pvalue = dict_find_name(myref)) == 0) {
                            ref_assign(&sstate.s_error.object, myref);
                            r_set_attrs(&sstate.s_error.object,
                                a_executable); /* Adobe compatibility */
                            sreturn(gs_error_undefined);
                        }
                        if (sstate.s_pstack != 0 &&
                            r_space(pvalue) > ialloc_space(idmemory)
                            )
                            sreturn(gs_error_invalidaccess);
                        ref_assign_new(myref, pvalue);
                    }
            }
    }
  sret:if (retcode < 0) {
        s_end_inline(s, sptr, endptr);
        pstate->s_error = sstate.s_error;
        if (sstate.s_pstack != 0) {
            if (retcode == gs_error_undefined)
                *pref = *osp;   /* return undefined name as error token */
            ref_stack_pop(&o_stack,
                          ref_stack_count(&o_stack) - (sstate.s_pdepth - 1));
        }
        return retcode;
    }
    /* If we are at the top level, return the object, */
    /* otherwise keep going. */
    if (sstate.s_pstack == 0) {
        s_end_inline(s, sptr, endptr);
        return retcode;
    }
  snext:if_not_spush1() {
        s_end_inline(s, sptr, endptr);
        sstate.s_scan_type = scanning_none;
        goto save;
    }
    myref = osp;
    goto top;

    /* Pause for an interrupt or callout. */
  pause_name:
    /* If we're still scanning within the stream buffer, */
    /* move the characters to the private buffer (sstate.s_da.buf) now. */
    sstate.s_da.next = daptr;
    dynamic_save(&sstate.s_da);
    sstate.s_scan_type = scanning_name;
  pause:
    retcode = scan_Refill;
  pause_ret:
    s_end_inline(s, sptr, endptr);
  suspend:
    if (sstate.s_pstack != 0)
        osp--;                  /* myref */
  save:
    *pstate = sstate;
    return retcode;

    /* Handle a scanned comment. */
 comment:
    if (retcode < 0)
        goto sret;
    s_end_inline(s, sptr, endptr);
    sstate.s_scan_type = scanning_none;
    goto save;
}
