/* Copyright (C) 2001-2008 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* ICCBased color operators */

#include "math_.h"
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gsstruct.h"
#include "gxcspace.h"		/* gscolor2.h requires gscspace.h */
#include "stream.h"
#include "files.h"
#include "gscolor2.h"
#include "gsicc.h"
#include "estack.h"
#include "idict.h"
#include "idparam.h"
#include "igstate.h"
#include "icie.h"
#include "ialloc.h"
#include "zicc.h"
#include "gsicc_manage.h"
#include "gx.h"
#include "gxistate.h"
#include "gserror.h"
#include "gsicc_create.h"
#include "gsicc_profilecache.h"

int seticc(i_ctx_t * i_ctx_p, int ncomps, ref *ICCdict, float *range_buff)
{
    int                     code, k;
    gs_color_space *        pcs;
    gs_color_space *  palt_cs;
    ref *                   pstrmval;
    stream *                s = 0L;
    cmm_profile_t           *picc_profile;
    gs_imager_state *       pis = (gs_imager_state *)igs;
    int                     i;
    ref *                   pnameval;
    static const char *const icc_std_profile_names[] = {
	    GSICC_STANDARD_PROFILES
	};
    static const char *const icc_std_profile_keys[] = {
	    GSICC_STANDARD_PROFILES_KEYS
	};

    palt_cs = gs_currentcolorspace(igs);

    /* verify the DataSource entry */
    if (dict_find_string(ICCdict, "DataSource", &pstrmval) <= 0)
        return_error(e_undefined);
    check_read_file(i_ctx_p, s, pstrmval);

    /* build the color space object */
    code = gs_cspace_build_ICC(&pcs, NULL, gs_state_memory(igs));
    if (code < 0)
        return gs_rethrow(code, "building color space object");

    /*  For now, dump the profile into a buffer
        and obtain handle from the buffer when we need it. 
        We may want to change this later.
        This depends to some degree on what the CMS is capable of doing.
        I don't want to get bogged down on stream I/O at this point.
        Note also, if we are going to be putting these into the clist we will 
        want to have this buffer. */
    /* Check if we have the /Name entry.  This is used to associate with
       specs that have enumerated types to indicate sRGB sGray etc */
    if (dict_find_string(ICCdict, "Name", &pnameval) > 0){
        uint size = r_size(pnameval);
        char *str = (char *)gs_alloc_bytes(gs_state_memory(igs), size+1, "seticc");
        memcpy(str, (const char *)pnameval->value.bytes, size);
        str[size] = 0;

        /* Compare this to the standard profile names */
        picc_profile = NULL;
        for (k = 0; k < GSICC_NUMBER_STANDARD_PROFILES; k++) {
            if ( strcmp( str, icc_std_profile_keys[k] ) == 0 ) {
                picc_profile = gsicc_get_profile_handle_file(icc_std_profile_names[k], 
                    strlen(icc_std_profile_names[k]), gs_state_memory(igs));
                break;
    }
        }
    } else {
        picc_profile = gsicc_profile_new(s, gs_state_memory(igs), NULL, 0);
    }
    if (picc_profile == NULL) {
        rc_decrement(picc_profile,"seticc");
        rc_decrement(pcs,"seticc");
        return gs_throw(e_unknownerror,
              "couldn't create icc profile from stream");
    }
    code = gsicc_set_gscs_profile(pcs, picc_profile, gs_state_memory(igs));
    if (code < 0) {
        rc_decrement(picc_profile,"seticc");
        rc_decrement(pcs,"seticc");
        return gs_rethrow(code, "installing the profile");
    }
    picc_profile->num_comps = ncomps;

    /* We have to get the profile handle due to the fact that we need to know
       if it has a data space that is CIELAB */
    picc_profile->profile_handle = 
        gsicc_get_profile_handle_buffer(picc_profile->buffer,
                                        picc_profile->buffer_size);
    if (picc_profile->profile_handle == NULL) {
        /* Free up everything, the profile is not valid. We will end up going 
           ahead and using a default based upon the number of components */
        rc_decrement(picc_profile,"seticc");
        rc_decrement(pcs,"seticc");
        return gs_rethrow(-1, "installing the profile");
    }
    picc_profile->data_cs = gscms_get_profile_data_space(picc_profile->profile_handle);

    /* Lets go ahead and get the hash code and check if we match one of the default spaces */
    /* Later we may want to delay this, but for now lets go ahead and do it */
    gsicc_init_hash_cs(picc_profile, pis);

    /* Set the range according to the data type that is associated with the 
       ICC input color type.  Occasionally, we will run into CIELAB to CIELAB
       profiles for spot colors in PDF documents. These spot colors are typically described
       as separation colors with tint transforms that go from a tint value
       to a linear mapping between the CIELAB white point and the CIELAB tint
       color.  This results in a CIELAB value that we need to use to fill.  We
       need to detect this to make sure we do the proper scaling of the data.  For
       CIELAB images in PDF, the source is always normal 8 or 16 bit encoded data
       in the range from 0 to 255 or 0 to 65535.  In that case, there should not
       be any encoding and decoding to CIELAB.  The PDF content will not include
       an ICC profile but will set the color space to \Lab.  In this case, we use
       our seticc_lab operation to install the LAB to LAB profile, but we detect
       that we did that through the use of the is_lab flag in the profile descriptor.
       When then avoid the CIELAB encode and decode */
    if (picc_profile->data_cs == gsCIELAB) {
    /* If the input space to this profile is CIELAB, then we need to adjust the limits */
        /* See ICC spec ICC.1:2004-10 Section 6.3.4.2 and 6.4.  I don't believe we need to
           worry about CIEXYZ profiles or any of the other odds ones.  Need to check that though
           at some point. */
        picc_profile->Range.ranges[0].rmin = 0.0;
        picc_profile->Range.ranges[0].rmax = 100.0;
        picc_profile->Range.ranges[1].rmin = -128.0;
        picc_profile->Range.ranges[1].rmax = 127.0;
        picc_profile->Range.ranges[2].rmin = -128.0;
        picc_profile->Range.ranges[2].rmax = 127.0;
    } else {
        for (i = 0; i < ncomps; i++) {
            picc_profile->Range.ranges[i].rmin = range_buff[2 * i];
            picc_profile->Range.ranges[i].rmax = range_buff[2 * i + 1];
    } 
	}
    /* Set the color space.  We are done.  No joint cache here... */
    code = gs_setcolorspace(igs, pcs);
    /* In this case, we already have a ref count of 2 on the icc profile 
       one for when it was created and one for when it was set.  We really
       only want one here so adjust */
    rc_decrement(picc_profile,"seticc");
    /* Remove the ICC dict from the stack */
    pop(1);
	return code;
}

