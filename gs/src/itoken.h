/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Interface to exported procedures in ztoken.c */

#ifndef itoken_INCLUDED
#  define itoken_INCLUDED

/*
 * Continue after handling a procedure stream refill or other callout
 * while reading tokens in the interpreter.
 */
int ztokenexec_continue(P1(i_ctx_t *i_ctx_p));

/*
 * Handle a scan_Comment or scan_DSC_Comment return from scan_token.
 */
#ifndef scanner_state_DEFINED
#  define scanner_state_DEFINED
typedef struct scanner_state_s scanner_state;
#endif
int ztoken_handle_comment(P8(i_ctx_t *i_ctx_p, const ref *fop,
			     scanner_state *sstate, const ref *ptoken,
			     int scan_code, bool save, bool push_file,
			     op_proc_t cont));

/*
 * Update the cached scanner_options in the context state after doing a
 * setuserparams.  (We might move this procedure somewhere else eventually.)
 */
int ztoken_scanner_options(P2(const ref *upref, int old_options));

#endif /* itoken_INCLUDED */
