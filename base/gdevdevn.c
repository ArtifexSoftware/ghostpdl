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

/* Example DeviceN process color model devices. */

#include "math_.h"
#include "string_.h"
#include "gdevprn.h"
#include "gsparam.h"
#include "gscrd.h"
#include "gscrdp.h"
#include "gxlum.h"
#include "gdevdcrd.h"
#include "gstypes.h"
#include "gxdcconv.h"
#include "gdevdevn.h"
#include "gsequivc.h"
#include "gxblend.h"
#include "gdevp14.h"
#include "gdevdevnprn.h"
#include "gxdevsop.h"

/*
 * Utility routines for common DeviceN related parameters:
 *   SeparationColorNames, SeparationOrder, and MaxSeparations
 */

/* Convert a gray color space to DeviceN colorants. */
void
gray_cs_to_devn_cm(const gx_device * dev, int * map, frac gray, frac out[])
{
    int i = dev->color_info.num_components - 1;

    for(; i >= 0; i--)                  /* Clear colors */
        out[i] = frac_0;
    if ((i = map[3]) != GX_DEVICE_COLOR_MAX_COMPONENTS)
        out[i] = frac_1 - gray;
}

/* Convert an RGB color space to DeviceN colorants. */
void
rgb_cs_to_devn_cm(const gx_device * dev, int * map,
                const gs_gstate *pgs, frac r, frac g, frac b, frac out[])
{
    int i = dev->color_info.num_components - 1;
    frac cmyk[4];

    for(; i >= 0; i--)                  /* Clear colors */
        out[i] = frac_0;
    color_rgb_to_cmyk(r, g, b, pgs, cmyk, dev->memory);
    if ((i = map[0]) != GX_DEVICE_COLOR_MAX_COMPONENTS)
        out[i] = cmyk[0];
    if ((i = map[1]) != GX_DEVICE_COLOR_MAX_COMPONENTS)
        out[i] = cmyk[1];
    if ((i = map[2]) != GX_DEVICE_COLOR_MAX_COMPONENTS)
        out[i] = cmyk[2];
    if ((i = map[3]) != GX_DEVICE_COLOR_MAX_COMPONENTS)
        out[i] = cmyk[3];
}

/* Convert a CMYK color space to DeviceN colorants. */
void
cmyk_cs_to_devn_cm(const gx_device * dev, const int * map,
                frac c, frac m, frac y, frac k, frac out[])
{
    int i = dev->color_info.num_components - 1;

    for(; i >= 0; i--)                  /* Clear colors */
        out[i] = frac_0;
    if ((i = map[0]) != GX_DEVICE_COLOR_MAX_COMPONENTS)
        out[i] = c;
    if ((i = map[1]) != GX_DEVICE_COLOR_MAX_COMPONENTS)
        out[i] = m;
    if ((i = map[2]) != GX_DEVICE_COLOR_MAX_COMPONENTS)
        out[i] = y;
    if ((i = map[3]) != GX_DEVICE_COLOR_MAX_COMPONENTS)
        out[i] = k;
}

/* Some devices need to create composite mappings of the spot colorants.
   This code was originally in the tiffsep device but was moved here to be
   sharable across multiple separation devices that need this capability */


/*
* Build the map to be used to create a CMYK equivalent to the current
* device components.
*/
void build_cmyk_map(gx_device *pdev, int num_comp,
    equivalent_cmyk_color_params *equiv_cmyk_colors,
    cmyk_composite_map * cmyk_map)
{
    int comp_num;
    gs_devn_params *devn_params =  dev_proc(pdev, ret_devn_params)(pdev);

    if (devn_params == NULL)
        return;

    for (comp_num = 0; comp_num < num_comp; comp_num++) {
        int sep_num = devn_params->separation_order_map[comp_num];

        cmyk_map[comp_num].c = cmyk_map[comp_num].m =
            cmyk_map[comp_num].y = cmyk_map[comp_num].k = frac_0;
        /* The tiffsep device has 4 standard colors:  CMYK */
        if (sep_num < devn_params->num_std_colorant_names) {
            switch (sep_num) {
            case 0: cmyk_map[comp_num].c = frac_1; break;
            case 1: cmyk_map[comp_num].m = frac_1; break;
            case 2: cmyk_map[comp_num].y = frac_1; break;
            case 3: cmyk_map[comp_num].k = frac_1; break;
            }
        } else {
            sep_num -= devn_params->num_std_colorant_names;
            if (equiv_cmyk_colors->color[sep_num].color_info_valid) {
                cmyk_map[comp_num].c = equiv_cmyk_colors->color[sep_num].c;
                cmyk_map[comp_num].m = equiv_cmyk_colors->color[sep_num].m;
                cmyk_map[comp_num].y = equiv_cmyk_colors->color[sep_num].y;
                cmyk_map[comp_num].k = equiv_cmyk_colors->color[sep_num].k;
            }
        }
    }
}

/*
 * This utility routine calculates the number of bits required to store
 * color information.  In general the values are rounded up to an even
 * byte boundary except those cases in which mulitple pixels can evenly
 * into a single byte.
 *
 * The parameter are:
 *   ncomp - The number of components (colorants) for the device.  Valid
 *           values are 1 to GX_DEVICE_COLOR_MAX_COMPONENTS
 *   bpc - The number of bits per component.  Valid values are 1, 2, 4, 5,
 *         and 8.
 * Input values are not tested for validity.
 */
int
bpc_to_depth(uchar ncomp, int bpc)
{
    static const byte depths[4][8] = {
        {1, 2, 0, 4, 8, 0, 0, 8},
        {2, 4, 0, 8, 16, 0, 0, 16},
        {4, 8, 0, 16, 16, 0, 0, 24},
        {4, 8, 0, 16, 32, 0, 0, 32}
    };

    if (ncomp <=4 && bpc <= 8)
        return depths[ncomp -1][bpc-1];
    else
        return (ncomp * bpc + 7) & ~7;
}

#define compare_color_names(name, name_size, str, str_size) \
    (name_size == str_size && \
        (strncmp((const char *)name, (const char *)str, name_size) == 0))

/*
 * This routine will check if a name matches any item in a list of process
 * color model colorant names.
 */
static bool
check_process_color_names(fixed_colorant_names_list plist,
                          const gs_param_string * pstring)
{
    if (plist) {
        uint size = pstring->size;

        while( *plist) {
            if (compare_color_names(*plist, strlen(*plist), pstring->data, size)) {
                return true;
            }
            plist++;
        }
    }
    return false;
}

static int count_process_color_names(fixed_colorant_names_list plist)
{
    int count = 0;

    if (plist) {
        while( *plist){
            count++;
            plist++;
        }
    }
    return count;
}

/* Check only the separation names */
int
check_separation_names(const gx_device * dev, const gs_devn_params * pparams,
    const char * pname, int name_size, int component_type, int number)
{
    const gs_separations * separations = &pparams->separations;
    int num_spot = separations->num_separations;
    int color_component_number = number;
    int i;

    for (i = 0; i<num_spot; i++) {
        if (compare_color_names((const char *)separations->names[i].data,
            separations->names[i].size, pname, name_size)) {
            return color_component_number;
        }
        color_component_number++;
    }
    return -1;
}

/*
 * This routine will check to see if the color component name  match those
 * of either the process color model colorants or the names on the
 * SeparationColorNames list.
 *
 * Parameters:
 *   dev - pointer to device data structure.
 *   pname - pointer to name (zero termination not required)
 *   nlength - length of the name
 *
 * This routine returns a positive value (0 to n) which is the device colorant
 * number if the name is found.  It returns a negative value if not found.
 */
int
check_pcm_and_separation_names(const gx_device * dev,
                const gs_devn_params * pparams, const char * pname,
                int name_size, int component_type)
{
    fixed_colorant_name * pcolor = pparams->std_colorant_names;
    int color_component_number = 0;

    /* Check if the component is in the process color model list. */
    if (pcolor) {
        while( *pcolor) {
            if (compare_color_names(pname, name_size, *pcolor, strlen(*pcolor)))
                return color_component_number;
            pcolor++;
            color_component_number++;
        }
    }
    /* For some devices, Tags is part of the process color model list. If so,
     * that throws us off here since it is thrown at the end of the list. Adjust. */
    if (device_encodes_tags(dev)) {
        color_component_number--;
    }

    return check_separation_names(dev, pparams, pname, name_size,
        component_type, color_component_number);
}

/*
 * This routine will check to see if the color component name  match those
 * that are available amoung the current device's color components.
 *
 * Parameters:
 *   dev - pointer to device data structure.
 *   pname - pointer to name (zero termination not required)
 *   nlength - length of the name
 *   component_type - separation name or not
 *   pdevn_params - pointer to device's DeviceN paramters
 *   pequiv_colors - pointer to equivalent color structure (may be NULL)
 *
 * This routine returns a positive value (0 to n) which is the device colorant
 * number if the name is found.  It returns GX_DEVICE_COLOR_MAX_COMPONENTS if
 * the color component is found but is not being used due to the
 * SeparationOrder device parameter.  It returns a negative value if not found.
 *
 * This routine will also add separations to the device if space is
 * available.
 */
