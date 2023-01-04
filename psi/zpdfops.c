/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/* Custom operators for PDF interpreter */

#if defined(BUILD_PDF) && BUILD_PDF == 1
#include "ghostpdf.h"
#include "pdf_page.h"
#include "gzht.h"
#include "gsrefct.h"
#include "pdf_misc.h"
#include "pdf_stack.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_loop_detect.h"

#include "iminst.h"
#include "dstack.h"
#include "gsicc_profilecache.h"
#include "pagelist.h"
#endif

#include "ghost.h"
#include "gsmchunk.h"
#include "oper.h"
#include "igstate.h"
#include "istack.h"
#include "iutil.h"
#include "gspath.h"
#include "math_.h"
#include "ialloc.h"
#include "malloc_.h"
#include "string_.h"
#include "store.h"
#include "gxgstate.h"
#include "gxdevsop.h"
#include "idict.h"
#include "iname.h"
#include "bfont.h"

#ifdef HAVE_LIBIDN
#  include <stringprep.h>
#endif

/* ------ Graphics state ------ */

/* <screen_index> <x> <y> .setscreenphase - */
static int
zsetscreenphase(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code;
    int x, y;

    check_type(op[-2], t_integer);
    check_type(op[-1], t_integer);
    check_type(*op, t_integer);
    x = op[-1].value.intval;
    y = op->value.intval;
    if (op[-2].value.intval < -1 ||
        op[-2].value.intval >= gs_color_select_count
        )
        return_error(gs_error_rangecheck);
    code = gs_setscreenphase(igs, x, y,
                             (gs_color_select_t) op[-2].value.intval);
    if (code >= 0)
        pop(3);
    return code;
}

/* Construct a smooth path passing though a number of points  on the stack */
/* for PDF ink annotations. The program is based on a very simple method of */
/* smoothing polygons by Maxim Shemanarev. */
/* http://www.antigrain.com/research/bezier_interpolation/ */
/* <mark> <x0> <y0> ... <xn> <yn> .pdfinkpath - */
static int
zpdfinkpath(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    uint i, count;
    int code;
    double x0, y0, x1, y1, x2, y2, x3, y3, xc1, yc1, xc2, yc2, xc3, yc3;
    double len1, len2, len3, k1, k2, xm1, ym1, xm2, ym2;
    double ctrl1_x, ctrl1_y, ctrl2_x, ctrl2_y;
    const double smooth_value = 1; /* from 0..1 range */
    ref lval;

    check_read_type(*op, t_array);
    count = r_size(op);

    if ((count & 1) != 0 || count < 2)
        return_error(gs_error_rangecheck);

    if ((code = array_get(imemory, op, 0, &lval)) < 0)
        return code;
    if ((code = real_param(&lval, &x1)) < 0)
        return code;

    if ((code = array_get(imemory, op, 1, &lval)) < 0)
        return code;
    if ((code = real_param(&lval, &y1)) < 0)
        return code;

    if ((code = gs_moveto(igs, x1, y1)) < 0)
        return code;
    if (count == 2)
          goto pop;

    if ((code = array_get(imemory, op, 2, &lval)) < 0)
        return code;
    if ((code = real_param(&lval, &x2)) < 0)
        return code;

    if ((code = array_get(imemory, op, 3, &lval)) < 0)
        return code;
    if ((code = real_param(&lval, &y2)) < 0)
        return code;

    if (count == 4) {
        if((code = gs_lineto(igs, x2, y2)) < 0)
            return code;
        goto pop;
    }
    x0 = 2*x1 - x2;
    y0 = 2*y1 - y2;

    for (i = 4; i <= count; i += 2) {
        if (i < count) {
            if ((code = array_get(imemory, op, i, &lval)) < 0)
                return code;
            if ((code = real_param(&lval, &x3)) < 0)
                return code;
            if ((code = array_get(imemory, op, i + 1, &lval)) < 0)
                return code;
            if ((code = real_param(&lval, &y3)) < 0)
                return code;
        } else {
            x3 = 2*x2 - x1;
            y3 = 2*y2 - y1;
        }

        xc1 = (x0 + x1) / 2.0;
        yc1 = (y0 + y1) / 2.0;
        xc2 = (x1 + x2) / 2.0;
        yc2 = (y1 + y2) / 2.0;
        xc3 = (x2 + x3) / 2.0;
        yc3 = (y2 + y3) / 2.0;

        len1 = hypot(x1 - x0, y1 - y0);
        len2 = hypot(x2 - x1, y2 - y1);
        len3 = hypot(x3 - x2, y3 - y2);

        k1 = len1 / (len1 + len2);
        k2 = len2 / (len2 + len3);

        xm1 = xc1 + (xc2 - xc1) * k1;
        ym1 = yc1 + (yc2 - yc1) * k1;

        xm2 = xc2 + (xc3 - xc2) * k2;
        ym2 = yc2 + (yc3 - yc2) * k2;

        ctrl1_x = xm1 + (xc2 - xm1) * smooth_value + x1 - xm1;
        ctrl1_y = ym1 + (yc2 - ym1) * smooth_value + y1 - ym1;

        ctrl2_x = xm2 + (xc2 - xm2) * smooth_value + x2 - xm2;
        ctrl2_y = ym2 + (yc2 - ym2) * smooth_value + y2 - ym2;

        code = gs_curveto(igs, ctrl1_x, ctrl1_y, ctrl2_x, ctrl2_y, x2, y2);
        if (code < 0)
            return code;
        x0 = x1, x1 = x2, x2 = x3;
        y0 = y1, y1 = y2, y2 = y3;
    }
  pop:
    pop(1);
    return 0;
}

static int
zpdfFormName(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    uint count = ref_stack_count(&o_stack);
    int code;

    if (count == 0)
        return_error(gs_error_stackunderflow);
    check_read_type(*op, t_string);

    code = (*dev_proc(i_ctx_p->pgs->device, dev_spec_op))((gx_device *)i_ctx_p->pgs->device,
        gxdso_pdf_form_name, (void *)op->value.const_bytes, r_size(op));

    if (code < 0)
        return code;

    pop(1);
    return 0;
}

#ifdef HAVE_LIBIDN
/* Given a UTF-8 password string, convert it to the canonical form
 * defined by SASLprep (RFC 4013).  This is a permissive implementation,
 * suitable for verifying existing passwords but not for creating new
 * ones -- if you want to create a new password, you'll need to add a
 * strict mode that returns stringprep errors to the user, and uses the
 * STRINGPREP_NO_UNASSIGNED flag to disallow unassigned characters.
 * <string> .saslprep <string> */
