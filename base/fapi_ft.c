/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/*
   GhostScript Font API plug-in that allows fonts to be rendered by FreeType.
   Started by Graham Asher, 6th June 2002.
 */

/* GhostScript headers. */
#include "stdio_.h"
#include "malloc_.h"
#include "write_t1.h"
#include "write_t2.h"
#include "math_.h"
#include "gserrors.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gxfixed.h"
#include "gdebug.h"
#include "gxbitmap.h"
#include "gsmchunk.h"
#include "gxfont.h"
#include "gxfont1.h"

#include "stream.h"
#include "gxiodev.h"            /* must come after stream.h */

#include "gsfname.h"

#include "gxfapi.h"


/* FreeType headers */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_INCREMENTAL_H
#include FT_GLYPH_H
#include FT_SYSTEM_H
#include FT_MODULE_H
#include FT_TRIGONOMETRY_H
#include FT_BBOX_H
#include FT_OUTLINE_H
#include FT_IMAGE_H
#include FT_BITMAP_H
#include FT_TRUETYPE_DRIVER_H
#include FT_TRUETYPE_TABLES_H
#include FT_MULTIPLE_MASTERS_H
#include FT_TYPE1_TABLES_H

/* Note: structure definitions here start with FF_, which stands for 'FAPI FreeType". */

#define ft_emprintf(m,s) { outflush(m); emprintf(m, s); outflush(m); }
#define ft_emprintf1(m,s,d) { outflush(m); emprintf1(m, s, d); outflush(m); }

typedef struct ff_server_s
{
    gs_fapi_server fapi_server;
    FT_Library freetype_library;
    FT_OutlineGlyph outline_glyph;
    FT_BitmapGlyph bitmap_glyph;
    gs_memory_t *mem;
    FT_Memory ftmemory;
    struct FT_MemoryRec_ ftmemory_rec;
} ff_server;



typedef struct ff_face_s
{
    FT_Face ft_face;

    /* Currently in force scaling/transform for this face */
    FT_Matrix ft_transform;
    FT_F26Dot6 width, height;
    FT_UInt horz_res;
    FT_UInt vert_res;

    /* If non-null, the incremental interface object passed to FreeType. */
    FT_Incremental_InterfaceRec *ft_inc_int;
    /* If non-null, we're using a custom stream object for Freetype to read the font file */
    FT_Stream ftstrm;
    /* Non-null if font data is owned by this object. */
    unsigned char *font_data;
    int font_data_len;
    bool data_owned;
    ff_server *server;
} ff_face;

/* Here we define the struct FT_Incremental that is used as an opaque type
 * inside FreeType. This structure has to have the tag FT_IncrementalRec_
 * to be compatible with the functions defined in FT_Incremental_FuncsRec.
 */
typedef struct FT_IncrementalRec_
{
    gs_fapi_font *fapi_font;    /* The font. */

    /* If it is already in use glyph data is allocated on the heap. */
    unsigned char *glyph_data;  /* A one-shot buffer for glyph data. */
    size_t glyph_data_length;   /* Length in bytes of glyph_data. */
    bool glyph_data_in_use;     /* True if glyph_data is already in use. */

    FT_Incremental_MetricsRec glyph_metrics;    /* Incremental glyph metrics supplied by Ghostscript. */
    unsigned long glyph_metrics_index;  /* contains data for this glyph index unless it is 0xFFFFFFFF. */
    gs_fapi_metrics_type metrics_type;  /* determines whether metrics are replaced, added, etc. */
} FT_IncrementalRec;


static void
delete_inc_int(gs_fapi_server * a_server,
               FT_Incremental_InterfaceRec * a_inc_int);

static void
delete_inc_int_info(gs_fapi_server * a_server,
                    FT_IncrementalRec * a_inc_int_info);

static void *
FF_alloc(FT_Memory memory, long size)
{
    gs_memory_t *mem = (gs_memory_t *) memory->user;

    return (gs_malloc(mem, size, 1, "FF_alloc"));
}

static void *
    FF_realloc(FT_Memory memory, long cur_size, long new_size, void *block)
{
    gs_memory_t *mem = (gs_memory_t *) memory->user;
    void *tmp;

    if (cur_size == new_size) {
        return (block);
    }

    tmp = gs_malloc(mem, new_size, 1, "FF_realloc");
    if (tmp && block) {
        memcpy(tmp, block, min(cur_size, new_size));

        gs_free(mem, block, 0, 0, "FF_realloc");
    }

    return (tmp);
}

static void
    FF_free(FT_Memory memory, void *block)
{
    gs_memory_t *mem = (gs_memory_t *) memory->user;

    gs_free(mem, block, 0, 0, "FF_free");
}

/* The following three functions are used in providing a custom stream
 * object to Freetype, so file access happens through Ghostscript's
 * file i/o. Most importantly, this gives Freetype direct access to
 * files in the romfs
 */
static FT_ULong
FF_stream_read(FT_Stream str, unsigned long offset, unsigned char *buffer,
               unsigned long count)
{
    stream *ps = (stream *) str->descriptor.pointer;
    unsigned int rlen = 0;
    int status = 0;

    if (sseek(ps, (gs_offset_t)offset) < 0)
        return_error(-1);

    if (count) {
        status = sgets(ps, buffer, count, &rlen);

        if (status < 0 && status != EOFC)
            return_error (-1);
    }
    return (rlen);
}

static void
FF_stream_close(FT_Stream str)
{
    stream *ps = (stream *) str->descriptor.pointer;
    gs_memory_t *mem = ps->memory;

    (void)sclose(ps);
    gs_free_object(mem, ps, "FF_stream_close");
}

extern const uint file_default_buffer_size;

static int
FF_open_read_stream(gs_memory_t * mem, char *fname, FT_Stream * fts)
{
    int code = 0;
    gs_parsed_file_name_t pfn;
    stream *ps = (stream *)NULL;
    gs_offset_t length;
    FT_Stream ftstrm = NULL;

    code = gs_parse_file_name(&pfn, (const char *)fname, strlen(fname), mem);
    if (code < 0) {
        goto error_out;
    }

    if (!pfn.fname) {
        code = gs_error_undefinedfilename;
        goto error_out;
    }

    if (pfn.iodev == NULL) {
        pfn.iodev = iodev_default(mem);
    }

    if (pfn.iodev) {
        gx_io_device *const iodev = pfn.iodev;

        iodev_proc_open_file((*open_file)) = iodev->procs.open_file;

        if (open_file) {
            code = open_file(iodev, pfn.fname, pfn.len, "r", &ps, mem);
            if (code < 0) {
                goto error_out;
            }
        }
        else {
            code =
                file_open_stream(pfn.fname, pfn.len, "r",
                                 file_default_buffer_size, &ps, pfn.iodev,
                                 pfn.iodev->procs.gp_fopen, mem);
            if (code < 0) {
                goto error_out;
            }
        }
    }
    else {
        goto error_out;
    }

    if ((code = savailable(ps, &length)) < 0) {
        goto error_out;
    }

    ftstrm = gs_malloc(mem, sizeof(FT_StreamRec), 1, "FF_open_read_stream");
    if (!ftstrm) {
        code = gs_error_VMerror;
        goto error_out;
    }
    memset(ftstrm, 0x00, sizeof(FT_StreamRec));

    ftstrm->descriptor.pointer = ps;
    ftstrm->read = FF_stream_read;
    ftstrm->close = FF_stream_close;
    ftstrm->size = (long)length;
    *fts = ftstrm;

  error_out:
    if (code < 0) {
        if (ps)
            (void)sclose(ps);
        if (ftstrm)
            gs_free(mem, ftstrm, 0, 0, "FF_open_read_stream");
    }
    return (code);
}


static ff_face *
new_face(gs_fapi_server * a_server, FT_Face a_ft_face,
         FT_Incremental_InterfaceRec * a_ft_inc_int, FT_Stream ftstrm,
         unsigned char *a_font_data, int a_font_data_len, bool data_owned)
{
    ff_server *s = (ff_server *) a_server;

    ff_face *face = (ff_face *) FF_alloc(s->ftmemory, sizeof(ff_face));

    if (face) {
        face->ft_face = a_ft_face;
        face->ft_inc_int = a_ft_inc_int;
        face->font_data = a_font_data;
        face->font_data_len = a_font_data_len;
        face->data_owned = data_owned;
        face->ftstrm = ftstrm;
        face->server = (ff_server *) a_server;
    }
    return face;
}

