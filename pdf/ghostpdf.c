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

/* Top level PDF access routines */
#include "ghostpdf.h"
#include "plmain.h"
#include "pdf_types.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_file.h"
#include "pdf_loop_detect.h"
#include "pdf_trans.h"
#include "stream.h"
#include "strmio.h"
#include "pdf_colour.h"

/* This routine is slightly misnamed, as it also checks ColorSpaces for spot colours.
 * This is done at the page level, so we maintain a dictionary of the spot colours
 * encountered so far, which we consult before adding any new ones.
 */
static int pdfi_check_Resources_for_transparency(pdf_context *ctx, pdf_dict *Resources_dict, pdf_dict *page_dict, bool *transparent, int *num_spots);

static int pdfi_process_page_contents(pdf_context *ctx, pdf_dict *page_dict)
{
    int i, code = 0;
    pdf_obj *o, *o1;

    code = pdfi_dict_get(ctx, page_dict, "Contents", &o);
    if (code == gs_error_undefined)
        /* Don't throw an error if there are no contents, just render nothing.... */
        return 0;
    if (code < 0)
        return code;

    if (o->type == PDF_INDIRECT) {
        if (((pdf_indirect_ref *)o)->ref_object_num == page_dict->object_num)
            return_error(gs_error_circular_reference);

        code = pdfi_dereference(ctx, ((pdf_indirect_ref *)o)->ref_object_num, ((pdf_indirect_ref *)o)->ref_generation_num, &o1);
        pdfi_countdown(o);
        if (code < 0) {
            if (code == gs_error_VMerror)
                return code;
            return 0;
        }
        o = o1;
    }

    if (o->type == PDF_ARRAY) {
        pdf_array *a = (pdf_array *)o;

        for (i=0;i < a->size; i++) {
            pdf_indirect_ref *r;
            code = pdfi_array_get(a, i, (pdf_obj **)&r);
            if (code < 0) {
                pdfi_countdown(o);
                return code;
            }
            if (r->type == PDF_DICT) {
                code = pdfi_interpret_content_stream(ctx, (pdf_dict *)r, page_dict);
                pdfi_countdown(r);
                if (code < 0) {
                    pdfi_countdown(o);
                    return(code);
                }
            } else {
                if (r->type != PDF_INDIRECT) {
                    pdfi_countdown(o);
                    pdfi_countdown(r);
                    return_error(gs_error_typecheck);
                } else {
                    if (r->ref_object_num == page_dict->object_num) {
                        pdfi_countdown(o);
                        pdfi_countdown(r);
                        return_error(gs_error_circular_reference);
                    }
                    code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, &o1);
                    pdfi_countdown(r);
                    if (code < 0) {
                        pdfi_countdown(o);
                        if (code == gs_error_VMerror || ctx->pdfstoponerror)
                            return code;
                        else
                            return 0;
                    }
                    if (o1->type != PDF_DICT) {
                        pdfi_countdown(o);
                        return_error(gs_error_typecheck);
                    }
                    code = pdfi_interpret_content_stream(ctx, (pdf_dict *)o1, page_dict);
                    pdfi_countdown(o1);
                    if (code < 0) {
                        if (code == gs_error_VMerror || ctx->pdfstoponerror == true) {
                            pdfi_countdown(o);
                            return code;
                        }
                    }
                }
            }
        }
    } else {
        if (o->type == PDF_DICT) {
            code = pdfi_interpret_content_stream(ctx, (pdf_dict *)o, page_dict);
        } else {
            pdfi_countdown(o);
            return_error(gs_error_typecheck);
        }
    }
    pdfi_countdown(o);
    return code;
}

