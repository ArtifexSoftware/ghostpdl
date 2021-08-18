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

/* Device to implement N-up printing */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gpmisc.h"
#include "gsparam.h"
#include "gxdevice.h"
#include "gsdevice.h"		/* requires gsmatrix.h */
#include "gxiparam.h"		/* for image source size */
#include "gxgstate.h"
#include "gxpaint.h"
#include "gxpath.h"
#include "gxcpath.h"
#include "gsstype.h"
#include "gdevprn.h"
#include "gdevp14.h"        /* Needed to patch up the procs after compositor creation */
#include "gdevsclass.h"
#include "gxdevsop.h"
#include "gdevnup.h"

/* GC descriptor */
#define public_st_nup_device()	/* in gsdevice.c */\
  gs_public_st_complex_only(st_nup_device, gx_device, "Nup Device",\
    0, nup_device_enum_ptrs, nup_device_reloc_ptrs, default_subclass_finalize)

static
ENUM_PTRS_WITH(nup_device_enum_ptrs, gx_device *dev);
return 0; /* default case */
case 0:ENUM_RETURN(gx_device_enum_ptr(dev->parent));
case 1:ENUM_RETURN(gx_device_enum_ptr(dev->child));
ENUM_PTRS_END
static RELOC_PTRS_WITH(nup_device_reloc_ptrs, gx_device *dev)
{
    dev->parent = gx_device_reloc_ptr(dev->parent, gcst);
    dev->child = gx_device_reloc_ptr(dev->child, gcst);
}
RELOC_PTRS_END

public_st_nup_device();


/**************************************************************************************/
/* Externals not in headers                                                           */
/* Imported from gsdparam.c                                                           */
extern void rc_free_NupControl(gs_memory_t * mem, void *ptr_in, client_name_t cname);
/**************************************************************************************/

/* This device is one of the 'subclassing' devices, part of a chain or pipeline
 * of devices, each of which can process some aspect of the graphics methods
 * before passing them on to the next device in the chain.
 *
 * This operates by hooking the device procs:
 *    get_initial_matrix	To modify the scale and origin of the sub-page
 *    fillpage 			To prevent erasing previously imaged sub-pages
 *    output_page		To ignore all output_page calls until all sub-pages
 *				have been imaged
 *    close_device		To output the final (partial) collection of sub-pages
 */

/* Device procedures */
static dev_proc_get_initial_matrix(nup_get_initial_matrix);
static dev_proc_close_device(nup_close_device);
static dev_proc_put_params(nup_put_params);
static dev_proc_output_page(nup_output_page);
static dev_proc_fillpage(nup_fillpage);
static dev_proc_dev_spec_op(nup_dev_spec_op);

/* The device prototype */

#define MAX_COORD (max_int_in_fixed - 1000)
#define MAX_RESOLUTION 4000

static void
nup_initialize_device_procs(gx_device *dev)
{
    default_subclass_initialize_device_procs(dev);

    set_dev_proc(dev, get_initial_matrix, nup_get_initial_matrix);
    set_dev_proc(dev, output_page, nup_output_page);
    set_dev_proc(dev, close_device, nup_close_device);
    set_dev_proc(dev, put_params, nup_put_params); /* to catch PageSize changes */
    set_dev_proc(dev, fillpage, nup_fillpage);
    set_dev_proc(dev, dev_spec_op, nup_dev_spec_op);
}

const
gx_device_nup gs_nup_device =
{
    /*
     * Define the device as 8-bit gray scale to avoid computing halftones.
     */
    std_device_dci_type_body_sc(gx_device_nup, nup_initialize_device_procs,
                        "N-up", &st_nup_device,
                        MAX_COORD, MAX_COORD,
                        MAX_RESOLUTION, MAX_RESOLUTION,
                        1, 8, 255, 0, 256, 1, NULL, NULL, NULL)
};

#undef MAX_COORD
#undef MAX_RESOLUTION

static void
nup_disable_nesting(Nup_device_subclass_data *pNup_data)
{
    /* set safe non-nesting defaults if we don't know the size of the Nested Page */
    pNup_data->PagesPerNest = 1;
    pNup_data->NupH = 1;
    pNup_data->NupV = 1;
    pNup_data->Scale = 1.0;
    pNup_data->PageCount = 0;
}