static void
delete_face(gs_fapi_server * a_server, ff_face * a_face)
{
    if (a_face) {
        ff_server *s = (ff_server *) a_server;
        if (a_face->ft_inc_int) {
            FT_Incremental a_info = a_face->ft_inc_int->object;

            if (a_info->glyph_data) {
                gs_free(s->mem, a_info->glyph_data, 0, 0, "delete_face");
            }
            a_info->glyph_data = NULL;
            a_info->glyph_data_length = 0;
            delete_inc_int(a_server, a_face->ft_inc_int);
            a_face->ft_inc_int = NULL;
        }
        FT_Done_Face(a_face->ft_face);

        FF_free(s->ftmemory, a_face->ft_inc_int);
        if (a_face->data_owned)
            FF_free(s->ftmemory, a_face->font_data);
        if (a_face->ftstrm) {
            FF_free(s->ftmemory, a_face->ftstrm);
        }
        FF_free(s->ftmemory, a_face);
    }
}

static FT_IncrementalRec *
new_inc_int_info(gs_fapi_server * a_server, gs_fapi_font * a_fapi_font)
{
    ff_server *s = (ff_server *) a_server;

    FT_IncrementalRec *info =
        (FT_IncrementalRec *) FF_alloc(s->ftmemory,
                                       sizeof(FT_IncrementalRec));
    if (info) {
        info->fapi_font = a_fapi_font;
        info->glyph_data = NULL;
        info->glyph_data_length = 0;
        info->glyph_data_in_use = false;
        info->glyph_metrics_index = 0xFFFFFFFF;
        info->metrics_type = gs_fapi_metrics_notdef;
    }
    return info;
}

static void
delete_inc_int_info(gs_fapi_server * a_server,
                    FT_IncrementalRec * a_inc_int_info)
{
    ff_server *s = (ff_server *) a_server;

    if (a_inc_int_info) {
        FF_free(s->ftmemory, a_inc_int_info->glyph_data);
        FF_free(s->ftmemory, a_inc_int_info);
    }
}

static FT_Error
get_fapi_glyph_data(FT_Incremental a_info, FT_UInt a_index, FT_Data * a_data)
{
    gs_fapi_font *ff = a_info->fapi_font;
    int length = 0;
    ff_face *face = (ff_face *) ff->server_font_data;
    gs_memory_t *mem = (gs_memory_t *) face->server->mem;

    /* Tell the FAPI interface that we need to decrypt the glyph data. */
    ff->need_decrypt = true;

    /* If glyph_data is already in use (as will happen for composite glyphs)
     * create a new buffer on the heap.
     */
    if (a_info->glyph_data_in_use) {
        unsigned char *buffer = NULL;

        length = ff->get_glyph(ff, a_index, NULL, 0);
        if (length == gs_fapi_glyph_invalid_format
            || length == gs_fapi_glyph_invalid_index)
            return FT_Err_Invalid_Glyph_Index;

        buffer = gs_malloc(mem, length, 1, "get_fapi_glyph_data");
        if (!buffer)
            return FT_Err_Out_Of_Memory;

        length = ff->get_glyph(ff, a_index, buffer, length);
        if (length == gs_fapi_glyph_invalid_format) {
            gs_free((gs_memory_t *) mem, buffer, 0, 0,
                    "get_fapi_glyph_data");
            return FT_Err_Invalid_Glyph_Index;
        }
        a_data->pointer = buffer;
    }
    else {
        /* Save ff->char_data, which is set to null by FAPI_FF_get_glyph as part of a hack to
         * make the deprecated Type 2 endchar ('seac') work, so that it can be restored
         * if we need to try again with a longer buffer.
         */
        const void *saved_char_data = ff->char_data;

        /* Get as much of the glyph data as possible into the buffer */
        length =
            ff->get_glyph(ff, a_index, a_info->glyph_data,
                          (ushort) a_info->glyph_data_length);
        if (length == gs_fapi_glyph_invalid_format) {
            ff->char_data = saved_char_data;
            return FT_Err_Unknown_File_Format;
        }

        if (length == gs_fapi_glyph_invalid_index) {
            ff->char_data = saved_char_data;
            return FT_Err_Invalid_Glyph_Index;
        }

        /* If the buffer was too small enlarge it and try again. */
        if (length > a_info->glyph_data_length) {
            if (a_info->glyph_data) {
                gs_free((gs_memory_t *) mem,
                        a_info->glyph_data, 0, 0, "get_fapi_glyph_data");
            }

            a_info->glyph_data =
                gs_malloc(mem, length, 1, "get_fapi_glyph_data");

            if (!a_info->glyph_data) {
                a_info->glyph_data_length = 0;
                return FT_Err_Out_Of_Memory;
            }
            a_info->glyph_data_length = length;
            ff->char_data = saved_char_data;
            length = ff->get_glyph(ff, a_index, a_info->glyph_data, length);
            if (length == gs_fapi_glyph_invalid_format)
                return FT_Err_Unknown_File_Format;
            if (length == gs_fapi_glyph_invalid_index)
                return FT_Err_Invalid_Glyph_Index;
        }

        /* Set the returned pointer and length. */
        a_data->pointer = a_info->glyph_data;

        a_info->glyph_data_in_use = true;
    }

    a_data->length = length;
    return 0;
}

static void
free_fapi_glyph_data(FT_Incremental a_info, FT_Data * a_data)
{
    gs_fapi_font *ff = a_info->fapi_font;
    ff_face *face = (ff_face *) ff->server_font_data;
    gs_memory_t *mem = (gs_memory_t *) face->server->mem;

    if (a_data->pointer == (const FT_Byte *)a_info->glyph_data)
        a_info->glyph_data_in_use = false;
    else
        gs_free(mem, (FT_Byte *) a_data->pointer, 0, 0, "free_fapi_glyph_data");
}

static FT_Error
get_fapi_glyph_metrics(FT_Incremental a_info, FT_UInt a_glyph_index,
                       FT_Bool bVertical,
                       FT_Incremental_MetricsRec * a_metrics)
{
    /* FreeType will create synthetic vertical metrics, including a vertical
     * advance, if none is present. We don't want this, so if the font uses Truetype outlines
     * and the WMode is not 1 (vertical) we ignore the advance by setting it to 0
     */
    if (bVertical && !a_info->fapi_font->is_type1)
        a_metrics->advance = 0;

    if (a_info->glyph_metrics_index == a_glyph_index) {
        switch (a_info->metrics_type) {
            case gs_fapi_metrics_add:
                a_metrics->advance += a_info->glyph_metrics.advance;
                break;
            case gs_fapi_metrics_replace_width:
                a_metrics->advance = a_info->glyph_metrics.advance;
                break;
            case gs_fapi_metrics_replace:
                *a_metrics = a_info->glyph_metrics;
                /* We are replacing the horizontal metrics, so the vertical must be 0 */
                a_metrics->advance_v = 0;
                break;
            default:
                /* This can't happen. */
                return FT_Err_Invalid_Argument;
        }
    }
    return 0;
}

static const FT_Incremental_FuncsRec TheFAPIIncrementalInterfaceFuncs = {
    get_fapi_glyph_data,
    free_fapi_glyph_data,
    get_fapi_glyph_metrics
};

static FT_Incremental_InterfaceRec *
new_inc_int(gs_fapi_server * a_server, gs_fapi_font * a_fapi_font)
{
    ff_server *s = (ff_server *) a_server;

    FT_Incremental_InterfaceRec *i =
        (FT_Incremental_InterfaceRec *) FF_alloc(s->ftmemory,
                                                 sizeof
                                                 (FT_Incremental_InterfaceRec));
    if (i) {
        i->funcs = &TheFAPIIncrementalInterfaceFuncs;
        i->object = (FT_Incremental) new_inc_int_info(a_server, a_fapi_font);

        if (!i->object) {
            FF_free(s->ftmemory, i);
            i = NULL;
        }
    }
    return i;
}

static void
delete_inc_int(gs_fapi_server * a_server,
               FT_Incremental_InterfaceRec * a_inc_int)
{
    ff_server *s = (ff_server *) a_server;

    if (a_inc_int) {
        delete_inc_int_info(a_server, a_inc_int->object);
        FF_free(s->ftmemory, a_inc_int);
    }
}

/* Convert FreeType error codes to GhostScript ones.
 * Very rudimentary because most don't correspond.
 */
static int
ft_to_gs_error(FT_Error a_error)
{
    if (a_error) {
        if (a_error == FT_Err_Out_Of_Memory)
            return_error(gs_error_VMerror);
        else
            return_error(gs_error_unknownerror);
    }
    return 0;
}

/* Load a glyph and optionally rasterize it. Return its metrics in a_metrics.
 * If a_bitmap is true convert the glyph to a bitmap.
 */