static int
zsaslprep(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    uint input_size = r_size(op);
    byte *buffer;
    uint buffer_size;
    uint output_size;
    Stringprep_rc err;

    check_read_type(*op, t_string);

    /* According to http://unicode.org/faq/normalization.html, converting
     * a UTF-8 string to normalization form KC has a worst-case expansion
     * factor of 11, so we allocate 11 times the length of the string plus
     * 1 for the NUL terminator.  If somehow that's still not big enough,
     * stringprep will return STRINGPREP_TOO_SMALL_BUFFER; there's no
     * danger of corrupting memory. */
    buffer_size = input_size * 11 + 1;
    buffer = ialloc_string(buffer_size, "saslprep result");
    if (buffer == 0)
        return_error(gs_error_VMerror);

    memcpy(buffer, op->value.bytes, input_size);
    buffer[input_size] = '\0';

    err = stringprep((char *)buffer, buffer_size, 0, stringprep_saslprep);
    if (err != STRINGPREP_OK) {
        ifree_string(buffer, buffer_size, "saslprep result");

        /* Since we're just verifying the password to an existing
         * document here, we don't care about "invalid input" errors
         * like STRINGPREP_CONTAINS_PROHIBITED.  In these cases, we
         * ignore the error and return the original string unchanged --
         * chances are it's not the right password anyway, and if it
         * is we shouldn't gratuitously fail to decrypt the document.
         *
         * On the other hand, errors like STRINGPREP_NFKC_FAILED are
         * real errors, and should be returned to the user.
         *
         * Fortunately, the stringprep error codes are sorted to make
         * this easy: the errors we want to ignore are the ones with
         * codes less than 100. */
        if ((int)err < 100)
            return 0;

        return_error(gs_error_ioerror);
    }

    output_size = strlen((char *)buffer);
    buffer = iresize_string(buffer, buffer_size, output_size,
        "saslprep result");	/* can't fail */
    make_string(op, a_all | icurrent_space, output_size, buffer);

    return 0;
}
#endif

#if defined(BUILD_PDF) && BUILD_PDF == 1

static int zpdfi_populate_search_paths(i_ctx_t *i_ctx_p, pdf_context *ctx)
{
    int code = 0;
    /* This should only be called once per pdfi context
       if the paths are already populated, just skip it.
     */
    if (ctx->search_paths.resource_paths == NULL) {
        ref *l2dictref, *grdref, *fpathref;
        int i;
        const gs_file_path *pfpath = i_ctx_p->lib_path;
        gs_main_instance *minst = get_minst_from_memory(imemory);
        code = dict_find_string(systemdict, "pssystemparams", &l2dictref);
        if (code >= 0 && r_has_type(l2dictref, t_dictionary)) {
            code = dict_find_string(l2dictref, "GenericResourceDir", &grdref);
            if (code >= 0 && r_has_type(grdref, t_string)) {
                ctx->search_paths.genericresourcedir.data = gs_alloc_bytes(ctx->memory, r_size(grdref), "zpdfi_populate_search_paths");
                if (ctx->search_paths.genericresourcedir.data == NULL) {
                    code = gs_note_error(gs_error_VMerror);
                    goto done;
                }
                memcpy((char *)ctx->search_paths.genericresourcedir.data, grdref->value.const_bytes, r_size(grdref));
                ctx->search_paths.genericresourcedir.size = r_size(grdref);
                ctx->search_paths.genericresourcedir.persistent = false;
            }
        }

        ctx->search_paths.resource_paths = (gs_param_string *)gs_alloc_bytes(ctx->memory, sizeof(gs_param_string) * r_size(&pfpath->list), "array of paths");
        if (ctx->search_paths.resource_paths == NULL) {
            code = gs_note_error(gs_error_VMerror);
            goto done;
        }
        memset(ctx->search_paths.resource_paths, 0x00, sizeof(gs_param_string) * r_size(&pfpath->list));

        ctx->search_paths.num_resource_paths = r_size(&pfpath->list);
        for (i = 0; i < r_size(&pfpath->list); i++) {
            const ref *prdir = pfpath->list.value.refs + i; /* By nature, this cannot be a short/mixed array, only a "normal" array */
            ctx->search_paths.resource_paths[i].data = gs_alloc_bytes(ctx->memory, r_size(prdir), "zpdfi_populate_search_paths");
            if (ctx->search_paths.resource_paths[i].data == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto done;
            }
            memcpy((char *)ctx->search_paths.resource_paths[i].data, prdir->value.const_bytes, r_size(prdir));
            ctx->search_paths.resource_paths[i].size = r_size(prdir);
            ctx->search_paths.resource_paths[i].persistent = false;
        }
        code = dict_find_string(systemdict, "FONTPATH", &fpathref);
        if (code >= 0 && r_has_type(fpathref, t_array)) {
            ctx->search_paths.font_paths = (gs_param_string *)gs_alloc_bytes(ctx->memory, sizeof(gs_param_string) * r_size(fpathref), "array of font paths");
            memset(ctx->search_paths.font_paths, 0x00, sizeof(gs_param_string) * r_size(fpathref));

            ctx->search_paths.num_font_paths = r_size(fpathref);
            for (i = 0; i < r_size(fpathref); i++) {
                const ref *prdir = fpathref->value.refs + i; /* By nature, this cannot be a short/mixed array, only a "normal" array */
                ctx->search_paths.font_paths[i].data = gs_alloc_bytes(ctx->memory, r_size(prdir), "zpdfi_populate_search_paths");
                if (ctx->search_paths.font_paths[i].data == NULL) {
                    code = gs_note_error(gs_error_VMerror);
                    goto done;
                }
                memcpy((char *)ctx->search_paths.font_paths[i].data, prdir->value.const_bytes, r_size(prdir));
                ctx->search_paths.font_paths[i].size = r_size(prdir);
                ctx->search_paths.font_paths[i].persistent = false;
            }
        }
        ctx->search_paths.search_here_first = minst->search_here_first;
    }
done:
    return code >= 0 ? 0 : code;
}

static int zpdfi_populate_fontmap_files(i_ctx_t *i_ctx_p, pdf_context *ctx)
{
    int code, i;
    ref *fontmaps;

    code = dict_find_string(systemdict, "FONTMAP", &fontmaps);
    if (code >= 0 && r_has_type(fontmaps, t_array)) {
        ctx->fontmapfiles = (gs_string *)gs_alloc_bytes(ctx->memory, sizeof(gs_string) * r_size(fontmaps), "array of fontmap files");
        if (ctx->fontmapfiles != 0) {
            memset(ctx->fontmapfiles, 0x00, sizeof(gs_string) * r_size(fontmaps));
            ctx->num_fontmapfiles = r_size(fontmaps);
            for (i = 0; i < r_size(fontmaps); i++) {
                const ref *fmapfile = fontmaps->value.refs + i; /* By nature, this cannot be a short/mixed array, only a "normal" array */
                ctx->fontmapfiles[i].data = gs_alloc_bytes(ctx->memory, r_size(fmapfile), "zpdfi_populate_fontmap_files");
                if (ctx->fontmapfiles[i].data == NULL) {
                    code = gs_note_error(gs_error_VMerror);
                    goto done;
                }
                memcpy((char *)ctx->fontmapfiles[i].data, fmapfile->value.const_bytes, r_size(fmapfile));
                ctx->fontmapfiles[i].size = r_size(fmapfile);
            }
        }
        else {
            code = gs_note_error(gs_error_VMerror);
            goto done;
        }
    }
    else if (code >= 0 && r_has_type(fontmaps, t_string)) {
        ctx->fontmapfiles = (gs_string *)gs_alloc_bytes(ctx->memory, sizeof(gs_string), "array of fontmap files");
        if (ctx->fontmapfiles != 0) {
            ctx->num_fontmapfiles = 1;
            ctx->fontmapfiles[0].data = gs_alloc_bytes(ctx->memory, r_size(fontmaps), "zpdfi_populate_fontmap_files");
            if (ctx->fontmapfiles[0].data == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto done;
            }
            memcpy((char *)ctx->fontmapfiles[0].data, fontmaps->value.const_bytes, r_size(fontmaps));
            ctx->fontmapfiles[0].size = r_size(fontmaps);
        }
        else {
            code = gs_note_error(gs_error_VMerror);
            goto done;
        }
    }
    else
        code = 0;
done:
    return code;
}

