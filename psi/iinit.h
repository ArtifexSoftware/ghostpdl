/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* (Internal) interface to iinit.c */

/* The following will allow for -Z! to trace PS operators. */
/* This is slightly less noisy but less informative than -ZI */
/* #define DEBUG_TRACE_PS_OPERATORS */

#ifndef iinit_INCLUDED
#  define iinit_INCLUDED

#include "imemory.h"

/*
 * Declare initialization procedures exported by iinit.c for imain.c.
 * These must be executed in the order they are declared below.
 */
int obj_init(i_ctx_t **, gs_dual_memory_t *);
int zop_init(i_ctx_t *);
int op_init(i_ctx_t *);
#if defined(DEBUG_TRACE_PS_OPERATORS) || defined(DEBUG)
const char *op_get_name_string(op_proc_t opproc);
#endif

int
i_iodev_init(gs_dual_memory_t *);

void
i_iodev_finit(gs_dual_memory_t *);

/*
 * Test whether there are any Level 2 operators in the executable.
 * (This is different from the language level in which the interpreter is
 * actually running: it is only tested during initialization.)
 */
bool gs_have_level2(void);

#endif /* iinit_INCLUDED */
