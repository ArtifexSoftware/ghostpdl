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

/* library context functionality for ghostscript 
 * api callers get a gs_main_instance 
 */

/* Capture stdin/out/err before gs.h redefines them. */
#include <stdio.h>

static void
gs_lib_ctx_get_real_stdio(FILE **in, FILE **out, FILE **err)
{
    *in = stdin;
    *out = stdout;
    *err = stderr;
}


#include "gslibctx.h"
#include "gsmemory.h"

int gs_lib_ctx_init( gs_memory_t *mem )
{
    gs_lib_ctx_t *pio = 0;

    if ( mem == 0 ) 
	return -1;  /* assert mem != 0 */

    if (mem->gs_lib_ctx) /* one time initialization */
	return 0;  

    pio = mem->gs_lib_ctx = 
	(gs_lib_ctx_t*)gs_alloc_bytes_immovable(mem, 
						sizeof(gs_lib_ctx_t), 
						"gs_lib_ctx_init");
    if( pio == 0 ) 
	return -1;
    
    gs_lib_ctx_get_real_stdio(&pio->fstdin, &pio->fstdout, &pio->fstderr );

    pio->stdout_is_redirected = false;
    pio->stdout_to_stderr = false;
    pio->stdin_is_interactive = true;
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
    gs_lib_ctx_t *pio = mem->gs_lib_ctx;

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
    if (mem->gs_lib_ctx->stderr_fn)
	return (*mem->gs_lib_ctx->stderr_fn)(mem->gs_lib_ctx->caller_handle, str, len);

    code = fwrite(str, 1, len, mem->gs_lib_ctx->fstderr);
    fflush(mem->gs_lib_ctx->fstderr);
    return code;
}

void outflush(const gs_memory_t *mem)
{
    if (mem->gs_lib_ctx->stdout_is_redirected) {
	if (mem->gs_lib_ctx->stdout_to_stderr) {
	    if (!mem->gs_lib_ctx->stderr_fn)
		fflush(mem->gs_lib_ctx->fstderr);
	}
	else
	    fflush(mem->gs_lib_ctx->fstdout2);
    }
    else if (!mem->gs_lib_ctx->stdout_fn)
        fflush(mem->gs_lib_ctx->fstdout);
}

void errflush(const gs_memory_t *mem)
{
    if (!mem->gs_lib_ctx->stderr_fn)
        fflush(mem->gs_lib_ctx->fstderr);
    /* else nothing to flush */
}

