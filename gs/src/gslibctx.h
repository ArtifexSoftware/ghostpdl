/* Portions Copyright (C) 2003 artofcode LLC.
   Portions Copyright (C) 2003 Artifex Software Inc.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id$ */
#ifndef GSLIBCTX_H 
#define GSLIBCTX_H 

#include "std.h"
#include "stdio.h"

#ifndef GSDLLEXPORT
# define GSDLLEXPORT
#endif
#ifndef GSDLLAPI
# define GSDLLAPI
#endif
#ifndef GSDLLCALL
# define GSDLLCALL
#endif

typedef struct name_table_s *name_table_ptr;

typedef struct gs_lib_ctx_s
{  
    FILE *fstdin;
    FILE *fstdout;
    FILE *fstderr;
    FILE *fstdout2;		/* for redirecting %stdout and diagnostics */
    bool stdout_is_redirected;	/* to stderr or fstdout2 */
    bool stdout_to_stderr;
    bool stdin_is_interactive;   
    void *caller_handle;	/* identifies caller of GS DLL/shared object */
    int (GSDLLCALL *stdin_fn)(void *caller_handle, char *buf, int len);
    int (GSDLLCALL *stdout_fn)(void *caller_handle, const char *str, int len);
    int (GSDLLCALL *stderr_fn)(void *caller_handle, const char *str, int len);
    int (GSDLLCALL *poll_fn)(void *caller_handle);  
    ulong gs_next_id; /* gs_id initialized here, private variable of gs_next_ids() */
    void *top_of_system;  /* use accessor functions to walk down the system 
			   * to the desired structure gs_lib_ctx_get_*()
			   */
    name_table_ptr gs_name_table;  /* hack this is the ps interpreters name table 
				    * doesn't belong here 
				    */
} gs_lib_ctx_t;

/** initializes and stores itself in the given gs_memory_t pointer.
 * it is the responsibility of the gs_memory_t objects to copy 
 * the pointer to subsequent memory objects.
 */
int gs_lib_ctx_init( gs_memory_t *mem ); 

void *gs_lib_ctx_get_interp_instance( gs_memory_t *mem );
 
#endif /* GSLIBCTX_H */
