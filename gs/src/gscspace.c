/* Copyright (C) 1998, 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Color space operators and support */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsccolor.h"
#include "gsutil.h"		/* for gs_next_ids */
#include "gxcmap.h"
#include "gxcspace.h"
#include "gxistate.h"
#include "gsovrc.h"
#include "gsstate.h"
#include "gsdevice.h"
#include "gxdevcli.h"

/*
 * Define the standard color space types.  We include DeviceCMYK in the base
 * build because it's too awkward to omit it, but we don't provide any of
 * the PostScript operator procedures (setcmykcolor, etc.) for dealing with
 * it.
 */
private const gs_color_space_type gs_color_space_type_DeviceGray = {
    gs_color_space_index_DeviceGray, true, true,
    &st_base_color_space, gx_num_components_1,
    gx_no_base_space,
    gx_init_paint_1, gx_restrict01_paint_1,
    gx_same_concrete_space,
    gx_concretize_DeviceGray, gx_remap_concrete_DGray,
    gx_remap_DeviceGray, gx_no_install_cspace,
    gx_spot_colors_set_overprint,
    gx_no_adjust_cspace_count, gx_no_adjust_color_count
};
private const gs_color_space_type gs_color_space_type_DeviceRGB = {
    gs_color_space_index_DeviceRGB, true, true,
    &st_base_color_space, gx_num_components_3,
    gx_no_base_space,
    gx_init_paint_3, gx_restrict01_paint_3,
    gx_same_concrete_space,
    gx_concretize_DeviceRGB, gx_remap_concrete_DRGB,
    gx_remap_DeviceRGB, gx_no_install_cspace,
    gx_spot_colors_set_overprint,
    gx_no_adjust_cspace_count, gx_no_adjust_color_count
};

private cs_proc_set_overprint(gx_set_overprint_DeviceCMYK);

private const gs_color_space_type gs_color_space_type_DeviceCMYK = {
    gs_color_space_index_DeviceCMYK, true, true,
    &st_base_color_space, gx_num_components_4,
    gx_no_base_space,
    gx_init_paint_4, gx_restrict01_paint_4,
    gx_same_concrete_space,
    gx_concretize_DeviceCMYK, gx_remap_concrete_DCMYK,
    gx_remap_DeviceCMYK, gx_no_install_cspace,
    gx_set_overprint_DeviceCMYK,
    gx_no_adjust_cspace_count, gx_no_adjust_color_count
};

/* Structure descriptors */
public_st_color_space();
public_st_base_color_space();


/* ------ Create/copy/destroy ------ */

void
gs_cspace_init(gs_color_space *pcs, const gs_color_space_type * pcstype,
	       gs_memory_t *mem)
{
    pcs->type = pcstype;
    pcs->pmem = mem;
    pcs->id = gs_next_ids(1);
}

int
gs_cspace_alloc(gs_color_space ** ppcspace,
		const gs_color_space_type * pcstype,
		gs_memory_t * mem)
{
    gs_color_space *pcspace =
	gs_alloc_struct(mem, gs_color_space, &st_color_space,
			"gs_cspace_alloc");

    if (pcspace == 0)
	return_error(gs_error_VMerror);
    if (pcstype != 0)
        gs_cspace_init(pcspace, pcstype, mem);
    *ppcspace = pcspace;
    return 0;
}

int
gs_cspace_init_DeviceGray(gs_color_space *pcs)
{
    /* parameterless color space; no re-entrancy problems */
    static gs_color_space  dev_gray_proto;

    if (dev_gray_proto.id == 0)
        gs_cspace_init( &dev_gray_proto,
                        &gs_color_space_type_DeviceGray,
                        NULL );
    *pcs = dev_gray_proto;
    return 0;
}
int
gs_cspace_build_DeviceGray(gs_color_space ** ppcspace, gs_memory_t * pmem)
{
    int     code = gs_cspace_alloc(ppcspace, NULL, pmem);

    if (code >= 0)
        code = gs_cspace_init_DeviceGray(*ppcspace);
    return code;
}

int
gs_cspace_init_DeviceRGB(gs_color_space *pcs)
{
    /* parameterless color space; no re-entrancy problems */
    static gs_color_space  dev_rgb_proto;

    if (dev_rgb_proto.id == 0)
        gs_cspace_init( &dev_rgb_proto,
                        &gs_color_space_type_DeviceRGB,
                        NULL );
    *pcs = dev_rgb_proto;
    return 0;
}
int
gs_cspace_build_DeviceRGB(gs_color_space ** ppcspace, gs_memory_t * pmem)
{
    int     code = gs_cspace_alloc(ppcspace, NULL, pmem);

    if (code >= 0)
        code = gs_cspace_init_DeviceRGB(*ppcspace);
    return code;
}

