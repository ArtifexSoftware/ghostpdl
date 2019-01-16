/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* gs_gstate housekeeping */
#include "gx.h"
#include "gserrors.h"
#include "gscspace.h"
#include "gscie.h"
#include "gsstruct.h"
#include "gsutil.h"		/* for gs_next_ids */
#include "gxbitmap.h"
#include "gxcmap.h"
#include "gxdht.h"
#include "gxgstate.h"
#include "gzht.h"
#include "gzline.h"
#include "gxfmap.h"
#include "gsicc_cache.h"
#include "gsicc_manage.h"
#include "gsicc_profilecache.h"

/******************************************************************************
 * See gsstate.c for a discussion of graphics state memory management. *
 ******************************************************************************/

/* Imported values */
/* The following should include a 'const', but for some reason */
/* the Watcom compiler won't accept it, even though it happily accepts */
/* the same construct everywhere else. */
extern /*const*/ gx_color_map_procs *const cmap_procs_default;

/* GC procedures for gx_line_params */
static
ENUM_PTRS_WITH(line_params_enum_ptrs, gx_line_params *plp) return 0;
    case 0: return ENUM_OBJ((plp->dash.pattern_size == 0 ?
                             NULL : plp->dash.pattern));
ENUM_PTRS_END
static RELOC_PTRS_WITH(line_params_reloc_ptrs, gx_line_params *plp)
{
    if (plp->dash.pattern_size)
        RELOC_VAR(plp->dash.pattern);
} RELOC_PTRS_END
private_st_line_params();

/*
 * GC procedures for gs_gstate
 *
 * See comments in gixstate.h before the definition of gs_cr_state_do_rc and
 * st_cr_state_num_ptrs for an explanation about why the effective_transfer
 * pointers are handled in this manner.
 */
public_st_gs_gstate();
static
ENUM_PTRS_WITH(gs_gstate_enum_ptrs, gs_gstate *gisvptr)
    ENUM_SUPER(gs_gstate, st_line_params, line_params, st_gs_gstate_num_ptrs - st_line_params_num_ptrs);
#define e1(i,elt) ENUM_PTR(i,gs_gstate,elt);
    gs_gstate_do_ptrs(e1)
#undef e1
#define E1(i,elt) ENUM_PTR(i + gs_gstate_num_ptrs,gs_gstate,elt);
    gs_cr_state_do_ptrs(E1)
#undef E1
    case (gs_gstate_num_ptrs + st_cr_state_num_ptrs): /* handle device specially */
        ENUM_RETURN(gx_device_enum_ptr(gisvptr->device));
ENUM_PTRS_END

static RELOC_PTRS_WITH(gs_gstate_reloc_ptrs, gs_gstate *gisvptr)
{
    RELOC_SUPER(gs_gstate, st_line_params, line_params);
#define r1(i,elt) RELOC_PTR(gs_gstate,elt);
    gs_gstate_do_ptrs(r1)
#undef r1
#define R1(i,elt) RELOC_PTR(gs_gstate,elt);
    gs_cr_state_do_ptrs(R1)
#undef R1

    gisvptr->device = gx_device_reloc_ptr(gisvptr->device, gcst);
    {
        int i = GX_DEVICE_COLOR_MAX_COMPONENTS - 1;

        for (; i >= 0; i--)
            RELOC_PTR(gs_gstate, effective_transfer[i]);
    }
} RELOC_PTRS_END

/* Initialize an gs_gstate, other than the parts covered by */
/* GS_STATE_INIT_VALUES(). */
int
gs_gstate_initialize(gs_gstate * pgs, gs_memory_t * mem)
{
    int i;
    pgs->memory = mem;
    pgs->client_data = 0;
    pgs->trans_device = 0;
    /* Color rendering state */
    pgs->halftone = 0;
    {
        int i;

        for (i = 0; i < gs_color_select_count; ++i)
            pgs->screen_phase[i].x = pgs->screen_phase[i].y = 0;
    }
    pgs->dev_ht = 0;
    pgs->cie_render = 0;
    pgs->cie_to_xyz = false;
    pgs->black_generation = 0;
    pgs->undercolor_removal = 0;
    /* Allocate an initial transfer map. */
    rc_alloc_struct_n(pgs->set_transfer.gray,
                      gx_transfer_map, &st_transfer_map,
                      mem, return_error(gs_error_VMerror),
                      "gs_gstate_init(transfer)", 1);
    pgs->set_transfer.gray->proc = gs_identity_transfer;
    pgs->set_transfer.gray->id = gs_next_ids(pgs->memory, 1);
    pgs->set_transfer.gray->values[0] = frac_0;
    pgs->set_transfer.red =
        pgs->set_transfer.green =
        pgs->set_transfer.blue = NULL;
    for (i = 0; i < GX_DEVICE_COLOR_MAX_COMPONENTS; i++)
        pgs->effective_transfer[i] = pgs->set_transfer.gray;
    pgs->cie_joint_caches = NULL;
    pgs->cie_joint_caches_alt = NULL;
    pgs->cmap_procs = cmap_procs_default;
    pgs->pattern_cache = NULL;
    pgs->have_pattern_streams = false;
    pgs->devicergb_cs = gs_cspace_new_DeviceRGB(mem);
    pgs->devicecmyk_cs = gs_cspace_new_DeviceCMYK(mem);
    if (pgs->devicergb_cs == NULL || pgs->devicecmyk_cs == NULL)
        return_error(gs_error_VMerror);
    pgs->icc_link_cache = gsicc_cache_new(pgs->memory);
    pgs->icc_manager = gsicc_manager_new(pgs->memory);
    pgs->icc_profile_cache = gsicc_profilecache_new(pgs->memory);
#if ENABLE_CUSTOM_COLOR_CALLBACK
    pgs->custom_color_callback = INIT_CUSTOM_COLOR_PTR;
#endif
    return 0;
}

