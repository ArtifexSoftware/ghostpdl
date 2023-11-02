/* Copyright (C) 2020-2023 Artifex Software, Inc.
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

#include "strmio.h"
#include "stream.h"
#include "scanchar.h"

#include "pdf_int.h"
#include "pdf_cmap.h"

#include "pdf_stack.h"
#include "pdf_dict.h"
#include "pdf_file.h"
#include "pdf_fontps.h"
#include "pdf_deref.h"

static int pdfi_free_cmap_contents(pdf_cmap *cmap);

static int cmap_usecmap_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    pdf_cmap *pdficmap = (pdf_cmap *)s->client_data;
    pdf_name *n = NULL;
    pdf_cmap *upcmap = NULL;
    int code = 0;

    if (pdf_ps_stack_count(s) < 1)
        return_error(gs_error_stackunderflow);

    /* If we've already got some definitions, ignore the usecmap op */
    if (pdficmap->code_space.num_ranges == 0) {
        byte *nstr = NULL;
        int len = s->cur[0].size;

        if (pdf_ps_obj_has_type(&(s->cur[0]), PDF_PS_OBJ_NAME)) {
            nstr = s->cur[0].val.name;
        }
        else if (pdf_ps_obj_has_type(&(s->cur[0]), PDF_PS_OBJ_STRING)) {
            nstr = s->cur[0].val.string;
        }
        else {
            code = gs_note_error(gs_error_typecheck);
        }
        if (code >= 0)
            code = pdfi_name_alloc(pdficmap->ctx, nstr, len, (pdf_obj **)&n);
        if (code >= 0) {
            pdfi_countup(n);
            code = pdfi_read_cmap(pdficmap->ctx, (pdf_obj *)n, &upcmap);
            if (code >= 0) {
                gx_code_space_range_t * ranges =
                         (gx_code_space_range_t *)gs_alloc_byte_array(mem, upcmap->code_space.num_ranges,
                          sizeof(gx_code_space_range_t), "cmap_usecmap_func(ranges)");
                if (ranges != NULL) {
                    int i;
                    memcpy(&pdficmap->code_space, &upcmap->code_space, sizeof(pdficmap->code_space));
                    for (i = 0; i < upcmap->code_space.num_ranges; i++) {
                        memcpy(&(ranges[i]), &(upcmap->code_space.ranges[i]), sizeof(ranges[i]));
                    }
                    pdficmap->code_space.ranges = ranges;
                    memcpy(&pdficmap->cmap_range, &upcmap->cmap_range, sizeof(pdficmap->cmap_range));
                    memcpy(&pdficmap->notdef_cmap_range, &upcmap->notdef_cmap_range, sizeof(pdficmap->notdef_cmap_range));
                    /* Once we've assumed control of these, NULL out entries for the sub-cmap. */
                    upcmap->cmap_range.ranges = NULL;
                    upcmap->notdef_cmap_range.ranges = NULL;
                    /* But we keep the subcmap itself because we rely on its storage */
                    pdficmap->next = upcmap;
                    pdfi_countup(upcmap);
                }
            }
        }
    }
    pdfi_countdown(upcmap);
    pdfi_countdown(n);
    if (code < 0) {
        (void)pdf_ps_stack_pop(s, 1);
        return code;
    }
    return pdf_ps_stack_pop(s, 1);
}

#if 0 /* no longer used */
static int cmap_pushmark_func(gs_memory_t *mem, pdf_ps_ctx_t *stack, pdf_cmap *pdficmap)
{
    return pdf_ps_stack_push_mark(stack);
}
#endif


static int cmap_endcodespacerange_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    pdf_cmap *pdficmap = (pdf_cmap *)s->client_data;
    int i, numranges, to_pop = pdf_ps_stack_count_to_mark(s, PDF_PS_OBJ_MARK);
    gx_code_space_t *code_space = &pdficmap->code_space;
    int nr = code_space->num_ranges;
    gx_code_space_range_t *gcsr = code_space->ranges;

    /* increment to_pop to cover the mark object */
    numranges = to_pop++;
    while (numranges % 2) numranges--;
    if (numranges > 200) {
        (void)pdf_ps_stack_pop(s, to_pop);
        return_error(gs_error_syntaxerror);
    }

    if (numranges > 0
     && pdf_ps_obj_has_type(&(s->cur[0]), PDF_PS_OBJ_STRING)  && s->cur[0].size <= MAX_CMAP_CODE_SIZE
     && pdf_ps_obj_has_type(&(s->cur[-1]), PDF_PS_OBJ_STRING) && s->cur[-1].size <= MAX_CMAP_CODE_SIZE) {

        code_space->num_ranges += numranges >> 1;

        code_space->ranges = (gx_code_space_range_t *)gs_alloc_byte_array(mem, code_space->num_ranges,
                          sizeof(gx_code_space_range_t), "cmap_endcodespacerange_func(ranges)");
        if (code_space->ranges != NULL) {
            if (nr > 0) {
                memcpy(code_space->ranges, gcsr, nr * sizeof(gx_code_space_range_t));
                gs_free_object(mem, gcsr, "cmap_endcodespacerange_func(gcsr");
            }

            for (i = nr; i < code_space->num_ranges; i++) {
                int si = i - nr;
                int s1 = s->cur[-((si * 2) + 1)].size < MAX_CMAP_CODE_SIZE ? s->cur[-((si * 2) + 1)].size : MAX_CMAP_CODE_SIZE;
                int s2 = s->cur[-(si * 2)].size < MAX_CMAP_CODE_SIZE ? s->cur[-(si * 2)].size : MAX_CMAP_CODE_SIZE;

                memcpy(code_space->ranges[i].first, s->cur[-((si * 2) + 1)].val.string, s1);
                memcpy(code_space->ranges[i].last, s->cur[-(si * 2)].val.string, s2);
                code_space->ranges[i].size = s->cur[-(si * 2)].size;
            }
        }
        else {
            (void)pdf_ps_stack_pop(s, to_pop);
            return_error(gs_error_VMerror);
        }
    }
    return pdf_ps_stack_pop(s, to_pop);
}