static int
ParseNupControl(gx_device *dev, Nup_device_subclass_data *pNup_data)
{
    int code = 0;
    float HScale, VScale;

    /* Make sure PageW and PageH are set -- from dev->width, dev->height */
    pNup_data->PageW = dev->width * 72.0 / dev->HWResolution[0];
    pNup_data->PageH = dev->height * 72.0 / dev->HWResolution[1];

    /* pNup_data->NestedPage[WH] size is set by nup_put_params by PageSize or .MediaSize */
    if (dev->NupControl== NULL) {
        nup_disable_nesting(pNup_data);
        return 0;
    }
    /* First parse the NupControl string for our parameters */
    if (sscanf(dev->NupControl->nupcontrol_str, "%dx%d", &(pNup_data->NupH), &(pNup_data->NupV)) != 2) {
        emprintf1(dev->memory, "*** Invalid NupControl format '%s'\n", dev->NupControl->nupcontrol_str);
        nup_disable_nesting(pNup_data);
        return_error(gs_error_unknownerror);
    }
    pNup_data->PagesPerNest = pNup_data->NupH * pNup_data->NupV;

    /* -dNupControl=1x1 effectively turns off nesting */
    if (pNup_data->PagesPerNest == 1) {
        nup_disable_nesting(pNup_data);
        return 0;
    }
    if (pNup_data->NestedPageW == 0.0 || pNup_data->NestedPageH == 0.0) {
        pNup_data->NestedPageW = pNup_data->PageW;
        pNup_data->NestedPageH = pNup_data->PageH;
    }
    /* Calculate based on the PageW and PageH and NestedPage size	*/
    /* Set HSize, VSize, Scale, HMargin, and VMargin			*/

    HScale = pNup_data->PageW / (pNup_data->NestedPageW * pNup_data->NupH);
    VScale = pNup_data->PageH / (pNup_data->NestedPageH * pNup_data->NupV);
    if (HScale < VScale) {
        pNup_data->Scale = HScale;
        pNup_data->HMargin = 0.0;
        pNup_data->VMargin = (pNup_data->PageH - (HScale * pNup_data->NestedPageH * pNup_data->NupV))/2.0;
    } else {
        pNup_data->Scale = VScale;
        pNup_data->VMargin = 0.0;
        pNup_data->HMargin = (pNup_data->PageW - (VScale * pNup_data->NestedPageW * pNup_data->NupH))/2.0;
    }
    pNup_data->HSize = pNup_data->NestedPageW * pNup_data->Scale;
    pNup_data->VSize = pNup_data->NestedPageH * pNup_data->Scale;

    return code;
}

static void
nup_get_initial_matrix(gx_device *dev, gs_matrix *pmat)
{
    int code = 0, Hindex, Vindex;
    Nup_device_subclass_data *pNup_data = dev->subclass_data;

    if (pNup_data->PagesPerNest == 0)		/* not yet initialized */
        code = ParseNupControl(dev, pNup_data);

    default_subclass_get_initial_matrix(dev, pmat);	/* get the matrix from the device */
    if (code < 0)
        return;

    if (pNup_data->PagesPerNest == 1)
        return;						/* nesting disabled */

    /* Modify the matrix according to N-up nesting paramters */
    pmat->tx += pNup_data->HMargin * pmat->xx;
    pmat->ty += pNup_data->VMargin * pmat->yy;		/* ty is the bottom */

    Hindex = imod(pNup_data->PageCount, pNup_data->NupH);
    Vindex = pNup_data->PageCount/pNup_data->NupH;
    Vindex = pNup_data->NupV - (imod(Vindex, pNup_data->NupV) + 1);	/* rows from top down */

    pmat->tx += pNup_data->HSize * Hindex * pmat->xx;
    pmat->tx += pNup_data->VSize * Vindex * pmat->xy;

    pmat->ty += pNup_data->HSize * Hindex * pmat->yx;
    pmat->ty += pNup_data->VSize * Vindex * pmat->yy;

    pmat->xx *= pNup_data->Scale;
    pmat->xy *= pNup_data->Scale;
    pmat->yx *= pNup_data->Scale;
    pmat->yy *= pNup_data->Scale;

    return;
}

/* Used to set/resest child device's MediaSize which is needed around output_page */
static void
nup_set_children_MediaSize(gx_device *dev, float PageW, float PageH)
{
    do {
        dev = dev->child;
        dev->MediaSize[0] = PageW;
        dev->MediaSize[1] = PageH;
    } while (dev->child != NULL);
    return;
}

