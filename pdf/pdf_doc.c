/* Copyright (C) 2020-2024 Artifex Software, Inc.
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

/* Functions to deal with PDF structure, such as retrieving
 * the Info, Catalog, Root dictionaries, and finding resources
 * and page dictionaries.
 */

#include "ghostpdf.h"
#include "pdf_stack.h"
#include "pdf_deref.h"
#include "pdf_array.h"
#include "pdf_dict.h"
#include "pdf_loop_detect.h"
#include "pdf_misc.h"
#include "pdf_repair.h"
#include "pdf_doc.h"
#include "pdf_mark.h"
#include "pdf_colour.h"
#include "pdf_device.h"

int pdfi_read_Root(pdf_context *ctx)
{
    pdf_obj *o, *o1;
    pdf_dict *d;
    int code;

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "%% Reading Root dictionary\n");

    /* Unusual code. This is because if the entry in the trailer dictionary causes
     * us to repair the file, the Trailer dictionary in the context can be replaced.
     * This counts it down and frees it, potentially while pdfi_dict_get is still
     * using it! Rather than countup and down in the dict_get routine, which is
     * normally unnecessary, count it up and down round the access here.
     */
    d = ctx->Trailer;
    pdfi_countup(d);
    code = pdfi_dict_get(ctx, d, "Root", &o1);
    if (code < 0) {
        pdfi_countdown(d);
        return code;
    }
    pdfi_countdown(d);

    if (pdfi_type_of(o1) == PDF_INDIRECT) {
        code = pdfi_dereference(ctx, ((pdf_indirect_ref *)o1)->ref_object_num,  ((pdf_indirect_ref *)o1)->ref_generation_num, &o);
        pdfi_countdown(o1);
        if (code < 0)
            return code;

        if (pdfi_type_of(o) != PDF_DICT) {
            pdfi_countdown(o);
            return_error(gs_error_typecheck);
        }

        code = pdfi_dict_put(ctx, ctx->Trailer, "Root", o);
        if (code < 0) {
            pdfi_countdown(o);
            return code;
        }
        o1 = o;
    } else {
        if (pdfi_type_of(o1) != PDF_DICT) {
            pdfi_countdown(o1);
            if (ctx->Root == NULL)
                return_error(gs_error_typecheck);
            return 0;
        }
    }

    code = pdfi_dict_get_type(ctx, (pdf_dict *)o1, "Type", PDF_NAME, &o);
    if (code < 0) {
        bool known = false;

        if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_MISSINGTYPE, "pdfi_read_Root", NULL)) < 0) {
            pdfi_countdown(o1);
            return code;
        }

        /* Missing the *required* /Type key! See if it has /Pages at least, if it does carry on */
        code = pdfi_dict_known(ctx, (pdf_dict *)o1, "Pages", &known);
        if (code < 0 || known == false) {
            pdfi_countdown(o1);
            return code;
        }
    }
    else {
        if (pdfi_name_strcmp((pdf_name *)o, "Catalog") != 0){
            pdf_obj *pages = NULL;

            /* Bug #706038 has a Root dictionary with /Type /Calalog which (of course) Acrobat
             * happily opens :-( So if we find a /Type and it's not /Catalog, try and see if
             * we have a /Pages key (the only other required entry). If we do assume that the
             * Type is wrong and use this dictionary, otherwise fall back to seeing if we
             * have a repaired Root. See above for handling a missing /Type.....
             */
            code = pdfi_dict_get_type(ctx, (pdf_dict *)o1, "Pages", PDF_DICT, &pages);
            if (code < 0) {
                pdfi_countdown(o);
                pdfi_countdown(o1);
                /* If we repaired the file, we may already have spotted a potential Root dictionary
                 * so if the one we found here isn't valid, try the one we found when scanning
                 */
                if (ctx->Root == NULL) {
                    pdfi_set_error(ctx, 0, NULL, E_PDF_NO_ROOT, "pdfi_read_Root", NULL);
                    return_error(gs_error_syntaxerror);
                }
                return 0;
            }
            pdfi_countdown(pages);
            if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_typecheck), NULL, E_PDF_BAD_ROOT_TYPE, "pdfi_read_Root", NULL)) < 0) {
                pdfi_countdown(o);
                pdfi_countdown(o1);
                return code;
            }
        }
        pdfi_countdown(o);
    }

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "\n");
    /* We don't pdfi_countdown(o1) now, because we've transferred our
     * reference to the pointer in the pdf_context structure.
     */
    pdfi_countdown(ctx->Root); /* If file was repaired it might be set already */
    ctx->Root = (pdf_dict *)o1;
    return 0;
}

static int Info_check_dict(pdf_context *ctx, pdf_dict *d);

static int Info_check_array(pdf_context *ctx, pdf_array *a)
{
    int code = 0, i = 0;
    pdf_obj *array_obj = NULL;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    for (i = 0;i < pdfi_array_size(a); i++) {
        code = pdfi_array_fetch_recursing(ctx, a, i, &array_obj, true, true);
        if (code < 0)
            goto error;

        switch(pdfi_type_of(array_obj)) {
            case PDF_DICT:
                if (array_obj->object_num != 0) {
                    code = pdfi_loop_detector_add_object(ctx, array_obj->object_num);
                    if (code < 0)
                        goto error;
                }
                code = Info_check_dict(ctx, (pdf_dict *)array_obj);
                if (code < 0) {
                    if (array_obj->object_num != 0) {
                        pdf_indirect_ref *i_o;

                        code = pdfi_object_alloc(ctx, PDF_INDIRECT, 0, (pdf_obj **)&i_o);
                        if (code < 0)
                            goto error;
                        i_o->ref_generation_num = array_obj->generation_num;
                        i_o->ref_object_num = array_obj->object_num;
                        i_o->indirect_num = array_obj->object_num;
                        i_o->indirect_gen = array_obj->generation_num;
                        code = pdfi_array_put(ctx, a, i, (pdf_obj *)i_o);
                        if (code < 0)
                            return code;
                    }
                    goto error;
                }
                break;
            case PDF_ARRAY:
                if (array_obj->object_num != 0) {
                    code = pdfi_loop_detector_add_object(ctx, array_obj->object_num);
                    if (code < 0)
                        goto error;
                }
                code = Info_check_array(ctx, (pdf_array *)array_obj);
                if (code < 0) {
                    if (array_obj->object_num != 0) {
                        pdf_indirect_ref *i_o;

                        code = pdfi_object_alloc(ctx, PDF_INDIRECT, 0, (pdf_obj **)&i_o);
                        if (code < 0)
                            goto error;
                        i_o->ref_generation_num = array_obj->generation_num;
                        i_o->ref_object_num = array_obj->object_num;
                        i_o->indirect_num = array_obj->object_num;
                        i_o->indirect_gen = array_obj->generation_num;
                        code = pdfi_array_put(ctx, a, i, (pdf_obj *)i_o);
                        if (code < 0)
                            return code;
                    }
                    goto error;
                }
                break;
            default:
                break;
        }

        pdfi_countdown(array_obj);
        array_obj = NULL;
    }
error:
    pdfi_countdown(array_obj);
    pdfi_loop_detector_cleartomark(ctx);
    return code;
}

static int Info_check_dict(pdf_context *ctx, pdf_dict *d)
{
    int code = 0;
    uint64_t index = 0;
    pdf_name *Key = NULL;
    pdf_obj *Value = NULL;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    code = pdfi_dict_first(ctx, d, (pdf_obj **)&Key, &Value, &index);
    if (code < 0) {
        if (code == gs_error_undefined)
            code = 0;
        goto error;
    }

    while (code >= 0) {
        switch(pdfi_type_of(Value)) {
            case PDF_DICT:
                if (Value->object_num != 0) {
                    code = pdfi_loop_detector_add_object(ctx, Value->object_num);
                    if (code < 0)
                        goto error;
                }
                code = Info_check_dict(ctx, (pdf_dict *)Value);
                if (code < 0)
                    goto error;
                break;
            case PDF_ARRAY:
                if (Value->object_num != 0) {
                    code = pdfi_loop_detector_add_object(ctx, Value->object_num);
                    if (code < 0)
                        goto error;
                }
                code = Info_check_array(ctx, (pdf_array *)Value);
                if (code < 0)
                    goto error;
                break;
            default:
                break;
        }
        pdfi_countdown(Key);
        Key = NULL;
        pdfi_countdown(Value);
        Value = NULL;

        code = pdfi_dict_next(ctx, d, (pdf_obj **)&Key, &Value, &index);
        if (code == gs_error_undefined) {
            code = 0;
            break;
        }
    }
error:
    pdfi_countdown(Key);
    pdfi_countdown(Value);
    pdfi_loop_detector_cleartomark(ctx);
    return code;
}

