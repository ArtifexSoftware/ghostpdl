/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Interface routines for IJG code, common to encode/decode. */
#include "stdio_.h"
#include "string_.h"
#include "jpeglib_.h"
#include "jerror_.h"
#include "gx.h"
#include "gserrors.h"
#include "strimpl.h"
#include "sdct.h"
#include "sjpeg.h"
#include "gsmchunk.h"

#if !defined(SHARE_JPEG) || SHARE_JPEG==0
#include "jmemcust.h"
#endif

/*
  Ghostscript uses a non-public interface to libjpeg in order to
  override the library's default memory manager implementation.
  Since many users will want to compile Ghostscript using the
  shared jpeg library, we provide these prototypes so that a copy
  of the libjpeg source distribution is not needed.

  The presence of the jmemsys.h header file is detected in
  unix-aux.mak, and written to gconfig_.h
 */

#include "gconfig_.h"

/*
 * Error handling routines (these replace corresponding IJG routines from
 * jpeg/jerror.c).  These are used for both compression and decompression.
 * We assume
 * offset_of(jpeg_compress_data, cinfo)==offset_of(jpeg_decompress_data, dinfo)
 */

static void
gs_jpeg_error_exit(j_common_ptr cinfo)
{
    jpeg_stream_data *jcomdp =
    (jpeg_stream_data *) ((char *)cinfo -
                          offset_of(jpeg_compress_data, cinfo));

    longjmp(find_jmp_buf(jcomdp->exit_jmpbuf), 1);
}

static void
gs_jpeg_emit_message(j_common_ptr cinfo, int msg_level)
{
    if (msg_level < 0) {	/* GS policy is to ignore IJG warnings when Picky=0,
                                 * treat them as errors when Picky=1.
                                 */
        jpeg_stream_data *jcomdp =
        (jpeg_stream_data *) ((char *)cinfo -
                              offset_of(jpeg_compress_data, cinfo));

        if (jcomdp->Picky)
            gs_jpeg_error_exit(cinfo);
    }
    /* Trace messages are always ignored. */
}

/*
 * This routine initializes the error manager fields in the JPEG object.
 * It is based on jpeg_std_error from jpeg/jerror.c.
 */

void
gs_jpeg_error_setup(stream_DCT_state * st)
{
    struct jpeg_error_mgr *err = &st->data.common->err;

    /* Initialize the JPEG compression object with default error handling */
    jpeg_std_error(err);

    /* Replace some methods with our own versions */
    err->error_exit = gs_jpeg_error_exit;
    err->emit_message = gs_jpeg_emit_message;

    st->data.compress->cinfo.err = err;		/* works for decompress case too */
}

/* Stuff the IJG error message into errorinfo after an error exit. */

int
gs_jpeg_log_error(stream_DCT_state * st)
{
    j_common_ptr cinfo = (j_common_ptr) & st->data.compress->cinfo;
    char buffer[JMSG_LENGTH_MAX];

    /* Format the error message */
    (*cinfo->err->format_message) (cinfo, buffer);
    (*st->report_error) ((stream_state *) st, buffer);
    return_error(gs_error_ioerror);	/* caller will do return_error() */
}

/*
 * Interface routines.  This layer of routines exists solely to limit
 * side-effects from using setjmp.
 */

JQUANT_TBL *
gs_jpeg_alloc_quant_table(stream_DCT_state * st)
{
    if (setjmp(find_jmp_buf(st->data.common->exit_jmpbuf))) {
        gs_jpeg_log_error(st);
        return NULL;
    }
    return jpeg_alloc_quant_table((j_common_ptr)
                                  & st->data.compress->cinfo);
}

JHUFF_TBL *
gs_jpeg_alloc_huff_table(stream_DCT_state * st)
{
    if (setjmp(find_jmp_buf(st->data.common->exit_jmpbuf))) {
        gs_jpeg_log_error(st);
        return NULL;
    }
    return jpeg_alloc_huff_table((j_common_ptr)
                                 & st->data.compress->cinfo);
}