static int
nup_flush_nest_to_output(gx_device *dev, Nup_device_subclass_data *pNup_data)
{
    int code = 0;

    nup_set_children_MediaSize(dev, pNup_data->PageW, pNup_data->PageH);
    code = default_subclass_output_page(dev, 1, true);
    nup_set_children_MediaSize(dev, pNup_data->NestedPageW, pNup_data->NestedPageH);

    pNup_data->PageCount = 0;
    return code;
}

static int
nup_close_device(gx_device *dev)
{
    int code = 0, acode = 0;
    Nup_device_subclass_data *pNup_data = dev->subclass_data;

    if (pNup_data->PagesPerNest == 0)
        code = ParseNupControl(dev, pNup_data);
    if (code < 0)
        return code;

    if (pNup_data->PageCount > 0)
        acode = nup_flush_nest_to_output(dev, pNup_data);

    /* Reset the Nup control data */
    /* NB: the data will be freed from non_gc_memory by the finalize function */
    memset(pNup_data, 0, sizeof(Nup_device_subclass_data));

    /* close children devices, even if there was an error from flush (acode < 0) */
    code = default_subclass_close_device(dev);

    return min(code, acode);
}

/* Read .MediaSize or, if supported as a synonym, PageSize. */
static int
param_MediaSize(gs_param_list * plist, gs_param_name pname,
                const float *res, gs_param_float_array * pa)
{
    gs_param_name param_name;
    int ecode = 0;
    int code;

    switch (code = param_read_float_array(plist, (param_name = pname), pa)) {
    case 0:
        if (pa->size != 2) {
          ecode = gs_note_error(gs_error_rangecheck);
          pa->data = 0;	/* mark as not filled */
        } else {
            float width_new = pa->data[0] * res[0] / 72;
            float height_new = pa->data[1] * res[1] / 72;

            if (width_new < 0 || height_new < 0)
                ecode = gs_note_error(gs_error_rangecheck);
#define max_coord (max_fixed / fixed_1)
#if max_coord < max_int
            else if (width_new > (long)max_coord || height_new > (long)max_coord)
                ecode = gs_note_error(gs_error_limitcheck);
#endif
#undef max_coord
            else
                break;
        }
        goto err;
    default:
        ecode = code;
err:	param_signal_error(plist, param_name, ecode);
        /* fall through */
    case 1:
        pa->data = 0;		/* mark as not filled */
    }
    return ecode;
}

/* Horrible hacked version of param_list_copy from gsparamx.c.
 * Copy one parameter list to another, recursively if necessary,
 * rewriting PageUsesTransparency to be true if it occurs. */
static int
copy_and_modify_sub(gs_param_list *plto, gs_param_list *plfrom, int *present)
{
    gs_param_enumerator_t key_enum;
    gs_param_key_t key;
    bool copy_persists;
    int code;

    if (present)
        *present = 0;
    if (plfrom == NULL)
        return 0;

    /* If plfrom and plto use different allocators, we must copy
     * aggregate values even if they are "persistent". */
    copy_persists = plto->memory == plfrom->memory;

    param_init_enumerator(&key_enum);
    while ((code = param_get_next_key(plfrom, &key_enum, &key)) == 0) {
        char string_key[256];	/* big enough for any reasonable key */
        gs_param_typed_value value;
        gs_param_collection_type_t coll_type;
        gs_param_typed_value copy;

        if (key.size > sizeof(string_key) - 1) {
            code = gs_note_error(gs_error_rangecheck);
            break;
        }
        memcpy(string_key, key.data, key.size);
        string_key[key.size] = 0;
        if ((code = param_read_typed(plfrom, string_key, &value)) != 0) {
            code = (code > 0 ? gs_note_error(gs_error_unknownerror) : code);
            break;
        }
        gs_param_list_set_persistent_keys(plto, key.persistent);
        switch (value.type) {
        case gs_param_type_dict:
            coll_type = gs_param_collection_dict_any;
            goto cc;
        case gs_param_type_dict_int_keys:
            coll_type = gs_param_collection_dict_int_keys;
            goto cc;
        case gs_param_type_array:
            coll_type = gs_param_collection_array;
        cc:
            copy.value.d.size = value.value.d.size;
            if (copy.value.d.size == 0)
                break;
            if ((code = param_begin_write_collection(plto, string_key,
                                                     &copy.value.d,
                                                     coll_type)) < 0 ||
                (code = copy_and_modify_sub(copy.value.d.list,
                                            value.value.d.list,
                                            NULL)) < 0 ||
                (code = param_end_write_collection(plto, string_key,
                                                   &copy.value.d)) < 0)
                break;
            code = param_end_read_collection(plfrom, string_key,
                                             &value.value.d);
            break;
        case gs_param_type_bool:
            if (strcmp(string_key, "PageUsesTransparency") == 0 && present != NULL)
            {
                value.value.b = 1;
                *present = 1;
            }
            goto ca;
        case gs_param_type_string:
            value.value.s.persistent &= copy_persists; goto ca;
        case gs_param_type_name:
            value.value.n.persistent &= copy_persists; goto ca;
        case gs_param_type_int_array:
            value.value.ia.persistent &= copy_persists; goto ca;
        case gs_param_type_float_array:
            value.value.fa.persistent &= copy_persists; goto ca;
        case gs_param_type_string_array:
            value.value.sa.persistent &= copy_persists;
            /* fall through */
        ca:
        default:
            code = param_write_typed(plto, string_key, &value);
        }
        if (code < 0)
            break;
    }
    return code;
}

