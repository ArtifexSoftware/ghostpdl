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


/* Default, stream-based readline implementation */
#include "std.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gp.h"

int
gp_readline_init(void **preadline_data, gs_memory_t * mem)
{
    return 0;
}

int
gp_readline(stream *s_in, stream *s_out, void *readline_data,
            gs_const_string *prompt, gs_string * buf,
            gs_memory_t * bufmem, uint * pcount, bool *pin_eol,
            bool (*is_stdin)(const stream *))
{
    return sreadline(s_in, s_out, readline_data, prompt, buf, bufmem, pcount,
                     pin_eol, is_stdin);
}

void
gp_readline_finit(void *readline_data)
{
}
