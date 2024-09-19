/* Copyright (C) 2019-2024 Artifex Software, Inc.
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

/* Checks for transparency and spots for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_page.h"
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_loop_detect.h"
#include "pdf_colour.h"
#include "pdf_trans.h"
#include "pdf_font_types.h"
#include "pdf_gstate.h"
#include "pdf_misc.h"
#include "pdf_check.h"
#include "pdf_device.h"
#include "gsdevice.h"       /* For gs_setdevice_no_erase */
#include "gspaint.h"        /* For gs_erasepage() */

/* For performance and resource reasons we do not want to install the transparency blending
 * compositor unless we need it. Similarly, if a device handles spot colours it can minimise
 * memory usage if it knows ahead of time how many spot colours there will be.
 *
 * The PDF interpreter written in PostScript performed these as two separate tasks, on opening
 * a PDF file it would count spot colour usage and then for each page it would chek if the page
 * used any transparency. The code below is used to check for both transparency and spot colours.
 * If the int pointer to num_spots is NULL then we aren't interested in spot colours (not supported
 * by the device), if the int pointer to transparent is NULL then we aren't interested in transparency
 * (-dNOTRANSPARENCY is set).
 *
 * Currently the code is run when we open the PDF file, the existence of transparency on any given
 * page is recorded in a bit array for later use. If it turns out that checking every page when we
 * open the file is a performance burden then we could do it on a per-page basis instead. NB if
 * the device supports spot colours then we do need to scan the file before starting any page.
 *
 * The technique is fairly straight-forward, we start with each page, and open its Resources
 * dictionary, we then check by type each possible resource. Some resources (eg Pattern, XObject)
 * can themselves contain Resources, in which case we recursively check that dictionary. Note; not
 * all Resource types need to be checked for both transparency and spot colours, some types can
 * only contain one or the other.
 *
 * Routines with the name pdfi_check_xxx_dict are intended to check a Resource dictionary entry, this
 * will be a dictioanry of names and values, where the values are objects of the given Resource type.
 *
 */

/* Structure for keeping track of resources as they are being checked */
/* CheckedResources is a bit of a hack, for the file Bug697655.pdf. That file has many (unused)
 * XObjects which use Resources and the Resources use other XObjects and so on nested
 * very deeply. This takes *hours* to check. Ghostscript gets around this because it
 * stores all the objects (which we don't want to do because its wasteful) and checking
 * to see if its already tested a given resource for spots/transparency.
 * This is a temporary allocation, big enough to hold all the objects in the file (1 per bit)
 * each time we have fully checked a resource we add it here, when checking a resource we
 * first check this list to see if its already been checked, in which case we can skip
 * it. When done we release the memory.
 */
typedef struct {
    bool transparent;
    bool BM_Not_Normal;
    bool has_overprint; /* Does it have OP or op in an ExtGState? */
    pdf_dict *spot_dict;
    pdf_array *font_array;
    uint32_t size;
    byte *CheckedResources;
} pdfi_check_tracker_t;

static int pdfi_check_Resources(pdf_context *ctx, pdf_dict *Resources_dict, pdf_dict *page_dict, pdfi_check_tracker_t *tracker);

static inline bool resource_is_checked(pdfi_check_tracker_t *tracker, pdf_obj *o)
{
    uint32_t byte_offset;
    byte bit_offset;
    int object_num;

    if(tracker->CheckedResources == NULL)
        return 0;

    /* objects with object number 0 are directly defined, we can't
     * store those so just return immediately
     */
    object_num = pdf_object_num(o);
    if (object_num > 0 && (object_num >> 3) < tracker->size) {
        /* CheckedResources is a byte array, each byte represents
         * 8 objects. So the object number / 8 is the byte offset
         * into the array, and then object number % 8 is the bit
         * within that byte that we want.
         */
        bit_offset = 0x01 << (object_num % 8);
        byte_offset = object_num >> 3;

        /* If its already set, then return that. */
        if (tracker->CheckedResources[byte_offset] & bit_offset)
            return true;
        else
            /* Otherwise set it for futre reference */
            tracker->CheckedResources[byte_offset] |= bit_offset;
    }
    return false;
}


static int
pdfi_check_free_tracker(pdf_context *ctx, pdfi_check_tracker_t *tracker)
{
    gs_free_object(ctx->memory, tracker->CheckedResources, "pdfi_check_free_tracker(flags)");
    pdfi_countdown(tracker->spot_dict);
    pdfi_countdown(tracker->font_array);
    memset(tracker, 0, sizeof(*tracker));
    return 0;
}

static int
pdfi_check_init_tracker(pdf_context *ctx, pdfi_check_tracker_t *tracker, pdf_array **fonts_array, pdf_array **spot_array)
{
    int code = 0;

    memset(tracker, 0, sizeof(*tracker));

    tracker->size = (ctx->xref_table->xref_size + 7) / 8;
    tracker->CheckedResources = gs_alloc_bytes(ctx->memory, tracker->size,
                                            "pdfi_check_init_tracker(flags)");
    if (tracker->CheckedResources == NULL)
        return_error(gs_error_VMerror);

    memset(tracker->CheckedResources, 0x00, tracker->size);

    if (ctx->device_state.spot_capable ||
        (ctx->pgs->device->icc_struct->overprint_control) == gs_overprint_control_simulate ||
        spot_array != NULL)
    {
        code = pdfi_dict_alloc(ctx, 32, &tracker->spot_dict);
        if (code < 0)
            goto cleanup;
        pdfi_countup(tracker->spot_dict);
    }

    if (fonts_array != NULL) {
        code = pdfi_array_alloc(ctx, 0, &tracker->font_array);
        if (code < 0)
            goto cleanup;
        pdfi_countup(tracker->font_array);
    }

    return 0;

cleanup:
    pdfi_check_free_tracker(ctx, tracker);
    return code;
}

/*
 * Check the Resources dictionary ColorSpace entry. pdfi_check_ColorSpace_for_spots is defined
 * in pdf_colour.c
 */
