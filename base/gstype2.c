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


/* Adobe Type 2 charstring interpreter */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxarith.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxcoord.h"
#include "gxgstate.h"
#include "gxpath.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxtype1.h"
#include "gxhintn.h"

/* NOTE: The following are not yet implemented:
 *	Registry items other than 0
 *	Counter masks (but they are parsed correctly)
 *	'random' operator
 */

/* ------ Internal routines ------ */

/*
 * Set the character width.  This is provided as an optional extra operand
 * on the stack for the first operator.  After setting the width, we remove
 * the extra operand, and back up the interpreter pointer so we will
 * re-execute the operator when control re-enters the interpreter.
 */
#define check_first_operator(explicit_width)\
  BEGIN\
    if ( pcis->init_done < 0 )\
      { ipsp->ip = cip, ipsp->dstate = state;\
        ipsp->ip_end = endp;\
        return type2_sbw(pcis, csp, cstack, ipsp, explicit_width);\
      }\
  END
static int
type2_sbw(gs_type1_state * pcis, cs_ptr csp, cs_ptr cstack, ip_state_t * ipsp,
          bool explicit_width)
{
    t1_hinter *h = &pcis->h;
    fixed sbx = fixed_0, sby = fixed_0, wx, wy = fixed_0;
    int code;

    if (explicit_width) {
        wx = cstack[0] + pcis->pfont->data.nominalWidthX;
        memmove(cstack, cstack + 1, (csp - cstack) * sizeof(*cstack));
        --csp;
    } else
        wx = pcis->pfont->data.defaultWidthX;
    if (pcis->seac_accent < 0) {
        if (pcis->sb_set) {
            pcis->origin_offset.x = pcis->lsb.x - sbx;
            pcis->origin_offset.y = pcis->lsb.y - sby;
            sbx = pcis->lsb.x;
            sby = pcis->lsb.y;
        }
        if (pcis->width_set) {
            wx = pcis->width.x;
            wy = pcis->width.y;
        }
    }
    code = t1_hinter__sbw(h, sbx, sby, wx, wy);
    if (code < 0)
        return code;
    gs_type1_sbw(pcis, fixed_0, fixed_0, wx, fixed_0);
    /* Back up the interpretation pointer. */
    ipsp->ip--;
    decrypt_skip_previous(*ipsp->ip, ipsp->dstate);
    /* Save the interpreter state. */
    pcis->os_count = csp + 1 - cstack;
    pcis->ips_count = ipsp - &pcis->ipstack[0] + 1;
    memcpy(pcis->ostack, cstack, pcis->os_count * sizeof(cstack[0]));
    if (pcis->init_done < 0) {	/* Finish init when we return. */
        pcis->init_done = 0;
    }
    return type1_result_sbw;
}
static int
type2_vstem(gs_type1_state * pcis, cs_ptr csp, cs_ptr cstack)
{
    fixed x = 0;
    cs_ptr ap;
    t1_hinter *h = &pcis->h;
    int code;

    for (ap = cstack; ap + 1 <= csp; x += ap[1], ap += 2) {
        code = t1_hinter__vstem(h, x += ap[0], ap[1]);
        if (code < 0)
            return code;
    }
    pcis->num_hints += (csp + 1 - cstack) >> 1;
    return 0;
}

/* ------ Main interpreter ------ */
/*
 * Continue interpreting a Type 2 charstring.  If str != 0, it is taken as
 * the byte string to interpret.  Return 0 on successful completion, <0 on
 * error, or >0 when client intervention is required (or allowed).  The int*
 * argument is only for compatibility with the Type 1 charstring interpreter.
 */
