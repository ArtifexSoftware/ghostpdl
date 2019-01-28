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


/* Interface to exported procedures in ztoken.c */

#ifndef itoken_INCLUDED
#  define itoken_INCLUDED

#include "iref.h"
#include "iscan.h"

/*
 * Continue after handling a procedure stream refill or other callout
 * while reading tokens in the interpreter.
 */
int ztokenexec_continue(i_ctx_t *i_ctx_p);

/*
 * Handle a scan_Comment or scan_DSC_Comment return from gs_scan_token.
 */
int ztoken_handle_comment(i_ctx_t *i_ctx_p,
                          scanner_state *sstate, const ref *ptoken,
                          int scan_code, bool save, bool push_file,
                          op_proc_t cont);

/*
 * Update the cached scanner_options in the context state after doing a
 * setuserparams.  (We might move this procedure somewhere else eventually.)
 */
int ztoken_scanner_options(const ref *upref, int old_options);
/*
 * Get the value for a scanner option.
 * return -1 if no such option, 1/0 for on/off and option's name in *pname as a C string
 */
int ztoken_get_scanner_option(const ref *psref, int options, const char **pname);

#endif /* itoken_INCLUDED */
