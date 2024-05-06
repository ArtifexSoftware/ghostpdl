/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* DeviceN color space and operation definition */

#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gscdevn.h"
#include "gsfunc.h"
#include "gsrefct.h"
#include "gsmatrix.h"		/* for gscolor2.h */
#include "gsstruct.h"
#include "gxcspace.h"
#include "gxcdevn.h"
#include "gxfarith.h"
#include "gxfrac.h"
#include "gxcmap.h"
#include "gxgstate.h"
#include "gscoord.h"
#include "gzstate.h"
#include "gxdevcli.h"
#include "gsovrc.h"
#include "stream.h"
#include "gsnamecl.h"  /* Custom color call back define */
#include "gsicc_manage.h"
#include "gsicc.h"
#include "gsicc_cache.h"
#include "gxdevice.h"
#include "gxcie.h"
#include "gxdevsop.h"

/* ---------------- Color space ---------------- */

/* GC descriptors */
gs_private_st_composite(st_color_space_DeviceN, gs_color_space,
     "gs_color_space_DeviceN", cs_DeviceN_enum_ptrs, cs_DeviceN_reloc_ptrs);
private_st_device_n_colorant();
private_st_device_n_map();

/* Define the DeviceN color space type. */
static cs_proc_num_components(gx_num_components_DeviceN);
static cs_proc_init_color(gx_init_DeviceN);
static cs_proc_restrict_color(gx_restrict_DeviceN);
static cs_proc_concrete_space(gx_concrete_space_DeviceN);
static cs_proc_concretize_color(gx_concretize_DeviceN);
static cs_proc_remap_concrete_color(gx_remap_concrete_DeviceN);
static cs_proc_remap_color(gx_remap_DeviceN);
static cs_proc_install_cspace(gx_install_DeviceN);
static cs_proc_set_overprint(gx_set_overprint_DeviceN);
static cs_proc_final(gx_final_DeviceN);
static cs_proc_serialize(gx_serialize_DeviceN);
static cs_proc_polarity(gx_polarity_DeviceN);

const gs_color_space_type gs_color_space_type_DeviceN = {
    gs_color_space_index_DeviceN, true, false,
    &st_color_space_DeviceN, gx_num_components_DeviceN,
    gx_init_DeviceN, gx_restrict_DeviceN,
    gx_concrete_space_DeviceN,
    gx_concretize_DeviceN, gx_remap_concrete_DeviceN,
    gx_remap_DeviceN, gx_install_DeviceN,
    gx_set_overprint_DeviceN,
    gx_final_DeviceN, gx_no_adjust_color_count,
    gx_serialize_DeviceN,
    gx_cspace_is_linear_default, gx_polarity_DeviceN
};

/* GC procedures */

static
ENUM_PTRS_BEGIN(cs_DeviceN_enum_ptrs)
    return 0;
ENUM_PTR(0, gs_color_space, params.device_n.map);
ENUM_PTR(1, gs_color_space, params.device_n.colorants);
ENUM_PTRS_END
static RELOC_PTRS_BEGIN(cs_DeviceN_reloc_ptrs)
{
    RELOC_PTR(gs_color_space, params.device_n.map);
    RELOC_PTR(gs_color_space, params.device_n.colorants);
}
RELOC_PTRS_END

/* ------ Public procedures ------ */

/*
 * Create a new DeviceN colorspace.
 */
