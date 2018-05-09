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
#include "pdf_file.h"
#include "pdf_int.h"
#include "stream.h"
#include "strimpl.h"
#include "strmio.h"
#include "szlibx.h"     /* Flate */
#include "spngpx.h"     /* PNG Predictor */
#include "slzwx.h"      /* LZW ZLib */
#include "sstring.h"    /* ASCIIHexDecode */
#include "sa85d.h"      /* ASCII85Decode */
#include "scfx.h"       /* CCITTFaxDecode */
#include "srlx.h"       /* RunLengthDecode */
#include "jpeglib.h"
#include "sdct.h"       /* DCTDecode */

/***********************************************************************************/
/* Decompression filters.                                                          */

static int
pdf_filter_report_error(stream_state * st, const char *str)
{
    if_debug1m('s', st->memory, "[s]stream error: %s\n", str);
    strncpy(st->error_string, str, STREAM_MAX_ERROR_STRING);
    /* Ensure null termination. */
    st->error_string[STREAM_MAX_ERROR_STRING] = 0;
    return 0;
}

/* Open a file stream for a filter. */
static int
pdf_filter_open(uint buffer_size,
            const stream_procs * procs, const stream_template * templat,
            const stream_state * st, gs_memory_t *mem, stream **new_stream)
{
    stream *s;
    uint ssize = gs_struct_type_size(templat->stype);
    stream_state *sst = NULL;
    int code;

    if (templat->stype != &st_stream_state) {
        sst = s_alloc_state(mem, templat->stype, "pdf_filter_open(stream_state)");
        if (sst == NULL)
            return_error(gs_error_VMerror);
    }
    code = file_open_stream((char *)0, 0, "r", buffer_size, &s,
                                (gx_io_device *)0, (iodev_proc_fopen_t)0, mem);
    if (code < 0) {
        gs_free_object(mem, sst, "pdf_filter_open(stream_state)");
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
    sst->report_error = pdf_filter_report_error;

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

static int pdf_Predictor_filter(pdf_context *ctx, pdf_dict *d, stream *source, stream **new_stream)
{
    int code;
    uint32_t Predictor = 1;
    pdf_dict *DP;
    pdf_obj *o;
    uint min_size = 2048;
    stream_PNGP_state pps;

    code = pdf_dict_get(d, "Predictor", &o);
    if (code < 0 && code != gs_error_undefined)
        return code;

    if (code != gs_error_undefined) {
        if (o->type != PDF_INT)
            return_error(gs_error_typecheck);

        Predictor = (uint32_t)((pdf_num *)o)->value.i;
    }
    switch(Predictor) {
        case 0:
            break;
        case 1:
            min_size = 2048;
            break;
        case 2:
            /* zpd_setup, componentwise horizontal differencing */
            break;
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            /* zpp_setup, PNG predictor */
            min_size = s_zlibD_template.min_out_size + max_min_left;
            code = pdf_dict_get(d, "Colors", &o);
            if (code < 0 && code != gs_error_undefined)
                return code;
            if(code == gs_error_undefined)
                pps.Colors = 1;
            else {
                if (o->type != PDF_INT)
                    return_error(gs_error_typecheck);

                pps.Colors = ((pdf_num *)o)->value.i;
            }
            if (pps.Colors < 1 || pps.Colors > s_PNG_max_Colors)
                return_error(gs_error_rangecheck);

            code = pdf_dict_get(d, "BitsPerComponent", &o);
            if (code < 0 && code != gs_error_undefined)
                return code;
            if(code == gs_error_undefined)
                pps.BitsPerComponent = 8;
            else {
                if (o->type != PDF_INT)
                    return_error(gs_error_typecheck);

                pps.BitsPerComponent = ((pdf_num *)o)->value.i;
            }
            if (pps.BitsPerComponent < 1 || pps.BitsPerComponent > 16 || (pps.BitsPerComponent & (pps.BitsPerComponent - 1)) != 0)
                return_error(gs_error_rangecheck);

            code = pdf_dict_get(d, "Columns", &o);
            if (code < 0 && code != gs_error_undefined)
                return code;
            if(code == gs_error_undefined)
                pps.Columns = 1;
            else {
                if (o->type != PDF_INT)
                    return_error(gs_error_typecheck);

                pps.Columns = ((pdf_num *)o)->value.i;
            }
            if (pps.Columns < 1)
                return_error(gs_error_rangecheck);

            pps.Predictor = Predictor;
            break;
        default:
            return_error(gs_error_rangecheck);
    }

    switch(Predictor) {
        case 1:
            *new_stream = source;
            break;
        case 2:
        default:
            pdf_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_PNGPD_template, (const stream_state *)&pps, ctx->memory->non_gc_memory, new_stream);
            (*new_stream)->strm = source;
            break;
    }
    return 0;
}

static int pdf_Flate_filter(pdf_context *ctx, pdf_dict *d, stream *source, stream **new_stream)
{
    stream_zlib_state zls;
    uint min_size = 2048;
    int code;

    /* s_zlibD_template defined in base/szlibd.c */
    (*s_zlibD_template.set_defaults)((stream_state *)&zls);

    code = pdf_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_zlibD_template, (const stream_state *)&zls, ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        return code;

    (*new_stream)->strm = source;
    source = *new_stream;

    if (d && d->type == PDF_DICT)
        pdf_Predictor_filter(ctx, d, source, new_stream);
    return 0;
}

static int pdf_LZW_filter(pdf_context *ctx, pdf_dict *d, stream *source, stream **new_stream)
{
    stream_LZW_state lzs;
    uint min_size = 2048;
    pdf_obj *o;
    int code;
    int64_t i;

    /* s_zlibD_template defined in base/szlibd.c */
    s_LZW_set_defaults_inline(&lzs);

    if (d && d->type == PDF_DICT) {
        code = pdf_dict_get_int(ctx, d, "EarlyChange", &i);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0) {
            if (i == 0)
                lzs.EarlyChange = false;
            else
                lzs.EarlyChange = true;
        }
    }

    code = pdf_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_LZWD_template, (const stream_state *)&lzs, ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        return code;
    (*new_stream)->strm = source;
    source = *new_stream;

    if (d && d->type == PDF_DICT)
        pdf_Predictor_filter(ctx, d, source, new_stream);
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

static int pdf_DCT_filter(pdf_context *ctx, pdf_dict *d, stream *source, stream **new_stream)
{
    stream_DCT_state dcts;
    uint min_size = 2048;
    pdf_obj *o;
    int code;
    int64_t i;
    jpeg_decompress_data *jddp;
    gx_device *dev = gs_currentdevice_inline(ctx->pgs);

    dcts.memory = ctx->memory;
    /* First allocate space for IJG parameters. */
    jddp = gs_alloc_struct_immovable(ctx->memory, jpeg_decompress_data,
      &st_jpeg_decompress_data, "pdf_DCT");
    if (jddp == 0)
        return_error(gs_error_VMerror);
    if (s_DCTD_template.set_defaults)
        (*s_DCTD_template.set_defaults) ((stream_state *) & dcts);

    dcts.data.decompress = jddp;
    jddp->memory = dcts.jpeg_memory = ctx->memory;	/* set now for allocation */
    jddp->scanline_buffer = NULL;	                /* set this early for safe error exit */
    dcts.report_error = pdf_filter_report_error;	    /* in case create fails */
    if ((code = gs_jpeg_create_decompress(&dcts)) < 0) {
        gs_jpeg_destroy(&dcts);
        gs_free_object(ctx->memory, jddp, "zDCTD fail");
        return code;
    }

    if (d && d->type == PDF_DICT) {
        code = pdf_dict_get_int(ctx, d, "ColorTransform", &i);
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

    code = pdf_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&jddp->templat, (const stream_state *)&dcts, ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        return code;
    (*new_stream)->strm = source;
    source = *new_stream;

    return 0;
}

static int pdf_ASCII85_filter(pdf_context *ctx, pdf_dict *d, stream *source, stream **new_stream)
{
    stream_A85D_state ss;
    uint min_size = 2048;
    pdf_obj *o;
    int code;
    int64_t i;

    ss.pdf_rules = true;

    code = pdf_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_A85D_template, (const stream_state *)&ss, ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        return code;

    (*new_stream)->strm = source;
    return 0;
}

static int pdf_CCITTFax_filter(pdf_context *ctx, pdf_dict *d, stream *source, stream **new_stream)
{
    stream_CFD_state ss;
    uint min_size = 2048;
    pdf_obj *o;
    int code;
    int64_t i;

    s_CF_set_defaults_inline(&ss);

    if (d && d->type == PDF_DICT) {
        code = pdf_dict_get_int(ctx, d, "K", &i);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.K = i;

        code = pdf_dict_get_type(ctx, d, "EndOfLine", PDF_BOOL, &o);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.EndOfLine = ((pdf_bool *)o)->value;

        code = pdf_dict_get_type(ctx, d, "EncodedByteAlign", PDF_BOOL, &o);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.EncodedByteAlign = ((pdf_bool *)o)->value;

        code = pdf_dict_get_int(ctx, d, "Columns", &i);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.Columns = i;

        code = pdf_dict_get_int(ctx, d, "Rows", &i);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.Rows = i;

        code = pdf_dict_get_int(ctx, d, "EndOfBlock", &i);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.EndOfBlock = i;

        code = pdf_dict_get_int(ctx, d, "BlackIs1", &i);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.BlackIs1 = i;

        code = pdf_dict_get_int(ctx, d, "DamagedRowsBeforeError", &i);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0)
            ss.DamagedRowsBeforeError = i;

    }

    code = pdf_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_CFD_template, (const stream_state *)&ss, ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        return code;

    (*new_stream)->strm = source;
    return 0;
}

static int pdf_simple_filter(pdf_context *ctx, const stream_template *tmplate, stream *source, stream **new_stream)
{
    uint min_size = 2048;
    stream *temp_stream;
    int code;

    code = pdf_filter_open(min_size, &s_filter_read_procs, tmplate, NULL, ctx->memory->non_gc_memory, new_stream);
    if (code < 0)
        return code;

    (*new_stream)->strm = source;
    return 0;
}

static int pdf_apply_filter(pdf_context *ctx, pdf_name *n, pdf_dict *decode, stream *source, stream **new_stream, bool inline_image)
{
    int code;

    if (n->length == 15 && memcmp((const char *)n->data, "RunLengthDecode", 15) == 0) {
        code = pdf_simple_filter(ctx, &s_RLE_template, source, new_stream);
        return code;
    }
    if (n->length == 14 && memcmp((const char *)n->data, "CCITTFaxDecode", 14) == 0) {
        code = pdf_CCITTFax_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (n->length == 14 && memcmp((const char *)n->data, "ASCIIHexDecode", 14) == 0) {
        code = pdf_simple_filter(ctx, &s_AXD_template, source, new_stream);
        return code;
    }
    if (n->length == 13 && memcmp((const char *)n->data, "ASCII85Decode", 13) == 0) {
        code = pdf_ASCII85_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (n->length == 11 && memcmp((const char *)n->data, "FlateDecode", 11) == 0) {
        code = pdf_Flate_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (n->length == 11 && memcmp((const char *)n->data, "JBIG2Decode", 11) == 0) {
    }
    if (n->length == 9 && memcmp((const char *)n->data, "LZWDecode", 9) == 0) {
        code = pdf_LZW_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (n->length == 9 && memcmp((const char *)n->data, "DCTDecode", 9) == 0) {
        code = pdf_DCT_filter(ctx, decode, source, new_stream);
        return code;
    }
    if (n->length == 9 && memcmp((const char *)n->data, "JPXDecode", 9) == 0) {
    }

    if (inline_image) {
        if (n->length == 3 && memcmp((const char *)n->data, "AHx", 3) == 0) {
            code = pdf_simple_filter(ctx, &s_AXD_template, source, new_stream);
            return code;
        }
        if (n->length == 3 && memcmp((const char *)n->data, "A85", 3) == 0) {
            code = pdf_simple_filter(ctx, &s_AXD_template, source, new_stream);
            return code;
        }
        if (n->length == 3 && memcmp((const char *)n->data, "LZW", 3) == 0) {
            code = pdf_LZW_filter(ctx, decode, source, new_stream);
            return code;
        }
        if (n->length == 3 && memcmp((const char *)n->data, "CCF", 3) == 0) {
            code = pdf_CCITTFax_filter(ctx, decode, source, new_stream);
            return code;
        }
        if (n->length == 3 && memcmp((const char *)n->data, "DCT", 3) == 0) {
            code = pdf_DCT_filter(ctx, decode, source, new_stream);
            return code;
        }
        if (n->length == 2 && memcmp((const char *)n->data, "Fl", 2) == 0) {
            code = pdf_Flate_filter(ctx, decode, source, new_stream);
            return code;
        }
        if (n->length == 2 && memcmp((const char *)n->data, "RL", 2) == 0) {
            code = pdf_simple_filter(ctx, &s_RLE_template, source, new_stream);
            return code;
        }
    }

    return_error(gs_error_undefined);
}

int pdf_filter(pdf_context *ctx, pdf_dict *d, pdf_stream *source, pdf_stream **new_stream, bool inline_image)
{
    pdf_obj *o = NULL, *decode = NULL;
    int code;
    uint64_t i;
    stream *s = source->s, *new_s = NULL;
    *new_stream = NULL;

    code = pdf_dict_get(d, "Filter", &o);
    if (code < 0){
        if (code == gs_error_undefined) {
            if (inline_image == true) {
                code = pdf_dict_get(d, "F", &o);
                if (code < 0 && code != gs_error_undefined)
                    return code;
            }
            if (code < 0) {
                *new_stream = (pdf_stream *)gs_alloc_bytes(ctx->memory, sizeof(pdf_stream), "pdf_filter, new pdf_stream");
                if (*new_stream == NULL)
                    return_error(gs_error_VMerror);
                memset(*new_stream, 0x00, sizeof(pdf_stream));
                (*new_stream)->eof = false;
                ((pdf_stream *)(*new_stream))->s = s;
                ((pdf_stream *)(*new_stream))->original = source->s;
                return 0;
            }
        } else
            return code;
    }

    if (o->type != PDF_NAME) {
        if (o->type == PDF_ARRAY) {
            pdf_array *filter_array = (pdf_array *)o;
            pdf_array *decodeparams_array = NULL;

            code = pdf_dict_get(d, "DecodeParms", &o);
            if (code < 0 && code) {
                if (code == gs_error_undefined) {
                    if (inline_image == true) {
                        code = pdf_dict_get(d, "DP", &o);
                        if (code < 0 && code != gs_error_undefined) {
                            pdf_countdown(o);
                            pdf_countdown(filter_array);
                            return code;
                        }
                    }
                } else {
                    pdf_countdown(o);
                    pdf_countdown(filter_array);
                    return code;
                }
            }

            if (code != gs_error_undefined) {
                decodeparams_array = (pdf_array *)o;
                if (decodeparams_array->type != PDF_ARRAY) {
                    pdf_countdown(decodeparams_array);
                    pdf_countdown(filter_array);
                    return_error(gs_error_typecheck);
                }
                if (decodeparams_array->entries != filter_array->entries) {
                    pdf_countdown(decodeparams_array);
                    pdf_countdown(filter_array);
                    return_error(gs_error_rangecheck);
                }
            }

            for (i = 0; i < filter_array->entries;i++) {
                code = pdf_array_get(filter_array, i, &o);
                if (code < 0) {
                    pdf_countdown(decodeparams_array);
                    pdf_countdown(filter_array);
                    return code;
                }
                if (o->type != PDF_NAME) {
                    pdf_countdown(decodeparams_array);
                    pdf_countdown(filter_array);
                    return_error(gs_error_typecheck);
                }

                if (decodeparams_array != NULL) {
                    code = pdf_array_get(decodeparams_array, i, &decode);
                    if (code < 0) {
                        pdf_countdown(decodeparams_array);
                        pdf_countdown(filter_array);
                        return code;
                    }
                }
                if (decode && decode->type != PDF_NULL && decode->type != PDF_DICT) {
                    pdf_countdown(decodeparams_array);
                    pdf_countdown(filter_array);
                    return_error(gs_error_typecheck);
                }

                code = pdf_apply_filter(ctx, (pdf_name *)o, (pdf_dict *)decode, s, &new_s, inline_image);
                if (code < 0) {
                    *new_stream = 0;
                    pdf_countdown(decodeparams_array);
                    pdf_countdown(filter_array);
                    return code;
                }
                s = new_s;
            }
            pdf_countdown(decodeparams_array);
            pdf_countdown(filter_array);
            *new_stream = (pdf_stream *)gs_alloc_bytes(ctx->memory, sizeof(pdf_stream), "pdf_filter, new pdf_stream");
            if (*new_stream == NULL)
                return_error(gs_error_VMerror);
            memset(*new_stream, 0x00, sizeof(pdf_stream));
            (*new_stream)->eof = false;
            ((pdf_stream *)(*new_stream))->original = source->s;
            ((pdf_stream *)(*new_stream))->s = s;
        } else
            return_error(gs_error_typecheck);
    } else {
        code = pdf_dict_get(d, "DecodeParms", &decode);
        if (code < 0 && code) {
            if (code == gs_error_undefined) {
                if (inline_image == true) {
                    code = pdf_dict_get(d, "DP", &decode);
                    if (code < 0 && code != gs_error_undefined) {
                        pdf_countdown(o);
                        return code;
                    }
                }
            } else {
                pdf_countdown(o);
                return code;
            }
        }

        code = pdf_apply_filter(ctx, (pdf_name *)o, (pdf_dict *)decode, s, &new_s, inline_image);
        pdf_countdown(o);
        if (code < 0)
            return code;

        *new_stream = (pdf_stream *)gs_alloc_bytes(ctx->memory, sizeof(pdf_stream), "pdf_filter, new pdf_stream");
        if (*new_stream == NULL)
            return_error(gs_error_VMerror);
        memset(*new_stream, 0x00, sizeof(pdf_stream));
        (*new_stream)->eof = false;
        ((pdf_stream *)(*new_stream))->original = source->s;
        ((pdf_stream *)(*new_stream))->s = new_s;
    }
    return code;
}


/***********************************************************************************/
/* Basic 'file' operations. Because of the need to 'unread' bytes we need our own  */

void pdf_close_file(pdf_context *ctx, pdf_stream *s)
{
    stream *next_s = s->s;

    while(next_s && next_s != s->original){
        if (next_s && next_s != ctx->main_stream->s)
            sfclose(next_s);
        next_s = next_s->strm;
    }
    gs_free_object(ctx->memory, s, "closing pdf_file");
}

int pdf_seek(pdf_context *ctx, pdf_stream *s, gs_offset_t offset, uint32_t origin)
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
gs_offset_t pdf_tell(pdf_context *ctx)
{
    gs_offset_t off = stell(ctx->main_stream->s);

    return (off - ctx->main_stream->unread_size);
}

int pdf_unread(pdf_context *ctx, pdf_stream *s, byte *Buffer, uint32_t size)
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

int pdf_read_bytes(pdf_context *ctx, byte *Buffer, uint32_t size, uint32_t count, pdf_stream *s)
{
    uint32_t i = 0, total = size * count;
    int32_t bytes = 0;

    if (s->unread_size) {
        if (s->unread_size >= total) {
            memcpy(Buffer, s->unget_buffer, total);
            for(i=0;i < s->unread_size - total;i++) {
                s->unget_buffer[i] = s->unget_buffer[i + total];
            }
            s->unread_size -= total;
            return size;
        } else {
            memcpy(Buffer, s->unget_buffer, s->unread_size);
            total -= s->unread_size;
            Buffer += s->unread_size;
            i = s->unread_size;
            s->unread_size = 0;
        }
    }
    if (total) {
        bytes = sfread(Buffer, 1, total, s->s);
    }
    if (bytes >= 0)
        return i + bytes;
    else {
        if (bytes == EOFC){
            s->eof = true;
            return 0;
        }
        return bytes;
    }
}
