/* Copyright (C) 2001-2020 Artifex Software, Inc.
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

int pdfi_read_Root(pdf_context *ctx)
{
    pdf_obj *o, *o1;
    int code;

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading Root dictionary\n");

    code = pdfi_dict_get(ctx, ctx->Trailer, "Root", &o1);
    if (code < 0)
        return code;

    if (o1->type == PDF_INDIRECT) {
        code = pdfi_dereference(ctx, ((pdf_indirect_ref *)o1)->ref_object_num,  ((pdf_indirect_ref *)o1)->ref_generation_num, &o);
        pdfi_countdown(o1);
        if (code < 0)
            return code;

        if (o->type != PDF_DICT) {
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
        if (o1->type != PDF_DICT) {
            pdfi_countdown(o1);
            return_error(gs_error_typecheck);
        }
    }

    code = pdfi_dict_get_type(ctx, (pdf_dict *)o1, "Type", PDF_NAME, &o);
    if (code < 0) {
        pdfi_countdown(o1);
        return code;
    }
    if (pdfi_name_strcmp((pdf_name *)o, "Catalog") != 0){
        pdfi_countdown(o);
        pdfi_countdown(o1);
        return_error(gs_error_syntaxerror);
    }
    pdfi_countdown(o);

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n");
    /* We don't pdfi_countdown(o1) now, because we've transferred our
     * reference to the pointer in the pdf_context structure.
     */
    pdfi_countdown(ctx->Root); /* If file was repaired it might be set already */
    ctx->Root = (pdf_dict *)o1;
    return 0;
}

int pdfi_read_Info(pdf_context *ctx)
{
    pdf_obj *o, *o1;
    int code;

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading Info dictionary\n");

    code = pdfi_dict_get(ctx, ctx->Trailer, "Info", &o1);
    if (code < 0)
        return code;

    if (o1->type == PDF_INDIRECT) {
        code = pdfi_dereference(ctx, ((pdf_indirect_ref *)o1)->ref_object_num,  ((pdf_indirect_ref *)o1)->ref_generation_num, &o);
        pdfi_countdown(o1);
        if (code < 0)
            return code;

        if (o->type != PDF_DICT) {
            pdfi_countdown(o);
            return_error(gs_error_typecheck);
        }

        code = pdfi_dict_put(ctx, ctx->Trailer, "Info", o);
        if (code < 0) {
            pdfi_countdown(o);
            return code;
        }
        o1 = o;
    } else {
        if (o1->type != PDF_DICT) {
            pdfi_countdown(o1);
            return_error(gs_error_typecheck);
        }
    }

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n");
    /* We don't pdfi_countdown(o1) now, because we've transferred our
     * reference to the pointer in the pdf_context structure.
     */
    ctx->Info = (pdf_dict *)o1;
    return 0;
}