int
devn_get_color_comp_index(gx_device * dev, gs_devn_params * pdevn_params,
                    equivalent_cmyk_color_params * pequiv_colors,
                    const char * pname, int name_size, int component_type,
                    int auto_spot_colors)
{
    int num_order = pdevn_params->num_separation_order_names;
    int color_component_number = 0;
    int num_res_comps = pdevn_params->num_reserved_components;
    int max_spot_colors = GX_DEVICE_MAX_SEPARATIONS - pdevn_params->num_std_colorant_names - num_res_comps;

    /*
     * Check if the component is in either the process color model list
     * or in the SeparationNames list.
     */
    color_component_number = check_pcm_and_separation_names(dev, pdevn_params,
                                        pname, name_size, component_type);

    /* If we have a valid component */
    if (color_component_number >= 0) {
        /* Check if the component is in the separation order map. */
        if (num_order)
            color_component_number =
                pdevn_params->separation_order_map[color_component_number];
        else
            /*
             * We can have more spot colors than we can image.  We simply
             * ignore the component (i.e. treat it the same as we would
             * treat a component that is not in the separation order map).
             * Note:  Most device do not allow more spot colors than we can
             * image.  (See the options for auto_spot_color in gdevdevn.h.)
             */
            if (color_component_number >= dev->color_info.max_components)
                color_component_number = GX_DEVICE_COLOR_MAX_COMPONENTS;

        return color_component_number;
    }
    /*
     * The given name does not match any of our current components or
     * separations.  Check if we should add the spot color to our list.
     * If the SeparationOrder parameter has been specified then we should
     * already have our complete list of desired spot colorants.
     */
    if (component_type != SEPARATION_NAME ||
            auto_spot_colors == NO_AUTO_SPOT_COLORS ||
            pdevn_params->num_separation_order_names != 0)
        return -1;      /* Do not add --> indicate colorant unknown. */

    /* Make sure the name is not "None"  this is sometimes
       within a DeviceN list and should not be added as one of the
       separations.  */
    if (strncmp(pname, "None", name_size) == 0) {
        return -1;
    }

    /*
     * Check if we have room for another spot colorant.
     */
    if (auto_spot_colors == ENABLE_AUTO_SPOT_COLORS)
        /* limit max_spot_colors to what the device can handle given max_components */
        max_spot_colors = min(max_spot_colors,
                              dev->color_info.max_components - pdevn_params->num_std_colorant_names - num_res_comps);
    if (pdevn_params->separations.num_separations < max_spot_colors) {
        byte * sep_name;
        gs_separations * separations = &pdevn_params->separations;
        int sep_num = separations->num_separations++;
        /* We have a new spot colorant - put in stable memory to avoid "restore" */
        sep_name = gs_alloc_bytes(dev->memory->stable_memory, name_size, "devn_get_color_comp_index");
        if (sep_name == NULL) {
            separations->num_separations--;	/* we didn't add it */
            return -1;
        }
        memcpy(sep_name, pname, name_size);
        separations->names[sep_num].size = name_size;
        separations->names[sep_num].data = sep_name;
        color_component_number = sep_num + pdevn_params->num_std_colorant_names;
        if (color_component_number >= dev->color_info.max_components)
            color_component_number = GX_DEVICE_COLOR_MAX_COMPONENTS;
        else
            pdevn_params->separation_order_map[color_component_number] =
                                               color_component_number;

        if (pequiv_colors != NULL) {
            /* Indicate that we need to find equivalent CMYK color. */
            pequiv_colors->color[sep_num].color_info_valid = false;
            pequiv_colors->all_color_info_valid = false;
        }
    }

    return color_component_number;
}

#define set_param_array(a, d, s)\
  (a.data = d, a.size = s, a.persistent = false);

/* Get parameters.  We provide a default CRD. */
int
devn_get_params(gx_device * pdev, gs_param_list * plist,
    gs_devn_params * pdevn_params, equivalent_cmyk_color_params * pequiv_colors)
{
    int code, i = 0, spot_num;
    bool seprs = false;
    gs_param_string_array scna;
    gs_param_string_array sona;
    gs_param_int_array equiv_cmyk;
    /* there are 5 ints  per colorant in equiv_elements: a valid flag and an int for C, M, Y and K */
    int equiv_elements[5 * GX_DEVICE_MAX_SEPARATIONS] = { 0 }; /* 5 * max_colors */
    /* limit in case num_separations in pdevn_params exceeds what is expected. */
    int num_separations = min(pdevn_params->separations.num_separations, sizeof(equiv_elements)/(5*sizeof(int)));


    set_param_array(scna, NULL, 0);
    set_param_array(sona, NULL, 0);

    if (pequiv_colors != NULL) {
        for (spot_num = 0; spot_num < num_separations; spot_num++) {
            equiv_elements[i++] = pequiv_colors->color[spot_num].color_info_valid ? 1 : 0;
            equiv_elements[i++] = pequiv_colors->color[spot_num].c;
            equiv_elements[i++] = pequiv_colors->color[spot_num].m;
            equiv_elements[i++] = pequiv_colors->color[spot_num].y;
            equiv_elements[i++] = pequiv_colors->color[spot_num].k;
        }
    }

    equiv_cmyk.data = equiv_elements;
    equiv_cmyk.size = i;
    equiv_cmyk.persistent = false;

    if ( (code = sample_device_crd_get_params(pdev, plist, "CRDDefault")) < 0 ||
         (code = param_write_name_array(plist, "SeparationColorNames", &scna)) < 0 ||
         (code = param_write_name_array(plist, "SeparationOrder", &sona)) < 0 ||
         (code = param_write_bool(plist, "Separations", &seprs)) < 0)
        return code;

    if (pdev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE &&
        (code = param_write_int(plist, "PageSpotColors", &(pdevn_params->page_spot_colors))) < 0)
        return code;

    if (pdevn_params->separations.num_separations > 0)
        code = param_write_int_array(plist, ".EquivCMYKColors", &equiv_cmyk);

    return code;
}
#undef set_param_array

#define BEGIN_ARRAY_PARAM(pread, pname, pa, psize, e)\
    BEGIN\
    switch (code = pread(plist, (param_name = pname), &(pa))) {\
      case 0:\
        if ((pa).size != psize) {\
          ecode = gs_note_error(gs_error_rangecheck);\
          (pa).data = 0;        /* mark as not filled */\
        } else
#define END_ARRAY_PARAM(pa, e)\
        goto e;\
      default:\
        ecode = code;\
e:      param_signal_error(plist, param_name, ecode);\
      case 1:\
        (pa).data = 0;          /* mark as not filled */\
    }\
    END

/*
 * Utility routine for handling DeviceN related parameters.  This routine
 * may modify the color_info, devn_params, and the equiv_cmyk_colors fields.
 *
 * Note:  This routine does not restore values in case of a problem.  This
 * is left to the caller.
 */
