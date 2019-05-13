/* Copyright (C) 2001-2018 Artifex Software, Inc.
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

#include "ghostpdf.h"
#include "pdf_types.h"
#include "pdf_stack.h"
#include "pdf_dict.h"
#include "pdf_file.h"
#include "pdf_int.h"
#include "pdf_array.h"
#include "stream.h"
#include "strimpl.h"
#include "strmio.h"
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

/* Utility routine to create a pdf_stream object */
static int pdfi_alloc_stream(pdf_context *ctx, stream *source, stream *original, pdf_stream **new_stream)
{
    *new_stream = NULL;
    *new_stream = (pdf_stream *)gs_alloc_bytes(ctx->memory, sizeof(pdf_stream), "pdfi_alloc_stream");
    if (*new_stream == NULL)
        return_error(gs_error_VMerror);
    memset(*new_stream, 0x00, sizeof(pdf_stream));
    (*new_stream)->eof = false;
    ((pdf_stream *)(*new_stream))->s = source;
    ((pdf_stream *)(*new_stream))->original = original;
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
            pdfi_filter_open(min_size, &s_filter_read_procs,
                             (const stream_template *)&s_PDiffD_template,
                             (const stream_state *)&ppds, ctx->memory->non_gc_memory, new_stream);
            (*new_stream)->strm = source;
            break;
        default:
            /* zpp_setup, PNG predictor */
            pps.Colors = (int)Colors;
            pps.BitsPerComponent = (int)BPC;
            pps.Columns = (uint)Columns;
            pps.Predictor = Predictor;
            pdfi_filter_open(min_size, &s_filter_read_procs,
                             (const stream_template *)&s_PNGPD_template,
                             (const stream_state *)&pps, ctx->memory->non_gc_memory, new_stream);
            (*new_stream)->strm = source;
            break;
    }
    return 0;
}

static int pdfi_Arc4_filter(pdf_context *ctx, char *Key, stream *source, stream **new_stream)
{
    stream_arcfour_state state;

    uint min_size = 2048;
    int code;

    s_arcfour_set_key(&state, (const unsigned char *)Key, strlen(Key));

    code = pdfi_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_arcfour_template, (const stream_state *)&state, ctx->memory->non_gc_memory, new_stream);

    if (code < 0)
        return code;

    (*new_stream)->strm = source;
    source = *new_stream;

    return 0;
}

static int pdfi_AES_filter(pdf_context *ctx, char *Key, bool use_padding, stream *source, stream **new_stream)
{
    stream_aes_state state;

    uint min_size = 2048;
    int code;

    s_aes_set_key(&state, (const unsigned char *)Key, strlen(Key));
    s_aes_set_padding(&state, use_padding);

    code = pdfi_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_aes_template, (const stream_state *)&state, ctx->memory->non_gc_memory, new_stream);

    if (code < 0)
        return code;

    (*new_stream)->strm = source;
    source = *new_stream;

    return 0;
}

static int pdfi_Flate_filter(pdf_context *ctx, pdf_dict *d, stream *source, stream **new_stream)
{
    stream_zlib_state zls;
    uint min_size = 2048;
    int code;

    memset(&zls, 0, sizeof(zls));

    /* s_zlibD_template defined in base/szlibd.c */
    (*s_zlibD_template.set_defaults)((stream_state *)&zls);

    code = pdfi_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_zlibD_template, (const stream_state *)&zls, ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        return code;

    (*new_stream)->strm = source;
    source = *new_stream;

    if (d && d->type == PDF_DICT)
        pdfi_Predictor_filter(ctx, d, source, new_stream);
    return 0;
}