static int pdfi_sanitize_Info_references(pdf_context *ctx, pdf_dict *Info)
{
    int code = 0;
    uint64_t index = 0;
    pdf_name *Key = NULL;
    pdf_obj *Value = NULL;

restart_scan:
    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    code = pdfi_dict_first(ctx, Info, (pdf_obj **)&Key, &Value, &index);
    if (code == gs_error_undefined) {
        code = 0;
        goto error;
    }

    while (code >= 0) {
        switch(pdfi_type_of(Value)) {
            case PDF_DICT:
                code = Info_check_dict(ctx, (pdf_dict *)Value);
                break;
            case PDF_ARRAY:
                code = Info_check_array(ctx, (pdf_array *)Value);
                break;
            default:
                code = 0;
                break;
        }
        pdfi_countdown(Value);
        Value = NULL;
        if (code < 0) {
            code = pdfi_dict_delete_pair(ctx, Info, Key);
            if (code < 0)
                goto error;
            pdfi_countdown(Key);
            Key = NULL;

            pdfi_loop_detector_cleartomark(ctx);
            goto restart_scan;
        }
        pdfi_countdown(Key);
        Key = NULL;

        pdfi_loop_detector_cleartomark(ctx);
        code = pdfi_loop_detector_mark(ctx);
        if (code < 0) {
            pdfi_countdown(Key);
            pdfi_countdown(Value);
            return code;
        }

        code = pdfi_dict_next(ctx, Info, (pdf_obj **)&Key, &Value, &index);
        if (code == gs_error_undefined) {
            code = 0;
            break;
        }
    }
error:
    pdfi_countdown(Key);
    pdfi_countdown(Value);
    pdfi_loop_detector_cleartomark(ctx);
    return code;
}

int pdfi_read_Info(pdf_context *ctx)
{
    pdf_dict *Info;
    int code;
    pdf_dict *d;

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "%% Reading Info dictionary\n");

    /* See comment in pdfi_read_Root() for details */
    d = ctx->Trailer;
    pdfi_countup(d);
    code = pdfi_dict_get_type(ctx, ctx->Trailer, "Info", PDF_DICT, (pdf_obj **)&Info);
    pdfi_countdown(d);
    if (code < 0)
        return code;

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "\n");

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        goto error;
    code = pdfi_loop_detector_add_object(ctx, Info->object_num);
    if (code < 0)
        goto error1;

    /* sanitize Info for circular references */
    code = pdfi_sanitize_Info_references(ctx, Info);
    if (code < 0)
        goto error1;

    (void)pdfi_loop_detector_cleartomark(ctx);

    pdfi_pdfmark_write_docinfo(ctx, Info);

    /* We don't pdfi_countdown(Info) now, because we've transferred our
     * reference to the pointer in the pdf_context structure.
     */
    ctx->Info = Info;
    return 0;

error1:
    pdfi_loop_detector_cleartomark(ctx);
error:
    pdfi_countdown(Info);
    return code;
}

int pdfi_read_Pages(pdf_context *ctx)
{
    pdf_obj *o, *o1;
    pdf_array *a = NULL;
    int code, pagecount = 0;
    double d;

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "%% Reading Pages dictionary\n");

    code = pdfi_dict_get(ctx, ctx->Root, "Pages", &o1);
    if (code < 0)
        return code;

    if (pdfi_type_of(o1) == PDF_INDIRECT) {
        code = pdfi_dereference(ctx, ((pdf_indirect_ref *)o1)->ref_object_num,  ((pdf_indirect_ref *)o1)->ref_generation_num, &o);
        pdfi_countdown(o1);
        if (code < 0)
            return code;

        if (pdfi_type_of(o) != PDF_DICT) {
            pdfi_countdown(o);
            if (pdfi_type_of(o) == PDF_INDIRECT)
                pdfi_set_error(ctx, 0, NULL, E_PDF_BADPAGEDICT, "pdfi_read_Pages", (char *)"*** Error: Something is wrong with the Pages dictionary.  Giving up.");
            else
                pdfi_set_error(ctx, 0, NULL, E_PDF_BADPAGEDICT, "pdfi_read_Pages", (char *)"*** Error: Something is wrong with the Pages dictionary.  Giving up.\n           Double indirect reference.  Loop in Pages tree?");
            return_error(gs_error_typecheck);
        }

        code = pdfi_dict_put(ctx, ctx->Root, "Pages", o);
        if (code < 0) {
            pdfi_countdown(o);
            return code;
        }
        o1 = o;
    } else {
        if (pdfi_type_of(o1) != PDF_DICT) {
            pdfi_countdown(o1);
            return_error(gs_error_typecheck);
        }
    }

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "\n");

    /* Acrobat allows the Pages Count to be a floating point number (!) */
    /* sample w_a.PDF from Bug688419 (not on the cluster, maybe it should be?) has no /Count entry because
     * The Root dictionary Pages key points directly to a single dictionary of type /Page. This is plainly
     * illegal but Acrobat can deal with it. We do so by ignoring the error her, and adding logic in
     * pdfi_get_page_dict() which notes that ctx->PagesTree is NULL and tries to get the single Page
     * dictionary from the Root instead of using the PagesTree.
     */
    code = pdfi_dict_get_number(ctx, (pdf_dict *)o1, "Count", &d);
    if (code < 0) {
        if (code == gs_error_undefined) {
            pdf_name *n = NULL;
            /* It may be that the Root dictionary Pages entry points directly to a sinlge Page dictionary
             * See if the dictionary has a Type of /Page, if so don't throw an error and the pdf_page.c
             * logic in pdfi_get_page_dict() logic will take care of it.
             */
            code = pdfi_dict_get_type(ctx, (pdf_dict *)o1, "Type", PDF_NAME, (pdf_obj **)&n);
            if (code == 0) {
                if(pdfi_name_is(n, "Page")) {
                    ctx->num_pages = 1;
                    code = 0;
                }
                else
                    code = gs_error_undefined;
                pdfi_countdown(n);
            }
        }
        pdfi_countdown(o1);
        return code;
    }

    if (floor(d) != d) {
        pdfi_countdown(o1);
        return_error(gs_error_rangecheck);
    } else {
        ctx->num_pages = (int)floor(d);
    }

    /* A simple confidence check in the value of Count. We only do this because
     * the OSS-fuzz tool keeps on coming up with files that time out because the
     * initial Count is insanely huge, and we spend much time trying to find
     * millions of pages which don't exist.
     */
    code = pdfi_dict_knownget_type(ctx, (pdf_dict *)o1, "Kids", PDF_ARRAY, (pdf_obj **)&a);
    if (code == 0)
        code = gs_note_error(gs_error_undefined);
    if (code < 0) {
        pdfi_countdown(o1);
        return code;
    }

    /* Firstly check if the Kids array has enough nodes, in which case it's
     * probably flat (the common case)
     */
    if (a->size != ctx->num_pages) {
        int i = 0;
        pdf_obj *p = NULL, *p1 = NULL;
        pdf_num *c = NULL;

        /* Either its not a flat tree, or the top node /Count is incorrect.
         * Get each entry in the Kids array in turn and total the /Count of
         * each node and add any leaf nodes.
         */
        for (i=0;i < a->size; i++) {
            /* We must not allow the entry in the array to be replaced, in case it's a circular reference */
            code = pdfi_array_get_no_store_R(ctx, a, i, &p);
            if (code < 0)
                continue;
            if (pdfi_type_of(p) != PDF_DICT) {
                pdfi_countdown(p);
                p = NULL;
                continue;
            }
            /* Explicit check that the root node Kids array entry is not a self-reference
             * back to the root node. We only check one level of the Kids array. so we don't
             * need a full loop detection setup here.
             */
            if (p->object_num != 0 && p->object_num == o1->object_num) {
                pdfi_countdown(p);
                p = NULL;
                pdfi_countdown(a);
                pdfi_countdown(o1);
                ctx->num_pages = 0;
                return_error(gs_error_circular_reference);
            }
            /* Now we've checked for circular reference we can replace the entry in the
             * array, and add the object to the cache.
             * These are optimisations, so we don't especially care whether they succeed
             */
            (void)pdfi_array_put(ctx, a, i, p);
            (void)replace_cache_entry(ctx, p);
            code = pdfi_dict_knownget_type(ctx, (pdf_dict *)p, "Type", PDF_NAME, (pdf_obj **)&p1);
            if (code <= 0) {
                pdfi_countdown(p);
                p = NULL;
                continue;
            }
            if (pdfi_name_is((pdf_name *)p1, "Page")) {
                pagecount++;
            } else {
                if (pdfi_name_is((pdf_name *)p1, "Pages")) {
                    code = pdfi_dict_knownget(ctx, (pdf_dict *)p, "Count", (pdf_obj **)&c);
                    if (code >= 0) {
                        if (pdfi_type_of(c) == PDF_INT)
                            pagecount += c->value.i;
                        if (pdfi_type_of(c) == PDF_REAL)
                            pagecount += (int)c->value.d;
                        pdfi_countdown(c);
                        c = NULL;
                    }
                }
            }
            pdfi_countdown(p1);
            p1 = NULL;
            pdfi_countdown(p);
            p = NULL;
        }
    } else
        pagecount = a->size;

    pdfi_countdown(a);

    /* If the count of the top level of the tree doesn't match the /Count
     * of the root node then something is wrong. We could abort right now
     * and will if this continues to be a problem, but initially let's assume
     * the count of the top level is correct and the root node /Count is wrong.
     * This will allow us to recover if only the root /Count gets corrupted.
     * In future we could also try validating the entire tree at this point,
     * though I suspect that's pointless; if the tree is corrupted we aren't
     * likely to get much that's usable from it.
     */
    if (pagecount != ctx->num_pages) {
        ctx->num_pages = pagecount;
        if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_BADPAGECOUNT, "pdfi_read_Pages", NULL)) < 0)
            return code;
        code = pdfi_dict_put_int(ctx, (pdf_dict *)o1, "Count", ctx->num_pages);
        if (code < 0) {
            pdfi_countdown(o1);
            return code;
        }
    }

    /* We don't pdfi_countdown(o1) now, because we've transferred our
     * reference to the pointer in the pdf_context structure.
     */
    ctx->PagesTree = (pdf_dict *)o1;
    return 0;
}