int
devn_put_params(gx_device * pdev, gs_param_list * plist,
    gs_devn_params * pdevn_params, equivalent_cmyk_color_params * pequiv_colors)
{
    int code = 0, ecode, i;
    gs_param_name param_name;
    int npcmcolors = pdevn_params->num_std_colorant_names;
    int num_spot = pdevn_params->separations.num_separations;
    bool num_spot_changed = false;
    int num_order = pdevn_params->num_separation_order_names;
    int max_sep = pdevn_params->max_separations;
    int page_spot_colors = pdevn_params->page_spot_colors;
    gs_param_string_array scna;         /* SeparationColorNames array */
    gs_param_string_array sona;         /* SeparationOrder names array */
    gs_param_int_array equiv_cmyk;      /* equivalent_cmyk_color_params */
    int num_res_comps = pdevn_params->num_reserved_components;

    /* Get the SeparationOrder names */
    BEGIN_ARRAY_PARAM(param_read_name_array, "SeparationOrder",
                                        sona, sona.size, sone)
    {
        break;
    } END_ARRAY_PARAM(sona, sone);
    if (sona.data != 0 && sona.size > pdev->color_info.max_components) {
        param_signal_error(plist, "SeparationOrder", gs_error_rangecheck);
        return_error(gs_error_rangecheck);
    }

    /* Get the SeparationColorNames */
    BEGIN_ARRAY_PARAM(param_read_name_array, "SeparationColorNames",
                                        scna, scna.size, scne)
    {
        break;
    } END_ARRAY_PARAM(scna, scne);
    if (scna.data != 0 && scna.size > pdev->color_info.max_components) {
        param_signal_error(plist, "SeparationColorNames", gs_error_rangecheck);
        return_error(gs_error_rangecheck);
    }
    /* Get the equivalent_cmyk_color_params -- array is N * 5 elements */
    BEGIN_ARRAY_PARAM(param_read_int_array, ".EquivCMYKColors",
                                        equiv_cmyk, equiv_cmyk.size, equiv_cmyk_e)
    {
        break;
    } END_ARRAY_PARAM(equiv_cmyk, equiv_cmyk_e);
    if (equiv_cmyk.data != 0 && equiv_cmyk.size > 5 * pdev->color_info.max_components) {
        param_signal_error(plist, ".EquivCMYKColors", gs_error_rangecheck);
        return_error(gs_error_rangecheck);
    }

    /* Separations are only valid with a subtractive color model */
    if (pdev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE) {
        /*
         * Process the SeparationColorNames.  Remove any names that already
         * match the process color model colorant names for the device.
         */
        if (scna.data != 0) {
            int num_names = scna.size, num_std_names = 0;
            fixed_colorant_names_list pcomp_names = pdevn_params->std_colorant_names;

            num_spot = pdevn_params->separations.num_separations;
            for (i = 0; i < num_names; i++) {
                /* Verify that the name is not one of our process colorants */
                if (!check_process_color_names(pcomp_names, &scna.data[i])) {
                    byte * sep_name;
                    int name_size = scna.data[i].size;

                    /* We have a new separation */
                    sep_name = (byte *)gs_alloc_bytes(pdev->memory,
                        name_size, "devicen_put_params_no_sep_order");
                    if (sep_name == NULL) {
                        param_signal_error(plist, "SeparationColorNames", gs_error_VMerror);
                        return_error(gs_error_VMerror);
                    }
                    memcpy(sep_name, scna.data[i].data, name_size);
                    pdevn_params->separations.names[num_spot].size = name_size;
                    pdevn_params->separations.names[num_spot].data = sep_name;
                    if (pequiv_colors != NULL) {
                        /* Indicate that we need to find equivalent CMYK color. */
                        pequiv_colors->color[num_spot].color_info_valid = false;
                        pequiv_colors->all_color_info_valid = false;
                    }
                    num_spot++;
                    num_spot_changed = true;
                }
            }
            /* You would expect that pdevn_params->num_std_colorant_names would have this value but it does not.
             * That appears to be copied from the 'ncomps' of the device and that has to be the number of components
             * in the 'base' colour model, 1, 3 or 4 for Gray, RGB or CMYK. Other kinds of DeviceN devices can have
             * additional standard names, eg Tags, or Artifex Orange and Artifex Green, but these are not counted in
             * the num_std_colorant_names. They are, however, listed in pdevn_params->std_colorant_names (when is a
             * std_colorant_name not a std_colorant_name ?), which is checked above to see if a SeparationOrder name is one
             * of the inks we are already dealing with. If it is, then we *don't* add it to num_spots.
             * So we need to actually count the number of colorants in std_colorant_names to make sure that we
             * don't exceed the maximum number of components.
             */
            num_std_names = count_process_color_names(pcomp_names);
            if (num_std_names + num_spot > pdev->color_info.max_components) {
                param_signal_error(plist, "SeparationColorNames", gs_error_rangecheck);
                return_error(gs_error_rangecheck);
            }

            for (i = pdevn_params->separations.num_separations; i < num_spot; i++)
                pdevn_params->separation_order_map[i + pdevn_params->num_std_colorant_names] =
                i + pdevn_params->num_std_colorant_names;
            pdevn_params->separations.num_separations = num_spot;
        }
        /* Process any .EquivCMYKColors info */
        if (equiv_cmyk.data != 0 && pequiv_colors != 0) {
            int spot_num = 0;

            for (i=0; i < equiv_cmyk.size; i += 5) {	/* valid, C, M, Y, K for each equiv_color */
                if (equiv_cmyk.data[i] == 0) {
                    /* This occurs if we've added a spot, but not yet set it's equiv color */
                    pequiv_colors->color[spot_num].color_info_valid = false;
                    pequiv_colors->all_color_info_valid = false;
                } else {
                    pequiv_colors->color[spot_num].color_info_valid = true;
                    pequiv_colors->color[spot_num].c = (frac)(equiv_cmyk.data[i+1]);
                    pequiv_colors->color[spot_num].m = (frac)(equiv_cmyk.data[i+2]);
                    pequiv_colors->color[spot_num].y = (frac)(equiv_cmyk.data[i+3]);
                    pequiv_colors->color[spot_num].k = (frac)(equiv_cmyk.data[i+4]);
                }
                spot_num++;
            }
        }
        /*
         * Process the SeparationOrder names.
         */
        if (sona.data != 0) {
            int comp_num;

            num_order = sona.size;
            for (i = 0; i < num_order; i++) {
                /*
                 * Check if names match either the process color model or
                 * SeparationColorNames.  If not then error.
                 */
                if ((comp_num = (*dev_proc(pdev, get_color_comp_index))
                                (pdev, (const char *)sona.data[i].data,
                                sona.data[i].size, SEPARATION_NAME)) < 0) {
                    param_signal_error(plist, "SeparationOrder", gs_error_rangecheck);
                    return_error(gs_error_rangecheck);
                }
                pdevn_params->separation_order_map[i] = comp_num;
                /* If the device enabled AUTO_SPOT_COLORS some separations may */
                /* have been added. Adjust num_spots if so.                    */
                if (num_spot != pdevn_params->separations.num_separations) {
                    num_spot = pdevn_params->separations.num_separations;
                    num_spot_changed = true;
                }
            }
        }
        /*
         * Adobe says that MaxSeparations is supposed to be 'read only'
         * however we use this to allow the specification of the maximum
         * number of separations.  Memory is allocated for the specified
         * number of separations.  This allows us to then accept separation
         * colors in color spaces even if they we not specified at the start
         * of the image file.
         */
        code = param_read_int(plist, param_name = "MaxSeparations", &max_sep);
        switch (code) {
            default:
                param_signal_error(plist, param_name, code);
            case 1:
                break;
            case 0:
                if (max_sep < 1 || max_sep > GX_DEVICE_COLOR_MAX_COMPONENTS) {
                    param_signal_error(plist, "MaxSeparations", gs_error_rangecheck);
                    return_error(gs_error_rangecheck);
                }
        }
        /*
         * The PDF interpreter scans the resources for pages to try to
         * determine the number of spot colors.  (Unfortuneately there is
         * no way to determine the number of spot colors for a PS page
         * except to interpret the entire page.)  The spot color count for
         * a PDF page may be high since there may be spot colors in a PDF
         * page's resources that are not used.  However this does give us
         * an upper limit on the number of spot colors.  A value of -1
         * indicates that the number of spot colors in unknown (a PS file).
         */
        code = param_read_int(plist, param_name = "PageSpotColors",
                                                        &page_spot_colors);
        switch (code) {
            default:
                param_signal_error(plist, param_name, code);
            case 1:
                break;
            case 0:
                if (page_spot_colors < -1) {
                    param_signal_error(plist, "PageSpotColors", gs_error_rangecheck);
                    return_error(gs_error_rangecheck);
                }
                if (page_spot_colors > pdev->color_info.max_components - pdevn_params->num_std_colorant_names - num_res_comps)
                    page_spot_colors = pdev->color_info.max_components - pdevn_params->num_std_colorant_names - num_res_comps;
                    /* Need to leave room for the process colors (and tags!) in GX_DEVICE_COLOR_MAX_COMPONENTS  */
        }
        /*
         * The DeviceN device can have zero components if nothing has been
         * specified.  This causes some problems so force at least one
         * component until something is specified.
         */
        if (!pdev->color_info.num_components)
            pdev->color_info.num_components = 1;
        /*
         * Update the number of device components if we have changes in
         * SeparationColorNames, SeparationOrder, or MaxSeparations.
         */
        if (num_spot_changed || pdevn_params->max_separations != max_sep ||
                    pdevn_params->num_separation_order_names != num_order ||
                    pdevn_params->page_spot_colors != page_spot_colors) {
            int has_tags = !!(pdev->graphics_type_tag & GS_DEVICE_ENCODES_TAGS);
            pdevn_params->separations.num_separations = num_spot;
            pdevn_params->num_separation_order_names = num_order;
            pdevn_params->max_separations = max_sep;
            pdevn_params->page_spot_colors = page_spot_colors;
            if (max_sep != 0)
                 pdev->color_info.max_components = max_sep;
            /*
             * If we have SeparationOrder specified then the number of
             * components is given by the number of names in the list.
             * Otherwise check if the MaxSeparations parameter has specified
             * a value.  If so then use that value, otherwise use the number
             * of ProcessColorModel components plus the number of
             * SeparationColorNames is used.
             */
            pdev->color_info.num_components = (num_order)
                ? num_order
                : (page_spot_colors >= 0)
                    ? npcmcolors + page_spot_colors
                    : pdev->color_info.max_components;
            pdev->color_info.num_components += has_tags;

            if (pdev->color_info.num_components >
                    pdev->color_info.max_components)
                pdev->color_info.num_components =
                        pdev->color_info.max_components;

            if (pdev->color_info.num_components > pdev->num_planar_planes)
                pdev->num_planar_planes = pdev->color_info.num_components;

            /*
             * See earlier comment about the depth and non compressed
             * pixel encoding.
             */
            if (pdev->num_planar_planes)
                pdev->color_info.depth = bpc_to_depth(pdev->num_planar_planes,
                                                      pdevn_params->bitspercomponent);
            else
                pdev->color_info.depth = bpc_to_depth(pdev->color_info.num_components,
                                                      pdevn_params->bitspercomponent);
        }
    }
    if (code >= 0)
    {
        int ecode = dev_proc(pdev, dev_spec_op)(pdev, gxdso_adjust_colors, NULL, 0);
        if (ecode < 0 && ecode != gs_error_undefined)
            code = ecode;
    }
    return code;
}