static int pdfi_check_ColorSpace_dict(pdf_context *ctx, pdf_dict *cspace_dict,
                                      pdf_dict *page_dict, pdfi_check_tracker_t *tracker)
{
    int code;
    uint64_t i, index;
    pdf_obj *Key = NULL, *Value = NULL;

    if (resource_is_checked(tracker, (pdf_obj *)cspace_dict))
        return 0;

    if (pdfi_type_of(cspace_dict) != PDF_DICT)
        return_error(gs_error_typecheck);

    if (pdfi_dict_entries(cspace_dict) > 0) {
        code = pdfi_loop_detector_mark(ctx); /* Mark the start of the ColorSpace dictionary loop */
        if (code < 0)
            return code;

        code = pdfi_dict_first(ctx, cspace_dict, &Key, &Value, &index);
        if (code < 0)
            goto error1;

        i = 1;
        do {
            code = pdfi_check_ColorSpace_for_spots(ctx, Value, cspace_dict, page_dict, tracker->spot_dict);
            if (code < 0)
                goto error2;

            pdfi_countdown(Key);
            Key = NULL;
            pdfi_countdown(Value);
            Value = NULL;

            (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the Shading dictionary loop */

            code = pdfi_loop_detector_mark(ctx); /* Mark the new start of the Shading dictionary loop */
            if (code < 0)
                goto error1;

            do {
                if (i++ >= pdfi_dict_entries(cspace_dict)) {
                    code = 0;
                    goto transparency_exit;
                }

                code = pdfi_dict_next(ctx, cspace_dict, &Key, &Value, &index);
                if (code == 0 && pdfi_type_of(Value) == PDF_ARRAY)
                    break;
                pdfi_countdown(Key);
                Key = NULL;
                pdfi_countdown(Value);
                Value = NULL;
            } while(1);
        }while (1);
    }
    return 0;

transparency_exit:
error2:
    pdfi_countdown(Key);
    pdfi_countdown(Value);

error1:
    (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the current resource loop */
    return code;
}

/*
 * Process an individual Shading dictionary to see if it contains a ColorSpace with a spot colour
 */
static int pdfi_check_Shading(pdf_context *ctx, pdf_obj *shading,
                              pdf_dict *page_dict, pdfi_check_tracker_t *tracker)
{
    int code;
    pdf_obj *o = NULL;
    pdf_dict *shading_dict = NULL;

    if (resource_is_checked(tracker, shading))
        return 0;

    code = pdfi_dict_from_obj(ctx, shading, &shading_dict);
    if (code < 0)
        return code;

    if (pdfi_type_of(shading_dict) != PDF_DICT)
        return_error(gs_error_typecheck);

    code = pdfi_dict_knownget(ctx, shading_dict, "ColorSpace", (pdf_obj **)&o);
    if (code > 0) {
        code = pdfi_check_ColorSpace_for_spots(ctx, o, shading_dict, page_dict, tracker->spot_dict);
        pdfi_countdown(o);
        return code;
    }
    return 0;
}

/*
 * Check the Resources dictionary Shading entry.
 */
static int pdfi_check_Shading_dict(pdf_context *ctx, pdf_dict *shading_dict,
                                   pdf_dict *page_dict, pdfi_check_tracker_t *tracker)
{
    int code;
    uint64_t i, index;
    pdf_obj *Key = NULL, *Value = NULL;

    if (resource_is_checked(tracker, (pdf_obj *)shading_dict))
        return 0;

    if (pdfi_type_of(shading_dict) != PDF_DICT)
        return_error(gs_error_typecheck);

    if (pdfi_dict_entries(shading_dict) > 0) {
        code = pdfi_loop_detector_mark(ctx); /* Mark the start of the Shading dictionary loop */
        if (code < 0)
            return code;

        code = pdfi_dict_first(ctx, shading_dict, &Key, &Value, &index);
        if (code < 0 || !(pdfi_type_of(Value) == PDF_DICT || pdfi_type_of(Value) == PDF_STREAM))
            goto error2;

        i = 1;
        do {
            code = pdfi_check_Shading(ctx, Value, page_dict, tracker);
            if (code < 0)
                goto error2;

            pdfi_countdown(Key);
            Key = NULL;
            pdfi_countdown(Value);
            Value = NULL;

            (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the Shading dictionary loop */

            code = pdfi_loop_detector_mark(ctx); /* Mark the new start of the Shading dictionary loop */
            if (code < 0)
                goto error1;

            do {
                if (i++ >= pdfi_dict_entries(shading_dict)) {
                    code = 0;
                    goto transparency_exit;
                }

                code = pdfi_dict_next(ctx, shading_dict, &Key, &Value, &index);
                if (code == 0 && pdfi_type_of(Value) == PDF_DICT)
                    break;
                pdfi_countdown(Key);
                Key = NULL;
                pdfi_countdown(Value);
                Value = NULL;
            } while(1);
        }while (1);
    }
    return 0;

transparency_exit:
error2:
    pdfi_countdown(Key);
    pdfi_countdown(Value);

error1:
    (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the current resource loop */
    return code;
}

/*
 * This routine checks an XObject to see if it contains any spot
 * colour definitions, or transparency usage.
 */
static int pdfi_check_XObject(pdf_context *ctx, pdf_dict *xobject, pdf_dict *page_dict,
                              pdfi_check_tracker_t *tracker)
{
    int code = 0;
    pdf_name *n = NULL;
    bool known = false;
    double f;

    if (resource_is_checked(tracker, (pdf_obj *)xobject))
        return 0;

    if (pdfi_type_of(xobject) != PDF_DICT)
        return_error(gs_error_typecheck);

    code = pdfi_dict_get_type(ctx, xobject, "Subtype", PDF_NAME, (pdf_obj **)&n);
    if (code >= 0) {
        if (pdfi_name_is((const pdf_name *)n, "Image")) {
            pdf_obj *CS = NULL;

            pdfi_countdown(n);
            n = NULL;
            code = pdfi_dict_known(ctx, xobject, "SMask", &known);
            if (code >= 0) {
                if (known == true) {
                    tracker->transparent = true;
                    if (tracker->spot_dict == NULL)
                        goto transparency_exit;
                }
                code = pdfi_dict_knownget_number(ctx, xobject, "SMaskInData", &f);
                if (code > 0) {
                    code = 0;
                    if (f != 0.0)
                        tracker->transparent = true;
                    if (tracker->spot_dict == NULL)
                        goto transparency_exit;
                }
                /* Check the image dictionary for a ColorSpace entry, if we are checking spot names */
                if (tracker->spot_dict) {
                    code = pdfi_dict_knownget(ctx, xobject, "ColorSpace", (pdf_obj **)&CS);
                    if (code > 0) {
                        /* We don't care if there's an error here, it'll be picked up if we use the ColorSpace later */
                        (void)pdfi_check_ColorSpace_for_spots(ctx, CS, xobject, page_dict, tracker->spot_dict);
                        pdfi_countdown(CS);
                    }
                }
            }
        } else {
            if (pdfi_name_is((const pdf_name *)n, "Form")) {
                pdf_dict *group_dict = NULL, *resource_dict = NULL;
                pdf_obj *CS = NULL;

                pdfi_countdown(n);
                code = pdfi_dict_knownget_type(ctx, xobject, "Group", PDF_DICT, (pdf_obj **)&group_dict);
                if (code > 0) {
                    tracker->transparent = true;
                    if (tracker->spot_dict != NULL) {
                        /* Start a new loop detector group to avoid this being detected in the Resources check below */
                        code = pdfi_loop_detector_mark(ctx); /* Mark the start of the XObject dictionary loop */
                        if (code == 0) {
                            code = pdfi_dict_knownget(ctx, group_dict, "CS", &CS);
                            if (code > 0)
                                /* We don't care if there's an error here, it'll be picked up if we use the ColorSpace later */
                                (void)pdfi_check_ColorSpace_for_spots(ctx, CS, group_dict, page_dict, tracker->spot_dict);
                            (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the XObject dictionary loop */
                        }
                        pdfi_countdown(group_dict);
                        pdfi_countdown(CS);
                    }
                    else if (tracker->BM_Not_Normal)
                    {
                        /* We already know it has a non-normal Blend Mode. No point in keeping searching. */
                        pdfi_countdown(group_dict);
                        goto transparency_exit;
                    }
                    else
                    {
                        pdfi_countdown(group_dict);
                    }
                    /* We need to keep checking Resources in case there are non-Normal blend mode things still to be found. */
                }

                code = pdfi_dict_knownget_type(ctx, xobject, "Resources", PDF_DICT, (pdf_obj **)&resource_dict);
                if (code > 0) {
                    if (ctx->loop_detection && pdf_object_num((pdf_obj *)resource_dict) != 0) {
                        code = pdfi_loop_detector_add_object(ctx, resource_dict->object_num);
                        if (code < 0) {
                            pdfi_countdown(resource_dict);
                            goto transparency_exit;
                        }
                    }
                    code = pdfi_check_Resources(ctx, resource_dict, page_dict, tracker);
                    pdfi_countdown(resource_dict);
                    if (code < 0)
                        goto transparency_exit;
                }
            } else
                pdfi_countdown(n);
        }
    }

    return 0;

transparency_exit:
    return code;
}

/*
 * Check the Resources dictionary XObject entry.
 */
static int pdfi_check_XObject_dict(pdf_context *ctx, pdf_dict *xobject_dict, pdf_dict *page_dict,
                                   pdfi_check_tracker_t *tracker)
{
    int code;
    uint64_t i, index;
    pdf_obj *Key = NULL, *Value = NULL;
    pdf_dict *Value_dict = NULL;

    if (resource_is_checked(tracker, (pdf_obj *)xobject_dict))
        return 0;

    if (pdfi_type_of(xobject_dict) != PDF_DICT)
        return_error(gs_error_typecheck);

    if (pdfi_dict_entries(xobject_dict) > 0) {
        code = pdfi_loop_detector_mark(ctx); /* Mark the start of the XObject dictionary loop */
        if (code < 0)
            return code;

        code = pdfi_dict_first(ctx, xobject_dict, &Key, &Value, &index);
        if (code < 0)
            goto error_exit;
        if (pdfi_type_of(Value) != PDF_STREAM)
            goto error_exit;

        i = 1;
        do {
            code = pdfi_dict_from_obj(ctx, Value, &Value_dict);
            if (code < 0)
                goto error_exit;

            code = pdfi_check_XObject(ctx, Value_dict, page_dict, tracker);
            if (code < 0)
                goto error_exit;

            (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the XObject dictionary loop */

            code = pdfi_loop_detector_mark(ctx); /* Mark the new start of the XObject dictionary loop */
            if (code < 0)
                goto error_exit;

            pdfi_countdown(Key);
            Key = NULL;
            pdfi_countdown(Value);
            Value = NULL;
            Value_dict = NULL;

            do {
                if (i++ >= pdfi_dict_entries(xobject_dict)) {
                    code = 0;
                    goto transparency_exit;
                }

                code = pdfi_dict_next(ctx, xobject_dict, &Key, &Value, &index);
                if (code == 0 && pdfi_type_of(Value) == PDF_STREAM)
                    break;
                pdfi_countdown(Key);
                Key = NULL;
                pdfi_countdown(Value);
                Value = NULL;
            } while(1);
        }while(1);
    }
    return 0;

transparency_exit:
error_exit:
    pdfi_countdown(Key);
    pdfi_countdown(Value);
    (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the current resource loop */
    return code;
}

/*
 * This routine checks an ExtGState dictionary to see if it contains transparency or OP
 */
static int pdfi_check_ExtGState(pdf_context *ctx, pdf_dict *extgstate_dict, pdf_dict *page_dict,
                                pdfi_check_tracker_t *tracker)
{
    int code;
    pdf_obj *o = NULL;
    double f;
    bool overprint;

    if (resource_is_checked(tracker, (pdf_obj *)extgstate_dict))
        return 0;

    if (pdfi_type_of(extgstate_dict) != PDF_DICT)
        return_error(gs_error_typecheck);

    if (pdfi_dict_entries(extgstate_dict) > 0) {
        /* See if /OP or /op is true */
        code = pdfi_dict_get_bool(ctx, extgstate_dict, "OP", &overprint);
        if (code == 0 && overprint)
            tracker->has_overprint = true;
        code = pdfi_dict_get_bool(ctx, extgstate_dict, "op", &overprint);
        if (code == 0 && overprint)
            tracker->has_overprint = true;

        /* Check Blend Mode *first* because we short-circuit transparency detection
         * when we find transparency, and we want to be able to record the use of
         * blend modes other than Compatible/Normal so that Patterns using these
         * always use the clist and not tiles. The tiles implementation can't work
         * if we change to a blend mode other than Normal or Compatible during
         * the course of a Pattern.
         */
        code = pdfi_dict_knownget_type(ctx, extgstate_dict, "BM", PDF_NAME, &o);
        if (code > 0) {
            if (!pdfi_name_is((pdf_name *)o, "Normal")) {
                if (!pdfi_name_is((pdf_name *)o, "Compatible")) {
                    pdfi_countdown(o);
                    tracker->transparent = true;
                    tracker->BM_Not_Normal = true;
                    return 0;
                }
            }
        }
        pdfi_countdown(o);
        o = NULL;

        /* Check SMask */
        code = pdfi_dict_knownget(ctx, extgstate_dict, "SMask", &o);
        if (code > 0) {
            switch (pdfi_type_of(o)) {
                case PDF_NAME:
                    if (!pdfi_name_is((pdf_name *)o, "None")) {
                        pdfi_countdown(o);
                        tracker->transparent = true;
                        return 0;
                    }
                    break;
                case PDF_DICT:
                {
                    pdf_obj *G = NULL;

                    tracker->transparent = true;

                    if (tracker->spot_dict != NULL) {
                        /* Check if the SMask has a /G (Group) */
                        code = pdfi_dict_knownget(ctx, (pdf_dict *)o, "G", &G);
                        if (code > 0) {
                            code = pdfi_check_XObject(ctx, (pdf_dict *)G, page_dict,
                                                      tracker);
                            pdfi_countdown(G);
                        }
                    }
                    pdfi_countdown(o);
                    return code;
                }
                default:
                    break;
            }
        }
        pdfi_countdown(o);
        o = NULL;

        code = pdfi_dict_knownget_number(ctx, extgstate_dict, "CA", &f);
        if (code > 0) {
            if (f != 1.0) {
                tracker->transparent = true;
                return 0;
            }
        }

        code = pdfi_dict_knownget_number(ctx, extgstate_dict, "ca", &f);
        if (code > 0) {
            if (f != 1.0) {
                tracker->transparent = true;
                return 0;
            }
        }

    }
    return 0;
}

/*
 * Check the Resources dictionary ExtGState entry.
 */
static int pdfi_check_ExtGState_dict(pdf_context *ctx, pdf_dict *extgstate_dict, pdf_dict *page_dict,
                                     pdfi_check_tracker_t *tracker)
{
    int code;
    uint64_t i, index;
    pdf_obj *Key = NULL, *Value = NULL;

    if (resource_is_checked(tracker, (pdf_obj *)extgstate_dict))
        return 0;

    if (pdfi_type_of(extgstate_dict) != PDF_DICT)
        return_error(gs_error_typecheck);

    if (pdfi_dict_entries(extgstate_dict) > 0) {
        code = pdfi_loop_detector_mark(ctx); /* Mark the start of the ColorSpace dictionary loop */
        if (code < 0)
            return code;

        code = pdfi_dict_first(ctx, extgstate_dict, &Key, &Value, &index);
        if (code < 0)
            goto error1;

        i = 1;
        do {

            (void)pdfi_check_ExtGState(ctx, (pdf_dict *)Value, page_dict, tracker);
            if (tracker->transparent == true && tracker->spot_dict == NULL)
                goto transparency_exit;

            pdfi_countdown(Key);
            Key = NULL;
            pdfi_countdown(Value);
            Value = NULL;

            (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the ExtGState dictionary loop */

            code = pdfi_loop_detector_mark(ctx); /* Mark the new start of the ExtGState dictionary loop */
            if (code < 0)
                goto error1;

            do {
                if (i++ >= pdfi_dict_entries(extgstate_dict)) {
                    code = 0;
                    goto transparency_exit;
                }

                code = pdfi_dict_next(ctx, extgstate_dict, &Key, &Value, &index);
                if (code == 0 && pdfi_type_of(Value) == PDF_DICT)
                    break;
                pdfi_countdown(Key);
                Key = NULL;
                pdfi_countdown(Value);
                Value = NULL;
            } while(1);
        }while (1);
    }
    return 0;

transparency_exit:
    pdfi_countdown(Key);
    pdfi_countdown(Value);

error1:
    (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the current resource loop */
    return code;
}

/*
 * This routine checks a Pattern dictionary to see if it contains any spot
 * colour definitions, or transparency usage (or blend modes other than Normal/Compatible).
 */
static int pdfi_check_Pattern(pdf_context *ctx, pdf_dict *pattern, pdf_dict *page_dict,
                              pdfi_check_tracker_t *tracker)
{
    int code = 0;
    pdf_obj *o = NULL;

    if (resource_is_checked(tracker, (pdf_obj *)pattern))
        return 0;

    if (pdfi_type_of(pattern) != PDF_DICT)
        return_error(gs_error_typecheck);

    if (tracker->spot_dict != NULL) {
        code = pdfi_dict_knownget(ctx, pattern, "Shading", &o);
        if (code > 0)
            (void)pdfi_check_Shading(ctx, o, page_dict, tracker);
        pdfi_countdown(o);
        o = NULL;
    }

    code = pdfi_dict_knownget_type(ctx, pattern, "Resources", PDF_DICT, &o);
    if (code > 0)
        (void)pdfi_check_Resources(ctx, (pdf_dict *)o, page_dict, tracker);
    pdfi_countdown(o);
    o = NULL;
    if (tracker->transparent == true && tracker->spot_dict == NULL)
        goto transparency_exit;

    code = pdfi_dict_knownget_type(ctx, pattern, "ExtGState", PDF_DICT, &o);
    if (code > 0)
        (void)pdfi_check_ExtGState(ctx, (pdf_dict *)o, page_dict, tracker);
    pdfi_countdown(o);
    o = NULL;

transparency_exit:
    return 0;
}

/*
 * This routine checks a Pattern dictionary for transparency.
 */
int pdfi_check_Pattern_transparency(pdf_context *ctx, pdf_dict *pattern, pdf_dict *page_dict,
                                    bool *transparent, bool *BM_Not_Normal)
{
    int code;
    pdfi_check_tracker_t tracker = {0, 0, 0, NULL, NULL, 0, NULL};

    /* NOTE: We use a "null" tracker that won't do any optimization to prevent
     * checking the same resource twice.
     * This means we don't get the optimization, but we also don't have the overhead
     * of setting it up.
     * If we do want the optimization, call pdfi_check_init_tracker() and
     * pdfi_check_free_tracker() as in pdfi_check_page(), below.
     */
    code = pdfi_check_Pattern(ctx, pattern, page_dict, &tracker);
    if (code == 0) {
        *transparent = tracker.transparent;
        *BM_Not_Normal = tracker.BM_Not_Normal;
    }
    else
        *transparent = false;
    return code;
}

/*
 * Check the Resources dictionary Pattern entry.
 */
static int pdfi_check_Pattern_dict(pdf_context *ctx, pdf_dict *pattern_dict, pdf_dict *page_dict,
                                   pdfi_check_tracker_t *tracker)
{
    int code;
    uint64_t i, index;
    pdf_obj *Key = NULL, *Value = NULL;
    pdf_dict *instance_dict = NULL;

    if (resource_is_checked(tracker, (pdf_obj *)pattern_dict))
        return 0;

    if (pdfi_type_of(pattern_dict) != PDF_DICT)
        return_error(gs_error_typecheck);

    if (pdfi_dict_entries(pattern_dict) > 0) {
        code = pdfi_loop_detector_mark(ctx); /* Mark the start of the Pattern dictionary loop */
        if (code < 0)
            return code;

        code = pdfi_dict_first(ctx, pattern_dict, &Key, &Value, &index);
        if (code < 0)
            goto error1;

        if (pdfi_type_of(Value) != PDF_DICT && pdfi_type_of(Value) != PDF_STREAM)
            goto transparency_exit;

        i = 1;
        do {
            code = pdfi_dict_from_obj(ctx, Value, &instance_dict);
            if (code < 0)
                goto transparency_exit;

            code = pdfi_check_Pattern(ctx, instance_dict, page_dict, tracker);
            if (code < 0)
                goto transparency_exit;

            pdfi_countdown(Key);
            Key = NULL;
            pdfi_countdown(Value);
            instance_dict = NULL;
            Value = NULL;
            (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the Shading dictionary loop */

            code = pdfi_loop_detector_mark(ctx); /* Mark the new start of the Shading dictionary loop */
            if (code < 0)
                goto error1;

            do {
                if (i++ >= pdfi_dict_entries(pattern_dict)) {
                    code = 0;
                    goto transparency_exit;
                }

                code = pdfi_dict_next(ctx, pattern_dict, &Key, &Value, &index);
                if (code == 0 && (pdfi_type_of(Value) == PDF_DICT || pdfi_type_of(Value) == PDF_STREAM))
                    break;
                pdfi_countdown(Key);
                Key = NULL;
                pdfi_countdown(Value);
                Value = NULL;
            } while(1);
        }while (1);
    }
    return 0;

transparency_exit:
    pdfi_countdown(Key);
    pdfi_countdown(Value);
    pdfi_countdown(instance_dict);

error1:
    (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the current resource loop */
    return code;
}

/*
 * This routine checks a Font dictionary to see if it contains any spot
 * colour definitions, or transparency usage. While we are here, if the tracker's font_array
 * is not NULL, pick up the font information and store it in the array.
 */
static int pdfi_check_Font(pdf_context *ctx, pdf_dict *font, pdf_dict *page_dict,
                           pdfi_check_tracker_t *tracker)
{
    int code = 0;
    pdf_obj *o = NULL;

    if (resource_is_checked(tracker, (pdf_obj *)font))
        return 0;

    if (pdfi_type_of(font) != PDF_DICT)
        return_error(gs_error_typecheck);

    if (tracker->font_array != NULL) {
        /* If we get to here this is a font we have not seen before. We need
         * to make a new font array big enough to hold the existing entries +1
         * copy the existing entries to the new array and free the old array.
         * Finally create a dictionary with all the font information we want
         * and add it to the array.
         */
        pdf_array *new_fonts = NULL;
        int index = 0;
        pdf_obj *array_obj = NULL;
        pdf_dict *font_info_dict = NULL;

        /* Let's start by gathering the information we need and storing it in a dictionary */
        code = pdfi_dict_alloc(ctx, 4, &font_info_dict);
        if (code < 0)
            return code;
        pdfi_countup(font_info_dict);

        if (font->object_num != 0) {
            pdf_num *int_obj = NULL;

            code = pdfi_object_alloc(ctx, PDF_INT, 0, (pdf_obj **)&int_obj);
            if (code >= 0) {
                pdfi_countup(int_obj);
                int_obj->value.i = font->object_num;
                code = pdfi_dict_put(ctx, font_info_dict, "ObjectNum", (pdf_obj *)int_obj);
                pdfi_countdown(int_obj);
            }
            if (code < 0) {
                pdfi_countdown(font_info_dict);
                return code;
            }
        }

        code = pdfi_dict_get(ctx, font, "BaseFont", &array_obj);
        if (code >= 0) {
            code = pdfi_dict_put(ctx, font_info_dict, "BaseFont", array_obj);
            if (code < 0) {
                pdfi_countdown(array_obj);
                pdfi_countdown(font_info_dict);
                return code;
            }
        }
        pdfi_countdown(array_obj);
        array_obj = NULL;

        code = pdfi_dict_get(ctx, font, "ToUnicode", &array_obj);
        if (code >= 0)
            code = pdfi_dict_put(ctx, font_info_dict, "ToUnicode", PDF_TRUE_OBJ);
        else
            code = pdfi_dict_put(ctx, font_info_dict, "ToUnicode", PDF_FALSE_OBJ);
        pdfi_countdown(array_obj);
        array_obj = NULL;
        if (code < 0)
            return code;

        code = pdfi_dict_get(ctx, font, "FontDescriptor", &array_obj);
        if (code >= 0) {
            bool known = false;

            (void)pdfi_dict_known(ctx, (pdf_dict *)array_obj, "FontFile", &known);
            if (!known) {
                (void)pdfi_dict_known(ctx, (pdf_dict *)array_obj, "FontFile2", &known);
                if (!known) {
                    (void)pdfi_dict_known(ctx, (pdf_dict *)array_obj, "FontFile3", &known);
                }
            }

            if (known > 0)
                code = pdfi_dict_put(ctx, font_info_dict, "Embedded", PDF_TRUE_OBJ);
            else
                code = pdfi_dict_put(ctx, font_info_dict, "Embedded", PDF_FALSE_OBJ);
        } else
            code = pdfi_dict_put(ctx, font_info_dict, "Embedded", PDF_FALSE_OBJ);

        pdfi_countdown(array_obj);
        array_obj = NULL;

        if (code < 0)
            return code;


        code = pdfi_dict_knownget_type(ctx, font, "Subtype", PDF_NAME, &array_obj);
        if (code > 0) {
            code = pdfi_dict_put(ctx, font_info_dict, "Subtype", array_obj);
            if (code < 0) {
                pdfi_countdown(array_obj);
                pdfi_countdown(font_info_dict);
                return code;
            }

            if (pdfi_name_is((pdf_name *)array_obj, "Type3")) {
                pdfi_countdown(o);
                o = NULL;

                code = pdfi_dict_knownget_type(ctx, font, "Resources", PDF_DICT, &o);
                if (code > 0)
                    (void)pdfi_check_Resources(ctx, (pdf_dict *)o, page_dict, tracker);
            }

            if (pdfi_name_is((const pdf_name *)array_obj, "Type0")){
                pdf_array *descendants = NULL;
                pdf_dict *desc_font = NULL;

                code = pdfi_dict_get(ctx, font, "DescendantFonts", (pdf_obj **)&descendants);
                if (code >= 0) {
                    code = pdfi_array_get(ctx, descendants, 0, (pdf_obj **)&desc_font);
                    if (code >= 0){
                        pdf_array *desc_array = NULL;

                        code = pdfi_array_alloc(ctx, 0, &desc_array);
                        pdfi_countup(desc_array);
                        if (code >= 0) {
                            pdf_array *saved = tracker->font_array;

                            tracker->font_array = desc_array;
                            (void)pdfi_check_Font(ctx, desc_font, page_dict, tracker);
                            (void)pdfi_dict_put(ctx, font_info_dict, "Descendants", (pdf_obj *)tracker->font_array);
                            pdfi_countdown((pdf_obj *)tracker->font_array);
                            tracker->font_array = saved;
                        }
                        pdfi_countdown(descendants);
                        pdfi_countdown(desc_font);
                    }
                }
            }
        }
        pdfi_countdown(array_obj);
        array_obj = NULL;

        code = pdfi_array_alloc(ctx, pdfi_array_size(tracker->font_array) + 1, &new_fonts);
        if (code < 0) {
            pdfi_countdown(font_info_dict);
            return code;
        }
        pdfi_countup(new_fonts);

        for (index = 0; index < pdfi_array_size(tracker->font_array); index++) {
            code = pdfi_array_get(ctx, tracker->font_array, index, &array_obj);
            if (code < 0) {
                pdfi_countdown(font_info_dict);
                pdfi_countdown(new_fonts);
                return code;
            }
            code = pdfi_array_put(ctx, new_fonts, index, array_obj);
            pdfi_countdown(array_obj);
            if (code < 0) {
                pdfi_countdown(font_info_dict);
                pdfi_countdown(new_fonts);
                return code;
            }
        }
        code = pdfi_array_put(ctx, new_fonts, index, (pdf_obj *)font_info_dict);
        if (code < 0) {
            pdfi_countdown(font_info_dict);
            pdfi_countdown(new_fonts);
            return code;
        }
        pdfi_countdown(font_info_dict);
        pdfi_countdown(tracker->font_array);
        tracker->font_array = new_fonts;
    } else {
        code = pdfi_dict_knownget_type(ctx, font, "Subtype", PDF_NAME, &o);
        if (code > 0) {
            if (pdfi_name_is((pdf_name *)o, "Type3")) {
                pdfi_countdown(o);
                o = NULL;

                code = pdfi_dict_knownget_type(ctx, font, "Resources", PDF_DICT, &o);
                if (code > 0)
                    (void)pdfi_check_Resources(ctx, (pdf_dict *)o, page_dict, tracker);
            }
        }

        pdfi_countdown(o);
        o = NULL;
    }

    return 0;
}

/*
 * Check the Resources dictionary Font entry.
 */
static int pdfi_check_Font_dict(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *page_dict,
                                pdfi_check_tracker_t *tracker)
{
    int code = 0;
    uint64_t i, index;
    pdf_obj *Key = NULL, *Value = NULL;

    if (resource_is_checked(tracker, (pdf_obj *)font_dict))
        return 0;

    if (pdfi_type_of(font_dict) != PDF_DICT)
        return_error(gs_error_typecheck);

    if (pdfi_dict_entries(font_dict) > 0) {
        code = pdfi_loop_detector_mark(ctx); /* Mark the start of the Font dictionary loop */
        if (code < 0)
            return code;

        code = pdfi_dict_first(ctx, font_dict, &Key, &Value, &index);
        if (code < 0) {
            (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the current resource loop */
            goto error1;
        }

        i = 1;
        do {
            if (pdfi_type_of(Value) == PDF_DICT)
                code = pdfi_check_Font(ctx, (pdf_dict *)Value, page_dict, tracker);
            else if (pdfi_type_of(Value) == PDF_FONT) {
                pdf_dict *d = ((pdf_font *)Value)->PDF_font;

                code = pdfi_check_Font(ctx, d, page_dict, tracker);
            } else {
                pdfi_set_warning(ctx, 0, NULL, W_PDF_FONTRESOURCE_TYPE, "pdfi_check_Font_dict", "");
            }
            if (code < 0)
                break;

            pdfi_countdown(Key);
            Key = NULL;
            pdfi_countdown(Value);
            Value = NULL;

            (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the Font dictionary loop */

            code = pdfi_loop_detector_mark(ctx); /* Mark the new start of the Font dictionary loop */
            if (code < 0)
                goto error1;

            if (i++ >= pdfi_dict_entries(font_dict)) {
                code = 0;
                break;
            }

            code = pdfi_dict_next(ctx, font_dict, &Key, &Value, &index);
            if (code < 0)
                break;
        }while (1);

        (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the current resource loop */
    }

    pdfi_countdown(Key);
    pdfi_countdown(Value);

error1:
    return code;
}

static int pdfi_check_Resources(pdf_context *ctx, pdf_dict *Resources_dict,
                                pdf_dict *page_dict, pdfi_check_tracker_t *tracker)
{
    int code;
    pdf_obj *d = NULL;

    if (resource_is_checked(tracker, (pdf_obj *)Resources_dict))
        return 0;

    if (pdfi_type_of(Resources_dict) != PDF_DICT)
        return_error(gs_error_typecheck);

    /* First up, check any colour spaces, for new spot colours.
     * We only do this if asked because its expensive. spot_dict being NULL
     * means we aren't interested in spot colours (not a DeviceN or Separation device)
     */
    if (tracker->spot_dict != NULL) {
        code = pdfi_dict_knownget_type(ctx, Resources_dict, "ColorSpace", PDF_DICT, &d);
        if (code < 0) {
            if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_SPOT_CHK_BADTYPE, "pdfi_check_Resources", "")) < 0)
                return code;
        }
        (void)pdfi_check_ColorSpace_dict(ctx, (pdf_dict *)d, page_dict, tracker);

        pdfi_countdown(d);
        d = NULL;

        code = pdfi_dict_knownget_type(ctx, Resources_dict, "Shading", PDF_DICT, &d);
        if (code < 0) {
            if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_SPOT_CHK_BADTYPE, "pdfi_check_Resources", "")) < 0)
                return code;
        }
        (void)pdfi_check_Shading_dict(ctx, (pdf_dict *)d, page_dict, tracker);
        pdfi_countdown(d);
        d = NULL;
    }

    code = pdfi_dict_knownget_type(ctx, Resources_dict, "XObject", PDF_DICT, &d);
    if (code < 0) {
        if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_TRANS_CHK_BADTYPE, "pdfi_check_Resources", "")) < 0)
            return code;
    }
    (void)pdfi_check_XObject_dict(ctx, (pdf_dict *)d, page_dict, tracker);
    pdfi_countdown(d);
    d = NULL;

    code = pdfi_dict_knownget_type(ctx, Resources_dict, "Pattern", PDF_DICT, &d);
    if (code < 0) {
        if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_TRANS_CHK_BADTYPE, "pdfi_check_Resources", "")) < 0) {
            return code;
        }
    }
    (void)pdfi_check_Pattern_dict(ctx, (pdf_dict *)d, page_dict, tracker);
    pdfi_countdown(d);
    d = NULL;

    code = pdfi_dict_knownget_type(ctx, Resources_dict, "Font", PDF_DICT, &d);
    if (code < 0) {
        if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_TRANS_CHK_BADTYPE, "pdfi_check_Resources", "")) < 0)
            return code;
    }
    (void)pdfi_check_Font_dict(ctx, (pdf_dict *)d, page_dict, tracker);
    /* From this point onwards, if we detect transparency (or have already detected it) we
     * can exit, we have already counted up any spot colours.
     */
    pdfi_countdown(d);
    d = NULL;

    code = pdfi_dict_knownget_type(ctx, Resources_dict, "ExtGState", PDF_DICT, &d);
    if (code < 0) {
        if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_TRANS_CHK_BADTYPE, "pdfi_check_Resources", "")) < 0)
            return code;
    }
    (void)pdfi_check_ExtGState_dict(ctx, (pdf_dict *)d, page_dict, tracker);
    pdfi_countdown(d);
    d = NULL;

    return 0;
}