static gs_fapi_retcode
load_glyph(gs_fapi_server * a_server, gs_fapi_font * a_fapi_font,
           const gs_fapi_char_ref * a_char_ref, gs_fapi_metrics * a_metrics,
           FT_Glyph * a_glyph, bool a_bitmap, int max_bitmap)
{
    ff_server *s = (ff_server *) a_server;
    FT_Error ft_error = 0;
    FT_Error ft_error_fb = 1;
    ff_face *face = (ff_face *) a_fapi_font->server_font_data;
    FT_Face ft_face = face->ft_face;
    int index = a_char_ref->char_codes[0];
    FT_Long w;
    FT_Long h;
    FT_Long fflags;
    FT_Int32 load_flags = 0;
    FT_Vector  delta = {0,0};

    /* Save a_fapi_font->char_data, which is set to null by FAPI_FF_get_glyph as part of a hack to
     * make the deprecated Type 2 endchar ('seac') work, so that it can be restored
     * after the first call to FT_Load_Glyph.
     */
    const void *saved_char_data = a_fapi_font->char_data;
    const int saved_char_data_len = a_fapi_font->char_data_len;

    if (s->bitmap_glyph) {
        FT_Bitmap_Done(s->freetype_library, &s->bitmap_glyph->bitmap);
        FF_free(s->ftmemory, s->bitmap_glyph);
        s->bitmap_glyph = NULL;
    }
    if (s->outline_glyph) {
        FT_Outline_Done(s->freetype_library, &s->outline_glyph->outline);
        FF_free(s->ftmemory, s->outline_glyph);
        s->outline_glyph = NULL;
    }

    if (!a_char_ref->is_glyph_index) {
        if (ft_face->num_charmaps)
            index = FT_Get_Char_Index(ft_face, index);
        else {
            /* If there are no character maps and no glyph index, loading the glyph will still work
             * properly if both glyph data and metrics are supplied by the incremental interface.
             * In that case we use a dummy glyph index which will be passed
             * back to FAPI_FF_get_glyph by get_fapi_glyph_data.
             *
             * Type 1 fonts don't use the code and can appear to FreeType to have only one glyph,
             * so we have to set the index to 0.
             *
             * For other font types, FAPI_FF_get_glyph requires the character code
             * when getting data.
             */
            if (a_fapi_font->is_type1)
                index = 0;
            else
                index = a_char_ref->char_codes[0];
        }
    }
    else {
        /* This is a heuristic to try to avoid using the TTF notdef (empty rectangle), and replace it
           with a non-marking glyph instead. This is only required for fonts where we don't use the
           FT incremental interface - when we are using the incremental interface, we handle it in
           our own glyph lookup code.
         */
        if (!a_fapi_font->is_cid && !face->ft_inc_int &&
            (index == 0 ||
            (a_char_ref->client_char_code != gs_no_char &&
            FT_Get_Char_Index(ft_face, a_char_ref->client_char_code) <= 0))) {
            int tmp_ind;

            if ((tmp_ind = FT_Get_Char_Index(ft_face, 32)) > 0) {
                index = tmp_ind;
            }
        }
    }
    /* Refresh the pointer to the FAPI_font held by the incremental interface. */
    if (face->ft_inc_int)
        face->ft_inc_int->object->fapi_font = a_fapi_font;

    /* Store the overriding metrics if they have been supplied. */
    if (face->ft_inc_int && a_char_ref->metrics_type != gs_fapi_metrics_notdef) {

        FT_Incremental_MetricsRec *m = &face->ft_inc_int->object->glyph_metrics;

        m->bearing_x = a_char_ref->sb_x >> 16;
        m->bearing_y = a_char_ref->sb_y >> 16;
        m->advance = a_char_ref->aw_x >> 16;

        face->ft_inc_int->object->glyph_metrics_index = index;

        /* For most font types, the original metrics come directly from the font, and
           what we have here are customized (such as a Matrics dict in Postscript). We
           only want to use the width, in that case, because other metrics can mess up
           the hinting in Freetype. We'll apply custom lsb outselves, using the "delta"
           stuff below.
           The exception here is PCL/XL embedded TTF fonts, where the h/vmtx tables can
           be missing, and we *have* to use the explicit metrics from the PCL/XL glyph
           data. (NOTE: if those do not match the original font's metrics, again, the hinting
           can be distorted)
         */
        if (a_char_ref->metrics_type == gs_fapi_metrics_replace && !a_fapi_font->is_mtx_skipped) {
            face->ft_inc_int->object->glyph_metrics_index = 0xFFFFFFFF;
            delta.x = FT_MulFix(a_char_ref->sb_x >> 16, ft_face->size->metrics.x_scale);
            delta.y = FT_MulFix(a_char_ref->sb_y >> 16, ft_face->size->metrics.y_scale);
            FT_Vector_Transform( &delta, &face->ft_transform);
        }
        else {
            face->ft_inc_int->object->metrics_type = a_char_ref->metrics_type;
        }
    }
    else if (face->ft_inc_int)
        /* Make sure we don't leave this set to the last value, as we may then use inappropriate metrics values */
        face->ft_inc_int->object->glyph_metrics_index = 0xFFFFFFFF;

    /* We have to load the glyph, scale it correctly, and render it if we need a bitmap. */
    if (!ft_error) {
        /* We disable loading bitmaps because if we allow it then FreeType invents metrics for them, which messes up our glyph positioning */
        /* Also the bitmaps tend to look somewhat different (though more readable) than FreeType's rendering. By disabling them we */
        /* maintain consistency better.  (FT_LOAD_NO_BITMAP) */
        a_fapi_font->char_data = saved_char_data;
        if (!a_fapi_font->is_mtx_skipped && !a_fapi_font->is_type1) {
            /* grid_fit == 1 is the default - use font's native hints
             * with freetype, 1 & 3 are, in practice, the same.
             */

            if (a_server->grid_fit == 0) {
                load_flags = FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT;
            }
            else if (a_server->grid_fit == 2) {
                load_flags = FT_LOAD_FORCE_AUTOHINT;
            }
            load_flags |= FT_LOAD_MONOCHROME | FT_LOAD_NO_BITMAP | FT_LOAD_LINEAR_DESIGN;
        }
        else {
            /* Current FreeType hinting for type 1 fonts is so poor we are actually better off without it (fewer files render incorrectly) (FT_LOAD_NO_HINTING) */
            /* We also need to disable hinting for XL format embedded truetypes */
            load_flags |= FT_LOAD_MONOCHROME | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_LINEAR_DESIGN;
        }

        ft_error = FT_Load_Glyph(ft_face, index, load_flags);
        if (ft_error == FT_Err_Unknown_File_Format) {
            return index + 1;
        }
    }

    if (ft_error == FT_Err_Invalid_Argument
        || ft_error == FT_Err_Invalid_Reference
        || ft_error == FT_Err_Invalid_Glyph_Index
        || (ft_error >= FT_Err_Invalid_Opcode
            && ft_error <= FT_Err_Too_Many_Instruction_Defs)) {

        a_fapi_font->char_data = saved_char_data;

        /* We want to prevent hinting, even for a "tricky" font - it shouldn't matter for the notdef */
        fflags = ft_face->face_flags;
        ft_face->face_flags &= ~FT_FACE_FLAG_TRICKY;
        load_flags |= FT_LOAD_NO_HINTING;
        ft_error = FT_Load_Glyph(ft_face, index, load_flags);

        ft_face->face_flags = fflags;
    }

    if (ft_error == FT_Err_Out_Of_Memory
        || ft_error == FT_Err_Array_Too_Large) {
        return (gs_error_VMerror);
    }

    /* If FT gives us an error, try to fall back to the notdef - if that doesn't work, we'll throw an error over to Ghostscript */
    if (ft_error) {
        gs_string notdef_str;

        notdef_str.data = (byte *)".notdef";
        notdef_str.size = 7;

        a_fapi_font->char_data = (void *)(&notdef_str);
        a_fapi_font->char_data_len = 0;

        /* We want to prevent hinting, even for a "tricky" font - it shouldn't matter for the notdef */
        fflags = ft_face->face_flags;
        ft_face->face_flags &= ~FT_FACE_FLAG_TRICKY;

        ft_error_fb = FT_Load_Glyph(ft_face, 0, load_flags);

        ft_face->face_flags = fflags;

        a_fapi_font->char_data = saved_char_data;
        a_fapi_font->char_data_len = saved_char_data_len;
    }

    if ((!ft_error || !ft_error_fb) && (delta.x != 0 || delta.y != 0)) {
        FT_Outline_Translate( &ft_face->glyph->outline, delta.x, delta.y);
    }

    /* Previously we interpreted the glyph unscaled, and derived the metrics from that. Now we only interpret it
     * once, and work out the metrics from the scaled/hinted outline.
     */
    if ((!ft_error || !ft_error_fb) && a_metrics) {
        FT_Long hx;
        FT_Long hy;
        FT_Long vadv;

        /* In order to get the metrics in the form we need them, we have to remove the size scaling
         * the resolution scaling, and convert to points.
         */
        hx = (FT_Long) (((double)ft_face->glyph->metrics.horiBearingX *
                         ft_face->units_per_EM * 72.0) /
                        ((double)face->width * face->horz_res))  + (a_fapi_font->is_mtx_skipped == 1 ? 0 : a_char_ref->sb_x >> 16);
        hy = (FT_Long) (((double)ft_face->glyph->metrics.horiBearingY *
                         ft_face->units_per_EM * 72.0) /
                        ((double)face->height * face->vert_res)) + (a_fapi_font->is_mtx_skipped == 1 ? 0 : a_char_ref->sb_y >> 16);

        w = (FT_Long) (((double)ft_face->glyph->metrics.width *
                        ft_face->units_per_EM * 72.0) / ((double)face->width *
                                                         face->horz_res));
        h = (FT_Long) (((double)ft_face->glyph->metrics.height *
                        ft_face->units_per_EM * 72.0) /
                       ((double)face->height * face->vert_res));

        /* Ugly. FreeType creates verticla metrics for TT fonts, normally we override them in the
         * metrics callbacks, but those only work for incremental interface fonts, and TrueType fonts
         * loaded as CIDFont replacements are not incrementally handled. So here, if its a CIDFont, and
         * its not type 1 outlines, and its not a vertical mode fotn, ignore the advance.
         */
        if (a_fapi_font->is_type1
           || ((a_fapi_font->full_font_buf || a_fapi_font->font_file_path)
           && a_fapi_font->is_vertical &&  FT_HAS_VERTICAL(ft_face))) {

            vadv = ft_face->glyph->linearVertAdvance;
        }
        else {
            vadv = 0;
        }

        a_metrics->bbox_x0 = hx;
        a_metrics->bbox_y0 = hy - h;
        a_metrics->bbox_x1 = a_metrics->bbox_x0 + w;
        a_metrics->bbox_y1 = a_metrics->bbox_y0 + h;
        a_metrics->escapement = ft_face->glyph->linearHoriAdvance;
        a_metrics->v_escapement = vadv;
        a_metrics->em_x = ft_face->units_per_EM;
        a_metrics->em_y = ft_face->units_per_EM;
    }

    if ((!ft_error || !ft_error_fb)) {

        FT_BBox cbox;

        /* compute the control box, and grid fit it - lifted from ft_raster1_render() */
        FT_Outline_Get_CBox(&ft_face->glyph->outline, &cbox);

        /* These round operations are only to preserve behaviour compared to the 9.00 release
           which used the bitmap dimensions as calculated by Freetype.
           But FT_PIX_FLOOR/FT_PIX_CEIL aren't public.
         */
        cbox.xMin = ((cbox.xMin) & ~63);        /* FT_PIX_FLOOR( cbox.xMin ) */
        cbox.yMin = ((cbox.yMin) & ~63);
        cbox.xMax = (((cbox.xMax) + 63) & ~63);
        cbox.yMax = (((cbox.yMax) + 63) & ~63); /* FT_PIX_CEIL( cbox.yMax ) */

        w = (FT_UInt) ((cbox.xMax - cbox.xMin) >> 6);
        h = (FT_UInt) ((cbox.yMax - cbox.yMin) >> 6);

        if (!a_fapi_font->metrics_only && a_bitmap == true && ft_face->glyph->format != FT_GLYPH_FORMAT_BITMAP
            && ft_face->glyph->format != FT_GLYPH_FORMAT_COMPOSITE) {
            if ((bitmap_raster(w) * h) < max_bitmap) {
                FT_Render_Mode mode = FT_RENDER_MODE_MONO;

                ft_error = FT_Render_Glyph(ft_face->glyph, mode);
                if (ft_error != 0) {
                    (*a_glyph) = NULL;
                    return (gs_error_VMerror);
                }
            }
            else {
                (*a_glyph) = NULL;
                return (gs_error_VMerror);
            }
        }
    }

    if (!a_fapi_font->metrics_only) {
        /* The following works around the fact that at the scales we deal with
         * these values may not fit in a 16.16 fixed point value, and thus cause
         * freetype to error due to overflow - but we don't use these values
         * and neither does freetype, we can set them to zero and avoid the error
         */
        ft_face->glyph->advance.x = ft_face->glyph->advance.y = 0;
        if ((!ft_error || !ft_error_fb) && a_glyph) {
            ft_error = FT_Get_Glyph(ft_face->glyph, a_glyph);
        }
        else {
            if (ft_face->glyph->format == FT_GLYPH_FORMAT_BITMAP) {
                FT_BitmapGlyph bmg;

                ft_error = FT_Get_Glyph(ft_face->glyph, (FT_Glyph *) & bmg);
                if (!ft_error) {
                    FT_Bitmap_Done(s->freetype_library, &bmg->bitmap);
                    FF_free(s->ftmemory, bmg);
                }
            }
            else if (ft_face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
                FT_OutlineGlyph olg;

                ft_error = FT_Get_Glyph(ft_face->glyph, (FT_Glyph *) & olg);
                if (!ft_error) {
                    FT_Outline_Done(s->freetype_library, &olg->outline);
                    FF_free(s->ftmemory, olg);
                }
            }
        }
    }

    if (ft_error == FT_Err_Too_Many_Hints) {
#ifdef DEBUG
        if (gs_debug_c('1')) {
            ft_emprintf1(a_fapi_font->memory,
                      "TrueType glyph %"PRId64" uses more instructions than the declared maximum in the font.",
                      a_char_ref->char_codes[0]);

            if (!ft_error_fb) {
                ft_emprintf(a_fapi_font->memory,
                         " Continuing, falling back to notdef\n\n");
            }
        }
#endif
        if (!ft_error_fb)
            ft_error = 0;
    }
    if (ft_error == FT_Err_Invalid_Argument) {
#ifdef DEBUG
        if (gs_debug_c('1')) {
            ft_emprintf1(a_fapi_font->memory,
                      "TrueType parsing error in glyph %"PRId64" in the font.",
                      a_char_ref->char_codes[0]);

            if (!ft_error_fb) {
                ft_emprintf(a_fapi_font->memory,
                         " Continuing, falling back to notdef\n\n");
            }
        }
#endif
        if (!ft_error_fb)
            ft_error = 0;
    }
    if (ft_error == FT_Err_Too_Many_Function_Defs) {
#ifdef DEBUG
        if (gs_debug_c('1')) {
            ft_emprintf1(a_fapi_font->memory,
                      "TrueType instruction error in glyph %"PRId64" in the font.",
                      a_char_ref->char_codes[0]);

            if (!ft_error_fb) {
                ft_emprintf(a_fapi_font->memory,
                         " Continuing, falling back to notdef\n\n");
            }
        }
#endif
        if (!ft_error_fb)
            ft_error = 0;
    }
    if (ft_error == FT_Err_Invalid_Glyph_Index) {
#ifdef DEBUG
        if (gs_debug_c('1')) {
            ft_emprintf1(a_fapi_font->memory,
                      "FreeType is unable to find the glyph %"PRId64" in the font.",
                      a_char_ref->char_codes[0]);

            if (!ft_error_fb) {
                ft_emprintf(a_fapi_font->memory,
                         " Continuing, falling back to notdef\n\n");
            }
        }
#endif
        if (!ft_error_fb)
            ft_error = 0;
    }
    return ft_to_gs_error(ft_error);
}

