/* Copyright (C) 1989, 2000, 2001 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Special file operators */

#include "memory_.h"
#include "ghost.h"
#include "gp.h"
#include "errors.h"
#include "oper.h"
#include "ialloc.h"
#include "opdef.h"
#include "opcheck.h"
#include "store.h"

/* <string> <string> .file_name_combine <string> */
private int
zfile_name_combine(i_ctx_t *i_ctx_p)
{
    uint plen, flen, blen, blen0;
    const byte *prefix, *fname;
    byte *buffer;
    os_ptr op = osp;

    check_type(op[ 0], t_string);
    check_type(op[-1], t_string);
    plen = r_size(op - 1);
    flen = r_size(op);
    blen = blen0 = plen + flen + 2; /* Inserts separator and ending zero byte. */
    buffer = ialloc_string(blen, "zfile_name_combine");
    if (buffer == 0)
	return_error(e_VMerror);
    prefix = op[-1].value.const_bytes;
    fname =  op[ 0].value.const_bytes;
    if (gp_file_name_combine((const char *)prefix, plen, 
			     (const char *)fname, flen, 
		             (char *)buffer, &blen) != gp_combine_success)
	return_error(e_undefinedfilename);
    buffer = iresize_string(buffer, blen0, blen, "zfile_name_combine");
    if (buffer == 0)
	return_error(e_VMerror);
    make_string(op - 1, a_read | icurrent_space, blen, buffer);
    pop(1);
    return 0;
}

#if NEW_COMBINE_PATH
/* This is compiled conditionally to let PS library to know
 * whether it works with the new gp_combine_file_name.
 */

private int
push_string(i_ctx_t *i_ctx_p, const char *v)
{   os_ptr op = osp;
    int len = strlen(v);
    byte *buffer = ialloc_string(len, "zfile_name_combine");

    if (buffer == 0)
	return_error(e_VMerror);
    memcpy(buffer, v, len);
    push(1);
    make_string(op + 1, a_read | icurrent_space, 1, buffer);
    return 0;
}

/* .file_name_separator <string> */
private int
zfile_name_separator(i_ctx_t *i_ctx_p)
{   return push_string(i_ctx_p, gp_file_name_separator());
}

/* .file_name_directory_separator <string> */
private int
zfile_name_directory_separator(i_ctx_t *i_ctx_p)
{   return push_string(i_ctx_p, gp_file_name_directory_separator());
}

/* - .file_name_parent <string> */
private int
zfile_name_parent(i_ctx_t *i_ctx_p)
{   return push_string(i_ctx_p, gp_file_name_parent());
}

#endif

const op_def zfile1_op_defs[] =
{
    {"2.file_name_combine", zfile_name_combine},
#if NEW_COMBINE_PATH
    {"2.file_name_separator", zfile_name_separator},
    {"2.file_name_directory_separator", zfile_name_directory_separator},
    {"2.file_name_parent", zfile_name_parent},
#endif
    op_def_end(0)
};

