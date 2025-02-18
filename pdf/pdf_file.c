/* Copyright (C) 2018-2025 Artifex Software, Inc.
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

#include "ghostpdf.h"
#include "pdf_types.h"
#include "pdf_stack.h"
#include "pdf_dict.h"
#include "pdf_file.h"
#include "pdf_int.h"
#include "pdf_array.h"
#include "pdf_misc.h"
#include "pdf_sec.h"
#include "stream.h"
#include "strimpl.h"
#include "strmio.h"
#include "gpmisc.h"
#include "simscale.h"   /* SIMScaleDecode */
#include "szlibx.h"     /* Flate */
#include "spngpx.h"     /* PNG Predictor */
#include "spdiffx.h"    /* Horizontal differencing predictor */
#include "slzwx.h"      /* LZW ZLib */
#include "sstring.h"    /* ASCIIHexDecode */
#include "sa85d.h"      /* ASCII85Decode */
#include "scfx.h"       /* CCITTFaxDecode */
#include "srlx.h"       /* RunLengthDecode */
#include "jpeglib_.h"
#include "sdct.h"       /* DCTDecode */
#include "sjpeg.h"
#include "sfilter.h"    /* SubFileDecode and PFBDecode */
#include "sarc4.h"      /* Arc4Decode */
#include "saes.h"       /* AESDecode */
#include "ssha2.h"      /* SHA256Encode */
#include "gxdevsop.h"       /* For special ops */

#ifdef USE_LDF_JB2
#include "sjbig2_luratech.h"
#else
#include "sjbig2.h"
#endif
#if defined(USE_LWF_JP2)
#  include "sjpx_luratech.h"
#elif defined(USE_OPENJPEG_JP2)
#  include "sjpx_openjpeg.h"
#else
#  include "sjpx.h"
#endif

extern const uint file_default_buffer_size;

static void pdfi_close_filter_chain(pdf_context *ctx, stream *s, stream *target);

/* Utility routine to create a pdf_c_stream object */
static int pdfi_alloc_stream(pdf_context *ctx, stream *source, stream *original, pdf_c_stream **new_stream)
{
    *new_stream = NULL;
    *new_stream = (pdf_c_stream *)gs_alloc_bytes(ctx->memory, sizeof(pdf_c_stream), "pdfi_alloc_stream");
    if (*new_stream == NULL)
        return_error(gs_error_VMerror);
    memset(*new_stream, 0x00, sizeof(pdf_c_stream));
    (*new_stream)->eof = false;
    ((pdf_c_stream *)(*new_stream))->s = source;
    ((pdf_c_stream *)(*new_stream))->original = original;
    return 0;
}

/***********************************************************************************/
/* Decompression filters.                                                          */

static int
pdfi_filter_report_error(stream_state * st, const char *str)
{
    if_debug1m('s', st->memory, "[s]stream error: %s\n", str);
    strncpy(st->error_string, str, STREAM_MAX_ERROR_STRING);
    /* Ensure null termination. */
    st->error_string[STREAM_MAX_ERROR_STRING] = 0;
    return 0;
}

/* Open a file stream for a filter. */
static int
pdfi_filter_open(uint buffer_size,
            const stream_procs * procs, const stream_template * templat,
            const stream_state * st, gs_memory_t *mem, stream **new_stream)
{
    stream *s;
    uint ssize = gs_struct_type_size(templat->stype);
    stream_state *sst = NULL;
    int code;

    if (templat->stype != &st_stream_state) {
        sst = s_alloc_state(mem, templat->stype, "pdfi_filter_open(stream_state)");
        if (sst == NULL)
            return_error(gs_error_VMerror);
    }
    if (buffer_size < 128)
        buffer_size = file_default_buffer_size;

    code = file_open_stream((char *)0, 0, "r", buffer_size, &s,
                                (gx_io_device *)0, (iodev_proc_fopen_t)0, mem);
    if (code < 0) {
        gs_free_object(mem, sst, "pdfi_filter_open(stream_state)");
        return code;
    }
    s_std_init(s, s->cbuf, s->bsize, procs, s_mode_read);
    s->procs.process = templat->process;
    s->save_close = s->procs.close;
    s->procs.close = file_close_file;
    s->close_at_eod = 0;
    if (sst == NULL) {
        /* This stream doesn't have any state of its own. */
        /* Hack: use the stream itself as the state. */
        sst = (stream_state *) s;
    } else if (st != NULL)         /* might not have client parameters */
        memcpy(sst, st, ssize);
    s->state = sst;
    s_init_state(sst, templat, mem);
    sst->report_error = pdfi_filter_report_error;

    if (templat->init != NULL) {
        code = (*templat->init)(sst);
        if (code < 0) {
            gs_free_object(mem, sst, "filter_open(stream_state)");
            gs_free_object(mem, s->cbuf, "filter_open(buffer)");
            gs_free_object(mem, s, "filter_open(stream)");
            return code;
        }
    }
    *new_stream = s;
    return 0;
}

static int pdfi_Predictor_filter(pdf_context *ctx, pdf_dict *d, stream *source, stream **new_stream)
{
    int code;
    int64_t Predictor, Colors, BPC, Columns;
    uint min_size;
    stream_PNGP_state pps;
    stream_PDiff_state ppds;
    /* NOTE: 'max_min_left=1' is a horribly named definition from stream.h */

    code = pdfi_dict_get_int_def(ctx, d, "Predictor", &Predictor, 1);
    if (code < 0)
        return code;

    /* Predictor values 0,1 are identity (use the existing stream).
     * The other values need to be cascaded.
     */
    switch(Predictor) {
        case 0:
            Predictor = 1;
            break;
        case 1:
            break;
        case 2:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            /* grab values common to both kinds of predictors */
            min_size = s_zlibD_template.min_out_size + max_min_left;
            code = pdfi_dict_get_int_def(ctx, d, "Colors", &Colors, 1);
            if (code < 0)
                return code;
            if (Colors < 1 || Colors > s_PNG_max_Colors)
                return_error(gs_error_rangecheck);

            code = pdfi_dict_get_int_def(ctx, d, "BitsPerComponent", &BPC, 8);
            if (code < 0)
                return code;
            /* tests for 1-16, powers of 2 */
            if (BPC < 1 || BPC > 16 || (BPC & (BPC - 1)) != 0)
                return_error(gs_error_rangecheck);

            code = pdfi_dict_get_int_def(ctx, d, "Columns", &Columns, 1);
            if (code < 0)
                return code;
            if (Columns < 1)
                return_error(gs_error_rangecheck);
            break;
        default:
            return_error(gs_error_rangecheck);
    }

    switch(Predictor) {
        case 1:
            *new_stream = source;
            break;
        case 2:
            /* zpd_setup, componentwise horizontal differencing */
            ppds.Colors = (int)Colors;
            ppds.BitsPerComponent = (int)BPC;
            ppds.Columns = (int)Columns;
            code = pdfi_filter_open(min_size, &s_filter_read_procs,
                             (const stream_template *)&s_PDiffD_template,
                             (const stream_state *)&ppds, ctx->memory->non_gc_memory, new_stream);
            if (code < 0)
                return code;

            (*new_stream)->strm = source;
            break;
        default:
            /* zpp_setup, PNG predictor */
            pps.Colors = (int)Colors;
            pps.BitsPerComponent = (int)BPC;
            pps.Columns = (uint)Columns;
            pps.Predictor = Predictor;
            code = pdfi_filter_open(min_size, &s_filter_read_procs,
                             (const stream_template *)&s_PNGPD_template,
                             (const stream_state *)&pps, ctx->memory->non_gc_memory, new_stream);
            if (code < 0)
                return code;

            (*new_stream)->strm = source;
            break;
    }
    return 0;
}

int pdfi_apply_Arc4_filter(pdf_context *ctx, pdf_string *Key, pdf_c_stream *source, pdf_c_stream **new_stream)
{
    int code = 0;
    stream_arcfour_state state;
    stream *new_s;
    int min_size = 2048;

    s_arcfour_set_key(&state, (const unsigned char *)Key->data, Key->length); /* lgtm [cpp/weak-cryptographic-algorithm] */

    code = pdfi_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_arcfour_template, (const stream_state *)&state, ctx->memory->non_gc_memory, &new_s);
    if (code < 0)
        return code;

    code = pdfi_alloc_stream(ctx, new_s, source->s, new_stream);
    new_s->strm = source->s;
    return code;
}