static int cmap_insert_map(pdfi_cmap_range_t *cmap_range, pdfi_cmap_range_map_t *pdfir)
{
    if (cmap_range->ranges == NULL) {
        cmap_range->ranges = cmap_range->ranges_tail = pdfir;
    }
    else {
        cmap_range->ranges_tail->next = pdfir;
        cmap_range->ranges_tail = pdfir;
    }
    cmap_range->numrangemaps++;
    return 0;
}

static int general_endcidrange_func(gs_memory_t *mem, pdf_ps_ctx_t *s, pdf_cmap *pdficmap, pdfi_cmap_range_t *cmap_range)
{
    int ncodemaps, to_pop = pdf_ps_stack_count_to_mark(s, PDF_PS_OBJ_MARK);
    int i, j;
    pdfi_cmap_range_map_t *pdfir;
    pdf_ps_stack_object_t *stobj;

    /* increment to_pop to cover the mark object */
    ncodemaps = to_pop++;
    /* mapping should have 3 objects on the stack:
     * startcode, endcode and basecid
     */
    while (ncodemaps % 3) ncodemaps--;
    if (ncodemaps > 300) {
        (void)pdf_ps_stack_pop(s, to_pop);
        return_error(gs_error_syntaxerror);
    }

    stobj = &s->cur[-ncodemaps] + 1;

    for (i = 0; i < ncodemaps; i += 3) {
        int preflen, valuelen;

        if (pdf_ps_obj_has_type(&(stobj[i + 2]), PDF_PS_OBJ_INTEGER)
        &&  pdf_ps_obj_has_type(&(stobj[i + 1]), PDF_PS_OBJ_STRING)
        &&  pdf_ps_obj_has_type(&(stobj[i]), PDF_PS_OBJ_STRING)){
            uint cidbase = stobj[i + 2].val.i;

            /* First, find the length of the prefix */
            for (preflen = 0; preflen < stobj[i].size; preflen++) {
                if(stobj[i].val.string[preflen] != stobj[i + 1].val.string[preflen]) {
                    break;
                }
            }

            if (preflen == stobj[i].size) {
                preflen = 1;
            }

            if (preflen > MAX_CMAP_CODE_SIZE || stobj[i].size - preflen > MAX_CMAP_CODE_SIZE || stobj[i + 1].size - preflen > MAX_CMAP_CODE_SIZE
                || stobj[i].size - preflen < 0 || stobj[i + 1].size - preflen < 0) {
                (void)pdf_ps_stack_pop(s, to_pop);
                return_error(gs_error_syntaxerror);
            }

            /* Find how many bytes we need for the cidbase value */
            /* We always store at least two bytes for the cidbase value */
            for (valuelen = 16; valuelen < 32 && (cidbase >> valuelen) > 0; valuelen += 1)
                DO_NOTHING;

            valuelen = ((valuelen + 7) & ~7) >> 3;

            /* The prefix is already directly in the gx_cmap_lookup_range_t
             * We need to store the lower and upper character codes, after lopping the prefix
             * off them. The upper and lower codes must be the same number of bytes.
             */
            j = sizeof(pdfi_cmap_range_map_t) + 2 * (stobj[i].size - preflen) + valuelen;

            pdfir = (pdfi_cmap_range_map_t *)gs_alloc_bytes(mem, j, "cmap_endcidrange_func(pdfi_cmap_range_map_t)");
            if (pdfir != NULL) {
                gx_cmap_lookup_range_t *gxr = &pdfir->range;
                pdfir->next = NULL;
                gxr->num_entries = 1;
                gxr->keys.data = (byte *)&(pdfir[1]);
                gxr->values.data = gxr->keys.data + 2 * (stobj[i].size - preflen);

                gxr->cmap = NULL;
                gxr->font_index = 0;
                gxr->key_is_range = true;
                gxr->value_type = cmap_range == &(pdficmap->cmap_range) ? CODE_VALUE_CID : CODE_VALUE_NOTDEF;
                gxr->key_prefix_size = preflen;
                gxr->key_size = stobj[i].size - gxr->key_prefix_size;
                memcpy(gxr->key_prefix, stobj[i].val.string, gxr->key_prefix_size);

                memcpy(gxr->keys.data, stobj[i].val.string + gxr->key_prefix_size, stobj[i].size - gxr->key_prefix_size);
                memcpy(gxr->keys.data + (stobj[i].size - gxr->key_prefix_size), stobj[i + 1].val.string + gxr->key_prefix_size, stobj[i + 1].size - gxr->key_prefix_size);

                gxr->keys.size = (stobj[i].size - gxr->key_prefix_size) + (stobj[i + 1].size - gxr->key_prefix_size);
                for (j = 0; j < valuelen; j++) {
                    gxr->values.data[j] = (cidbase >> ((valuelen - 1 - j) * 8)) & 255;
                }
                gxr->value_size = valuelen; /* I'm not sure.... */
                gxr->values.size = valuelen;
                if (cmap_insert_map(cmap_range, pdfir) < 0) break;
            }
            else {
                (void)pdf_ps_stack_pop(s, to_pop);
                return_error(gs_error_VMerror);
            }
        }
    }
    return pdf_ps_stack_pop(s, to_pop);
}

static int cmap_endcidrange_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    pdf_cmap *pdficmap = (pdf_cmap *)s->client_data;
    return general_endcidrange_func(mem, s, pdficmap, &pdficmap->cmap_range);
}

static int cmap_endnotdefrange_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    pdf_cmap *pdficmap = (pdf_cmap *)s->client_data;
    return general_endcidrange_func(mem, s, pdficmap, &pdficmap->notdef_cmap_range);
}