/* Read optional things in from Root */
void pdfi_read_OptionalRoot(pdf_context *ctx)
{
    pdf_obj *obj = NULL;
    int code;
    bool known;

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "%% Reading other Root contents\n");

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "%% OCProperties\n");
    code = pdfi_dict_get_type(ctx, ctx->Root, "OCProperties", PDF_DICT, &obj);
    if (code == 0) {
        ctx->OCProperties = (pdf_dict *)obj;
    } else {
        ctx->OCProperties = NULL;
        if (ctx->args.pdfdebug)
            dmprintf(ctx->memory, "%% (None)\n");
    }

    (void)pdfi_dict_known(ctx, ctx->Root, "Collection", &known);

    if (known) {
        if (ctx->args.pdfdebug)
            dmprintf(ctx->memory, "%% Collection\n");
        code = pdfi_dict_get(ctx, ctx->Root, "Collection", (pdf_obj **)&ctx->Collection);
        if (code < 0)
            dmprintf(ctx->memory, "\n   **** Warning: Failed to read Collection information.\n");
    }

}

void pdfi_free_OptionalRoot(pdf_context *ctx)
{
    if (ctx->OCProperties) {
        pdfi_countdown(ctx->OCProperties);
        ctx->OCProperties = NULL;
    }
    if (ctx->Collection) {
        pdfi_countdown(ctx->Collection);
        ctx->Collection = NULL;
    }
}

/* Handle child node processing for page_dict */
static int pdfi_get_child(pdf_context *ctx, pdf_array *Kids, int i, pdf_dict **pchild)
{
    pdf_indirect_ref *node = NULL;
    pdf_dict *child = NULL;
    pdf_name *Type = NULL;
    pdf_dict *leaf_dict = NULL;
    pdf_name *Key = NULL;
    int code = 0;

    code = pdfi_array_get_no_deref(ctx, Kids, i, (pdf_obj **)&node);
    if (code < 0)
        goto errorExit;

    if (pdfi_type_of(node) != PDF_INDIRECT && pdfi_type_of(node) != PDF_DICT) {
        code = gs_note_error(gs_error_typecheck);
        goto errorExit;
    }

    if (pdfi_type_of(node) == PDF_INDIRECT) {
        code = pdfi_dereference(ctx, node->ref_object_num, node->ref_generation_num,
                                (pdf_obj **)&child);
        if (code < 0) {
            int code1 = pdfi_repair_file(ctx);
            if (code1 < 0)
                goto errorExit;
            code = pdfi_dereference(ctx, node->ref_object_num,
                                    node->ref_generation_num, (pdf_obj **)&child);
            if (code < 0)
                goto errorExit;
        }
        /* It makes no sense for the Page dictionary to be a stream, but safedocs/DialectDictIsStream.pdf
         * has this (stream length is 0, not that it matters) and Acrobat happily opens it....
         */
        if (pdfi_type_of(child) != PDF_DICT) {
            pdf_dict *d1 = NULL;

            if (pdfi_type_of(child) != PDF_STREAM) {
                code = gs_note_error(gs_error_typecheck);
                goto errorExit;
            }
            if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_typecheck), NULL, E_PDF_DICT_IS_STREAM, "pdfi_get_child", NULL)) < 0)
                goto errorExit;

            code = pdfi_get_stream_dict(ctx, (pdf_stream *)child, &d1);
            if (code < 0)
                goto errorExit;
            pdfi_countdown(child);
            child = d1;
            code = replace_cache_entry(ctx, (pdf_obj *)d1);
            if (code < 0)
                goto errorExit;
        }
        /* If its an intermediate node, store it in the page_table, if its a leaf node
         * then don't store it. Instead we create a special dictionary of our own which
         * has a /Type of /PageRef and a /PageRef key which is the indirect reference
         * to the page. However in this case we pass on the actual page dictionary to
         * the Kids processing below. If we didn't then we'd fall foul of the loop
         * detection by dereferencing the same object twice.
         * This is tedious, but it means we don't store all the page dictionaries in
         * the Pages tree, because page dictionaries can be large and we generally
         * only use them once. If processed in order we only dereference each page
         * dictionary once, any other order will dereference each page twice. (or more
         * if we render the same page multiple times).
         */
        code = pdfi_dict_get_type(ctx, child, "Type", PDF_NAME, (pdf_obj **)&Type);
        if (code < 0)
            goto errorExit;
        if (pdfi_name_is(Type, "Pages")) {
            code = pdfi_array_put(ctx, Kids, i, (pdf_obj *)child);
            if (code < 0)
                goto errorExit;
        } else {
            /* Bizarrely, one of the QL FTS files (FTS_07_0704.pdf) has a page diciotnary with a /Type of /Template */
            if (!pdfi_name_is(Type, "Page"))
                if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_typecheck), NULL, E_PDF_BADPAGETYPE, "pdfi_get_child", NULL)) < 0)
                    goto errorExit;
            /* Make a 'PageRef' entry (just stores an indirect reference to the actual page)
             * and store that in the Kids array for future reference. But pass on the
             * dereferenced Page dictionary, in case this is the target page.
             */

            code = pdfi_dict_alloc(ctx, 0, &leaf_dict);
            if (code < 0)
                goto errorExit;
            code = pdfi_name_alloc(ctx, (byte *)"PageRef", 7, (pdf_obj **)&Key);
            if (code < 0)
                goto errorExit;
            pdfi_countup(Key);

            code = pdfi_dict_put_obj(ctx, leaf_dict, (pdf_obj *)Key, (pdf_obj *)node, true);
            if (code < 0)
                goto errorExit;
            code = pdfi_dict_put(ctx, leaf_dict, "Type", (pdf_obj *)Key);
            if (code < 0)
                goto errorExit;
            code = pdfi_array_put(ctx, Kids, i, (pdf_obj *)leaf_dict);
            if (code < 0)
                goto errorExit;
            leaf_dict = NULL;
        }
    } else {
        if (ctx->loop_detection != NULL) {
            if (node->object_num != 0 && pdfi_loop_detector_check_object(ctx, node->object_num)) {
                code = gs_note_error(gs_error_circular_reference);
                goto errorExit;
            }
            if (node->object_num > 0) {
                code = pdfi_loop_detector_add_object(ctx, node->object_num);
                if (code < 0)
                    goto errorExit;
            }
        }
        child = (pdf_dict *)node;
        pdfi_countup(child);
    }

    *pchild = child;
    child = NULL;

 errorExit:
    pdfi_free_object((pdf_obj *)leaf_dict);
    pdfi_countdown(child);
    pdfi_countdown(node);
    pdfi_countdown(Type);
    pdfi_countdown(Key);
    return code;
}