int pdfi_apply_AES_filter(pdf_context *ctx, pdf_string *Key, bool use_padding, pdf_c_stream *source, pdf_c_stream **new_stream)
{
    stream_aes_state state;
    uint min_size = 2048;
    int code = 0;
    stream *new_s;

    s_aes_set_key(&state, Key->data, Key->length);
    s_aes_set_padding(&state, use_padding);

    code = pdfi_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_aes_template, (const stream_state *)&state, ctx->memory->non_gc_memory, &new_s);

    if (code < 0)
        return code;

    code = pdfi_alloc_stream(ctx, new_s, source->s, new_stream);
    new_s->strm = source->s;
    return code;
}

#ifdef UNUSED_FILTER
int pdfi_apply_SHA256_filter(pdf_context *ctx, pdf_c_stream *source, pdf_c_stream **new_stream)
{
    stream_SHA256E_state state;
    uint min_size = 2048;
    int code = 0;
    stream *new_s;

    pSHA256_Init(&state.sha256);
    code = pdfi_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_SHA256E_template, (const stream_state *)&state, ctx->memory->non_gc_memory, &new_s);

    if (code < 0)
        return code;

    code = pdfi_alloc_stream(ctx, new_s, source->s, new_stream);
    new_s->strm = source->s;
    return code;
}
#endif

int pdfi_apply_imscale_filter(pdf_context *ctx, pdf_string *Key, int width, int height, pdf_c_stream *source, pdf_c_stream **new_stream)
{
    int code = 0;
    stream_imscale_state state;
    stream *new_s;

    state.params.spp_decode = 1;
    state.params.spp_interp = 1;
    state.params.BitsPerComponentIn = 1;
    state.params.MaxValueIn = 1;
    state.params.WidthIn = width;
    state.params.HeightIn = height;
    state.params.BitsPerComponentOut = 1;
    state.params.MaxValueOut = 1;
    state.params.WidthOut = width << 2;
    state.params.HeightOut = height << 2;

    code = pdfi_filter_open(2048, &s_filter_read_procs, (const stream_template *)&s_imscale_template, (const stream_state *)&state, ctx->memory->non_gc_memory, &new_s);

    if (code < 0)
        return code;

    code = pdfi_alloc_stream(ctx, new_s, source->s, new_stream);
    new_s->strm = source->s;
    return code;
}

static int pdfi_Flate_filter(pdf_context *ctx, pdf_dict *d, stream *source, stream **new_stream)
{
    stream_zlib_state zls;
    uint min_size = 2048;
    int code;
    stream *Flate_source = NULL;

    memset(&zls, 0, sizeof(zls));

    /* s_zlibD_template defined in base/szlibd.c */
    (*s_zlibD_template.set_defaults)((stream_state *)&zls);

    code = pdfi_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_zlibD_template, (const stream_state *)&zls, ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        return code;

    (*new_stream)->strm = source;
    source = *new_stream;

    if (d && pdfi_type_of(d) == PDF_DICT) {
        Flate_source = (*new_stream)->strm;
        code = pdfi_Predictor_filter(ctx, d, source, new_stream);
        if (code < 0)
            pdfi_close_filter_chain(ctx, source, Flate_source);
    }
    return code;
}

static int
pdfi_JBIG2Decode_filter(pdf_context *ctx, pdf_dict *dict, pdf_dict *decode,
                        stream *source, stream **new_stream)
{
    stream_jbig2decode_state state;
    uint min_size = s_jbig2decode_template.min_out_size;
    int code;
    pdf_stream *Globals = NULL;
    byte *buf = NULL;
    int64_t buflen = 0;
    void *globalctx;

    s_jbig2decode_set_global_data((stream_state*)&state, NULL, NULL);

    if (decode) {
        code = pdfi_dict_knownget_type(ctx, decode, "JBIG2Globals", PDF_STREAM,
                                       (pdf_obj **)&Globals);
        if (code < 0) {
            goto cleanupExit;
        }

        /* read in the globals from stream */
        if (code > 0) {
            code = pdfi_stream_to_buffer(ctx, Globals, &buf, &buflen);
            if (code == 0) {
                code = s_jbig2decode_make_global_data(ctx->memory->non_gc_memory,
                                                      buf, buflen, &globalctx);
                if (code < 0)
                    goto cleanupExit;

                s_jbig2decode_set_global_data((stream_state*)&state, NULL, globalctx);
            }
        }
    }

    code = pdfi_filter_open(min_size, &s_filter_read_procs,
                            (const stream_template *)&s_jbig2decode_template,
                            (const stream_state *)&state, ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        goto cleanupExit;

    (*new_stream)->strm = source;
    code = 0;

 cleanupExit:
    gs_free_object(ctx->memory, buf, "pdfi_JBIG2Decode_filter (Globals buf)");
    pdfi_countdown(Globals);
    return code;
}

static int pdfi_LZW_filter(pdf_context *ctx, pdf_dict *d, stream *source, stream **new_stream)
{
    stream_LZW_state lzs;
    uint min_size = 2048;
    int code;
    int64_t i;

    /* s_zlibD_template defined in base/szlibd.c */
    s_LZW_set_defaults_inline(&lzs);

    if (d && pdfi_type_of(d) == PDF_DICT) {
        code = pdfi_dict_get_int(ctx, d, "EarlyChange", &i);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0) {
            if (i == 0)
                lzs.EarlyChange = false;
            else
                lzs.EarlyChange = true;
        }
    }

    code = pdfi_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_LZWD_template, (const stream_state *)&lzs, ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        return code;
    (*new_stream)->strm = source;
    source = *new_stream;

    if (d && pdfi_type_of(d) == PDF_DICT)
        pdfi_Predictor_filter(ctx, d, source, new_stream);
    return 0;
}

static int PS_JPXD_PassThrough(void *d, byte *Buffer, int Size)
{
    gx_device *dev = (gx_device *)d;

    if (Buffer == NULL) {
        if (Size == 0)
            dev_proc(dev, dev_spec_op)(dev, gxdso_JPX_passthrough_end, NULL, 0);
        else
            dev_proc(dev, dev_spec_op)(dev, gxdso_JPX_passthrough_begin, NULL, 0);
    } else {
        dev_proc(dev, dev_spec_op)(dev, gxdso_JPX_passthrough_data, Buffer, Size);
    }
    return 0;
}

/*
 * dict -- the dict that contained the decoder (i.e. the image dict)
 * decode -- the decoder dict
 */