static int cmap_endfbrange_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    pdf_cmap *pdficmap = (pdf_cmap *)s->client_data;
    int ncodemaps, to_pop = pdf_ps_stack_count_to_mark(s, PDF_PS_OBJ_MARK);
    int i, j, k;
    pdfi_cmap_range_map_t *pdfir;
    pdf_ps_stack_object_t *stobj;

    /* increment to_pop to cover the mark object */
    ncodemaps = to_pop++;
    /* mapping should have 3 objects on the stack
     */
    while (ncodemaps % 3) ncodemaps--;

    if (ncodemaps > 300) {
        (void)pdf_ps_stack_pop(s, to_pop);
        return_error(gs_error_syntaxerror);
    }

    stobj = &s->cur[-ncodemaps] + 1;
    for (i = 0; i < ncodemaps; i += 3) {
        /* Lazy: to make the loop below simpler, put single
           values into a one element array
         */
        if (pdf_ps_obj_has_type(&(stobj[i + 2]), PDF_PS_OBJ_STRING)) {
            pdf_ps_stack_object_t *arr;
            arr = (pdf_ps_stack_object_t *) gs_alloc_bytes(mem, sizeof(pdf_ps_stack_object_t), "cmap_endfbrange_func(pdf_ps_stack_object_t");
            if (arr == NULL) {
                (void)pdf_ps_stack_pop(s, to_pop);
                return_error(gs_error_VMerror);
            }
            else {
                memcpy(arr, &(stobj[i + 2]), sizeof(pdf_ps_stack_object_t));
                pdf_ps_make_array(&(stobj[i + 2]), arr, 1);
            }
        }
    }

    stobj = &s->cur[-ncodemaps] + 1;

    for (i = 0; i < ncodemaps; i += 3) {
        int preflen, valuelen;

        if (pdf_ps_obj_has_type(&(stobj[i + 2]), PDF_PS_OBJ_ARRAY)
        &&  pdf_ps_obj_has_type(&(stobj[i + 1]), PDF_PS_OBJ_STRING)
        &&  pdf_ps_obj_has_type(&(stobj[i]), PDF_PS_OBJ_STRING)){
            uint cidbase;
            int m, size;

            if (stobj[i + 2].size < 1)
                continue;

            else if (stobj[i + 2].size == 1) {
                if (stobj[i + 2].val.arr[0].type != PDF_PS_OBJ_STRING)
                     continue;

                size = stobj[i + 2].val.arr[0].size;

                cidbase = 0;
                for (m = 0; m < size; m++) {
                    cidbase |= stobj[i + 2].val.arr[0].val.string[size - m - 1] << (8 * m);
                }

                /* First, find the length of the prefix */
                for (preflen = 0; preflen < stobj[i].size; preflen++) {
                    if(stobj[i].val.string[preflen] != stobj[i + 1].val.string[preflen]) {
                        break;
                    }
                }

                if (preflen == stobj[i].size) {
                    preflen = 1;
                }

                if (preflen > MAX_CMAP_CODE_SIZE || stobj[i].size - preflen > MAX_CMAP_CODE_SIZE || stobj[i + 1].size - preflen > MAX_CMAP_CODE_SIZE
                    || stobj[i].size - preflen < 0 || stobj[i + 1].size - preflen < 0) {
                    (void)pdf_ps_stack_pop(s, to_pop);
                    return_error(gs_error_syntaxerror);
                }

                /* Find how many bytes we need for the cidbase value */
                /* We always store at least two bytes for the cidbase value */
                for (valuelen = 16; valuelen < 32 && (cidbase >> valuelen) > 0; valuelen += 1)
                    DO_NOTHING;

                valuelen = ((valuelen + 7) & ~7) >> 3;

                /* The prefix is already directly in the gx_cmap_lookup_range_t
                 * We need to store the lower and upper character codes, after lopping the prefix
                 * off them. The upper and lower codes must be the same number of bytes.
                 */
                j = sizeof(pdfi_cmap_range_map_t) + 2 * (stobj[i].size - preflen) + valuelen;

                pdfir = (pdfi_cmap_range_map_t *)gs_alloc_bytes(mem, j, "cmap_endcidrange_func(pdfi_cmap_range_map_t)");
                if (pdfir != NULL) {
                    gx_cmap_lookup_range_t *gxr = &pdfir->range;
                    pdfir->next = NULL;
                    gxr->num_entries = 1;
                    gxr->keys.data = (byte *)&(pdfir[1]);
                    gxr->values.data = gxr->keys.data + 2 * (stobj[i].size - preflen);

                    gxr->cmap = NULL;
                    gxr->font_index = 0;
                    gxr->key_is_range = true;
                    gxr->value_type = CODE_VALUE_CID;
                    gxr->key_prefix_size = preflen;
                    gxr->key_size = stobj[i].size - gxr->key_prefix_size;
                    memcpy(gxr->key_prefix, stobj[i].val.string, gxr->key_prefix_size);

                    memcpy(gxr->keys.data, stobj[i].val.string + gxr->key_prefix_size, stobj[i].size - gxr->key_prefix_size);
                    memcpy(gxr->keys.data + (stobj[i].size - gxr->key_prefix_size), stobj[i + 1].val.string + gxr->key_prefix_size, stobj[i + 1].size - gxr->key_prefix_size);

                    gxr->keys.size = (stobj[i].size - gxr->key_prefix_size) + (stobj[i + 1].size - gxr->key_prefix_size);
                    for (j = 0; j < valuelen; j++) {
                        gxr->values.data[j] = (cidbase >> ((valuelen - 1 - j) * 8)) & 255;
                    }
                    gxr->value_size = valuelen; /* I'm not sure.... */
                    gxr->values.size = valuelen;
                    if (cmap_insert_map(&pdficmap->cmap_range, pdfir) < 0) break;
                }
                else {
                    (void)pdf_ps_stack_pop(s, to_pop);
                    return_error(gs_error_VMerror);
                }
            }
            else {
                int m, size, keysize;
                uint codelo = 0, codehi = 0;

                size = stobj[i].size;
                for (m = 0; m < size; m++) {
                    codelo |= stobj[i].val.string[size - m - 1] << (8 * m);
                }
                size = stobj[i + 1].size;
                for (m = 0; m < size; m++) {
                    codehi |= stobj[i + 1].val.string[size - m - 1] << (8 * m);
                }

                if (codehi <= codelo || stobj[i + 2].size < (codehi - codelo))
                    continue;

                for (k = codelo; k <= codehi; k++) {
                    uint cidbase;
                    int ind = k - codelo;

                    if (stobj[i + 2].val.arr[ind].type != PDF_PS_OBJ_STRING)
                         continue;

                    size = stobj[i + 2].val.arr[ind].size;

                    cidbase = 0;
                    for (m = 0; m < size; m++) {
                        cidbase |= stobj[i + 2].val.arr[ind].val.string[size - m - 1] << (8 * m);
                    }
                    /* Find how many bytes we need for the cidbase value */
                    /* We always store at least two bytes for the cidbase value */
                    for (valuelen = 16; valuelen < 32 && (cidbase >> valuelen) > 0; valuelen += 1)
                        DO_NOTHING;

                    valuelen = ((valuelen + 7) & ~7) >> 3;

                    /* Find how many bytes we need for the cidbase value */
                    /* We always store at least two bytes for the cidbase value */
                    for (keysize = 16; keysize < 32 && (cidbase >> keysize) > 0; keysize += 1)
                        DO_NOTHING;

                    keysize = ((keysize + 7) & ~7) >> 3;
                    if (keysize > MAX_CMAP_CODE_SIZE * 2) {
                        (void)pdf_ps_stack_pop(s, to_pop);
                        return_error(gs_error_syntaxerror);
                    }
                    preflen = keysize > 4 ? 4 : keysize;
                    keysize -= preflen;

                    /* The prefix is already directly in the gx_cmap_lookup_range_t
                     * We need to store the lower and upper character codes, after lopping the prefix
                     * off them. The upper and lower codes must be the same number of bytes.
                     */
                    j = sizeof(pdfi_cmap_range_map_t) + keysize + valuelen;

                    pdfir = (pdfi_cmap_range_map_t *)gs_alloc_bytes(mem, j, "cmap_endcidrange_func(pdfi_cmap_range_map_t)");
                    if (pdfir != NULL) {
                        gx_cmap_lookup_range_t *gxr = &pdfir->range;
                        pdfir->next = NULL;
                        gxr->num_entries = 1;
                        gxr->keys.data = (byte *)&(pdfir[1]);
                        gxr->values.data = gxr->keys.data + keysize;

                        gxr->cmap = NULL;
                        gxr->font_index = 0;
                        gxr->key_is_range = false;
                        gxr->value_type = CODE_VALUE_CID;
                        gxr->key_prefix_size = preflen;
                        gxr->key_size = keysize;
                        for (j = 0; j < preflen; j++) {
                            gxr->key_prefix[j] = (k >> ((preflen - 1 - j) * 8)) & 255;
                        }

                        for (j = preflen; j < preflen + keysize; j++) {
                            gxr->keys.data[j] = (k >> (((preflen + keysize) - 1 - j) * 8)) & 255;
                        }

                        gxr->keys.size = keysize;
                        for (j = 0; j < valuelen; j++) {
                            gxr->values.data[j] = (cidbase >> ((valuelen - 1 - j) * 8)) & 255;
                        }
                        gxr->value_size = valuelen; /* I'm not sure.... */
                        gxr->values.size = valuelen;
                        if (cmap_insert_map(&pdficmap->cmap_range, pdfir) < 0) break;
                    }
                    else {
                        (void)pdf_ps_stack_pop(s, to_pop);
                        return_error(gs_error_VMerror);
                    }
                }
            }
        }
    }
    return pdf_ps_stack_pop(s, to_pop);
}