/* Free the copied deviceN parameters */
void
devn_free_params(gx_device *thread_cdev)
{
    gs_devn_params *devn_params;
    int k;

    devn_params = dev_proc(thread_cdev, ret_devn_params)(thread_cdev);
    if (devn_params == NULL) return;

    for (k = 0; k < devn_params->separations.num_separations; k++) {
        gs_free_object(thread_cdev->memory,
                       devn_params->separations.names[k].data,
                       "devn_free_params");
        devn_params->separations.names[k].data = NULL;
    }

    for (k = 0; k < devn_params->pdf14_separations.num_separations; k++) {
        gs_free_object(thread_cdev->memory,
                       devn_params->pdf14_separations.names[k].data,
                       "devn_free_params");
        devn_params->pdf14_separations.names[k].data = NULL;
    }
}

/* This is used to copy the deviceN parameters from the parent clist device to the
   individual thread clist devices for multi-threaded rendering */
int
devn_copy_params(gx_device * psrcdev, gx_device * pdesdev)
{
    gs_devn_params *src_devn_params, *des_devn_params;
    int code = 0;
    int k;

    /* Get pointers to the parameters */
    src_devn_params = dev_proc(psrcdev, ret_devn_params)(psrcdev);
    des_devn_params = dev_proc(pdesdev, ret_devn_params)(pdesdev);
    if (src_devn_params == NULL || des_devn_params == NULL)
        return gs_note_error(gs_error_undefined);

    /* First the easy items */
    des_devn_params->bitspercomponent = src_devn_params->bitspercomponent;
    des_devn_params->max_separations = src_devn_params->max_separations;
    des_devn_params->num_separation_order_names =
        src_devn_params->num_separation_order_names;
    des_devn_params->num_std_colorant_names =
        src_devn_params->num_std_colorant_names;
    des_devn_params->page_spot_colors = src_devn_params->page_spot_colors;
    des_devn_params->std_colorant_names = src_devn_params->std_colorant_names;
    des_devn_params->separations.num_separations
        = src_devn_params->separations.num_separations;
    /* Now the more complex structures */
    /* Spot color names */
    for (k = 0; k < des_devn_params->separations.num_separations; k++) {
        byte * sep_name;
        int name_size = src_devn_params->separations.names[k].size;
        sep_name = (byte *)gs_alloc_bytes(pdesdev->memory->stable_memory,
                                          name_size, "devn_copy_params");
        if (sep_name == NULL) {
            return_error(gs_error_VMerror);
        }
        memcpy(sep_name, src_devn_params->separations.names[k].data, name_size);
        des_devn_params->separations.names[k].size = name_size;
        des_devn_params->separations.names[k].data = sep_name;
    }
    /* Order map */
    memcpy(des_devn_params->separation_order_map,
           src_devn_params->separation_order_map, sizeof(gs_separation_map));

    /* Handle the PDF14 items if they are there */
    des_devn_params->pdf14_separations.num_separations
        = src_devn_params->pdf14_separations.num_separations;
    for (k = 0; k < des_devn_params->pdf14_separations.num_separations; k++) {
        byte * sep_name;
        int name_size = src_devn_params->pdf14_separations.names[k].size;
        sep_name = (byte *)gs_alloc_bytes(pdesdev->memory->stable_memory,
                                          name_size, "devn_copy_params");
        if (sep_name == NULL) {
            return_error(gs_error_VMerror);
        }
        memcpy(sep_name, src_devn_params->pdf14_separations.names[k].data,
               name_size);
        des_devn_params->pdf14_separations.names[k].size = name_size;
        des_devn_params->pdf14_separations.names[k].data = sep_name;
    }
    return code;
}

static int
compare_equivalent_cmyk_color_params(const equivalent_cmyk_color_params *pequiv_colors1, const equivalent_cmyk_color_params *pequiv_colors2)
{
  int i;
  if (pequiv_colors1->all_color_info_valid != pequiv_colors2->all_color_info_valid)
    return(1);
  for (i=0;  i<GX_DEVICE_MAX_SEPARATIONS;  i++) {
    if (pequiv_colors1->color[i].color_info_valid != pequiv_colors2->color[i].color_info_valid)
      return(1);
    if (pequiv_colors1->color[i].c                != pequiv_colors2->color[i].c               )
      return(1);
    if (pequiv_colors1->color[i].m                != pequiv_colors2->color[i].m               )
      return(1);
    if (pequiv_colors1->color[i].y                != pequiv_colors2->color[i].y               )
      return(1);
    if (pequiv_colors1->color[i].k                != pequiv_colors2->color[i].k               )
      return(1);
  }
  return(0);
}

static bool separations_equal(const gs_separations *p1, const gs_separations *p2)
{
    int k;

    if (p1->num_separations != p2->num_separations)
        return false;
    for (k = 0; k < p1->num_separations; k++) {
        if (p1->names[k].size != p2->names[k].size)
            return false;
        else if (p1->names[k].size > 0) {
            if (memcmp(p1->names[k].data, p2->names[k].data, p1->names[k].size) != 0)
                return false;
        }
    }
    return true;
}

static bool devn_params_equal(const gs_devn_params *p1, const gs_devn_params *p2)
{
    if (p1->bitspercomponent != p2->bitspercomponent)
        return false;
    if (p1->max_separations != p2->max_separations)
        return false;
    if (p1->num_separation_order_names != p2->num_separation_order_names)
        return false;
    if (p1->num_std_colorant_names != p2->num_std_colorant_names)
        return false;
    if (p1->page_spot_colors != p2->page_spot_colors)
        return false;
    if (!separations_equal(&p1->pdf14_separations, &p2->pdf14_separations))
        return false;
    if (!separations_equal(&p1->separations, &p2->separations))
        return false;
    if (memcmp(p1->separation_order_map, p2->separation_order_map, sizeof(gs_separation_map)) != 0)
        return false;
    if (p1->std_colorant_names != p2->std_colorant_names)
        return false;
    return true;
}

int
devn_generic_put_params(gx_device *pdev, gs_param_list *plist,
                        gs_devn_params *pdevn_params, equivalent_cmyk_color_params *pequiv_colors,
                        int is_printer)
{
    int code;
    /* Save current data in case we have a problem */
    gx_device_color_info save_info = pdev->color_info;
    gs_devn_params saved_devn_params = *pdevn_params;
    equivalent_cmyk_color_params saved_equiv_colors;
    int save_planes = pdev->num_planar_planes;

    if (pequiv_colors != NULL)
        saved_equiv_colors = *pequiv_colors;

    /* Use utility routine to handle parameters */
    code = devn_put_params(pdev, plist, pdevn_params, pequiv_colors);

    /* Check for default printer parameters */
    if (is_printer && code >= 0)
        code = gdev_prn_put_params(pdev, plist);

    /* If we have an error then restore original data. */
    if (code < 0) {
        pdev->color_info = save_info;
        *pdevn_params = saved_devn_params;
        if (pequiv_colors != NULL)
           *pequiv_colors = saved_equiv_colors;
        return code;
    }

    /* If anything changed, then close the device, etc. */
    if (!gx_color_info_equal(&pdev->color_info, &save_info) ||
        !devn_params_equal(pdevn_params, &saved_devn_params) ||
        (pequiv_colors != NULL &&
            compare_equivalent_cmyk_color_params(pequiv_colors, &saved_equiv_colors)) ||
        pdev->num_planar_planes != save_planes) {
        gx_device *parent_dev = pdev;
        gx_device_color_info resave_info = pdev->color_info;
        int resave_planes = pdev->num_planar_planes;

        while (parent_dev->parent != NULL)
            parent_dev = parent_dev->parent;

        /* Temporarily restore the old color_info, so the close happens with
         * the old version. In particular this allows Nup to flush properly. */
        pdev->color_info = save_info;
        pdev->num_planar_planes = save_planes;
        gs_closedevice(parent_dev);
        /* Then put the shiny new color_info back in. */
        pdev->color_info = resave_info;
        pdev->num_planar_planes = resave_planes;
        /* Reset the separable and linear shift, masks, bits. */
        set_linear_color_bits_mask_shift(pdev);
    }
    /*
     * Also check for parameters which are being passed from the PDF 1.4
     * compositior clist write device.  This device needs to pass info
     * to the PDF 1.4 compositor clist reader device.  However this device
     * is not crated until the clist is being read.  Thus we have to buffer
     * this info in the output device.   (This is only needed for devices
     * which support spot colors.)
     */
    code = pdf14_put_devn_params(pdev, pdevn_params, plist);
    return code;
}

/*
 * Utility routine for handling DeviceN related parameters in a
 * standard raster printer type device.
 */
int
devn_printer_put_params(gx_device *pdev, gs_param_list *plist,
        gs_devn_params *pdevn_params, equivalent_cmyk_color_params *pequiv_colors)
{
    return devn_generic_put_params(pdev, plist, pdevn_params, pequiv_colors, 1);
}

/*
 * Free a set of separation names
 */
void
free_separation_names(gs_memory_t * mem,
                gs_separations * pseparation)
{
    int i;

    /* Discard the sub levels. */
    for (i = 0; i < pseparation->num_separations; i++) {
        gs_free_object(mem->stable_memory, pseparation->names[i].data,
                                "free_separation_names");
        pseparation->names[i].data = NULL;
        pseparation->names[i].size = 0;
    }
    pseparation->num_separations = 0;
    return;
}

/* ***************** The spotcmyk and devicen devices ***************** */

/* Define the device parameters. */
#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* The device descriptor */
static dev_proc_open_device(spotcmyk_prn_open);
static dev_proc_print_page(spotcmyk_print_page);