int
gs_cspace_new_DeviceN(
    gs_color_space **ppcs,
    uint num_components,
    gs_color_space *palt_cspace,
    gs_memory_t *pmem
    )
{
    gs_color_space *pcs;
    gs_device_n_params *pcsdevn;
    char **pnames;
    int i, code;

    if (palt_cspace == 0 || !palt_cspace->type->can_be_alt_space)
        return_error(gs_error_rangecheck);

    pcs = gs_cspace_alloc(pmem, &gs_color_space_type_DeviceN);
    if (pcs == NULL)
        return_error(gs_error_VMerror);
    pcsdevn = &pcs->params.device_n;
    pcsdevn->names = NULL;
    pcsdevn->map = NULL;
    pcsdevn->colorants = NULL;
    pcsdevn->named_color_supported = false;
    pcsdevn->num_process_names = 0;
    pcsdevn->process_names = NULL;
    pcsdevn->mem = pmem->non_gc_memory;
    pcsdevn->all_none = false;

    /* Allocate space for color names list. */
    code = alloc_device_n_map(&pcsdevn->map, pmem, "gs_cspace_build_DeviceN");
    if (code < 0) {
        gs_free_object(pmem, pcs, "gs_cspace_new_DeviceN");
        return code;
    }
    pnames = (char **)gs_alloc_bytes(pcsdevn->mem, num_components * sizeof(char *), "gs_cspace_new_DeviceN");
    if (pnames == 0) {
        gs_free_object(pmem, pcsdevn->map, ".gs_cspace_build_DeviceN(map)");
        gs_free_object(pmem, pcs, "gs_cspace_new_DeviceN");
        return_error(gs_error_VMerror);
    }
    for (i=0; i<num_components; i++)
        pnames[i] = NULL;		/* empty string for check_DeviceN_component_names */
    pcs->base_space = palt_cspace;
    rc_increment_cs(palt_cspace);
    pcsdevn->names = pnames;
    pcsdevn->num_components = num_components;
    *ppcs = pcs;
    return 0;
}

/* Allocate and initialize a DeviceN map. */
int
alloc_device_n_map(gs_device_n_map ** ppmap, gs_memory_t * mem,
                   client_name_t cname)
{
    gs_device_n_map *pimap;

    rc_alloc_struct_1(pimap, gs_device_n_map, &st_device_n_map, mem,
                      return_error(gs_error_VMerror), cname);
    pimap->tint_transform = 0;
    pimap->tint_transform_data = 0;
    pimap->cache_valid = false;
    *ppmap = pimap;
    return 0;
}

/*
 * DeviceN and NChannel color spaces can have an attributes dict.  In the
 * attribute dict can be a Colorants dict which contains Separation color
 * spaces.  If the Colorant dict is present, the PS logic will build each of
 * the Separation color spaces in a temp gstate and then call this procedure
 * to attach the Separation color space to the DeviceN color space.
 * The parameter to this procedure is a colorant name.  The Separation
 * color space is in the current (temp) gstate.  The DeviceN color space is
 * in the next gstate down in the gstate list (pgs->saved).
 */
int
gs_attachcolorant(char *sep_name, gs_gstate * pgs)
{
    gs_color_space * pdevncs;
    gs_device_n_colorant * patt;

    /* Verify that we have a DeviceN color space */
    if (!pgs->saved)
        return_error(gs_error_rangecheck);
    pdevncs = gs_currentcolorspace_inline(pgs->saved);
    if (pdevncs->type != &gs_color_space_type_DeviceN)
        return_error(gs_error_rangecheck);

    /* Allocate an attribute list element for our linked list of colorants */
    rc_alloc_struct_1(patt, gs_device_n_colorant, &st_device_n_colorant,
                        pgs->memory, return_error(gs_error_VMerror),
                        "gs_attachattributrescolorspace");

    /* Point our attribute list entry to the attribute color space */
    patt->colorant_name = sep_name;
    patt->cspace = gs_currentcolorspace_inline(pgs);
    rc_increment_cs(patt->cspace);

    /* Link our new attribute color space to the DeviceN color space */
    patt->next = pdevncs->params.device_n.colorants;
    pdevncs->params.device_n.colorants = patt;

    return 0;
}

int
gs_attach_colorant_to_space(char *sep_name, gs_color_space *pcs, gs_color_space *colorant_space, gs_memory_t *mem)
{
    gs_device_n_colorant * patt;

    if (pcs->type != &gs_color_space_type_DeviceN)
        return_error(gs_error_rangecheck);

    /* Allocate an attribute list element for our linked list of colorants */
    rc_alloc_struct_1(patt, gs_device_n_colorant, &st_device_n_colorant,
                        mem, return_error(gs_error_VMerror),
                        "gs_attachattributrescolorspace");

    /* Point our attribute list entry to the attribute color space */
    patt->colorant_name = sep_name;
    patt->cspace = colorant_space;
    rc_increment_cs(patt->cspace);

    /* Link our new attribute color space to the DeviceN color space */
    patt->next = pcs->params.device_n.colorants;
    pcs->params.device_n.colorants = patt;

    return 0;
}