static int general_endcidchar_func(gs_memory_t *mem, pdf_ps_ctx_t *s, pdf_cmap *pdficmap, pdfi_cmap_range_t *cmap_range)
{
    int ncodemaps, to_pop = pdf_ps_stack_count_to_mark(s, PDF_PS_OBJ_MARK);
    int i, j;
    pdfi_cmap_range_map_t *pdfir;
    pdf_ps_stack_object_t *stobj;

    /* increment to_pop to cover the mark object */
    ncodemaps = to_pop++;
    /* mapping should have 2 objects on the stack:
     * startcode, endcode and basecid
     */
    while (ncodemaps % 2) ncodemaps--;

    if (ncodemaps > 200) {
        (void)pdf_ps_stack_pop(s, to_pop);
        return_error(gs_error_syntaxerror);
    }

    stobj = &s->cur[-ncodemaps] + 1;

    for (i = 0; i < ncodemaps; i += 2) {
        int preflen = 1, valuelen;

        if (pdf_ps_obj_has_type(&(stobj[i + 1]), PDF_PS_OBJ_INTEGER)
        &&  pdf_ps_obj_has_type(&(stobj[i]), PDF_PS_OBJ_STRING) && stobj[i].size > 0) {
            uint cidbase = stobj[i + 1].val.i;

            /* Find how many bytes we need for the cidbase value */
            /* We always store at least two bytes for the cidbase value */
            for (valuelen = 16; valuelen < 32 && (cidbase >> valuelen) > 0; valuelen += 1)
                DO_NOTHING;

            preflen = stobj[i].size > 4 ? 4 : stobj[i].size;

            valuelen = ((valuelen + 7) & ~7) >> 3;

            /* The prefix is already directly in the gx_cmap_lookup_range_t
             * We need to store the lower and upper character codes, after lopping the prefix
             * off them. The upper and lower codes must be the same number of bytes.
             */
            j = sizeof(pdfi_cmap_range_map_t) + (stobj[i].size - preflen) + valuelen;

            pdfir = (pdfi_cmap_range_map_t *)gs_alloc_bytes(mem, j, "cmap_endcidrange_func(pdfi_cmap_range_map_t)");
            if (pdfir != NULL) {
                gx_cmap_lookup_range_t *gxr = &pdfir->range;
                pdfir->next = NULL;
                gxr->num_entries = 1;
                gxr->keys.data = (byte *)&(pdfir[1]);
                gxr->values.data = gxr->keys.data + (stobj[i].size - preflen);

                gxr->cmap = NULL;
                gxr->font_index = 0;
                gxr->key_is_range = false;
                gxr->value_type = cmap_range == &(pdficmap->cmap_range) ? CODE_VALUE_CID : CODE_VALUE_NOTDEF;
                gxr->key_prefix_size = preflen;
                gxr->key_size = stobj[i].size - gxr->key_prefix_size;
                memcpy(gxr->key_prefix, stobj[i].val.string, gxr->key_prefix_size);

                memcpy(gxr->keys.data, stobj[i].val.string + gxr->key_prefix_size, stobj[i].size - gxr->key_prefix_size);

                gxr->keys.size = stobj[i].size - gxr->key_prefix_size;
                for (j = 0; j < valuelen; j++) {
                    gxr->values.data[j] = (cidbase >> ((valuelen - 1 - j) * 8)) & 255;
                }
                gxr->value_size = valuelen; /* I'm not sure.... */
                gxr->values.size = valuelen;
                if (cmap_insert_map(cmap_range, pdfir) < 0) break;
            }
            else {
                (void)pdf_ps_stack_pop(s, to_pop);
                return_error(gs_error_VMerror);
            }
        }
    }
    return pdf_ps_stack_pop(s, to_pop);
}