/* GC procedures */

static
ENUM_PTRS_WITH(gx_devn_prn_device_enum_ptrs, gx_devn_prn_device *pdev)
{
    if (index < pdev->devn_params.separations.num_separations)
        ENUM_RETURN(pdev->devn_params.separations.names[index].data);
    ENUM_PREFIX(st_device_printer,
                    pdev->devn_params.separations.num_separations);
}

ENUM_PTRS_END
static RELOC_PTRS_WITH(gx_devn_prn_device_reloc_ptrs, gx_devn_prn_device *pdev)
{
    RELOC_PREFIX(st_device_printer);
    {
        int i;

        for (i = 0; i < pdev->devn_params.separations.num_separations; ++i) {
            RELOC_PTR(gx_devn_prn_device, devn_params.separations.names[i].data);
        }
    }
}
RELOC_PTRS_END

void
gx_devn_prn_device_finalize(const gs_memory_t *cmem, void *vpdev)
{
    devn_free_params((gx_device*) vpdev);
    gx_device_finalize(cmem, vpdev);
}

/* Even though gx_devn_prn_device_finalize is the same as gx_device_finalize, */
/* we need to implement it separately because st_composite_final */
/* declares all 3 procedures as private. */
static void
static_gx_devn_prn_device_finalize(const gs_memory_t *cmem, void *vpdev)
{
    gx_devn_prn_device_finalize(cmem, vpdev);
}

gs_public_st_composite_final(st_gx_devn_prn_device, gx_devn_prn_device,
    "gx_devn_prn_device", gx_devn_prn_device_enum_ptrs, gx_devn_prn_device_reloc_ptrs,
    static_gx_devn_prn_device_finalize);

static void
devicen_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, open_device, spotcmyk_prn_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page_seekable);
    set_dev_proc(dev, close_device, gdev_prn_close);
    set_dev_proc(dev, get_params, gx_devn_prn_get_params);
    set_dev_proc(dev, put_params, gx_devn_prn_put_params);
    set_dev_proc(dev, get_page_device, gx_page_device_get_page_device);
    set_dev_proc(dev, get_color_mapping_procs, gx_devn_prn_get_color_mapping_procs);
    set_dev_proc(dev, get_color_comp_index, gx_devn_prn_get_color_comp_index);
    set_dev_proc(dev, encode_color, gx_devn_prn_encode_color);
    set_dev_proc(dev, decode_color, gx_devn_prn_decode_color);
    set_dev_proc(dev, update_spot_equivalent_colors, gx_devn_prn_update_spot_equivalent_colors);
    set_dev_proc(dev, ret_devn_params, gx_devn_prn_ret_devn_params);
}

fixed_colorant_name DeviceGrayComponents[] = {
        "Gray",
        0              /* List terminator */
};

fixed_colorant_name DeviceRGBComponents[] = {
        "Red",
        "Green",
        "Blue",
        0              /* List terminator */
};

fixed_colorant_name DeviceCMYKComponents[] = {
        "Cyan",
        "Magenta",
        "Yellow",
        "Black",
        0               /* List terminator */
};

#define gx_devn_prn_device_body(init, dname, ncomp, pol, depth, mg, mc, cn)\
    std_device_full_body_type_extended(gx_devn_prn_device, init, dname,\
          &st_gx_devn_prn_device,\
          (int)((long)(DEFAULT_WIDTH_10THS) * (X_DPI) / 10),\
          (int)((long)(DEFAULT_HEIGHT_10THS) * (Y_DPI) / 10),\
          X_DPI, Y_DPI,\
          GX_DEVICE_COLOR_MAX_COMPONENTS,       /* MaxComponents */\
          ncomp,                /* NumComp */\
          pol,                  /* Polarity */\
          depth, 0,             /* Depth, GrayIndex */\
          mg, mc,               /* MaxGray, MaxColor */\
          mg + 1, mc + 1,       /* DitherGray, DitherColor */\
          GX_CINFO_SEP_LIN,     /* Linear & Separable */\
          cn,                   /* Process color model name */\
          0, 0,                 /* offsets */\
          0, 0, 0, 0            /* margins */\
        ),\
        prn_device_body_rest_(spotcmyk_print_page)

/*
 * Example device with CMYK and spot color support
 */
const gx_devn_prn_device gs_spotcmyk_device =
{
    gx_devn_prn_device_body(devicen_initialize_device_procs, "spotcmyk",
                            4, GX_CINFO_POLARITY_SUBTRACTIVE, 4, 1, 1,
                            "DeviceCMYK"),
    /* DeviceN device specific parameters */
    { 1,                        /* Bits per color - must match ncomp, depth, etc. above */
      DeviceCMYKComponents,     /* Names of color model colorants */
      4,                        /* Number colorants for CMYK */
      0,                        /* MaxSeparations has not been specified */
      -1,                       /* PageSpotColors has not been specified */
      {0},                      /* SeparationNames */
      0,                        /* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 } /* Initial component SeparationOrder */
    }
};

/*
 * Example DeviceN color device
 */
const gx_devn_prn_device gs_devicen_device =
{
    gx_devn_prn_device_body(devicen_initialize_device_procs, "devicen",
                            4, GX_CINFO_POLARITY_SUBTRACTIVE, 32, 255, 255,
                            "DeviceCMYK"),
    /* DeviceN device specific parameters */
    { 8,                        /* Bits per color - must match ncomp, depth, etc. above */
      DeviceCMYKComponents,     /* Names of color model colorants */
      4,                        /* Number colorants for CMYK */
      0,                        /* MaxSeparations has not been specified */
      -1,                       /* PageSpotColors has not been specified */
      {0},                      /* SeparationNames */
      0,                        /* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 } /* Initial component SeparationOrder */
    }
};

/* Open the psd devices */
int
spotcmyk_prn_open(gx_device * pdev)
{
    int code = gdev_prn_open(pdev);

    while (pdev->child)
        pdev = pdev->child;

    set_linear_color_bits_mask_shift(pdev);
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    return code;
}

/* Color mapping routines for the spotcmyk device */

static void
gray_cs_to_spotcmyk_cm(const gx_device * dev, frac gray, frac out[])
{
    int * map = ((gx_devn_prn_device *) dev)->devn_params.separation_order_map;

    gray_cs_to_devn_cm(dev, map, gray, out);
}

static void
rgb_cs_to_spotcmyk_cm(const gx_device * dev, const gs_gstate *pgs,
                                   frac r, frac g, frac b, frac out[])
{
    int * map = ((gx_devn_prn_device *) dev)->devn_params.separation_order_map;

    rgb_cs_to_devn_cm(dev, map, pgs, r, g, b, out);
}

static void
cmyk_cs_to_spotcmyk_cm(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    int * map = ((gx_devn_prn_device *) dev)->devn_params.separation_order_map;

    cmyk_cs_to_devn_cm(dev, map, c, m, y, k, out);
}

static const gx_cm_color_map_procs spotCMYK_procs = {
    gray_cs_to_spotcmyk_cm, rgb_cs_to_spotcmyk_cm, cmyk_cs_to_spotcmyk_cm
};

const gx_cm_color_map_procs *
gx_devn_prn_get_color_mapping_procs(const gx_device * dev, const gx_device **map_dev)
{
    *map_dev = dev;
    return &spotCMYK_procs;
}

/*
 * Encode a list of colorant values into a gx_color_index_value.
 */
gx_color_index
gx_devn_prn_encode_color(gx_device *dev, const gx_color_value colors[])
{
    int bpc = ((gx_devn_prn_device *)dev)->devn_params.bitspercomponent;
    gx_color_index color = 0;
    int i = 0;
    uchar ncomp = dev->color_info.num_components;
    COLROUND_VARS;

    COLROUND_SETUP(bpc);
    for (; i<ncomp; i++) {
        color <<= bpc;
        color |= COLROUND_ROUND(colors[i]);
    }
    return (color == gx_no_color_index ? color ^ 1 : color);
}

/*
 * Decode a gx_color_index value back to a list of colorant values.
 */
int
gx_devn_prn_decode_color(gx_device * dev, gx_color_index color, gx_color_value * out)
{
    int bpc = ((gx_devn_prn_device *)dev)->devn_params.bitspercomponent;
    int mask = (1 << bpc) - 1;
    int i = 0;
    uchar ncomp = dev->color_info.num_components;
    COLDUP_VARS;

    COLDUP_SETUP(bpc);
    for (; i<ncomp; i++) {
        out[ncomp - i - 1] = COLDUP_DUP(color & mask);
        color >>= bpc;
    }
    return 0;
}

/* Get parameters. */
int
gx_devn_prn_get_params(gx_device *dev, gs_param_list *plist)
{
    gx_devn_prn_device *pdev = (gx_devn_prn_device *)dev;
    int code = gdev_prn_get_params(dev, plist);

    if (code < 0)
        return code;
    return devn_get_params(dev, plist, &pdev->devn_params,
                           &pdev->equiv_cmyk_colors);
}

/* Set parameters. */
int
gx_devn_prn_put_params(gx_device *dev, gs_param_list *plist)
{
    gx_devn_prn_device *pdev = (gx_devn_prn_device *)dev;

    return devn_printer_put_params(dev, plist, &pdev->devn_params,
                                   &pdev->equiv_cmyk_colors);
}