static int
pdfi_JPX_filter(pdf_context *ctx, pdf_dict *dict, pdf_dict *decode,
                stream *source, stream **new_stream)
{
    stream_jpxd_state state;
    uint min_size = s_jpxd_template.min_out_size;
    int code;
    pdf_obj *csobj = NULL;
    pdf_name *csname = NULL;
    bool alpha;
    gx_device *dev = gs_currentdevice(ctx->pgs);

    state.memory = ctx->memory->non_gc_memory;
    if (s_jpxd_template.set_defaults)
      (*s_jpxd_template.set_defaults)((stream_state *)&state);

    /* Pull some extra params out of the image dict */
    if (dict) {
        /* This Alpha is a thing that gs code uses to tell that we
         * are doing an SMask.  It's a bit of a hack, but
         * I guess we can do the same.
         */
        code = pdfi_dict_get_bool(ctx, dict, "Alpha", &alpha);
        if (code == 0)
            state.alpha = alpha;
    }
    if (dict && pdfi_dict_get(ctx, dict, "ColorSpace", &csobj) == 0) {
        /* parse the value */
        switch (pdfi_type_of(csobj)) {
        case PDF_ARRAY:
            /* assume it's the first array element */
            code = pdfi_array_get(ctx, (pdf_array *)csobj, (uint64_t)0, (pdf_obj **)&csname);
            if (code < 0) {
                pdfi_countdown(csobj);
                return code;
            }
            break;
        case PDF_NAME:
            /* use the name directly */
            csname = (pdf_name *)csobj;
            csobj = NULL; /* To keep ref counting straight */
            break;
        default:
            dmprintf(ctx->memory, "warning: JPX ColorSpace value is an unhandled type!\n");
            break;
        }
        if (csname != NULL && pdfi_type_of(csname) == PDF_NAME) {
            /* request raw index values if the colorspace is /Indexed */
            if (pdfi_name_is(csname, "Indexed"))
                state.colorspace = gs_jpx_cs_indexed;
            /* tell the filter what output we want for other spaces */
            else if (pdfi_name_is(csname, "DeviceGray"))
                state.colorspace = gs_jpx_cs_gray;
            else if (pdfi_name_is(csname, "DeviceRGB"))
                state.colorspace = gs_jpx_cs_rgb;
            else if (pdfi_name_is(csname, "DeviceCMYK"))
                state.colorspace = gs_jpx_cs_cmyk;
            else if (pdfi_name_is(csname, "ICCBased")) {
                /* TODO: I don't think this even happens without PS wrapper code? */
#if 0
                /* The second array element should be the profile's
                   stream dict */
                ref *csdict = csobj->value.refs + 1;
                ref *nref;
                ref altname;
                if (r_is_array(csobj) && (r_size(csobj) > 1) &&
                    r_has_type(csdict, t_dictionary)) {
                    check_dict_read(*csdict);
                    /* try to look up the alternate space */
                    if (dict_find_string(csdict, "Alternate", &nref) > 0) {
                        name_string_ref(imemory, csname, &altname);
                        if (pdfi_name_is(&altname, "DeviceGray"))
                            state.colorspace = gs_jpx_cs_gray;
                        else if (pdfi_name_is(&altname, "DeviceRGB"))
                            state.colorspace = gs_jpx_cs_rgb;
                        else if (pdfi_name_is(&altname, "DeviceCMYK"))
                            state.colorspace = gs_jpx_cs_cmyk;
                    }
                    /* else guess based on the number of components */
                    if (state.colorspace == gs_jpx_cs_unset &&
                        dict_find_string(csdict, "N", &nref) > 0) {
                        if_debug1m('w', imemory, "[w] JPX image has an external %"PRIpsint
                                   " channel colorspace\n", nref->value.intval);
                        switch (nref->value.intval) {
                        case 1: state.colorspace = gs_jpx_cs_gray;
                            break;
                        case 3: state.colorspace = gs_jpx_cs_rgb;
                            break;
                        case 4: state.colorspace = gs_jpx_cs_cmyk;
                            break;
                        }
                    }
                }
#endif
            }
        }
    }

    if (csobj)
        pdfi_countdown(csobj);
    if (csname)
        pdfi_countdown(csname);


    if (dev_proc(dev, dev_spec_op)(dev, gxdso_JPX_passthrough_query, NULL, 0) > 0) {
        state.StartedPassThrough = 0;
        state.PassThrough = 1;
        state.PassThroughfn = (PS_JPXD_PassThrough);
        state.device = (void *)dev;
    }
    else {
        state.PassThrough = 0;
        state.device = (void *)NULL;
    }

    code = pdfi_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_jpxd_template,
                            (const stream_state *)&state, ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        return code;
    (*new_stream)->strm = source;
    source = *new_stream;

    return 0;
}

private_st_jpeg_decompress_data();

static int PDF_DCTD_PassThrough(void *d, byte *Buffer, int Size)
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

static int pdfi_DCT_filter(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *decode,
                           stream *source, stream **new_stream)
{
    stream_DCT_state dcts;
    uint min_size = s_DCTD_template.min_out_size;
    int code;
    int64_t i;
    jpeg_decompress_data *jddp;
    gx_device *dev = gs_currentdevice_inline(ctx->pgs);
    double Height = 0;

    dcts.memory = ctx->memory;
    /* First allocate space for IJG parameters. */
    jddp = gs_alloc_struct_immovable(ctx->memory, jpeg_decompress_data,
      &st_jpeg_decompress_data, "pdfi_DCT");
    if (jddp == 0)
        return_error(gs_error_VMerror);
    if (s_DCTD_template.set_defaults)
        (*s_DCTD_template.set_defaults) ((stream_state *) & dcts);

    dcts.data.decompress = jddp;
    jddp->memory = dcts.jpeg_memory = ctx->memory;	/* set now for allocation */
    jddp->scanline_buffer = NULL;	                /* set this early for safe error exit */
    dcts.report_error = pdfi_filter_report_error;	    /* in case create fails */
    if ((code = gs_jpeg_create_decompress(&dcts)) < 0) {
        gs_jpeg_destroy(&dcts);
        gs_free_object(ctx->memory, jddp, "zDCTD fail");
        return code;
    }

    if (decode && pdfi_type_of(decode) == PDF_DICT) {
        /* TODO: Why is this here?  'i' never gets used? */
        code = pdfi_dict_get_int(ctx, decode, "ColorTransform", &i);
        if (code < 0 && code != gs_error_undefined)
            return code;
    }

    if (dev_proc(dev, dev_spec_op)(dev, gxdso_JPEG_passthrough_query, NULL, 0) > 0) {
        jddp->StartedPassThrough = 0;
        jddp->PassThrough = 1;
        jddp->PassThroughfn = (PDF_DCTD_PassThrough);
        jddp->device = (void *)dev;
    }
    else {
        jddp->PassThrough = 0;
        jddp->device = (void *)NULL;
    }

    /* Hack for Bug695112.pdf to grab a height in case it is missing from the JPEG data */
    code = pdfi_dict_knownget_number(ctx, stream_dict, "Height", &Height);
    if (code < 0)
        return code;
    jddp->Height = (int)floor(Height);

    jddp->templat = s_DCTD_template;

    code = pdfi_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&jddp->templat, (const stream_state *)&dcts, ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        return code;
    (*new_stream)->strm = source;
    source = *new_stream;

    return 0;
}

static int pdfi_ASCII85_filter(pdf_context *ctx, pdf_dict *d, stream *source, stream **new_stream)
{
    stream_A85D_state ss;
    uint min_size = 2048;
    int code;

    ss.pdf_rules = true;

    code = pdfi_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_A85D_template, (const stream_state *)&ss, ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        return code;

    (*new_stream)->strm = source;
    return 0;
}