static int cmap_endcidchar_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    pdf_cmap *pdficmap = (pdf_cmap *)s->client_data;
    return general_endcidchar_func(mem, s, pdficmap, &pdficmap->cmap_range);
}

static int cmap_endnotdefchar_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    pdf_cmap *pdficmap = (pdf_cmap *)s->client_data;
    return general_endcidchar_func(mem, s, pdficmap, &pdficmap->notdef_cmap_range);
}

static int cmap_endbfchar_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    pdf_cmap *pdficmap = (pdf_cmap *)s->client_data;
    int ncodemaps = pdf_ps_stack_count_to_mark(s, PDF_PS_OBJ_MARK);
    pdf_ps_stack_object_t *stobj;
    int i, j;

    if (ncodemaps > 200) {
        (void)pdf_ps_stack_pop(s, ncodemaps);
        return_error(gs_error_syntaxerror);
    }

    stobj = &s->cur[-ncodemaps] + 1;

    for (i = 0; i < ncodemaps; i += 2) {
        if (pdf_ps_obj_has_type(&(stobj[i + 1]), PDF_PS_OBJ_STRING)) {
            byte *c = stobj[i + 1].val.string;
            int l = stobj[i + 1].size;
            unsigned int v = 0;

            for (j = 0; j < l; j++) {
                v += c[l - j - 1] << (8 * j);
            }
            pdf_ps_make_int(&(stobj[i + 1]), v);
        }
        else {
            continue;
        }
    }
    return general_endcidchar_func(mem, s, pdficmap, &pdficmap->cmap_range);
}

#define CMAP_NAME_AND_LEN(s) PDF_PS_OPER_NAME_AND_LEN(s)

static int cmap_def_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    int code = 0, code2 = 0;
    pdf_cmap *pdficmap = (pdf_cmap *)s->client_data;

    if (pdf_ps_stack_count(s) < 2) {
        return pdf_ps_stack_pop(s, 1);
    }

    if (pdf_ps_obj_has_type(&s->cur[-1], PDF_PS_OBJ_NAME)) {
        if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("Registry"))) {
            pdficmap->csi_reg.data = gs_alloc_bytes(mem, s->cur[0].size + 1, "cmap_def_func(Registry)");
            if (pdficmap->csi_reg.data != NULL) {
                pdficmap->csi_reg.size = s->cur[0].size;
                if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_STRING)) {
                    memcpy(pdficmap->csi_reg.data, s->cur[0].val.string, s->cur[0].size);
                }
                else {
                    memcpy(pdficmap->csi_reg.data, s->cur[0].val.name, s->cur[0].size);
                }
                pdficmap->csi_reg.data[pdficmap->csi_reg.size] = '\0';
            }
            else {
                code = gs_note_error(gs_error_VMerror);
            }
        }
        else if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("Ordering"))) {
            pdficmap->csi_ord.data = gs_alloc_bytes(mem, s->cur[0].size + 1, "cmap_def_func(Ordering)");
            if (pdficmap->csi_ord.data != NULL) {
                pdficmap->csi_ord.size = s->cur[0].size;
                if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_STRING))
                    memcpy(pdficmap->csi_ord.data, s->cur[0].val.string, s->cur[0].size);
                else
                    memcpy(pdficmap->csi_ord.data, s->cur[0].val.name, s->cur[0].size);
                pdficmap->csi_ord.data[pdficmap->csi_ord.size] = '\0';
            }
            else {
                code = gs_note_error(gs_error_VMerror);
            }
        }
        else if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("Supplement"))) {
            if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER)) {
                pdficmap->csi_supplement = s->cur[0].val.i;
            }
            else {
                pdficmap->csi_supplement = 0;
            }
        }
        else if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("CMapName"))) {
            pdficmap->name.data = gs_alloc_bytes(mem, s->cur[0].size + 1, "cmap_def_func(CMapName)");
            if (pdficmap->name.data != NULL) {
                pdficmap->name.size = s->cur[0].size;
                if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_STRING))
                    memcpy(pdficmap->name.data, s->cur[0].val.string, s->cur[0].size);
                else
                    memcpy(pdficmap->name.data, s->cur[0].val.name, s->cur[0].size);
                pdficmap->name.data[pdficmap->name.size] = '\0';
            }
            else {
                code = gs_note_error(gs_error_VMerror);
            }
        }
        else if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("CMapVersion"))) {
            if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER)) {
                pdficmap->vers = (float)s->cur[0].val.i;
            }
            else if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_FLOAT)){
                pdficmap->vers = s->cur[0].val.f;
            }
            else {
                pdficmap->vers = (float)0;
            }
        }
        else if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("CMapType"))) {
            if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER)) {
                pdficmap->cmaptype = s->cur[0].val.i;
            }
            else {
                pdficmap->cmaptype = 1;
            }
        }
        else if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("XUID"))) {
            if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_ARRAY)) {
                int len = s->cur->size;
                pdficmap->uid.xvalues = (long *)gs_alloc_bytes(mem, len * sizeof(*pdficmap->uid.xvalues), "cmap_def_func(XUID)");
                if (pdficmap->uid.xvalues != NULL) {
                     int i;
                     pdf_ps_stack_object_t *a = s->cur->val.arr;
                     pdficmap->uid.id = -len;
                     for (i = 0; i < len; i++) {
                         if (pdf_ps_obj_has_type(&a[i], PDF_PS_OBJ_INTEGER)) {
                             pdficmap->uid.xvalues[i] = (long)a[i].val.i;
                         }
                         else {
                             pdficmap->uid.xvalues[i] = 0;
                         }
                     }
                }
                else {
                    code = gs_note_error(gs_error_VMerror);
                }
            }
        }
        else if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("WMode"))) {
            if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER)) {
                if (s->cur[0].val.i != 0) {
                    if (s->cur[0].val.i != 1)
                        pdfi_set_warning(s->pdfi_ctx, 0, NULL, W_PDF_BAD_WMODE, "cmap_def_func", NULL);
                    pdficmap->wmode = 1;
                }
                else
                    pdficmap->wmode = 0;
            }
            else {
                pdficmap->wmode = 0;
            }
        }
    }

    code2 = pdf_ps_stack_pop(s, 2);
    if (code < 0)
        return code;
    else
        return code2;
}

