/* Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Dummy scan_binary_token for Level 1 systems */
#include "ghost.h"
#include "errors.h"
#include "stream.h"
#include "iscan.h"
#include "iscanbin.h"

int
scan_binary_token(i_ctx_t *i_ctx_p, stream *s, ref *pref,
		  scanner_state *pstate)
{
    return_error(e_unregistered);
}
