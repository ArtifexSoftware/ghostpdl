/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Library initialization and finalization interface */
/* Requires stdio.h, gsmemory.h */

#ifndef gslib_INCLUDED
#  define gslib_INCLUDED

/*
 * Initialize the library.  gs_lib_init does all of the initialization,
 * using the C heap for initial allocation; if a client wants the library to
 * use a different default allocator during initialization, it should call
 * gs_lib_init0 and then gs_lib_init1.
 */
int gs_lib_init(P1(FILE * debug_out));
gs_memory_t *gs_lib_init0(P1(FILE * debug_out));
int gs_lib_init1(P1(gs_memory_t *));

/* Clean up after execution. */
void gs_lib_finit(P2(int exit_status, int code));

#endif /* gslib_INCLUDED */