int
gs_jpeg_destroy(stream_DCT_state * st)
{
    if (st->data.common && setjmp(find_jmp_buf(st->data.common->exit_jmpbuf)))
        return_error(gs_jpeg_log_error(st));

    if (st->data.compress){
        jpeg_destroy((j_common_ptr) & st->data.compress->cinfo);
        gs_jpeg_mem_term((j_common_ptr) & st->data.compress->cinfo);
    }
    return 0;
}

#if !defined(SHARE_JPEG) || SHARE_JPEG==0
static void *gs_j_mem_alloc(j_common_ptr cinfo, size_t size)
{
    gs_memory_t *mem = (gs_memory_t *)(GET_CUST_MEM_DATA(cinfo)->priv);

    return(gs_alloc_bytes(mem, size, "JPEG allocation"));
}

static void gs_j_mem_free(j_common_ptr cinfo, void *object, size_t size)
{
    gs_memory_t *mem = (gs_memory_t *)(GET_CUST_MEM_DATA(cinfo)->priv);

    gs_free_object(mem, object, "JPEG free");
}

static long gs_j_mem_init (j_common_ptr cinfo)
{
    gs_memory_t *mem = (gs_memory_t *)(GET_CUST_MEM_DATA(cinfo)->priv);
    gs_memory_t *cmem = NULL;

    if (gs_memory_chunk_wrap(&(cmem), mem) < 0) {
        return (-1);
    }
    
    (void)jpeg_cust_mem_set_private(GET_CUST_MEM_DATA(cinfo), cmem);

    return 0;
}

static void gs_j_mem_term (j_common_ptr cinfo)
{
    gs_memory_t *cmem = (gs_memory_t *)(GET_CUST_MEM_DATA(cinfo)->priv);
    gs_memory_t *mem = gs_memory_chunk_unwrap(cmem);

    if (mem == cmem)
        return;

    (void)jpeg_cust_mem_set_private(GET_CUST_MEM_DATA(cinfo), mem);
}
#endif /* SHAREJPEG == 0 */


int gs_jpeg_mem_init (gs_memory_t *mem, j_common_ptr cinfo)
{
    int code = 0;
#if !defined(SHARE_JPEG) || SHARE_JPEG==0
    jpeg_cust_mem_data custm, *custmptr;

    memset(&custm, 0x00, sizeof(custm));

    /* JPEG allocated chunks don't need to be subject to gc. */
    mem = mem->non_gc_memory;

    if (!jpeg_cust_mem_init(&custm, (void *) mem, gs_j_mem_init, gs_j_mem_term, NULL,
                            gs_j_mem_alloc, gs_j_mem_free,
                            gs_j_mem_alloc, gs_j_mem_free, NULL)) {
        code = gs_note_error(gs_error_VMerror);
    }
    if (code == 0) {
        custmptr = (jpeg_cust_mem_data *)gs_alloc_bytes(mem, sizeof(custm) + sizeof(void *), "JPEG custom memory descriptor");
        if (!custmptr) {
            code = gs_note_error(gs_error_VMerror);
        }
        else {
            memcpy(custmptr, &custm, sizeof(custm));
            cinfo->client_data = custmptr;
        }
    }
#endif /* SHAREJPEG == 0 */
    return code;
}

void
gs_jpeg_mem_term(j_common_ptr cinfo)
{
#if !defined(SHARE_JPEG) || SHARE_JPEG==0
    if (cinfo->client_data) {
        jpeg_cust_mem_data *custmptr = (jpeg_cust_mem_data *)cinfo->client_data;
        gs_memory_t *mem = (gs_memory_t *)(GET_CUST_MEM_DATA(cinfo)->priv);
        
        gs_free_object(mem, custmptr, "gs_jpeg_mem_term");
        cinfo->client_data = NULL;
    }
#endif /* SHAREJPEG == 0 */
}