static int
pdfi_JBIG2Decode_filter(pdf_context *ctx, pdf_dict *dict, pdf_dict *decode,
                        stream *source, stream **new_stream)
{
    stream_jbig2decode_state state;
    uint min_size = s_jbig2decode_template.min_out_size;
    int code;
    pdf_dict *Globals = NULL;
    byte *buf;
    int64_t buflen;
    void *globalctx;

    s_jbig2decode_set_global_data((stream_state*)&state, NULL, NULL);

    if (decode) {
        code = pdfi_dict_knownget_type(ctx, decode, "JBIG2Globals", PDF_DICT, (pdf_obj **)&Globals);
        if (code < 0) {
            goto cleanupExit;
        }

        /* read in the globals from stream */
        if (code > 0) {
            code = pdfi_stream_to_buffer(ctx, Globals, &buf, &buflen);
            if (code == 0) {
                code = s_jbig2decode_make_global_data(ctx->memory->non_gc_memory,
                                                      buf, buflen, &globalctx);

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

    if (d && d->type == PDF_DICT) {
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

    if (d && d->type == PDF_DICT)
        pdfi_Predictor_filter(ctx, d, source, new_stream);
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

    state.memory = ctx->memory->non_gc_memory;
    if (s_jpxd_template.set_defaults)
      (*s_jpxd_template.set_defaults)((stream_state *)&state);

    /* Pull some extra params out of the image dict */
    /* TODO: Alpha -- this seems to be a hack that PS implementation did, need to get equiv */
    if (dict && pdfi_dict_get(ctx, dict, "ColorSpace", &csobj) == 0) {
        /* parse the value */
        if (csobj->type == PDF_ARRAY) {
            /* assume it's the first array element */
            code = pdfi_array_get(ctx, (pdf_array *)csobj, (uint64_t)0, (pdf_obj **)&csname);
            if (code < 0) {
                pdfi_countdown(csobj);
                return code;
            }
        } else if (csobj->type == PDF_NAME) {
            /* use the name directly */
            csname = (pdf_name *)csobj;
            csobj = NULL; /* To keep ref counting straight */
        } else {
            dmprintf(ctx->memory, "warning: JPX ColorSpace value is an unhandled type!\n");
        }
        if (csname != NULL && csname->type == PDF_NAME) {
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

static int pdfi_DCT_filter(pdf_context *ctx, pdf_dict *d, stream *source, stream **new_stream)
{
    stream_DCT_state dcts;
    uint min_size = s_DCTD_template.min_out_size;
    int code;
    int64_t i;
    jpeg_decompress_data *jddp;
    gx_device *dev = gs_currentdevice_inline(ctx->pgs);

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

    if (d && d->type == PDF_DICT) {
        code = pdfi_dict_get_int(ctx, d, "ColorTransform", &i);
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

    if (d && d->type == PDF_DICT) {
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

    if (ctx->pdfdebug)
    {
        char str[100];
        memcpy(str, (const char *)n->data, n->length);
        str[n->length] = '\0';
        dmprintf1(ctx->memory, "FILTER NAME:%s\n", str);
    }

    if (pdfi_name_is(n, "RunLengthDecode")) {
        code = pdfi_simple_filter(ctx, &s_RLD_template, source, new_stream);
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
        code = pdfi_DCT_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "JPXDecode")) {
        code = pdfi_JPX_filter(ctx, dict, decode, source, new_stream);
        return code;
    }

    if (pdfi_name_is(n, "AHx")) {
        if (!inline_image) {
            ctx->pdf_warnings|= W_PDF_BAD_INLINEFILTER;
            if (ctx->pdfstoponwarning)
                return_error(gs_error_syntaxerror);
        }
        code = pdfi_simple_filter(ctx, &s_AXD_template, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "A85")) {
        if (!inline_image) {
            ctx->pdf_warnings|= W_PDF_BAD_INLINEFILTER;
            if (ctx->pdfstoponwarning)
                return_error(gs_error_syntaxerror);
        }
        code = pdfi_ASCII85_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "LZW")) {
        if (!inline_image) {
            ctx->pdf_warnings|= W_PDF_BAD_INLINEFILTER;
            if (ctx->pdfstoponwarning)
                return_error(gs_error_syntaxerror);
        }
        code = pdfi_LZW_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "CCF")) {
        if (!inline_image) {
            ctx->pdf_warnings|= W_PDF_BAD_INLINEFILTER;
            if (ctx->pdfstoponwarning)
                return_error(gs_error_syntaxerror);
        }
        code = pdfi_CCITTFax_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "DCT")) {
        if (!inline_image) {
            ctx->pdf_warnings|= W_PDF_BAD_INLINEFILTER;
            if (ctx->pdfstoponwarning)
                return_error(gs_error_syntaxerror);
        }
        code = pdfi_DCT_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "Fl")) {
        if (!inline_image) {
            ctx->pdf_warnings|= W_PDF_BAD_INLINEFILTER;
            if (ctx->pdfstoponwarning)
                return_error(gs_error_syntaxerror);
        }
        code = pdfi_Flate_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (pdfi_name_is(n, "RL")) {
        if (!inline_image) {
            ctx->pdf_warnings|= W_PDF_BAD_INLINEFILTER;
            if (ctx->pdfstoponwarning)
                return_error(gs_error_syntaxerror);
        }
        code = pdfi_simple_filter(ctx, &s_RLD_template, source, new_stream);
        return code;
    }

    ctx->pdf_errors |= E_PDF_UNKNOWNFILTER;
    return_error(gs_error_undefined);
}

int pdfi_filter(pdf_context *ctx, pdf_dict *dict, pdf_stream *source, pdf_stream **new_stream, bool inline_image)
{
    pdf_obj *o = NULL, *decode = NULL, *o1 = NULL;
    int code;
    int64_t i, j, duplicates;
    stream *s = source->s, *new_s = NULL;

    *new_stream = NULL;

    if (ctx->pdfdebug)
        dmprintf2(ctx->memory, "Filter: offset %ld(0x%lx)\n", dict->stream_offset, dict->stream_offset);

    code = pdfi_dict_get(ctx, dict, "Filter", &o);
    if (code < 0){
        if (code == gs_error_undefined) {
            if (inline_image == true) {
                code = pdfi_dict_get(ctx, dict, "F", &o);
                if (code < 0 && code != gs_error_undefined)
                    return code;
            }
            if (code < 0) {
                code = pdfi_alloc_stream(ctx, s, source->s, new_stream);
                return code;
            }
        } else
            return code;
    }

    if (o->type != PDF_NAME) {
        if (o->type == PDF_ARRAY) {
            pdf_array *filter_array = (pdf_array *)o;
            pdf_array *decodeparams_array = NULL;

            code = pdfi_dict_get(ctx, dict, "DecodeParms", &o);
            if (code < 0 && code) {
                if (code == gs_error_undefined) {
                    if (inline_image == true) {
                        code = pdfi_dict_get(ctx, dict, "DP", &o);
                        if (code < 0 && code != gs_error_undefined) {
                            pdfi_countdown(o);
                            pdfi_countdown(filter_array);
                            return code;
                        }
                    }
                } else {
                    pdfi_countdown(o);
                    pdfi_countdown(filter_array);
                    return code;
                }
            }

            if (code != gs_error_undefined) {
                decodeparams_array = (pdf_array *)o;
                if (decodeparams_array->type != PDF_ARRAY) {
                    pdfi_countdown(decodeparams_array);
                    pdfi_countdown(filter_array);
                    return_error(gs_error_typecheck);
                }
                if (pdfi_array_size(decodeparams_array) != pdfi_array_size(filter_array)) {
                    pdfi_countdown(decodeparams_array);
                    pdfi_countdown(filter_array);
                    return_error(gs_error_rangecheck);
                }
            }

            /* Check the Filter array to see if we have any duplicates (to prevent filter bombs)
             * For now we will allow one duplicate (in case people do stupid things like ASCIIEncode
             * and Flate and ASCIIEncode again or something).
             */
            for (i = 0; i < pdfi_array_size(filter_array) - 1;i++) {
                code = pdfi_array_get_type(ctx, filter_array, i, PDF_NAME, &o);
                if (code < 0) {
                    pdfi_countdown(decodeparams_array);
                    pdfi_countdown(filter_array);
                    return code;
                }
                duplicates = 0;

                for (j = i + 1; j < pdfi_array_size(filter_array);j++) {
                    code = pdfi_array_get_type(ctx, filter_array, j, PDF_NAME, &o1);
                    if (code < 0) {
                        pdfi_countdown(o);
                        pdfi_countdown(decodeparams_array);
                        pdfi_countdown(filter_array);
                        return code;
                    }
                    if (((pdf_name *)o)->length == ((pdf_name *)o1)->length) {
                        if (memcmp(((pdf_name *)o)->data, ((pdf_name *)o1)->data, ((pdf_name *)o)->length) == 0)
                            duplicates++;
                    }
                    pdfi_countdown(o1);
                }
                pdfi_countdown(o);
                if (duplicates > 2) {
                    pdfi_countdown(decodeparams_array);
                    pdfi_countdown(filter_array);
                    return_error(gs_error_syntaxerror);
                }
            }

            for (i = 0; i < pdfi_array_size(filter_array);i++) {
                code = pdfi_array_get_type(ctx, filter_array, i, PDF_NAME, &o);
                if (code < 0) {
                    pdfi_countdown(decodeparams_array);
                    pdfi_countdown(filter_array);
                    return code;
                }
                if (decodeparams_array != NULL) {
                    code = pdfi_array_get(ctx, decodeparams_array, i, &decode);
                    if (code < 0) {
                        pdfi_countdown(decodeparams_array);
                        pdfi_countdown(filter_array);
                        return code;
                    }
                }
                if (decode && decode->type != PDF_NULL && decode->type != PDF_DICT) {
                    pdfi_countdown(decodeparams_array);
                    pdfi_countdown(filter_array);
                    return_error(gs_error_typecheck);
                }

                code = pdfi_apply_filter(ctx, dict, (pdf_name *)o,
                                         (pdf_dict *)decode, s, &new_s, inline_image);
                pdfi_countdown(o);
                if (code < 0) {
                    *new_stream = 0;
                    pdfi_countdown(decodeparams_array);
                    pdfi_countdown(filter_array);
                    return code;
                }
                s = new_s;
            }
            pdfi_countdown(decodeparams_array);
            pdfi_countdown(filter_array);
            code = pdfi_alloc_stream(ctx, s, source->s, new_stream);
        } else
            return_error(gs_error_typecheck);
    } else {
        code = pdfi_dict_get(ctx, dict, "DecodeParms", &decode);
        if (code < 0 && code) {
            if (code == gs_error_undefined) {
                if (inline_image == true) {
                    code = pdfi_dict_get(ctx, dict, "DP", &decode);
                    if (code < 0 && code != gs_error_undefined) {
                        pdfi_countdown(o);
                        return code;
                    }
                }
            } else {
                pdfi_countdown(o);
                return code;
            }
        }

        code = pdfi_apply_filter(ctx, dict, (pdf_name *)o,
                                 (pdf_dict *)decode, s, &new_s, inline_image);
        pdfi_countdown(o);
        if (code < 0)
            return code;

        code = pdfi_alloc_stream(ctx, new_s, source->s, new_stream);
    }
    return code;
}

/* This is just a convenience routine. We could use pdfi_filter() above, but because PDF
 * doesn't support the SubFileDecode filter that would mean callers having to manufacture
 * a dictionary in order to use it. That's excessively convoluted, so just supply a simple
 * means to instantiate a SubFileDecode filter.
 */
int pdfi_apply_SubFileDecode_filter(pdf_context *ctx, int EODCount, gs_const_string *EODString, pdf_stream *source, pdf_stream **new_stream, bool inline_image)
{
    pdf_name SFD_name;
    int code;
    stream *s = source->s, *new_s = NULL;
    *new_stream = NULL;

    SFD_name.data = (byte *)"SubFileDecode";
    SFD_name.length = 13;
    code = pdfi_apply_filter(ctx, NULL, &SFD_name, NULL, s, &new_s, inline_image);
    if (code < 0)
        return code;
    code = pdfi_alloc_stream(ctx, new_s, source->s, new_stream);
    return code;
}

/* We would really like to use a ReusableStreamDecode filter here, but that filter is defined
 * purely in the PostScript interpreter. So instead we make a temporary stream from a
 * memory buffer. Its icky (we can end up with the same data in memory multiple times)
 * but it works, and is used elsewhere in Ghostscript.
 * The calling function is responsible for the stream and buffer pointer lifetimes.
 */
int pdfi_open_memory_stream_from_stream(pdf_context *ctx, unsigned int size, byte **Buffer, pdf_stream *source, pdf_stream **new_pdf_stream)
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

    sread_string(new_stream, *Buffer, size);

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
int pdfi_open_memory_stream_from_filtered_stream(pdf_context *ctx, pdf_dict *stream_dict, unsigned int size, byte **Buffer, pdf_stream *source, pdf_stream **new_pdf_stream)
{
    int code;
    int decompressed_length = 0;
    byte *decompressed_Buffer = NULL;
    pdf_stream *compressed_stream = NULL, *decompressed_stream = NULL;
    bool known = false;

    code = pdfi_open_memory_stream_from_stream(ctx, (unsigned int)size, Buffer, source, new_pdf_stream);
    if (code < 0) {
        pdfi_close_memory_stream(ctx, *Buffer, *new_pdf_stream);
        *Buffer = NULL;
        *new_pdf_stream = NULL;
        return code;
    }

    if (stream_dict == NULL)
        return size;

    pdfi_dict_known(stream_dict, "F", &known);
    if (!known)
        pdfi_dict_known(stream_dict, "Filter", &known);

    if (!known)
        return size;

    compressed_stream = *new_pdf_stream;
    /* This is again complicated by requiring a seekable stream, and the fact that,
     * unlike fonts, there is no Length2 key to tell us how large the uncompressed
     * stream is.
     */
    code = pdfi_filter(ctx, stream_dict, compressed_stream, &decompressed_stream, false);
    if (code < 0) {
        pdfi_close_memory_stream(ctx, *Buffer, *new_pdf_stream);
        gs_free_object(ctx->memory, *Buffer, "pdfi_open_memory_stream_from_filtered_stream");
        *Buffer = NULL;
        *new_pdf_stream = NULL;
        return code;
    }
    do {
        byte b;
        code = pdfi_read_bytes(ctx, &b, 1, 1, decompressed_stream);
        if (code <= 0)
            break;
        decompressed_length++;
    } while (true);
    pdfi_close_file(ctx, decompressed_stream);

    decompressed_Buffer = gs_alloc_bytes(ctx->memory, decompressed_length, "pdfi_open_memory_stream_from_filtered_stream (decompression buffer)");
    if (decompressed_Buffer != NULL) {
        code = srewind(compressed_stream->s);
        if (code >= 0) {
            code = pdfi_filter(ctx, stream_dict, compressed_stream, &decompressed_stream, false);
            if (code >= 0) {
                code = pdfi_read_bytes(ctx, decompressed_Buffer, 1, decompressed_length, decompressed_stream);
                pdfi_close_file(ctx, decompressed_stream);
                code = pdfi_close_memory_stream(ctx, *Buffer, *new_pdf_stream);
                if (code >= 0) {
                    *Buffer = decompressed_Buffer;
                    code = pdfi_open_memory_stream_from_memory(ctx, (unsigned int)decompressed_length,
                                                               *Buffer, new_pdf_stream);
                } else {
                    *Buffer = NULL;
                    *new_pdf_stream = NULL;
                }
            }
        } else {
            pdfi_close_memory_stream(ctx, *Buffer, *new_pdf_stream);
            gs_free_object(ctx->memory, decompressed_Buffer, "pdfi_open_memory_stream_from_filtered_stream");
            gs_free_object(ctx->memory, Buffer, "pdfi_open_memory_stream_from_filtered_stream");
            *Buffer = NULL;
            *new_pdf_stream = NULL;
            return code;
        }
    } else {
        pdfi_close_memory_stream(ctx, *Buffer, *new_pdf_stream);
        gs_free_object(ctx->memory, Buffer, "pdfi_open_memory_stream_from_filtered_stream");
        *Buffer = NULL;
        *new_pdf_stream = NULL;
        return_error(gs_error_VMerror);
    }
    if (code < 0) {
        gs_free_object(ctx->memory, Buffer, "pdfi_build_function_4");
        *Buffer = NULL;
        *new_pdf_stream = NULL;
        return code;
    }
    return decompressed_length;
}

int pdfi_open_memory_stream_from_memory(pdf_context *ctx, unsigned int size, byte *Buffer, pdf_stream **new_pdf_stream)
{
    int code;
    stream *new_stream;

    new_stream = file_alloc_stream(ctx->memory, "open memory stream from memory(stream)");
    if (new_stream == NULL)
        return_error(gs_error_VMerror);
    sread_string(new_stream, Buffer, size);

    code = pdfi_alloc_stream(ctx, new_stream, NULL, new_pdf_stream);
    if (code < 0) {
        sclose(new_stream);
        gs_free_object(ctx->memory, new_stream, "open memory stream from memory(stream)");
    }

    return code;
}

int pdfi_close_memory_stream(pdf_context *ctx, byte *Buffer, pdf_stream *source)
{
    sclose(source->s);
    gs_free_object(ctx->memory, Buffer, "open memory stream(buffer)");
    gs_free_object(ctx->memory, source->s, "open memory stream(stream)");
    gs_free_object(ctx->memory, source, "open memory stream(pdf_stream)");
    return 0;
}

/***********************************************************************************/
/* Basic 'file' operations. Because of the need to 'unread' bytes we need our own  */

void pdfi_close_file(pdf_context *ctx, pdf_stream *s)
{
    stream *next_s = s->s;

    while(next_s && next_s != s->original){
        stream *curr_s = next_s;
        next_s = next_s->strm;
        if (curr_s != ctx->main_stream->s)
            sfclose(curr_s);
    }
    gs_free_object(ctx->memory, s, "closing pdf_file");
}

int pdfi_seek(pdf_context *ctx, pdf_stream *s, gs_offset_t offset, uint32_t origin)
{
    if (origin == SEEK_CUR && s->unread_size != 0)
        offset -= s->unread_size;

    s->unread_size = 0;;

    return (sfseek(s->s, offset, origin));
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

gs_offset_t pdfi_tell(pdf_stream *s)
{
    return stell(s->s);
}

int pdfi_unread(pdf_context *ctx, pdf_stream *s, byte *Buffer, uint32_t size)
{
    if (size + s->unread_size > UNREAD_BUFFER_SIZE)
        return_error(gs_error_ioerror);

    if (s->unread_size) {
        uint32_t index = s->unread_size - 1;

        do {
            s->unget_buffer[size + index] = s->unget_buffer[index];
        } while(index--);
    }

    memcpy(s->unget_buffer, Buffer, size);
    s->unread_size += size;

    return 0;
}

int pdfi_read_bytes(pdf_context *ctx, byte *Buffer, uint32_t size, uint32_t count, pdf_stream *s)
{
    uint32_t i = 0, total = size * count;
    uint32_t bytes = 0;
    int32_t code;

    if (s->eof)
        return 0;

    if (s->unread_size) {
        if (s->unread_size >= total) {
            memcpy(Buffer, s->unget_buffer, total);
            for(i=0;i < s->unread_size - total;i++) {
                s->unget_buffer[i] = s->unget_buffer[i + total];
            }
            s->unread_size -= total;
            return total;
        } else {
            memcpy(Buffer, s->unget_buffer, s->unread_size);
            total -= s->unread_size;
            Buffer += s->unread_size;
            i = s->unread_size;
            s->unread_size = 0;
        }
    }
    if (total) {
        /* TODO the Ghostscript code uses sbufptr(s) to avoid a memcpy
         * at some point we should modify this code to do so as well.
         */
        code = sgets(s->s, Buffer, total, &bytes);
        if (code == EOFC) {
            s->eof = true;
        } else if(code == ERRC) {
            bytes = ERRC;
        } else {
            bytes = bytes + i;
        }
    }

    return bytes;
}

/* Read bytes from stream object into buffer.
 * Handles both plain streams and filtered streams.
 * Buffer gets allocated here, and must be freed by caller.
 * Preserves the location of the current stream file position.
 */
int
pdfi_stream_to_buffer(pdf_context *ctx, pdf_dict *stream_dict, byte **buf, int64_t *bufferlen)
{
    byte *Buffer = NULL;
    int code = 0;
    int64_t buflen = 0;
    int bytes;
    char c;
    gs_offset_t savedoffset;
    pdf_stream *stream;
    bool filtered;

    savedoffset = pdfi_tell(ctx->main_stream);

    pdfi_seek(ctx, ctx->main_stream, stream_dict->stream_offset, SEEK_SET);

    /* See if this is a filtered stream */
    code = pdfi_dict_known(stream_dict, "Filter", &filtered);
    if (code < 0)
        goto exit;

    if (!filtered) {
        code = pdfi_dict_known(stream_dict, "F", &filtered);
        if (code < 0)
            goto exit;
    }

    if (filtered) {
        code = pdfi_filter(ctx, stream_dict, ctx->main_stream, &stream, false);
        if (code < 0) {
            goto exit;
        }
        /* Find out how big it is */
        do {
            bytes = sfread(&c, 1, 1, stream->s);
            if (bytes > 0)
                buflen++;
        } while (bytes >= 0);
        pdfi_close_file(ctx, stream);
    } else {
        code = pdfi_dict_get_int(ctx, stream_dict, "Length", &buflen);
        if (code < 0)
            goto exit;
    }

    /* Alloc buffer */
    Buffer = gs_alloc_bytes(ctx->memory, buflen, "pdfi_stream_to_buffer (Buffer)");
    if (!Buffer) {
        code = gs_note_error(gs_error_VMerror);
        goto exit;
    }
    code = pdfi_seek(ctx, ctx->main_stream, stream_dict->stream_offset, SEEK_SET);
    if (code < 0)
        goto exit;
    if (filtered) {
        code = pdfi_filter(ctx, stream_dict, ctx->main_stream, &stream, false);
        sfread(Buffer, 1, buflen, stream->s);
        pdfi_close_file(ctx, stream);
    } else {
        sfread(Buffer, 1, buflen, ctx->main_stream->s);
    }

 exit:
    pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
    if (Buffer && code < 0)
        gs_free_object(ctx->memory, Buffer, "pdfi_stream_to_buffer (Buffer)");
    *buf = Buffer;
    *bufferlen = buflen;
    return code;
}