static int pdfi_check_annot_for_transparency(pdf_context *ctx, pdf_dict *annot, pdf_dict *page_dict,
                                             pdfi_check_tracker_t *tracker)
{
    int code;
    pdf_name *n;
    pdf_obj *N = NULL;
    pdf_dict *ap = NULL;
    pdf_dict *Resources = NULL;
    double f;

    if (resource_is_checked(tracker, (pdf_obj *)annot))
        return 0;

    if (pdfi_type_of(annot) != PDF_DICT)
        return_error(gs_error_typecheck);

    /* Check #1 Does the (Normal) Appearnce stream use any Resources which include transparency.
     * We check this first, because this also checks for spot colour spaces. Once we've done that we
     * can exit the checks as soon as we detect transparency.
     */
    code = pdfi_dict_knownget_type(ctx, annot, "AP", PDF_DICT, (pdf_obj **)&ap);
    if (code > 0)
    {
        /* Fetch without resolving indirect ref because pdfmark wants it that way later */
        code = pdfi_dict_get_no_store_R(ctx, ap, "N", (pdf_obj **)&N);
        if (code >= 0) {
            pdf_dict *dict = NULL;

            code = pdfi_dict_from_obj(ctx, N, &dict);
            if (code == 0)
                code = pdfi_dict_knownget_type(ctx, dict, "Resources", PDF_DICT, (pdf_obj **)&Resources);
            if (code > 0)
                code = pdfi_check_Resources(ctx, (pdf_dict *)Resources, page_dict, tracker);
        }
        if (code == gs_error_undefined)
            code = 0;
    }
    pdfi_countdown(ap);
    pdfi_countdown(N);
    pdfi_countdown(Resources);

    if (code < 0)
        return code;
    /* We've checked the Resources, and nothing else in an annotation can define spot colours, so
     * if we detected transparency in the Resources we need not check further.
     */
    if (tracker->transparent == true)
        return 0;

    code = pdfi_dict_get_type(ctx, annot, "Subtype", PDF_NAME, (pdf_obj **)&n);
    if (code < 0) {
        if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_BAD_TYPE, "pdfi_check_annot_for_transparency", "")) < 0) {
            return code;
        }
    } else {
        /* Check #2, Highlight annotations are always preformed with transparency */
        if (pdfi_name_is((const pdf_name *)n, "Highlight")) {
            pdfi_countdown(n);
            tracker->transparent = true;
            return 0;
        }
        pdfi_countdown(n);
        n = NULL;

        /* Check #3 Blend Mode (BM) not being 'Normal' or 'Compatible' */
        code = pdfi_dict_knownget_type(ctx, annot, "BM", PDF_NAME, (pdf_obj **)&n);
        if (code > 0) {
            if (!pdfi_name_is((const pdf_name *)n, "Normal")) {
                if (!pdfi_name_is((const pdf_name *)n, "Compatible")) {
                    pdfi_countdown(n);
                    tracker->transparent = true;
                    return 0;
                }
            }
            code = 0;
        }
        pdfi_countdown(n);
        if (code < 0)
            return code;

        /* Check #4 stroke constant alpha (CA) is not 1 (100% opaque) */
        code = pdfi_dict_knownget_number(ctx, annot, "CA", &f);
        if (code > 0) {
            if (f != 1.0) {
                tracker->transparent = true;
                return 0;
            }
        }
        if (code < 0)
            return code;

        /* Check #5 non-stroke constant alpha (ca) is not 1 (100% opaque) */
        code = pdfi_dict_knownget_number(ctx, annot, "ca", &f);
        if (code > 0) {
            if (f != 1.0) {
                tracker->transparent = true;
                return 0;
            }
        }
        if (code < 0)
            return code;
    }

    return 0;
}