/*
 * Declare the structure we use to represent an instance of the PDF parser
 * as a t_struct.
 */
typedef struct pdfctx_s {
    pdf_context *ctx;               /* Not exposed to garbager */
    stream *ps_stream;
    gs_memory_t *pdf_memory;        /* The 'wrapped' memory allocator used by the PDF interpreter. Not exposed to garbager */
    gs_memory_t *pdf_stream_memory; /* The memory allocator used to copy the PostScript stream to pdf_stream. Not exposed to garbager */
    stream *pdf_stream;
    bool UsingPDFFile;
    gsicc_profile_cache_t *profile_cache;
    gs_memory_t *cache_memory;      /* The memory allocator used to allocate the working (GC'ed) profile cache */
} pdfctx_t;

/* There is some unfortunately necessary complication in the structure above regarding the ICC profile cache.
 * Initially the problem was due to adding the colour space name to the 'interpreter data' part of the colour
 * space, in order to be able to identify the case where we switch to the current colour space.
 *
 * This caused problems with ICC spaces, because these maintain a reference to the colour space. If the profile
 * was stored in the PostScript profile cache (which it would be, because the profile cache is stored in the
 * grapchis state, which we inherit from the PostScript interpreter) then it would outlive the PDF interpreter,
 *  and when we came to free the cache it would try to free the colour space, which would try to free the PDF
 * name object, which had already been freed.
 *
 * To work around this we replaced the profile cache in the graphics state with the profile cache from the PDF
 * interpreter. Now when we free the PDF profile cache, it frees the colour spaces, and the PDF names, while they
 * are still valid.
 *
 * However, we then discovered that the garbage collector could reclaim the colour spaces. This happens because
 * no GC-visible object declares it has a reference to the colour space. It doesn't matter that the colour
 * space has been counted up, the GC frees it. Now the profile cache in pdfi is pointing to a colour space which
 * has gone away....
 *
 * To get round this, we create a brand-new profile cache, using GC'ed memory (well actually the interpreter
 * allocator, which is GC'ed for Ghostscript). When we switch to pdfi we replace the graphics state pdfi is
 * using with the one from the PostScript environment, and at the same time we swap out the profile cache
 * being used by that graphics state, so it uses our new one. On return to PostScript we swap them back.
 * This allows the lifetime of the profiles in the profile cache to be correct, because they are visible to the
 * garbage collector (the PDF context is a GC-visible object and enumerates it's member profile cache)
 * When we finalize the PDF context we count down the profile cache which dereferences all its entries. This
 * effectively frees the colour space (the memory may not be reclaimed yet) which calls the interpreter
 * specific cleanup, which frees the (still valid at this point) PDF name object and sets the pointer to NULL.
 *
 * The net result is that we have the profiles visible to the garbage collector during their lifetime, but
 * still have control over the cache, so we can empty it of the profiles created for pdfi when we close the
 * PDF interpreter.
 */

/* Structure descriptors */
static void pdfctx_finalize(const gs_memory_t *cmem, void *vptr);

gs_private_st_composite_final(st_pdfctx_t, pdfctx_t, "pdfctx_struct",\
    pdfctx_enum_ptrs, pdfctx_reloc_ptrs, pdfctx_finalize);

static
ENUM_PTRS_BEGIN(pdfctx_enum_ptrs) return 0;
ENUM_PTR3(0, pdfctx_t, ps_stream, pdf_stream, profile_cache);
ENUM_PTRS_END

static RELOC_PTRS_BEGIN(pdfctx_reloc_ptrs);
RELOC_PTR3(pdfctx_t, ps_stream, pdf_stream, profile_cache);
RELOC_PTRS_END

static void
pdfctx_finalize(const gs_memory_t *cmem, void *vptr)
{
    pdfctx_t *pdfctx = vptr;
    /* Finalize methods have to cope with the possibility of being called multiple times
     *  on the same object - hence we null the entries.
     */

    if (pdfctx->profile_cache != NULL) {
        rc_decrement(pdfctx->profile_cache, "free the working profile cache");
        pdfctx->profile_cache = NULL;
    }
    if (cmem != NULL) {
        if (pdfctx->ctx != NULL) {
            if (pdfctx->pdf_stream) {
                 memset(pdfctx->pdf_stream, 0x00, sizeof(stream));
                 gs_free_object(pdfctx->pdf_stream_memory, pdfctx->pdf_stream, "free PDF copy of stream");
                 pdfctx->pdf_stream = NULL;
            }

            if (pdfctx->ps_stream) {
                /* Detach the PostScript stream from the PDF context, otherwise the
                 * free_context code will close the main stream.
                 */
                pdfctx->ctx->main_stream = NULL;
            }
            (void)pdfi_free_context(pdfctx->ctx);
            pdfctx->ctx = NULL;
        }
        if (pdfctx->pdf_memory != NULL) {
            /* gs_memory_chunk_unwrap() returns the "wrapped" allocator, which we don't need */
            (void)gs_memory_chunk_unwrap(pdfctx->pdf_memory);
            pdfctx->pdf_memory = NULL;
        }
    }
}

static int zPDFstream(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code = 0;
    stream *s;
    pdfctx_t *pdfctx;
    pdfi_switch_t i_switch;

    check_op(2);

    check_read_file(i_ctx_p, s, op - 1);

    check_type(*op, t_pdfctx);
    pdfctx = r_ptr(op, pdfctx_t);

    /* If the supplied context already has a file open, signal an error */
    if (pdfctx->ps_stream != NULL || pdfctx->UsingPDFFile)
        return_error(gs_error_ioerror);

    s->close_at_eod = false;
    pdfctx->ps_stream = s;
    pdfctx->pdf_stream = s_alloc_immovable(imemory->stable_memory, "PDFstream copy of PS stream");
    pdfctx->pdf_stream_memory = imemory->stable_memory;
    if (pdfctx->pdf_stream == NULL)
        return_error(gs_error_VMerror);

    *(pdfctx->pdf_stream) = *(pdfctx->ps_stream);

    code = pdfi_gstate_from_PS(pdfctx->ctx, igs, &i_switch, pdfctx->profile_cache);

    if (code >= 0) {
        code = pdfi_set_input_stream(pdfctx->ctx, pdfctx->pdf_stream);

        pdfi_gstate_to_PS(pdfctx->ctx, igs, &i_switch);
    }

    if (code < 0) {
        memset(pdfctx->pdf_stream, 0x00, sizeof(stream));
        gs_free_object(imemory, pdfctx->pdf_stream, "PDFstream copy of PS stream");
        pdfctx->pdf_stream = NULL;
        pdfctx->ps_stream = NULL;
        return code;
    }

    pdfctx->ctx->finish_page = NULL;
    make_tav(op, t_pdfctx, icurrent_space | a_all, pstruct, (obj_header_t *)(pdfctx));

    pop(2);
    return 0;
}

