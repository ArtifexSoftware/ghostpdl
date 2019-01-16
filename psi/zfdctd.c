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


/* DCTDecode filter creation */
#include "memory_.h"
#include "stdio_.h"		/* for jpeglib.h */
#include "jpeglib_.h"
#include "ghost.h"
#include "oper.h"
#include "gsmemory.h"
#include "strimpl.h"
#include "sdct.h"
#include "sjpeg.h"
#include "ialloc.h"
#include "ifilter.h"
#include "iparam.h"

#include "igstate.h"  /* For igs macro */
#include "gxdevcli.h" /* for dev_spec_op */
#include "gxdevsop.h" /* For spec_op enumerated types */

private_st_jpeg_decompress_data();

/* Import the parameter processing procedure from sddparam.c */
stream_state_proc_put_params(s_DCTD_put_params, stream_DCT_state);

/* Find the memory that will be used for allocating the stream. */
static gs_ref_memory_t *
find_stream_memory(i_ctx_t *i_ctx_p, int npop, uint *space)
{
    uint use_space = max(*space, avm_global);
    os_ptr sop = osp - npop;

    if (r_has_type(sop, t_dictionary)) {
        --sop;
    }
    *space = max(use_space, r_space(sop));

    return idmemory->spaces_indexed[*space >> r_space_shift];
}

static int PS_DCTD_PassThrough(void *d, byte *Buffer, int Size)
{
    gx_device *dev = (gx_device *)d;

    if (Buffer == NULL) {
        if (Size == 0)
            dev_proc(dev, dev_spec_op)(dev, gxdso_JPEG_passthrough_end, NULL, 0);
        else
            dev_proc(dev, dev_spec_op)(dev, gxdso_JPEG_passthrough_begin, NULL, 0);
    } else {
        dev_proc(dev, dev_spec_op)(dev, gxdso_JPEG_passthrough_data, Buffer, Size);
    }
    return 0;
}

/* <source> <dict> DCTDecode/filter <file> */
/* <source> DCTDecode/filter <file> */
static int
zDCTD(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_memory_t *mem;
    stream_DCT_state state;
    dict_param_list list;
    jpeg_decompress_data *jddp;
    int code;
    const ref *dop;
    uint dspace;
    gx_device *dev = gs_currentdevice(igs);

    if (r_has_type(op, t_dictionary))
        dop = op, dspace = r_space(op);
    else
        dop = 0, dspace = 0;
    mem = (gs_memory_t *)find_stream_memory(i_ctx_p, 0, &dspace);
    state.memory = mem;
    /* First allocate space for IJG parameters. */
    jddp = gs_alloc_struct_immovable(mem,jpeg_decompress_data,
      &st_jpeg_decompress_data, "zDCTD");
    if (jddp == 0)
        return_error(gs_error_VMerror);
    if (s_DCTD_template.set_defaults)
        (*s_DCTD_template.set_defaults) ((stream_state *) & state);
    state.data.decompress = jddp;
    jddp->memory = state.jpeg_memory = mem;	/* set now for allocation */
    jddp->scanline_buffer = NULL;	/* set this early for safe error exit */
    state.report_error = filter_report_error;	/* in case create fails */
    if ((code = gs_jpeg_create_decompress(&state)) < 0)
        goto fail;		/* correct to do jpeg_destroy here */
    /* Read parameters from dictionary */
    if ((code = dict_param_list_read(&list, dop, NULL, false, iimemory)) < 0)
        goto fail;
    if ((code = s_DCTD_put_params((gs_param_list *) & list, &state)) < 0)
        goto rel;

    if (dev_proc(dev, dev_spec_op)(dev, gxdso_JPEG_passthrough_query, NULL, 0) > 0) {
        jddp->StartedPassThrough = 0;
        jddp->PassThrough = 1;
        jddp->PassThroughfn = (PS_DCTD_PassThrough);
        jddp->device = (void *)dev;
    }
    else {
        jddp->PassThrough = 0;
        jddp->device = (void *)NULL;
    }

    /* Create the filter. */
    jddp->templat = s_DCTD_template;
    code = filter_read(i_ctx_p, 0, &jddp->templat,
                       (stream_state *) & state, dspace);
    if (code >= 0)		/* Success! */
        return code;
    /*
     * We assume that if filter_read fails, the stream has not been
     * registered for closing, so s_DCTD_release will never be called.
     * Therefore we free the allocated memory before failing.
     */
rel:
    iparam_list_release(&list);
fail:
    gs_jpeg_destroy(&state);
    gs_free_object(mem, jddp, "zDCTD fail");
    return code;
}

/* ------ Initialization procedure ------ */

const op_def zfdctd_op_defs[] =
{
    op_def_begin_filter(),
    {"2DCTDecode", zDCTD},
    op_def_end(0)
};
