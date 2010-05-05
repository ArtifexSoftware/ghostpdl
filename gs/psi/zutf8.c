/* Copyright (C) 2009 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* The .locale_to_utf8 operator, for converting text from the locale
 * charset to UTF-8.  This code is not used on Windows; there's a
 * separate Windows implementation in zwinutf8.c. */

#include "ghost.h"
#include "oper.h"
#include "iutil.h"
#include "ialloc.h"
#include "malloc_.h"
#include "errno_.h"
#include "string_.h"
#include "store.h"
#include <stringprep.h>

/* Convert a string from the current locale's character set to UTF-8.
 * <string> .locale_to_utf8 <string> */
static int
zlocale_to_utf8(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    char *input;
    char *output;
    int code;

    check_read_type(*op, t_string);
    input = ref_to_string(op, imemory, "locale_to_utf8 input");
    if (input == 0)
	return_error(e_VMerror);

    output = stringprep_locale_to_utf8(input);
    ifree_string((byte *)input, r_size(op) + 1, "locale_to_utf8 input");
    if (output == 0) {
	/* This function is intended to be used on strings whose
	 * character set is unknown, so it's not an error if the
	 * input contains invalid characters.  Just return the input
	 * string unchanged.
	 *
	 * Sadly, EINVAL from stringprep_locale_to_utf8 can mean
	 * either an invalid character set conversion (which we care
	 * about), or an incomplete input string (which we don't).
	 * For now, we ignore EINVAL; the right solution is probably
	 * to not use stringprep_locale_to_utf8, and just call iconv
	 * by hand. */
	if (errno == EILSEQ || errno == EINVAL)
	    return 0;

	/* Other errors (like ENFILE) are real errors, which we
	 * want to return to the user. */
	return_error(e_ioerror);
    }

    code = string_to_ref(output, op, iimemory, "locale_to_utf8 output");
    free(output);
    if (code < 0)
	return code;

    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zutf8_op_defs[] =
{
    {"1.locale_to_utf8", zlocale_to_utf8},
    op_def_end(0)
};