static int zPDFfile(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    pdfctx_t *pdfctx;
    char pdffilename[gp_file_name_sizeof];
    int code = 0;
    pdfi_switch_t i_switch;

    check_op(2);

    check_type(*op, t_pdfctx);
    pdfctx = r_ptr(op, pdfctx_t);

    check_read_type(*(op - 1), t_string);
    if (r_size(op - 1) > gp_file_name_sizeof - 2)
        return_error(gs_error_limitcheck);

    /* If the supplied context already has a file open, signal an error */
    if (pdfctx->ps_stream != NULL)
        return_error(gs_error_ioerror);

    pdfctx->ps_stream = NULL;

    memcpy(pdffilename, (op - 1)->value.bytes, r_size(op - 1));
    pdffilename[r_size(op - 1)] = 0;

    code = pdfi_gstate_from_PS(pdfctx->ctx, igs, &i_switch, pdfctx->profile_cache);

    if (code >= 0) {
        code = pdfi_open_pdf_file(pdfctx->ctx, pdffilename);

        pdfi_gstate_to_PS(pdfctx->ctx, igs, &i_switch);
    }

    if (code < 0)
        return code;

    pdfctx->UsingPDFFile = true;
    pdfctx->ctx->finish_page = NULL;

    pop(2);
    return 0;
}

static int zPDFclose(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code = 0;
    pdfctx_t *pdfctx;

    check_type(*op, t_pdfctx);
    pdfctx = r_ptr(op, pdfctx_t);

    if (pdfctx->profile_cache != NULL) {
        rc_decrement(pdfctx->profile_cache, "free the working profile cache");
        pdfctx->profile_cache = NULL;
    }

    if (pdfctx->ctx != NULL) {
        pdfi_report_errors(pdfctx->ctx);
        if (pdfctx->ps_stream) {
            /* Detach the PostScript stream from the PDF context, otherwise the
             * close code will close the main stream
             */
            pdfctx->ctx->main_stream->s = NULL;
        }
        code = pdfi_free_context(pdfctx->ctx);
        pdfctx->ctx = NULL;
    }
    if (pdfctx->pdf_stream) {
        memset(pdfctx->pdf_stream, 0x00, sizeof(stream));
        gs_free_object(imemory, pdfctx->pdf_stream, "free copy of PostScript stream");
        pdfctx->pdf_stream = NULL;
    }
    if (pdfctx->ps_stream)
        pdfctx->ps_stream = NULL;
    pop(1);
    return code;
}

static int PDFobj_to_PSobj(i_ctx_t *i_ctx_p, pdfctx_t *pdfctx, pdf_obj *PDFobj, ref *PSobj);

#define DEBUG_INFO_DICT 0

static void debug_printmark(int type)
{
#if DEBUG_INFO_DICT
    switch(type) {
        case 0:
            dbgprintf("<<\n");
            break;
        case 1:
            dbgprintf(">>\n");
            break;
        case 2:
            dbgprintf("[");
            break;
        case 3:
            dbgprintf("]\n");
            break;
    }
#endif
}

static void debug_pdfobj(i_ctx_t *i_ctx_p, pdfctx_t *pdfctx, pdf_obj *PDFobj)
{
#if DEBUG_INFO_DICT
    char *str = NULL;
    int code = 0, len = 0;

    switch(pdfi_type_of(PDFobj)) {
        case PDF_NAME:
            code = pdfi_string_from_name(pdfctx->ctx, (pdf_name *)PDFobj, &str, &len);
            if (code < 0)
                return;
            dbgprintf1("/%s ", str);
            (void)pdfi_free_string_from_name(pdfctx->ctx, str);
            break;
        case PDF_STRING:
            str = (char *)gs_alloc_bytes(pdfctx->ctx->memory, ((pdf_string *)PDFobj)->length + 1, "");
            if (str == NULL)
                return;
            memset(str, 0x00, ((pdf_string *)PDFobj)->length + 1);
            memcpy(str, ((pdf_string *)PDFobj)->data, ((pdf_string *)PDFobj)->length);
            dbgprintf1("/%s ", str);
            gs_free_object(pdfctx->ctx->memory, str, "");
            break;
        case PDF_INT:
            dbgprintf1("/%d ", ((pdf_num *)PDFobj)->value.i);
            break;
        case PDF_REAL:
            dbgprintf1("/%f ", ((pdf_num *)PDFobj)->value.d);
            break;
            break;
        case PDF_BOOL:
            if (PDFobj == PDF_TRUE_OBJ)
                dbgprintf("true ");
            else
                dbgprintf("false ");
            break;
        default:
            break;
    }
#endif
}

static int PDFdict_to_PSdict(i_ctx_t *i_ctx_p, pdfctx_t *pdfctx, pdf_dict *PDFdict, ref *PSdict)
{
    int code = 0;
    uint64_t index = 0;
    pdf_name *Key = NULL;
    pdf_obj *Value = NULL;
    ref nameref, valueref;
    char *str = NULL;
    int len = 0;

    code = dict_create(pdfi_dict_entries(PDFdict), PSdict);
    if (code < 0)
        return code;

    code = pdfi_loop_detector_mark(pdfctx->ctx);
    if (code < 0)
        goto error;

    code = pdfi_dict_first(pdfctx->ctx, PDFdict, (pdf_obj **)&Key, &Value, &index);
    if (code == gs_error_undefined) {
        code = 0;
        goto error;
    }

    while (code >= 0) {
        code = pdfi_string_from_name(pdfctx->ctx, Key, &str, &len);
        if (code < 0)
            goto error;

        debug_pdfobj(i_ctx_p, pdfctx, (pdf_obj *)Key);

        code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)str, len, &nameref, 1);
        if (code < 0)
            goto error;

        code = PDFobj_to_PSobj(i_ctx_p, pdfctx, Value, &valueref);
        if (code < 0)
            goto error;

        code = dict_put(PSdict, &nameref, &valueref, &i_ctx_p->dict_stack);
        if (code < 0)
            goto error;

        pdfi_countdown(Key);
        pdfi_countdown(Value);
        Key = NULL;
        Value = NULL;

        code = pdfi_loop_detector_cleartomark(pdfctx->ctx);
        if (code < 0)
            goto error;

        code = pdfi_loop_detector_mark(pdfctx->ctx);
        if (code < 0)
            goto error;

        (void)pdfi_free_string_from_name(pdfctx->ctx, str);
        str = NULL;
        code = pdfi_dict_next(pdfctx->ctx, PDFdict, (pdf_obj **)&Key, &Value, &index);
        if (code == gs_error_undefined) {
            code = 0;
            break;
        }
    }