/*
 *  Device proc for returning a pointer to DeviceN parameter structure
 */
gs_devn_params *
gx_devn_prn_ret_devn_params(gx_device * dev)
{
    gx_devn_prn_device *pdev = (gx_devn_prn_device *)dev;

    return &pdev->devn_params;
}

const gs_devn_params *
gx_devn_prn_ret_devn_params_const(const gx_device * dev)
{
    const gx_devn_prn_device *pdev = (const gx_devn_prn_device *)dev;

    return &pdev->devn_params;
}

/*
 *  Device proc for updating the equivalent CMYK color for spot colors.
 */
int
gx_devn_prn_update_spot_equivalent_colors(gx_device *dev, const gs_gstate * pgs, const gs_color_space *pcs)
{
    gx_devn_prn_device *pdev = (gx_devn_prn_device *)dev;

    return update_spot_equivalent_cmyk_colors(dev, pgs, pcs, &pdev->devn_params,
                                              &pdev->equiv_cmyk_colors);
}

/*
 * This routine will check to see if the color component name  match those
 * that are available amoung the current device's color components.
 *
 * Parameters:
 *   dev - pointer to device data structure.
 *   pname - pointer to name (zero termination not required)
 *   nlength - length of the name
 *
 * This routine returns a positive value (0 to n) which is the device colorant
 * number if the name is found.  It returns GX_DEVICE_COLOR_MAX_COMPONENTS if
 * the colorant is not being used due to a SeparationOrder device parameter.
 * It returns a negative value if not found.
 */
int
gx_devn_prn_get_color_comp_index(gx_device * dev, const char * pname,
                                        int name_size, int component_type)
{
    gx_devn_prn_device *pdev = (gx_devn_prn_device *)dev;

    return devn_get_color_comp_index(dev,
                                     &pdev->devn_params,
                                     &pdev->equiv_cmyk_colors,
                                     pname,
                                     name_size,
                                     component_type,
                                     ENABLE_AUTO_SPOT_COLORS);
}

/*
 * This routine will extract a specified set of bits from a buffer and pack
 * them into a given buffer.
 *
 * Parameters:
 *   source - The source of the data
 *   dest - The destination for the data
 *   depth - The size of the bits per pixel - must be a multiple of 8
 *   first_bit - The location of the first data bit (LSB).
 *   bit_width - The number of bits to be extracted.
 *   npixel - The number of pixels.
 *
 * Returns:
 *   Length of the output line (in bytes)
 *   Data in dest.
 */
int
repack_data(byte * source, byte * dest, int depth, int first_bit,
                int bit_width, int npixel)
{
    int in_nbyte = depth >> 3;          /* Number of bytes per input pixel */
    int out_nbyte = bit_width >> 3;     /* Number of bytes per output pixel */
    gx_color_index mask = 1;
    gx_color_index data;
    int i, j, length = 0;
    byte temp;
    byte * out = dest;
    int in_bit_start = 8 - depth;
    int out_bit_start = 8 - bit_width;
    int in_byte_loc = in_bit_start, out_byte_loc = out_bit_start;

    mask = (mask << bit_width) - 1;
    for (i=0; i<npixel; i++) {
        /* Get the pixel data */
        if (!in_nbyte) {                /* Multiple pixels per byte */
            data = *source;
            data >>= in_byte_loc;
            in_byte_loc -= depth;
            if (in_byte_loc < 0) {      /* If finished with byte */
                in_byte_loc = in_bit_start;
                source++;
            }
        }
        else {                          /* One or more bytes per pixel */
            data = *source++;
            for (j=1; j<in_nbyte; j++)
                data = (data << 8) + *source++;
        }
        data >>= first_bit;
        data &= mask;

        /* Put the output data */
        if (!out_nbyte) {               /* Multiple pixels per byte */
            temp = (byte)(*out & ~(mask << out_byte_loc));
            *out = (byte)(temp | (data << out_byte_loc));
            out_byte_loc -= bit_width;
            if (out_byte_loc < 0) {     /* If finished with byte */
                out_byte_loc = out_bit_start;
                out++;
            }
        }
        else {                          /* One or more bytes per pixel */
            *out++ = (byte)(data >> ((out_nbyte - 1) * 8));
            for (j=1; j<out_nbyte; j++) {
                *out++ = (byte)(data >> ((out_nbyte - 1 - j) * 8));
            }
        }
    }
    /* Return the number of bytes in the destination buffer. */
    if (out_byte_loc != out_bit_start) {        /* If partially filled last byte */
        *out = *out & ((~0) << out_byte_loc);   /* Mask unused part of last byte */
        out++;
    }
    length = out - dest;
    return length;
}

static int devn_write_pcx_file(gx_device_printer * pdev, char * filename, int ncomp,
                            int bpc, int pcmlinelength);
/*
 * This is an example print page routine for a DeviceN device.  This routine
 * will handle a DeviceN, a CMYK with spot colors, or an RGB process color model.
 *
 * This routine creates several output files.  If the process color model is
 * RGB or CMYK then a bit image file is created which contains the data for the
 * process color model data.  This data is put into the given file stream.
 * I.e. into the output file specified by the user.  This file is not created
 * for the DeviceN process color model.  A separate bit image file is created
 * is created for the data for each of the given spot colors.  The names for
 * these files are created by taking the given output file name and appending
 * "sn" (where n is the spot color number 0 to ...) to the output file name.
 * The results are unknown if the output file is stdout etc.
 *
 * After the bit image files are created, then a set of PCX format files are
 * created from the bit image files.  This files have a ".pcx" appended to the
 * end of the files.  Thus a CMYK process color model with two spot colors
 * would end up with a total of six files being created.  (xxx, xxxs0, xxxs1,
 * xxx.pcx, xxxs0.pcx, and xxxs1.pcx).
 *
 * I do not assume that any users will actually want to create all of these
 * different files.  However I wanted to show an example of how each of the
 * spot * colorants could be unpacked from the process color model colorants.
 * The bit images files are an easy way to show this without the complication
 * of trying to put the data into a specific format.  However I do not have a
 * tool which will display the bit image data directly so I needed to convert
 * it to a form which I can view.  Thus the PCX format files are being created.
 * Note:  The PCX implementation is not complete.  There are many (most)
 * combinations of bits per pixel and number of colorants that are not supported.
 */
static int
spotcmyk_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{
    int line_size = gdev_mem_bytes_per_scan_line((gx_device *) pdev);
    byte *in = gs_alloc_bytes(pdev->memory, line_size, "spotcmyk_print_page(in)");
    byte *buf = gs_alloc_bytes(pdev->memory, line_size + 3, "spotcmyk_print_page(buf)");
    const gx_devn_prn_device * pdevn = (gx_devn_prn_device *) pdev;
    uint npcmcolors = pdevn->devn_params.num_std_colorant_names;
    uchar ncomp = pdevn->color_info.num_components;
    int depth = pdevn->color_info.depth;
    int nspot = pdevn->devn_params.separations.num_separations;
    int bpc = pdevn->devn_params.bitspercomponent;
    int lnum = 0, bottom = pdev->height;
    int width = pdev->width;
    gp_file * spot_file[GX_DEVICE_COLOR_MAX_COMPONENTS] = {0};
    uint i;
    int code = 0;
    int first_bit;
    int pcmlinelength = 0; /* Initialize against indeterminizm in case of pdev->height == 0. */
    int linelength[GX_DEVICE_COLOR_MAX_COMPONENTS];
    byte *data;
    char *spotname = (char *)gs_alloc_bytes(pdev->memory, gp_file_name_sizeof, "spotcmyk_print_page(spotname)");

    if (in == NULL || buf == NULL || spotname == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto prn_done;
    }
    /*
     * Check if the SeparationOrder list has changed the order of the process
     * color model colorants. If so then we will treat all colorants as if they
     * are spot colors.
     */
    for (i = 0; i < npcmcolors; i++)
        if (pdevn->devn_params.separation_order_map[i] != i)
            break;
    if (i < npcmcolors || ncomp < npcmcolors) {
        nspot = ncomp;
        npcmcolors = 0;
    }

    /* Open the output files for the spot colors */
    for(i = 0; i < nspot; i++) {
        gs_snprintf(spotname, gp_file_name_sizeof, "%ss%d", pdevn->fname, i);
        code = gs_add_control_path(pdev->memory, gs_permit_file_writing, spotname);
        if (code < 0)
            goto prn_done;
        spot_file[i] = gp_fopen(pdev->memory, spotname, "wb");
        (void)gs_remove_control_path(pdev->memory, gs_permit_file_writing, spotname);
        if (spot_file[i] == NULL) {
            code = gs_note_error(gs_error_VMerror);
            goto prn_done;
        }
    }

    /* Now create the output bit image files */
    for (; lnum < bottom; ++lnum) {
        code = gdev_prn_get_bits(pdev, lnum, in, &data);
        if (code < 0)
            goto prn_done;
        /* Now put the pcm data into the output file */
        if (npcmcolors) {
            first_bit = bpc * (ncomp - npcmcolors);
            pcmlinelength = repack_data(data, buf, depth, first_bit, bpc * npcmcolors, width);
            gp_fwrite(buf, 1, pcmlinelength, prn_stream);
        }
        /* Put spot color data into the output files */
        for (i = 0; i < nspot; i++) {
            first_bit = bpc * (nspot - 1 - i);
            linelength[i] = repack_data(data, buf, depth, first_bit, bpc, width);
            gp_fwrite(buf, 1, linelength[i], spot_file[i]);
        }
    }

    /* Close the bit image files */
    for(i = 0; i < nspot; i++) {
        gp_fclose(spot_file[i]);
        spot_file[i] = NULL;
    }

    /* Now convert the bit image files into PCX files */
    if (npcmcolors) {
        code = devn_write_pcx_file(pdev, (char *) &pdevn->fname,
                                npcmcolors, bpc, pcmlinelength);
        if (code < 0)
            goto prn_done;
    }
    for(i = 0; i < nspot; i++) {
        gs_snprintf(spotname, gp_file_name_sizeof, "%ss%d", pdevn->fname, i);
        code = devn_write_pcx_file(pdev, spotname, 1, bpc, linelength[i]);
        if (code < 0)
            goto prn_done;
    }

    /* Clean up and exit */
  prn_done:
    for(i = 0; i < nspot; i++) {
        if (spot_file[i] != NULL)
            gp_fclose(spot_file[i]);
    }
    if (in != NULL)
        gs_free_object(pdev->memory, in, "spotcmyk_print_page(in)");
    if (buf != NULL)
        gs_free_object(pdev->memory, buf, "spotcmyk_print_page(buf)");
    if (spotname != NULL)
        gs_free_object(pdev->memory, spotname, "spotcmyk_print_page(spotname)");
    return code;
}