static int pdfi_check_Annots_for_transparency(pdf_context *ctx, pdf_array *annots_array,
                                              pdf_dict *page_dict, pdfi_check_tracker_t *tracker)
{
    int i, code = 0;
    pdf_dict *annot = NULL;

    if (resource_is_checked(tracker, (pdf_obj *)annots_array))
        return 0;

    if (pdfi_type_of(annots_array) != PDF_ARRAY)
        return_error(gs_error_typecheck);

    for (i=0; i < pdfi_array_size(annots_array); i++) {
        code = pdfi_array_get_type(ctx, annots_array, (uint64_t)i, PDF_DICT, (pdf_obj **)&annot);
        if (code >= 0) {
            code = pdfi_check_annot_for_transparency(ctx, annot, page_dict, tracker);
            if (code < 0 && (code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_INVALID_TRANS_XOBJECT, "pdfi_check_Annots_for_transparency", "")) < 0) {
                goto exit;
            }

            /* If we've found transparency, and don't need to continue checkign for spot colours
             * just exit as fast as possible.
             */
            if (tracker->transparent == true && tracker->spot_dict == NULL)
                goto exit;

            pdfi_countdown(annot);
            annot = NULL;
        }
        else if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_INVALID_TRANS_XOBJECT, "pdfi_check_Annots_for_transparency", "")) < 0) {
            goto exit;
        }
    }