error:
    (void)pdfi_free_string_from_name(pdfctx->ctx, str);
    pdfi_countdown(Key);
    pdfi_countdown(Value);

    if (code < 0)
        (void)pdfi_loop_detector_cleartomark(pdfctx->ctx);
    else
        code = pdfi_loop_detector_cleartomark(pdfctx->ctx);
    return code;
}

static int PDFarray_to_PSarray(i_ctx_t *i_ctx_p, pdfctx_t *pdfctx, pdf_array *PDFarray, ref *PSarray)
{
    int code = 0, i = 0;
    ref PS_ref;
    pdf_obj *array_obj = NULL;
    ref *eltp = NULL;

    code = pdfi_loop_detector_mark(pdfctx->ctx);
    if (code < 0)
        goto error;

    code = ialloc_ref_array(PSarray, a_all, pdfi_array_size(PDFarray), "zPDFInfo");
    if (code < 0)
        goto error;

    for (i = 0;i < pdfi_array_size(PDFarray); i++) {
        code = pdfi_array_fetch_recursing(pdfctx->ctx, PDFarray, i, &array_obj, true, true);
        if (code < 0)
            goto error;
        code = PDFobj_to_PSobj(i_ctx_p, pdfctx, array_obj, &PS_ref);
        if (code < 0) {
            pdfi_countdown(array_obj);
            goto error;
        }

        eltp = PSarray->value.refs + i;
        ref_assign_old((const ref *)PSarray, eltp, &PS_ref, "zPDFInfo");

        pdfi_countdown(array_obj);
        array_obj = NULL;
    }
error:
    if (code < 0)
        (void)pdfi_loop_detector_cleartomark(pdfctx->ctx);
    else
        code = pdfi_loop_detector_cleartomark(pdfctx->ctx);
    return code;
}

static int PDFobj_to_PSobj(i_ctx_t *i_ctx_p, pdfctx_t *pdfctx, pdf_obj *PDFobj, ref *PSobj)
{
    int code = 0;

    code = pdfi_loop_detector_mark(pdfctx->ctx);
    if (code < 0)
        goto error;

    debug_pdfobj(i_ctx_p, pdfctx, PDFobj);

    switch(pdfi_type_of(PDFobj)) {
        case PDF_NAME:
            {
                char *str = NULL;
                int len;

                code = pdfi_string_from_name(pdfctx->ctx, (pdf_name *)PDFobj, &str, &len);
                if (code < 0)
                    goto error;
                code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)str, len, PSobj, 1);
                (void)pdfi_free_string_from_name(pdfctx->ctx, str);
            }
            break;
        case PDF_STRING:
            {
                byte *sbody;
                uint size = ((pdf_name *)PDFobj)->length;

                sbody = ialloc_string(size, "string");
                if (sbody == 0) {
                    code = gs_note_error(gs_error_VMerror);
                } else {
                    make_string(PSobj, a_all | icurrent_space, size, sbody);
                    memcpy(sbody, ((pdf_name *)PDFobj)->data, size);
                }
            }
            break;
        case PDF_INT:
            {
                int64_t i;

                code = pdfi_obj_to_int(pdfctx->ctx, PDFobj, &i);
                if (code < 0)
                    goto error;
                make_int(PSobj, i);
            }
            break;
        case PDF_BOOL:
            if (PDFobj == PDF_TRUE_OBJ)
                make_bool(PSobj, 1);
            else
                make_bool(PSobj, 0);
            break;
        case PDF_REAL:
            {
                double d;

                code = pdfi_obj_to_real(pdfctx->ctx, PDFobj, &d);
                if (code < 0)
                    goto error;
                make_real(PSobj, d);
            }
            break;
        case PDF_DICT:
            if (PDFobj->object_num != 0) {
                code = pdfi_loop_detector_add_object(pdfctx->ctx, PDFobj->object_num);
                if (code < 0)
                    goto error;
            }
            debug_printmark(0);
            code = PDFdict_to_PSdict(i_ctx_p, pdfctx, (pdf_dict *)PDFobj, PSobj);
            debug_printmark(1);
            break;
        case PDF_ARRAY:
            if (PDFobj->object_num != 0) {
                code = pdfi_loop_detector_add_object(pdfctx->ctx, PDFobj->object_num);
                if (code < 0)
                    goto error;
            }
            debug_printmark(2);
            code = PDFarray_to_PSarray(i_ctx_p, pdfctx, (pdf_array *)PDFobj, PSobj);
            debug_printmark(3);
            break;
        default:
            make_null(PSobj);
            break;
    }

error:
    if (code < 0)
        (void)pdfi_loop_detector_cleartomark(pdfctx->ctx);
    else
        code = pdfi_loop_detector_cleartomark(pdfctx->ctx);
    return code;
}

