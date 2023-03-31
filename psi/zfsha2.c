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


/* SHA256Encode filter creation */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gsstruct.h"
#include "stream.h"
#include "strimpl.h"
#include "ialloc.h"
#include "ifilter.h"
#include "ssha2.h"

/* <source> SHA256Encode/filter <file> */
/* <source> <dict> SHA256Encode/filter <file> */
static int
zSHA256E(i_ctx_t *i_ctx_p)
{
    return filter_write_simple(i_ctx_p, &s_SHA256E_template);
}

/* ------ Initialization procedure ------ */

const op_def zfsha2_op_defs[] =
{
    op_def_begin_filter(),
    {"1SHA256Encode", zSHA256E},
    op_def_end(0)
};