static pdf_ps_oper_list_t cmap_oper_list[] =
{
  {PDF_PS_OPER_NAME_AND_LEN("usecmap"), cmap_usecmap_func},
  {PDF_PS_OPER_NAME_AND_LEN("usefont"), ps_pdf_null_oper_func},
  {PDF_PS_OPER_NAME_AND_LEN("beginusematrix"), ps_pdf_null_oper_func},
  {PDF_PS_OPER_NAME_AND_LEN("endusematrix"), ps_pdf_null_oper_func},
  {PDF_PS_OPER_NAME_AND_LEN("begincodespacerange"), pdf_ps_pop_and_pushmark_func},
  {PDF_PS_OPER_NAME_AND_LEN("endcodespacerange"), cmap_endcodespacerange_func},
  {PDF_PS_OPER_NAME_AND_LEN("begincmap"), ps_pdf_null_oper_func},
  {PDF_PS_OPER_NAME_AND_LEN("beginbfchar"), pdf_ps_pop_and_pushmark_func},
  {PDF_PS_OPER_NAME_AND_LEN("endbfchar"), cmap_endbfchar_func},
  {PDF_PS_OPER_NAME_AND_LEN("beginbfrange"), pdf_ps_pop_and_pushmark_func},
  {PDF_PS_OPER_NAME_AND_LEN("endbfrange"), cmap_endfbrange_func},
  {PDF_PS_OPER_NAME_AND_LEN("begincidchar"), pdf_ps_pop_and_pushmark_func},
  {PDF_PS_OPER_NAME_AND_LEN("endcidchar"), cmap_endcidchar_func},
  {PDF_PS_OPER_NAME_AND_LEN("begincidrange"), pdf_ps_pop_and_pushmark_func},
  {PDF_PS_OPER_NAME_AND_LEN("endcidrange"), cmap_endcidrange_func},
  {PDF_PS_OPER_NAME_AND_LEN("beginnotdefchar"), pdf_ps_pop_and_pushmark_func},
  {PDF_PS_OPER_NAME_AND_LEN("endnotdefchar"), cmap_endnotdefchar_func},
  {PDF_PS_OPER_NAME_AND_LEN("beginnotdefrange"), pdf_ps_pop_and_pushmark_func},
  {PDF_PS_OPER_NAME_AND_LEN("endnotdefrange"), cmap_endnotdefrange_func},
  {PDF_PS_OPER_NAME_AND_LEN("findresource"), clear_stack_oper_func},
  {PDF_PS_OPER_NAME_AND_LEN("dict"), pdf_ps_pop_oper_func},
  {PDF_PS_OPER_NAME_AND_LEN("begin"), ps_pdf_null_oper_func},
  {PDF_PS_OPER_NAME_AND_LEN("end"), ps_pdf_null_oper_func},
  {PDF_PS_OPER_NAME_AND_LEN("pop"), ps_pdf_null_oper_func},
  {PDF_PS_OPER_NAME_AND_LEN("def"), cmap_def_func},
  {PDF_PS_OPER_NAME_AND_LEN("dup"), ps_pdf_null_oper_func},
  {PDF_PS_OPER_NAME_AND_LEN("defineresource"), clear_stack_oper_func},
  {PDF_PS_OPER_NAME_AND_LEN("beginrearrangedfont"), clear_stack_oper_func}, /* we should never see this */
  {NULL, 0, NULL}
};

static int
pdf_cmap_open_file(pdf_context *ctx, gs_string *cmap_name, byte **buf, int64_t *buflen)
{
    int code = 0;
    stream *s;
    char fname[gp_file_name_sizeof];
    const char *path_pfx = "CMap/";
    fname[0] = '\0';

    if (strlen(path_pfx) + cmap_name->size >= gp_file_name_sizeof)
        return_error(gs_error_rangecheck);

    strncat(fname, path_pfx, strlen(path_pfx));
    strncat(fname, (char *)cmap_name->data, cmap_name->size);
    code = pdfi_open_resource_file(ctx, (const char *)fname, (const int)strlen(fname), &s);
    if (code >= 0) {
        sfseek(s, 0, SEEK_END);
        *buflen = sftell(s);
        sfseek(s, 0, SEEK_SET);
        *buf = gs_alloc_bytes(ctx->memory, *buflen, "pdf_cmap_open_file(buf)");
        if (*buf != NULL) {
            sfread(*buf, 1, *buflen, s);
        }
        else {
            code = gs_note_error(gs_error_VMerror);
        }
        sfclose(s);
    }
    return code;
}