#if 0 /* Unused; Unsupported by gx_serialize_device_n_map. */
/*
 * Set the DeviceN tint transformation procedure.
 */
int
gs_cspace_set_devn_proc(gs_color_space * pcspace,
                        int (*proc)(const float *,
                                    float *,
                                    const gs_gstate *,
                                    void *
                                    ),
                        void *proc_data
                        )
{
    gs_device_n_map *pimap;

    if (gs_color_space_get_index(pcspace) != gs_color_space_index_DeviceN)
        return_error(gs_error_rangecheck);
    pimap = pcspace->params.device_n.map;
    pimap->tint_transform = proc;
    pimap->tint_transform_data = proc_data;
    pimap->cache_valid = false;
    return 0;
}
#endif

/*
 * Check if we are using the alternate color space.
 */
bool
using_alt_color_space(const gs_gstate * pgs)
{
    return (pgs->color_component_map.use_alt_cspace);
}

/* Map a DeviceN color using a Function. */
int
map_devn_using_function(const float *in, float *out,
                        const gs_gstate *pgs, void *data)

{
    gs_function_t *const pfn = data;

    return gs_function_evaluate(pfn, in, out);
}

/*
 * Set the DeviceN tint transformation procedure to a Function.
 */
int
gs_cspace_set_devn_function(gs_color_space *pcspace, gs_function_t *pfn)
{
    gs_device_n_map *pimap;

    if (gs_color_space_get_index(pcspace) != gs_color_space_index_DeviceN ||
        pfn->params.m != pcspace->params.device_n.num_components ||
        pfn->params.n != gs_color_space_num_components(pcspace->base_space)
        )
        return_error(gs_error_rangecheck);
    pimap = pcspace->params.device_n.map;
    pimap->tint_transform = map_devn_using_function;
    pimap->tint_transform_data = pfn;
    pimap->cache_valid = false;
    return 0;
}

/*
 * If the DeviceN tint transformation procedure is a Function,
 * return the function object, otherwise return 0.
 */
gs_function_t *
gs_cspace_get_devn_function(const gs_color_space *pcspace)
{
    if (gs_color_space_get_index(pcspace) == gs_color_space_index_DeviceN &&
        pcspace->params.device_n.map->tint_transform ==
          map_devn_using_function)
        return pcspace->params.device_n.map->tint_transform_data;
    return 0;
}

/* ------ Color space implementation ------ */

/* Return the number of components of a DeviceN space. */
static int
gx_num_components_DeviceN(const gs_color_space * pcs)
{
    return pcs->params.device_n.num_components;
}

/* Determine best guess of polarity */
static gx_color_polarity_t
gx_polarity_DeviceN(const gs_color_space * pcs)
{
    /* DeviceN initializes to 1.0 like a separation so
       for now, treat this as subtractive.  It is possible
       that we may have to do a special test for Red, Green
       and Blue but for now, I believe the following is
       correct */
    return GX_CINFO_POLARITY_SUBTRACTIVE;
}

/* Initialize a DeviceN color. */
static void
gx_init_DeviceN(gs_client_color * pcc, const gs_color_space * pcs)
{
    uint i;

    for (i = 0; i < pcs->params.device_n.num_components; ++i)
        pcc->paint.values[i] = 1.0;
}

/* Force a DeviceN color into legal range. */
static void
gx_restrict_DeviceN(gs_client_color * pcc, const gs_color_space * pcs)
{
    uint i;

    for (i = 0; i < pcs->params.device_n.num_components; ++i) {
        double value = pcc->paint.values[i];
        pcc->paint.values[i] = (value <= 0 ? 0 : value >= 1 ? 1 : value);
    }
}