static int pdfi_check_ColorSpace_dict(pdf_context *ctx, pdf_dict *cspace_dict, pdf_dict *page_dict, int *num_spots)
{
    int code, i, index;
    pdf_obj *Key = NULL, *Value = NULL;

    if (cspace_dict->entries > 0) {
        code = pdfi_loop_detector_mark(ctx); /* Mark the start of the ColorSpace dictionary loop */
        if (code < 0)
            return code;

        code = pdfi_dict_first(ctx, (pdf_dict *)cspace_dict, &Key, &Value, (void *)&index);
        if (code < 0)
            goto error1;

        i = 1;
        do {
            code = pdfi_check_ColorSpace_for_spots(ctx, Value, cspace_dict, page_dict, num_spots);
            if (code < 0) {
                goto error2;
            } else {
                pdfi_countdown(Key);
                Key = NULL;
                pdfi_countdown(Value);
                Value = NULL;
            }
            (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the Shading dictionary loop */

            code = pdfi_loop_detector_mark(ctx); /* Mark the new start of the Shading dictionary loop */
            if (code < 0)
                goto error1;

            do {
                if (i++ >= pdfi_dict_entries((pdf_dict *)cspace_dict)) {
                    code = 0;
                    goto transparency_exit;
                }

                code = pdfi_dict_next(ctx, (pdf_dict *)cspace_dict, &Key, &Value, (void *)&index);
                if (code == 0 && Value->type == PDF_DICT)
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

static int pdfi_check_Shading_dict(pdf_context *ctx, pdf_dict *shading_dict, pdf_dict *page_dict, int *num_spots)
{
    int code, i, index;
    pdf_obj *Key = NULL, *Value = NULL, *o = NULL;

    if (shading_dict->entries > 0) {
        code = pdfi_loop_detector_mark(ctx); /* Mark the start of the Shading dictionary loop */
        if (code < 0)
            return code;

        code = pdfi_dict_first(ctx, (pdf_dict *)shading_dict, &Key, &Value, (void *)&index);
        if (code < 0 || Value->type != PDF_DICT)
            goto error1;

        i = 1;
        do {
            code = pdfi_dict_knownget(ctx, (pdf_dict *)Value, "ColorSpace", (pdf_obj **)&o);
            if (code > 0) {
                code = pdfi_check_ColorSpace_for_spots(ctx, o, (pdf_dict *)Value, page_dict, num_spots);
                if (code < 0) {
                    pdfi_countdown(o);
                    goto error2;
                } else {
                    pdfi_countdown(Key);
                    Key = NULL;
                    pdfi_countdown(Value);
                    Value = NULL;
                    pdfi_countdown(o);
                    o = NULL;
                }
            }

            (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the Shading dictionary loop */

            code = pdfi_loop_detector_mark(ctx); /* Mark the new start of the Shading dictionary loop */
            if (code < 0)
                goto error1;

            do {
                if (i++ >= pdfi_dict_entries((pdf_dict *)shading_dict)) {
                    code = 0;
                    goto transparency_exit;
                }

                code = pdfi_dict_next(ctx, (pdf_dict *)shading_dict, &Key, &Value, (void *)&index);
                if (code == 0 && Value->type == PDF_DICT)
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

static int pdfi_check_XObject_dict(pdf_context *ctx, pdf_dict *xobject_dict, pdf_dict *page_dict, int *transparent, int *num_spots)
{
    int code, i, index;
    pdf_obj *Key = NULL, *Value = NULL, *o = NULL;
    pdf_name *n;
    bool known = false;
    double f;

    if (xobject_dict->entries > 0) {
        code = pdfi_loop_detector_mark(ctx); /* Mark the start of the XObject dictionary loop */
        if (code < 0)
            return code;

        code = pdfi_dict_first(ctx, (pdf_dict *)xobject_dict, &Key, &Value, (void *)&index);
        if (code < 0 || Value->type != PDF_DICT)
            goto error1;

        i = 1;
        do {
            code = pdfi_dict_get_type(ctx, (pdf_dict *)Value, "Subtype", PDF_NAME, (pdf_obj **)&n);
            if (code >= 0) {
                if (pdfi_name_strcmp((const pdf_name *)n, "Image") == 0) {
                    pdfi_countdown(n);
                    n = NULL;
                    code = pdfi_dict_known((pdf_dict *)Value, "SMask", &known);
                    if (code >= 0) {
                        if (known == true) {
                            *transparent = true;
                            if (num_spots == NULL)
                                goto transparency_exit;
                        }
                        code = pdfi_dict_knownget_number(ctx, (pdf_dict *)Value, "SMaskInData", &f);
                        if (code > 0) {
                            code = 0;
                            if (f != 0.0)
                                *transparent = true;
                            if (num_spots == NULL)
                                goto transparency_exit;
                        }
                        /* Check the image dictionary for a ColorSpace entry, if we are checkign spot names */
                        if (num_spots) {
                            code = pdfi_dict_knownget(ctx, (pdf_dict *)Value, "ColorSpace", (pdf_obj **)&o);
                            if (code > 0) {
                                /* We don't care if there's an error here, it'll be picked up if we use the ColorSpace later */
                                (void)pdfi_check_ColorSpace_for_spots(ctx, o, (pdf_dict *)Value, page_dict, num_spots);
                                pdfi_countdown(o);
                            }
                        }
                    }
                } else {
                    if (pdfi_name_strcmp((const pdf_name *)n, "Form") == 0) {
                        pdf_dict *group_dict = NULL, *resource_dict = NULL;
                        pdf_obj *CS = NULL;

                        code = pdfi_dict_knownget_type(ctx, (pdf_dict *)Value, "Group", PDF_DICT, (pdf_obj **)&group_dict);
                        if (code > 0) {
                            *transparent = true;
                            if (num_spots = NULL) {
                                pdfi_countdown(group_dict);
                                pdfi_countdown(n);
                                goto transparency_exit;
                            }

                            /* Start a new loop detector group to avoid this being detected in the Resources check below */
                            code = pdfi_loop_detector_mark(ctx); /* Mark the start of the XObject dictionary loop */
                            if (code > 0) {
                                code = pdfi_dict_knownget(ctx, group_dict, "CS", &CS);
                                if (code > 0)
                                    /* We don't care if there's an error here, it'll be picked up if we use the ColorSpace later */
                                    (void)pdfi_check_ColorSpace_for_spots(ctx, CS, group_dict, page_dict, num_spots);
                            }
                            pdfi_countdown(CS);
                            (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the XObject dictionary loop */
                        }

                        code = pdfi_dict_knownget_type(ctx, (pdf_dict *)Value, "Resources", PDF_DICT, (pdf_obj **)&resource_dict);
                        if (code > 0) {
                            code = pdfi_check_Resources_for_transparency(ctx, resource_dict, page_dict, transparent, num_spots);
                            pdfi_countdown(resource_dict);
                            if (code < 0)
                                goto error2;
                        }
                    }
                    pdfi_countdown(n);
                }
                pdfi_countdown(Key);
                Key = NULL;
                pdfi_countdown(Value);
                Value = NULL;

                (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the XObject dictionary loop */

                code = pdfi_loop_detector_mark(ctx); /* Mark the new start of the XObject dictionary loop */
                if (code < 0)
                    goto error1;

                do {
                    if (i++ >= pdfi_dict_entries((pdf_dict *)xobject_dict)) {
                        code = 0;
                        goto transparency_exit;
                    }

                    code = pdfi_dict_next(ctx, (pdf_dict *)xobject_dict, &Key, &Value, (void *)&index);
                    if (code == 0 && Value->type == PDF_DICT)
                        break;
                    pdfi_countdown(Key);
                    Key = NULL;
                    pdfi_countdown(Value);
                    Value = NULL;
                } while(1);
            }
        }while(1);
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

static int pdfi_check_ExtGState(pdf_context *ctx, pdf_dict *extgstate_dict, int *transparent)
{
    int code;
    pdf_obj *o = NULL;
    double f;

    if (extgstate_dict->entries > 0) {
        code = pdfi_dict_knownget_type(ctx, extgstate_dict, "BM", PDF_NAME, &o);
        if (code > 0) {
            if (pdfi_name_strcmp((pdf_name *)o, "Normal") != 0) {
                if (pdfi_name_strcmp((pdf_name *)o, "Compatible") != 0) {
                    pdfi_countdown(o);
                    *transparent = true;
                    return 0;
                }
            }
        }
        pdfi_countdown(o);
        o = NULL;

        code = pdfi_dict_knownget(ctx, extgstate_dict, "SMask", &o);
        if (code > 0) {
            if (o->type == PDF_NAME) {
                if (pdfi_name_strcmp((pdf_name *)o, "None") != 0) {
                    pdfi_countdown(o);
                    *transparent = true;
                    return 0;
                }
            } else {
                if (o->type == PDF_DICT) {
                    pdfi_countdown(o);
                    *transparent = true;
                    return 0;
                }
            }
        }
        pdfi_countdown(o);
        o = NULL;

        code = pdfi_dict_knownget_number(ctx, extgstate_dict, "CA", &f);
        if (code > 0) {
            if (f != 1.0) {
                *transparent = true;
                return 0;
            }
        }

        code = pdfi_dict_knownget_number(ctx, extgstate_dict, "ca", &f);
        if (code > 0) {
            if (f != 1.0) {
                *transparent = true;
                return 0;
            }
        }

    }
    return 0;
}

static int pdfi_check_ExtGState_dict(pdf_context *ctx, pdf_dict *extgstate_dict, pdf_dict *page_dict, int *transparent)
{
    int code, i, index;
    pdf_obj *Key = NULL, *Value = NULL, *o = NULL;
    double f;

    if (extgstate_dict->entries > 0) {
        code = pdfi_loop_detector_mark(ctx); /* Mark the start of the ColorSpace dictionary loop */
        if (code < 0)
            return code;

        code = pdfi_dict_first(ctx, (pdf_dict *)extgstate_dict, &Key, &Value, (void *)&index);
        if (code < 0)
            goto error1;

        i = 1;
        do {

            (void)pdfi_check_ExtGState(ctx, extgstate_dict, transparent);
            if (*transparent == true)
                goto transparency_exit;

            pdfi_countdown(Key);
            Key = NULL;
            pdfi_countdown(Value);
            Value = NULL;

            (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the Shading dictionary loop */

            code = pdfi_loop_detector_mark(ctx); /* Mark the new start of the Shading dictionary loop */
            if (code < 0)
                goto error1;

            do {
                if (i++ >= pdfi_dict_entries((pdf_dict *)extgstate_dict)) {
                    code = 0;
                    goto transparency_exit;
                }

                code = pdfi_dict_next(ctx, (pdf_dict *)extgstate_dict, &Key, &Value, (void *)&index);
                if (code == 0 && Value->type == PDF_DICT)
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

static int pdfi_check_Pattern_dict(pdf_context *ctx, pdf_dict *pattern_dict, pdf_dict *page_dict, int *transparent, int *num_spots)
{
    int code, i, index;
    pdf_obj *Key = NULL, *Value = NULL, *o = NULL;
    double f;

    if (pattern_dict->entries > 0) {
        code = pdfi_loop_detector_mark(ctx); /* Mark the start of the ColorSpace dictionary loop */
        if (code < 0)
            return code;

        code = pdfi_dict_first(ctx, (pdf_dict *)pattern_dict, &Key, &Value, (void *)&index);
        if (code < 0)
            goto error1;

        i = 1;
        do {
            code = pdfi_dict_knownget_type(ctx, (pdf_dict *)Value, "Resources", PDF_DICT, &o);
            if (code > 0)
                (void)pdfi_check_Resources_for_transparency(ctx, (pdf_dict *)o, page_dict, transparent, num_spots);
            pdfi_countdown(o);
            o = NULL;
            if (*transparent == true)
                goto transparency_exit;

            code = pdfi_dict_knownget_type(ctx, (pdf_dict *)Value, "ExtGState", PDF_DICT, &o);
            if (code > 0)
                (void)pdfi_check_ExtGState(ctx, (pdf_dict *)o, transparent);
            pdfi_countdown(o);
            o = NULL;
            if (*transparent == true)
                goto transparency_exit;

            pdfi_countdown(Key);
            Key = NULL;
            pdfi_countdown(Value);
            Value = NULL;

            (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the Shading dictionary loop */

            code = pdfi_loop_detector_mark(ctx); /* Mark the new start of the Shading dictionary loop */
            if (code < 0)
                goto error1;

            do {
                if (i++ >= pdfi_dict_entries((pdf_dict *)pattern_dict)) {
                    code = 0;
                    goto transparency_exit;
                }

                code = pdfi_dict_next(ctx, (pdf_dict *)pattern_dict, &Key, &Value, (void *)&index);
                if (code == 0 && Value->type == PDF_DICT)
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

static int pdfi_check_Font_dict(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *page_dict, int *transparent, int *num_spots)
{
    int code, i, index;
    pdf_obj *Key = NULL, *Value = NULL, *o = NULL;
    double f;

    if (font_dict->entries > 0) {
        code = pdfi_loop_detector_mark(ctx); /* Mark the start of the ColorSpace dictionary loop */
        if (code < 0)
            return code;

        code = pdfi_dict_first(ctx, (pdf_dict *)font_dict, &Key, &Value, (void *)&index);
        if (code < 0)
            goto error1;

        i = 1;
        do {
            code = pdfi_dict_knownget_type(ctx, (pdf_dict *)Value, "Subtype", PDF_NAME, &o);
            if (code > 0) {
                if (pdfi_name_strcmp((pdf_name *)o, "Type3") == 0) {
                    pdfi_countdown(o);
                    o = NULL;

                    code = pdfi_dict_knownget_type(ctx, (pdf_dict *)Value, "Resources", PDF_DICT, &o);
                    if (code > 0)
                        (void)pdfi_check_Resources_for_transparency(ctx, (pdf_dict *)o, page_dict, transparent, num_spots);
                }
            }
            pdfi_countdown(o);
            o = NULL;

            pdfi_countdown(Key);
            Key = NULL;
            pdfi_countdown(Value);
            Value = NULL;

            (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the Shading dictionary loop */

            code = pdfi_loop_detector_mark(ctx); /* Mark the new start of the Shading dictionary loop */
            if (code < 0)
                goto error1;

            do {
                if (i++ >= pdfi_dict_entries((pdf_dict *)font_dict)) {
                    code = 0;
                    goto transparency_exit;
                }

                code = pdfi_dict_next(ctx, (pdf_dict *)font_dict, &Key, &Value, (void *)&index);
                if (code == 0 && Value->type == PDF_DICT)
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

static int pdfi_check_Resources_for_transparency(pdf_context *ctx, pdf_dict *Resources_dict, pdf_dict *page_dict, bool *transparent, int *num_spots)
{
    int code, i, index;
    pdf_obj *d = NULL, *n = NULL;
    double f;
    bool known = false;

    /* First up, check any colour spaces, for new spot colours.
     * We only do this if asked because its expensive. num_spots being NULL
     * means we aren't interested in spot colours (not a DeviceN or Separation device)
     */
    if (num_spots != NULL) {
        code = pdfi_dict_knownget_type(ctx, Resources_dict, "ColorSpace", PDF_DICT, &d);
        if (code > 0)
            (void)pdfi_check_ColorSpace_dict(ctx, (pdf_dict *)d, page_dict, num_spots);

        pdfi_countdown(d);
        d = NULL;

        code = pdfi_dict_knownget_type(ctx, Resources_dict, "Shading", PDF_DICT, &d);
        if (code > 0)
            (void)pdfi_check_Shading_dict(ctx, (pdf_dict *)d, page_dict, num_spots);
        pdfi_countdown(d);
        d = NULL;
    }

    code = pdfi_dict_knownget_type(ctx, Resources_dict, "XObject", PDF_DICT, &d);
    if (code > 0)
        (void)pdfi_check_XObject_dict(ctx, (pdf_dict *)d, page_dict, transparent, num_spots);
    pdfi_countdown(d);
    d = NULL;

    code = pdfi_dict_knownget_type(ctx, Resources_dict, "Pattern", PDF_DICT, &d);
    if (code > 0)
        (void)pdfi_check_Pattern_dict(ctx, (pdf_dict *)d, page_dict, transparent, num_spots);
    pdfi_countdown(d);
    d = NULL;

    code = pdfi_dict_knownget_type(ctx, Resources_dict, "Font", PDF_DICT, &d);
    if (code > 0)
        (void)pdfi_check_Font_dict(ctx, (pdf_dict *)d, page_dict, transparent, num_spots);
    /* From this point onwards, if we detect transparency (or have already detected it) we
     * can exit, we have already counted up any spot colours.
     */
    pdfi_countdown(d);
    d = NULL;

    code = pdfi_dict_knownget_type(ctx, Resources_dict, "ExtGState", PDF_DICT, &d);
    if (code > 0)
        (void)pdfi_check_ExtGState_dict(ctx, (pdf_dict *)d, page_dict, transparent);
    pdfi_countdown(d);
    d = NULL;

    return 0;
}

static int pdfi_check_annot_for_transparency(pdf_context *ctx, pdf_dict *annot, pdf_dict *page_dict, bool *transparent, int *num_spots)
{
    int code;
    pdf_name *n;
    pdf_dict *ap = NULL, *N = NULL, *Resources = NULL;
    bool known = false;
    double f;

    /* Check #1 Does the (Normal) Appearnce stream use any Resources which include transparency.
     * We check this first, because this also checks for spot colour spaces. Once we've done that we
     * can exit the checks as soon as we detect transparency.
     */
    code = pdfi_dict_knownget_type(ctx, annot, "AP", PDF_DICT, (pdf_obj **)&ap);
    if (code > 0)
    {
        code = pdfi_dict_knownget_type(ctx, ap, "N", PDF_DICT, (pdf_obj **)&N);
        if (code > 0) {
            code = pdfi_dict_knownget_type(ctx, N, "Resources", PDF_DICT, (pdf_obj **)&Resources);
            if (code > 0)
                code = pdfi_check_Resources_for_transparency(ctx, (pdf_dict *)Resources, page_dict, transparent, num_spots);
        }
    }
    pdfi_countdown(ap);
    pdfi_countdown(N);
    pdfi_countdown(Resources);
    if (code < 0)
        return code;
    /* We've checked the Resources, and nothing else in an annotation can define spot colours, so
     * if we detected transparency in the Resources we need not check further.
     */
    if (*transparent == true)
        return 0;

    code = pdfi_dict_get_type(ctx, annot, "Subtype", PDF_NAME, (pdf_obj **)&n);
    if (code < 0) {
        if (ctx->pdfstoponerror)
            return code;
    } else {
        /* Check #2, Highlight annotations are always preformed with transparency */
        if (pdfi_name_strcmp((const pdf_name *)n, "Highlight") == 0) {
            pdfi_countdown(n);
            *transparent = true;
            return 0;
        }
        pdfi_countdown(n);
        n = NULL;

        /* Check #3 Blend Mode (BM) not being 'Normal' or 'Compatible' */
        code = pdfi_dict_knownget_type(ctx, annot, "BM", PDF_NAME, (pdf_obj **)&n);
        if (code > 0) {
            if (pdfi_name_strcmp((const pdf_name *)n, "Normal") != 0) {
                if (pdfi_name_strcmp((const pdf_name *)n, "Compatible") != 0) {
                    pdfi_countdown(n);
                    *transparent = true;
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
                *transparent = true;
                return 0;
            }
        }
        if (code < 0)
            return code;

        /* Check #5 non-stroke constant alpha (ca) is not 1 (100% opaque) */
        code = pdfi_dict_knownget_number(ctx, annot, "ca", &f);
        if (code > 0) {
            if (f != 1.0) {
                *transparent = true;
                return 0;
            }
        }
        if (code < 0)
            return code;
    }

    return 0;
}

static int pdfi_check_Annots_for_transparency(pdf_context *ctx, pdf_array *annots_array, pdf_dict *page_dict, bool *transparent, int *num_spots)
{
    int i, code;
    pdf_dict *annot = NULL;

    for (i=0; i < annots_array->entries; i++) {
        annot = (pdf_dict *)annots_array->values[i];

        if (annot->type == PDF_DICT) {
            code = pdfi_check_annot_for_transparency(ctx, annot, page_dict, transparent, num_spots);
            if (code < 0 && ctx->pdfstoponerror)
                return code;
            /* If we've found transparency, and don't need to continue checkign for spot colours
             * just exit as fast as possible.
             */
            if (*transparent == true && num_spots == NULL)
                return 0;
        }
    }
    return 0;
}

/* From the original PDF interpreter written in PostScript:
 * Note: we deliberately don't check to see whether a Group is defined,
 * because Adobe Illustrator 10 (and possibly other applications) define
 * a page-level group whether transparency is actually used or not.
 * Ignoring the presence of Group is justified because, in the absence
 * of any other transparency features, they have no effect.
 */
static int pdfi_check_page_transparency(pdf_context *ctx, pdf_dict *page_dict, bool *transparent, int *spots)
{
    int code;
    pdf_obj *d = NULL;
    int num_spots = 0;
    int spot_capable = 0;

    *transparent = false;
    if (ctx->notransparency == true)
        return 0;

    gs_c_param_list_read(&ctx->pdfi_param_list);
    code = param_read_int((gs_param_list *)&ctx->pdfi_param_list, "PageSpotColors", &spot_capable);
    if (code < 0)
        return code;
    if (code > 0)
        spot_capable = 0;
    else
        spot_capable = 1;

    code = pdfi_dict_get_type(ctx, page_dict, "Resources", PDF_DICT, &d);
    if (code >= 0) {
        if (spot_capable)
            code = pdfi_check_Resources_for_transparency(ctx, (pdf_dict *)d, page_dict, transparent, &num_spots);
        else
            code = pdfi_check_Resources_for_transparency(ctx, (pdf_dict *)d, page_dict, transparent, NULL);
        pdfi_countdown(d);
        if (code < 0)
            return code;
    }
    if (code < 0 && code != gs_error_undefined && ctx->pdfstoponerror)
        return code;

    if (ctx->showannots) {
        code = pdfi_dict_get_type(ctx, page_dict, "Annots", PDF_ARRAY, &d);
        if (code >= 0) {
            if (spot_capable)
                code = pdfi_check_Annots_for_transparency(ctx, (pdf_array *)d, page_dict, transparent, &num_spots);
            else
                code = pdfi_check_Annots_for_transparency(ctx, (pdf_array *)d, page_dict, transparent, NULL);
            pdfi_countdown(d);
            if (code < 0)
                return code;
        }
        if (code < 0 && code != gs_error_undefined && ctx->pdfstoponerror)
            return code;
    }

    if (spot_capable) {
        *spots = num_spots;
    } else
        *spots = -1;

    return 0;
}

static int pdfi_set_media_size(pdf_context *ctx, pdf_dict *page_dict)
{
    gs_c_param_list list;
    gs_param_float_array fa;
    pdf_array *a = NULL, *default_media = NULL;
    float fv[2];
    double d[4];
    int code;
    uint64_t i;
    int64_t rotate = 0;

    gs_c_param_list_write(&list, ctx->memory);

    code = pdfi_dict_get_type(ctx, page_dict, "MediaBox", PDF_ARRAY, (pdf_obj **)&default_media);
    if (code < 0) {
        code = gs_erasepage(ctx->pgs);
        return 0;
    }

    if (ctx->usecropbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "CropBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (ctx->useartbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "ArtBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (ctx->usebleedbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "BleedBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (ctx->usetrimbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "MediaBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (a == NULL)
        a = default_media;

    for (i=0;i<4;i++) {
        code = pdfi_array_get_number(ctx, a, i, &d[i]);
    }

    normalize_rectangle(d);
    memcpy(ctx->PageSize, d, 4 * sizeof(double));

    code = pdfi_dict_get_int(ctx, page_dict, "Rotate", &rotate);

    rotate = rotate % 360;

    switch(rotate) {
        default:
        case 0:
        case 360:
        case -180:
        case 180:
            fv[0] = (float)(d[2] - d[0]);
            fv[1] = (float)(d[3] - d[1]);
            break;
        case -90:
        case 90:
        case -270:
        case 270:
            fv[1] = (float)(d[2] - d[0]);
            fv[0] = (float)(d[3] - d[1]);
            break;
    }

    fa.persistent = false;
    fa.data = fv;
    fa.size = 2;

    code = param_write_float_array((gs_param_list *)&list, ".MediaSize", &fa);
    if (code >= 0)
    {
        gx_device *dev = gs_currentdevice(ctx->pgs);

        gs_c_param_list_read(&list);
        code = gs_putdeviceparams(dev, (gs_param_list *)&list);
        if (code < 0) {
            gs_c_param_list_release(&list);
            return code;
        }
    }
    gs_c_param_list_release(&list);

    gs_initgraphics(ctx->pgs);

    switch(rotate) {
        case 0:
            break;
        case -270:
        case 90:
            gs_translate(ctx->pgs, 0, fv[1]);
            gs_rotate(ctx->pgs, -90);
            break;
        case -180:
        case 180:
            gs_translate(ctx->pgs, fv[0], fv[1]);
            gs_rotate(ctx->pgs, 180);
            break;
        case -90:
        case 270:
            gs_translate(ctx->pgs, fv[0], 0);
            gs_rotate(ctx->pgs, 90);
            break;
        default:
            break;
    }

    gs_translate(ctx->pgs, d[0] * -1, d[1] * -1);

    code = gs_erasepage(ctx->pgs);
    return 0;
}

/*
 * Convenience routine to check if a given string exists in a dictionary
 * verify its contents and print it in a particular fashion to stdout. This
 * is used to display information about the PDF in response to -dPDFINFO
 */
static int dump_info_string(pdf_context *ctx, pdf_dict *source_dict, const char *Key)
{
    int code;
    pdf_string *s = NULL;
    char *Cstr;
    bool known;

    code = pdfi_dict_knownget_type(ctx, source_dict, Key, PDF_STRING, (pdf_obj **)&s);
    if (code > 0) {
        Cstr = (char *)gs_alloc_bytes(ctx->memory, s->length + 1, "Working memory for string dumping");
        if (Cstr) {
            memcpy(Cstr, s->data, s->length);
            Cstr[s->length] = 0x00;
            dmprintf2(ctx->memory, "%s: %s\n", Key, Cstr);
            gs_free_object(ctx->memory, Cstr, "Working memory for string dumping");
        }
        code = 0;
    }
    pdfi_countdown(s);

    return code;
}

static int pdfi_output_metadata(pdf_context *ctx)
{
    int code;
    bool known = false;

    if (ctx->num_pages > 1)
        dmprintf2(ctx->memory, "\n        %s has %"PRIi64" pages\n\n", ctx->filename, ctx->num_pages);
    else
        dmprintf2(ctx->memory, "\n        %s has %"PRIi64" page.\n\n", ctx->filename, ctx->num_pages);

    if (ctx->Info != NULL) {
        pdf_name *n = NULL;
        char *Cstr;

        code = dump_info_string(ctx, ctx->Info, "Title");
        if (code < 0) {
            if (ctx->pdfstoponerror)
                return code;
        }

        code = dump_info_string(ctx, ctx->Info, "Author");
        if (code < 0) {
            if (ctx->pdfstoponerror)
                return code;
        }

        code = dump_info_string(ctx, ctx->Info, "Subject");
        if (code < 0) {
            if (ctx->pdfstoponerror)
                return code;
        }

        code = dump_info_string(ctx, ctx->Info, "Keywords");
        if (code < 0) {
            if (ctx->pdfstoponerror)
                return code;
        }

        code = dump_info_string(ctx, ctx->Info, "Creator");
        if (code < 0) {
            if (ctx->pdfstoponerror)
                return code;
        }

        code = dump_info_string(ctx, ctx->Info, "Producer");
        if (code < 0) {
            if (ctx->pdfstoponerror)
                return code;
        }

        code = dump_info_string(ctx, ctx->Info, "CreationDate");
        if (code < 0) {
            if (ctx->pdfstoponerror)
                return code;
        }

        code = dump_info_string(ctx, ctx->Info, "ModDate");
        if (code < 0) {
            if (ctx->pdfstoponerror)
                return code;
        }


        code = pdfi_dict_knownget_type(ctx, ctx->Info, "Trapped", PDF_NAME, (pdf_obj **)&n);
        if (code > 0) {
            Cstr = (char *)gs_alloc_bytes(ctx->memory, n->length + 1, "Working memory for string dumping");
            if (Cstr) {
                memcpy(Cstr, n->data, n->length);
                Cstr[n->length] = 0x00;
                dmprintf1(ctx->memory, "Trapped: %s\n\n", Cstr);
                gs_free_object(ctx->memory, Cstr, "Working memory for string dumping");
            }
            code = 0;
        }
        pdfi_countdown(n);
        n = NULL;
    }
    return code;
}

/*
 * Convenience routine to check if a given *Box exists in a page dictionary
 * verify its contents and print it in a particular fashion to stdout. This
 * is used to display information about the PDF in response to -dPDFINFO
 */
static int pdfi_dump_box(pdf_context *ctx, pdf_dict *page_dict, const char *Key)
{
    int code, i;
    pdf_array *a = NULL;

    code = pdfi_dict_knownget_type(ctx, page_dict, Key, PDF_ARRAY, (pdf_obj **)&a);
    if (code > 0) {
        if (a->entries != 4) {
            code = gs_note_error(gs_error_rangecheck);
        } else {
            for (i = 0; i < a->entries; i++) {
                if (a->values[i]->type != PDF_INT && a->values[i]->type != PDF_REAL){
                    code = gs_note_error(gs_error_rangecheck);
                    break;
                }
            }
            if (i == a->entries) {
                dmprintf1(ctx->memory, " %s: [", Key);
                for (i=0;i < a->entries;i++) {
                    if (i != 0)
                        dmprintf(ctx->memory, " ");
                    if (a->values[i]->type == PDF_INT)
                        dmprintf1(ctx->memory, "%"PRIi64"", ((pdf_num *)a->values[i])->value.i);
                    else
                        dmprintf1(ctx->memory, "%f", ((pdf_num *)a->values[i])->value.d);
                }
                dmprintf(ctx->memory, "]");
            }
        }
    }
    pdfi_countdown(a);
    return code;
}

/*
 * This routine along with pdfi_output_metadtaa above, dumps certain kinds
 * of metadata from the PDF file, and from each page in the PDF file. It is
 * intended to duplicate the pdf_info.ps functionality of the PostScript-based
 * PDF interpreter in Ghostscript.
 *
 * It is not yet complete, we don't allow an option for dumping media sizes
 * we always emit them, and the switches -dDumpFontsNeeded, -dDumpXML,
 * -dDumpFontsUsed and -dShowEmbeddedFonts are not implemented at all yet.
 */
static int pdfi_output_page_info(pdf_context *ctx, uint64_t page_num)
{
    int code, spots;
    bool known = false, transparent = false;
    double f;
    uint64_t page_offset = 0;
    pdf_dict *page_dict = NULL;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    code = pdfi_loop_detector_add_object(ctx, ctx->Pages->object_num);
    if (code < 0) {
        pdfi_loop_detector_cleartomark(ctx);
        return code;
    }

    code = pdfi_get_page_dict(ctx, ctx->Pages, page_num, &page_offset, &page_dict, NULL);
    pdfi_loop_detector_cleartomark(ctx);
    if (code < 0) {
        if (code == gs_error_VMerror || ctx->pdfstoponerror)
            return code;
        return 0;
    }

    dmprintf1(ctx->memory, "Page %"PRIi64"", page_num + 1);

    code = pdfi_dict_knownget_number(ctx, page_dict, "UserUnit", &f);
    if (code > 0)
        dmprintf1(ctx->memory, " UserUnit: %f ", f);
    if (code < 0) {
        pdfi_countdown(page_dict);
        return code;
    }

    code = pdfi_dump_box(ctx, page_dict, "MediaBox");
    if (code < 0) {
        if (code != gs_error_undefined && ctx->pdfstoponerror) {
            pdfi_countdown(page_dict);
            return code;
        }
    }

    code = pdfi_dump_box(ctx, page_dict, "CropBox");
    if (code < 0) {
        if (code != gs_error_undefined && ctx->pdfstoponerror) {
            pdfi_countdown(page_dict);
            return code;
        }
    }

    code = pdfi_dump_box(ctx, page_dict, "BleedBox");
    if (code < 0) {
        if (code != gs_error_undefined && ctx->pdfstoponerror) {
            pdfi_countdown(page_dict);
            return code;
        }
    }

    code = pdfi_dump_box(ctx, page_dict, "TrimBox");
    if (code < 0) {
        if (code != gs_error_undefined && ctx->pdfstoponerror) {
            pdfi_countdown(page_dict);
            return code;
        }
    }

    code = pdfi_dump_box(ctx, page_dict, "ArtBox");
    if (code < 0) {
        if (code != gs_error_undefined && ctx->pdfstoponerror) {
            pdfi_countdown(page_dict);
            return code;
        }
    }

    code = pdfi_dict_knownget_number(ctx, page_dict, "Rotate", &f);
    if (code > 0)
        dmprintf1(ctx->memory, "    Rotate = %d ", (int)f);
    if (code < 0) {
        pdfi_countdown(page_dict);
        return code;
    }

    code = pdfi_check_page_transparency(ctx, page_dict, &transparent, &spots);
    if (code < 0) {
        if (ctx->pdfstoponerror)
            return code;
    } else {
        if (transparent == true)
            dmprintf(ctx->memory, "     Page uses transparency features");
    }

    code = pdfi_dict_known(page_dict, "Annots", &known);
    if (code < 0) {
        if (code != gs_error_undefined && ctx->pdfstoponerror)
            return code;
    } else {
        if (known == true)
            dmprintf(ctx->memory, "     Page contains Annotations");
    }

    dmprintf(ctx->memory, "\n\n");
    pdfi_countdown(page_dict);

    return 0;
}

static int pdfi_render_page(pdf_context *ctx, uint64_t page_num)
{
    int code, spots;
    uint64_t page_offset = 0;
    pdf_dict *page_dict = NULL;
    bool uses_transparency = false;
    bool page_group_known = false;

    if (page_num > ctx->num_pages)
        return_error(gs_error_rangecheck);

    if (ctx->pdfdebug)
        dmprintf1(ctx->memory, "%% Processing Page %"PRIi64" content stream\n", page_num + 1);

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    code = pdfi_loop_detector_add_object(ctx, ctx->Pages->object_num);
    if (code < 0) {
        pdfi_loop_detector_cleartomark(ctx);
        return code;
    }

    code = pdfi_get_page_dict(ctx, ctx->Pages, page_num, &page_offset, &page_dict, NULL);
    pdfi_loop_detector_cleartomark(ctx);
    if (code < 0) {
        if (code == gs_error_VMerror || ctx->pdfstoponerror)
            return code;
        return 0;
    }

    if (code > 0)
        return_error(gs_error_unknownerror);

    code = pdfi_set_media_size(ctx, page_dict);
    if (code < 0) {
        pdfi_countdown(page_dict);
        return code;
    }

    code = pdfi_alloc_object(ctx, PDF_DICT, 32, (pdf_obj **)&ctx->SpotNames);
    if (code < 0) {
        pdfi_countdown(page_dict);
        return code;
    }
    ctx->SpotNames->refcnt++;

    if (ctx->notransparency == false) {
        pdf_dict *group_dict = NULL;

        code = pdfi_check_page_transparency(ctx, page_dict, &uses_transparency, &spots);
        if (code < 0) {
            pdfi_countdown(ctx->SpotNames);
            pdfi_countdown(page_dict);
            return code;
        }

        code = pdfi_dict_knownget_type(ctx, page_dict, "Group", PDF_DICT, (pdf_obj **)&group_dict);
        if (code > 0) {
            pdf_obj *CS = NULL;

            page_group_known = true;
            code = pdfi_dict_knownget(ctx, group_dict, "CS", &CS);
            if (code > 0) {
                code = pdfi_check_ColorSpace_for_spots(ctx, CS, group_dict, page_dict, &spots);
                if (code < 0 && ctx->pdfstoponerror) {
                    pdfi_countdown(CS);
                    pdfi_countdown(group_dict);
                    pdfi_countdown(ctx->SpotNames);
                    pdfi_countdown(page_dict);
                    return code;
                }
                pdfi_countdown(CS);
            }
            pdfi_countdown(group_dict);
        }
        if (code < 0) {
            pdfi_countdown(ctx->SpotNames);
            pdfi_countdown(page_dict);
            return code;
        }
    }

    ctx->page_has_transparency = uses_transparency;

#ifndef DEBUG
    ctx->page_has_transparency = false;
    dmprintf1(ctx->memory, "Setting page transparency to false, current page setting is %d\n", uses_transparency);
#else
    dmprintf1(ctx->memory, "Current page transparency setting is %d\n", uses_transparency);
#endif

    if (spots > 0) {
        gs_c_param_list_write(&ctx->pdfi_param_list, ctx->memory);
        param_write_int((gs_param_list *)&ctx->pdfi_param_list, "PageSpotColors", &spots);
        gs_c_param_list_read(&ctx->pdfi_param_list);
        code = gs_putdeviceparams(ctx->pgs->device, (gs_param_list *)&ctx->pdfi_param_list);
        if (code > 0) {
            /* The device was closed, we need to reopen it */
            code = gs_setdevice_no_erase(ctx->pgs, ctx->pgs->device);
            if (code < 0) {
                if (uses_transparency)
                    (void)gs_abort_pdf14trans_device(ctx->pgs);
                pdfi_countdown(ctx->SpotNames);
                pdfi_countdown(page_dict);
                return code;
            }
        }
    }

#ifdef DEBUG
    if (uses_transparency) {
#else
    if (ctx->page_has_transparency) {
#endif
        code = gs_push_pdf14trans_device(ctx->pgs, false);
        if (code < 0) {
            pdfi_countdown(ctx->SpotNames);
            pdfi_countdown(page_dict);
            return code;
        }
        if (page_group_known) {
            code = pdfi_begin_page_group(ctx, page_dict);
            if (code < 0) {
                pdfi_countdown(ctx->SpotNames);
                pdfi_countdown(page_dict);
                return code;
            }
        }
    }


    code = pdfi_process_page_contents(ctx, page_dict);
    if (code < 0) {
        if (uses_transparency)
            (void)gs_abort_pdf14trans_device(ctx->pgs);
        pdfi_countdown(ctx->SpotNames);
        pdfi_countdown(page_dict);
        return code;
    }

#ifdef DEBUG
    if (uses_transparency) {
#else
    if (ctx->page_has_transparency) {
#endif
        if (page_group_known) {
            code = pdfi_end_transparency_group(ctx);
            if (code < 0) {
                (void)gs_abort_pdf14trans_device(ctx->pgs);
                pdfi_countdown(ctx->SpotNames);
                pdfi_countdown(page_dict);
                return code;
            }
        }

        code = gs_pop_pdf14trans_device(ctx->pgs, false);
        if (code < 0) {
            pdfi_countdown(ctx->SpotNames);
            pdfi_countdown(page_dict);
            return code;
        }
    }

    pdfi_countdown(ctx->SpotNames);
    ctx->SpotNames = NULL;

    pdfi_countdown(page_dict);

    return pl_finish_page(ctx->memory->gs_lib_ctx->top_of_system,
                          ctx->pgs, 1, true);
}

/* These functions are used by the 'PL' implementation, eventually we will */
/* need to have custom PostScript operators to process the file or at      */
/* (least pages from it).                                                  */

int pdfi_close_pdf_file(pdf_context *ctx)
{
    if (ctx->main_stream) {
        if (ctx->main_stream->s) {
            sfclose(ctx->main_stream->s);
            ctx->main_stream = NULL;
        }
        gs_free_object(ctx->memory, ctx->main_stream, "Closing main PDF file");
    }
    return 0;
}

int pdfi_process_pdf_file(pdf_context *ctx, char *filename)
{
    int code = 0, i;
    pdf_obj *o;

    ctx->filename = (char *)gs_alloc_bytes(ctx->memory, strlen(filename) + 1, "copy of filename");
    if (ctx->filename == NULL)
        return_error(gs_error_VMerror);
    strcpy(ctx->filename, filename);

    code = pdfi_open_pdf_file(ctx, filename);
    if (code < 0) {
        return code;
    }

    code = pdfi_read_xref(ctx);
    if (code < 0) {
        if (ctx->is_hybrid) {
            /* If its a hybrid file, and we failed to read the XrefStm, try
             * again, but this time read the xref table instead.
             */
            ctx->pdf_errors |= E_PDF_BADXREFSTREAM;
            pdfi_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            ctx->prefer_xrefstm = false;
            code = pdfi_read_xref(ctx);
            if (code < 0)
                return code;
        } else {
            ctx->pdf_errors |= E_PDF_BADXREF;
            return code;
        }
    }

    if (ctx->Trailer) {
        code = pdfi_dict_get(ctx, ctx->Trailer, "Encrypt", &o);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0) {
            dmprintf(ctx->memory, "Encrypted PDF files not yet supported.\n");
            return 0;
        }
    }

read_root:
    if (ctx->Trailer) {
        code = pdfi_read_Root(ctx);
        if (code < 0) {
            /* If we couldn#'t find the Root object, and we were using the XrefStm
             * from a hybrid file, then try again, but this time use the xref table
             */
            if (code == gs_error_undefined && ctx->is_hybrid && ctx->prefer_xrefstm) {
                ctx->pdf_errors |= E_PDF_BADXREFSTREAM;
                pdfi_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                ctx->prefer_xrefstm = false;
                code = pdfi_read_xref(ctx);
                if (code < 0) {
                    ctx->pdf_errors |= E_PDF_BADXREF;
                    return code;
                }
                code = pdfi_read_Root(ctx);
                if (code < 0)
                    return code;
            } else {
                if (!ctx->repaired) {
                    code = pdfi_repair_file(ctx);
                    if (code < 0)
                        return code;
                    goto read_root;
                }
                return code;
            }
        }
    }

    if (ctx->Trailer) {
        code = pdfi_read_Info(ctx);
        if (code < 0 && code != gs_error_undefined) {
            if (ctx->pdfstoponerror)
                return code;
            pdfi_clearstack(ctx);
        }
    }

    if (!ctx->Root) {
        dmprintf(ctx->memory, "Catalog dictionary not located in file, unable to proceed\n");
        return_error(gs_error_syntaxerror);
    }

    code = pdfi_read_Pages(ctx);
    if (code < 0)
        return code;

    if (ctx->pdfinfo) {
        code = pdfi_output_metadata(ctx);
        if (code < 0 && ctx->pdfstoponerror)
            return code;
    }

    for (i=0;i < ctx->num_pages;i++) {
        if (ctx->first_page != 0) {
            if (i < ctx->first_page - 1)
                continue;
        }
        if (ctx->last_page != 0) {
            if (i >= ctx->last_page - 1)
                break;;
        }
        if (ctx->pdfinfo)
            code = pdfi_output_page_info(ctx, i);
        else
            code = pdfi_render_page(ctx, i);

        if (code < 0 && ctx->pdfstoponerror)
            return code;
    }

    code = sfclose(ctx->main_stream->s);
    ctx->main_stream = NULL;
    return code;
}

static void cleanup_pdfi_open_file(pdf_context *ctx, byte *Buffer)
{
    if (Buffer != NULL)
        gs_free_object(ctx->memory, Buffer, "PDF interpreter - cleanup working buffer for file validation");

    if (ctx->main_stream != NULL) {
        sfclose(ctx->main_stream->s);
        ctx->main_stream = NULL;
    }
    ctx->main_stream_length = 0;
}

int pdfi_open_pdf_file(pdf_context *ctx, char *filename)
{
    byte *Buffer = NULL;
    char *s = NULL;
    float version = 0.0;
    gs_offset_t Offset = 0;
    int64_t bytes = 0;
    bool found = false;

    if (ctx->pdfdebug)
        dmprintf1(ctx->memory, "%% Attempting to open %s as a PDF file\n", filename);

    ctx->main_stream = (pdf_stream *)gs_alloc_bytes(ctx->memory, sizeof(pdf_stream), "PDF interpreter allocate main PDF stream");
    if (ctx->main_stream == NULL)
        return_error(gs_error_VMerror);
    memset(ctx->main_stream, 0x00, sizeof(pdf_stream));

    ctx->main_stream->s = sfopen(filename, "r", ctx->memory);
    if (ctx->main_stream == NULL) {
        emprintf1(ctx->memory, "Failed to open file %s\n", filename);
        return_error(gs_error_ioerror);
    }

    Buffer = gs_alloc_bytes(ctx->memory, BUF_SIZE, "PDF interpreter - allocate working buffer for file validation");
    if (Buffer == NULL) {
        cleanup_pdfi_open_file(ctx, Buffer);
        return_error(gs_error_VMerror);
    }

    /* Determine file size */
    pdfi_seek(ctx, ctx->main_stream, 0, SEEK_END);
    ctx->main_stream_length = pdfi_tell(ctx->main_stream);
    Offset = BUF_SIZE;
    bytes = BUF_SIZE;
    pdfi_seek(ctx, ctx->main_stream, 0, SEEK_SET);

    bytes = Offset = min(BUF_SIZE, ctx->main_stream_length);

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading header\n");

    bytes = pdfi_read_bytes(ctx, Buffer, 1, Offset, ctx->main_stream);
    if (bytes <= 0) {
        emprintf(ctx->memory, "Failed to read any bytes from file\n");
        cleanup_pdfi_open_file(ctx, Buffer);
        return_error(gs_error_ioerror);
    }

    /* First check for existence of header */
    s = strstr((char *)Buffer, "%PDF");
    if (s == NULL) {
        if (ctx->pdfdebug)
            dmprintf1(ctx->memory, "%% File %s does not appear to be a PDF file (no %%PDF in first 2Kb of file)\n", filename);
        ctx->pdf_errors |= E_PDF_NOHEADER;
    } else {
        /* Now extract header version (may be overridden later) */
        if (sscanf(s + 5, "%f", &version) != 1) {
            if (ctx->pdfdebug)
                dmprintf(ctx->memory, "%% Unable to read PDF version from header\n");
            ctx->HeaderVersion = 0;
            ctx->pdf_errors |= E_PDF_NOHEADERVERSION;
        }
        else {
            ctx->HeaderVersion = version;
        }
        if (ctx->pdfdebug)
            dmprintf1(ctx->memory, "%% Found header, PDF version is %f\n", ctx->HeaderVersion);
    }

    /* Jump to EOF and scan backwards looking for startxref */
    pdfi_seek(ctx, ctx->main_stream, 0, SEEK_END);

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Searching for 'startxerf' keyword\n");

    bytes = Offset;
    do {
        byte *last_lineend = NULL;
        uint32_t read;

        if (pdfi_seek(ctx, ctx->main_stream, ctx->main_stream_length - Offset, SEEK_SET) != 0) {
            emprintf1(ctx->memory, "File is smaller than %"PRIi64" bytes\n", (int64_t)Offset);
            cleanup_pdfi_open_file(ctx, Buffer);
            return_error(gs_error_ioerror);
        }
        read = pdfi_read_bytes(ctx, Buffer, 1, bytes, ctx->main_stream);

        if (read <= 0) {
            emprintf1(ctx->memory, "Failed to read %"PRIi64" bytes from file\n", (int64_t)bytes);
            cleanup_pdfi_open_file(ctx, Buffer);
            return_error(gs_error_ioerror);
        }

        read = bytes = read + (BUF_SIZE - bytes);

        while(read) {
            if (memcmp(Buffer + read - 9, "startxref", 9) == 0) {
                found = true;
                break;
            } else {
                if (Buffer[read] == 0x0a || Buffer[read] == 0x0d)
                    last_lineend = Buffer + read;
            }
            read--;
        }
        if (found) {
            byte *b = Buffer + read;

            if(sscanf((char *)b, " %ld", &ctx->startxref) != 1) {
                dmprintf(ctx->memory, "Unable to read offset of xref from PDF file\n");
            }
            break;
        } else {
            if (last_lineend) {
                uint32_t len = last_lineend - Buffer;
                memcpy(Buffer + bytes - len, last_lineend, len);
                bytes -= len;
            }
        }

        Offset += bytes;
    } while(Offset < ctx->main_stream_length);

    if (!found)
        ctx->pdf_errors |= E_PDF_NOSTARTXREF;

    gs_free_object(ctx->memory, Buffer, "PDF interpreter - allocate working buffer for file validation");
    return 0;
}

/***********************************************************************************/
/* Highest level functions. The context we create here is returned to the 'PL'     */
/* implementation, in future we plan to return it to PostScript by wrapping a      */
/* gargabe collected object 'ref' around it and returning that to the PostScript   */
/* world. custom PostScript operators will then be able to render pages, annots,   */
/* AcroForms etc by passing the opaque object back to functions here, allowing     */
/* the interpreter access to its context.                                          */

/* We start with routines for creating and destroying the interpreter context */
pdf_context *pdfi_create_context(gs_memory_t *pmem)
{
    pdf_context *ctx = NULL;
    gs_gstate *pgs = NULL;
    int code = 0;

    ctx = (pdf_context *) gs_alloc_bytes(pmem->non_gc_memory,
            sizeof(pdf_context), "pdf_create_context");

    pgs = gs_gstate_alloc(pmem);

    if (!ctx || !pgs)
    {
        if (ctx)
            gs_free_object(pmem->non_gc_memory, ctx, "pdf_create_context");
        if (pgs)
            gs_gstate_free(pgs);
        return NULL;
    }

    memset(ctx, 0, sizeof(pdf_context));

    ctx->stack_bot = (pdf_obj **)gs_alloc_bytes(pmem->non_gc_memory, INITIAL_STACK_SIZE * sizeof (pdf_obj *), "pdf_imp_allocate_interp_stack");
    if (ctx->stack_bot == NULL) {
        gs_free_object(pmem->non_gc_memory, ctx, "pdf_create_context");
        gs_gstate_free(pgs);
        return NULL;
    }
    ctx->stack_size = INITIAL_STACK_SIZE;
    ctx->stack_top = ctx->stack_bot - sizeof(pdf_obj *);
    code = sizeof(pdf_obj *);
    code *= ctx->stack_size;
    ctx->stack_limit = ctx->stack_bot + ctx->stack_size;

    pmem->gs_lib_ctx->font_dir = gs_font_dir_alloc2(pmem->non_gc_memory, pmem->non_gc_memory);
    if (pmem->gs_lib_ctx->font_dir == NULL) {
        gs_free_object(pmem->non_gc_memory, ctx->stack_bot, "pdf_create_context");
        gs_free_object(pmem->non_gc_memory, ctx, "pdf_create_context");
        gs_gstate_free(pgs);
        return NULL;
    }

    code = gsicc_init_iccmanager(pgs);
    if (code < 0) {
        gs_free_object(pmem->non_gc_memory, ctx->stack_bot, "pdf_create_context");
        gs_free_object(pmem->non_gc_memory, ctx, "pdf_create_context");
        gs_gstate_free(pgs);
        return NULL;
    }

    ctx->memory = pmem->non_gc_memory;
    ctx->pgs = pgs;
    /* Declare PDL client support for high level patterns, for the benefit
     * of pdfwrite and other high-level devices
     */
    ctx->pgs->have_pattern_streams = true;
    ctx->preserve_tr_mode = 0;
    ctx->notransparency = false;

    ctx->main_stream = NULL;

    /* Gray, RGB and CMYK profiles set when color spaces installed in graphics lib */
    ctx->gray_lin = gs_cspace_new_ICC(ctx->memory, ctx->pgs, -1);
    ctx->gray = gs_cspace_new_ICC(ctx->memory, ctx->pgs, 1);
    ctx->cmyk = gs_cspace_new_ICC(ctx->memory, ctx->pgs, 4);
    ctx->srgb = gs_cspace_new_ICC(ctx->memory, ctx->pgs, 3);
    ctx->scrgb = gs_cspace_new_ICC(ctx->memory, ctx->pgs, 3);

    /* Initially, prefer the XrefStm in a hybrid file */
    ctx->prefer_xrefstm = true;

    memset(&ctx->pdfi_param_list, 0x00, sizeof(gs_c_param_list));
#if REFCNT_DEBUG
    ctx->UID = 1;
#endif
    return ctx;
}

/* Purge all */
static bool
pdfi_fontdir_purge_all(const gs_memory_t * mem, cached_char * cc, void *dummy)
{
    return true;
}

int pdfi_free_context(gs_memory_t *pmem, pdf_context *ctx)
{
    if (ctx->cache_entries != 0) {
        pdf_obj_cache_entry *entry = ctx->cache_LRU, *next;

        while(entry) {
            next = entry->next;
            pdfi_countdown(entry->o);
            ctx->cache_entries--;
            gs_free_object(ctx->memory, entry, "pdfi_free_context, free LRU");
            entry = next;
        }
        ctx->cache_LRU = ctx->cache_MRU = NULL;
        ctx->cache_entries = 0;
    }

    if (ctx->PDFPassword)
        gs_free_object(ctx->memory, ctx->PDFPassword, "pdfi_free_context");

    if (ctx->PageList)
        gs_free_object(ctx->memory, ctx->PageList, "pdfi_free_context");

    if (ctx->Trailer)
        pdfi_countdown(ctx->Trailer);

    if(ctx->Root)
        pdfi_countdown(ctx->Root);

    if (ctx->Info)
        pdfi_countdown(ctx->Info);

    if (ctx->Pages)
        pdfi_countdown(ctx->Pages);

    if (ctx->xref_table) {
        pdfi_countdown(ctx->xref_table);
        ctx->xref_table = NULL;
    }

    if (ctx->stack_bot) {
        pdfi_clearstack(ctx);
        gs_free_object(ctx->memory, ctx->stack_bot, "pdfi_free_context");
    }

    if (ctx->filename) {
        gs_free_object(ctx->memory, ctx->filename, "pdfi_free_context, free copy of filename");
        ctx->filename = NULL;
    }

    if (ctx->main_stream != NULL) {
        sfclose(ctx->main_stream->s);
        ctx->main_stream = NULL;
    }

    if (ctx->memory->gs_lib_ctx->font_dir) {
        gx_purge_selected_cached_chars(ctx->memory->gs_lib_ctx->font_dir, pdfi_fontdir_purge_all, (void *)NULL);
        gs_free_object(ctx->memory, ctx->memory->gs_lib_ctx->font_dir, "pdfi_free_context");
    }

    if(ctx->pgs != NULL) {
        gs_gstate_free(ctx->pgs);
        ctx->pgs = NULL;
    }

    if (ctx->pdfi_param_list.head != NULL)
        gs_c_param_list_release(&ctx->pdfi_param_list);

    gs_free_object(ctx->memory, ctx, "pdfi_free_context");
    return 0;
}