static int
pdfi_make_gs_cmap(gs_memory_t *mem, pdf_cmap *pdficmap)
{
    int code = 0, i;
    gs_cmap_adobe1_t *pgscmap = 0;
    gx_cmap_lookup_range_t *lookups, *ndlookups = NULL;
    pdfi_cmap_range_map_t *l;
    /* FIXME: We have to use gs_cmap_adobe1_alloc() to get the cmap procs
       but even if it gets passed num_ranges = 0 it still allocates
       a zero length array. Change gs_cmap_adobe1_alloc to work better
     */
    if ((code = gs_cmap_adobe1_alloc(&pgscmap, pdficmap->wmode,
                                     pdficmap->name.data,
                                     pdficmap->name.size,
                                     1,
                                     0, 0, 0, 0, 0, mem)) >= 0) {
        gs_free_object(mem, pgscmap->code_space.ranges, "empty ranges");

        lookups = gs_alloc_struct_array(mem, pdficmap->cmap_range.numrangemaps,
                               gx_cmap_lookup_range_t,
                               &st_cmap_lookup_range_element,
                               "pdfi_make_gs_cmap(lookup ranges)");
        if (lookups == NULL) {
            gs_free_object(mem, pgscmap, "pdfi_make_gs_cmap(pgscmap)");
            code = gs_note_error(gs_error_VMerror);
            goto done;
        }
        if (pdficmap->notdef_cmap_range.numrangemaps > 0){
            ndlookups = gs_alloc_struct_array(mem, pdficmap->notdef_cmap_range.numrangemaps,
                               gx_cmap_lookup_range_t,
                               &st_cmap_lookup_range_element,
                               "pdfi_make_gs_cmap(notdef lookup ranges)");
            if (ndlookups == NULL) {
                gs_free_object(mem, lookups, "pdfi_make_gs_cmap(lookups)");
                gs_free_object(mem, pgscmap, "pdfi_make_gs_cmap(pgscmap)");
                code = gs_note_error(gs_error_VMerror);
                goto done;
            }
        }
        pgscmap->def.lookup = lookups;
        pgscmap->def.num_lookup = pdficmap->cmap_range.numrangemaps;
        pgscmap->notdef.lookup = ndlookups;
        pgscmap->notdef.num_lookup = pdficmap->notdef_cmap_range.numrangemaps;

        pgscmap->CIDSystemInfo[0].Registry.data = pdficmap->csi_reg.data;
        pgscmap->CIDSystemInfo[0].Registry.size = pdficmap->csi_reg.size;
        pgscmap->CIDSystemInfo[0].Ordering.data = pdficmap->csi_ord.data;
        pgscmap->CIDSystemInfo[0].Ordering.size = pdficmap->csi_ord.size;
        pgscmap->CIDSystemInfo[0].Supplement = pdficmap->csi_supplement;
        memcpy(&pgscmap->code_space, &pdficmap->code_space, sizeof(pgscmap->code_space));
        memcpy(&pgscmap->uid, &pdficmap->uid, sizeof(pdficmap->uid));
        l = pdficmap->cmap_range.ranges;
        for (i = 0; i < pdficmap->cmap_range.numrangemaps && l != NULL; i++) {
            memcpy(&lookups[i], &l->range, sizeof(gx_cmap_lookup_range_t));
            l = l->next;
        }

        l = pdficmap->notdef_cmap_range.ranges;
        for (i = 0; i < pdficmap->notdef_cmap_range.numrangemaps && l != NULL; i++) {
            memcpy(&ndlookups[i], &l->range, sizeof(gx_cmap_lookup_range_t));
            l = l->next;
        }

        pdficmap->gscmap = pgscmap;
    }

done:
    return code;
}

int
pdfi_read_cmap(pdf_context *ctx, pdf_obj *cmap, pdf_cmap **pcmap)
{
    int code = 0;
    pdf_cmap pdficm[3] = {0};
    pdf_cmap *pdfi_cmap = &(pdficm[1]);
    byte *buf = NULL;
    int64_t buflen = 0;
    pdf_ps_ctx_t cmap_ctx;

    pdfi_cmap->ctx = ctx;
    switch (pdfi_type_of(cmap)) {
        case PDF_NAME:
        {
            gs_string cmname;
            pdf_name *cmapn = (pdf_name *)cmap;
            cmname.data = cmapn->data;
            cmname.size = cmapn->length;
            code = pdf_cmap_open_file(ctx, &cmname, &buf, &buflen);
            if (code < 0)
                goto error_out;
            break;
        }
        case PDF_STREAM:
        {
            pdf_obj *ucmap;
            pdf_cmap *upcmap = NULL;
            pdf_dict *cmap_dict = NULL;

            code = pdfi_dict_from_obj(ctx, cmap, &cmap_dict);
            if (code < 0)
                goto error_out;

            code = pdfi_dict_knownget(ctx, cmap_dict, "UseCMap", &ucmap);
            if (code > 0) {
                code = pdfi_read_cmap(ctx, ucmap, &upcmap);
                pdfi_countdown(ucmap);
                if (code >= 0) {
                    gx_code_space_range_t * ranges =
                         (gx_code_space_range_t *)gs_alloc_byte_array(ctx->memory, upcmap->code_space.num_ranges,
                          sizeof(gx_code_space_range_t), "cmap_usecmap_func(ranges)");
                    if (ranges != NULL) {
                        int i;
                        memcpy(&pdfi_cmap->code_space, &upcmap->code_space, sizeof(pdfi_cmap->code_space));
                        for (i = 0; i < upcmap->code_space.num_ranges; i++) {
                            memcpy(&(ranges[i]), &(upcmap->code_space.ranges[i]), sizeof(ranges[i]));
                        }
                        pdfi_cmap->code_space.ranges = ranges;
                        memcpy(&pdfi_cmap->cmap_range, &upcmap->cmap_range, sizeof(pdfi_cmap->cmap_range));
                        memcpy(&pdfi_cmap->notdef_cmap_range, &upcmap->notdef_cmap_range, sizeof(pdfi_cmap->notdef_cmap_range));
                        /* Once we've assumed control of these, NULL out entries for the sub-cmap. */
                        upcmap->cmap_range.ranges = NULL;
                        upcmap->notdef_cmap_range.ranges = NULL;
                        /* But we keep the subcmap itself because we rely on its storage */
                        pdfi_cmap->next = upcmap;
                    }
                }
                else {
                    pdfi_countdown(upcmap);
                }
            }

            code = pdfi_stream_to_buffer(ctx, (pdf_stream *)cmap, &buf, &buflen);
            if (code < 0) {
                goto error_out;
            }
            break;
        }
        default:
            code = gs_note_error(gs_error_typecheck);
            goto error_out;
    }
    pdfi_cmap->ctx = ctx;
    pdfi_cmap->buf = buf;
    pdfi_cmap->buflen = buflen;

    /* In case of technically invalid CMap files which do not contain a CMapType, See Bug #690737.
     * This makes sure we clean up the CMap contents in pdfi_free_cmap() below.
     */
    pdfi_cmap->cmaptype = 1;

    pdfi_pscript_stack_init(ctx, cmap_oper_list, (void *)pdfi_cmap, &cmap_ctx);

    code = pdfi_pscript_interpret(&cmap_ctx, buf, buflen);
    pdfi_pscript_stack_finit(&cmap_ctx);
    if (code < 0) goto error_out;

    code = pdfi_make_gs_cmap(ctx->memory, pdfi_cmap);

    if (code >= 0) {
        *pcmap = (pdf_cmap *)gs_alloc_bytes(ctx->memory, sizeof(pdf_cmap), "pdfi_read_cmap(*pcmap)");
        if (*pcmap != NULL) {
            pdfi_cmap->type = PDF_CMAP;
            pdfi_cmap->ctx = ctx;
            pdfi_cmap->refcnt = 1;
            pdfi_cmap->object_num = cmap->object_num;
            pdfi_cmap->generation_num = cmap->generation_num;
            pdfi_cmap->indirect_num = cmap->indirect_num;
            pdfi_cmap->indirect_gen = cmap->indirect_gen;
            memcpy(*pcmap, pdfi_cmap, sizeof(pdf_cmap));
            pdfi_cmap = *pcmap;
            /* object_num can be zero if the dictionary was defined inline */
            if (pdfi_cmap->object_num != 0) {
                code = replace_cache_entry(ctx, (pdf_obj *)pdfi_cmap);
            }
        }
    }
    else {
        goto error_out;
    }
    return 0;

error_out:
    pdfi_free_cmap_contents(pdfi_cmap);
    memset(pdfi_cmap, 0x00, sizeof(pdf_cmap));
    return code;
}