static int pdfi_CCITTFax_filter(pdf_context *ctx, pdf_dict *d, stream *source, stream **new_stream)
{
    stream_CFD_state ss;
    uint min_size = 2048;
    bool bval;
    int code;
    int64_t i;

    s_CF_set_defaults_inline(&ss);

    if (d && pdfi_type_of(d) == PDF_DICT) {
        code = pdfi_dict_get_int(ctx, d, "K", &i);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.K = i;

        code = pdfi_dict_get_bool(ctx, d, "EndOfLine", &bval);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.EndOfLine = bval ? 1 : 0;

        code = pdfi_dict_get_bool(ctx, d, "EncodedByteAlign", &bval);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.EncodedByteAlign = bval ? 1 : 0;

        code = pdfi_dict_get_bool(ctx, d, "EndOfBlock", &bval);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.EndOfBlock = bval ? 1 : 0;

        code = pdfi_dict_get_bool(ctx, d, "BlackIs1", &bval);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.BlackIs1 = bval ? 1 : 0;

        code = pdfi_dict_get_int(ctx, d, "Columns", &i);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.Columns = i;

        code = pdfi_dict_get_int(ctx, d, "Rows", &i);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.Rows = i;

        code = pdfi_dict_get_int(ctx, d, "DamagedRowsBeforeError", &i);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.DamagedRowsBeforeError = i;

    }

    code = pdfi_filter_open(min_size, &s_filter_read_procs,
                            (const stream_template *)&s_CFD_template,
                            (const stream_state *)&ss,
                            ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        return code;

    (*new_stream)->strm = source;
    return 0;
}

static int pdfi_RunLength_filter(pdf_context *ctx, pdf_dict *d, stream *source, stream **new_stream)
{
    stream_RLD_state ss;
    uint min_size = 2048;
    int code;

    if (s_RLD_template.set_defaults)
        (*s_RLD_template.set_defaults) ((stream_state *) & ss);

    code = pdfi_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_RLD_template, (const stream_state *)&ss, ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        return code;

    (*new_stream)->strm = source;
    return 0;
}

static int pdfi_simple_filter(pdf_context *ctx, const stream_template *tmplate, stream *source, stream **new_stream)
{
    uint min_size = 2048;
    int code;

    code = pdfi_filter_open(min_size, &s_filter_read_procs, tmplate, NULL, ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        return code;

    (*new_stream)->strm = source;
    return 0;
}

static int pdfi_apply_filter(pdf_context *ctx, pdf_dict *dict, pdf_name *n, pdf_dict *decode,
                             stream *source, stream **new_stream, bool inline_image)
{
    int code;

    if (ctx->args.pdfdebug)
    {
        char *str;
        str = (char *)gs_alloc_bytes(ctx->memory, n->length + 1, "temp string for debug");
        if (str == NULL)
            return_error(gs_error_VMerror);
        memcpy(str, (const char *)n->data, n->length);
        str[n->length] = '\0';
        dmprintf1(ctx->memory, "FILTER NAME:%s\n", str);
        gs_free_object(ctx->memory, str, "temp string for debug");
    }

    if (pdfi_name_is(n, "RunLengthDecode")) {
        code = pdfi_RunLength_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "CCITTFaxDecode")) {
        code = pdfi_CCITTFax_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "ASCIIHexDecode")) {
        code = pdfi_simple_filter(ctx, &s_AXD_template, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "ASCII85Decode")) {
        code = pdfi_ASCII85_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "SubFileDecode")) {
        code = pdfi_simple_filter(ctx, &s_SFD_template, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "FlateDecode")) {
        code = pdfi_Flate_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "JBIG2Decode")) {
        code = pdfi_JBIG2Decode_filter(ctx, dict, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "LZWDecode")) {
        code = pdfi_LZW_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "DCTDecode")) {
        code = pdfi_DCT_filter(ctx, dict, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "JPXDecode")) {
        code = pdfi_JPX_filter(ctx, dict, decode, source, new_stream);
        return code;
    }

    if (pdfi_name_is(n, "AHx")) {
        if (!inline_image) {
            if ((code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_BAD_INLINEFILTER, "pdfi_apply_filter", NULL)) < 0)
                return code;
        }
        code = pdfi_simple_filter(ctx, &s_AXD_template, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "A85")) {
        if (!inline_image) {
            if ((code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_BAD_INLINEFILTER, "pdfi_apply_filter", NULL)) < 0)
                return code;
        }
        code = pdfi_ASCII85_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "LZW")) {
        if (!inline_image) {
            if ((code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_BAD_INLINEFILTER, "pdfi_apply_filter", NULL)) < 0)
                return code;
        }
        code = pdfi_LZW_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "CCF")) {
        if (!inline_image) {
            if ((code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_BAD_INLINEFILTER, "pdfi_apply_filter", NULL)) < 0)
                return code;
        }
        code = pdfi_CCITTFax_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "DCT")) {
        if (!inline_image) {
            if ((code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_BAD_INLINEFILTER, "pdfi_apply_filter", NULL)) < 0)
                return code;
        }
        code = pdfi_DCT_filter(ctx, dict, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "Fl")) {
        if (!inline_image) {
            if ((code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_BAD_INLINEFILTER, "pdfi_apply_filter", NULL)) < 0)
                return code;
        }
        code = pdfi_Flate_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "RL")) {
        if (!inline_image) {
            if ((code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_BAD_INLINEFILTER, "pdfi_apply_filter", NULL)) < 0)
                return code;
        }
        code = pdfi_RunLength_filter(ctx, decode, source, new_stream);
        return code;
    }

    pdfi_set_error(ctx, 0, NULL, E_PDF_UNKNOWNFILTER, "pdfi_apply_filter", NULL);
    return_error(gs_error_undefined);
}

int pdfi_filter_no_decryption(pdf_context *ctx, pdf_stream *stream_obj,
                              pdf_c_stream *source, pdf_c_stream **new_stream, bool inline_image)
{
    pdf_obj *o = NULL, *o1 = NULL;
    pdf_obj *decode = NULL;
    pdf_obj *Filter = NULL;
    pdf_dict *stream_dict = NULL;
    pdf_array *DecodeParams = NULL;
    int code;
    int64_t i, j, duplicates;
    stream *s = source->s, *new_s = NULL;

    *new_stream = NULL;

    if (ctx->args.pdfdebug) {
        gs_offset_t stream_offset = pdfi_stream_offset(ctx, stream_obj);
        dmprintf2(ctx->memory, "Filter: offset %ld(0x%lx)\n", stream_offset, stream_offset);
    }

    code = pdfi_dict_from_obj(ctx, (pdf_obj *)stream_obj, &stream_dict);
    if (code < 0)
        goto exit;

    /* ISO 32000-2:2020 (PDF 2.0) - abbreviated names take precendence. */
    if (inline_image) {
        code = pdfi_dict_knownget(ctx, stream_dict, "F", &Filter);
        if (code == 0)
            code = pdfi_dict_knownget(ctx, stream_dict, "Filter", &Filter);
    } else
        code = pdfi_dict_knownget(ctx, stream_dict, "Filter", &Filter);

    if (code < 0)
        goto exit;
    if (code == 0) {
        /* No filter, just open the stream */
        code = pdfi_alloc_stream(ctx, s, source->s, new_stream);
        goto exit;
    }

    switch (pdfi_type_of(Filter)) {
    default:
        code = gs_note_error(gs_error_typecheck);
        goto exit;
    case PDF_NAME:
        /* ISO 32000-2:2020 (PDF 2.0) - abbreviated names take precendence. */
        if (inline_image) {
            code = pdfi_dict_knownget(ctx, stream_dict, "DP", &decode);
            if (code == 0)
                code = pdfi_dict_knownget(ctx, stream_dict, "DecodeParms", &decode);
        } else {
            code = pdfi_dict_knownget(ctx, stream_dict, "DecodeParms", &decode);
            if (code == 0)
                code = pdfi_dict_knownget(ctx, stream_dict, "DP", &decode);
        }
        if (code < 0)
            goto exit;

        code = pdfi_apply_filter(ctx, stream_dict, (pdf_name *)Filter,
                                 (pdf_dict *)decode, s, &new_s, inline_image);
        if (code < 0)
            goto exit;

        code = pdfi_alloc_stream(ctx, new_s, source->s, new_stream);
        break;
    case PDF_ARRAY:
    {
        pdf_array *filter_array = (pdf_array *)Filter;

        /* ISO 32000-2:2020 (PDF 2.0) - abbreviated names take precendence. */
        if (inline_image) {
            code = pdfi_dict_knownget(ctx, stream_dict, "DP", (pdf_obj **)&DecodeParams);
            if (code == 0)
                code = pdfi_dict_knownget(ctx, stream_dict, "DecodeParms", (pdf_obj **)&DecodeParams);
        } else {
            code = pdfi_dict_knownget(ctx, stream_dict, "DecodeParms", (pdf_obj **)&DecodeParams);
            if (code == 0)
                code = pdfi_dict_knownget(ctx, stream_dict, "DP", &decode);
        }
        if (code < 0)
            goto exit;

        if (DecodeParams != NULL) {
            if (pdfi_array_size(DecodeParams) == 0 || pdfi_array_size(DecodeParams) != pdfi_array_size(filter_array)) {
                pdfi_countdown(DecodeParams);
                DecodeParams = NULL;
                if ((code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_rangecheck), NULL, W_PDF_STREAM_BAD_DECODEPARMS, "pdfi_filter_no_decryption", NULL)) < 0)
                    goto exit;
            } else {
                if (pdfi_array_size(DecodeParams) != pdfi_array_size(filter_array)) {
                    code = gs_note_error(gs_error_typecheck);
                    goto exit;
                }
            }
        }

        /* Check the Filter array to see if we have any duplicates (to prevent filter bombs)
         * For now we will allow one duplicate (in case people do stupid things like ASCIIEncode
         * and Flate and ASCIIEncode again or something).
         */
        for (i = 0; i < (int)pdfi_array_size(filter_array) - 1;i++) {
            code = pdfi_array_get_type(ctx, filter_array, i, PDF_NAME, &o);
            if (code < 0)
                goto exit;
            duplicates = 0;

            for (j = i + 1; j < pdfi_array_size(filter_array);j++) {
                code = pdfi_array_get_type(ctx, filter_array, j, PDF_NAME, &o1);
                if (code < 0) {
                    goto exit;
                }
                if (((pdf_name *)o)->length == ((pdf_name *)o1)->length) {
                    if (memcmp(((pdf_name *)o)->data, ((pdf_name *)o1)->data, ((pdf_name *)o)->length) == 0)
                        duplicates++;
                }
                pdfi_countdown(o1);
                o1 = NULL;
            }
            pdfi_countdown(o);
            o = NULL;
            if (duplicates > 2) {
                pdfi_set_error(ctx, 0, NULL, E_PDF_BADSTREAM, "pdfi_filter_nodecryption", (char *)"**** ERROR Detected possible filter bomb (duplicate Filters).  Aborting processing");
                code = gs_note_error(gs_error_syntaxerror);
                goto exit;
            }
        }

        for (i = 0; i < pdfi_array_size(filter_array);i++) {
            code = pdfi_array_get_type(ctx, filter_array, i, PDF_NAME, &o);
            if (code < 0)
                goto error;
            if (DecodeParams != NULL) {
                code = pdfi_array_get(ctx, DecodeParams, i, &decode);
                if (code < 0) {
                    goto error;
                }
            }
            if (decode && decode != PDF_NULL_OBJ && pdfi_type_of(decode) != PDF_DICT) {
                pdfi_countdown(decode);
                decode = NULL;
                if ((code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_typecheck), NULL, W_PDF_STREAM_BAD_DECODEPARMS, "pdfi_filter_no_decryption", NULL)) < 0)
                    goto error;
            }
            if (decode && decode == PDF_NULL_OBJ) {
                pdfi_countdown(decode);
                decode = NULL;
            }

            code = pdfi_apply_filter(ctx, stream_dict, (pdf_name *)o,
                                     (pdf_dict *)decode, s, &new_s, inline_image);
            pdfi_countdown(decode);
            decode = NULL;
            pdfi_countdown(o);
            o = NULL;
            if (code < 0)
                goto error;

            s = new_s;
        }
        code = pdfi_alloc_stream(ctx, s, source->s, new_stream);
    }
    }

 exit:
    pdfi_countdown(o);
    pdfi_countdown(o1);
    pdfi_countdown(DecodeParams);
    pdfi_countdown(decode);
    pdfi_countdown(Filter);
    return code;

 error:
    if (s)
        pdfi_close_filter_chain(ctx, s, source->s);
    *new_stream = NULL;
    pdfi_countdown(o);
    pdfi_countdown(o1);
    pdfi_countdown(DecodeParams);
    pdfi_countdown(decode);
    pdfi_countdown(Filter);
    return code;
}

