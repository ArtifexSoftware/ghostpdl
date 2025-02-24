/* Copyright (C) 2025 Artifex Software, Inc.
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


/* Brotli filter creation */
#include "ghost.h"
#include "oper.h"
#include "idict.h"
#include "strimpl.h"
#include "spdiffx.h"
#include "sbrotlix.h"
#include "idparam.h"
#include "ifilter.h"
#include "ifrpred.h"
#include "ifwpred.h"

/* <source> FlateEncode/filter <file> */
/* <source> <dict> FlateEncode/filter <file> */
static int
zBrotliE(i_ctx_t *i_ctx_p)
{
    stream_brotlie_state zls;
    os_ptr op = osp;
    int code = 0;

    (*s_brotliE_template.set_defaults)((stream_state *)&zls);
    if (r_has_type(op, t_dictionary))
        code = dict_int_param(op, "Effort", 1, 11, 9, &zls.level);
    if (code < 0)
        return code;

    return filter_write_predictor(i_ctx_p, 0, &s_brotliE_template,
                                  (stream_state *)&zls);
}

/* <target> FlateDecode/filter <file> */
/* <target> <dict> FlateDecode/filter <file> */
static int
zBrotliD(i_ctx_t *i_ctx_p)
{
    stream_brotlid_state zls;

    (*s_brotliD_template.set_defaults)((stream_state *)&zls);
    return filter_read_predictor(i_ctx_p, 0, &s_brotliD_template,
                                 (stream_state *)&zls);
}

/* ------ Initialization procedure ------ */

const op_def zfbrotli_op_defs[] =
{
    op_def_begin_filter(),
    {"1BrotliEncode", zBrotliE},
    {"1BrotliDecode", zBrotliD},
    op_def_end(0)
};
