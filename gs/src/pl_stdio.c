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

/* std in out err functionality for ghostscript */

/* Capture stdin/out/err before gs.h redefines them. */
#include <stdio.h>

static void
pl_get_real_stdio(FILE **in, FILE **out, FILE **err)
{
    *in = stdin;
    *out = stdout;
    *err = stderr;
}


#include "pl_stdio.h"
#include "gsmemory.h"

int pl_stdio_init( gs_memory_t *mem )
{
    pl_stdio_t *pio = 0;

    if (mem->pl_stdio) /* one time initialization */
	return 0;  

    pio = mem->pl_stdio = (pl_stdio_t*)gs_alloc_bytes_immovable(mem, 
								sizeof(pl_stdio_t), 
								"pl_stdio_init");
    if( pio == 0 ) 
	return -1;
    
    pl_get_real_stdio(&pio->fstdin, &pio->fstdout, &pio->fstderr );

    pio->stdout_is_redirected = false;
    pio->stdout_to_stderr = false;
    pio->stdin_is_interactive = false;
    pio->stdin_fn = 0;
    pio->stdout_fn = 0;
    pio->stderr_fn = 0;
    pio->poll_fn = 0;

    pio->gs_next_id = 1;
    return 0;
}

/* Provide a single point for all "C" stdout and stderr.
 */

int outwrite(const gs_memory_t *mem, const char *str, int len)
{
    int code;
    FILE *fout;
    pl_stdio_t *pio = mem->pl_stdio;

    if (len == 0)
	return 0;
    if (pio->stdout_is_redirected) {
	if (pio->stdout_to_stderr)
	    return errwrite(mem, str, len);
	fout = pio->fstdout2;
    }
    else if (pio->stdout_fn) {
	return (*pio->stdout_fn)(pio->caller_handle, str, len);
    }
    else {
	fout = pio->fstdout;
    }
    code = fwrite(str, 1, len, fout);
    fflush(fout);
    return code;
}

int errwrite(const gs_memory_t *mem, const char *str, int len)
{    
    int code;
    if (len == 0)
	return 0;
    if (mem->pl_stdio->stderr_fn)
	return (*mem->pl_stdio->stderr_fn)(mem->pl_stdio->caller_handle, str, len);

    code = fwrite(str, 1, len, mem->pl_stdio->fstderr);
    fflush(mem->pl_stdio->fstderr);
    return code;
}

void outflush(const gs_memory_t *mem)
{
    if (mem->pl_stdio->stdout_is_redirected) {
	if (mem->pl_stdio->stdout_to_stderr) {
	    if (!mem->pl_stdio->stderr_fn)
		fflush(mem->pl_stdio->fstderr);
	}
	else
	    fflush(mem->pl_stdio->fstdout2);
    }
    else if (!mem->pl_stdio->stdout_fn)
        fflush(mem->pl_stdio->fstdout);
}

void errflush(const gs_memory_t *mem)
{
    if (!mem->pl_stdio->stderr_fn)
        fflush(mem->pl_stdio->fstderr);
    /* else nothing to flush */
}