/*
 * Ensure that the rasterizer is open.
 *
 * In the case of FreeType this means creating the FreeType library object.
 */
static gs_fapi_retcode
gs_fapi_ft_ensure_open(gs_fapi_server * a_server, const char * server_param,
                       int server_param_size)
{
    ff_server *s = (ff_server *) a_server;
    FT_UInt tt_ins_version = TT_INTERPRETER_VERSION_35;
    FT_Error ft_error;

    if (s->freetype_library)
        return 0;

    /* As we want FT to use our memory management, we cannot use the convenience of
     * FT_Init_FreeType(), we have to do each stage "manually"
     */
    s->ftmemory->user = s->mem;
    s->ftmemory->alloc = FF_alloc;
    s->ftmemory->free = FF_free;
    s->ftmemory->realloc = FF_realloc;

    ft_error = FT_New_Library(s->ftmemory, &s->freetype_library);
    if (ft_error)
        return ft_to_gs_error(ft_error);

    FT_Add_Default_Modules(s->freetype_library);
    FT_Property_Set( s->freetype_library, "truetype", "interpreter-version", &tt_ins_version);

    return 0;
}

#if 0                           /* Not currently used */
static void
transform_concat(FT_Matrix * a_A, const FT_Matrix * a_B)
{
    FT_Matrix result = *a_B;

    FT_Matrix_Multiply(a_A, &result);
    *a_A = result;
}

