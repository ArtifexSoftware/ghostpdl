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


/* Miscellaneous Type 1 font operators */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gscrypt1.h"
#include "stream.h"		/* for getting state of PFBD stream */
#include "strimpl.h"
#include "sfilter.h"
#include "idict.h"
#include "idparam.h"
#include "ifilter.h"

/* Get the seed parameter for eexecEncode/Decode. */
/* Return npop if OK. */
static int
eexec_param(os_ptr op, ushort * pcstate)
{
    int npop = 1;

    if (r_has_type(op, t_dictionary))
        ++npop, --op;
    check_type(*op, t_integer);
    *pcstate = op->value.intval;
    if (op->value.intval != *pcstate)
        return_error(gs_error_rangecheck);	/* state value was truncated */
    return npop;
}

/* <source> <seed> eexecDecode/filter <file> */
/* <source> <dict> eexecDecode/filter <file> */
static int
zexD(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    stream_exD_state state = {0};
    int code = 0;

    check_op(2);
    (*s_exD_template.set_defaults)((stream_state *)&state);
    if (r_has_type(op, t_dictionary)) {
        uint cstate = 0;
        bool is_eexec = false;

        check_dict_read(*op);
        if ((code = dict_uint_param(op, "seed", 0, 0xffff, 0x10000,
                                    &cstate)) < 0 ||
            (code = dict_int_param(op, "lenIV", 0, max_int, 4,
                                   &state.lenIV)) < 0 ||
            (code = dict_bool_param(op, "eexec", false,
                                   &is_eexec)) < 0 ||
            (code = dict_bool_param(op, "keep_spaces", false,
                                   &state.keep_spaces)) < 0
            )
            return code;
        state.cstate = cstate;
        state.binary = (is_eexec ? -1 : 1);
        code = 1;
    } else {
        state.binary = 1;
        code = eexec_param(op, &state.cstate);
    }
    if (code < 0)
        return code;

    if (gs_is_path_control_active(imemory) != 0 && state.cstate != 55665) {
        return_error(gs_error_rangecheck);
    }

    /*
     * If we're reading a .PFB file, let the filter know about it,
     * so it can read recklessly to the end of the binary section.
     */
    if (r_has_type(op - 1, t_file)) {
        stream *s = (op - 1)->value.pfile;

        if (s->state != 0 && s->state->templat == &s_PFBD_template) {
            stream_PFBD_state *pss = (stream_PFBD_state *)s->state;

            state.pfb_state = pss;
            /*
             * If we're reading the binary section of a PFB stream,
             * avoid the conversion from binary to hex and back again.
             */
            if (pss->record_type == 2) {
                /*
                 * The PFB decoder may have converted some data to hex
                 * already.  Convert it back if necessary.
                 */
                if (pss->binary_to_hex && sbufavailable(s) > 0) {
                    state.binary = 0;	/* start as hex */
                    state.hex_left = sbufavailable(s);
                } else {
                    state.binary = 1;
                }
                pss->binary_to_hex = 0;
            }
        }
    }
    return filter_read(i_ctx_p, code, &s_exD_template, (stream_state *)&state, 0);
}

/* ------ Initialization procedure ------ */

const op_def zmisc1_op_defs[] =
{
    op_def_begin_filter(),
    {"2eexecDecode", zexD},
    op_def_end(0)
};