static int zPDFinfo(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    pdfctx_t *pdfctx;
    int code = 0;
    ref intref, nameref;
    uint64_t TotalFiles = 0, ix = 0;
    char **names_array = NULL;

    check_type(*(op), t_pdfctx);
    pdfctx = r_ptr(op, pdfctx_t);

    if (pdfctx->pdf_stream != NULL || pdfctx->UsingPDFFile) {
        code = dict_create(4, op);
        if (code < 0)
            return code;

        code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)"NumPages", 8, &nameref, 1);
        if (code < 0)
            return code;

        make_int(&intref, pdfctx->ctx->num_pages);

        code = dict_put(op, &nameref, &intref, &i_ctx_p->dict_stack);
        if (code < 0)
            return code;

        /* Code to process Collections. The pdfi_prep_collection() function returns an
         * array of descriptions and filenames. Because the descriptions can contain
         * UTF16-BE encoded data we can't sue a NULL terminated string, so the description
         * strings are terminated with a triple-NULL sequence of bytes.
         * We copy the contents into a PostScript array, which we store in the info
         * dictionary using the /Collection key.
         */
        if (pdfctx->ctx->Collection != NULL) {
            code = pdfi_prep_collection(pdfctx->ctx, &TotalFiles, &names_array);
            if (code >= 0 && TotalFiles > 0) {
                uint size;
                ref collection, stringref;

                code = ialloc_ref_array(&collection, a_all, TotalFiles * 2, "names array");
                if (code < 0)
                    goto error;

                code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)"Collection", 10, &nameref, 1);
                if (code < 0)
                    goto error;

                code = dict_put(op, &nameref, &collection, &i_ctx_p->dict_stack);
                if (code < 0)
                    goto error;

                for (ix=0; ix < TotalFiles * 2; ix++) {
                    char *ptr = names_array[ix];
                    byte *sbody;
                    ref *pelement;

                    size = 0;
                    do {
                        if (ptr[0] == 0x00 && ptr[1] == 0x00 && ptr[2] == 0x00)
                            break;
                        ptr++;
                        size++;
                    } while (1);
                    sbody = ialloc_string(size, "string");
                    if (sbody == 0) {
                        code = gs_error_VMerror;
                        goto error;
                    }
                    make_string(&stringref, a_all | icurrent_space, size, sbody);
                    memset(sbody, 0x00, size);
                    memcpy(sbody, names_array[ix], size);
                    gs_free_object(pdfctx->ctx->memory, names_array[ix], "free collection temporary filenames");
                    names_array[ix] = NULL;
                    pelement = collection.value.refs + ix;
                    ref_assign_old(&collection, pelement, &stringref, "put names string");
                }
            }
            gs_free_object(pdfctx->ctx->memory, names_array, "free collection temporary filenames");
            code = 0;
        } else {
            if (pdfctx->ctx->Info != NULL) {
                code = PDFobj_to_PSobj(i_ctx_p, pdfctx, (pdf_obj *)pdfctx->ctx->Info, (ref *)op);
                if (code < 0)
                    return code;

                code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)"NumPages", 8, &nameref, 1);
                if (code < 0)
                    return code;

                make_int(&intref, pdfctx->ctx->num_pages);

                code = dict_put(op, &nameref, &intref, &i_ctx_p->dict_stack);
                if (code < 0)
                    return code;
            } else {
                code = dict_create(1, op);
                if (code < 0)
                    return code;

                code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)"NumPages", 8, &nameref, 1);
                if (code < 0)
                    return code;

                make_int(&intref, pdfctx->ctx->num_pages);

                code = dict_put(op, &nameref, &intref, &i_ctx_p->dict_stack);
                if (code < 0)
                    return code;
            }
        }
    }
    else {
        return_error(gs_error_ioerror);
    }
    return code;

error:
    for (ix=0; ix < TotalFiles * 2; ix++)
        gs_free_object(pdfctx->ctx->memory, names_array[ix], "free collection temporary filenames");
    gs_free_object(pdfctx->ctx->memory, names_array, "free collection temporary filenames");
    return code;
}

static int zPDFpageinfo(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int page = 0, code = 0;
    pdfctx_t *pdfctx;
    pdf_dict *InfoDict = NULL;
    pdfi_switch_t i_switch;

    check_op(2);

    check_type(*op, t_integer);
    page = op->value.intval;

    check_type(*(op - 1), t_pdfctx);
    pdfctx = r_ptr(op - 1, pdfctx_t);

    if (pdfctx->pdf_stream != NULL || pdfctx->UsingPDFFile) {
        code = pdfi_gstate_from_PS(pdfctx->ctx, igs, &i_switch, pdfctx->profile_cache);

        if (code >= 0) {
            code = pdfi_page_info(pdfctx->ctx, (uint64_t)page, &InfoDict, false);

            pdfi_gstate_to_PS(pdfctx->ctx, igs, &i_switch);
        }

        if (code < 0)
            return code;

        pop(1);
        op = osp;

        code = PDFobj_to_PSobj(i_ctx_p, pdfctx, (pdf_obj *)InfoDict, op);
        pdfi_countdown(InfoDict);
        if (code < 0)
            return code;
    }
    else {
        return_error(gs_error_ioerror);
    }
    return 0;
}

static int zPDFpageinfoExt(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int page = 0, code = 0;
    pdfctx_t *pdfctx;
    pdf_dict *InfoDict = NULL;
    pdfi_switch_t i_switch;

    check_op(2);

    check_type(*op, t_integer);
    page = op->value.intval;

    check_type(*(op - 1), t_pdfctx);
    pdfctx = r_ptr(op - 1, pdfctx_t);

    if (pdfctx->pdf_stream != NULL || pdfctx->UsingPDFFile) {
        code = pdfi_gstate_from_PS(pdfctx->ctx, igs, &i_switch, pdfctx->profile_cache);

        if (code >= 0) {
            code = pdfi_page_info(pdfctx->ctx, (uint64_t)page, &InfoDict, true);

            pdfi_gstate_to_PS(pdfctx->ctx, igs, &i_switch);
        }

        if (code < 0)
            return code;

        pop(1);
        op = osp;

        code = PDFobj_to_PSobj(i_ctx_p, pdfctx, (pdf_obj *)InfoDict, op);
        pdfi_countdown(InfoDict);
        if (code < 0)
            return code;
    }
    else {
        return_error(gs_error_ioerror);
    }
    return 0;
}

static int zPDFmetadata(i_ctx_t *i_ctx_p)
{
#if 0
    os_ptr op = osp;
    pdfctx_t *pdfctx;

    check_type(*op, t_pdfctx);
    pdfctx = r_ptr(op, pdfctx_t);
#endif

    return_error(gs_error_undefined);
}

static int zPDFdrawpage(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code = 0;
    uint64_t page = 0;
    pdfctx_t *pdfctx;
    pdfi_switch_t i_switch;

    check_op(2);

    check_type(*op, t_integer);
    page = op->value.intval;

    check_type(*(op - 1), t_pdfctx);
    pdfctx = r_ptr(op - 1, pdfctx_t);

    if (pdfctx->pdf_stream != NULL || pdfctx->UsingPDFFile) {
        code = gs_gsave(igs);
        if (code < 0)
            return code;

        code = pdfi_gstate_from_PS(pdfctx->ctx, igs, &i_switch, pdfctx->profile_cache);

        if (code >= 0) {
            if (pdfctx->ctx->args.pdfinfo)
                code = pdfi_output_page_info(pdfctx->ctx, page);
            else
                code = pdfi_page_render(pdfctx->ctx, page, false);
            if (code >= 0)
                pop(2);

            pdfi_gstate_to_PS(pdfctx->ctx, igs, &i_switch);
        }
        if (code == 0)
            code = gs_grestore(igs);
        else
            (void)gs_grestore(igs);
    }
    else {
        return_error(gs_error_ioerror);
    }
    return code;
}

static int zPDFdrawannots(i_ctx_t *i_ctx_p)
{
#if 0
    os_ptr op = osp;
    pdfctx_t *pdfctx;

    check_type(*op, t_pdfctx);
    pdfctx = r_ptr(op, pdfctx_t);
#endif

    return_error(gs_error_undefined);
}

static int zpdfi_glyph_index(gs_font *pfont, byte *str, uint size, uint *glyph)
{
    int code = 0;
    ref nref;
    code = name_ref(pfont->memory, str, size, &nref, true);
    if (code < 0)
        return code;
    *glyph = name_index(pfont->memory, &nref);
    return 0;
}