/* Create a transform representing an angle defined as a vector. */
static void
make_rotation(FT_Matrix * a_transform, const FT_Vector * a_vector)
{
    FT_Fixed length, cos, sin;

    if (a_vector->x >= 0 && a_vector->y == 0) {
        a_transform->xx = a_transform->yy = 65536;
        a_transform->xy = a_transform->yx = 0;
        return;
    }

    length = FT_Vector_Length((FT_Vector *) a_vector);
    cos = FT_DivFix(a_vector->x, length);
    sin = FT_DivFix(a_vector->y, length);
    a_transform->xx = a_transform->yy = cos;
    a_transform->xy = -sin;
    a_transform->yx = sin;
}
#endif /* Not currently used */

/* Divide a transformation into a scaling part and a rotation-and-shear part.
 * The scaling part is used for setting the pixel size for hinting.
 */
static void
transform_decompose(FT_Matrix * a_transform, FT_UInt * xresp, FT_UInt * yresp,
                    FT_Fixed * a_x_scale, FT_Fixed * a_y_scale, int units_per_EM)
{
    double scalex, scaley, fact = 1.0;
    double factx = 1.0, facty = 1.0;
    FT_Matrix ftscale_mat;
    FT_UInt xres;
    FT_UInt yres;
    /* We have to account for units_per_EM as we fiddle with the scaling
     * in order to avoid underflow (mostly in the TTF hinting code), but
     * we also want to clamp to a lower value (512, admittedly arrived at
     * via experimentation) in order to preserve the fidelity of the outlines.
     */
    double upe = units_per_EM > 512 ? (float)units_per_EM : 512.0;

    scalex = hypot((double)a_transform->xx, (double)a_transform->xy);
    scaley = hypot((double)a_transform->yx, (double)a_transform->yy);

    /* In addition to all the wrangling below, we have to make sure that
     * that the contents of a_transform can also be understood by Freetype.
     */
    if (scalex < 64.0 || scaley < 64.0) {
        factx = 64.0/scalex;
        facty = 64.0/scaley;

        ftscale_mat.xx = (FT_Fixed)(a_transform->xx * factx);
        ftscale_mat.xy = (FT_Fixed)(a_transform->xy * facty);
        ftscale_mat.yx = (FT_Fixed)(a_transform->yx * factx);
        ftscale_mat.yy = (FT_Fixed)(a_transform->yy * facty);
        memcpy(a_transform, &ftscale_mat, sizeof(ftscale_mat));
        scalex = hypot((double)a_transform->xx, (double)a_transform->xy);
        scaley = hypot((double)a_transform->yx, (double)a_transform->yy);
    }

    if (*xresp != *yresp) {
        /* To get good results, we have to pull the implicit scaling from
         * non-square resolutions, and apply it in the matrix. This means
         * we get the correct "shearing" effect for rotated glyphs.
         * The previous solution was only effective for for glyphs whose
         * axes were coincident with the axes of the page.
         */
        bool use_x = true;

        if (*xresp < *yresp) {
            use_x = false;
        }

        ftscale_mat.xx =
            (int)(((double)(*xresp) /
                   ((double)(use_x ? (*xresp) : (*yresp)))) * 65536);
        ftscale_mat.xy = ftscale_mat.yx = 0;
        ftscale_mat.yy =
            (int)(((double)(*yresp) /
                   ((double)(use_x ? (*xresp) : (*yresp)))) * 65536);

        FT_Matrix_Multiply(&ftscale_mat, a_transform);

        xres = yres = (use_x ? (*xresp) : (*yresp));
        xres = (FT_UInt)(xres / factx);
        yres = (FT_UInt)(yres / facty);
    }
    else {
        /* Life is considerably easier when square resolutions are in use! */
        xres = (FT_UInt)(*xresp / factx);
        yres = (FT_UInt)(*yresp / facty);
    }

    scalex *= 1.0 / 65536.0;
    scaley *= 1.0 / 65536.0;

    if (scalex < scaley) {
        scaley = scalex;
    }
    else if (scalex > scaley) {
        scalex = scaley;
    }

    /* FT clamps the width and height to a lower limit of 1.0 units
     * (note: as FT stores it in 64ths of a unit, that is 64)
     * So if either the width or the height are <1.0 here, we scale
     * the width and height appropriately, and then compensate using
     * the "final" matrix for FT
     */
    /* We use 1 1/64th to calculate the scale, so that we *guarantee* the
     * scalex/y we calculate will be >64 after rounding.
     */

    if (scalex < 1.0) {
        fact = 1.016 / scalex;
        scalex = scalex * fact;
        scaley = scaley * fact;
    }

    /* see above */
    if (scalex * xres < 2268.0 / 64.0) {
        fact = (2400.0 / 64.0) / (xres * scalex);
        scaley *= fact;
        scalex *= fact;
    }

    /* see above */
    fact = 1.0;
    while (scaley * yres > (double)upe * 72.0 && (xres > 0 && yres > 0)
           && (scalex > 0.0 && scaley > 0.0)) {
        if (scaley < yres) {
            xres >>= 1;
            yres >>= 1;
            fact *= 2.0;
        }
        else {
            scalex /= 1.25;
            scaley /= 1.25;
        }
    }

    ftscale_mat.xx = (FT_Fixed) ((65536.0 / scalex) * fact);
    ftscale_mat.xy = 0;
    ftscale_mat.yx = 0;
    ftscale_mat.yy = (FT_Fixed) ((65536.0 / scaley) * fact);

    FT_Matrix_Multiply(a_transform, &ftscale_mat);
    memcpy(a_transform, &ftscale_mat, sizeof(FT_Matrix));

    *xresp = xres;
    *yresp = yres;
    /* Return values ready scaled for FT */
    *a_x_scale = (FT_Fixed) (scalex * 64);
    *a_y_scale = (FT_Fixed) (scaley * 64);
}

/*
 * Open a font and set its size.
 */