/* Check if key is in the dictionary, and if so, copy it into the inheritable dict.
 */
static int pdfi_check_inherited_key(pdf_context *ctx, pdf_dict *d, const char *keyname, pdf_dict *inheritable)
{
    int code = 0;
    pdf_obj *object = NULL;
    bool known;

    /* Check for inheritable keys, if we find any copy them to the 'inheritable' dictionary at this level */
    code = pdfi_dict_known(ctx, d, keyname, &known);
    if (code < 0)
        goto exit;
    if (known) {
        code = pdfi_loop_detector_mark(ctx);
        if (code < 0){
            goto exit;
        }
        code = pdfi_dict_get(ctx, d, keyname, &object);
        if (code < 0) {
            (void)pdfi_loop_detector_cleartomark(ctx);
            goto exit;
        }
        code = pdfi_loop_detector_cleartomark(ctx);
        if (code < 0) {
            goto exit;
        }
        code = pdfi_dict_put(ctx, inheritable, keyname, object);
    }

 exit:
    pdfi_countdown(object);
    return code;
}

int pdfi_get_page_dict(pdf_context *ctx, pdf_dict *d, uint64_t page_num, uint64_t *page_offset,
                   pdf_dict **target, pdf_dict *inherited)
{
    int i, code = 0;
    pdf_array *Kids = NULL;
    pdf_dict *child = NULL;
    pdf_name *Type = NULL;
    pdf_dict *inheritable = NULL;
    int64_t num;
    double dbl;

    if (ctx->args.pdfdebug)
        dmprintf1(ctx->memory, "%% Finding page dictionary for page %"PRIi64"\n", page_num + 1);

    /* Allocated inheritable dict (it might stay empty) */
    code = pdfi_dict_alloc(ctx, 0, &inheritable);
    if (code < 0)
        return code;
    pdfi_countup(inheritable);

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    /* if we are being passed any inherited values from our parent, copy them now */
    if (inherited != NULL) {
        code = pdfi_dict_copy(ctx, inheritable, inherited);
        if (code < 0)
            goto exit;
    }

    code = pdfi_dict_get_number(ctx, d, "Count", &dbl);
    if (code < 0)
        goto exit;
    if (dbl != floor(dbl)) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }
    num = (int)dbl;

    if (num < 0 || (num + *page_offset) > ctx->num_pages) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }
    if (num + *page_offset < page_num) {
        *page_offset += num;
        code = 1;
        goto exit;
    }
    /* The requested page is a descendant of this node */

    /* Check for inheritable keys, if we find any copy them to the 'inheritable' dictionary at this level */
    code = pdfi_check_inherited_key(ctx, d, "Resources", inheritable);
    if (code < 0)
        goto exit;
    code = pdfi_check_inherited_key(ctx, d, "MediaBox", inheritable);
    if (code < 0)
        goto exit;
    code = pdfi_check_inherited_key(ctx, d, "CropBox", inheritable);
    if (code < 0)
        goto exit;
    code = pdfi_check_inherited_key(ctx, d, "Rotate", inheritable);
    if (code < 0) {
        goto exit;
    }

    /* Get the Kids array */
    code = pdfi_dict_get_type(ctx, d, "Kids", PDF_ARRAY, (pdf_obj **)&Kids);
    if (code < 0) {
        goto exit;
    }

    /* Check each entry in the Kids array */
    for (i = 0;i < pdfi_array_size(Kids);i++) {
        pdfi_countdown(child);
        child = NULL;
        pdfi_countdown(Type);
        Type = NULL;

        code = pdfi_get_child(ctx, Kids, i, &child);
        if (code < 0) {
            goto exit;
        }

        /* Check the type, if its a Pages entry, then recurse. If its a Page entry, is it the one we want */
        code = pdfi_dict_get_type(ctx, child, "Type", PDF_NAME, (pdf_obj **)&Type);
        if (code == 0) {
            if (pdfi_name_is(Type, "Pages")) {
                code = pdfi_dict_get_number(ctx, child, "Count", &dbl);
                if (code == 0) {
                    if (dbl != floor(dbl)) {
                        code = gs_note_error(gs_error_rangecheck);
                        goto exit;
                    }
                    num = (int)dbl;
                    if (num < 0 || (num + *page_offset) > ctx->num_pages) {
                        code = gs_note_error(gs_error_rangecheck);
                        goto exit;
                    } else {
                        if (num + *page_offset <= page_num) {
                            *page_offset += num;
                        } else {
                            code = pdfi_get_page_dict(ctx, child, page_num, page_offset, target, inheritable);
                            goto exit;
                        }
                    }
                }
            } else {
                if (pdfi_name_is(Type, "PageRef")) {
                    if ((*page_offset) == page_num) {
                        pdf_dict *page_dict = NULL;

                        code = pdfi_dict_get_no_store_R(ctx, child, "PageRef", (pdf_obj **)&page_dict);
                        if (code < 0)
                            goto exit;
                        code = pdfi_merge_dicts(ctx, page_dict, inheritable);
                        *target = page_dict;
                        pdfi_countup(*target);
                        pdfi_countdown(page_dict);
                        goto exit;
                    } else {
                        *page_offset += 1;
                    }
                } else {
                    if (!pdfi_name_is(Type, "Page")) {
                        if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_typecheck), NULL, E_PDF_BADPAGETYPE, "pdfi_get_page_dict", NULL)) < 0)
                            goto exit;
                    }
                    if ((*page_offset) == page_num) {
                        code = pdfi_merge_dicts(ctx, child, inheritable);
                        *target = child;
                        pdfi_countup(*target);
                        goto exit;
                    } else {
                        *page_offset += 1;
                    }
                }
            }
        }
        if (code < 0)
            goto exit;
    }
    /* Positive return value indicates we did not find the target below this node, try the next one */
    code = 1;

 exit:
    pdfi_loop_detector_cleartomark(ctx);
    pdfi_countdown(inheritable);
    pdfi_countdown(Kids);
    pdfi_countdown(child);
    pdfi_countdown(Type);
    return code;
}

int pdfi_doc_page_array_init(pdf_context *ctx)
{
    size_t size = ctx->num_pages*sizeof(uint32_t);

    ctx->page_array = (uint32_t *)gs_alloc_bytes(ctx->memory, size,
                                                 "pdfi_doc_page_array_init(page_array)");
    if (ctx->page_array == NULL)
        return_error(gs_error_VMerror);

    memset(ctx->page_array, 0, size);
    return 0;
}

void pdfi_doc_page_array_free(pdf_context *ctx)
{
    if (!ctx->page_array)
        return;
    gs_free_object(ctx->memory, ctx->page_array, "pdfi_doc_page_array_free(page_array)");
    ctx->page_array = NULL;
}

/*
 * Checks for both "Resource" and "RD" in the specified dict.
 * And then gets the typedict of Type (e.g. Font or XObject).
 * Returns 0 if undefined, >0 if found, <0 if error
 */
static int pdfi_resource_knownget_typedict(pdf_context *ctx, unsigned char *Type,
                                           pdf_dict *dict, pdf_dict **typedict)
{
    int code;
    pdf_dict *Resources = NULL;

    code = pdfi_dict_knownget_type(ctx, dict, "Resources", PDF_DICT, (pdf_obj **)&Resources);
    if (code == 0)
        code = pdfi_dict_knownget_type(ctx, dict, "DR", PDF_DICT, (pdf_obj **)&Resources);
    if (code < 0)
        goto exit;
    if (code > 0)
        code = pdfi_dict_knownget_type(ctx, Resources, (const char *)Type, PDF_DICT, (pdf_obj **)typedict);
 exit:
    pdfi_countdown(Resources);
    return code;
}