static int
param_list_copy_and_modify(gs_param_list *plto, gs_param_list *plfrom)
{
    int found_put;
    int code = copy_and_modify_sub(plto, plfrom, &found_put);

    if (code >= 0 && !found_put) {
        gs_param_typed_value value;
        value.type = gs_param_type_bool;
        value.value.b = 1;
        code = param_write_typed(plto, "PageUsesTransparency", &value);
    }

    return code;
}

static int
promote_errors(gs_param_list * plist_orig, gs_param_list * plist)
{
    gs_param_enumerator_t key_enum;
    gs_param_key_t key;
    int code;
    int error;

    param_init_enumerator(&key_enum);
    while ((code = param_get_next_key(plist_orig, &key_enum, &key)) == 0) {
        char string_key[256];	/* big enough for any reasonable key */

        if (key.size > sizeof(string_key) - 1) {
            code = gs_note_error(gs_error_rangecheck);
            break;
        }
        memcpy(string_key, key.data, key.size);
        string_key[key.size] = 0;
        error = param_read_signalled_error(plist, string_key);
        param_signal_error(plist_orig, string_key, error);
    }

    return code;
}

static int
nup_put_params(gx_device *dev, gs_param_list * plist_orig)
{
    int code, ecode = 0;
    gs_param_float_array msa;
    const float *data;
    const float *res = dev->HWResolution;
    gs_param_string nuplist;
    Nup_device_subclass_data* pNup_data = dev->subclass_data;
    gx_device *next_dev;
    gs_c_param_list *plist_c;
    gs_param_list *plist;

#if 0000
gs_param_list_dump(plist_orig);
#endif

    plist_c = gs_c_param_list_alloc(dev->memory->non_gc_memory, "nup_put_params");
    plist = (gs_param_list *)plist_c;
    if (plist == NULL)
        return_error(gs_error_VMerror);
    gs_c_param_list_write(plist_c, dev->memory->non_gc_memory);
    gs_param_list_set_persistent_keys((gs_param_list *)plist_c, false);

    /* Bulk copy the whole list. Can't enumerate and copy without it
     * becoming an absolute nightmare due to the stupid way we handle
     * 'collection' objects on writing. */
    code = param_list_copy_and_modify((gs_param_list *)plist_c, plist_orig);
    if (code < 0)
        goto fail;

    gs_c_param_list_read(plist_c);

    code = param_read_string(plist, "NupControl", &nuplist);
    if (code < 0)
        ecode = code;

    if (code == 0) {
        if (dev->NupControl && (nuplist.size == 0 ||
            (strncmp(dev->NupControl->nupcontrol_str, (const char *)nuplist.data, nuplist.size) != 0))) {
            /* If we have accumulated a nest when the NupControl changes, flush the nest */
            if (pNup_data->PagesPerNest > 1 && pNup_data->PageCount > 0)
                code = nup_flush_nest_to_output(dev, pNup_data);
            if (code < 0)
                ecode = code;
            /* There was a NupControl, but this one is different -- no longer use the old one */
            rc_decrement(dev->NupControl, "default put_params NupControl");
            dev->NupControl = 0;
        }
        if (dev->NupControl == NULL && nuplist.size > 0) {
            dev->NupControl = (gdev_nupcontrol *)gs_alloc_bytes(dev->memory->non_gc_memory,
                                                              sizeof(gdev_nupcontrol), "structure to hold nupcontrol_str");
            if (dev->NupControl == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto fail;
            }
            dev->NupControl->nupcontrol_str = (void *)gs_alloc_bytes(dev->memory->non_gc_memory,
                                                                     nuplist.size + 1, "nupcontrol string");
            if (dev->NupControl->nupcontrol_str == NULL){
                gs_free(dev->memory->non_gc_memory, dev->NupControl, 1, sizeof(gdev_nupcontrol),
                        "free structure to hold nupcontrol string");
                dev->NupControl = 0;
                code = gs_note_error(gs_error_VMerror);
                goto fail;
            }
            memset(dev->NupControl->nupcontrol_str, 0x00, nuplist.size + 1);
            memcpy(dev->NupControl->nupcontrol_str, nuplist.data, nuplist.size);
            rc_init_free(dev->NupControl, dev->memory->non_gc_memory, 1, rc_free_NupControl);
        }
        /* Propagate the new NupControl struct to children so get_params has a valid param */
        next_dev = dev->child;
        while (next_dev != NULL) {
            rc_decrement(next_dev->NupControl, "nup_put_params");
            next_dev->NupControl = dev->NupControl;
            rc_increment(next_dev->NupControl);
            next_dev = next_dev->child;
        }
        /* Propagate the new NupControl struct to parents so get_params has a valid param */
        next_dev = dev->parent;
        while (next_dev != NULL) {
            rc_decrement(next_dev->NupControl, "nup_put_params");
            next_dev->NupControl = dev->NupControl;
            rc_increment(next_dev->NupControl);
            next_dev = next_dev->parent;
        }
        if (ecode < 0) {
            code = ecode;
            goto fail;
        }
    }

    code = ParseNupControl(dev, pNup_data);		/* update the nesting params */
    if (code < 0)
        goto fail;

    /* If nesting is now off, just pass params on to children devices */
    if (pNup_data->PagesPerNest == 1) {
        code = default_subclass_put_params(dev, plist);
        goto fail; /* Not actually failing! */
    }

    /* .MediaSize takes precedence over PageSize, so we read PageSize first. */
    code = param_MediaSize(plist, "PageSize", res, &msa);
    if (code < 0)
        ecode = code;
    /* Prevent data from being set to 0 if PageSize is specified */
    /* but .MediaSize is not. */
    data = msa.data;
    code = param_MediaSize(plist, ".MediaSize", res, &msa);
    if (code < 0)
        ecode = code;
    else if (msa.data == NULL)
        msa.data = data;
    if (ecode < 0) {
        code = ecode;
        goto fail;
    }

    /* If there was PageSize or .MediaSize, update the NestedPage size */
    if (msa.data != NULL) {
        Nup_device_subclass_data *pNup_data = dev->subclass_data;
        /* Calculate the page sizes as ints to allow for tiny changes
         * of width that don't actually make a difference. */
        int w1 = (int)(pNup_data->NestedPageW * dev->HWResolution[0] / 72.0f + 0.5f);
        int w2 = (int)(msa.data[0]            * dev->HWResolution[0] / 72.0f + 0.5f);
        int h1 = (int)(pNup_data->NestedPageH * dev->HWResolution[1] / 72.0f + 0.5f);
        int h2 = (int)(msa.data[1]            * dev->HWResolution[1] / 72.0f + 0.5f);

        /* FIXME: Handle changing size (if previous value was non-zero) */
        if (w1 != w2 || h1 != h2) {
            /* If needed, flush previous nest before changing */
            if (pNup_data->PageCount > 0 && pNup_data->PagesPerNest > 1) {
                code = nup_flush_nest_to_output(dev, pNup_data);
                if (code < 0)
                    goto fail;
            }
            pNup_data->NestedPageW = msa.data[0];
            pNup_data->NestedPageH = msa.data[1];
            /* And update the Nup parameters based on the updated PageSize */
            code = ParseNupControl(dev, pNup_data);
            if (code < 0)
                goto fail;
        }
    }

    /* now that we've intercepted PageSize and/or MediaSize, pass the rest along */
    code = default_subclass_put_params(dev, plist);

    /* Now promote errors from the copied list to the original list. */
    {
        int ecode = promote_errors(plist_orig, plist);
        if (code == 0)
            code = ecode;
    }

fail:
    gs_c_param_list_release(plist_c);
    gs_free_object(dev->memory->non_gc_memory, plist_c, "nup_put_params");

    return code;
}