int pdfi_read_Pages(pdf_context *ctx)
{
    pdf_obj *o, *o1;
    int code;
    double d;

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading Pages dictionary\n");

    code = pdfi_dict_get(ctx, ctx->Root, "Pages", &o1);
    if (code < 0)
        return code;

    if (o1->type == PDF_INDIRECT) {
        code = pdfi_dereference(ctx, ((pdf_indirect_ref *)o1)->ref_object_num,  ((pdf_indirect_ref *)o1)->ref_generation_num, &o);
        pdfi_countdown(o1);
        if (code < 0)
            return code;

        if (o->type != PDF_DICT) {
            pdfi_countdown(o);
            ctx->pdf_errors |= E_PDF_BADPAGEDICT;
            dmprintf(ctx->memory, "*** Error: Something is wrong with the Pages dictionary.  Giving up.\n");
            if (o->type == PDF_INDIRECT)
                dmprintf(ctx->memory, "           Double indirect reference.  Loop in Pages tree?\n");
            return_error(gs_error_typecheck);
        }

        code = pdfi_dict_put(ctx, ctx->Root, "Pages", o);
        if (code < 0) {
            pdfi_countdown(o);
            return code;
        }
        o1 = o;
    } else {
        if (o1->type != PDF_DICT) {
            pdfi_countdown(o1);
            return_error(gs_error_typecheck);
        }
    }

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n");

    /* Acrobat allows the Pages Count to be a flaoting point nuber (!) */
    code = pdfi_dict_get_number(ctx, (pdf_dict *)o1, "Count", &d);
    if (code < 0)
        return code;

    if (floor(d) != d) {
        pdfi_countdown(o1);
        return_error(gs_error_rangecheck);
    } else {
        ctx->num_pages = (int)floor(d);
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

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading other Root contents\n");

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% OCProperties\n");
    code = pdfi_dict_get_type(ctx, ctx->Root, "OCProperties", PDF_DICT, &obj);
    if (code == 0) {
        ctx->OCProperties = (pdf_dict *)obj;
    } else {
        ctx->OCProperties = NULL;
        if (ctx->pdfdebug)
            dmprintf(ctx->memory, "%% (None)\n");
    }

}

void pdfi_free_OptionalRoot(pdf_context *ctx)
{
    if (ctx->OCProperties) {
        pdfi_countdown(ctx->OCProperties);
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

    if (node->type != PDF_INDIRECT && node->type != PDF_DICT) {
        code = gs_note_error(gs_error_typecheck);
        goto errorExit;
    }

    if (node->type == PDF_INDIRECT) {
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
        if (child->type != PDF_DICT) {
            code = gs_note_error(gs_error_typecheck);
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
                ctx->pdf_errors |= E_PDF_BADPAGETYPE;
            /* Make a 'PageRef' entry (just stores an indirect reference to the actual page)
             * and store that in the Kids array for future reference. But pass on the
             * dereferenced Page dictionary, in case this is the target page.
             */

            code = pdfi_alloc_object(ctx, PDF_DICT, 0, (pdf_obj **)&leaf_dict);
            if (code < 0)
                goto errorExit;
            code = pdfi_make_name(ctx, (byte *)"PageRef", 7, (pdf_obj **)&Key);
            if (code < 0)
                goto errorExit;
            code = pdfi_dict_put_obj(ctx, leaf_dict, (pdf_obj *)Key, (pdf_obj *)node);
            if (code < 0)
                goto errorExit;
            code = pdfi_dict_put(ctx, leaf_dict, "Type", (pdf_obj *)Key);
            if (code < 0)
                goto errorExit;
            code = pdfi_array_put(ctx, Kids, i, (pdf_obj *)leaf_dict);
            if (code < 0)
                goto errorExit;
        }
    } else {
        child = (pdf_dict *)node;
        pdfi_countup(child);
    }

    *pchild = child;
    child = NULL;

 errorExit:
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

    if (ctx->pdfdebug)
        dmprintf1(ctx->memory, "%% Finding page dictionary for page %"PRIi64"\n", page_num + 1);

    /* Allocated inheritable dict (it might stay empty) */
    code = pdfi_alloc_object(ctx, PDF_DICT, 0, (pdf_obj **)&inheritable);
    if (code < 0)
        return code;
    pdfi_countup(inheritable);

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
                        pdf_dict *d = NULL;

                        code = pdfi_dict_get(ctx, child, "PageRef", (pdf_obj **)&d);
                        if (code < 0)
                            goto exit;
                        code = pdfi_merge_dicts(ctx, d, inheritable);
                        *target = d;
                        pdfi_countup(*target);
                        pdfi_countdown(d);
                        goto exit;
                    } else {
                        *page_offset += 1;
                    }
                } else {
                    if (!pdfi_name_is(Type, "Page"))
                        ctx->pdf_errors |= E_PDF_BADPAGETYPE;
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
    int code;

    *o = NULL;

    /* Check the provided dict */
    code = pdfi_resource_knownget_typedict(ctx, Type, dict, &typedict);
    if (code < 0)
        goto exit;
    if (code > 0) {
        code = pdfi_dict_get_no_store_R_key(ctx, typedict, name, o);
        if (code != gs_error_undefined)
            goto exit;
    }

    /* Check the Parents, if any */
    code = pdfi_dict_knownget_type(ctx, dict, "Parent", PDF_DICT, (pdf_obj **)&Parent);
    if (code < 0)
        goto exit;
    if (code > 0) {
        if (Parent->object_num != ctx->CurrentPageDict->object_num) {
            code = pdfi_find_resource(ctx, Type, name, Parent, page_dict, o);
            if (code != gs_error_undefined)
                goto exit;
        }
    }

    pdfi_countdown(typedict);
    typedict = NULL;

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
            goto exit;
        }
    }

    pdfi_countdown(typedict);
    typedict = NULL;

    if (ctx->CurrentPageDict != NULL) {
        code = pdfi_resource_knownget_typedict(ctx, Type, ctx->CurrentPageDict, &typedict);
        if (code < 0)
            goto exit;

        if (code > 0) {
            code = pdfi_dict_get_no_store_R_key(ctx, typedict, name, o);
            goto exit;
        }
    }

    /* If we got all the way down there, we didn't find it */
    code = gs_error_undefined;