int pdfi_find_resource(pdf_context *ctx, unsigned char *Type, pdf_name *name,
                       pdf_dict *dict, pdf_dict *page_dict, pdf_obj **o)
{
    pdf_dict *typedict = NULL;
    pdf_dict *Parent = NULL;
    pdf_name *n = NULL;
    int code;
    bool known = false;

    *o = NULL;

    /* Check the provided dict, stream_dict can be NULL if we are trying to find a Default* ColorSpace */
    if (dict != NULL) {
        bool deref_parent = true, dict_is_XObject = false;

        code = pdfi_resource_knownget_typedict(ctx, Type, dict, &typedict);
        if (code < 0)
            goto exit;
        if (code > 0) {
            code = pdfi_dict_get_no_store_R_key(ctx, typedict, name, o);
            if (code != gs_error_undefined)
                goto exit;
        }

        /* Check the Parents, if any */
        /* If the current dictionary is a Page dictionary, do NOT dereference it's Parent, as that
         * will be the Pages tree, and we will end up with circular references, causing a memory leak.
         */
        if (pdfi_dict_knownget_type(ctx, dict, "Type", PDF_NAME, (pdf_obj **)&n) > 0) {
            if (pdfi_name_is(n, "Page"))
                deref_parent = false;
            if (pdfi_name_is(n, "XObject"))
                dict_is_XObject = true;
            pdfi_countdown(n);
        }

        if (deref_parent) {
            code = pdfi_dict_known(ctx, dict, "Parent", &known);
            if (code >= 0 && known == true) {
                code = pdfi_dict_get_no_store_R(ctx, dict, "Parent", (pdf_obj **)&Parent);

                if (code >= 0) {
                    if (pdfi_type_of(Parent) != PDF_DICT) {
                        if (pdfi_type_of(Parent) == PDF_INDIRECT) {
                            pdf_indirect_ref *o = (pdf_indirect_ref *)Parent;

                            Parent = NULL;
                            code = pdfi_dereference(ctx, o->ref_object_num, o->ref_generation_num, (pdf_obj **)&Parent);
                            pdfi_countdown(o);
                            if (code >= 0 && pdfi_type_of(Parent) != PDF_DICT) {
                                pdfi_countdown(Parent);
                                Parent = NULL;
                            }
                        } else {
                            pdfi_countdown(Parent);
                            Parent = NULL;
                        }
                    }
                } else
                    Parent = NULL;
            }

            if (Parent != NULL) {
                if (ctx->page.CurrentPageDict != NULL && Parent->object_num != ctx->page.CurrentPageDict->object_num) {
                    if (pdfi_loop_detector_check_object(ctx, Parent->object_num) == true) {
                        code = gs_note_error(gs_error_circular_reference);
                        goto exit;
                    }

                    code = pdfi_loop_detector_mark(ctx);
                    if (code < 0)
                        goto exit;

                    code = pdfi_loop_detector_add_object(ctx, dict->object_num);
                    if (code < 0) {
                        (void)pdfi_loop_detector_cleartomark(ctx);
                        goto exit;
                    }
                    code = pdfi_find_resource(ctx, Type, name, Parent, page_dict, o);
                    (void)pdfi_loop_detector_cleartomark(ctx);
                    if (code != gs_error_undefined) {
                        if (dict_is_XObject)
                            pdfi_set_warning(ctx, 0, NULL, W_PDF_INHERITED_STREAM_RESOURCE, "pdfi_find_resource", (char *)"Couldn't find named resource in supplied dictionary, matching name located in Page Resource");
                        goto exit;
                    }
                }
            }
            code = 0;
        }
        pdfi_countdown(typedict);
        typedict = NULL;
    }

    /* Normally page_dict can't be (or shouldn't be) NULL. However, if we are processing
     * a TYpe 3 font, then the 'page dict' is the Resources dictionary of that font. If
     * the font inherits Resources from its page (which it should not) then its possible
     * that the 'page dict' could be NULL here. We need to guard against that. Its possible
     * there may be other, similar, cases (eg Patterns within Patterns). In addition we
     * do need to be able to check the real page dictionary for inhereited resources, and
     * in the case of a type 3 font BuildChar at least there is no easy way to do that.
     * So we'll store the page dictionary for the current page in the context as a
     * last-ditch resource to check.
     */
    if (page_dict != NULL) {
        code = pdfi_resource_knownget_typedict(ctx, Type, page_dict, &typedict);
        if (code < 0)
            goto exit;

        if (code > 0) {
            code = pdfi_dict_get_no_store_R_key(ctx, typedict, name, o);
            if (code != gs_error_undefined)
                goto exit;
        }
    }

    pdfi_countdown(typedict);
    typedict = NULL;

    if (ctx->page.CurrentPageDict != NULL) {
        code = pdfi_resource_knownget_typedict(ctx, Type, ctx->page.CurrentPageDict, &typedict);
        if (code < 0)
            goto exit;

        if (code > 0) {
            code = pdfi_dict_get_no_store_R_key(ctx, typedict, name, o);
            if (code != gs_error_undefined)
                goto exit;
        }
    }

    pdfi_countdown(typedict);
    typedict = NULL;

    if (ctx->current_stream != NULL) {
        pdf_dict *stream_dict = NULL;
        pdf_stream *stream = ctx->current_stream;

        do {
            code = pdfi_dict_from_obj(ctx, (pdf_obj *)stream, &stream_dict);
            if (code < 0)
                goto exit;
            code = pdfi_resource_knownget_typedict(ctx, Type, stream_dict, &typedict);
            if (code < 0)
                goto exit;
            if (code > 0) {
                code = pdfi_dict_get_no_store_R_key(ctx, typedict, name, o);
                if (code == 0) {
                    pdfi_set_error(ctx, 0, NULL, E_PDF_INHERITED_STREAM_RESOURCE, "pdfi_find_resource", (char *)"Couldn't find named resource in supplied dictionary, or Parents, or Pages, matching name located in earlier stream Resource");
                    goto exit;
                }
            }
            pdfi_countdown(typedict);
            typedict = NULL;
            stream = pdfi_stream_parent(ctx, stream);
        }while(stream != NULL);
    }

    /* If we got all the way down there, we didn't find it */
    pdfi_set_warning(ctx, 0, NULL, W_PDF_MISSING_NAMED_RESOURCE, "pdfi_find_resource", NULL);
    code = gs_error_undefined;

exit:
    pdfi_countdown(typedict);
    pdfi_countdown(Parent);
    return code;
}