int
gs_type2_interpret(gs_type1_state * pcis, const gs_glyph_data_t *pgd,
                   int *ignore_pindex)
{
    gs_font_type1 *pfont = pcis->pfont;
    gs_type1_data *pdata = &pfont->data;
    t1_hinter *h = &pcis->h;
    bool encrypted = pdata->lenIV >= 0;
    fixed cstack[ostack_size];
    cs_ptr csp;
#define clear CLEAR_CSTACK(cstack, csp)
    ip_state_t *ipsp = &pcis->ipstack[pcis->ips_count - 1];
    register const byte *cip, *endp = NULL;
    register crypt_state state;
    register int c;
    cs_ptr ap;
    bool vertical;
    int code = 0;

/****** FAKE THE REGISTRY ******/
    struct {
        float *values;
        uint size;
    } Registry[1];

    Registry[0].values = pcis->pfont->data.WeightVector.values;

    switch (pcis->init_done) {
        case -1:
            t1_hinter__init(h, pcis->path);
            break;
        case 0:
            gs_type1_finish_init(pcis);	/* sets origin */
            code = t1_hinter__set_mapping(h, &pcis->pgs->ctm,
                            &pfont->FontMatrix, &pfont->base->FontMatrix,
                            pcis->scale.x.log2_unit, pcis->scale.x.log2_unit,
                            pcis->scale.x.log2_unit - pcis->log2_subpixels.x,
                            pcis->scale.y.log2_unit - pcis->log2_subpixels.y,
                            pcis->origin.x, pcis->origin.y,
                            gs_currentaligntopixels(pfont->dir));
            if (code < 0)
                return code;
            code = t1_hinter__set_font_data(pfont->memory, h, 2, pdata, pcis->no_grid_fitting,
                            pcis->pfont->is_resource);
            if (code < 0)
                return code;
            break;
        default /*case 1 */ :
            break;
    }
    INIT_CSTACK(cstack, csp, pcis);

    if (pgd == 0)
        goto cont;
    ipsp->cs_data = *pgd;
    cip = pgd->bits.data;
    if (cip == 0)
        return (gs_note_error(gs_error_invalidfont));
    endp = cip + pgd->bits.size;
    goto call;
    for (;;) {
        uint c0;

        if (endp != NULL && cip > endp)
            return_error(gs_error_invalidfont);

        c0 = *cip++;

        charstring_next(c0, state, c, encrypted);
        if (c >= c_num1) {
            /* This is a number, decode it and push it on the stack. */

            if (c < c_pos2_0) {	/* 1-byte number */
                decode_push_num1(csp, cstack, c);
            } else if (c < cx_num4) {	/* 2-byte number */
                decode_push_num2(csp, cstack, c, cip, state, encrypted);
            } else if (c == cx_num4) {	/* 4-byte number */
                long lw;

                decode_num4(lw, cip, state, encrypted);
                /* 32-bit numbers are 16:16. */
                CS_CHECK_PUSH(csp, cstack);
                *++csp = arith_rshift(lw, 16 - _fixed_shift);
            } else		/* not possible */
                return_error(gs_error_invalidfont);
            goto pushed;
        }
#ifdef DEBUG
        if (gs_debug['1']) {
            static const char *const c2names[] =
            {char2_command_names};

            if (c2names[c] == 0)
                dmlprintf2(pfont->memory, "[1]"PRI_INTPTR": %02x??\n",
                           (intptr_t)(cip - 1), c);
            else
                dmlprintf3(pfont->memory, "[1]"PRI_INTPTR": %02x %s\n",
                           (intptr_t)(cip - 1), c, c2names[c]);
        }
#endif
        switch ((char_command) c) {

                /* Commands with identical functions in Type 1 and Type 2, */
                /* except for 'escape'. */

            case c_undef0:
            case c_undef2:
            case c_undef17:
                return_error(gs_error_invalidfont);
            case c_callsubr:
                if (CS_CHECK_CSTACK_BOUNDS(csp, cstack)) {
                    CS_CHECK_IPSTACK(&(ipsp[1]), pcis->ipstack);
                    c = fixed2int_var(*csp) + pdata->subroutineNumberBias;
                    code = pdata->procs.subr_data
                        (pfont, c, false, &ipsp[1].cs_data);
                    goto subr;
                }
                else {
                    /* Consider a missing index to be "out-of-range", and see above
                     * comment.
                     */
                    cip++;
                    continue;
                }
            case c_return:
                gs_glyph_data_free(&ipsp->cs_data, "gs_type2_interpret");
                --ipsp;
  cont:         if (ipsp < pcis->ipstack || ipsp->ip == 0)
                    return (gs_note_error(gs_error_invalidfont));
                cip = ipsp->ip;
                endp = ipsp->ip_end;
                state = ipsp->dstate;
                continue;
            case c_undoc15:
                /* See gstype1.h for information on this opcode. */
                clear;
                continue;

                /* Commands with similar but not identical functions */
                /* in Type 1 and Type 2 charstrings. */

            case cx_hstem:
                goto hstem;
            case cx_vstem:
                goto vstem;
            case cx_vmoveto:
                if (CS_CHECK_CSTACK_BOUNDS(csp, cstack)) {
                    check_first_operator(csp > cstack);
                    code = t1_hinter__rmoveto(h, 0, *csp);
                    goto move;
                }
                else {
                    return_error(gs_error_invalidfont);
                }
            case cx_rlineto:
                for (ap = cstack; ap + 1 <= csp; ap += 2) {
                    code = t1_hinter__rlineto(h, ap[0], ap[1]);
                    if (code < 0)
                        return code;
                }
                goto pp;
            case cx_hlineto:
                vertical = false;
                goto hvl;
            case cx_vlineto:
                vertical = true;
              hvl:for (ap = cstack; ap <= csp; vertical = !vertical, ++ap) {
                    if (vertical) {
                        code = t1_hinter__rlineto(h, 0, ap[0]);
                    } else {
                        code = t1_hinter__rlineto(h, ap[0], 0);
                    }
                    if (code < 0)
                        return code;
                }
                goto pp;
            case cx_rrcurveto:
                for (ap = cstack; ap + 5 <= csp; ap += 6) {
                    code = t1_hinter__rcurveto(h, ap[0], ap[1], ap[2],
                                            ap[3], ap[4], ap[5]);
                    if (code < 0)
                        return code;
                }
                goto pp;
            case cx_endchar:
                /*
                 * It is a feature of Type 2 CharStrings that if endchar is
                 * invoked with 4 or 5 operands, it is equivalent to the
                 * Type 1 seac operator. In this case, the asb operand of
                 * seac is missing: we assume it is the same as the
                 * l.s.b. of the accented character.  This feature was
                 * undocumented until the 16 March 2000 version of the Type
                 * 2 Charstring Format specification, but, thankfully, is
                 * described in that revision.
                 */
                if (csp >= cstack + 3) {
                    check_first_operator(csp > cstack + 3);
                    code = gs_type1_seac(pcis, cstack, 0, ipsp);
                    if (code < 0)
                        return code;
                    clear;
                    cip = ipsp->cs_data.bits.data;
                    goto call;
                }
                /*
                 * This might be the only operator in the charstring.
                 * In this case, there might be a width on the stack.
                 */
                check_first_operator(csp >= cstack);
                if (pcis->seac_accent < 0) {
                    code = t1_hinter__endglyph(h);
                    if (code < 0)
                        return code;
                    code = gx_setcurrentpoint_from_path(pcis->pgs, pcis->path);
                    if (code < 0)
                        return code;
                } else {
                    t1_hinter__setcurrentpoint(h, pcis->save_adxy.x + pcis->origin_offset.x,
                                                  pcis->save_adxy.y + pcis->origin_offset.y);
                    code = t1_hinter__end_subglyph(h);
                    if (code < 0)
                        return code;
                }
                code = gs_type1_endchar(pcis);
                if (code == 1) {
                    /*
                     * Reset the total hint count so that hintmask will
                     * parse its following data correctly.
                     * (gs_type1_endchar already reset the actual hint
                     * tables.)
                     */
                    pcis->num_hints = 0;
                    /* do accent of seac */
                    ipsp = &pcis->ipstack[pcis->ips_count - 1];
                    cip = ipsp->cs_data.bits.data;
                    goto call;
                }
                return code;
            case cx_rmoveto:
                /* See vmoveto above re closing the subpath. */
                check_first_operator(!((csp - cstack) & 1));
                if (CS_CHECK_CSTACK_BOUNDS(&csp[-1], cstack)) {
                    if (csp > cstack + 1) {
                      /* Some Type 2 charstrings omit the vstemhm operator before rmoveto,
                         even though this is only allowed before hintmask and cntrmask.
                         Thanks to Felix Pahl.
                       */
                      type2_vstem(pcis, csp - 2, cstack);
                      cstack [0] = csp [-1];
                      cstack [1] = csp [ 0];
                      csp = cstack + 1;
                    }
                    code = t1_hinter__rmoveto(h, csp[-1], *csp);
                    goto move;
                }
                else
                    return_error(gs_error_invalidfont);
            case cx_hmoveto:
                if (CS_CHECK_CSTACK_BOUNDS(csp, cstack)) {
                    /* See vmoveto above re closing the subpath. */
                    check_first_operator(csp > cstack);
                    code = t1_hinter__rmoveto(h, *csp, 0);
                    goto move;
                } else
                    return_error(gs_error_invalidfont);
            case cx_vhcurveto:
                vertical = true;
                goto hvc;
            case cx_hvcurveto:
                vertical = false;
              hvc:for (ap = cstack; ap + 3 <= csp; vertical = !vertical, ap += 4) {
                    gs_fixed_point pt[2] = {{0, 0}, {0, 0}};
                    if (vertical) {
                        pt[0].y = ap[0];
                        pt[1].x = ap[3];
                        if (ap + 4 == csp)
                            pt[1].y = ap[4];
                    } else {
                        pt[0].x = ap[0];
                        if (ap + 4 == csp)
                            pt[1].x = ap[4];
                        pt[1].y = ap[3];
                    }
                    code = t1_hinter__rcurveto(h, pt[0].x, pt[0].y, ap[1], ap[2], pt[1].x, pt[1].y);
                    if (code < 0)
                        return code;
                }
                goto pp;

                        /***********************
                         * New Type 2 commands *
                         ***********************/

            case c2_blend:
                if (CS_CHECK_CSTACK_BOUNDS(csp, cstack))
                {
                    int n = fixed2int_var(*csp);
                    int num_values = csp - cstack;
                    gs_font_type1 *pfont = pcis->pfont;
                    int k = pfont->data.WeightVector.count;
                    int i, j;
                    cs_ptr base, deltas;

                    base = csp - 1 - num_values;
                    deltas = base + n - 1;
                    for (j = 0; j < n; j++, base++, deltas += k - 1)
                        for (i = 1; i < k; i++)
                            *base += (fixed)(deltas[i] *
                                pfont->data.WeightVector.values[i]);
                } else
                    return gs_note_error(gs_error_invalidfont);
                clear;
                continue;
            case c2_hstemhm:
              hstem: check_first_operator(!((csp - cstack) & 1));
                {
                    fixed x = 0;

                    for (ap = cstack; ap + 1 <= csp; x += ap[1], ap += 2) {
                            code = t1_hinter__hstem(h, x += ap[0], ap[1]);
                            if (code < 0)
                                return code;
                    }
                }
                pcis->num_hints += (csp + 1 - cstack) >> 1;
                clear;
                continue;
            case c2_hintmask:
                /*
                 * A hintmask at the beginning of the CharString is
                 * equivalent to vstemhm + hintmask.  For simplicity, we use
                 * this interpretation everywhere.
                 */
            case c2_cntrmask:
                if (CS_CHECK_CSTACK_BOUNDS(csp, cstack)) {
                    check_first_operator(!((csp - cstack) & 1));
                    type2_vstem(pcis, csp, cstack);
                }
                /*
                 * We should clear the stack here only if this is the
                 * initial mask operator that includes the implicit
                 * vstemhm, but currently this is too much trouble to
                 * detect.
                 */
                clear;
                {
                    byte mask[max_total_stem_hints / 8];
                    int i;

                    for (i = 0; i < pcis->num_hints; ++cip, i += 8) {
                        charstring_next(*cip, state, mask[i >> 3], encrypted);
                        if_debug1m('1', pfont->memory, " 0x%02x", mask[i >> 3]);
                    }
                    if_debug0m('1', pfont->memory, "\n");
                    ipsp->ip = cip;
                    ipsp->ip_end = endp;
                    ipsp->dstate = state;
                    if (c == c2_cntrmask) {
                        /****** NYI ******/
                    } else {	/* hintmask or equivalent */
                        if_debug0m('1', pfont->memory, "[1]hstem hints:\n");
                        if_debug0m('1', pfont->memory, "[1]vstem hints:\n");
                        code = t1_hinter__hint_mask(h, mask);
                        if (code < 0)
                            return code;
                    }
                }
                break;
            case c2_vstemhm:
              vstem:
                if (CS_CHECK_CSTACK_BOUNDS(csp, cstack)) {
                    check_first_operator(!((csp - cstack) & 1));
                    type2_vstem(pcis, csp, cstack);
                }else
                    return gs_note_error(gs_error_invalidfont);
                clear;
                continue;
            case c2_rcurveline:
                for (ap = cstack; ap + 5 <= csp; ap += 6) {
                    code = t1_hinter__rcurveto(h, ap[0], ap[1], ap[2], ap[3],
                                            ap[4], ap[5]);
                    if (code < 0)
                        return code;
                }
                if (ap + 1 <= csp)
                    code = t1_hinter__rlineto(h, ap[0], ap[1]);
                else
                    return_error(gs_error_invalidfont);
                goto cc;
            case c2_rlinecurve:
                for (ap = cstack; ap + 7 <= csp; ap += 2) {
                    code = t1_hinter__rlineto(h, ap[0], ap[1]);
                    if (code < 0)
                        return code;
                }
                if (ap + 5 <= csp)
                    code = t1_hinter__rcurveto(h, ap[0], ap[1], ap[2], ap[3],
                                        ap[4], ap[5]);
                else
                    return_error(gs_error_invalidfont);
  move:
  cc:
                if (code < 0)
                    return code;
                goto pp;
            case c2_vvcurveto:
                ap = cstack;
                {
                    int n = csp + 1 - cstack;
                    fixed dxa = (n & 1 ? *ap++ : 0);

                    for (; ap + 3 <= csp; ap += 4) {
                        code = t1_hinter__rcurveto(h, dxa, ap[0], ap[1], ap[2],
                                                fixed_0, ap[3]);
                        if (code < 0)
                            return code;
                        dxa = 0;
                    }
                }
                goto pp;
            case c2_hhcurveto:
                ap = cstack;
                {
                    int n = csp + 1 - cstack;
                    fixed dya = (n & 1 ? *ap++ : 0);

                    for (; ap + 3 <= csp; ap += 4) {
                        code = t1_hinter__rcurveto(h, ap[0], dya, ap[1], ap[2],
                                                ap[3], fixed_0);
                        if (code < 0)
                            return code;
                        dya = 0;
                    }
                }
              pp:
                clear;
                continue;
            case c2_shortint:
                {
                    int c1, c2;

                    charstring_next(*cip, state, c1, encrypted);
                    ++cip;
                    charstring_next(*cip, state, c2, encrypted);
                    ++cip;
                    CS_CHECK_PUSH(csp, cstack);
                    *++csp = int2fixed((((c1 ^ 0x80) - 0x80) << 8) + c2);
                }
  pushed:       if_debug3m('1', pfont->memory, "[1]%d: (%d) %f\n",
                           (int)(csp - cstack), c, fixed2float(*csp));
                continue;
            case c2_callgsubr:
                if (CS_CHECK_CSTACK_BOUNDS(csp, cstack)) {
                    CS_CHECK_IPSTACK(&(ipsp[1]), pcis->ipstack);
                    c = fixed2int_var(*csp) + pdata->gsubrNumberBias;
                    code = pdata->procs.subr_data
                        (pfont, c, true, &ipsp[1].cs_data);
  subr:
                    if (code < 0) {
                        /* Calling a Subr with an out-of-range index is clearly a error:
                         * the Adobe documentation says the results of doing this are
                         * undefined. However, we have seen a PDF file produced by Adobe
                         * PDF Library 4.16 that included a Type 2 font that called an
                         * out-of-range Subr, and Acrobat Reader did not signal an error.
                         * Therefore, we ignore such calls.
                         */
                        cip++;
                        continue;
                    }
                    --csp;
                    ipsp->ip = cip, ipsp->dstate = state;
                    ipsp->ip_end = endp;
                    ++ipsp;
                    cip = ipsp->cs_data.bits.data;
                    endp = cip + ipsp->cs_data.bits.size;
  call:
                    state = crypt_charstring_seed;
                    if (encrypted) {
                        int skip = pdata->lenIV;

                        /* Skip initial random bytes */
                        for (; skip > 0; ++cip, --skip)
                            decrypt_skip_next(*cip, state);
                    }
                    continue;
                } else {
                    cip++;
                    continue;
                }
            case cx_escape:
                charstring_next(*cip, state, c, encrypted);
                ++cip;
#ifdef DEBUG
                if (gs_debug['1'] && c < char2_extended_command_count) {
                    static const char *const ce2names[] =
                    {char2_extended_command_names};

                    if (ce2names[c] == 0)
                        dmlprintf2(pfont->memory, "[1]"PRI_INTPTR": %02x??\n",
                                   (intptr_t)(cip - 1), c);
                    else
                        dmlprintf3(pfont->memory, "[1]"PRI_INTPTR": %02x %s\n",
                                   (intptr_t)(cip - 1), c, ce2names[c]);
                }
#endif
                switch ((char2_extended_command) c) {
                    case ce2_and:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-1], cstack))
                            return_error(gs_error_invalidfont);
                        csp[-1] = ((csp[-1] != 0) & (*csp != 0) ? fixed_1 : 0);
                        --csp;
                        break;
                    case ce2_or:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-1], cstack))
                            return_error(gs_error_invalidfont);
                        csp[-1] = (csp[-1] | *csp ? fixed_1 : 0);
                        --csp;
                        break;
                    case ce2_not:
                        if (!CS_CHECK_CSTACK_BOUNDS(csp, cstack))
                            return_error(gs_error_invalidfont);
                        *csp = (*csp ? 0 : fixed_1);
                        break;
                    case ce2_store:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-3], cstack))
                            return_error(gs_error_invalidfont);
                        {
                            int i, n = fixed2int_var(*csp);
                            int ind = fixed2int_var(csp[-3]);
                            int offs = fixed2int_var(csp[-2]);
                            float *to;
                            const fixed *from = pcis->transient_array + fixed2int_var(csp[-1]);

                            if (!CS_CHECK_TRANSIENT_BOUNDS(from, pcis->transient_array))
                                return_error(gs_error_invalidfont);

                            if (ind < countof(Registry)) {
                                to = Registry[ind].values + offs;
                                for (i = 0; i < n; ++i)
                                    to[i] = fixed2float(from[i]);
                            }
                        }
                        csp -= 4;
                        break;
                    case ce2_abs:
                        if (!CS_CHECK_CSTACK_BOUNDS(csp, cstack))
                            return_error(gs_error_invalidfont);
                        if (*csp < 0)
                            *csp = -*csp;
                        break;
                    case ce2_add:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-1], cstack))
                            return_error(gs_error_invalidfont);
                        csp[-1] += *csp;
                        --csp;
                        break;
                    case ce2_sub:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-1], cstack))
                            return_error(gs_error_invalidfont);
                        csp[-1] -= *csp;
                        --csp;
                        break;
                    case ce2_div:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-1], cstack))
                            return_error(gs_error_invalidfont);
                        if (*csp == 0)
                            return_error(gs_error_invalidfont);
                        csp[-1] = float2fixed((double)csp[-1] / *csp);
                        --csp;
                        break;
                    case ce2_load:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-2], cstack))
                            return_error(gs_error_invalidfont);
                        /* The specification says there is no j (starting index */
                        /* in registry array) argument.... */
                        {
                            int i, n = fixed2int_var(*csp);
                            int ind = fixed2int_var(csp[-2]);
                            const float *from;
                            fixed *to = pcis->transient_array + fixed2int_var(csp[-1]);

                            if (!CS_CHECK_TRANSIENT_BOUNDS(to, pcis->transient_array))
                                return_error(gs_error_invalidfont);
                            if (ind < countof(Registry)) {
                                from = Registry[ind].values;
                                for (i = 0; i < n; ++i)
                                    to[i] = float2fixed(from[i]);
                            }
                        }
                        csp -= 3;
                        break;
                    case ce2_neg:
                        if (!CS_CHECK_CSTACK_BOUNDS(csp, cstack))
                            return_error(gs_error_invalidfont);
                        *csp = -*csp;
                        break;
                    case ce2_eq:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-1], cstack))
                            return_error(gs_error_invalidfont);
                        csp[-1] = (csp[-1] == *csp ? fixed_1 : 0);
                        --csp;
                        break;
                    case ce2_drop:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-1], cstack))
                            return_error(gs_error_invalidfont);
                        --csp;
                        break;
                    case ce2_put:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-1], cstack))
                            return_error(gs_error_invalidfont);
                        {
                            fixed *to = pcis->transient_array + fixed2int_var(*csp);

                            if (!CS_CHECK_TRANSIENT_BOUNDS(to, pcis->transient_array))
                                return_error(gs_error_invalidfont);

                            *to = csp[-1];
                            csp -= 2;
                        }
                        break;
                    case ce2_get:
                        if (!CS_CHECK_CSTACK_BOUNDS(csp, cstack))
                            return_error(gs_error_invalidfont);
                        {
                            fixed *from = pcis->transient_array + fixed2int_var(*csp);
                            if (!CS_CHECK_TRANSIENT_BOUNDS(from, pcis->transient_array))
                                return_error(gs_error_invalidfont);

                            *csp = *from;
                        }
                        break;
                    case ce2_ifelse:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-3], cstack))
                            return_error(gs_error_invalidfont);
                        if (csp[-1] > *csp)
                            csp[-3] = csp[-2];
                        csp -= 3;
                        break;
                    case ce2_random:
                        CS_CHECK_PUSH(csp, cstack);
                        ++csp;
                        /****** NYI ******/
                        break;
                    case ce2_mul:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-1], cstack))
                            return_error(gs_error_invalidfont);
                        {
                            double prod = fixed2float(csp[-1]) * *csp;

                            csp[-1] =
                                (prod > max_fixed ? max_fixed :
                                 prod < min_fixed ? min_fixed : (fixed)prod);
                        }
                        --csp;
                        break;
                    case ce2_sqrt:
                        if (!CS_CHECK_CSTACK_BOUNDS(csp, cstack))
                            return_error(gs_error_invalidfont);
                        if (*csp >= 0)
                            *csp = float2fixed(sqrt(fixed2float(*csp)));
                        break;
                    case ce2_dup:
                        if (!CS_CHECK_CSTACK_BOUNDS(csp, cstack))
                            return_error(gs_error_invalidfont);
                        CS_CHECK_PUSH(csp, cstack);
                        csp[1] = *csp;
                        ++csp;
                        break;
                    case ce2_exch:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-1], cstack))
                            return_error(gs_error_invalidfont);
                        {
                            fixed top = *csp;

                            *csp = csp[-1], csp[-1] = top;
                        }
                        break;
                    case ce2_index:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-1], cstack) ||
                            !CS_CHECK_CSTACK_BOUNDS(&csp[0], cstack) ||
                            !CS_CHECK_CSTACK_BOUNDS(&csp[-1 - fixed2int_var(csp[-1])], cstack))
                            return_error(gs_error_invalidfont);
                        *csp = (*csp < 0 ? csp[-1] : csp[-1 - fixed2int_var(csp[-1])]);
                        break;
                    case ce2_roll:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-1], cstack))
                            return_error(gs_error_invalidfont);
                        {
                            int distance = fixed2int_var(*csp);
                            int count = fixed2int_var(csp[-1]);
                            cs_ptr bot;

                            csp -= 2;
                            if (count < 0 || count > csp + 1 - cstack)
                                return_error(gs_error_invalidfont);
                            if (count == 0)
                                break;
                            if (distance < 0)
                                distance = count - (-distance % count);
                            bot = csp + 1 - count;
                            while (--distance >= 0) {
                                fixed top = *csp;

                                memmove(bot + 1, bot,
                                        (count - 1) * sizeof(fixed));
                                *bot = top;
                            }
                        }
                        break;
                    case ce2_hflex:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-6], cstack))
                            return_error(gs_error_invalidfont);
                        CS_CHECK_PUSHN(csp, cstack, 6);
                        csp[6] = fixed_half;	/* fd/100 */
                        csp[4] = *csp, csp[5] = 0;	/* dx6, dy6 */
                        csp[2] = csp[-1], csp[3] = -csp[-4];	/* dx5, dy5 */
                        *csp = csp[-2], csp[1] = 0;	/* dx4, dy4 */
                        csp[-2] = csp[-3], csp[-1] = 0;		/* dx3, dy3 */
                        csp[-3] = csp[-4], csp[-4] = csp[-5];	/* dx2, dy2 */
                        csp[-5] = 0;	/* dy1 */
                        csp += 6;
                        goto flex;
                    case ce2_flex:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-12], cstack))
                            return_error(gs_error_invalidfont);
                        *csp /= 100;	/* fd/100 */
                        goto flex;
                    case ce2_hflex1:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-8], cstack))
                            return_error(gs_error_invalidfont);
                        CS_CHECK_PUSHN(csp, cstack, 4);
                        csp[4] = fixed_half;	/* fd/100 */
                        csp[2] = *csp;          /* dx6 */
                        csp[3] = -(csp[-7] + csp[-5] + csp[-1]);	/* dy6 */
                        *csp = csp[-2], csp[1] = csp[-1];	/* dx5, dy5 */
                        csp[-2] = csp[-3], csp[-1] = 0;		/* dx4, dy4 */
                        csp[-3] = 0;	/* dy3 */
                        csp += 4;
                        goto flex;
                    case ce2_flex1:
                        if (!CS_CHECK_CSTACK_BOUNDS(&csp[-10], cstack))
                            return_error(gs_error_invalidfont);
                        CS_CHECK_PUSHN(csp, cstack, 2);
                        {
                            fixed dx = csp[-10] + csp[-8] + csp[-6] + csp[-4] + csp[-2];
                            fixed dy = csp[-9] + csp[-7] + csp[-5] + csp[-3] + csp[-1];

                            if (any_abs(dx) > any_abs(dy))
                                csp[1] = -dy;	/* d6 is dx6 */
                            else
                                csp[1] = *csp, *csp = -dx;	/* d6 is dy6 */
                        }
                        csp[2] = fixed_half;	/* fd/100 */
                        csp += 2;