exit:
    pdfi_countdown(typedict);
    pdfi_countdown(Parent);
    return code;
}

/* Count how many children an outline entry has
 * This is separate just to keep the code from getting cluttered.
 */
static int pdfi_doc_outline_count(pdf_context *ctx, pdf_dict *outline, int64_t *count)
{
    int code = 0;
    pdf_dict *child = NULL;
    pdf_dict *Next = NULL;

    /* Handle this outline entry */
    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        goto exit1;

    /* Count the children (don't deref them, we don't want to leave them hanging around) */
    code = pdfi_dict_get_no_store_R(ctx, outline, "First", (pdf_obj **)&child);
    if (code < 0 || child->type != PDF_DICT) {
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
        (*count) ++;

        code = pdfi_dict_get_no_store_R(ctx, child, "Next", (pdf_obj **)&Next);
        if (code == gs_error_circular_reference) {
            code = 0;
            goto exit;
        }
        if (code == gs_error_undefined) {
            code = 0;
            break;
        }

        if (code < 0 || Next->type != PDF_DICT)
            goto exit;

        pdfi_countdown(child);
        child = Next;
    } while (true);

 exit:
    (void)pdfi_loop_detector_cleartomark(ctx);
 exit1:
    pdfi_countdown(child);
    pdfi_countdown(Next);
    return code;
}