/* Mark the actual outline */
static int pdfi_doc_mark_the_outline(pdf_context *ctx, pdf_dict *outline)
{
    int code = 0;
    pdf_dict *tempdict = NULL;
    uint64_t dictsize;
    uint64_t index;
    pdf_name *Key = NULL;
    double num = 0;

    /* Basically we only do /Count, /Title, /A, /C, /F
     * The /First, /Last, /Next, /Parent get written magically by pdfwrite
     */

    /* Make a temporary copy of the outline dict */
    dictsize = pdfi_dict_entries(outline);
    code = pdfi_dict_alloc(ctx, dictsize, &tempdict);
    if (code < 0) goto exit;
    pdfi_countup(tempdict);
    code = pdfi_dict_copy(ctx, tempdict, outline);
    if (code < 0) goto exit;

    /* Due to some craziness on the part of Adobe, the /Count in an Outline entry
     * in a PDF file, and the /Count value in an /OUT pdfmark mean different things(!)
     * In a PDF file it is the number of outline entries beneath the current entry
     * (all child descsndants) whereas in a pdfmark it is the number of entries
     * in just the next level. So we cannot use the /Count from the PDF file, we
     * need to go to the /First entry of the next level, and then count all
     * the entries at that level by following each /Next.
     */
    code = pdfi_dict_knownget_number(ctx, outline, "Count", &num);
    if (code < 0)
        goto exit;

    /* We can't rely on Count being present to indicate that the Outline has descendants
     * see bug #707228. Instead, look for the presence of a /First key. If there is no count
     * key, or it is 0, then assume the entry is closed.
     */
    {
        pdf_dict *current = NULL, *next = NULL;
        int count = 0, code1;

        code1 = pdfi_dict_knownget_type(ctx, outline, "First", PDF_DICT, (pdf_obj **)&current);
        if (code1 > 0) {
            if (code <= 0) {
                if ((code = pdfi_set_warning_stop(ctx, code, NULL, W_PDF_OUTLINECHILD_NO_COUNT, "pdfi_doc_mark_the_outline", NULL)) < 0)
                    goto exit;
            }
            count++;
            do {
                code1 = pdfi_dict_knownget_type(ctx, current, "Next", PDF_DICT, (pdf_obj **)&next);
                if (code1 > 0) {
                    pdfi_countdown(current);
                    current = next;
                    next = NULL;
                    count++;
                } else
                    break;
            } while (1);
            pdfi_countdown(current);
        }
        if (num <= 0)
            count *= -1;
        pdfi_dict_put_int(ctx, tempdict, "Count", count);
    }

    /* Go through the dict, removing some keys and doing special handling for others.
     */
    code = pdfi_dict_key_first(ctx, outline, (pdf_obj **)&Key, &index);
    while (code >= 0) {
        if (pdfi_name_is(Key, "Last") || pdfi_name_is(Key, "Next") || pdfi_name_is(Key, "First") ||
            pdfi_name_is(Key, "Prev") || pdfi_name_is(Key, "Parent")) {
            /* Delete some keys
             * These are handled in pdfwrite and can lead to circular refs
             */
            code = pdfi_dict_delete_pair(ctx, tempdict, Key);
        } else if (pdfi_name_is(Key, "SE")) {
            /* TODO: Not sure what to do with SE, delete for now */
            /* Seems to be okay to just delete it, since there should also be a /Dest
             * See sample fts_29_2903.pdf
             * Currently we are same as gs
             */
            code = pdfi_dict_delete_pair(ctx, tempdict, Key);
        } else if (pdfi_name_is(Key, "A")) {
            code = pdfi_pdfmark_modA(ctx, tempdict);
        } else if (pdfi_name_is(Key, "Dest")) {
            code = pdfi_pdfmark_modDest(ctx, tempdict);
        }
        if (code < 0)
            goto exit;

        pdfi_countdown(Key);
        Key = NULL;

        code = pdfi_dict_key_next(ctx, outline, (pdf_obj **)&Key, &index);
        if (code == gs_error_undefined) {
            code = 0;
            break;
        }
    }
    if (code < 0) goto exit;

    /* Write the pdfmark */
    code = pdfi_pdfmark_from_dict(ctx, tempdict, NULL, "OUT");
    if (code < 0)
        goto exit;

 exit:
    pdfi_countdown(tempdict);
    pdfi_countdown(Key);
    return code;
}

/* Do pdfmark on an outline entry (recursive)
 * Note: the logic here is wonky.  It is relying on the behavior of the pdfwrite driver.
 * See pdf_main.ps/writeoutline()
 */
static int pdfi_doc_mark_outline(pdf_context *ctx, pdf_dict *outline)
{
    int code = 0;
    pdf_dict *child = NULL;
    pdf_dict *Next = NULL;

    if (outline == (pdf_dict *)PDF_NULL_OBJ)
        return 0;

    /* Mark the outline */
    /* NOTE: I think the pdfmark for this needs to be written before the children
     * because I think pdfwrite relies on the order of things.
     */
    code = pdfi_doc_mark_the_outline(ctx, outline);
    if (code < 0)
        goto exit1;

    /* Handle the children */
    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        goto exit1;

    /* Handle any children (don't deref them, we don't want to leave them hanging around) */
    code = pdfi_dict_get_no_store_R(ctx, outline, "First", (pdf_obj **)&child);
    if (code < 0 || pdfi_type_of(child) != PDF_DICT) {
        /* TODO: flag a warning? */
        code = 0;
        goto exit;
    }

    if (child->object_num != 0) {
        code = pdfi_loop_detector_add_object(ctx, child->object_num);
        if (code < 0)
            goto exit;
    }

    do {
        code = pdfi_doc_mark_outline(ctx, child);
        if (code < 0) goto exit;


        code = pdfi_dict_get_no_store_R(ctx, child, "Next", (pdf_obj **)&Next);
        if (code == gs_error_undefined) {
            code = 0;
            break;
        }
        if (code == gs_error_circular_reference) {
            code = 0;
            goto exit;
        }
        if (code < 0 || pdfi_type_of(Next) != PDF_DICT)
            goto exit;

        pdfi_countdown(child);
        child = Next;
        Next = NULL;
    } while (true);

 exit:
    (void)pdfi_loop_detector_cleartomark(ctx);
 exit1:
    pdfi_countdown(child);
    pdfi_countdown(Next);
    return code;
}

/* Do pdfmark for Outlines */
static int pdfi_doc_Outlines(pdf_context *ctx)
{
    int code = 0;
    pdf_dict *Outlines = NULL;
    pdf_dict *outline = NULL;
    pdf_dict *Next = NULL;

    if (ctx->args.no_pdfmark_outlines)
        goto exit1;

    code = pdfi_dict_knownget_type(ctx, ctx->Root, "Outlines", PDF_DICT, (pdf_obj **)&Outlines);
    if (code <= 0) {
        /* TODO: flag a warning */
        code = 0;
        goto exit1;
    }

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        goto exit1;

    /* Handle any children (don't deref them, we don't want to leave them hanging around) */
    code = pdfi_dict_get_no_store_R(ctx, Outlines, "First", (pdf_obj **)&outline);
    if (code < 0 || pdfi_type_of(outline) != PDF_DICT) {
        /* TODO: flag a warning? */
        code = 0;
        goto exit;
    }

    if (pdfi_type_of(outline) != PDF_DICT)
        goto exit; /* Exit with no error? */

    if (outline->object_num != 0) {
        code = pdfi_loop_detector_add_object(ctx, outline->object_num);
        if (code < 0)
            goto exit;
    }

    /* Loop through all the top-level outline entries
     * First one is in Outlines, and if there are more, they are the Next of the
     * current outline item.  (see spec)
     * (basically we are walking a linked list)
     */
    do {
        code = pdfi_doc_mark_outline(ctx, outline);
        if (code < 0) goto exit;


        code = pdfi_dict_get_no_store_R(ctx, outline, "Next", (pdf_obj **)&Next);
        if (code == gs_error_undefined) {
            code = 0;
            break;
        }
        if (code == gs_error_circular_reference) {
            code = 0;
            goto exit;
        }
        if (code < 0 || pdfi_type_of(Next) != PDF_DICT)
            goto exit;

        pdfi_countdown(outline);
        outline = Next;
        Next = NULL;
    } while (true);

 exit:
    (void)pdfi_loop_detector_cleartomark(ctx);
 exit1:
    pdfi_countdown(Outlines);
    pdfi_countdown(outline);
    pdfi_countdown(Next);
    return code;
}

/* Do pdfmark for Info */
static int pdfi_doc_Info(pdf_context *ctx)
{
    int code = 0;
    pdf_dict *Info = NULL, *d = NULL;
    pdf_dict *tempdict = NULL;
    uint64_t dictsize;
    uint64_t index;
    pdf_name *Key = NULL;
    pdf_obj *Value = NULL;

    /* See comment in pdfi_read_Root() for details */
    d = ctx->Trailer;
    pdfi_countup(d);
    code = pdfi_dict_knownget_type(ctx, d, "Info", PDF_DICT, (pdf_obj **)&Info);
    pdfi_countdown(d);
    if (code <= 0) {
        /* TODO: flag a warning */
        goto exit;
    }

    /* Make a temporary copy of the Info dict */
    dictsize = pdfi_dict_entries(Info);
    code = pdfi_dict_alloc(ctx, dictsize, &tempdict);
    if (code < 0) goto exit;
    pdfi_countup(tempdict);

    /* Copy only certain keys from Info to tempdict
     * NOTE: pdfwrite will set /Producer, /CreationDate and /ModDate
     */
    code = pdfi_dict_first(ctx, Info, (pdf_obj **)&Key, &Value, &index);
    while (code >= 0) {
        if (pdfi_name_is(Key, "Author") || pdfi_name_is(Key, "Creator") ||
            pdfi_name_is(Key, "Title") || pdfi_name_is(Key, "Subject") ||
            pdfi_name_is(Key, "Keywords")) {

            code = pdfi_dict_put_obj(ctx, tempdict, (pdf_obj *)Key, Value, true);
            if (code < 0)
                goto exit;
        }
        pdfi_countdown(Key);
        Key = NULL;
        pdfi_countdown(Value);
        Value = NULL;

        code = pdfi_dict_next(ctx, Info, (pdf_obj **)&Key, &Value, &index);
        if (code == gs_error_undefined) {
            code = 0;
            break;
        }
    }
    if (code < 0) goto exit;

    /* Write the pdfmark */
    code = pdfi_pdfmark_from_dict(ctx, tempdict, NULL, "DOCINFO");

 exit:
    pdfi_countdown(Key);
    pdfi_countdown(Value);
    pdfi_countdown(Info);
    pdfi_countdown(tempdict);
    return code;
}