int pdfi_filter(pdf_context *ctx, pdf_stream *stream_obj, pdf_c_stream *source,
                pdf_c_stream **new_stream, bool inline_image)
{
    int code;
    pdf_c_stream *crypt_stream = NULL, *SubFile_stream = NULL;
    pdf_string *StreamKey = NULL;
    pdf_dict *stream_dict = NULL;
    pdf_obj *FileSpec = NULL;
    pdf_stream *NewStream = NULL;
    bool known = false;

    *new_stream = NULL;

    code = pdfi_dict_from_obj(ctx, (pdf_obj *)stream_obj, &stream_dict);
    if (code < 0)
        goto error;

    /* Horrifyingly, any stream dictionary can contain a file specification, which means that
     * instead of using the stream from the PDF file we must use an external file.
     * So much for portability!
     * Note: We must not do this for inline images as an inline image dictionary can
     * contain the abbreviation /F for the Filter, and an inline image is never a
     * separate stream, it is (obviously) contained in the current stream.
     */
    if (!inline_image) {
        code = pdfi_dict_known(ctx, stream_dict, "F", &known);
        if (code >= 0 && known) {
            pdf_obj *FS = NULL, *o = NULL;
            pdf_dict *dict = NULL;
            stream *gstream = NULL;
            char CFileName[gp_file_name_sizeof];

            code = pdfi_dict_get(ctx, stream_dict, "F", &FileSpec);
            if (code < 0)
                goto error;
            if (pdfi_type_of(FileSpec) == PDF_DICT) {
                /* We don't really support FileSpec dictionaries, partly because we
                 * don't really know which platform to use. If there is a /F string
                 * then we will use that, just as if we had been given a string in
                 * the first place.
                 */
                code = pdfi_dict_knownget(ctx, (pdf_dict *)FileSpec, "F", &FS);
                if (code < 0) {
                    goto error;
                }
                pdfi_countdown(FileSpec);
                FileSpec = FS;
                FS = NULL;
            }
            if (pdfi_type_of(FileSpec) != PDF_STRING) {
                code = gs_note_error(gs_error_typecheck);
                goto error;
            }

            if (((pdf_string *)FileSpec)->length + 1 > gp_file_name_sizeof) {
                code = gs_note_error(gs_error_ioerror);
                goto error;
            }
            memcpy(CFileName, ((pdf_string *)FileSpec)->data, ((pdf_string *)FileSpec)->length);
            CFileName[((pdf_string *)FileSpec)->length] = 0x00;

            /* We should now have a string with the filename (or URL). We need
             * to open the file and create a stream, if that succeeds.
             */
            gstream = sfopen((const char *)CFileName, "r", ctx->memory);
            if (gstream == NULL) {
                emprintf1(ctx->memory, "Failed to open file %s\n", CFileName);
                code = gs_note_error(gs_error_ioerror);
                goto error;
            }

            source = (pdf_c_stream *)gs_alloc_bytes(ctx->memory, sizeof(pdf_c_stream), "external stream");
            if (source == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto error;
            }
            memset(source, 0x00, sizeof(pdf_c_stream));
            source->s = gstream;

            code = pdfi_object_alloc(ctx, PDF_STREAM, 0, (pdf_obj **)&NewStream);
            if (code < 0)
                goto error;
            pdfi_countup(NewStream);
            code = pdfi_dict_alloc(ctx, 32, &dict);
            if (code < 0){
                pdfi_countdown(NewStream);
                goto error;
            }
            pdfi_countup(dict);
            NewStream->stream_dict = dict;
            code = pdfi_dict_get(ctx, stream_dict, "FFilter", &o);
            if (code >= 0) {
                code = pdfi_dict_put(ctx, NewStream->stream_dict, "Filter", o);
                if (code < 0) {
                    pdfi_countdown(NewStream);
                    goto error;
                }
            }
            code = pdfi_dict_get(ctx, stream_dict, "FPredictor", &o);
            if (code >= 0) {
                code = pdfi_dict_put(ctx, NewStream->stream_dict, "Predictor", o);
                if (code < 0) {
                    pdfi_countdown(NewStream);
                    goto error;
                }
            }
            pdfi_countup(NewStream->stream_dict);
            NewStream->stream_offset = 0;
            NewStream->Length = 0;
            NewStream->length_valid = 0;
            NewStream->stream_written = 0;
            NewStream->is_marking = 0;
            NewStream->parent_obj = NULL;
            stream_obj = NewStream;
            stream_dict = NewStream->stream_dict;
        }
    }

    /* If the file isn't encrypted, don't apply encryption. If this is an inline
     * image then its in a content stream and will already be decrypted, so don't
     * apply decryption again.
     */
    if (ctx->encryption.is_encrypted && !inline_image) {
        int64_t Length;

        if (ctx->encryption.StmF == CRYPT_IDENTITY)
            return pdfi_filter_no_decryption(ctx, stream_obj, source, new_stream, inline_image);

        code = pdfi_dict_get_type(ctx, stream_dict, "StreamKey", PDF_STRING, (pdf_obj **)&StreamKey);
        if (code == gs_error_undefined) {
            code = pdfi_compute_objkey(ctx, (pdf_obj *)stream_dict, &StreamKey);
            if (code < 0)
                return code;
            code = pdfi_dict_put(ctx, stream_dict, "StreamKey", (pdf_obj *)StreamKey);
            if (code < 0)
                goto error;
        }
        if (code < 0)
            return code;

        /* If we are applying a decryption filter we must also apply a SubFileDecode filter.
         * This is because the underlying stream may not have a compression filter, if it doesn't
         * then we have no way of detecting the end of the data. Normally we would get an 'endstream'
         * token but if we have applied a decryption filter then we'll 'decrypt' that token
         * and that will corrupt it. So make sure we can't read past the end of the stream
         * by applying a SubFileDecode.
         * NB applying a SubFileDecode filter with an EODString seems to limit the amount of data
         * that the decode filter is prepared to return at any time to the size of the EODString.
         * This doesn't play well with other filters (eg the AESDecode filter) which require a
         * larger minimum to be returned (16 bytes for AESDecode). So I'm using the filter
         * Length here, even though I'd prefer not to.....
         */
        Length = pdfi_stream_length(ctx, stream_obj);

        if (Length <= 0) {
            /* Don't treat as an encrypted stream if Length is 0 */
            pdfi_countdown(StreamKey);
            return pdfi_filter_no_decryption(ctx, stream_obj, source, new_stream, inline_image);
        }

        code = pdfi_apply_SubFileDecode_filter(ctx, Length, NULL, source, &SubFile_stream, false);
        if (code < 0)
            goto error;

        SubFile_stream->original = source->s;

        switch(ctx->encryption.StmF) {
            case CRYPT_IDENTITY:
                /* Can't happen, handled above */
                break;
            /* There are only two possible filters, RC4 or AES, we take care
             * of the number of bits in the key by using ctx->Length.
             */
            case CRYPT_V1:
            case CRYPT_V2:
                code = pdfi_apply_Arc4_filter(ctx, StreamKey, SubFile_stream, &crypt_stream);
                break;
            case CRYPT_AESV2:
            case CRYPT_AESV3:
                code = pdfi_apply_AES_filter(ctx, StreamKey, 1, SubFile_stream, &crypt_stream);
                break;
            default:
                code = gs_error_rangecheck;
        }
        if (code < 0) {
            pdfi_close_file(ctx, SubFile_stream);
            goto error;
        }

        crypt_stream->original = SubFile_stream->original;
        gs_free_object(ctx->memory, SubFile_stream, "pdfi_filter");

        code = pdfi_filter_no_decryption(ctx, stream_obj, crypt_stream, new_stream, false);
        if (code < 0) {
            pdfi_close_file(ctx, crypt_stream);
            goto error;
        }

        (*new_stream)->original = source->s;
        gs_free_object(ctx->memory, crypt_stream, "pdfi_filter");
    } else {
        code = pdfi_filter_no_decryption(ctx, stream_obj, source, new_stream, inline_image);
    }
error:
    pdfi_countdown(NewStream);
    pdfi_countdown(StreamKey);
    pdfi_countdown(FileSpec);
    return code;
}