static gs_fapi_retcode
gs_fapi_ft_get_scaled_font(gs_fapi_server * a_server, gs_fapi_font * a_font,
                const gs_fapi_font_scale * a_font_scale,
                const char *a_map, gs_fapi_descendant_code a_descendant_code)
{
    ff_server *s = (ff_server *) a_server;
    ff_face *face = (ff_face *) a_font->server_font_data;
    FT_Error ft_error = 0;
    int i, j, code;
    FT_CharMap cmap = NULL;
    bool data_owned = true;

    if (s->bitmap_glyph) {
        FT_Bitmap_Done(s->freetype_library, &s->bitmap_glyph->bitmap);
        FF_free(s->ftmemory, s->bitmap_glyph);
        s->bitmap_glyph = NULL;
    }
    if (s->outline_glyph) {
        FT_Outline_Done(s->freetype_library, &s->outline_glyph->outline);
        FF_free(s->ftmemory, s->outline_glyph);
        s->outline_glyph = NULL;
    }

    /* dpf("gs_fapi_ft_get_scaled_font enter: is_type1=%d is_cid=%d font_file_path='%s' a_descendant_code=%d\n",
       a_font->is_type1, a_font->is_cid, a_font->font_file_path ? a_font->font_file_path : "", a_descendant_code); */

    /* If this font is the top level font of an embedded CID type 0 font (font type 9)
     * do nothing. See the comment in FAPI_prepare_font. The descendant fonts are
     * passed in individually.
     */
    if (a_font->is_cid && a_font->is_type1 && a_font->font_file_path == NULL
        && (a_descendant_code == gs_fapi_toplevel_begin
            || a_descendant_code == gs_fapi_toplevel_complete)) {
        /* dpf("gs_fapi_ft_get_scaled_font return 0\n"); */
        return 0;
    }

    /* Create the face if it doesn't already exist. */
    if (!face) {
        FT_Face ft_face = NULL;
        FT_Parameter ft_param;
        FT_Incremental_InterfaceRec *ft_inc_int = NULL;
        unsigned char *own_font_data = NULL;
        int own_font_data_len = -1;
        FT_Stream ft_strm = NULL;

        /* dpf("gs_fapi_ft_get_scaled_font creating face\n"); */

        if (a_font->full_font_buf) {

            own_font_data =
                gs_malloc(((gs_memory_t *) (s->ftmemory->user)),
                          a_font->full_font_buf_len, 1,
                          "gs_fapi_ft_get_scaled_font - full font buf");
            if (!own_font_data) {
                return_error(gs_error_VMerror);
            }

            own_font_data_len = a_font->full_font_buf_len;
            memcpy(own_font_data, a_font->full_font_buf,
                   a_font->full_font_buf_len);

            ft_error =
                FT_New_Memory_Face(s->freetype_library,
                                   (const FT_Byte *)own_font_data,
                                   own_font_data_len, a_font->subfont,
                                   &ft_face);

            if (ft_error) {
                gs_memory_t * mem = (gs_memory_t *) s->ftmemory->user;
                gs_free(mem, own_font_data, 0, 0, "FF_open_read_stream");
                return ft_to_gs_error(ft_error);
            }
        }
        /* Load a typeface from a file. */
        else if (a_font->font_file_path) {
            FT_Open_Args args;

            memset(&args, 0x00, sizeof(args));

            if ((code =
                 FF_open_read_stream((gs_memory_t *) (s->ftmemory->user),
                                     (char *)a_font->font_file_path,
                                     &ft_strm)) < 0) {
                return (code);
            }

            args.flags = FT_OPEN_STREAM;
            args.stream = ft_strm;

            ft_error =
                FT_Open_Face(s->freetype_library, &args, a_font->subfont,
                             &ft_face);
            if (ft_error) {
                /* in the event of an error, Freetype should cleanup the stream */
                return ft_to_gs_error(ft_error);
            }
        }

        /* Load a typeface from a representation in GhostScript's memory. */
        else {
            FT_Open_Args open_args;

            open_args.flags = FT_OPEN_MEMORY;
            open_args.stream = NULL;

            if (a_font->is_type1) {
                long length;
                unsigned short type = 0;

                code = a_font->get_word(a_font, gs_fapi_font_feature_FontType, 0, &type);
                if (code < 0) {
                    return code;
                }

                /* Tell the FAPI interface that we need to decrypt the /Subrs data. */
                a_font->need_decrypt = true;

                /*
                   Serialise a type 1 font in PostScript source form, or
                   a Type 2 font in binary form, so that FreeType can read it.
                 */
                if (type == 1)
                    length = gs_fapi_serialize_type1_font(a_font, NULL, 0);
                else
                    length = gs_fapi_serialize_type2_font(a_font, NULL, 0);

                open_args.memory_base = own_font_data =
                    FF_alloc(s->ftmemory, length);
                if (!open_args.memory_base)
                    return_error(gs_error_VMerror);
                own_font_data_len = length;
                if (type == 1)
                    open_args.memory_size =
                        gs_fapi_serialize_type1_font(a_font, own_font_data, length);
                else
                    open_args.memory_size =
                        gs_fapi_serialize_type2_font(a_font, own_font_data, length);

                if (open_args.memory_size != length)
                    return_error(gs_error_unregistered);        /* Must not happen. */

                ft_inc_int = new_inc_int(a_server, a_font);
                if (!ft_inc_int) {
                    FF_free(s->ftmemory, own_font_data);
                    return_error(gs_error_VMerror);
                }
            }

            /* It must be type 42 (see code in FAPI_FF_get_glyph in zfapi.c). */
            else {
                /* Get the length of the TrueType data. */

                if (a_font->retrieve_tt_font != NULL) {
                    unsigned int ms;
                    code = a_font->retrieve_tt_font(a_font, (void **)&own_font_data, &ms);
                    if (code == 0) {
                        data_owned = false;
                        open_args.memory_base = own_font_data;
                        open_args.memory_size = own_font_data_len = ms;
                    }
                }
                else
                    code = gs_error_unregistered;

                if (code < 0) {
                    unsigned long lms;
                    code = a_font->get_long(a_font, gs_fapi_font_feature_TT_size, 0, &lms);
                    if (code < 0)
                        return code;
                    if (lms == 0)
                        return_error(gs_error_invalidfont);

                    open_args.memory_size = (FT_Long)lms;

                    /* Load the TrueType data into a single buffer. */
                    open_args.memory_base = own_font_data =
                        FF_alloc(s->ftmemory, open_args.memory_size);
                    if (!own_font_data)
                        return_error(gs_error_VMerror);

                    own_font_data_len = open_args.memory_size;

                    code = a_font->serialize_tt_font(a_font, own_font_data,
                                          open_args.memory_size);
                    if (code < 0)
                        return code;
                }

                /* We always load incrementally. */
                ft_inc_int = new_inc_int(a_server, a_font);
                if (!ft_inc_int) {
                    if (data_owned)
                        FF_free(s->ftmemory, own_font_data);
                    return_error(gs_error_VMerror);
                }
            }

            if (ft_inc_int) {
                open_args.flags =
                    (FT_UInt) (open_args.flags | FT_OPEN_PARAMS);
                ft_param.tag = FT_PARAM_TAG_INCREMENTAL;
                ft_param.data = ft_inc_int;
                open_args.num_params = 1;
                open_args.params = &ft_param;
            }
            ft_error =
                FT_Open_Face(s->freetype_library, &open_args, a_font->subfont,
                             &ft_face);
            if (ft_error) {
                delete_inc_int (a_server, ft_inc_int);
                if (data_owned)
                    FF_free(s->ftmemory, own_font_data);
                return ft_to_gs_error(ft_error);
            }
        }

        if (ft_face) {
            face =
                new_face(a_server, ft_face, ft_inc_int, ft_strm,
                         own_font_data, own_font_data_len, data_owned);
            if (!face) {
                if (data_owned)
                    FF_free(s->ftmemory, own_font_data);
                FT_Done_Face(ft_face);
                delete_inc_int(a_server, ft_inc_int);
                return_error(gs_error_VMerror);
            }
            a_font->server_font_data = face;

            if (!a_font->is_type1) {
                for (i = 0; i < GS_FAPI_NUM_TTF_CMAP_REQ && !cmap; i++) {
                    if (a_font->ttf_cmap_req[i].platform_id > 0) {
                        for (j = 0; j < face->ft_face->num_charmaps; j++) {
                            if (FT_Get_CMap_Format(face->ft_face->charmaps[j]) >= 0
                             && face->ft_face->charmaps[j]->platform_id == a_font->ttf_cmap_req[i].platform_id
                             && face->ft_face->charmaps[j]->encoding_id == a_font->ttf_cmap_req[i].encoding_id) {

                                cmap = face->ft_face->charmaps[j];
                                break;
                            }
                        }
                    }
                    else {
                        break;
                    }
                }
                if (cmap) {
                    (void)FT_Set_Charmap(face->ft_face, cmap);
                }
                else if (a_font->full_font_buf != NULL || a_font->font_file_path != NULL) {
                    /* If we've passed a complete TTF to Freetype, but *haven't* requested a
                     * specific cmap table above, try to use a Unicode one
                     * If that doesn't work, just leave the default in place.
                     */
                    (void)FT_Select_Charmap(face->ft_face, ft_encoding_unicode);
                }
                /* For PDF, we have to know which cmap table actually was selected */
                if (face->ft_face->charmap != NULL) {
                    a_font->ttf_cmap_selected.platform_id = face->ft_face->charmap->platform_id;
                    a_font->ttf_cmap_selected.encoding_id = face->ft_face->charmap->encoding_id;
                }
                else {
                    /* Just in case */
                    a_font->ttf_cmap_selected.platform_id = -1;
                    a_font->ttf_cmap_selected.encoding_id = -1;
                }
            }
            else {
                /* Just in case */
                a_font->ttf_cmap_selected.platform_id = -1;
                a_font->ttf_cmap_selected.encoding_id = -1;
            }
        }
        else
            a_font->server_font_data = NULL;
    }

    /* Set the point size and transformation.
     * The matrix is scaled by the shift specified in the server, 16,
     * so we divide by 65536 when converting to a gs_matrix.
     */
    if (face) {
        /* Convert the GS transform into an FT transform.
         * Ignore the translation elements because they contain very large values
         * derived from the current transformation matrix and so are of no use.
         */
        face->ft_transform.xx = a_font_scale->matrix[0];
        face->ft_transform.xy = a_font_scale->matrix[2];
        face->ft_transform.yx = a_font_scale->matrix[1];
        face->ft_transform.yy = a_font_scale->matrix[3];

        face->horz_res = a_font_scale->HWResolution[0] >> 16;
        face->vert_res = a_font_scale->HWResolution[1] >> 16;

        /* Split the transform into scale factors and a rotation-and-shear
         * transform.
         */
        transform_decompose(&face->ft_transform, &face->horz_res,
                            &face->vert_res, &face->width, &face->height, face->ft_face->units_per_EM);

        ft_error = FT_Set_Char_Size(face->ft_face, face->width, face->height,
                                    face->horz_res, face->vert_res);

        if (ft_error) {
            /* The code originally cleaned up the face data here, but the "top level"
               font object still has references to the face data, and we've no way
               to tell it it's gone. So we defer releasing the data until the garbage
               collector collects the font object, and the font's finalize call will
               free the data correctly for us.
             */
            return ft_to_gs_error(ft_error);
        }

        /* Concatenate the transform to a reflection around (y=0) so that it
         * produces a glyph that is upside down in FreeType terms, with its
         * first row at the bottom. That is what GhostScript needs.
         */

        FT_Set_Transform(face->ft_face, &face->ft_transform, NULL);

    }

    /* dpf("gs_fapi_ft_get_scaled_font return %d\n", a_font->server_font_data ? 0 : -1); */
    return a_font->server_font_data ? 0 : -1;
}

/*
 * Return the name of a resource which maps names to character codes. Do this
 * by setting a_decoding_id to point to a null-terminated string. The resource
 * is in the 'decoding' directory in the directory named by /GenericResourceDir
 * in lib/gs_res.ps.
 */
