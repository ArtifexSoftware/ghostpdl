/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Client interface to HSB color routines */

#ifndef gshsb_INCLUDED
#  define gshsb_INCLUDED

int gs_sethsbcolor(P4(gs_state *, floatp, floatp, floatp)),
    gs_currenthsbcolor(P2(const gs_state *, float[3]));

#endif /* gshsb_INCLUDED */
