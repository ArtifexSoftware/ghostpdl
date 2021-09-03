/* Copyright (C) 2001-2021 Artifex Software, Inc.
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

#include "iminst.h"
#include "dstack.h"
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

/*
   This cannot fail. If gs doesn't have these settings it will never reach here.
   Because the lives of the string values here are all tied to the Postscript
   context, and the Postscript context *must* outlast the pdfi context, we can
   safely just take references, marking the strings as "persistent" so destroying
   the pdfi context doesn't try to free the string bodies.
 */
static void zpdfi_populate_search_paths(i_ctx_t *i_ctx_p, pdf_context *ctx)
{
    /* This should only be called once per pdfi context
       if the paths are already populated, just skip it.
     */
    if (ctx->search_paths.resource_paths == NULL) {
        ref *l2dictref, *grdref, *fpathref;
        int code, i;
        const gs_file_path *pfpath = i_ctx_p->lib_path;
        gs_main_instance *minst = get_minst_from_memory(imemory);
        code = dict_find_string(systemdict, "pssystemparams", &l2dictref);
        if (code >= 0 && r_has_type(l2dictref, t_dictionary)) {
            code = dict_find_string(l2dictref, "GenericResourceDir", &grdref);
            if (code >= 0 && r_has_type(grdref, t_string)) {
                ctx->search_paths.genericresourcedir.data = grdref->value.const_bytes;
                ctx->search_paths.genericresourcedir.size = r_size(grdref);
                ctx->search_paths.genericresourcedir.persistent = true;
            }
        }
        ctx->search_paths.resource_paths = (gs_param_string *)gs_alloc_bytes(ctx->memory, sizeof(gs_param_string) * r_size(&pfpath->list), "array of paths");
        ctx->search_paths.num_resource_paths = r_size(&pfpath->list);
        for (i = 0; i < r_size(&pfpath->list); i++) {
            const ref *prdir = pfpath->list.value.refs + i; /* By nature, this cannot be a short/mixed array, only a "normal" array */
            ctx->search_paths.resource_paths[i].data = prdir->value.const_bytes;
            ctx->search_paths.resource_paths[i].size = r_size(prdir);
            ctx->search_paths.resource_paths[i].persistent = true;
        }
        code = dict_find_string(systemdict, "FONTPATH", &fpathref);
        ctx->search_paths.font_paths = (gs_param_string *)gs_alloc_bytes(ctx->memory, sizeof(gs_param_string) * r_size(fpathref), "array of font paths");
        ctx->search_paths.num_font_paths = r_size(fpathref);
        for (i = 0; i < r_size(fpathref); i++) {
            const ref *prdir = pfpath->list.value.refs + i; /* By nature, this cannot be a short/mixed array, only a "normal" array */
            ctx->search_paths.resource_paths[i].data = prdir->value.const_bytes;
            ctx->search_paths.resource_paths[i].size = r_size(prdir);
            ctx->search_paths.resource_paths[i].persistent = true;
        }
        ctx->search_paths.search_here_first = minst->search_here_first;
    }
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
} pdfctx_t;

/* Structure descriptors */
static void pdfctx_finalize(const gs_memory_t *cmem, void *vptr);

gs_private_st_composite_final(st_pdfctx_t, pdfctx_t, "pdfctx_struct",\
    pdfctx_enum_ptrs, pdfctx_reloc_ptrs, pdfctx_finalize);

static
ENUM_PTRS_BEGIN(pdfctx_enum_ptrs) return 0;
ENUM_PTR2(0, pdfctx_t, ps_stream, pdf_stream);
ENUM_PTRS_END

static RELOC_PTRS_BEGIN(pdfctx_reloc_ptrs);
RELOC_PTR2(pdfctx_t, ps_stream, pdf_stream);
RELOC_PTRS_END