static int
nup_output_page(gx_device *dev, int num_copies, int flush)
{
    int code = 0;
    Nup_device_subclass_data *pNup_data = dev->subclass_data;

    if (pNup_data->PagesPerNest == 0)
        code = ParseNupControl(dev, pNup_data);	/* ensure reasonable values */
    if (code < 0)
        return code;

    /* If nesting is off, pass output_page to children */
    if (pNup_data->PagesPerNest == 1) {
        code = default_subclass_output_page(dev, num_copies, flush);
        dev->PageCount = dev->child->PageCount;
        dev->ShowpageCount = dev->child->ShowpageCount;
        return code;
    }

    /* FIXME: Handle num_copies > 1 */

    /* pNup_data holds the number of 'sub pages' we have produced,
     * so update that. dev->PageCount holds the number of 'actual'
     * pages we've output, so only increment that if we really
     * do an output. */
    pNup_data->PageCount++;
    dev->ShowpageCount = dev->child->ShowpageCount;
    if (pNup_data->PageCount >= pNup_data->PagesPerNest) {
        code = nup_flush_nest_to_output(dev, pNup_data);
        /* Increment this afterwards, in case the child accesses
         * the value to fill in a %d in the filename. */
        dev->PageCount++;
    }

    return code;
}

static int
nup_fillpage(gx_device *dev, gs_gstate * pgs, gx_device_color *pdevc)
{
    int code = 0;
    Nup_device_subclass_data *pNup_data = dev->subclass_data;

    if (pNup_data->PagesPerNest == 0)
        code = ParseNupControl(dev, pNup_data);
    if (code < 0)
        return code;

    /* Only fill the first page of a nest */
    if (pNup_data->PageCount == 0)
        code = default_subclass_fillpage(dev, pgs, pdevc);

    return code;
}