/*
 * We are using the PCX output format.  This is done for simplicity.
 * Much of the following code was copied from gdevpcx.c.
 */

/* ------ Private definitions ------ */

/* All two-byte quantities are stored LSB-first! */
#if ARCH_IS_BIG_ENDIAN
#  define assign_ushort(a,v) a = ((v) >> 8) + ((v) << 8)
#else
#  define assign_ushort(a,v) a = (v)
#endif

typedef struct pcx_header_s {
    byte manuf;                 /* always 0x0a */
    byte version;
#define version_2_5                     0
#define version_2_8_with_palette        2
#define version_2_8_without_palette     3
#define version_3_0 /* with palette */  5
    byte encoding;              /* 1=RLE */
    byte bpp;                   /* bits per pixel per plane */
    ushort x1;                  /* X of upper left corner */
    ushort y1;                  /* Y of upper left corner */
    ushort x2;                  /* x1 + width - 1 */
    ushort y2;                  /* y1 + height - 1 */
    ushort hres;                /* horz. resolution (dots per inch) */
    ushort vres;                /* vert. resolution (dots per inch) */
    byte palette[16 * 3];       /* color palette */
    byte reserved;
    byte nplanes;               /* number of color planes */
    ushort bpl;                 /* number of bytes per line (uncompressed) */
    ushort palinfo;
#define palinfo_color   1
#define palinfo_gray    2
    byte xtra[58];              /* fill out header to 128 bytes */
} pcx_header;

/* Define the prototype header. */
static const pcx_header pcx_header_prototype =
{
    10,                         /* manuf */
    0,                          /* version (variable) */
    1,                          /* encoding */
    0,                          /* bpp (variable) */
    00, 00,                     /* x1, y1 */
    00, 00,                     /* x2, y2 (variable) */
    00, 00,                     /* hres, vres (variable) */
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,        /* palette (variable) */
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    0,                          /* reserved */
    0,                          /* nplanes (variable) */
    00,                         /* bpl (variable) */
    00,                         /* palinfo (variable) */
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /* xtra */
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

/* Forward declarations */
static void devn_pcx_write_rle(const byte *, const byte *, int, gp_file *);
static int devn_pcx_write_page(gx_device_printer * pdev, gp_file * infile,
    int linesize, gp_file * outfile, pcx_header * phdr, bool planar, int depth);

static const byte pcx_cmyk_palette[16 * 3] =
{
    0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x0f, 0x0f, 0x00,
    0xff, 0x00, 0xff, 0x0f, 0x00, 0x0f, 0xff, 0x00, 0x00, 0x0f, 0x00, 0x00,
    0x00, 0xff, 0xff, 0x00, 0x0f, 0x0f, 0x00, 0xff, 0x00, 0x00, 0x0f, 0x00,
    0x00, 0x00, 0xff, 0x00, 0x00, 0x0f, 0x1f, 0x1f, 0x1f, 0x0f, 0x0f, 0x0f,
};

static const byte pcx_ega_palette[16 * 3] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0xaa, 0x00, 0xaa, 0x00, 0x00, 0xaa, 0xaa,
    0xaa, 0x00, 0x00, 0xaa, 0x00, 0xaa, 0xaa, 0xaa, 0x00, 0xaa, 0xaa, 0xaa,
    0x55, 0x55, 0x55, 0x55, 0x55, 0xff, 0x55, 0xff, 0x55, 0x55, 0xff, 0xff,
    0xff, 0x55, 0x55, 0xff, 0x55, 0xff, 0xff, 0xff, 0x55, 0xff, 0xff, 0xff
};

/*
 * This routine will set up the revision and palatte for the output
 * file.
 *
 * Please note that this routine does not currently handle all possible
 * combinations of bits and planes.
 *
 * Input parameters:
 *   pdev - Pointer to device data structure
 *   file - output file
 *   header - The header structure to hold the data.
 *   bits_per_plane - The number of bits per plane.
 *   num_planes - The number of planes.
 */
static bool
devn_setup_pcx_header(gx_device_printer * pdev, pcx_header * phdr, int num_planes, int bits_per_plane)
{
    bool planar = true; /* Invalid cases could cause an indeterminizm. */

    *phdr = pcx_header_prototype;
    phdr->bpp = bits_per_plane;
    phdr->nplanes = num_planes;

    switch (num_planes) {
        case 1:
            switch (bits_per_plane) {
                case 1:
                        phdr->version = version_2_8_with_palette;
                        assign_ushort(phdr->palinfo, palinfo_gray);
                        memcpy((byte *) phdr->palette, "\000\000\000\377\377\377", 6);
                        planar = false;
                        break;
                case 2:                         /* Not defined */
                        break;
                case 4:
                        phdr->version = version_2_8_with_palette;
                        memcpy((byte *) phdr->palette, pcx_ega_palette, sizeof(pcx_ega_palette));
                        planar = true;
                        break;
                case 5:                         /* Not defined */
                        break;
                case 8:
                        phdr->version = version_3_0;
                        assign_ushort(phdr->palinfo, palinfo_gray);
                        planar = false;
                        break;
                case 16:                        /* Not defined */
                        break;
            }
            break;
        case 2:
            switch (bits_per_plane) {
                case 1:                         /* Not defined */
                        break;
                case 2:                         /* Not defined */
                        break;
                case 4:                         /* Not defined */
                        break;
                case 5:                         /* Not defined */
                        break;
                case 8:                         /* Not defined */
                        break;
                case 16:                        /* Not defined */
                        break;
            }
            break;
        case 3:
            switch (bits_per_plane) {
                case 1:                         /* Not defined */
                        break;
                case 2:                         /* Not defined */
                        break;
                case 4:                         /* Not defined */
                        break;
                case 5:                         /* Not defined */
                        break;
                case 8:
                        phdr->version = version_3_0;
                        assign_ushort(phdr->palinfo, palinfo_color);
                        planar = true;
                        break;
                case 16:                        /* Not defined */
                        break;
            }
            break;
        case 4:
            switch (bits_per_plane) {
                case 1:
                        phdr->version = 2;
                        memcpy((byte *) phdr->palette, pcx_cmyk_palette,
                                sizeof(pcx_cmyk_palette));
                        planar = false;
                        phdr->bpp = 4;
                        phdr->nplanes = 1;
                        break;
                case 2:                         /* Not defined */
                        break;
                case 4:                         /* Not defined */
                        break;
                case 5:                         /* Not defined */
                        break;
                case 8:                         /* Not defined */
                        break;
                case 16:                        /* Not defined */
                        break;
            }
            break;
    }
    return planar;
}

/* Write a palette on a file. */
static int
pc_write_mono_palette(gx_device * dev, uint max_index, gp_file * file)
{
    uint i, c;
    gx_color_value rgb[3];

    for (i = 0; i < max_index; i++) {
        rgb[0] = rgb[1] = rgb[2] = i << 8;
        for (c = 0; c < 3; c++) {
            byte b = gx_color_value_to_byte(rgb[c]);

            gp_fputc(b, file);
        }
    }
    return 0;
}
/*
 * This routine will send any output data required at the end of a file
 * for a particular combination of planes and bits per plane.
 *
 * Please note that most combinations do not require anything at the end
 * of a data file.
 *
 * Input parameters:
 *   pdev - Pointer to device data structure
 *   file - output file
 *   header - The header structure to hold the data.
 *   bits_per_plane - The number of bits per plane.
 *   num_planes - The number of planes.
 */