/*
 *   <dict>  .seticcspace  -
 *
 * Create an ICCBased color space and set it to be the current color space.
 *
 * The PostScript structure of an ICCBased color space is that same as that
 * for a CIEBased* color space:
 *
 *     [ /ICCBased <dictionary> ]
 *
 * As is the for other .setcie*space operators, the operand dictionary rather
 * than the complete color space array is on the stack when this operator
 * is inovked.
 *
 * At the time this procedure is called, the alternative color space for
 * the ICCBased color space is expected to be the current color space,
 * whether that space was explicitly specified or implied by the number
 * of components in the ICCBased color space dictionary. This is consistent
 * with the handling of alternative spaces in Separation, DeviceN, and
 * Indexed color spaces. Unlike the "zset*space" routines for those spaces,
 * however, the current code does not attempt to build the color space
 * "in place" in the graphic state.
 *
 * The procedure that invokes this operator will already have checked that
 * the operand is a dictionary, is readable, and defines the key /N
 * (number of components).
 */
static int
zseticcspace(i_ctx_t * i_ctx_p)
{
    os_ptr                  op = osp;
    int                     code;
    gs_color_space *  palt_cs;
    ref *                   pnval;
    ref *                   pstrmval;
    stream *                s;
    int                     i, ncomps;
    float                   range_buff[8];
    static const float      dflt_range[8] = { 0, 1,   0, 1,   0, 1,   0, 1 };

    code = dict_find_string(op, "N", &pnval);
    if (code < 0)
	return code;
    ncomps = pnval->value.intval;
    if (2*ncomps > sizeof(range_buff)/sizeof(range_buff[0]))
        return_error(e_rangecheck);
    /* verify the DataSource entry */
    if (dict_find_string(op, "DataSource", &pstrmval) <= 0)
        return_error(e_undefined);
    check_read_file(i_ctx_p, s, pstrmval);
    /*
     * Verify that the current color space can be a alternative color space.
     * The check for ICCBased color space is a hack to avoid introducing yet
     * another category indicator into the gs_color_space_type structur.
     */
    palt_cs = gs_currentcolorspace(igs);
    if ( !palt_cs->type->can_be_alt_space                                ||
         gs_color_space_get_index(palt_cs) == gs_color_space_index_ICC  )
        return_error(e_rangecheck);
    /*
     * Fetch and verify the Range array.
     *
     * The PDF documentation is unclear as to the purpose of this array.
     * Essentially all that is stated is that "These values must match the
     * information in the ICC profile" (PDF Reference, 2nd ed., p. 174).
     * If that is the case, why not use the information in the profile?
     * The only reason we can think of is range specification is intended
     * to be used to limit the range of values passed to the alternate
     * color space (the range may be smaller than the native range of values
     * provided by that color space).
     *
     * Because the icclib code will perform normalization based on color
     * space, we use the range values only to restrict the set of input
     * values; they are not used for normalization.
     */
    code = dict_floats_param( imemory, 
			      op,
                              "Range",
                              2 * ncomps,
                              range_buff,
                              dflt_range );
    for (i = 0; i < 2 * ncomps && range_buff[i + 1] >= range_buff[i]; i += 2)
        ;
    if (i != 2 * ncomps)
        return_error(e_rangecheck);
    return seticc(i_ctx_p, ncomps, op, range_buff);
}