/* Handle PageLabels for pdfwrite device */
static int pdfi_doc_PageLabels(pdf_context *ctx)
{
    int code;
    pdf_dict *PageLabels = NULL;

    if (ctx->loop_detection) {
        code = pdfi_loop_detector_mark(ctx);
        if (code < 0)
            return code;
    }

    code = pdfi_dict_knownget_type(ctx, ctx->Root, "PageLabels", PDF_DICT, (pdf_obj **)&PageLabels);
    if (code <= 0) {
        if (ctx->loop_detection)
            (void)pdfi_loop_detector_cleartomark(ctx);
        /* TODO: flag a warning */
        goto exit;
    }

    if (ctx->loop_detection) {
        code = pdfi_loop_detector_cleartomark(ctx);
        if (code < 0)
            goto exit;
    }

    if (ctx->device_state.WantsPageLabels) {
        /* This will send the PageLabels object as a 'pdfpagelabels' setdeviceparams */
        code = pdfi_pdfmark_object(ctx, (pdf_obj *)PageLabels, "pdfpagelabels");
        if (code < 0)
            goto exit;
    }

 exit:
    pdfi_countdown(PageLabels);
    return code;
}

/* Handle OutputIntents stuff
 * (bottom of pdf_main.ps/process_trailer_attrs)
 */
static int pdfi_doc_OutputIntents(pdf_context *ctx)
{
    int code;
    pdf_array *OutputIntents = NULL;
    pdf_dict *intent = NULL;
    pdf_string *name = NULL;
    pdf_stream *DestOutputProfile = NULL;
    uint64_t index;

    /* NOTE: subtle difference in error handling -- we are checking for OutputIntents first,
     * so this will just ignore UsePDFX3Profile or UseOutputIntent params without warning,
     * if OutputIntents doesn't exist.  Seems fine to me.
     */
    code = pdfi_dict_knownget_type(ctx, ctx->Root, "OutputIntents", PDF_ARRAY,
                                   (pdf_obj **)&OutputIntents);
    if (code <= 0) {
        goto exit;
    }

    /* TODO: Implement writeoutputintents  if somebody ever complains...
     * See pdf_main.ps/writeoutputintents
     * I am not aware of a device that supports "/OutputIntent" so
     * couldn't figure out what to do for this.
     */

    /* Handle UsePDFX3Profile and UseOutputIntent command line options */
    if (ctx->args.UsePDFX3Profile) {
        /* This is an index into the array */
        code = pdfi_array_get_type(ctx, OutputIntents, ctx->args.PDFX3Profile_num,
                                   PDF_DICT, (pdf_obj **)&intent);
        if (code < 0) {
            dmprintf1(ctx->memory,
                      "*** WARNING UsePDFX3Profile specified invalid index %d for OutputIntents\n",
                      ctx->args.PDFX3Profile_num);
            goto exit;
        }
    } else if (ctx->args.UseOutputIntent != NULL) {
        /* This is a name to look up in the array */
        for (index=0; index<pdfi_array_size(OutputIntents); index ++) {
            code = pdfi_array_get_type(ctx, OutputIntents, index, PDF_DICT, (pdf_obj **)&intent);
            if (code < 0) goto exit;

            code = pdfi_dict_knownget_type(ctx, intent, "OutputConditionIdentifier", PDF_STRING,
                                           (pdf_obj **)&name);
            if (code < 0) goto exit;
            if (code == 0)
                continue;

            /* If the ID is "Custom" then check "Info" instead */
            if (pdfi_string_is(name, "Custom")) {
                pdfi_countdown(name);
                name = NULL;
                code = pdfi_dict_knownget_type(ctx, intent, "Info", PDF_STRING, (pdf_obj **)&name);
                if (code < 0) goto exit;
                if (code == 0)
                    continue;
            }

            /* Check for a match */
            if (pdfi_string_is(name, ctx->args.UseOutputIntent))
                break;

            pdfi_countdown(intent);
            intent = NULL;
            pdfi_countdown(name);
            name = NULL;
        }
        code = 0;
    } else {
        /* No command line arg was specified, so nothing to do */
        code = 0;
        goto exit;
    }

    /* Now if intent is non-null, we found the selected intent dictionary */
    if (intent == NULL)
        goto exit;

    /* Load the profile, if it exists */
    code = pdfi_dict_knownget_type(ctx, intent, "DestOutputProfile", PDF_STREAM, (pdf_obj **)&DestOutputProfile);
    /* TODO: Flag an error if it doesn't exist?  Only required in some cases */
    if (code <= 0) goto exit;

    /* Set the intent to the profile */
    code = pdfi_color_setoutputintent(ctx, intent, DestOutputProfile);

 exit:
    pdfi_countdown(OutputIntents);
    pdfi_countdown(intent);
    pdfi_countdown(name);
    pdfi_countdown(DestOutputProfile);
    return code;
}

/* Handled an embedded files Names array for pdfwrite device */
static int pdfi_doc_EmbeddedFiles_Names(pdf_context *ctx, pdf_array *names)
{
    int code;
    uint64_t arraysize;
    uint64_t index;
    pdf_string *name = NULL;
    pdf_dict *filespec = NULL;

    arraysize = pdfi_array_size(names);
    if ((arraysize % 2) != 0) {
        code = gs_note_error(gs_error_syntaxerror);
        goto exit;
    }

    /* This is supposed to be an array of
     * [ (filename1) (filespec1) (filename2) (filespec2) ... ]
     */
    for (index = 0; index < arraysize; index += 2) {
        code = pdfi_array_get_type(ctx, names, index, PDF_STRING, (pdf_obj **)&name);
        if (code < 0) goto exit;

        code = pdfi_array_get_type(ctx, names, index+1, PDF_DICT, (pdf_obj **)&filespec);
        if (code < 0) goto exit;

        code = pdfi_pdfmark_embed_filespec(ctx, name, filespec);
        if (code < 0) goto exit;

        pdfi_countdown(name);
        name = NULL;
        pdfi_countdown(filespec);
        filespec = NULL;
    }


 exit:
    pdfi_countdown(name);
    pdfi_countdown(filespec);
    return code;
}

/* Handle PageLabels for pdfwrite device */
static int pdfi_doc_EmbeddedFiles(pdf_context *ctx)
{
    int code;
    pdf_dict *Names = NULL;
    pdf_dict *EmbeddedFiles = NULL;
    pdf_array *Names_array = NULL;
    pdf_array *Kids = NULL;

    code = pdfi_dict_knownget_type(ctx, ctx->Root, "Collection", PDF_DICT, (pdf_obj **)&Names);
    if (code < 0) goto exit;
    if (code > 0) {
        code = 0;
        goto exit;
    }

    code = pdfi_dict_knownget_type(ctx, ctx->Root, "Names", PDF_DICT, (pdf_obj **)&Names);
    if (code <= 0) goto exit;

    code = pdfi_dict_knownget_type(ctx, Names, "EmbeddedFiles", PDF_DICT, (pdf_obj **)&EmbeddedFiles);
    if (code <= 0) goto exit;

    code = pdfi_dict_knownget_type(ctx, Names, "Kids", PDF_ARRAY, (pdf_obj **)&Kids);
    if (code < 0) goto exit;
    if (code > 0) {
        /* TODO: Need to implement */
        dmprintf(ctx->memory, "*** WARNING Kids array in EmbeddedFiles not implemented\n");
    }

    /* TODO: This is a name tree.
     * Can contain a Names array, or some complicated Kids.
     * Just handling Names array for now
     */
    code = pdfi_dict_knownget_type(ctx, EmbeddedFiles, "Names", PDF_ARRAY, (pdf_obj **)&Names_array);
    if (code <= 0) goto exit;

    code = pdfi_doc_EmbeddedFiles_Names(ctx, Names_array);
    if (code <= 0) goto exit;

 exit:
    pdfi_countdown(Kids);
    pdfi_countdown(Names);
    pdfi_countdown(EmbeddedFiles);
    pdfi_countdown(Names_array);
    return code;
}