/* This is just a convenience routine. We could use pdfi_filter() above, but because PDF
 * doesn't support the SubFileDecode filter that would mean callers having to manufacture
 * a dictionary in order to use it. That's excessively convoluted, so just supply a simple
 * means to instantiate a SubFileDecode filter.
 *
 * NB! The EODString can't be tracked by the stream code. The caller is responsible for
 * managing the lifetime of this object. It must remain valid until the filter is closed.
 */
int pdfi_apply_SubFileDecode_filter(pdf_context *ctx, int EODCount, const char *EODString, pdf_c_stream *source, pdf_c_stream **new_stream, bool inline_image)
{
    int code;
    stream_SFD_state state;
    stream *new_s = NULL;
    int min_size = 2048;

    *new_stream = NULL;

    memset(&state, 0, sizeof(state));

    if (s_SFD_template.set_defaults)
        s_SFD_template.set_defaults((stream_state *)&state);

    if (EODString != NULL) {
        state.eod.data = (const byte *)EODString;
        state.eod.size = strlen(EODString);
    }

    if (EODCount > 0)
        state.count = EODCount - source->unread_size;
    else
        state.count = EODCount;

    code = pdfi_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_SFD_template, (const stream_state *)&state, ctx->memory->non_gc_memory, &new_s);
    if (code < 0)
        return code;

    code = pdfi_alloc_stream(ctx, new_s, source->s, new_stream);
    if (code < 0) {
        gs_free_object(ctx->memory->non_gc_memory, new_s->state, "pdfi_apply_SubFileDecode_filter");
        gs_free_object(ctx->memory->non_gc_memory, new_s->cbuf, "pdfi_apply_SubFileDecode_filter");
        gs_free_object(ctx->memory->non_gc_memory, new_s, "pdfi_apply_SubFileDecode_filter");
        return code;
    }
    new_s->strm = source->s;
    if (source->unread_size != 0) {
        (*new_stream)->unread_size = source->unread_size;
        memcpy((*new_stream)->unget_buffer, source->unget_buffer, source->unread_size);
        source->unread_size = 0;
    }
    return code;
}

/* We would really like to use a ReusableStreamDecode filter here, but that filter is defined
 * purely in the PostScript interpreter. So instead we make a temporary stream from a
 * memory buffer. Its icky (we can end up with the same data in memory multiple times)
 * but it works, and is used elsewhere in Ghostscript.
 * If retain_ownership is true then the calling function is responsible for buffer pointer lifetime.
 * Otherwise the buffer will be freed when the stream is closed.
 */
int pdfi_open_memory_stream_from_stream(pdf_context *ctx, unsigned int size, byte **Buffer, pdf_c_stream *source, pdf_c_stream **new_pdf_stream, bool retain_ownership)
{
    stream *new_stream;
    int code;

    new_stream = file_alloc_stream(ctx->memory, "open memory stream(stream)");
    if (new_stream == NULL)
        return_error(gs_error_VMerror);

    *Buffer = gs_alloc_bytes(ctx->memory, size, "open memory stream (buffer)");
    if (*Buffer == NULL) {
        gs_free_object(ctx->memory, new_stream, "open memory stream(stream)");
        return_error(gs_error_VMerror);
    }
    code = pdfi_read_bytes(ctx, *Buffer, 1, size, source);
    if (code < 0) {
        gs_free_object(ctx->memory, *Buffer, "open memory stream(buffer)");
        gs_free_object(ctx->memory, new_stream, "open memory stream(stream)");
        return code;
    }

    if (retain_ownership)
        sread_string_reusable(new_stream, *Buffer, size);
    else
        sread_transient_string_reusable(new_stream, ctx->memory, *Buffer, size);

    code = pdfi_alloc_stream(ctx, new_stream, source->s, new_pdf_stream);
    if (code < 0) {
        sclose(new_stream);
        gs_free_object(ctx->memory, *Buffer, "open memory stream(buffer)");
        gs_free_object(ctx->memory, new_stream, "open memory stream(stream)");
    }

    return code;
}

/*
 * Like pdfi_open_memory_stream_from_stream (and makes use of it) this is a way to read from a stream into
 * memory, and return a stream which reads from that memory. The difference is that this function takes
 * any filters into account, decompressing them. We could layer a decompression stream onto the memory
 * stream returned by open_memory_stream_from_stream above instead.
 *
 * This function returns < 0 for an error, and the length of the uncompressed data on success.
 */
int pdfi_open_memory_stream_from_filtered_stream(pdf_context *ctx, pdf_stream *stream_obj,
                                                 byte **Buffer, pdf_c_stream **new_pdf_stream, bool retain_ownership)
{
    int code;
    int64_t bufferlen = 0;

    code = pdfi_stream_to_buffer(ctx, stream_obj, Buffer, &bufferlen);
    if (code < 0) {
        *Buffer = NULL;
        *new_pdf_stream = NULL;
        return code;
    }
    code = pdfi_open_memory_stream_from_memory(ctx, (unsigned int)bufferlen, *Buffer, new_pdf_stream, retain_ownership);
    if (code < 0) {
        gs_free_object(ctx->memory, *Buffer, "pdfi_open_memory_stream_from_filtered_stream");
        *Buffer = NULL;
        *new_pdf_stream = NULL;
    }
    return (int)bufferlen;
}

int pdfi_open_memory_stream_from_memory(pdf_context *ctx, unsigned int size, byte *Buffer, pdf_c_stream **new_pdf_stream, bool retain_ownership)
{
    int code;
    stream *new_stream;

    new_stream = file_alloc_stream(ctx->memory, "open memory stream from memory(stream)");
    if (new_stream == NULL)
        return_error(gs_error_VMerror);
    new_stream->close_at_eod = false;
    if (retain_ownership)
        sread_string(new_stream, Buffer, size);
    else
        sread_transient_string(new_stream, ctx->memory, Buffer, size);

    code = pdfi_alloc_stream(ctx, new_stream, NULL, new_pdf_stream);
    if (code < 0) {
        sclose(new_stream);
        gs_free_object(ctx->memory, new_stream, "open memory stream from memory(stream)");
    }

    return code;
}

int pdfi_close_memory_stream(pdf_context *ctx, byte *Buffer, pdf_c_stream *source)
{
    gs_free_object(ctx->memory, Buffer, "open memory stream(buffer)");
    if (source != NULL) {
        if (source->s != NULL) {
            sclose(source->s);
            gs_free_object(ctx->memory, source->s, "open memory stream(stream)");
        }
        gs_free_object(ctx->memory, source, "open memory stream(pdf_stream)");
    }
    return 0;
}

/***********************************************************************************/
/* Basic 'file' operations. Because of the need to 'unread' bytes we need our own  */

static void pdfi_close_filter_chain(pdf_context *ctx, stream *s, stream *target)
{
    stream *next_s = s;

    while(next_s && next_s != target){
        stream *curr_s = next_s;
        next_s = next_s->strm;
        if (curr_s != ctx->main_stream->s)
            sfclose(curr_s);
    }
}