/* Remap a DeviceN color. */
static const gs_color_space *
gx_concrete_space_DeviceN(const gs_color_space * pcs,
                          const gs_gstate * pgs)
{
    bool is_lab = false;
#ifdef DEBUG
    /*
     * Verify that the color space and gs_gstate info match.
     */
    if (pcs->id != pgs->color_component_map.cspace_id)
        dmprintf(pgs->memory, "gx_concrete_space_DeviceN: color space id mismatch");
#endif
    /*
     * Check if we are using the alternate color space.
     */
    if (pgs->color_component_map.use_alt_cspace) {
        /* Need to handle PS CIE space */
        if (gs_color_space_is_PSCIE(pcs->base_space)) {
            if (pcs->base_space->icc_equivalent == NULL) {
                (void)gs_colorspace_set_icc_equivalent(pcs->base_space,
                                                    &is_lab, pgs->memory);
            }
            return (pcs->base_space->icc_equivalent);
        }
        return cs_concrete_space(pcs->base_space, pgs);
    }
    /*
     * DeviceN color spaces are concrete (when not using alt. color space).
     */
    return pcs;
}

static int
gx_remap_DeviceN(const gs_client_color * pcc, const gs_color_space * pcs,
        gx_device_color * pdc, const gs_gstate * pgs, gx_device * dev,
                       gs_color_select_t select)
{
    frac conc[GS_CLIENT_COLOR_MAX_COMPONENTS];
    const gs_color_space *pconcs;
    int i = pcs->type->num_components(pcs),k;
    int code = 0;
    const gs_color_space *pacs = pcs->base_space;
    gs_client_color temp;
    bool mapped = false;

    if ( pcs->cmm_icc_profile_data != NULL && pgs->color_component_map.use_alt_cspace) {
        /* This is the case where we have placed a N-CLR source profile for
           this color space */
        if (pcs->cmm_icc_profile_data->devicen_permute_needed) {
            for ( k = 0; k < i; k++) {
                temp.paint.values[k] = pcc->paint.values[pcs->cmm_icc_profile_data->devicen_permute[k]];
            }
            code = pacs->type->remap_color(&temp, pacs, pdc, pgs, dev, select);
        } else {
            code = pacs->type->remap_color(pcc, pacs, pdc, pgs, dev, select);
        }
        return(code);
    } else {
        /* Check if we are doing any named color management.  This check happens
           regardless if we are doing the alternate tint transform or not.  If
           desired, that check could be made in gsicc_transform_named_color and
           a decision made to return true or false */
        if (pgs->icc_manager->device_named != NULL) {
            /* Try to apply the direct replacement */
            mapped = gx_remap_named_color(pcc, pcs, pdc, pgs, dev, select);
        }
        if (!mapped) {
            cmm_dev_profile_t *dev_profile;
            code = dev_proc(dev, get_profile)(dev, &dev_profile);
            if (code < 0)
                return code;
            code = (*pcs->type->concretize_color)(pcc, pcs, conc, pgs, dev);
            if (code < 0)
                return code;
            pconcs = cs_concrete_space(pcs, pgs);
            code = (*pconcs->type->remap_concrete_color)(pconcs, conc, pdc, pgs, dev, select, dev_profile);
        }
        /* Save original color space and color info into dev color */
        i = any_abs(i);
        for (i--; i >= 0; i--)
            pdc->ccolor.paint.values[i] = pcc->paint.values[i];
        pdc->ccolor_valid = true;
        return code;
    }
}