static int
nup_dev_spec_op(gx_device *dev, int dev_spec_op, void *data, int size)
{
    Nup_device_subclass_data *pNup_data = dev->subclass_data;
    int code = 0;

    if (pNup_data->PagesPerNest == 0)		/* not yet initialized */
        code = ParseNupControl(dev, pNup_data);
    if (code < 0)
        return code;

    /* If nesting is now off, just pass spec_op on to children devices */
    if (pNup_data->PagesPerNest == 1)
        return default_subclass_dev_spec_op(dev, dev_spec_op, data, size);

    switch (dev_spec_op) {
        case gxdso_set_HWSize:
            /* Call ParseNupControl since that will set the PageW and PageH from the	*/
            /* dev->width, dev->height as the size for the page on which we are placing	*/
            /* the nested pages. If we get here we know PagesPerNest > 0, so don't set	*/
            /* the dev->width and dev->height 						*/
            code = ParseNupControl(dev, pNup_data);
            if (code < 0)
                return code;
            return 1;
            break;
        case gxdso_get_dev_param:
            {
                dev_param_req_t *request = (dev_param_req_t *)data;
                int code = false;

                /* We need to disable pdfmark writing, primarily for CropBox, but also	*/
                /* they are probably not relevant for multiple input files to a single	*/
                /* output "page" (nest of several pages).				*/
                /* Write a 'false' (0) into the param list provided by the caller.	*/
                if (strcmp(request->Param, "PdfmarkCapable") == 0) {
                    return(param_write_bool(request->list, "PdfmarkCapable", &code));
                }
            }
            /* Fall through */
        default:
            break;
    }
    return default_subclass_dev_spec_op(dev, dev_spec_op, data, size);
}

int gx_device_nup_device_install(gx_device *dev)
{
    gs_param_typed_value value;
    gs_c_param_list *plist_c;
    int code;

    code = gx_device_subclass(dev, (gx_device *)&gs_nup_device, sizeof(Nup_device_subclass_data));
    if (code < 0)
        return code;

    /* Ensure that PageUsesTransparency is set. */
    plist_c = gs_c_param_list_alloc(dev->memory->non_gc_memory, "nup_open_device");
    if (plist_c == NULL)
        return_error(gs_error_VMerror);
    gs_c_param_list_write(plist_c, dev->memory->non_gc_memory);
    gs_param_list_set_persistent_keys((gs_param_list *)plist_c, false);

    value.type = gs_param_type_bool;
    value.value.b = 1;
    code = param_write_typed((gs_param_list *)plist_c, "PageUsesTransparency", &value);
    if (code >= 0) {
        gs_c_param_list_read(plist_c);

        code = default_subclass_put_params(dev, (gs_param_list *)plist_c);
        if (code >= 0)
            code = default_subclass_open_device(dev->child);
    }
    gs_c_param_list_release(plist_c);
    gs_free_object(dev->memory->non_gc_memory, plist_c, "nup_open_device");

    return code;
}