exit:
    pdfi_countdown(annot);
    return code;
}

/* Check for transparency and spots on page.
 *
 * Sets ctx->spot_capable_device
 * Builds a dictionary of the unique spot names in spot_dict
 * Set 'transparent' to true if there is transparency on the page
 *
 * From the original PDF interpreter written in PostScript:
 * Note: we deliberately don't check to see whether a Group is defined,
 * because Adobe Illustrator 10 (and possibly other applications) define
 * a page-level group whether transparency is actually used or not.
 * Ignoring the presence of Group is justified because, in the absence
 * of any other transparency features, they have no effect.
 */
static int pdfi_check_page_inner(pdf_context *ctx, pdf_dict *page_dict,
                                 pdfi_check_tracker_t *tracker)
{
    int code;
    pdf_dict *Resources = NULL;
    pdf_array *Annots = NULL;
    pdf_dict *Group = NULL;
    pdf_obj *CS = NULL;

    tracker->transparent = false;

    if (pdfi_type_of(page_dict) != PDF_DICT)
        return_error(gs_error_typecheck);


    /* Check if the page dictionary has a page Group entry (for spots).
     * Page group should mean the page has transparency but we ignore it for the purposes
     * of transparency detection. See above.
     */
    if (tracker->spot_dict) {
        code = pdfi_dict_knownget_type(ctx, page_dict, "Group", PDF_DICT, (pdf_obj **)&Group);
        if (code > 0) {
            /* If Group has a ColorSpace (CS), then check it for spot colours */
            code = pdfi_dict_knownget(ctx, Group, "CS", &CS);
            if (code > 0)
                code = pdfi_check_ColorSpace_for_spots(ctx, CS, Group, page_dict, tracker->spot_dict);

            if (code < 0) {
                if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_GS_LIB_ERROR, "pdfi_check_page_inner", "")) < 0) {
                    goto exit;
                }
            }
        }
    }

    /* Now check any Resources dictionary in the Page dictionary */
    code = pdfi_dict_knownget_type(ctx, page_dict, "Resources", PDF_DICT, (pdf_obj **)&Resources);
    if (code > 0)
        code = pdfi_check_Resources(ctx, Resources, page_dict, tracker);

    if (code == gs_error_pdf_stackoverflow || (code < 0 &&
       (code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_GS_LIB_ERROR, "pdfi_check_page_inner", "")) < 0)) {
        goto exit;
    }

    /* If we are drawing Annotations, check to see if the page uses any Annots */
    if (ctx->args.showannots) {
        code = pdfi_dict_knownget_type(ctx, page_dict, "Annots", PDF_ARRAY, (pdf_obj **)&Annots);
        if (code > 0)
            code = pdfi_check_Annots_for_transparency(ctx, Annots, page_dict,
                                                      tracker);
        if (code < 0) {
            if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_GS_LIB_ERROR, "pdfi_check_page_inner", "")) < 0) {
               goto exit;
            }
        }
    }

    code = 0;
 exit:
    pdfi_countdown(Resources);
    pdfi_countdown(Annots);
    pdfi_countdown(CS);
    pdfi_countdown(Group);
    return code;
}