static gs_fapi_retcode
gs_fapi_ft_get_decodingID(gs_fapi_server * a_server, gs_fapi_font * a_font,
               const char **a_decoding_id)
{
    *a_decoding_id = "Unicode";
    return 0;
}

/*
 * Get the font bounding box in font units.
 */
static gs_fapi_retcode
gs_fapi_ft_get_font_bbox(gs_fapi_server * a_server, gs_fapi_font * a_font, int a_box[4], int unitsPerEm[2])
{
    ff_face *face = (ff_face *) a_font->server_font_data;

    a_box[0] = face->ft_face->bbox.xMin;
    a_box[1] = face->ft_face->bbox.yMin;
    a_box[2] = face->ft_face->bbox.xMax;
    a_box[3] = face->ft_face->bbox.yMax;

    unitsPerEm[0] = unitsPerEm[1] = face->ft_face->units_per_EM;

    return 0;
}

/*
 * Return a boolean value in a_proportional stating whether the font is proportional
 * or fixed-width.
 */
static gs_fapi_retcode
gs_fapi_ft_get_font_proportional_feature(gs_fapi_server * a_server,
                              gs_fapi_font * a_font, bool * a_proportional)
{
    *a_proportional = true;
    return 0;
}

/* Convert the character name in a_char_ref.char_name to a character code or
 * glyph index and put it in a_char_ref.char_code, setting
 * a_char_ref.is_glyph_index as appropriate. If this is possible set a_result
 * to true, otherwise set it to false.  The return value is a standard error
 * return code.
 */
static gs_fapi_retcode
gs_fapi_ft_can_retrieve_char_by_name(gs_fapi_server * a_server, gs_fapi_font * a_font,
                          gs_fapi_char_ref * a_char_ref, bool * a_result)
{
    ff_face *face = (ff_face *) a_font->server_font_data;
    char name[128];

    if (FT_HAS_GLYPH_NAMES(face->ft_face)
        && a_char_ref->char_name_length < sizeof(name)) {
        memcpy(name, a_char_ref->char_name, a_char_ref->char_name_length);
        name[a_char_ref->char_name_length] = 0;
        a_char_ref->char_codes[0] = FT_Get_Name_Index(face->ft_face, name);
        *a_result = a_char_ref->char_codes[0] != 0;
        if (*a_result)
            a_char_ref->is_glyph_index = true;
    }
    else
        *a_result = false;
    return 0;
}

/*
 * Return non-zero if the metrics can be replaced.
 */
static gs_fapi_retcode
gs_fapi_ft_can_replace_metrics(gs_fapi_server * a_server, gs_fapi_font * a_font,
                    gs_fapi_char_ref * a_char_ref, int *a_result)
{
    /* Replace metrics only if the metrics are supplied in font units. */
    *a_result = 1;
    return 0;
}

/*
 * Retrieve the metrics of a_char_ref and put them in a_metrics.
 */
static gs_fapi_retcode
gs_fapi_ft_get_char_width(gs_fapi_server * a_server, gs_fapi_font * a_font,
               gs_fapi_char_ref * a_char_ref, gs_fapi_metrics * a_metrics)
{
    ff_server *s = (ff_server *) a_server;

    return load_glyph(a_server, a_font, a_char_ref, a_metrics,
                      (FT_Glyph *) & s->outline_glyph,
                      false, a_server->max_bitmap);
}

static gs_fapi_retcode
gs_fapi_ft_get_fontmatrix(gs_fapi_server * server, gs_matrix * m)
{
    m->xx = 1.0;
    m->xy = 0.0;
    m->yx = 0.0;
    m->yy = 1.0;
    m->tx = 0.0;
    m->ty = 0.0;
    return 0;
}

/*
 * Rasterize the character a_char and return its metrics. Do not return the
 * bitmap but store this. It can be retrieved by a subsequent call to
 * gs_fapi_ft_get_char_raster.
 */
static gs_fapi_retcode
gs_fapi_ft_get_char_raster_metrics(gs_fapi_server * a_server, gs_fapi_font * a_font,
                        gs_fapi_char_ref * a_char_ref,
                        gs_fapi_metrics * a_metrics)
{
    ff_server *s = (ff_server *) a_server;
    gs_fapi_retcode error =
        load_glyph(a_server, a_font, a_char_ref, a_metrics,
                   (FT_Glyph *) & s->bitmap_glyph, true,
                   a_server->max_bitmap);
    return error;
}

/*
 * Return the bitmap created by the last call to gs_fapi_ft_get_char_raster_metrics.
 */
static gs_fapi_retcode
gs_fapi_ft_get_char_raster(gs_fapi_server * a_server, gs_fapi_raster * a_raster)
{
    ff_server *s = (ff_server *) a_server;

    if (!s->bitmap_glyph)
        return(gs_error_unregistered);
    a_raster->p = s->bitmap_glyph->bitmap.buffer;
    a_raster->width = s->bitmap_glyph->bitmap.width;
    a_raster->height = s->bitmap_glyph->bitmap.rows;
    a_raster->line_step = s->bitmap_glyph->bitmap.pitch;
    a_raster->orig_x = s->bitmap_glyph->left * 16;
    a_raster->orig_y = s->bitmap_glyph->top * 16;
    a_raster->left_indent = a_raster->top_indent = a_raster->black_height =
        a_raster->black_width = 0;
    return 0;
}

/*
 * Create an outline for the character a_char and return its metrics. Do not
 * return the outline but store this.
 * It can be retrieved by a subsequent call to gs_fapi_ft_get_char_outline.
 */
static gs_fapi_retcode
gs_fapi_ft_get_char_outline_metrics(gs_fapi_server * a_server, gs_fapi_font * a_font,
                         gs_fapi_char_ref * a_char_ref,
                         gs_fapi_metrics * a_metrics)
{
    ff_server *s = (ff_server *) a_server;

    return load_glyph(a_server, a_font, a_char_ref, a_metrics,
                      (FT_Glyph *) & s->outline_glyph, false,
                      a_server->max_bitmap);
}

typedef struct FF_path_info_s
{
    gs_fapi_path *path;
    int64_t x;
    int64_t y;
    FT_Vector currentp;
} FF_path_info;

static inline int
FF_points_equal(const FT_Vector *p1, const FT_Vector *p2)
{
    if (p1->x == p2->x && p1->y == p2->y)
        return 1;
    else
        return 0;
}

static int
move_to(const FT_Vector * aTo, void *aObject)
{
    FF_path_info *p = (FF_path_info *) aObject;

    p->currentp = *aTo;

    /* FAPI expects that co-ordinates will be as implied by frac_shift
     * in our case 16.16 fixed precision. True for 'low level' FT
     * routines (apparently), it isn't true for these routines where
     * FT returns a 26.6 format. Rescale to 16.16 so that FAPI will
     * be able to convert to GS co-ordinates properly.
     */
    /* FAPI now expects these coordinates in 32.32 */
    p->x = ((int64_t) aTo->x) << 26;
    p->y = ((int64_t) aTo->y) << 26;

    return p->path->moveto(p->path, p->x, p->y) ? -1 : 0;
}

static int
line_to(const FT_Vector * aTo, void *aObject)
{
    FF_path_info *p = (FF_path_info *) aObject;

    if (!FF_points_equal(&p->currentp, aTo)) {
        p->currentp = *aTo;

        /* See move_to() above */
        p->x = ((int64_t) aTo->x) << 26;
        p->y = ((int64_t) aTo->y) << 26;

        return p->path->lineto(p->path, p->x, p->y) ? -1 : 0;
    }
    return 0;
}

static int
conic_to(const FT_Vector * aControl, const FT_Vector * aTo, void *aObject)
{
    FF_path_info *p = (FF_path_info *) aObject;
    double x, y, Controlx, Controly;
    int64_t Control1x, Control1y, Control2x, Control2y;
    double sx, sy;

    if (!FF_points_equal(&p->currentp, aControl) ||
        !FF_points_equal(&p->currentp, aTo) ||
        !FF_points_equal(aControl, aTo)) {
        p->currentp = *aTo;

        /* More complicated than above, we need to do arithmetic on the
         * co-ordinates, so we want them as floats and we will convert the
         * result into 16.16 fixed precision for FAPI
         *
         * NB this code is funcitonally the same as the original, but I don't believe
         * the comment (below) to be what the code is actually doing....
         *
         * NB2: the comment below was wrong, even though the code was correct(!!)
         * The comment has now been amended.
         *
         * Convert a quadratic spline to a cubic. Do this by changing the three points
         * A, B and C to A, 2/3(B,A), 2/3(B,C), C - that is, the two cubic control points are
         * a third of the way from the single quadratic control point to the end points. This
         * gives the same curve as the original quadratic.
         */

        sx = (double) (p->x >> 32);
        sy = (double) (p->y >> 32);

        x = aTo->x / 64.0;
        p->x = ((int64_t) float2fixed(x)) << 24;
        y = aTo->y / 64.0;
        p->y = ((int64_t) float2fixed(y)) << 24;
        Controlx = aControl->x / 64.0;
        Controly = aControl->y / 64.0;

        Control1x = ((int64_t) float2fixed((sx + Controlx * 2) / 3)) << 24;
        Control1y = ((int64_t) float2fixed((sy + Controly * 2) / 3)) << 24;
        Control2x = ((int64_t) float2fixed((x + Controlx * 2) / 3)) << 24;
        Control2y = ((int64_t) float2fixed((y + Controly * 2) / 3)) << 24;

        return p->path->curveto(p->path, Control1x,
                                Control1y,
                                Control2x, Control2y, p->x, p->y) ? -1 : 0;
    }
    return 0;
}

