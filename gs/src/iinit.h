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
/* (Internal) interface to iinit.c */

#ifndef iinit_INCLUDED
#  define iinit_INCLUDED

/*
 * Declare initialization procedures exported by iinit.c for imain.c.
 * These must be executed in the order they are declared below.
 */
int obj_init(P2(i_ctx_t **, gs_dual_memory_t *));
int zop_init(P1(i_ctx_t *));
int op_init(P1(i_ctx_t *));

/*
 * Test whether there are any Level 2 operators in the executable.
 * (This is different from the language level in which the interpreter is
 * actually running: it is only tested during initialization.)
 */
bool gs_have_level2(P0());

#endif /* iinit_INCLUDED */