/* Handle some bookkeeping related to AcroForm (and annotations)
 * See pdf_main.ps/process_trailer_attrs/AcroForm
 *
 * Mainly we preload AcroForm and NeedAppearances in the context
 *
 * TODO: gs code also seems to do something to link up parents in fields/annotations (ParentField)
 * We are going to avoid doing that for now.
 */
static int pdfi_doc_AcroForm(pdf_context *ctx)
{
    int code = 0;
    pdf_dict *AcroForm = NULL;
    bool boolval = false;

    code = pdfi_dict_knownget_type(ctx, ctx->Root, "AcroForm", PDF_DICT, (pdf_obj **)&AcroForm);
    if (code <= 0) goto exit;

    code = pdfi_dict_get_bool(ctx, AcroForm, "NeedAppearances", &boolval);
    if (code < 0) {
        if (code == gs_error_undefined) {
            boolval = true;
            code = 0;
        }
        else
            goto exit;
    }
    ctx->NeedAppearances = boolval;

    /* Save this for efficiency later */
    ctx->AcroForm = AcroForm;
    pdfi_countup(AcroForm);

    /* TODO: Link up ParentField (but hopefully we can avoid doing this hacky mess).
     * Also: Something to do with Bug692447.pdf?
     */


 exit:
    pdfi_countdown(AcroForm);
    return code;
}


static int pdfi_doc_view(pdf_context *ctx)
{
    int code = 0;
    pdf_dict *tempdict = NULL;
    pdf_name *Mode = NULL, *Layout = NULL;
    pdf_obj *ActionDest = NULL;

    code = pdfi_dict_knownget_type(ctx, ctx->Root, "PageMode", PDF_NAME, (pdf_obj **)&Mode);
    if (code < 0)
        return code;

    if (code != 0) {
        code = pdfi_dict_alloc(ctx, 1, &tempdict);
        if (code < 0)
            goto exit;

        pdfi_countup(tempdict);

        code = pdfi_dict_put(ctx, tempdict, "PageMode", (pdf_obj *)Mode);
        if (code < 0)
            goto exit;

        code = pdfi_pdfmark_from_dict(ctx, tempdict, NULL, "DOCVIEW");
        if (code < 0)
            goto exit;
        pdfi_countdown(tempdict);
        tempdict = NULL;
    }

    code = pdfi_dict_knownget_type(ctx, ctx->Root, "PageLayout", PDF_NAME, (pdf_obj **)&Layout);
    if (code < 0)
        goto exit;

    if (code != 0) {
        code = pdfi_dict_alloc(ctx, 1, &tempdict);
        if (code < 0)
            goto exit;

        pdfi_countup(tempdict);

        code = pdfi_dict_put(ctx, tempdict, "PageLayout", (pdf_obj *)Layout);
        if (code < 0)
            goto exit;

        code = pdfi_pdfmark_from_dict(ctx, tempdict, NULL, "DOCVIEW");
        if (code < 0)
            goto exit;
        pdfi_countdown(tempdict);
        tempdict = NULL;
    }

    code = pdfi_dict_knownget(ctx, ctx->Root, "OpenAction", &ActionDest);
    if (code < 0)
        goto exit;

    if (code != 0) {
        if (pdfi_type_of(ActionDest) == PDF_DICT) {
            /* Dictionary means this is an action */
            code = pdfi_dict_alloc(ctx, 1, &tempdict);
            if (code < 0)
                goto exit;

            pdfi_countup(tempdict);

            code = pdfi_dict_put(ctx, tempdict, "A", (pdf_obj *)ActionDest);
            if (code < 0)
                goto exit;

            code = pdfi_pdfmark_modA(ctx, tempdict);
            if (code < 0) goto exit;

            code = pdfi_pdfmark_from_dict(ctx, tempdict, NULL, "DOCVIEW");
        } else {
            if (pdfi_type_of(ActionDest) == PDF_ARRAY) {
                /* Array means this is a destination */
                code = pdfi_dict_alloc(ctx, 1, &tempdict);
                if (code < 0)
                    goto exit;

                pdfi_countup(tempdict);

                code = pdfi_dict_put(ctx, tempdict, "Dest", (pdf_obj *)ActionDest);
                if (code < 0)
                    goto exit;
                code = pdfi_pdfmark_modDest(ctx, tempdict);
                if (code < 0)
                    goto exit;
                code = pdfi_pdfmark_from_dict(ctx, tempdict, NULL, "DOCVIEW");
                if (code < 0)
                    goto exit;
            } else {
                code = gs_note_error(gs_error_typecheck);
                goto exit;
            }
        }
    }

exit:
    pdfi_countdown(ActionDest);
    pdfi_countdown(Layout);
    pdfi_countdown(Mode);
    pdfi_countdown(tempdict);
    return code;
}


/* See pdf_main.ps/process_trailer_attrs()
 * Some of this stuff is about pdfmarks, and some of it is just handling
 * random things in the trailer.
 */
int pdfi_doc_trailer(pdf_context *ctx)
{
    int code = 0;

    /* Can't do this stuff with no Trailer */
    if (!ctx->Trailer) {
        if ((code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_undefined), NULL, W_PDF_BAD_TRAILER, "pdfi_doc_trailer", NULL)) < 0)
            goto exit;
    }

    if (ctx->device_state.writepdfmarks) {
        /* Handle Outlines */
        code = pdfi_doc_Outlines(ctx);
        if (code < 0) {
            if ((code = pdfi_set_warning_stop(ctx, code, NULL, W_PDF_BAD_OUTLINES, "pdfi_doc_trailer", NULL)) < 0)
                goto exit;
        }

        /* Handle Docview pdfmark stuff */
        if (ctx->args.preservedocview) {
            code = pdfi_doc_view(ctx);
            if (code < 0) {
                if ((code = pdfi_set_warning_stop(ctx, code, NULL, W_PDF_BAD_VIEW, "pdfi_doc_view", NULL)) < 0)
                    goto exit;
            }
        }

        /* Handle Info */
        code = pdfi_doc_Info(ctx);
        if (code < 0) {
            /* pdfwrite will set a Fatal error when processing the DOCINFO if it has been
             * told to create a PDF/A. the PDFA compatibility is 2, and the file info
             * cannot be handled. In that case, abort immediately.
             */
            if ((code = pdfi_set_warning_stop(ctx, code, NULL, W_PDF_BAD_INFO, "pdfi_doc_trailer", NULL)) < 0)
                goto exit;
        }

        /* Handle EmbeddedFiles */
        /* TODO: add a configuration option to embed or omit */
        if (ctx->args.preserveembeddedfiles) {
            code = pdfi_doc_EmbeddedFiles(ctx);
            if (code < 0) {
                if ((code = pdfi_set_warning_stop(ctx, code, NULL, W_PDF_BAD_EMBEDDEDFILES, "pdfi_doc_trailer", NULL)) < 0)
                    goto exit;
            }
        }
    }

    /* Handle OCProperties */
    /* NOTE: Apparently already handled by pdfi_read_OptionalRoot() */

    /* Handle AcroForm -- this is some bookkeeping once per doc, not rendering them yet */
    code = pdfi_doc_AcroForm(ctx);
    if (code < 0) {
        if ((code = pdfi_set_warning_stop(ctx, code, NULL, W_PDF_BAD_ACROFORM, "pdfi_doc_trailer", NULL)) < 0)
            goto exit;
    }

    /* Handle OutputIntent ICC Profile */
    code = pdfi_doc_OutputIntents(ctx);
    if (code < 0) {
        if ((code = pdfi_set_warning_stop(ctx, code, NULL, W_PDF_BAD_OUTPUTINTENTS, "pdfi_doc_trailer", NULL)) < 0)
            goto exit;
    }

    /* Handle PageLabels */
    code = pdfi_doc_PageLabels(ctx);
    if (code < 0) {
        if ((code = pdfi_set_warning_stop(ctx, code, NULL, W_PDF_BAD_PAGELABELS, "pdfi_doc_trailer", NULL)) < 0)
            goto exit;
    }

 exit:
    return code;
}
