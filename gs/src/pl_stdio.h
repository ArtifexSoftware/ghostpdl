/* Portions Copyright (C) 2003 artofcode LLC.
   Portions Copyright (C) 2003 Artifex Software Inc.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id: */
#ifndef PL_STDIO_H 
#define PL_STDIO_H 

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

typedef struct pl_stdio_s
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
} pl_stdio_t;

/** initializes and stores itself in the given gs_memory_t pointer.
 * it is the responsibility of the gs_memory_t objects to copy the point to subsequent objects.
 */
int pl_stdio_init( gs_memory_t *mem ); 


#endif /* PL_STDIO_H */