static int pdfi_free_cmap_contents(pdf_cmap *cmap)
{
    pdfi_cmap_range_map_t *pdfir;
    gs_cmap_adobe1_t *pgscmap = cmap->gscmap;

    if (pgscmap != NULL) {
        gs_free_object(OBJ_MEMORY(cmap), pgscmap->def.lookup, "pdfi_free_cmap(def.lookup)");
        gs_free_object(OBJ_MEMORY(cmap), pgscmap->notdef.lookup, "pdfi_free_cmap(notdef.lookup)");
        (void)gs_cmap_free((gs_cmap_t *)pgscmap, OBJ_MEMORY(cmap));
    }
    gs_free_object(OBJ_MEMORY(cmap), cmap->code_space.ranges, "pdfi_free_cmap(code_space.ranges");
    pdfir = cmap->cmap_range.ranges;
    while (pdfir != NULL) {
        pdfi_cmap_range_map_t *pdfir2 = pdfir->next;
        gs_free_object(OBJ_MEMORY(cmap), pdfir, "pdfi_free_cmap(cmap_range.ranges");
        pdfir = pdfir2;
    }
    pdfir = cmap->notdef_cmap_range.ranges;
    while (pdfir != NULL) {
        pdfi_cmap_range_map_t *pdfir2 = pdfir->next;
        gs_free_object(OBJ_MEMORY(cmap), pdfir, "pdfi_free_cmap(cmap_range.ranges");
        pdfir = pdfir2;
    }
    gs_free_object(OBJ_MEMORY(cmap), cmap->csi_reg.data, "pdfi_free_cmap(csi_reg.data");
    gs_free_object(OBJ_MEMORY(cmap), cmap->csi_ord.data, "pdfi_free_cmap(csi_ord.data");
    gs_free_object(OBJ_MEMORY(cmap), cmap->name.data, "pdfi_free_cmap(name.data");
    gs_free_object(OBJ_MEMORY(cmap), cmap->uid.xvalues, "pdfi_free_cmap(xuid.xvalues");
    pdfi_countdown(cmap->next);
    gs_free_object(OBJ_MEMORY(cmap), cmap->buf, "pdfi_free_cmap(cmap->buf");

    return 0;
}

int pdfi_free_cmap(pdf_obj *cmapo)
{
    pdf_cmap *cmap = (pdf_cmap *)cmapo;
    /*
     * Note there is some inconsistency in the various specifications regarding CMapType; the
     * Adobe tech note 5014 specifically says it only documents CMaps with a CmapType of 0, the
     * PLRM says that CMapType can be 0 or 1, and the two are equivalent, the PDF Reference Manual
     * doesn't say, it just refers to tech note 5014 but the example has a CMapType of 1. The PDF
     * Reference does describe ToUnicode CMaps which have a CMapType of 2.
     */
    /* Well it seems we have PDF files which use CMapType 2 CMaps as values for a /Encoding, which is
     * I believe incorrect, as these are ToUnicode CMaps....
     * There's nothing for it, we'll just haev to free all CMaps for now. Note for Chris when implementing
     * ToUnicode CMaps, we'll obviously have to rely on the context to know whether a CMap is an Encoding
     * or a ToUnicode, we cna't use the CmMapType, just as you suspected. :-(
     * See bug #696449 633_R00728_E.pdf
     */
    /*
     * For now, we represent ToUnicode CMaps (CMapType 2) in the same data structures as regular CMaps
     * (CMapType 0/1) so there is no reason (yet!) to differentiate between the two.
     */

    pdfi_free_cmap_contents(cmap);
    gs_free_object(OBJ_MEMORY(cmap), cmap, "pdfi_free_cmap(cmap");
    return 0;
}
