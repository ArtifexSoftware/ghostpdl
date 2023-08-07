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



/* this is the ps interpreter interface to the AES cipher filter
   used in PDF encryption. We currently provide only decode support. */

#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gsstruct.h"
#include "ialloc.h"
#include "idict.h"
#include "idparam.h"
#include "stream.h"
#include "strimpl.h"
#include "ifilter.h"
#include "saes.h"

/* <source> <dict> aes/filter <file> */

static int
z_aes_d(i_ctx_t * i_ctx_p)
{
    os_ptr op = osp;		/* i_ctx_p->op_stack.stack.p defined in osstack.h */
    ref *sop = NULL;
    stream_aes_state state;
    int use_padding;

    check_op(2);
    /* extract the key from the parameter dictionary */
    check_type(*op, t_dictionary);
    check_dict_read(*op);
    if (dict_find_string(op, "Key", &sop) <= 0)
        return_error(gs_error_rangecheck);
    check_type(*sop, t_string);
    s_aes_set_key(&state, sop->value.const_bytes, r_size(sop));

    /* extract the padding flag, which defaults to true for compatibility */
    if (dict_bool_param(op, "Padding", 1, &use_padding) < 0)
        return_error(gs_error_rangecheck);

    s_aes_set_padding(&state, use_padding);

    /* we pass npop=0, since we've no arguments left to consume */
    /* FIXME: passing 0 instead of the usual rspace(sop) will allocate
       storage for filter state from the same memory pool as the stream
       it's coding. this caused no trouble when we were the arcfour cipher
       and maintained no pointers. */
    return filter_read(i_ctx_p, 0, &s_aes_template,
                       (stream_state *) & state, 0);
}

/* Match the above routine to its postscript filter name.
   This is how our static routines get called externally. */
const op_def zfaes_op_defs[] = {
    op_def_begin_filter(),
    {"2AESDecode", z_aes_d},
    op_def_end(0)
};