int
gs_cspace_init_DeviceCMYK(gs_color_space *pcs)
{
    /* parameterless color space; no re-entrancy problems */
    static gs_color_space  dev_cmyk_proto;

    if (dev_cmyk_proto.id == 0)
        gs_cspace_init( &dev_cmyk_proto,
                        &gs_color_space_type_DeviceCMYK,
                        NULL );
    *pcs = dev_cmyk_proto;
    return 0;
}
int
gs_cspace_build_DeviceCMYK(gs_color_space ** ppcspace, gs_memory_t * pmem)
{
    int     code = gs_cspace_alloc(ppcspace, NULL, pmem);

    if (code >= 0)
        code = gs_cspace_init_DeviceCMYK(*ppcspace);
    return code;
}

/*
 * Copy just enough of a color space object.  This will do the right thing
 * for copying color spaces into the base or alternate color space of a
 * compound color space when legal, but it can't check that the operation is
 * actually legal.
 */
inline private void
cs_copy(gs_color_space *pcsto, const gs_color_space *pcsfrom)
{
    memcpy(pcsto, pcsfrom, pcsfrom->type->stype->ssize);
}

/* Copy a color space into one newly allocated by the caller. */
void
gs_cspace_init_from(gs_color_space * pcsto, const gs_color_space * pcsfrom)
{
    cs_copy(pcsto, pcsfrom);
    (*pcsto->type->adjust_cspace_count)(pcsto, 1);
}

/* Assign a color space into a previously initialized one. */
void
gs_cspace_assign(gs_color_space * pdest, const gs_color_space * psrc)
{
    /* check for a = a */
    if (pdest == psrc)
	return;
    (*psrc->type->adjust_cspace_count)(psrc, 1);
    (*pdest->type->adjust_cspace_count)(pdest, -1);
    cs_copy(pdest, psrc);
}


/* Prepare to free a color space. */
void
gs_cspace_release(gs_color_space * pcs)
{
    (*pcs->type->adjust_cspace_count)(pcs, -1);
}

/* ------ Accessors ------ */

/* Get the index of a color space. */
gs_color_space_index
gs_color_space_get_index(const gs_color_space * pcs)
{
    return pcs->type->index;
}

/* Get the number of components in a color space. */
int
gs_color_space_num_components(const gs_color_space * pcs)
{
    return cs_num_components(pcs);
}

/* Restrict a color to its legal range. */
void
gs_color_space_restrict_color(gs_client_color *pcc, const gs_color_space *pcs)
{
    cs_restrict_color(pcc, pcs);
}

int
gx_num_components_1(const gs_color_space * pcs)
{
    return 1;
}
int
gx_num_components_3(const gs_color_space * pcs)
{
    return 3;
}
int
gx_num_components_4(const gs_color_space * pcs)
{
    return 4;
}

/*
 * For color spaces that have a base or alternative color space, return that
 * color space. Otherwise return null.
 */
const gs_color_space *
gs_cspace_base_space(const gs_color_space * pcspace)
{
    return cs_base_space(pcspace);
}

const gs_color_space *
gx_no_base_space(const gs_color_space * pcspace)
{
    return NULL;
}

/* ------ Other implementation procedures ------ */

/* Null color space installation procedure. */
int
gx_no_install_cspace(const gs_color_space * pcs, gs_state * pgs)
{
    return 0;
}

/*
 * Push an overprint compositor onto the current device indicating that,
 * at most, the spot color parameters are to be preserved.
 *
 * This routine should be used for all Device, CIEBased, and ICCBased
 * color spaces, except for DeviceCMKY. The latter color space requires a
 * special verson that supports overprint mode.
 */
int
gx_spot_colors_set_overprint(const gs_color_space * pcs, gs_state * pgs)
{
    gs_imager_state *       pis = (gs_imager_state *)pgs;
    gs_overprint_params_t   params;

    if ((params.retain_any_comps = pis->overprint)) {
        params.retain_spot_comps = true;
        params.retain_zeroed_comps = false;
    }
    return gs_state_update_overprint(pgs, &params);
}


private bool
check_single_comp(int comp, frac targ_val, int ncomps, const frac * pval)
{
    int     i;

    for (i = 0; i < ncomps; i++) {
        if ( (i != comp && pval[i] != frac_0)  ||
             (i == comp && pval[i] != targ_val)  )
            return false;
    }
    return true;
}

