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
/* Library initialization and finalization interface */
/* Requires stdio.h, gsmemory.h */

#ifndef gslib_INCLUDED
#  define gslib_INCLUDED

#ifndef gs_main_instance_DEFINED
# define gs_main_instance_DEFINED
typedef struct gs_main_instance_s gs_main_instance;
#endif

/*
 * Initialize the library.  gs_lib_init does all of the initialization,
 * using the C heap for initial allocation; if a client wants the library to
 * use a different default allocator during initialization, it should call
 * gs_lib_init0 and then gs_lib_init1.
 */
int gs_lib_init(P1(FILE * debug_out));
int gs_lib_init0(P2(gs_main_instance *minst, FILE * debug_out));
int gs_lib_init1(P1(gs_memory_t *));

/* Clean up after execution. */
void gs_lib_finit(P3(gs_main_instance *minst, int exit_status, int code));

#endif /* gslib_INCLUDED */