static int
gx_concretize_DeviceN(const gs_client_color * pc, const gs_color_space * pcs,
                      frac * pconc, const gs_gstate * pgs, gx_device *dev)
{
    int code, tcode = 0;
    gs_client_color cc;
    gs_color_space *pacs = (gs_color_space*) (pcs->base_space);
    gs_device_n_map *map = pcs->params.device_n.map;
    bool is_lab;
    int num_src_comps = pcs->params.device_n.num_components;

#ifdef DEBUG
    /*
     * Verify that the color space and gs_gstate info match.
     */
    if (pcs->id != pgs->color_component_map.cspace_id)
        dmprintf(dev->memory, "gx_concretize_DeviceN: color space id mismatch");
#endif

    /*
     * Check if we need to map into the alternate color space.
     * We must preserve tcode for implementing a semi-hack in the interpreter.
     */

    if (pgs->color_component_map.use_alt_cspace) {
        /* Check the 1-element cache first. */
        if (map->cache_valid) {
            int i;

            for (i = pcs->params.device_n.num_components; --i >= 0;) {
                if (map->tint[i] != pc->paint.values[i])
                    break;
            }
            if (i < 0) {
                int num_out = gs_color_space_num_components(pacs);

                for (i = 0; i < num_out; ++i)
                    pconc[i] = map->conc[i];
                return 0;
            }
        }
        tcode = (*pcs->params.device_n.map->tint_transform)
             (pc->paint.values, &cc.paint.values[0],
             pgs, pcs->params.device_n.map->tint_transform_data);
        (*pacs->type->restrict_color)(&cc, pacs);
        if (tcode < 0)
            return tcode;
        /* First check if this was PS based. */
        if (gs_color_space_is_PSCIE(pacs)) {
            /* We may have to rescale data to 0 to 1 range */
            rescale_cie_colors(pacs, &cc);
            /* If we have not yet created the profile do that now */
            if (pacs->icc_equivalent == NULL) {
                code = gs_colorspace_set_icc_equivalent(pacs, &(is_lab), pgs->memory);
                if (code < 0)
                   return code;
            }
            /* Use the ICC equivalent color space */
            pacs = pacs->icc_equivalent;
        }
        if (pacs->cmm_icc_profile_data &&
            (pacs->cmm_icc_profile_data->data_cs == gsCIELAB ||
            pacs->cmm_icc_profile_data->islab)) {
            /* Get the data in a form that is concrete for the CMM */
            cc.paint.values[0] /= 100.0;
            cc.paint.values[1] = (cc.paint.values[1]+128)/255.0;
            cc.paint.values[2] = (cc.paint.values[2]+128)/255.0;
        }
        code = cs_concretize_color(&cc, pacs, pconc, pgs, dev);
    }
    else {
        int i;

        for (i = num_src_comps; --i >= 0;)
            pconc[i] = gx_unit_frac(pc->paint.values[i]);
        return 0;
    }
    return (code < 0 || tcode == 0 ? code : tcode);
}

static int
gx_remap_concrete_DeviceN(const gs_color_space * pcs, const frac * pconc,
                          gx_device_color * pdc, const gs_gstate * pgs,
                          gx_device * dev, gs_color_select_t select,
                          const cmm_dev_profile_t *dev_profile)
{
    int code = 0;

#ifdef DEBUG
    /*
     * Verify that the color space and gs_gstate info match.
     */
    if (pcs->id != pgs->color_component_map.cspace_id)
        dmprintf(pgs->memory, "gx_remap_concrete_DeviceN: color space id mismatch");
#endif
    if (pgs->color_component_map.use_alt_cspace) {
        const gs_color_space *pacs = pcs->base_space;
        return (*pacs->type->remap_concrete_color)
                                (pacs, pconc, pdc, pgs, dev, select, dev_profile);
    } else {
    /* If we are going DeviceN out to real sep device that understands these,
       and if the destination profile is DeviceN, we print the colors directly.
       Make sure to disable the DeviceN profile color map so that is does not
       get used in gx_remap_concrete_devicen.  We probably should pass something
       through here but it is a pain due to the change in the proc. */
        bool temp_val;

        if (dev_profile->spotnames != NULL && code >= 0) {
            temp_val = dev_profile->spotnames->equiv_cmyk_set;
            dev_profile->spotnames->equiv_cmyk_set = false;
            gx_remap_concrete_devicen(pconc, pdc, pgs, dev, select, pcs);
            dev_profile->spotnames->equiv_cmyk_set = temp_val;
        } else {
            gx_remap_concrete_devicen(pconc, pdc, pgs, dev, select, pcs);
        }
        return 0;
    }
}

/*
 * Check that the color component names for a DeviceN color space
 * match the device colorant names.  Also build a gs_devicen_color_map
 * structure.
 */