/* Mark the actual outline */
static int pdfi_doc_mark_the_outline(pdf_context *ctx, pdf_dict *outline)
{
    int code = 0;
    int64_t count = 0;
    int64_t numkids = 0;
    pdf_dict *tempdict = NULL;
    uint64_t dictsize;
    pdf_obj *tempobj = NULL;
    bool resolve = false;

    /* Basically we only do /Count, /Title and /A
     * The /First, /Last, /Next get written magically by pdfwrite (as far as I can tell)
     */
    /* Count how many kids there are */
    code = pdfi_doc_outline_count(ctx, outline, &numkids);

    /* If no kids, see if there is a Count */
    if (numkids == 0) {
        code = pdfi_dict_get_int(ctx, outline, "Count", &count);
        if (code < 0 && code != gs_error_undefined)
            goto exit;
        if (count < 0)
            count = -count;
    } else {
        count = numkids;
    }

    /* NOTE: I am just grabbing the keys I want, rather than walking through
     * the whole dictionary.  If you walk through the whole dictionary you
     * will likely trigger some circular references because of the /Parent
     * keys and stuff.  There are ways around that, but not worth the trouble
     * for this case.
     */

    /* Make a temporary copy of the outline dict */
    dictsize = pdfi_dict_entries(outline);
    code = pdfi_alloc_object(ctx, PDF_DICT, dictsize, (pdf_obj **)&tempdict);
    if (code < 0) goto exit;
    pdfi_countup(tempdict);

    /* If count is non-zero, put in dictionary */
    if (count != 0) {
        code = pdfi_dict_put_int(ctx, tempdict, "Count", count);
        if (code < 0)
            goto exit;
    }

    /* Put Title in tempdict */
    code = pdfi_dict_knownget(ctx, outline, "Title", &tempobj);
    if (code > 0)
        code = pdfi_dict_put(ctx, tempdict, "Title", tempobj);
    if (code < 0)
        goto exit;
    pdfi_countdown(tempobj);
    tempobj = NULL;

    /* Put C in tempdict */
    code = pdfi_dict_knownget(ctx, outline, "C", &tempobj);
    if (code > 0)
        code = pdfi_dict_put(ctx, tempdict, "C", tempobj);
    if (code < 0)
        goto exit;
    pdfi_countdown(tempobj);
    tempobj = NULL;

    /* Put A in tempdict */
    code = pdfi_dict_knownget_type(ctx, outline, "A", PDF_DICT, &tempobj);
    if (code > 0) {
        code = pdfi_dict_put(ctx, tempdict, "A", tempobj);
        if (code < 0)
            goto exit;
        /* Turn it into a /Page /View */
        code = pdfi_mark_modA(ctx, tempdict, &resolve);
    }
    if (code < 0)
        goto exit;
    pdfi_countdown(tempobj);
    tempobj = NULL;

    /* Put Dest in tempdict */
    code = pdfi_dict_knownget(ctx, outline, "Dest", &tempobj);
    if (code > 0) {
        code = pdfi_dict_put(ctx, tempdict, "Dest", tempobj);
        if (code < 0)
            goto exit;
        /* Turn it into a /Page /View */
        code = pdfi_mark_modDest(ctx, tempdict);
    }
    if (code < 0)
        goto exit;
    pdfi_countdown(tempobj);
    tempobj = NULL;

    /* Write the pdfmark */
    code = pdfi_mark_from_dict(ctx, tempdict, NULL, "OUT");
    if (code < 0)
        goto exit;

 exit:
    pdfi_countdown(tempdict);
    pdfi_countdown(tempobj);
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
    if (code < 0 || child->type != PDF_DICT) {
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
        if (code < 0 || Next->type != PDF_DICT)
            goto exit;

        pdfi_countdown(child);
        child = Next;
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

    if (ctx->no_pdfmark_outlines)
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
    if (code < 0 || outline->type != PDF_DICT) {
        /* TODO: flag a warning? */
        code = 0;
        goto exit;
    }

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
        if (code < 0 || outline->type != PDF_DICT)
            goto exit;

        pdfi_countdown(outline);
        outline = Next;
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
    pdf_dict *Info = NULL;
    pdf_dict *tempdict = NULL;
    uint64_t dictsize;
    uint64_t index;
    pdf_name *Key = NULL;
    pdf_obj *Value = NULL;

    code = pdfi_dict_knownget_type(ctx, ctx->Trailer, "Info", PDF_DICT, (pdf_obj **)&Info);
    if (code <= 0) {
        /* TODO: flag a warning */
        goto exit;
    }

    /* Make a temporary copy of the Info dict */
    dictsize = pdfi_dict_entries(Info);
    code = pdfi_alloc_object(ctx, PDF_DICT, dictsize, (pdf_obj **)&tempdict);
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
            code = pdfi_dict_put_obj(ctx, tempdict, (pdf_obj *)Key, Value);
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
    code = pdfi_mark_from_dict(ctx, tempdict, NULL, "DOCINFO");

 exit:
    pdfi_countdown(Key);
    pdfi_countdown(Value);
    pdfi_countdown(Info);
    pdfi_countdown(tempdict);
    return code;
}

/* See pdf_main.ps/process_trailer_attrs() */
int pdfi_doc_trailer(pdf_context *ctx)
{
    int code = 0;

    /* TODO: writeoutputintents */

    if (!ctx->writepdfmarks)
        goto exit;

    /* Can't do this stuff with no Trailer */
    if (!ctx->Trailer)
        goto exit;

    /* Handle Outlines */
    code = pdfi_doc_Outlines(ctx);
    if (code < 0)
        goto exit;

    /* Handle Info */
    code = pdfi_doc_Info(ctx);
    if (code < 0)
        goto exit;

    /* Handle OCProperties */
    /* TODO: */

    /* Handle AcroForm */
    /* TODO: */

    /* Handle OutputIntent ICC Profile */
    /* TODO: */

    /* Handle PageLabels */
    /* TODO: */

 exit:
    return code;
}