private bool
gx_is_cmyk_color_model(const gs_state * pgs)
{
    gx_device *                     dev = gs_currentdevice(pgs);
    int                             ncomps = dev->color_info.num_components;
    int                             cyan_c, magenta_c, yellow_c, black_c;
    const gx_cm_color_map_procs *   pprocs;
    cm_map_proc_cmyk((*map_cmyk));
    frac                            frac_14 = frac_1 / 4;
    frac                            out[GX_DEVICE_COLOR_MAX_COMPONENTS];

    /* check for the appropriate components */
    if ( ncomps < 4                                       ||
         (cyan_c = dev_proc(dev, get_color_comp_index)(
                       dev,
                       "Cyan",
                       sizeof("Cyan") - 1,
                       -1 )) < 0                          ||
         cyan_c == GX_DEVICE_COLOR_MAX_COMPONENTS         ||
         (magenta_c = dev_proc(dev, get_color_comp_index)(
                          dev,
                          "Magenta",
                          sizeof("Magenta") - 1,
                          -1 )) < 0                       ||
         magenta_c == GX_DEVICE_COLOR_MAX_COMPONENTS      ||
         (yellow_c = dev_proc(dev, get_color_comp_index)(
                        dev,
                        "Yellow",
                        sizeof("Yellow") - 1,
                        -1 )) < 0                         ||
         yellow_c == GX_DEVICE_COLOR_MAX_COMPONENTS       ||
         (black_c = dev_proc(dev, get_color_comp_index)(
                        dev,
                        "Black",
                        sizeof("Black") - 1,
                        -1 )) < 0                         ||
         black_c == GX_DEVICE_COLOR_MAX_COMPONENTS          )
        return false;

    /* check the mapping */
    if ( (pprocs = dev_proc(dev, get_color_mapping_procs)(dev)) == 0 ||
         (map_cmyk = pprocs->map_cmyk) == 0                            )
        return false;

    map_cmyk(dev, frac_14, frac_0, frac_0, frac_0, out);
    if (!check_single_comp(cyan_c, frac_14, ncomps, out))
        return false;
    map_cmyk(dev, frac_0, frac_14, frac_0, frac_0, out);
    if (!check_single_comp(magenta_c, frac_14, ncomps, out))
        return false;
    map_cmyk(dev, frac_0, frac_0, frac_14, frac_0, out);
    if (!check_single_comp(yellow_c, frac_14, ncomps, out))
        return false;
    map_cmyk(dev, frac_0, frac_0, frac_0, frac_14, out);
    if (!check_single_comp(black_c, frac_14, ncomps, out))
        return false;

    return true;
}

private int
gx_set_overprint_DeviceCMYK(const gs_color_space * pcs, gs_state * pgs)
{
    gs_imager_state *   pis = (gs_imager_state *)pgs;

    /* check if we require special handling */
    if ( !pis->overprint             ||
         pis->overprint_mode != 1    ||
         !gx_is_cmyk_color_model(pgs)  )
        return gx_spot_colors_set_overprint(pcs, pgs);
    else {
        gs_overprint_params_t   params;

        /* we known everything is true */
        params.retain_any_comps = true;
        params.retain_spot_comps = true;
        params.retain_zeroed_comps = true;
        return gs_state_update_overprint(pgs, &params);
    }
}


/*
 * Push the overprint compositor appropriate for the current color map
 * onto the current device. This procedure is used for Separation and
 * DeviceN color spaces that are supported in native mode (and, for
 * Separation, do not involve component "All" or "None").
 */
int
gx_comp_map_set_overprint(const gs_color_space * pcs, gs_state * pgs)
{
    gs_imager_state *       pis = (gs_imager_state *)pgs;
    gs_devicen_color_map *  pcmap = &pis->color_component_map;
    gs_overprint_params_t   params;

    params.retain_any_comps = pis->overprint && pcmap->sep_type != SEP_ALL;
    if (params.retain_any_comps) {
        params.retain_zeroed_comps = false;
        if (!(params.retain_spot_comps = pcmap->use_alt_cspace)) {
            gs_overprint_clear_all_drawn_comps(&params);
            if (pcmap->sep_type != SEP_NONE) {
                int     i, ncomps = pcmap->num_components;

                for (i = 0; i < ncomps; i++) {
                    int     mcomp = pis->color_component_map.color_map[i];

                    if (mcomp >= 0)
                        gs_overprint_set_drawn_comp(&params, mcomp);
                }
            }
        }
    }

    return gs_state_update_overprint(pgs, &params);
}


/* Null reference count adjustment procedure. */
void
gx_no_adjust_cspace_count(const gs_color_space * pcs, int delta)
{
}

/* GC procedures */

private 
ENUM_PTRS_BEGIN_PROC(color_space_enum_ptrs)
{
    EV_CONST gs_color_space *pcs = vptr;

    return ENUM_USING(*pcs->type->stype, vptr, size, index);
    ENUM_PTRS_END_PROC
}
private 
RELOC_PTRS_WITH(color_space_reloc_ptrs, gs_color_space *pcs)
{
    RELOC_USING(*pcs->type->stype, vptr, size);
}
RELOC_PTRS_END