/* Checks page for transparency, and sets up device for spots, if applicable
 * Sets ctx->page.has_transparency and ctx->page.num_spots
 * do_setup -- indicates whether to actually set up the device with the spot count.
 */
int pdfi_check_page(pdf_context *ctx, pdf_dict *page_dict, pdf_array **fonts_array, pdf_array **spots_array, bool do_setup)
{
    int code;
    int spots = 0;
    pdfi_check_tracker_t tracker;

    ctx->page.num_spots = 0;
    ctx->page.has_transparency = false;

    code = pdfi_check_init_tracker(ctx, &tracker, fonts_array, spots_array);
    if (code < 0)
        goto exit;

    /* Check for spots and transparency in this page */
    code = pdfi_check_page_inner(ctx, page_dict, &tracker);
    if (code < 0)
        goto exit;

    /* Count the spots */
    if (tracker.spot_dict)
        spots = pdfi_dict_entries(tracker.spot_dict);

    /* If setup requested, tell the device about spots and transparency */
    if (do_setup) {
        gs_c_param_list list;
        int a = 0;
        pdf_name *Key = NULL;
        pdf_obj *Value = NULL;
        uint64_t index = 0;

        gs_c_param_list_write(&list, ctx->memory);

        /* If there are spot colours (and by inference, the device renders spot plates) then
         * send the number of Spots to the device, so it can setup correctly.
         */
        if (tracker.spot_dict) {
            /* There is some awkwardness here. If the SeparationColorNames setting
             * fails, we want to ignore it (this can mean that we exceeded the maximum
             * number of colourants and some will be converted to CMYK). But if that happens,
             * any other parameters in the same list which haven't already been prcoessed
             * will be lost. So we need to send two lists, the SeparationColorNames and
             * 'everything else'.
             */
            if (spots > 0) {
                gs_param_string_array sa;
                gs_param_string *table = NULL;

                table = (gs_param_string *)gs_alloc_byte_array(ctx->memory, spots, sizeof(gs_param_string), "SeparationNames");
                if (table != NULL)
                {
                    memset(table, 0x00, spots * sizeof(gs_param_string));

                    code = pdfi_dict_first(ctx, tracker.spot_dict, (pdf_obj **)&Key, &Value, &index);
                    while (code >= 0)
                    {
                        if (pdfi_type_of(Key) == PDF_NAME) {
                            table[a].data = ((pdf_string *)Key)->data;
                            table[a].size = ((pdf_string *)Key)->length;
                            table[a++].persistent = false;
                        }
                        /* Although we count down the returned PDF objects here, the pointers
                         * to the name data remain valid and won't move. Provided we don't
                         * retain the pointers after we free the tracker dictionary this is
                         * safe to do.
                         */
                        pdfi_countdown(Key);
                        Key = NULL;
                        pdfi_countdown(Value);
                        Value = NULL;
                        code = pdfi_dict_next(ctx, tracker.spot_dict, (pdf_obj **)&Key, &Value, &index);
                    }
                    sa.data = table;
                    sa.size = spots;
                    sa.persistent = false;

                    (void)param_write_string_array((gs_param_list *)&list, "SeparationColorNames", &sa);
                    gs_c_param_list_read(&list);
                    code = gs_putdeviceparams(ctx->pgs->device, (gs_param_list *)&list);
                    gs_c_param_list_release(&list);

                    gs_free_object(ctx->memory, table, "SeparationNames");
                    if (code > 0) {
                        /* The device was closed, we need to reopen it */
                        code = gs_setdevice_no_erase(ctx->pgs, ctx->pgs->device);
                        if (code < 0)
                            goto exit;
                        gs_erasepage(ctx->pgs);
                    }

                    /* Reset the list back to being writeable */
                    gs_c_param_list_write(&list, ctx->memory);
                }
                else {
                    code = gs_note_error(gs_error_VMerror);
                    goto exit;
                }
            }
            /* Update the number of spots */
            param_write_int((gs_param_list *)&list, "PageSpotColors", &spots);
        }
        /* Update the page transparency */
        (void)param_write_bool((gs_param_list *)&list, "PageUsesTransparency",
                                    &tracker.transparent);
        gs_c_param_list_read(&list);
        code = gs_putdeviceparams(ctx->pgs->device, (gs_param_list *)&list);
        gs_c_param_list_release(&list);

        if (code > 0) {
            /* The device was closed, we need to reopen it */
            code = gs_setdevice_no_erase(ctx->pgs, ctx->pgs->device);
            if (code < 0)
                goto exit;
            gs_erasepage(ctx->pgs);
        }
    }

    /* Set our values in the context, for caller */
    if (!ctx->args.notransparency)
        ctx->page.has_transparency = tracker.transparent;
    ctx->page.num_spots = spots;
    ctx->page.has_OP = tracker.has_overprint;

    /* High level devices do not render overprint */
    if (ctx->device_state.HighLevelDevice)
        ctx->page.has_OP = false;

 exit:
    if (fonts_array != NULL) {
        *fonts_array = tracker.font_array;
        pdfi_countup(*fonts_array);
    }

    if (spots_array != NULL && tracker.spot_dict != NULL && pdfi_dict_entries(tracker.spot_dict) != 0) {
        pdf_array *new_array = NULL;
        pdf_name *Key = NULL;
        pdf_obj *Value = NULL;
        uint64_t index = 0, a_index = 0;

        index = pdfi_dict_entries(tracker.spot_dict);

        code = pdfi_array_alloc(ctx, index, &new_array);
        if (code < 0)
            goto error;
        pdfi_countup(new_array);

        code = pdfi_dict_first(ctx, tracker.spot_dict, (pdf_obj **)&Key, &Value, &index);
        while (code >= 0)
        {
            if (pdfi_type_of(Key) == PDF_NAME) {
                code = pdfi_array_put(ctx, new_array, a_index++, (pdf_obj *)Key);
                if (code < 0) {
                    pdfi_countdown(new_array);
                    pdfi_countdown(Key);
                    pdfi_countdown(Value);
                    goto error;
                }
            }

            pdfi_countdown(Key);
            Key = NULL;
            pdfi_countdown(Value);
            Value = NULL;
            code = pdfi_dict_next(ctx, tracker.spot_dict, (pdf_obj **)&Key, &Value, &index);
        }
        code = 0;
        *spots_array = new_array;
    }
error:
    (void)pdfi_check_free_tracker(ctx, &tracker);
    return code;
}