/*
 * Make a temporary copy of a gs_gstate.  Note that this does not
 * do all the necessary reference counting, etc.  However, it does
 * clear out the transparency stack in the destination.
 */
gs_gstate *
gs_gstate_copy_temp(const gs_gstate * pgs, gs_memory_t * mem)
{
    gs_gstate *pgs_copy =
        gs_alloc_struct(mem, gs_gstate, &st_gs_gstate,
                        "gs_gstate_copy");

    if (pgs_copy) {
        *pgs_copy = *pgs;
    }
    return pgs_copy;
}

/* Increment reference counts to note that an gs_gstate has been copied. */
void
gs_gstate_copied(gs_gstate * pgs)
{
    rc_increment(pgs->halftone);
    rc_increment(pgs->dev_ht);
    rc_increment(pgs->cie_render);
    rc_increment(pgs->black_generation);
    rc_increment(pgs->undercolor_removal);
    rc_increment(pgs->set_transfer.gray);
    rc_increment(pgs->set_transfer.red);
    rc_increment(pgs->set_transfer.green);
    rc_increment(pgs->set_transfer.blue);
    rc_increment(pgs->cie_joint_caches);
    rc_increment(pgs->cie_joint_caches_alt);
    rc_increment(pgs->devicergb_cs);
    rc_increment(pgs->devicecmyk_cs);
    rc_increment(pgs->icc_link_cache);
    rc_increment(pgs->icc_profile_cache);
    rc_increment(pgs->icc_manager);
}

/* Adjust reference counts before assigning one gs_gstate to another. */
void
gs_gstate_pre_assign(gs_gstate *pto, const gs_gstate *pfrom)
{
    const char *const cname = "gs_gstate_pre_assign";

#define RCCOPY(element)\
    rc_pre_assign(pto->element, pfrom->element, cname)

    RCCOPY(cie_joint_caches);
    RCCOPY(cie_joint_caches_alt);
    RCCOPY(set_transfer.blue);
    RCCOPY(set_transfer.green);
    RCCOPY(set_transfer.red);
    RCCOPY(set_transfer.gray);
    RCCOPY(undercolor_removal);
    RCCOPY(black_generation);
    RCCOPY(cie_render);
    RCCOPY(dev_ht);
    RCCOPY(halftone);
    RCCOPY(devicergb_cs);
    RCCOPY(devicecmyk_cs);
    RCCOPY(icc_link_cache);
    RCCOPY(icc_profile_cache);
    RCCOPY(icc_manager);
#undef RCCOPY
}

/* Release a gs_gstate. */
void
gs_gstate_release(gs_gstate * pgs)
{
    const char *const cname = "gs_gstate_release";
    gx_device_halftone *pdht = pgs->dev_ht;

#define RCDECR(element)\
    rc_decrement(pgs->element, cname);\
    pgs->element = NULL	/* prevent subsequent decrements from this gs_gstate */

    RCDECR(cie_joint_caches);
    RCDECR(set_transfer.gray);
    RCDECR(set_transfer.blue);
    RCDECR(set_transfer.green);
    RCDECR(set_transfer.red);
    RCDECR(undercolor_removal);
    RCDECR(black_generation);
    RCDECR(cie_render);
    /*
     * If we're going to free the device halftone, make sure we free the
     * dependent structures as well.
     */
    if (pdht != 0 && pdht->rc.ref_count == 1) {
        gx_device_halftone_release(pdht, pdht->rc.memory);
    }
    RCDECR(dev_ht);
    RCDECR(halftone);
    RCDECR(devicergb_cs);
    RCDECR(devicecmyk_cs);
    RCDECR(icc_link_cache);
    RCDECR(icc_profile_cache);
    RCDECR(icc_manager);
#undef RCDECR
}
