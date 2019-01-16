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


/* The Windows implementation of the .locale_to_utf8 operator.  See
 * also zutf8.c, which is the non-Windows implementation. */

#include "windows_.h"
#include "ghost.h"
#include "oper.h"
#include "iutil.h"
#include "store.h"

/* Convert a string from the current locale's character set to UTF-8.
 * Unfortunately, "current locale" can mean a few different things on
 * Windows -- we use the default ANSI code page, which does the right
 * thing for command-line arguments (like "-sPDFPassword=foo") and
 * for strings typed as input to gswin32.exe.  It doesn't work for
 * strings typed as input to gswin32c.exe, which are normally in the
 * default OEM code page instead.
 * <string> .locale_to_utf8 <string> */
static int
zlocale_to_utf8(i_ctx_t *i_ctx_p)
{
#define LOCALE_TO_UTF8_BUFFER_SIZE 1024
    os_ptr op = osp;
    char *input;
    WCHAR wide_buffer[LOCALE_TO_UTF8_BUFFER_SIZE];
    char utf8_buffer[LOCALE_TO_UTF8_BUFFER_SIZE];
    int success;
    int code;

    check_read_type(*op, t_string);
    input = ref_to_string(op, imemory, "locale_to_utf8 input");
    if (input == 0)
        return_error(gs_error_VMerror);

    success = MultiByteToWideChar(CP_ACP, 0, input, -1,
        wide_buffer, LOCALE_TO_UTF8_BUFFER_SIZE);
    ifree_string((byte *)input, r_size(op) + 1, "locale_to_utf8 input");
    if (success == 0)
        return_error(gs_error_ioerror);

    success = WideCharToMultiByte(CP_UTF8, 0, wide_buffer, -1,
        utf8_buffer, LOCALE_TO_UTF8_BUFFER_SIZE, NULL, NULL);
    if (success == 0)
        return_error(gs_error_ioerror);

    code = string_to_ref(utf8_buffer, op, iimemory, "locale_to_utf8 output");
    if (code < 0)
        return code;

    return 0;
#undef LOCALE_TO_UTF8_BUFFER_SIZE
}

/* ------ Initialization procedure ------ */

const op_def zwinutf8_op_defs[] =
{
    {"1.locale_to_utf8", zlocale_to_utf8},
    op_def_end(0)
};