/* Install a ICC type color space and use the ICC LABLUT profile. */
int
seticc_lab(i_ctx_t * i_ctx_p, float *white, float *black, float *range_buff)
{
    int                     code;
    gs_color_space *        pcs;
    gs_color_space *        palt_cs;
    gs_imager_state *       pis = (gs_imager_state *)igs;
    int                     i;
    static const char *const rfs = LAB_ICC;
    gs_param_string val, *pval;    

    val.data = (const byte *)rfs;
    val.size = strlen(rfs);
    val.persistent = true;
    pval = &val;

    palt_cs = gs_currentcolorspace(igs);
    /* build the color space object */
    code = gs_cspace_build_ICC(&pcs, NULL, gs_state_memory(igs));
    if (code < 0)
        return gs_rethrow(code, "building color space object");
    /* record the current space as the alternative color space */
    /* pcs->base_space = palt_cs;
    rc_increment_cs(palt_cs); */
    /* Get the lab profile.  It may already be set in the icc manager.
       If not then lets populate it.  */
    if (pis->icc_manager->lab_profile == NULL ) {
        /* This can't happen as the profile
           should be initialized during the
           setting of the user params */
        return gs_rethrow(code, "cannot find lab icc profile");
    } 
    /* Assign the LAB to LAB profile to this color space */
    code = gsicc_set_gscs_profile(pcs, pis->icc_manager->lab_profile, gs_state_memory(igs));
    rc_increment(pis->icc_manager->lab_profile);
    if (code < 0)
        return gs_rethrow(code, "installing the lab profile");
    pcs->cmm_icc_profile_data->Range.ranges[0].rmin = 0.0;
    pcs->cmm_icc_profile_data->Range.ranges[0].rmax = 100.0;
    for (i = 1; i < 3; i++) {
        pcs->cmm_icc_profile_data->Range.ranges[i].rmin = 
            range_buff[2 * (i-1)];
        pcs->cmm_icc_profile_data->Range.ranges[i].rmax = 
            range_buff[2 * (i-1) + 1];
    } 
    /* Set the color space.  We are done.  */
    code = gs_setcolorspace(igs, pcs);
    return code;
}

/* Install an ICC space from the PDF CalRGB or CalGray types */
int
seticc_cal(i_ctx_t * i_ctx_p, float *white, float *black, float *gamma, 
           float *matrix, int num_colorants, ulong dictkey)
{
    int                     code;
    gs_color_space *        pcs;
    gs_imager_state *       pis = (gs_imager_state *)igs;
    gs_memory_t             *mem = pis->memory; 
    int                     i;
    cmm_profile_t           *cal_profile;

    /* See if the color space is in the profile cache */
    pcs = gsicc_find_cs(dictkey, igs);
    if (pcs == NULL ) {
        /* build the color space object.  Since this is cached
           in the profile cache which is a member variable
           of the graphic state, we will want to use stable 
           memory here */
        code = gs_cspace_build_ICC(&pcs, NULL, mem->stable_memory);
        if (code < 0)
            return gs_rethrow(code, "building color space object");
        /* There is no alternate for this.  Perhaps we should set DeviceRGB? */
        pcs->base_space = NULL;
        /* Create the ICC profile from the CalRGB or CalGray parameters */
        cal_profile = gsicc_create_from_cal(white, black, gamma, matrix, 
                                            mem->stable_memory, num_colorants);
        if (cal_profile == NULL)
            return gs_rethrow(-1, "creating the cal profile");
        /* Assign the profile to this color space */
        code = gsicc_set_gscs_profile(pcs, cal_profile, mem->stable_memory);
        if (code < 0)
            return gs_rethrow(code, "installing the cal profile");
        for (i = 0; i < num_colorants; i++) {
            pcs->cmm_icc_profile_data->Range.ranges[i].rmin = 0;
            pcs->cmm_icc_profile_data->Range.ranges[i].rmax = 1;
        } 
        /* Add the color space to the profile cache */
        gsicc_add_cs(igs, pcs,dictkey);
    }
    /* Set the color space.  We are done.  */
    code = gs_setcolorspace(igs, pcs);
    return code;
}

const op_def    zicc_ll3_op_defs[] = {
    op_def_begin_ll3(),
    { "1.seticcspace", zseticcspace },
    op_def_end(0)
};