static int param_value_get_namelist(gs_memory_t *ps_mem, pdf_context *ctx, ref *pvalueref, char ***pstrlist)
{
    uint count;
    char **strlist = NULL;
    ref lval, sref;
    int code;

    check_read_type(*pvalueref, t_array);

    strlist = (char **)gs_alloc_bytes(ctx->memory, (r_size(pvalueref)+1)*sizeof(char *), "param_value_get_namelist");
    if (strlist == NULL)
        return_error(gs_error_VMerror);
    memset(strlist, 0x00, (r_size(pvalueref)+1)*sizeof(char *));

    for (count = 0;count < r_size(pvalueref); count++) {
        code = array_get(ps_mem, pvalueref, count, &lval);
        if (code < 0)
            return code;

        if (!r_has_type(&lval, t_string) && !r_has_type(&lval, t_name))
            return_error(gs_error_typecheck);

        if (r_has_type(&lval, t_name))
            name_string_ref(ps_mem, (const ref *)&lval, &sref);
        else
            sref = lval;

        strlist[count] = (char *)gs_alloc_bytes(ctx->memory, sref.tas.rsize + 1, "param_value_get_namelist");
        if (strlist[count] == NULL)
            return_error(gs_error_VMerror);
        memset(strlist[count], 0x00, sref.tas.rsize + 1);
        memcpy(strlist[count], sref.value.bytes, sref.tas.rsize);
    }
    *pstrlist = strlist;
    return 0;
}

static int zPDFInit(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    ref *pdictref = NULL, *pvalueref;
    pdfctx_t *pdfctx = NULL;
    pdf_context *ctx = NULL;
    int code = 0;
    gs_memory_t *cmem;

    code = gs_memory_chunk_wrap(&cmem, imemory->non_gc_memory);
    if (code < 0)
        return_error(gs_error_VMerror);

    pdfctx = gs_alloc_struct(imemory, pdfctx_t, &st_pdfctx_t, "PDFcontext");
    if (!pdfctx) {
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }
    pdfctx->pdf_memory = cmem;
    pdfctx->ctx = NULL;
    pdfctx->ps_stream = NULL;
    pdfctx->pdf_stream = NULL;
    pdfctx->UsingPDFFile = false;
    pdfctx->pdf_stream_memory = NULL;
    pdfctx->profile_cache = gsicc_profilecache_new(imemory);
    if (pdfctx->profile_cache == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }
    pdfctx->cache_memory = imemory;

    ctx = pdfi_create_context(cmem);
    if (ctx == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }

    pdfctx->ctx = ctx;
    get_zfont_glyph_name(&pdfctx->ctx->get_glyph_name);
    pdfctx->ctx->get_glyph_index = zpdfi_glyph_index;

    if (ref_stack_count(&o_stack) > 0 && r_has_type(op, t_dictionary)) {
        pdictref = op;

        code = gs_error_typecheck;
        if (dict_find_string(pdictref, "PDFDEBUG", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.pdfdebug = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "PDFSTOPONERROR", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.pdfstoponerror = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "PDFSTOPONWARNING", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.pdfstoponwarning = pvalueref->value.boolval;
            if (pvalueref->value.boolval)
                pdfctx->ctx->args.pdfstoponerror = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "NOTRANSPARENCY", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.notransparency = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "QUIET", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.QUIET = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "VerboseErrors", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.verbose_errors = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "VerboseWarnings", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.verbose_warnings = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "PDFPassword", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_string))
                goto error;
            pdfctx->ctx->encryption.Password = (char *)gs_alloc_bytes(pdfctx->ctx->memory, r_size(pvalueref) + 1, "PDF Password from zpdfops");
            memset(pdfctx->ctx->encryption.Password, 0x00, r_size(pvalueref) + 1);
            memcpy(pdfctx->ctx->encryption.Password, pvalueref->value.const_bytes, r_size(pvalueref));
            pdfctx->ctx->encryption.PasswordLen = r_size(pvalueref);
        }

        if (dict_find_string(pdictref, "FirstPage", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_integer))
                goto error;
            pdfctx->ctx->args.first_page = pvalueref->value.intval;
        }

        if (dict_find_string(pdictref, "LastPage", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_integer))
                goto error;
            pdfctx->ctx->args.last_page = pvalueref->value.intval;
        }

        if (dict_find_string(pdictref, "PDFNOCIDFALLBACK", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.nocidfallback = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "NO_PDFMARK_OUTLINES", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.no_pdfmark_outlines = pvalueref->value.boolval;
        }

        /* This one can be a boolean OR an integer */
        if (dict_find_string(pdictref, "UsePDFX3Profile", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean)) {
                if (!r_has_type(pvalueref, t_integer))
                    goto error;
                else {
                    pdfctx->ctx->args.UsePDFX3Profile = true;
                    pdfctx->ctx->args.PDFX3Profile_num = pvalueref->value.intval;
                }
            } else {
                pdfctx->ctx->args.UsePDFX3Profile = pvalueref->value.boolval;
                pdfctx->ctx->args.PDFX3Profile_num = 0;
            }
        }

        if (dict_find_string(pdictref, "NO_PDFMARK_DESTS", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.no_pdfmark_dests = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "PDFFitPage", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.pdffitpage = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "Printed", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.printed = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "UseBleedBox", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.usebleedbox = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "UseCropBox", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.usecropbox = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "UseArtBox", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.useartbox = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "UseTrimBox", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.usetrimbox = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "ShowAcroForm", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.showacroform = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "ShowAnnots", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.showannots = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "PreserveAnnots", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.preserveannots = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "PreserveMarkedContent", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.preservemarkedcontent = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "NoUserUnit", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.nouserunit = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "RENDERTTNOTDEF", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.renderttnotdef = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "DOPDFMARKS", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.dopdfmarks = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "PDFINFO", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.pdfinfo = pvalueref->value.boolval;
        }
        if (dict_find_string(pdictref, "ShowAnnotTypes", &pvalueref) > 0) {
            code = param_value_get_namelist(imemory, pdfctx->ctx, pvalueref,
                                            &pdfctx->ctx->args.showannottypes);
            if (code < 0)
                goto error;
        }
        if (dict_find_string(pdictref, "PreserveAnnotTypes", &pvalueref) > 0) {
            code = param_value_get_namelist(imemory, pdfctx->ctx, pvalueref,
                                            &pdfctx->ctx->args.preserveannottypes);
            if (code < 0)
                goto error;
        }
        if (dict_find_string(pdictref, "CIDFSubstPath", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_string))
                goto error;
            pdfctx->ctx->args.cidfsubstpath.data = (byte *)gs_alloc_bytes(pdfctx->ctx->memory, r_size(pvalueref) + 1, "PDF cidfsubstpath from zpdfops");
            if (pdfctx->ctx->args.cidfsubstpath.data == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto error;
            }
            memcpy(pdfctx->ctx->args.cidfsubstpath.data, pvalueref->value.const_bytes, r_size(pvalueref));
            pdfctx->ctx->args.cidfsubstpath.size = r_size(pvalueref);
        }
        if (dict_find_string(pdictref, "CIDFSubstFont", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_string))
                goto error;
            pdfctx->ctx->args.cidfsubstfont.data = (byte *)gs_alloc_bytes(pdfctx->ctx->memory, r_size(pvalueref) + 1, "PDF cidfsubstfont from zpdfops");
            if (pdfctx->ctx->args.cidfsubstfont.data == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto error;
            }
            memcpy(pdfctx->ctx->args.cidfsubstfont.data, pvalueref->value.const_bytes, r_size(pvalueref));
            pdfctx->ctx->args.cidfsubstfont.size = r_size(pvalueref);
        }
        if (dict_find_string(pdictref, "SUBSTFONT", &pvalueref) > 0) {
            ref nmstr, *namstrp;
            if (r_has_type(pvalueref, t_string)) {
                namstrp = pvalueref;
                pdfctx->ctx->args.defaultfont_is_name = false;
            } else if (r_has_type(pvalueref, t_name)) {
                name_string_ref(imemory, pvalueref, &nmstr);
                namstrp = &nmstr;
                pdfctx->ctx->args.defaultfont_is_name = true;
            }
            else {
                code = gs_note_error(gs_error_typecheck);
                goto error;
            }
            pdfctx->ctx->args.defaultfont.data = (byte *)gs_alloc_bytes(pdfctx->ctx->memory, r_size(namstrp) + 1, "PDF defaultfontname from zpdfops");
            if (pdfctx->ctx->args.defaultfont.data == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto error;
            }
            memcpy(pdfctx->ctx->args.defaultfont.data, pvalueref->value.const_bytes, r_size(namstrp));
            pdfctx->ctx->args.defaultfont.size = r_size(namstrp);
        }
        if (dict_find_string(pdictref, "IgnoreToUnicode", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.ignoretounicode = pvalueref->value.boolval;
        }
        if (dict_find_string(pdictref, "NONATIVEFONTMAP", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.nonativefontmap = pvalueref->value.boolval;
        }
        if (dict_find_string(pdictref, "PageCount", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_integer))
                goto error;
            pdfctx->ctx->Pdfmark_InitialPage = pvalueref->value.intval;
        }
        code = 0;
        pop(1);
    }
    code = zpdfi_populate_search_paths(i_ctx_p, ctx);
    if (code < 0)
        goto error;

    code = zpdfi_populate_fontmap_files(i_ctx_p, ctx);
    if (code < 0)
        goto error;

    op = osp;
    push(1);
    make_tav(op, t_pdfctx, icurrent_space | a_all, pstruct, (obj_header_t *)(pdfctx));
    return 0;