static int
check_DeviceN_component_names(const gs_color_space * pcs, gs_gstate * pgs)
{
    char **names = pcs->params.device_n.names;
    int num_comp = pcs->params.device_n.num_components;
    int i;
    int colorant_number;
    const char *pname;
    uint name_size;
    gs_devicen_color_map * pcolor_component_map
        = &pgs->color_component_map;
    gx_device * dev = pgs->device;
    bool non_match = false;
    int none_count = 0;

    pcolor_component_map->num_components = num_comp;
    pcolor_component_map->cspace_id = pcs->id;
    pcolor_component_map->num_colorants = dev->color_info.num_components;
    pcolor_component_map->sep_type = SEP_OTHER;

    /* If the named color profile supports the components, don't use
       the alternate tint transform. */
    if (gsicc_support_named_color(pcs, pgs)) {
        pcolor_component_map->use_alt_cspace = false;
        return 0;
    }

    /* If our device is using an additive color model, then we need to
     * consider using the alternative color space, as separations are
     * generally only used with a subtractive color model. There are
     * exceptions, however.
     *
     * If we don't support devn, then we will certainly have to use the
     * alternative color space.
     *
     * If we are a pdf14 device, and we are doing transparency blending
     * in an additive space, we need to keep the spots separated and
     * blend them individually as per the PDF specification. Note
     * however, if the spot is a CMYK process color and we are doing
     * the blend in an additive color space the alternate color space
     * is used.  This matches AR.
     *
     * Otherwise, we will always use the alternate colorspace, unless
     * our device specifically claims to be a separation-supporting
     * additive device. Possibly all additive devn devices should
     * support this now, but I lack to confidence to make this change
     * globally. Instead we'll just enable it on a device by device
     * basis for now.
     *
     * This matches logic in check_Separation_component_name.
     */
    if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE) {
        if (dev_proc(dev, dev_spec_op)(dev, gxdso_supports_devn, NULL, 0) == 0 ||
            (dev_proc(dev, dev_spec_op)(dev, gxdso_is_pdf14_device, NULL, 0) == 0 &&
             dev_proc(dev, dev_spec_op)(dev, gxdso_is_sep_supporting_additive_device, NULL, 0) == 0)) {
            pcolor_component_map->use_alt_cspace = true;
            return 0;
        }
    }

    /*
     * Now check the names of the color components.  If any of the colorants
     * come back as process type (i.e. CMYK) and we are drawing in a pdf14
     * additive color space then use the alternate tint transform.
     */

    for(i = 0; i < num_comp; i++ ) {
        /*
         * Get the character string and length for the component name.
         */
        pname = names[i];
        if (pname == NULL)
            pname = "";
        name_size = strlen(pname);
        /*
         * Compare the colorant name to the device's.  If the device's
         * compare routine returns GX_DEVICE_COLOR_MAX_COMPONENTS then the
         * colorant is in the SeparationNames list but not in the
         * SeparationOrder list.
         */
        colorant_number = (*dev_proc(dev, get_color_comp_index))
                                   (dev, pname, name_size, SEPARATION_NAME);
        if (colorant_number >= 0) {		/* If valid colorant name */
            pcolor_component_map->color_map[i] =
            (colorant_number == GX_DEVICE_COLOR_MAX_COMPONENTS) ? -1
                                                   : colorant_number;
        } else {
            if (strncmp(pname, "None", name_size) != 0) {
                    non_match = true;
            } else {
                /* Device N includes one or more None Entries. We can't reduce
                   the number of components in the list count, since the None Name(s)
                   are present in the list and GCd and we may need the None
                   entries in the alternate tint trasform calcuation.
                   So we will detect the presence of them by setting
                   pcolor_component_map->color_map[i] = -1 and watching
                   for this case later during the remap operation. */
                pcolor_component_map->color_map[i] = -1;
                none_count++;
            }
        }
    }
    pcolor_component_map->use_alt_cspace = non_match;

    if (none_count == num_comp)
        return 1;

    return 0;
}

/* Check if the colorants are all process colorants and of the same type */
static separation_colors
gx_check_process_names_DeviceN(gs_color_space * pcs, gs_gstate * pgs)
{
    int i, num_comp, num_spots = 0, num_rgb_process = 0;
    int num_cmyk_process = 0, num_other = 0;
    char **names;
    const char *pname;
    uint name_size;

    names = pcs->params.device_n.names;
    num_comp = pcs->params.device_n.num_components;

    /* Step through the color space colorants */
    for (i = 0; i < num_comp; i++) {
        pname = names[i];
        if (pname == NULL)
            pname = "";
        name_size = strlen(pname);

        /* Classify */
        if (strncmp(pname, "None", name_size) == 0) {
            num_other++;
        } else {
            if (strncmp(pname, "Cyan", name_size) == 0 ||
                strncmp(pname, "Magenta", name_size) == 0 ||
                strncmp(pname, "Yellow", name_size) == 0 ||
                strncmp(pname, "Black", name_size) == 0) {
                num_cmyk_process++;
            } else if (strncmp(pname, "Red", name_size) == 0 ||
                strncmp(pname, "Green", name_size) == 0 ||
                strncmp(pname, "Blue", name_size) == 0) {
                num_rgb_process++;
            } else {
                num_spots++;
            }
        }
    }

    if (num_cmyk_process > 0 && num_rgb_process == 0 && num_spots == 0)
        return SEP_PURE_CMYK;
    if (num_rgb_process > 0 && num_cmyk_process == 0 && num_spots == 0)
        return SEP_PURE_RGB;
    if (num_spots > 0 && num_cmyk_process == 0 && num_rgb_process == 0)
        return SEP_PURE_SPOT;
    return SEP_MIX;
}