static int
devn_finish_pcx_file(gx_device_printer * pdev, gp_file * file, pcx_header * header, int num_planes, int bits_per_plane)
{
    switch (num_planes) {
        case 1:
            switch (bits_per_plane) {
                case 1:                         /* Do nothing */
                        break;
                case 2:                         /* Not defined */
                        break;
                case 4:                         /* Do nothing */
                        break;
                case 5:                         /* Not defined */
                        break;
                case 8:
                        gp_fputc(0x0c, file);
                        return pc_write_mono_palette((gx_device *) pdev, 256, file);
                case 16:                        /* Not defined */
                        break;
            }
            break;
        case 2:
            switch (bits_per_plane) {
                case 1:                         /* Not defined */
                        break;
                case 2:                         /* Not defined */
                        break;
                case 4:                         /* Not defined */
                        break;
                case 5:                         /* Not defined */
                        break;
                case 8:                         /* Not defined */
                        break;
                case 16:                        /* Not defined */
                        break;
            }
            break;
        case 3:
            switch (bits_per_plane) {
                case 1:                         /* Not defined */
                        break;
                case 2:                         /* Not defined */
                        break;
                case 4:                         /* Not defined */
                        break;
                case 5:                         /* Not defined */
                        break;
                case 8:                         /* Do nothing */
                        break;
                case 16:                        /* Not defined */
                        break;
            }
            break;
        case 4:
            switch (bits_per_plane) {
                case 1:                         /* Do nothing */
                        break;
                case 2:                         /* Not defined */
                        break;
                case 4:                         /* Not defined */
                        break;
                case 5:                         /* Not defined */
                        break;
                case 8:                         /* Not defined */
                        break;
                case 16:                        /* Not defined */
                        break;
            }
            break;
    }
    return 0;
}

/* Send the page to the printer. */
static int
devn_write_pcx_file(gx_device_printer * pdev, char * filename, int ncomp,
                            int bpc, int linesize)
{
    pcx_header header;
    int code;
    bool planar;
    char *outname = (char *)gs_alloc_bytes(pdev->memory, gp_file_name_sizeof, "devn_write_pcx_file(outname)");
    gp_file * in = NULL;
    gp_file * out = NULL;
    int depth = bpc_to_depth(ncomp, bpc);

    if (outname == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }

    code = gs_add_control_path(pdev->memory, gs_permit_file_reading, filename);
    if (code < 0)
        goto done;

    in = gp_fopen(pdev->memory, filename, "rb");
    if (!in) {
        code = gs_note_error(gs_error_invalidfileaccess);
        goto done;
    }
    gs_snprintf(outname, gp_file_name_sizeof, "%s.pcx", filename);
    code = gs_add_control_path(pdev->memory, gs_permit_file_writing, outname);
    if (code < 0)
        goto done;
    out = gp_fopen(pdev->memory, outname, "wb");
    if (!out) {
        code = gs_note_error(gs_error_invalidfileaccess);
        goto done;
    }

    if (ncomp == 4 && bpc == 8) {
        ncomp = 3;		/* we will convert 32-bit to 24-bit RGB */
    }
    planar = devn_setup_pcx_header(pdev, &header, ncomp, bpc);
    code = devn_pcx_write_page(pdev, in, linesize, out, &header, planar, depth);
    if (code >= 0)
        code = devn_finish_pcx_file(pdev, out, &header, ncomp, bpc);

done:
    (void)gs_remove_control_path(pdev->memory, gs_permit_file_reading, filename);
    (void)gs_remove_control_path(pdev->memory, gs_permit_file_writing, outname);
    if (in)
      gp_fclose(in);
    if (out)
      gp_fclose(out);

    if (outname)
      gs_free_object(pdev->memory, outname, "spotcmyk_print_page(outname)");

    return code;
}

/* Write out a page in PCX format. */
/* This routine is used for all formats. */
/* The caller has set header->bpp, nplanes, and palette. */
static int
devn_pcx_write_page(gx_device_printer * pdev, gp_file * infile, int linesize, gp_file * outfile,
               pcx_header * phdr, bool planar, int depth)
{
    int raster = linesize;
    uint rsize = ROUND_UP((pdev->width * phdr->bpp + 7) >> 3, 2);       /* PCX format requires even */
    int height = pdev->height;
    uint lsize = raster + rsize;
    byte *line = gs_alloc_bytes(pdev->memory, lsize, "pcx file buffer");
    byte *rgb_buff = NULL;
    byte *plane = line + raster;
    bool convert_to_rgb = false;
    int y;
    int code = 0;               /* return code */

    if (line == 0)              /* can't allocate line buffer */
        return_error(gs_error_VMerror);
    if (pdev->color_info.num_components == 4 && depth == 32) {
        rgb_buff = gs_alloc_bytes(pdev->memory, lsize, "pcx_rgb_buff");
        if (rgb_buff == 0)              /* can't allocate line buffer */
            return_error(gs_error_VMerror);
        raster = (raster * 3) / 4;	/* will be rounded up to even later */
        depth = 24;			/* we will be writing 24-bit rgb */
        convert_to_rgb = true;
    }

    /* Fill in the other variable entries in the header struct. */

    assign_ushort(phdr->x2, pdev->width - 1);
    assign_ushort(phdr->y2, height - 1);
    assign_ushort(phdr->hres, (int)pdev->x_pixels_per_inch);
    assign_ushort(phdr->vres, (int)pdev->y_pixels_per_inch);
    assign_ushort(phdr->bpl, (planar || depth == 1 ? rsize :
                              raster + (raster & 1)));

    /* Write the header. */

    if (gp_fwrite((const char *)phdr, 1, 128, outfile) < 128) {
        code = gs_error_ioerror;
        goto pcx_done;
    }
    /* Write the contents of the image. */
    for (y = 0; y < height; y++) {
        byte *row = line;
        byte *end;

        code = gp_fread(line, sizeof(byte), linesize, infile);
        if (code < 0)
            break;
        /* If needed, convert to rgb */
        if (convert_to_rgb) {
            int i;
            byte *row_in = line;

            /* Transform the data. */
            row = rgb_buff;	/* adjust to converted output buffer */
            for (i=0; i < linesize; i += 4) {
                *row++ = ((255 - row_in[0]) * (255 - row_in[3])) / 255;
                *row++ = ((255 - row_in[1]) * (255 - row_in[3])) / 255;
                *row++ = ((255 - row_in[2]) * (255 - row_in[3])) / 255;
                row_in += 4;
            }
            row = rgb_buff;	/* adjust to converted output buffer */
        }
        end = row + raster;
        if (!planar) {          /* Just write the bits. */
            if (raster & 1) {   /* Round to even, with predictable padding. */
                *end = end[-1];
                ++end;
            }
            devn_pcx_write_rle(row, end, 1, outfile);
        } else
            switch (depth) {

                case 4:
                    {
                        byte *pend = plane + rsize;
                        int shift;

                        for (shift = 0; shift < 4; shift++) {
                            register byte *from, *to;
                            register int bright = 1 << shift;
                            register int bleft = bright << 4;

                            for (from = row, to = plane;
                                 from < end; from += 4
                                ) {
                                *to++ =
                                    (from[0] & bleft ? 0x80 : 0) |
                                    (from[0] & bright ? 0x40 : 0) |
                                    (from[1] & bleft ? 0x20 : 0) |
                                    (from[1] & bright ? 0x10 : 0) |
                                    (from[2] & bleft ? 0x08 : 0) |
                                    (from[2] & bright ? 0x04 : 0) |
                                    (from[3] & bleft ? 0x02 : 0) |
                                    (from[3] & bright ? 0x01 : 0);
                            }
                            /* We might be one byte short of rsize. */
                            if (to < pend)
                                *to = to[-1];
                            devn_pcx_write_rle(plane, pend, 1, outfile);
                        }
                    }
                    break;

                case 24:
                    {
                        int pnum;

                        for (pnum = 0; pnum < 3; ++pnum) {
                            devn_pcx_write_rle(row + pnum, row + raster, 3, outfile);
                            if (pdev->width & 1)
                                gp_fputc(0, outfile);              /* pad to even */
                        }
                    }
                    break;

                default:
                    code = gs_note_error(gs_error_rangecheck);
                    goto pcx_done;

            }
        code = 0;
    }

  pcx_done:
    if (rgb_buff != NULL)
        gs_free_object(pdev->memory, rgb_buff, "pcx_rgb_buff");
    gs_free_object(pdev->memory, line, "pcx file buffer");

    return code;
}

/* ------ Internal routines ------ */

/* Write one line in PCX run-length-encoded format. */
static void
devn_pcx_write_rle(const byte * from, const byte * end, int step, gp_file * file)
{  /*
    * The PCX format theoretically allows encoding runs of 63
    * identical bytes, but some readers can't handle repetition
    * counts greater than 15.
    */
#define MAX_RUN_COUNT 15
    int max_run = step * MAX_RUN_COUNT;

    while (from < end) {
        byte data = *from;

        from += step;
        if (from >= end || data != *from) {
            if (data >= 0xc0)
                gp_fputc(0xc1, file);
        } else {
            const byte *start = from;

            while ((from < end) && (*from == data))
                from += step;
            /* Now (from - start) / step + 1 is the run length. */
            while (from - start >= max_run) {
                gp_fputc(0xc0 + MAX_RUN_COUNT, file);
                gp_fputc(data, file);
                start += max_run;
            }
            if (from > start || data >= 0xc0)
                gp_fputc((from - start) / step + 0xc1, file);
        }
        gp_fputc(data, file);
    }
#undef MAX_RUN_COUNT
}