error:
    if (ctx != NULL)
        pdfi_free_context(ctx);
    /* gs_memory_chunk_unwrap() returns the "wrapped" allocator, which we don't need */
    (void)gs_memory_chunk_unwrap(cmem);
    if (pdfctx != NULL) {
        pdfctx->ctx = NULL; /* Freed already above! */
        if (pdfctx->profile_cache != NULL) {
            gs_free_object(imemory, pdfctx->profile_cache, "discard temporary profile cache");
            pdfctx->profile_cache = NULL;
        }
        pdfctx->pdf_memory = NULL;
        gs_free_object(imemory, pdfctx, "PDFcontext");
    }
    return code;
}

/*   <Pagelist_string> num_pages .PDFparsePageList even/odd, start, end ... range_count	*/
/*   PostScript will create an array of 3 integers per range			*/
static int zPDFparsePageList(i_ctx_t *i_ctx_p)
{
    int code = 0, size = 0, i;
    os_ptr op = osp;
    int *page_range_array;
    int num_pages;

    check_op(2);

    code = int_param(op, max_int, &num_pages);
    if (code < 0)
        return code;

    check_type_only(*(op - 1), t_string);

    code = pagelist_parse_to_array((char *)((op - 1)->value.const_bytes), imemory, num_pages, &page_range_array);
    make_int(op, 0);				/* default return 0 */
    if (code < 0) {
        return code;
    }
    /* code returned is the number of ranges */
    size = 3 * (code - 1);		/* runpdfpagerange doesn't use 0, 0, 0 marker at end */
    code = ref_stack_push(&o_stack, size - 1);
    if (code < 0) {
        return code;
    }
    /* push the even/odd, start, end triples on the stack */
    for (i=0; i < size;  i++) {
        /* skip the initial "ordered" flag */
        make_int(ref_stack_index(&o_stack, size - i), page_range_array[i+1]);
    }
    make_int(ref_stack_index(&o_stack, 0), size);
    pagelist_free_range_array(imemory, page_range_array);		/* all done with C array */
    return 0;
}
#else

static int zPDFfile(i_ctx_t *i_ctx_p)
{
    return_error(gs_error_undefined);
}

static int zPDFstream(i_ctx_t *i_ctx_p)
{
    return_error(gs_error_undefined);
}

static int zPDFclose(i_ctx_t *i_ctx_p)
{
    return_error(gs_error_undefined);
}

static int zPDFinfo(i_ctx_t *i_ctx_p)
{
    return_error(gs_error_undefined);
}

static int zPDFpageinfo(i_ctx_t *i_ctx_p)
{
    return_error(gs_error_undefined);
}

static int zPDFpageinfoExt(i_ctx_t *i_ctx_p)
{
    return_error(gs_error_undefined);
}

static int zPDFmetadata(i_ctx_t *i_ctx_p)
{
    return_error(gs_error_undefined);
}

static int zPDFdrawpage(i_ctx_t *i_ctx_p)
{
    return_error(gs_error_undefined);
}

static int zPDFdrawannots(i_ctx_t *i_ctx_p)
{
    return_error(gs_error_undefined);
}

static int zPDFInit(i_ctx_t *i_ctx_p)
{
    return_error(gs_error_undefined);
}

static int zPDFparsePageList(i_ctx_t *i_ctx_p)
{
    return_error(gs_error_undefined);
}
#endif

static int zPDFAvailable(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
#if defined(BUILD_PDF) && BUILD_PDF == 1
    make_bool(op, true);
#else
    make_bool(op, false);
#endif
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zpdfops_op_defs[] =
{
    {"0.pdfinkpath", zpdfinkpath},
    {"1.pdfFormName", zpdfFormName},
    {"3.setscreenphase", zsetscreenphase},
    {"0.PDFFile", zPDFfile},
    {"1.PDFStream", zPDFstream},
    {"1.PDFClose", zPDFclose},
    {"1.PDFInfo", zPDFinfo},
    {"1.PDFPageInfo", zPDFpageinfo},
    {"1.PDFPageInfoExt", zPDFpageinfoExt},
    {"1.PDFMetadata", zPDFmetadata},
    {"1.PDFDrawPage", zPDFdrawpage},
    {"1.PDFDrawAnnots", zPDFdrawannots},
    {"1.PDFInit", zPDFInit},
    {"1.PDFparsePageList", zPDFparsePageList},
    {"0.PDFAvailable", zPDFAvailable},
#ifdef HAVE_LIBIDN
    {"1.saslprep", zsaslprep},
#endif
    op_def_end(0)
};