/* Install a DeviceN color space. */
static int
gx_install_DeviceN(gs_color_space * pcs, gs_gstate * pgs)
{
    int code;
    code = check_DeviceN_component_names(pcs, pgs);
    if (code < 0)
       return code;

    /* Indicates all colorants are /None */
    if (code > 0)
        pcs->params.device_n.all_none = true;

    if (pgs->icc_manager->device_named != NULL) {
        pcs->params.device_n.named_color_supported =
            gsicc_support_named_color(pcs, pgs);
    }
    pcs->params.device_n.color_type = gx_check_process_names_DeviceN(pcs, pgs);

    /* See if we have an ICC profile that we can associate with
       this DeviceN color space */
    if (pgs->icc_manager->device_n != NULL) {
        /* An nclr profile is in the manager.  Grab one that matches. */
        cmm_profile_t *profdata = gsicc_finddevicen(pcs, pgs->icc_manager);
        if (profdata != NULL)
            gsicc_adjust_profile_rc(profdata, 1, "gx_install_DeviceN");
        if (pcs->cmm_icc_profile_data != NULL)
            gsicc_adjust_profile_rc(pcs->cmm_icc_profile_data, -1, "gx_install_DeviceN");
        pcs->cmm_icc_profile_data = profdata;
    }
    /* {csrc} was pgs->color_space->params.device_n.use_alt_cspace */
    ((gs_color_space *)pcs)->params.device_n.use_alt_cspace =
        using_alt_color_space(pgs);
    if (pcs->params.device_n.use_alt_cspace && pcs->cmm_icc_profile_data == NULL ) {
        /* No nclr ICC profile */
        code = (pcs->base_space->type->install_cspace)
            (pcs->base_space, pgs);
    } else if (pcs->params.device_n.use_alt_cspace) {
        gs_color_space *nclr_pcs;
        /* Need to install the nclr cspace */
        code = gs_cspace_build_ICC(&nclr_pcs, NULL, pgs->memory);
        nclr_pcs->cmm_icc_profile_data = pcs->cmm_icc_profile_data;
        gsicc_adjust_profile_rc(pcs->cmm_icc_profile_data, 1, "gx_install_DeviceN");
        rc_increment(nclr_pcs);				/* FIXME: Suspicious - RJW */
        rc_decrement(pcs->base_space, "gx_install_DeviceN");
        pcs->base_space = nclr_pcs;
    }
    /*
     * Give the device an opportunity to capture equivalent colors for any
     * spot colors which might be present in the color space.
     */
    if (code >= 0) {
        if (dev_proc(pgs->device, update_spot_equivalent_colors))
            code = dev_proc(pgs->device, update_spot_equivalent_colors)
                                                        (pgs->device, pgs, pcs);
    }
    return code;
}

