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
#include "szlibx.h"
#include "spngpx.h"
#include "spdiffx.h"
#include "strmio.h"

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

static int pdf_Flate_filter(pdf_context *ctx, pdf_dict *d, stream *source, stream **new_stream)
{
    stream_zlib_state zls;
    pdf_dict *DP;
    pdf_obj *o;
    stream_PNGP_state pps;
    uint min_size = 2;
    int code;
    uint32_t Predictor = 1;

    /* s_zlibD_template defined in base/szlibd.c */
    (*s_zlibD_template.set_defaults)((stream_state *)&zls);

    code = pdf_dict_get(d, "DecodeParms", &o);
    if (code < 0 && code != gs_error_undefined)
        return code;

    if (code != gs_error_undefined) {
        DP = (pdf_dict *)o;
        code = pdf_dict_get(DP, "Predictor", &o);
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
                min_size = 2;
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
                code = pdf_dict_get(DP, "Colors", &o);
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

                code = pdf_dict_get(DP, "BitsPerComponent", &o);
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

                code = pdf_dict_get(DP, "Columns", &o);
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
    }
    switch(Predictor) {
        case 1:
            pdf_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_zlibD_template, (const stream_state *)&zls, ctx->memory->non_gc_memory, new_stream);
            break;
        case 2:
        default:
            pdf_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_zlibD_template, (const stream_state *)&zls, ctx->memory->non_gc_memory, new_stream);
            (*new_stream)->strm = source;
            source = *new_stream;
            pdf_filter_open(min_size, &s_filter_read_procs, (const stream_template *)&s_PNGPD_template, (const stream_state *)&pps, ctx->memory->non_gc_memory, new_stream);
            break;
    }
    (*new_stream)->strm = source;
    return 0;
}

int pdf_filter(pdf_context *ctx, pdf_dict *d, pdf_stream *source, pdf_stream **new_stream)
{
    pdf_obj *o;
    pdf_name *n;
    int code;
    stream *temp_stream;

    code = pdf_dict_get(d, "Filter", &o);
    if (code < 0)
        return code;

    if (o->type != PDF_NAME)
        return_error(gs_error_typecheck);

    n = (pdf_name *)o;

    if (n->length == 15 && memcmp((const char *)n->data, "RunLengthDecode", 15) == 0) {
    }
    if (n->length == 14 && memcmp((const char *)n->data, "CCITTFaxDecode", 14) == 0) {
    }
    if (n->length == 14 && memcmp((const char *)n->data, "ASCIIHexDecode", 14) == 0) {
    }
    if (n->length == 13 && memcmp((const char *)n->data, "ASCII85Decode", 13) == 0) {
    }
    if (n->length == 11 && memcmp((const char *)n->data, "FlateDecode", 11) == 0) {
        code = pdf_Flate_filter(ctx, d, source->s, &temp_stream);
        if (code < 0)
            return code;
        *new_stream = (pdf_stream *)gs_alloc_bytes(ctx->memory, sizeof(pdf_stream), "pdf_filter, new pdf_stream");
        if (*new_stream == NULL)
            return_error(gs_error_VMerror);
        memset(*new_stream, 0x00, sizeof(pdf_stream));
        (*new_stream)->eof = false;
        ((pdf_stream *)(*new_stream))->s = temp_stream;
        return 0;
    }
    if (n->length == 11 && memcmp((const char *)n->data, "JBIG2Decode", 11) == 0) {
    }
    if (n->length == 9 && memcmp((const char *)n->data, "LZWDecode", 9) == 0) {
    }
    if (n->length == 9 && memcmp((const char *)n->data, "DCTDecode", 9) == 0) {
    }
    if (n->length == 9 && memcmp((const char *)n->data, "JPXDecode", 9) == 0) {
    }

    return_error(gs_error_undefined);
}


/***********************************************************************************/
/* Basic 'file' operations. Because of the need to 'unread' bytes we need our own  */

void pdf_close_file(pdf_context *ctx, pdf_stream *s)
{
    if (s->s) {
        sfclose(s->s);
        s->s = NULL;
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