static void
pdfctx_finalize(const gs_memory_t *cmem, void *vptr)
{
    pdfctx_t *pdfctx = vptr;
    /* Finalize methods have to cope with the possibility of being called multiple times
     *  on the same object - hence we null the entries.
     */

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
    gs_gstate *pgs = NULL;
    gs_gstate_client_procs procs;
    void *client_data;

    check_op(2);

    check_read_file(i_ctx_p, s, op - 1);

    check_type(*op, t_pdfctx);
    pdfctx = r_ptr(op, pdfctx_t);

    /* If the supplied context already has a file open, signal an error */
    if (pdfctx->ps_stream != NULL)
        return_error(gs_error_ioerror);

    s->close_at_eod = false;
    pdfctx->ps_stream = s;
    pdfctx->pdf_stream = s_alloc_immovable(imemory, "PDFstream copy of PS stream");
    pdfctx->pdf_stream_memory = imemory;
    if (pdfctx->pdf_stream == NULL)
        return_error(gs_error_VMerror);

    *(pdfctx->pdf_stream) = *(pdfctx->ps_stream);

    pgs = pdfctx->ctx->pgs;
    procs = igs->client_procs;
    client_data = igs->client_data;
    pdfi_gstate_from_PS(pdfctx->ctx, igs, &client_data, &procs);
    pdfctx->ctx->pgs = igs;

    code = pdfi_set_input_stream(pdfctx->ctx, pdfctx->pdf_stream);

    pdfi_gstate_to_PS(pdfctx->ctx, igs, client_data, &procs);
    if (code == 0)
        code = gs_grestore(igs);
    else
        (void)gs_grestore(igs);
    pdfctx->ctx->pgs = pgs;

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
    gs_gstate *pgs = NULL;
    gs_gstate_client_procs procs;
    void *client_data;

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

    pgs = pdfctx->ctx->pgs;
    procs = igs->client_procs;
    client_data = igs->client_data;
    pdfi_gstate_from_PS(pdfctx->ctx, igs, &client_data, &procs);
    pdfctx->ctx->pgs = igs;

    code = pdfi_open_pdf_file(pdfctx->ctx, pdffilename);

    pdfi_gstate_to_PS(pdfctx->ctx, igs, client_data, &procs);
    if (code == 0)
        code = gs_grestore(igs);
    else
        (void)gs_grestore(igs);
    pdfctx->ctx->pgs = pgs;

    if (code < 0)
        return code;

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

    if (pdfctx->ctx != NULL) {
        if (pdfctx->ps_stream) {
            /* Detach the PostScript stream from the PDF context, otherwise the
             * close code will close the main stream
             */
            pdfctx->ctx->main_stream = NULL;
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
    ref aref, boolref, nameref, numref, *eltp;
    int page = 0, code = 0, i;
    pdfctx_t *pdfctx;
    pdf_info_t info;
    gs_gstate *pgs = NULL;
    gs_gstate_client_procs procs;
    void *client_data;

    check_op(2);

    check_type(*op, t_integer);
    page = op->value.intval;

    check_type(*(op - 1), t_pdfctx);
    pdfctx = r_ptr(op - 1, pdfctx_t);

    pgs = pdfctx->ctx->pgs;
    procs = igs->client_procs;
    client_data = igs->client_data;
    pdfi_gstate_from_PS(pdfctx->ctx, igs, &client_data, &procs);
    pdfctx->ctx->pgs = igs;

    code = pdfi_page_info(pdfctx->ctx, (uint64_t)page, &info);

    pdfi_gstate_to_PS(pdfctx->ctx, igs, client_data, &procs);
    if (code == 0)
        code = gs_grestore(igs);
    else
        (void)gs_grestore(igs);
    pdfctx->ctx->pgs = pgs;

    if (code < 0)
        return code;

    pop(1);
    op = osp;

    code = dict_create(4, op);
    if (code < 0)
        return code;

    code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)"HasAnnots", 9, &nameref, 1);
    if (code < 0)
        return code;
    make_bool(&boolref, false);
    code = dict_put(op, &nameref, &boolref, &i_ctx_p->dict_stack);
    if (code < 0)
        return code;

    code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)"UsesTransparency", 16, &nameref, 1);
    if (code < 0)
        return code;
    make_bool(&boolref, info.HasTransparency);
    code = dict_put(op, &nameref, &boolref, &i_ctx_p->dict_stack);
    if (code < 0)
        return code;

    code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)"NumSpots", 8, &nameref, 1);
    if (code < 0)
        return code;
    make_int(&numref, info.NumSpots);
    code = dict_put(op, &nameref, &numref, &i_ctx_p->dict_stack);
    if (code < 0)
        return code;

    if (info.boxes & MEDIA_BOX) {
        code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)"MediaBox", 8, &nameref, 1);
        if (code < 0)
            return code;
        code = ialloc_ref_array(&aref, a_all, 4, "array");
        if (code < 0)
            return code;
        refset_null(aref.value.refs, 4);
        for (i=0;i < 4;i++) {
            make_real(&numref, info.MediaBox[i]);
            eltp = aref.value.refs + i;
            ref_assign_old(&aref, eltp, &numref, "put");
        }
        code = dict_put(op, &nameref, &aref, &i_ctx_p->dict_stack);
        if (code < 0)
            return code;
    }

    if (info.boxes & CROP_BOX) {
        code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)"CropBox", 7, &nameref, 1);
        if (code < 0)
            return code;
        code = ialloc_ref_array(&aref, a_all, 4, "array");
        if (code < 0)
            return code;
        refset_null(aref.value.refs, 4);
        for (i=0;i < 4;i++) {
            make_real(&numref, info.CropBox[i]);
            eltp = aref.value.refs + i;
            ref_assign_old(&aref, eltp, &numref, "put");
        }
        code = dict_put(op, &nameref, &aref, &i_ctx_p->dict_stack);
        if (code < 0)
            return code;
    }

    if (info.boxes & TRIM_BOX) {
        code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)"TrimBox", 7, &nameref, 1);
        if (code < 0)
            return code;
        code = ialloc_ref_array(&aref, a_all, 4, "array");
        if (code < 0)
            return code;
        refset_null(aref.value.refs, 4);
        for (i=0;i < 4;i++) {
            make_real(&numref, info.TrimBox[i]);
            eltp = aref.value.refs + i;
            ref_assign_old(&aref, eltp, &numref, "put");
        }
        code = dict_put(op, &nameref, &aref, &i_ctx_p->dict_stack);
        if (code < 0)
            return code;
    }

    if (info.boxes & ART_BOX) {
        code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)"ArtBox", 6, &nameref, 1);
        if (code < 0)
            return code;
        code = ialloc_ref_array(&aref, a_all, 4, "array");
        if (code < 0)
            return code;
        refset_null(aref.value.refs, 4);
        for (i=0;i < 4;i++) {
            make_real(&numref, info.ArtBox[i]);
            eltp = aref.value.refs + i;
            ref_assign_old(&aref, eltp, &numref, "put");
        }
        code = dict_put(op, &nameref, &aref, &i_ctx_p->dict_stack);
        if (code < 0)
            return code;
    }

    if (info.boxes & BLEED_BOX) {
        code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)"BleedBox", 8, &nameref, 1);
        if (code < 0)
            return code;
        code = ialloc_ref_array(&aref, a_all, 4, "array");
        if (code < 0)
            return code;
        refset_null(aref.value.refs, 4);
        for (i=0;i < 4;i++) {
            make_real(&numref, info.BleedBox[i]);
            eltp = aref.value.refs + i;
            ref_assign_old(&aref, eltp, &numref, "put");
        }
        code = dict_put(op, &nameref, &aref, &i_ctx_p->dict_stack);
        if (code < 0)
            return code;
    }

    code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)"Rotate", 6, &nameref, 1);
    if (code < 0)
        return code;
    make_real(&numref, info.Rotate);
    code = dict_put(op, &nameref, &numref, &i_ctx_p->dict_stack);
    if (code < 0)
        return code;

    if (info.UserUnit != 1) {
        code = names_ref(imemory->gs_lib_ctx->gs_name_table, (const byte *)"UserUnit", 8, &nameref, 1);
        if (code < 0)
            return code;
        make_real(&numref, info.UserUnit);
        code = dict_put(op, &nameref, &numref, &i_ctx_p->dict_stack);
        if (code < 0)
            return code;
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
    gs_gstate *pgs = NULL;
    gs_gstate_client_procs procs;
    void *client_data;

    check_op(2);

    check_type(*op, t_integer);
    page = op->value.intval;

    check_type(*(op - 1), t_pdfctx);
    pdfctx = r_ptr(op - 1, pdfctx_t);

    code = gs_gsave(igs);
    if (code < 0)
        return code;

    pgs = pdfctx->ctx->pgs;
    procs = igs->client_procs;
    client_data = igs->client_data;
    pdfi_gstate_from_PS(pdfctx->ctx, igs, &client_data, &procs);
    pdfctx->ctx->pgs = igs;

    code = pdfi_page_render(pdfctx->ctx, page, false);
    if (code >= 0)
        pop(2);

    pdfi_gstate_to_PS(pdfctx->ctx, igs, client_data, &procs);
    if (code == 0)
        code = gs_grestore(igs);
    else
        (void)gs_grestore(igs);
    pdfctx->ctx->pgs = pgs;

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

static int param_value_get_namelist(pdf_context *ctx, ref *pvalueref, char ***pstrlist)
{
    char *data;
    uint size;

    if (!r_has_type(pvalueref, t_string))
        return_error(gs_error_typecheck);

    data = (char *)pvalueref->value.bytes;
    size = pvalueref->tas.rsize;

    return pdfi_parse_name_cstring_array(ctx, data, size, pstrlist);
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
    pdfctx->pdf_stream_memory = NULL;

    ctx = pdfi_create_context(cmem);
    if (ctx == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }

    pdfctx->ctx = ctx;
    get_zfont_glyph_name((void **)&pdfctx->ctx->get_glyph_name);
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

        if (dict_find_string(pdictref, "NOCIDFALLBACK", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.nocidfallback = pvalueref->value.boolval;
        }

        if (dict_find_string(pdictref, "NO_PDFMARK_OUTLINES", &pvalueref) > 0) {
            if (!r_has_type(pvalueref, t_boolean))
                goto error;
            pdfctx->ctx->args.no_pdfmark_outlines = pvalueref->value.boolval;
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
        if (dict_find_string(pdictref, "SHOWANNOTTYPES", &pvalueref) > 0) {
            code = param_value_get_namelist(pdfctx->ctx, pvalueref,
                                            &pdfctx->ctx->args.showannottypes);
            if (code < 0)
                goto error;
        }
        if (dict_find_string(pdictref, "PRESERVEANNOTTYPES", &pvalueref) > 0) {
            code = param_value_get_namelist(pdfctx->ctx, pvalueref,
                                            &pdfctx->ctx->args.preserveannottypes);
            if (code < 0)
                goto error;
        }
        code = 0;
        pop(1);
    }
    zpdfi_populate_search_paths(i_ctx_p, ctx);
    op = osp;
    push(1);
    make_tav(op, t_pdfctx, icurrent_space | a_all, pstruct, (obj_header_t *)(pdfctx));
    return 0;

error:
    if (ctx)
        pdfi_free_context(ctx);
    /* gs_memory_chunk_unwrap() returns the "wrapped" allocator, which we don't need */
    (void)gs_memory_chunk_unwrap(cmem);
    if (pdfctx) {
        pdfctx->pdf_memory = NULL;
        gs_free_object(imemory, pdfctx, "PDFcontext");
    }
    return code;
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
#endif

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
    {"1.PDFMetadata", zPDFmetadata},
    {"1.PDFDrawPage", zPDFdrawpage},
    {"1.PDFDrawAnnots", zPDFdrawannots},
    {"1.PDFInit", zPDFInit},
#ifdef HAVE_LIBIDN
    {"1.saslprep", zsaslprep},
#endif
    op_def_end(0)
};