static int
cubic_to(const FT_Vector * aControl1, const FT_Vector * aControl2,
         const FT_Vector * aTo, void *aObject)
{
    FF_path_info *p = (FF_path_info *) aObject;
    int64_t Control1x, Control1y, Control2x, Control2y;

    if (!FF_points_equal(&p->currentp, aControl1) ||
        !FF_points_equal(&p->currentp, aControl2) ||
        !FF_points_equal(&p->currentp, aTo) ||
        !FF_points_equal(aControl1, aControl2) ||
        !FF_points_equal(aControl1, aTo) ||
        !FF_points_equal(aControl2, aTo)) {
        p->currentp = *aTo;

        /* See move_to() above */
        p->x = ((int64_t) aTo->x) << 26;
        p->y = ((int64_t) aTo->y) << 26;

        Control1x = ((int64_t) aControl1->x) << 26;
        Control1y = ((int64_t) aControl1->y) << 26;
        Control2x = ((int64_t) aControl2->x) << 26;
        Control2y = ((int64_t) aControl2->y) << 26;
        return p->path->curveto(p->path, Control1x, Control1y, Control2x,
                                Control2y, p->x, p->y) ? -1 : 0;
    }
    return 0;
}

static const FT_Outline_Funcs TheFtOutlineFuncs = {
    move_to,
    line_to,
    conic_to,
    cubic_to,
    0,
    0
};

/*
 * Return the outline created by the last call to gs_fapi_ft_get_char_outline_metrics.
 */
static gs_fapi_retcode
gs_fapi_ft_get_char_outline(gs_fapi_server * a_server, gs_fapi_path * a_path)
{
    ff_server *s = (ff_server *) a_server;
    FF_path_info p;
    FT_Error ft_error = 0;

    p.path = a_path;
    p.x = 0;
    p.y = 0;
    /* If we got an error during glyph creation, we can get
     * here with s->outline_glyph == NULL
     */
    if (s->outline_glyph) {
        ft_error =
            FT_Outline_Decompose(&s->outline_glyph->outline, &TheFtOutlineFuncs,
                                 &p);
    }
    else {
        a_path->moveto(a_path, 0, 0);
    }

    if (a_path->gs_error == 0)
        a_path->closepath(a_path);
    return ft_to_gs_error(ft_error);
}

static gs_fapi_retcode
gs_fapi_ft_release_char_data(gs_fapi_server * a_server)
{
    ff_server *s = (ff_server *) a_server;

    if (s->outline_glyph) {
        FT_Outline_Done(s->freetype_library, &s->outline_glyph->outline);
        FF_free(s->ftmemory, s->outline_glyph);
    }

    if (s->bitmap_glyph) {
        FT_Bitmap_Done(s->freetype_library, &s->bitmap_glyph->bitmap);
        FF_free(s->ftmemory, s->bitmap_glyph);
    }

    s->outline_glyph = NULL;
    s->bitmap_glyph = NULL;
    return 0;
}

static gs_fapi_retcode
gs_fapi_ft_release_typeface(gs_fapi_server * a_server, void *a_server_font_data)
{
    ff_face *face = (ff_face *) a_server_font_data;

    delete_face(a_server, face);
    return 0;
}

static gs_fapi_retcode
gs_fapi_ft_check_cmap_for_GID(gs_fapi_server * server, uint * index)
{
    ff_face *face = (ff_face *) (server->ff.server_font_data);
    FT_Face ft_face = face->ft_face;

    *index = FT_Get_Char_Index(ft_face, *index);
    return 0;
}

static gs_fapi_retcode
gs_fapi_ft_set_mm_weight_vector(gs_fapi_server *server, gs_fapi_font *ff, float *wvector, int length)
{
#if defined(SHARE_FT) && SHARE_FT == 1 && \
    FREETYPE_MAJOR <= 2 && FREETYPE_MINOR <= 9 && FREETYPE_PATCH <= 1

    return gs_error_invalidaccess;
#else
    ff_face *face = (ff_face *) ff->server_font_data;
    FT_Fixed nwv[16] = {0};
    FT_Fixed cwv[16] = {0};
    FT_UInt len = 16;
    int i;
    bool setit = false;
    FT_Error ft_error;
    (void)server;

    ft_error = FT_Get_MM_WeightVector(face->ft_face, &len, cwv);
    if (ft_error != 0) return_error(gs_error_invalidaccess);

    for (i = 0; i < length; i++) {
        nwv[i] = (FT_Fixed)(wvector[i] * 65536.0);
        if (nwv[i] != cwv[i]) {
            setit = true;
        }
    }

    if (setit == true) {
        ft_error = FT_Set_MM_WeightVector(face->ft_face, length, nwv);
        if (ft_error != 0) return_error(gs_error_invalidaccess);
    }

    return 0;
#endif
}

static void gs_fapi_freetype_destroy(gs_fapi_server ** serv);

static const gs_fapi_server_descriptor freetypedescriptor = {
    (const char *)"FAPI",
    (const char *)"FreeType",
    gs_fapi_freetype_destroy
};

static const gs_fapi_server freetypeserver = {
    {&freetypedescriptor},
    NULL,                       /* client_ctx_p */
    16,                         /* frac_shift */
    {gs_no_id},
    {0},
    0,
    false,
    false,
    {1, 0, 0, 1, 0, 0},
    1,
    {1, 0, 0, 1, 0, 0},
    gs_fapi_ft_ensure_open,
    gs_fapi_ft_get_scaled_font,
    gs_fapi_ft_get_decodingID,
    gs_fapi_ft_get_font_bbox,
    gs_fapi_ft_get_font_proportional_feature,
    gs_fapi_ft_can_retrieve_char_by_name,
    gs_fapi_ft_can_replace_metrics,
    NULL,                       /* can_simulate_style */
    gs_fapi_ft_get_fontmatrix,
    gs_fapi_ft_get_char_width,
    gs_fapi_ft_get_char_raster_metrics,
    gs_fapi_ft_get_char_raster,
    gs_fapi_ft_get_char_outline_metrics,
    gs_fapi_ft_get_char_outline,
    gs_fapi_ft_release_char_data,
    gs_fapi_ft_release_typeface,
    gs_fapi_ft_check_cmap_for_GID,
    NULL,                        /* get_font_info */
    gs_fapi_ft_set_mm_weight_vector,
};

int gs_fapi_ft_init(gs_memory_t * mem, gs_fapi_server ** server);

int
gs_fapi_ft_init(gs_memory_t * mem, gs_fapi_server ** server)
{
    ff_server *serv;
    int code = 0;
    gs_memory_t *cmem = mem->non_gc_memory;

    code = gs_memory_chunk_wrap(&(cmem), mem);
    if (code != 0) {
        return (code);
    }


    serv = (ff_server *) gs_alloc_bytes_immovable(cmem, sizeof(ff_server), "gs_fapi_ft_init");
    if (!serv) {
        gs_memory_chunk_release(cmem);
        return_error(gs_error_VMerror);
    }
    memset(serv, 0, sizeof(*serv));
    serv->mem = cmem;
    serv->fapi_server = freetypeserver;

    serv->ftmemory = (FT_Memory) (&(serv->ftmemory_rec));

    (*server) = (gs_fapi_server *) serv;
    return (0);
}


void
gs_fapi_freetype_destroy(gs_fapi_server ** serv)
{
    ff_server *server = (ff_server *) * serv;
    gs_memory_t *cmem = server->mem;

    FT_Done_Glyph(&server->outline_glyph->root);
    FT_Done_Glyph(&server->bitmap_glyph->root);

    /* As with initialization: since we're supplying memory management to
     * FT, we cannot just to use FT_Done_FreeType (), we have to use
     * FT_Done_Library () and then discard the memory ourselves
     */
    FT_Done_Library(server->freetype_library);
    gs_free(cmem, *serv, 0, 0, "gs_fapi_freetype_destroy: ff_server");
    *serv = NULL;
    gs_memory_chunk_release(cmem);
}