void pdfi_close_file(pdf_context *ctx, pdf_c_stream *s)
{
    pdfi_close_filter_chain(ctx, s->s, s->original);

    gs_free_object(ctx->memory, s, "closing pdf_file");
}

int pdfi_seek(pdf_context *ctx, pdf_c_stream *s, gs_offset_t offset, uint32_t origin)
{
    int code = 0;

    if (origin == SEEK_CUR && s->unread_size != 0)
        offset -= s->unread_size;

    s->unread_size = 0;;

    code = sfseek(s->s, offset, origin);
    if (s->eof && code >= 0)
        s->eof = 0;

    return code;
}

/* We use 'stell' sometimes to save the position of the underlying file
 * when reading a compressed stream, so that we can return to the same
 * point in the underlying file after performing some other operation. This
 * allows us (for instance) to load a font while interpreting a content stream.
 * However, if we've 'unread' any bytes we need to take that into account.
 * NOTE! this is only going to be valid when performed on the main stream
 * the original PDF file, not any compressed stream!
 */
gs_offset_t pdfi_unread_tell(pdf_context *ctx)
{
    gs_offset_t off = stell(ctx->main_stream->s);

    return (off - ctx->main_stream->unread_size);
}

gs_offset_t pdfi_tell(pdf_c_stream *s)
{
    return stell(s->s);
}

int pdfi_unread_byte(pdf_context *ctx, pdf_c_stream *s, char c)
{
    if (s->unread_size == UNREAD_BUFFER_SIZE)
        return_error(gs_error_ioerror);

    s->unget_buffer[s->unread_size++] = c;

    return 0;
}

int pdfi_unread(pdf_context *ctx, pdf_c_stream *s, byte *Buffer, uint32_t size)
{
    if (size + s->unread_size > UNREAD_BUFFER_SIZE)
        return_error(gs_error_ioerror);

    Buffer += size;
    while (size) {
        s->unget_buffer[s->unread_size++] = *--Buffer;
        size--;
    }

    return 0;
}

int pdfi_read_byte(pdf_context *ctx, pdf_c_stream *s)
{
    int32_t code;

    if (s->eof && s->unread_size == 0)
        return EOFC;

    if (s->unread_size)
        return (byte)s->unget_buffer[--s->unread_size];

    /* TODO the Ghostscript code uses sbufptr(s) to avoid a memcpy
     * at some point we should modify this code to do so as well.
     */
    code = spgetc(s->s);
    if (code == EOFC) {
        s->eof = true;
        return EOFC;
    } else if (code == gs_error_ioerror) {
        pdfi_set_error(ctx, code, "sgets", E_PDF_BADSTREAM, "pdfi_read_bytes", NULL);
        s->eof = true;
        return EOFC;
    } else if(code == ERRC) {
        return ERRC;
    }
    return (int)code;
}


int pdfi_read_bytes(pdf_context *ctx, byte *Buffer, uint32_t size, uint32_t count, pdf_c_stream *s)
{
    uint32_t i = 0, total = size * count;
    uint32_t bytes = 0;
    int32_t code;

    if (s->eof && s->unread_size == 0)
        return 0;

    if (s->unread_size) {
        i = s->unread_size;
        if (i >= total)
            i = total;
        bytes = i;
        while (bytes) {
            *Buffer++ = s->unget_buffer[--s->unread_size];
            bytes--;
        }
        total -= i;
        if (total == 0 || s->eof)
            return i;
    }

    /* TODO the Ghostscript code uses sbufptr(s) to avoid a memcpy
     * at some point we should modify this code to do so as well.
     */
    code = sgets(s->s, Buffer, total, &bytes);
    if (code == EOFC) {
        s->eof = true;
    } else if (code == gs_error_ioerror) {
        pdfi_set_error(ctx, code, "sgets", E_PDF_BADSTREAM, "pdfi_read_bytes", NULL);
        s->eof = true;
    } else if(code == ERRC) {
        bytes = ERRC;
    } else {
        bytes = bytes + i;
    }

    return bytes;
}

/* Read bytes from stream object into buffer.
 * Handles both plain streams and filtered streams.
 * Buffer gets allocated here, and must be freed by caller.
 * Preserves the location of the current stream file position.
 */
int
pdfi_stream_to_buffer(pdf_context *ctx, pdf_stream *stream_obj, byte **buf, int64_t *bufferlen)
{
    byte *Buffer = NULL;
    int code = 0;
    uint read = 0, buflen = 0;
    int64_t ToRead = *bufferlen;
    gs_offset_t savedoffset;
    pdf_c_stream *stream = NULL, *SubFileStream = NULL;
    bool filtered;
    pdf_dict *stream_dict = NULL;

    savedoffset = pdfi_tell(ctx->main_stream);

    pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, stream_obj), SEEK_SET);

    code = pdfi_dict_from_obj(ctx, (pdf_obj *)stream_obj, &stream_dict);
    if (code < 0)
        goto exit;

    /* See if this is a filtered stream */
    code = pdfi_dict_known(ctx, stream_dict, "Filter", &filtered);
    if (code < 0)
        goto exit;

    if (!filtered) {
        code = pdfi_dict_known(ctx, stream_dict, "F", &filtered);
        if (code < 0)
            goto exit;
    }

retry:
    if (ToRead == 0) {
        if (filtered || ctx->encryption.is_encrypted) {
            code = pdfi_apply_SubFileDecode_filter(ctx, 0, "endstream", ctx->main_stream, &SubFileStream, false);
            if (code < 0)
                goto exit;
            code = pdfi_filter(ctx, stream_obj, SubFileStream, &stream, false);
            if (code < 0) {
                pdfi_close_file(ctx, SubFileStream);
                goto exit;
            }
            while (seofp(stream->s) != true && serrorp(stream->s) != true) {
                s_process_read_buf(stream->s);
                buflen += sbufavailable(stream->s);
                (void)sbufskip(stream->s, sbufavailable(stream->s));
            }
            pdfi_close_file(ctx, stream);
            pdfi_close_file(ctx, SubFileStream);
        } else {
            buflen = pdfi_stream_length(ctx, stream_obj);
        }
    } else
        buflen = *bufferlen;

    /* Alloc buffer */
    Buffer = gs_alloc_bytes(ctx->memory, buflen, "pdfi_stream_to_buffer (Buffer)");
    if (!Buffer) {
        code = gs_note_error(gs_error_VMerror);
        goto exit;
    }

    code = pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, stream_obj), SEEK_SET);
    if (code < 0) {
        buflen = 0;
        goto exit;
    }
    if (filtered || ctx->encryption.is_encrypted) {
        if (ToRead && stream_obj->length_valid)
            code = pdfi_apply_SubFileDecode_filter(ctx, stream_obj->Length, NULL, ctx->main_stream, &SubFileStream, false);
        else
            code = pdfi_apply_SubFileDecode_filter(ctx, 0, "endstream", ctx->main_stream, &SubFileStream, false);
        if (code < 0)
            goto exit;

        code = pdfi_filter(ctx, stream_obj, SubFileStream, &stream, false);
        if (code < 0) {
            /* Because we opened the SubFileDecode separately to the filter chain, we need to close it separately too */
            pdfi_close_file(ctx, SubFileStream);
            goto exit;
        }

        code = sgets(stream->s, Buffer, buflen, (unsigned int *)&read);
        if (read < buflen) {
            memset(Buffer + read, 0x00, buflen - read);
        }

        pdfi_close_file(ctx, stream);
        /* Because we opened the SubFileDecode separately to the filter chain, we need to close it separately too */
        pdfi_close_file(ctx, SubFileStream);
        if (code == ERRC || code == EOFC) {
            code = 0;
            /* Error reading the expected number of bytes. If we already calculated the number of
             * bytes in the loop above, then ignore the error and carry on. If, however, we were
             * told how many bytes to expect, and failed to read that many, go back and do this
             * the slow way to determine how many bytes are *really* available.
             */
            if(ToRead != 0) {
                buflen = ToRead = 0;
                code = pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, stream_obj), SEEK_SET);
                if (code < 0)
                    goto exit;
                gs_free_object(ctx->memory, Buffer, "pdfi_stream_to_buffer (Buffer)");
                goto retry;
            }
        }
    } else {
        if (ToRead && stream_obj->length_valid)
            code = pdfi_apply_SubFileDecode_filter(ctx, stream_obj->Length, NULL, ctx->main_stream, &SubFileStream, false);
        else
            code = pdfi_apply_SubFileDecode_filter(ctx, ToRead, "endstream", ctx->main_stream, &SubFileStream, false);
        if (code < 0)
            goto exit;

        code = sgets(SubFileStream->s, Buffer, buflen, (unsigned int *)&read);
        if (read < buflen) {
            memset(Buffer + read, 0x00, buflen - read);
        }

        pdfi_close_file(ctx, SubFileStream);
        if (code == ERRC || code == EOFC) {
            code = 0;
            /* Error reading the expected number of bytes. If we already calculated the number of
             * bytes in the loop above, then ignore the error and carry on. If, however, we were
             * told how many bytes to expect, and failed to read that many, go back and do this
             * the slow way to determine how many bytes are *really* available.
             */
            buflen = ToRead = 0;
            /* Setting filtered to true is a lie, but it forces the code to go through the slow path and check the *real* number of bytes
             * in the stream. This will be slow, but it should only happen when we get a file which is invalid.
             */
            filtered = 1;
            code = pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, stream_obj), SEEK_SET);
            if (code < 0)
                goto exit;
            gs_free_object(ctx->memory, Buffer, "pdfi_stream_to_buffer (Buffer)");
            goto retry;
        }
    }

 exit:
    pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
    if (Buffer && code < 0)
        gs_free_object(ctx->memory, Buffer, "pdfi_stream_to_buffer (Buffer)");
    else
        *buf = Buffer;
    *bufferlen = buflen;
    return code;
}