flex:			{
                            fixed x_join = csp[-12] + csp[-10] + csp[-8];
                            fixed y_join = csp[-11] + csp[-9] + csp[-7];
                            fixed x_end = x_join + csp[-6] + csp[-4] + csp[-2];
                            fixed y_end = y_join + csp[-5] + csp[-3] + csp[-1];
                            gs_point join, end;
                            double flex_depth;

                            if ((code =
                                 gs_distance_transform(fixed2float(x_join),
                                                       fixed2float(y_join),
                                                       &ctm_only(pcis->pgs),
                                                       &join)) < 0 ||
                                (code =
                                 gs_distance_transform(fixed2float(x_end),
                                                       fixed2float(y_end),
                                                       &ctm_only(pcis->pgs),
                                                       &end)) < 0
                                )
                                return code;
                            /*
                             * Use the X or Y distance depending on whether
                             * the curve is more horizontal or more
                             * vertical.
                             */
                            if (any_abs(end.y) > any_abs(end.x))
                                flex_depth = join.x;
                            else
                                flex_depth = join.y;
                            if (fabs(flex_depth) < fixed2float(*csp)) {
                                /* Do flex as line. */
                                code = t1_hinter__rlineto(h, x_end, y_end);
                            } else {
                                /*
                                 * Do flex as curve.  We can't jump to rrc,
                                 * because the flex operators don't clear
                                 * the stack (!).
                                 */
                                code = t1_hinter__rcurveto(h,
                                        csp[-12], csp[-11], csp[-10],
                                        csp[-9], csp[-8], csp[-7]);
                                if (code < 0)
                                    return code;
                                code = t1_hinter__rcurveto(h,
                                        csp[-6], csp[-5], csp[-4],
                                        csp[-3], csp[-2], csp[-1]);
                            }
                            if (code < 0)
                                return code;
                        }
                        clear;
                        continue;
                }
                break;

                /* Fill up the dispatch up to 32. */

            case_c2_undefs:
            default:		/* pacify compiler */
                return_error(gs_error_invalidfont);
        }
    }
}
