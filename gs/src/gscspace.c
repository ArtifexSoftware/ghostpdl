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
#include "gzstate.h"
#include "stream.h"

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
    gx_no_adjust_cspace_count, gx_no_adjust_color_count,
    gx_serialize_cspace_type
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
    gx_no_adjust_cspace_count, gx_no_adjust_color_count,
    gx_serialize_cspace_type
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
    gx_no_adjust_cspace_count, gx_no_adjust_color_count,
    gx_serialize_cspace_type
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

    if ((params.retain_any_comps = pis->overprint))
        params.retain_spot_comps = true;
    pgs->effective_overprint_mode = 0;
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

/*
 * Determine if the current color model is a "DeviceCMYK" color model, and
 * if so what are its process color components. This information is required
 * only if overprint is true and overprint mode is set to 1.
 *
 * A color model is considered a "DeviceCMYK" color model if it supports the
 * cyan, magenta, yellow, and black color components, and maps the DeviceCMYK
 * color model components directly to these color components. Note that this
 * does not require any particular component order, allows for additional
 * spot color components, and does admit DeviceN color spaces if they have
 * the requisite behavior.
 *
 * If the color model is a "DeviceCMYK" color model, return the set of
 * process color components; otherwise return 0.
 */
private gx_color_index
check_cmyk_color_model_comps(gx_device * dev)
{
    gx_device_color_info *          pcinfo = &dev->color_info;
    int                             ncomps = pcinfo->num_components;
    int                             cyan_c, magenta_c, yellow_c, black_c;
    const gx_cm_color_map_procs *   pprocs;
    cm_map_proc_cmyk((*map_cmyk));
    frac                            frac_14 = frac_1 / 4;
    frac                            out[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index                  process_comps;

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
        return 0;

    /* check the mapping */
    if ( (pprocs = dev_proc(dev, get_color_mapping_procs)(dev)) == 0 ||
         (map_cmyk = pprocs->map_cmyk) == 0                            )
        return 0;

    map_cmyk(dev, frac_14, frac_0, frac_0, frac_0, out);
    if (!check_single_comp(cyan_c, frac_14, ncomps, out))
        return 0;
    map_cmyk(dev, frac_0, frac_14, frac_0, frac_0, out);
    if (!check_single_comp(magenta_c, frac_14, ncomps, out))
        return 0;
    map_cmyk(dev, frac_0, frac_0, frac_14, frac_0, out);
    if (!check_single_comp(yellow_c, frac_14, ncomps, out))
        return false;
    map_cmyk(dev, frac_0, frac_0, frac_0, frac_14, out);
    if (!check_single_comp(black_c, frac_14, ncomps, out))
        return 0;

    process_comps =  ((gx_color_index)1 << cyan_c)
                   | ((gx_color_index)1 << magenta_c)
                   | ((gx_color_index)1 << yellow_c)
                   | ((gx_color_index)1 << black_c);
    pcinfo->opmode = GX_CINFO_OPMODE;
    pcinfo->process_comps = process_comps;
    return process_comps;
}

/*
 * This set_overprint method is unique. If overprint is true, overprint
 * mode is set to 1, the process color model has DeviceCMYK behavior (see
 * the comment ahead of gx_is_cmyk_color_model above), and the device
 * color is set, the device color needs to be considered in setting up
 * the set of drawn components.
 */
private int
gx_set_overprint_DeviceCMYK(const gs_color_space * pcs, gs_state * pgs)
{
    gx_device *             dev = pgs->device;
    gx_device_color_info *  pcinfo = (dev == 0 ? 0 : &dev->color_info);
    gx_color_index          drawn_comps = 0;
    gs_overprint_params_t   params;

    /* check if we require special handling */
    if ( !pgs->overprint                      ||
         pgs->overprint_mode != 1             ||
         pcinfo == 0                          ||
         pcinfo->opmode == GX_CINFO_OPMODE_NOT  )
        return gx_spot_colors_set_overprint(pcs, pgs);

    /* check if color model behavior must be determined */
    if (pcinfo->opmode == GX_CINFO_OPMODE_UNKNOWN)
        drawn_comps = check_cmyk_color_model_comps(dev);
    else
        drawn_comps = pcinfo->process_comps;
    if (drawn_comps == 0)
        return gx_spot_colors_set_overprint(pcs, pgs);

    /* correct for any zero'ed color components */
    pgs->effective_overprint_mode = 1;
    if (color_is_set(pgs->dev_color)) {
        gx_color_index  nz_comps;
        int             code;
        dev_color_proc_get_nonzero_comps((*procp));

        procp = pgs->dev_color->type->get_nonzero_comps;
        if ((code = procp(pgs->dev_color, dev, &nz_comps)) < 0)
            return code;
        drawn_comps &= nz_comps;
    }

    params.retain_any_comps = true;
    params.retain_spot_comps = false;
    params.drawn_comps = drawn_comps;
    return gs_state_update_overprint(pgs, &params);
}


/* Null reference count adjustment procedure. */
void
gx_no_adjust_cspace_count(const gs_color_space * pcs, int delta)
{
}

/* Serialization. */
int 
gx_serialize_cspace_type(const gs_color_space * pcs, stream * s)
{
    const gs_color_space_type * type = pcs->type;
    uint n;
    return sputs(s, (const byte *)&type->index, sizeof(type->index), &n);
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