static int pdfi_open_resource_file_inner(pdf_context *ctx, const char *fname, const int fnamelen, stream **s)
{
    int code = 0;
    if (fname == NULL || fnamelen == 0 || fnamelen >= gp_file_name_sizeof)
        *s = NULL;
    else if (gp_file_name_is_absolute(fname, fnamelen) || fname[0] == '%') {
        char CFileName[gp_file_name_sizeof];

        if (fnamelen + 1 > gp_file_name_sizeof)
            return_error(gs_error_ioerror);
        memcpy(CFileName, fname, fnamelen);
        CFileName[fnamelen] = 0x00;
        /* If it's an absolute path or an explicit PS style device, just try to open it */
        *s = sfopen(CFileName, "r", ctx->memory);
    }
    else {
        char fnametotry[gp_file_name_sizeof];
        uint fnlen;
        gs_parsed_file_name_t pname;
        gp_file_name_combine_result r;
        int i, total;

        *s = NULL;
        i = 0;
        total = ctx->search_paths.num_resource_paths - ctx->search_paths.num_init_resource_paths - 1;
retry:
        for (; i < total; i++) {
            gs_param_string *ss = &ctx->search_paths.resource_paths[i];

            if (ss->data[0] == '%') {
                code = gs_parse_file_name(&pname, (char *)ss->data, ss->size, ctx->memory);
                if (code < 0 || (pname.len + fnamelen >= gp_file_name_sizeof)) {
                    continue;
                }
                memcpy(fnametotry, pname.fname, pname.len);
                memcpy(fnametotry + pname.len, fname, fnamelen);
                code = pname.iodev->procs.open_file(pname.iodev, fnametotry, pname.len + fnamelen, "r", s, ctx->memory);
                if (code < 0) {
                    continue;
                }
                break;
            }
            else {
                fnlen = gp_file_name_sizeof;
                r = gp_file_name_combine((char *)ss->data, ss->size, fname, fnamelen, false, fnametotry, &fnlen);
                if (r != gp_combine_success || fnlen > gp_file_name_sizeof - 1)
                    continue;
                fnametotry[fnlen] = '\0';
                *s = sfopen(fnametotry, "r", ctx->memory);
                if (*s != NULL)
                    break;
            }
        }
        if (*s == NULL && i < ctx->search_paths.num_resource_paths) {
            gs_param_string *ss = &ctx->search_paths.genericresourcedir;
            fnlen = gp_file_name_sizeof;
            r = gp_file_name_combine((char *)ss->data, ss->size, fname, fnamelen, false, fnametotry, &fnlen);
            if (r == gp_combine_success || fnlen < gp_file_name_sizeof) {
                fnametotry[fnlen] = '\0';
                *s = sfopen(fnametotry, "r", ctx->memory);
            }
        }
        if (*s == NULL && i < ctx->search_paths.num_resource_paths) {
            total = ctx->search_paths.num_resource_paths;
            goto retry;
        }
    }
    if (*s == NULL)
        return_error(gs_error_invalidfileaccess);

    return 0;
}

int pdfi_open_resource_file(pdf_context *ctx, const char *fname, const int fnamelen, stream **s)
{
    return pdfi_open_resource_file_inner(ctx, fname, fnamelen, s);
}

bool pdfi_resource_file_exists(pdf_context *ctx, const char *fname, const int fnamelen)
{
    stream *s = NULL;
    int code = pdfi_open_resource_file_inner(ctx, fname, fnamelen, &s);
    if (s)
        sfclose(s);

    return (code >= 0);
}

static int pdfi_open_font_file_inner(pdf_context *ctx, const char *fname, const int fnamelen, stream **s)
{
    int code = 0;
    const char *fontdirstr = "Font/";
    const int fontdirstrlen = strlen(fontdirstr);

    if (fname == NULL || fnamelen == 0 || fnamelen >= (gp_file_name_sizeof - fontdirstrlen))
        *s = NULL;
    else if (gp_file_name_is_absolute(fname, fnamelen) || fname[0] == '%') {
        char CFileName[gp_file_name_sizeof];

        if (fnamelen + 1 > gp_file_name_sizeof)
            return_error(gs_error_ioerror);
        memcpy(CFileName, fname, fnamelen);
        CFileName[fnamelen] = 0x00;
        /* If it's an absolute path or an explicit PS style device, just try to open it */
        *s = sfopen(CFileName, "r", ctx->memory);
    }
    else {
        char fnametotry[gp_file_name_sizeof];
        uint fnlen;
        gs_parsed_file_name_t pname;
        gp_file_name_combine_result r;
        int i;

        *s = NULL;
        for (i = 0; i < ctx->search_paths.num_font_paths; i++) {
            gs_param_string *ss = &ctx->search_paths.font_paths[i];

            if (ss->data[0] == '%') {
                code = gs_parse_file_name(&pname, (char *)ss->data, ss->size, ctx->memory);
                if (code < 0 || (pname.len + fnamelen >= gp_file_name_sizeof)) {
                    continue;
                }
                memcpy(fnametotry, pname.fname, pname.len);
                memcpy(fnametotry + pname.len, fname, fnamelen);
                code = pname.iodev->procs.open_file(pname.iodev, fnametotry, pname.len + fnamelen, "r", s, ctx->memory);
                if (code < 0) {
                    continue;
                }
                break;
            }
            else {
                fnlen = gp_file_name_sizeof;
                r = gp_file_name_combine((char *)ss->data, ss->size, fname, fnamelen, false, fnametotry, &fnlen);
                if (r != gp_combine_success || fnlen > gp_file_name_sizeof - 1)
                    continue;
                fnametotry[fnlen] = '\0';
                *s = sfopen(fnametotry, "r", ctx->memory);
                if (*s != NULL)
                    break;
            }
        }
        if (*s == NULL && i < ctx->search_paths.num_resource_paths) {
            gs_param_string *ss = &ctx->search_paths.genericresourcedir;
            char fstr[gp_file_name_sizeof];

            fnlen = gp_file_name_sizeof;

            memcpy(fstr, fontdirstr, fontdirstrlen);
            memcpy(fstr + fontdirstrlen, fname, fnamelen);

            r = gp_file_name_combine((char *)ss->data, ss->size, fstr, fontdirstrlen + fnamelen, false, fnametotry, &fnlen);
            if (r == gp_combine_success || fnlen < gp_file_name_sizeof) {
                fnametotry[fnlen] = '\0';
                *s = sfopen(fnametotry, "r", ctx->memory);
            }
        }
    }
    if (*s == NULL)
        return pdfi_open_resource_file_inner(ctx, fname, fnamelen, s);

    return 0;
}

int pdfi_open_font_file(pdf_context *ctx, const char *fname, const int fnamelen, stream **s)
{
    return pdfi_open_font_file_inner(ctx, fname, fnamelen, s);
}

bool pdfi_font_file_exists(pdf_context *ctx, const char *fname, const int fnamelen)
{
    stream *s = NULL;
    int code = pdfi_open_font_file_inner(ctx, fname, fnamelen, &s);
    if (s)
        sfclose(s);

    return (code >= 0);
}