/* Set overprint information for a DeviceN color space */
static int
gx_set_overprint_DeviceN(const gs_color_space * pcs, gs_gstate * pgs)
{
    gs_devicen_color_map *  pcmap = &pgs->color_component_map;
    int code;

    /* It is possible that the color map information in the graphic state
       is not current due to save/restore and or if we are coming from
       a color space that is inside a PatternType 2 */
    code = check_DeviceN_component_names(pcs, pgs);
    if (code < 0)
       return code;
    if (pcmap->use_alt_cspace) {
        const gs_color_space_type* base_type = pcs->base_space->type;

        /* If the base space is DeviceCMYK, handle overprint as DeviceCMYK */
        if ( base_type->index == gs_color_space_index_DeviceCMYK )
            return base_type->set_overprint( pcs->base_space, pgs );
        else
            return gx_set_no_overprint(pgs);
    } else {
        gs_overprint_params_t   params = { 0 };

        params.retain_any_comps = (pgs->overprint && pgs->is_fill_color) ||
                                  (pgs->stroke_overprint && !pgs->is_fill_color);

        params.drawn_comps = 0;
        if (params.retain_any_comps) {
            int     i, ncomps = pcs->params.device_n.num_components;

            params.is_fill_color = pgs->is_fill_color;	/* for fill_stroke */
            for (i = 0; i < ncomps; i++) {
                int mcomp = pcmap->color_map[i];
                if (mcomp >= 0)
                    gs_overprint_set_drawn_comp( params.drawn_comps, mcomp);
            }
        }

        /* Only DeviceCMYK can use overprint mode */
        params.effective_opm = pgs->color[0].effective_opm = 0;
        params.op_state = OP_STATE_NONE;
        params.is_fill_color =pgs->is_fill_color;
        return gs_gstate_update_overprint(pgs, &params);
    }
}

/* Finalize contents of a DeviceN color space. */
static void
gx_final_DeviceN(gs_color_space * pcs)
{
    gs_device_n_colorant * pnextatt, * patt = pcs->params.device_n.colorants;
    uint num_proc_names = pcs->params.device_n.num_process_names;
    gs_memory_t *mem = pcs->params.device_n.mem->non_gc_memory;
    char **proc_names = pcs->params.device_n.process_names;
    int k;

    for (k = 0; k < pcs->params.device_n.num_components; k++)
        gs_free_object(mem, pcs->params.device_n.names[k], "gx_final_DeviceN");
    gs_free_object(mem, pcs->params.device_n.names, "gx_final_DeviceN");

    if (num_proc_names > 0 && proc_names != NULL) {
        for (k = 0; k < num_proc_names; k++) {
            gs_free_object(mem, proc_names[k], "gx_final_DeviceN");
        }
        gs_free_object(mem, proc_names, "gx_final_DeviceN");
    }

    rc_decrement_only(pcs->params.device_n.map, "gx_adjust_DeviceN");
    while (patt != NULL) {
        pnextatt = patt->next;
        gs_free_object(mem, patt->colorant_name, "gx_final_DeviceN");
        rc_decrement_cs(patt->cspace, "gx_final_DeviceN");
        rc_decrement(patt, "gx_adjust_DeviceN");
        patt = pnextatt;
    }
    if (pcs->params.device_n.devn_process_space)
        rc_decrement_only(pcs->params.device_n.devn_process_space, "gx_final_DeviceN");
    /* Ensure idempotency */
    memset(&pcs->params.device_n, 0, sizeof(pcs->params.device_n));
}

/* ---------------- Serialization. -------------------------------- */

int
gx_serialize_device_n_map(const gs_color_space * pcs, gs_device_n_map * m, stream * s)
{
    const gs_function_t *pfn;

    if (m->tint_transform != map_devn_using_function)
        return_error(gs_error_unregistered); /* Unimplemented. */
    pfn = (const gs_function_t *)m->tint_transform_data;
    return gs_function_serialize(pfn, s);
}

static int
gx_serialize_DeviceN(const gs_color_space * pcs, stream * s)
{
    const gs_device_n_params * p = &pcs->params.device_n;
    uint n, m;
    int code = gx_serialize_cspace_type(pcs, s);

    if (code < 0)
        return code;
    code = sputs(s, (const byte *)&p->num_components, sizeof(p->num_components), &n);
    if (code < 0)
        return code;
    for (n=0;n < p->num_components;n++) {
        const char *name = p->names[n];
        if (name == NULL)
            name = "";
        code = sputs(s, (const byte *)name, strlen(name) + 1, &m);
        if (code < 0)
            return code;
    }
    code = cs_serialize(pcs->base_space, s);
    if (code < 0)
        return code;
    return gx_serialize_device_n_map(pcs, p->map, s);
    /* p->use_alt_cspace isn't a property of the space. */
}
