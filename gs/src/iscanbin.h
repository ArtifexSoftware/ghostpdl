/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
/* Interface to binary token scanner */

#ifndef iscanbin_INCLUDED
#  define iscanbin_INCLUDED

/*
 * Scan a binary token.  The main scanner calls this iff recognize_btokens()
 * is true.  Return e_unregistered if Level 2 features are not included.
 * Return 0 or scan_BOS on success, <0 on failure.
 *
 * This header file exists only because there are two implementations of
 * this procedure: a dummy one for Level 1 systems, and the real one.
 * The interface is entirely internal to the scanner.
 */
int scan_binary_token(i_ctx_t *i_ctx_p, stream *s, ref *pref,
		      scanner_state *pstate);

#endif /* iscanbin_INCLUDED */
