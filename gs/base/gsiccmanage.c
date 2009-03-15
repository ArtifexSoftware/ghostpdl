/* Copyright (C) 2001-2009 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/
/*  GS ICC link manager.  Initial stubbing of functions.  */


#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsiccmanage.h"
#include "gxistate.h"
#include "gsicc_littlecms.h"
#include "gscie.h"
#include "smd5.h"  


#define ICC_CACHE_MAXLINKS 50   /* Note that the the external memory used to 
                                    maintain links in the CMS is generally not visible
                                    to GS.  For most CMS's the  links are 33x33x33x33x4 bytes
                                    at worst for a CMYK to CMYK MLUT which is about 4.5Mb per link.
                                    If the link were matrix based it would be much much smaller.
                                    We will likely want to do at least have an estimate of the 
                                    memory based used based upon how the CMS is configured.
                                    This will be done later.  For now, just limit the number
                                    of links. */

/* Structure pointer information */

gs_private_st_ptrs1(st_icc_profile, gsicc_profile_t, "gsiccmanage_profile",
		    icc_pro_enum_ptrs, icc_pro_reloc_ptrs,
		    ProfileRawBuf);

gs_private_st_ptrs6(st_icc_colorspace, gsicc_colorspace_t, "gsicc_colorspace",
		    icc_clr_enum_ptrs, icc_clr_reloc_ptrs,
		    ProfileData, pcrd, pcie_a, pcie_abc, pcie_def, pcie_defg); 


gs_private_st_ptrs4(st_icc_link, gsicc_link_t, "gsiccmanage_link",
		    icc_link_enum_ptrs, icc_link_reloc_ptrs,
		    LinkHandle, ContextPtr, NextLink, PrevLink);


gs_private_st_ptrs1(st_icc_linkcache, gsicc_link_cache_t, "gsiccmanage_linkcache",
		    icc_linkcache_enum_ptrs, icc_linkcache_reloc_ptrs,
		    ICCLink);


/**
 * gsicc_cache_new: Allocate a new ICC cache manager
 * Return value: Pointer to allocated manager, or NULL on failure.
 **/

gsicc_link_cache_t *
gsicc_cache_new(int cachesize, gs_memory_t *memory)
{

    gsicc_link_cache_t *result;

    result = gs_alloc_struct(memory, gsicc_link_cache_t, &st_icc_linkcache,
			     "gsiccmanage_linkcache_new");
    result->ICCLink = NULL;
    result->num_links = 0;

    return(result);

}


void
gsicc_add_link(gsicc_link_cache_t *link_cache, void *LinkHandle,void *ContextPtr, int64_t LinkHashCode, gs_memory_t *memory)
{

    gsicc_link_t *result, *Nextlink;

    result = gs_alloc_struct(memory, gsicc_link_t, &st_icc_link,
			     "gsiccmanage_link_new");

    result->ContextPtr = ContextPtr;
    result->LinkHandle = LinkHandle;
    result->LinkHashCode = LinkHashCode;
    result->ref_count = 1;
    
    if (link_cache->ICCLink != NULL){

        /* Add where ever we are right
           now.  Later we may want to
           do this differently.  */
        
        Nextlink = link_cache->ICCLink->NextLink;
        link_cache->ICCLink->NextLink = result;
        result->PrevLink = link_cache->ICCLink;
        result->NextLink = Nextlink;
        
        if (Nextlink != NULL){

            Nextlink->PrevLink = result;

        }

    } else {

        result->NextLink = NULL;
        result->PrevLink = NULL;
        link_cache->ICCLink = result;
    
    }

    link_cache->num_links++;

}



void
gsicc_cache_free(gsicc_link_cache_t *icc_cache, gs_memory_t *memory)
{

    /* ToDo: Need to free all the links and release them from the CMS first */

    gs_free_object(memory, icc_cache, "gsiccmanage_linkcache_free");
}



void
gsicc_link_free(gsicc_link_t *icc_link, gs_memory_t *memory)
{

    gscms_release_link(icc_link);
    gs_free_object(memory, icc_link, "gsiccmanage_link_free");

}



/*  Initialize the CMS 
    Prepare the ICC Manager
*/
void 
gsicc_create()
{




}


/*  Shut down the  CMS 
    and the ICC Manager
*/
void 
gsicc_destroy()
{


}

/*  This computes the hash code for the
    ICC data and assigns the code
    and the profile to DefaultGray
    member variable in the ICC manager */

void 
gsicc_set_gray_profile(gsicc_profile_t graybuffer)
{


}

/*  This computes the hash code for the
    ICC data and assigns the code
    and the profile to DefaultRGB
    member variable in the ICC manager */

void 
gsicc_set_rgb_profile(gsicc_profile_t rgbbuffer)
{


}

/*  This computes the hash code for the
    ICC data and assigns the code
    and the profile to DefaultCMYK
    member variable in the ICC manager */

void 
gsicc_set_cmyk_profile(gsicc_profile_t cmykbuffer)
{


}

/*  This computes the hash code for the
    device profile and assigns the code
    and the profile to the DeviceProfile
    member variable in the ICC Manager
    This should really occur only one time.
    This is different than gs_set_device_profile
    which sets the profile on the output
    device */

void 
gsicc_set_device_profile(gsicc_profile_t deviceprofile)
{



}

/* Set the named color profile in the Device manager */

void 
gsicc_set_device_named_color_profile(gsicc_profile_t nameprofile)
{


}

/* Set the ouput device linked profile */

void 
gsicc_set_device_linked_profile(gsicc_profile_t outputlinkedprofile )
{

}

/* Set the proofing profile, extract the header and compute
   its hash code */

void 
gsicc_set_proof_profile(gsicc_profile_t proofprofile )
{

}

/*  This will occur only if the device did not
    return a profie when asked for one.  This
    will set DeviceProfile member variable to the
    appropriate default device.  If output device
    has more than 4 channels you need to use
    gsicc_set_device_profile with the appropriate
    ICC profile.  */

void 
gsicc_load_default_device_profile(int numchannels)
{



}


/* This is a special case of the functions gsicc_set_gray_profile
    gsicc_set_rgb_profile and gsicc_set_cmyk_profile. In
    this function the number of channels is specified and the 
    appropriate profile contained in Resources/Color/InputProfiles is used */

void 
gsicc_load_default_input_profile(int numchannels)
{


}


int
gsicc_get_color_info(gsicc_colorspace_t *colorspace,unsigned char *color, int *size_color){

    switch(colorspace->ColorType){

	case DEVICETYPE:

            color = (unsigned char*) &(colorspace->DeviceType);
            *size_color = sizeof(gs_icc_devicecolor_t);

            break;
	case ICCTYPE:				

            color = (unsigned char*) &(colorspace->ProfileData);
            *size_color = colorspace->ProfileData->iccheader.size;

            break;
	case CRDTYPE:				

            color = (unsigned char*) (colorspace->pcrd);
            *size_color = sizeof(gs_cie_render);

            break;

        case CIEATYPE:				

            color = (unsigned char*) (colorspace->pcie_a);
            *size_color = sizeof(gs_cie_a);

            break;


        case CIEABCTYPE:				

            color = (unsigned char*) (colorspace->pcie_abc);
            *size_color = sizeof(gs_cie_abc);

            break;


        case CIEDEFTYPE:				

             color = (unsigned char*) (colorspace->pcie_def);
            *size_color = sizeof(gs_cie_def);

            break;


        case CIEDEFGTYPE:				

             color = (unsigned char*) (colorspace->pcie_defg);
            *size_color = sizeof(gs_cie_defg);

            break;

	default:			/* Should never occur */
	    return_error(gs_error_rangecheck);
	    break;

    }

    return(0);


}


/* Compute a hash code for the current transformation case.
    This just computes a 64bit xor of upper and lower portions of
    md5 for the input, output
    and rendering params structure.  We may change this later */

static int64_t
gsicc_compute_hash(gsicc_colorspace_t *input_colorspace, 
                   gsicc_colorspace_t *output_colorspace, 
                   gsicc_rendering_param_t *rendering_params)

{
   
    unsigned char *color1=NULL,*color2=NULL;
    int size_color1,size_color2;
    int ok;
    gs_md5_state_t md5;
    byte digest[16];
    int k,shift;
    int64_t word1,word2,result;

   /* first get the appropriate item from the structures */
   
    ok = gsicc_get_color_info(input_colorspace,color1,&size_color1);
    ok = gsicc_get_color_info(output_colorspace,color2,&size_color2);

    /* We could probably do something faster than this. But use this for now. */
    gs_md5_init(&md5);
    gs_md5_append(&md5, color1, size_color1);
    gs_md5_append(&md5, color2, size_color2);
    gs_md5_append(&md5, (gs_md5_byte_t*) rendering_params, sizeof(gsicc_rendering_param_t));
    gs_md5_finish(&md5, digest);

    /* xor this into 64 bit hash */

    word1 = 0;
    word2 = 0;
    shift = 0;

    for( k = 0; k<8; k++){
    
       word1 += digest[k] << shift;
       word2 += digest[k+8] << shift;
       shift += 8;

    }

    result = word1 ^ word2; 
    return(result);

}

static gsicc_link_t*
FindCacheLink(int64_t hashcode,gsicc_link_cache_t *icc_cache)
{

    gsicc_link_t *curr_pos1,*curr_pos2;
    bool foundit = 0;

    /* Look through the cache for the hashcode */

    curr_pos1 = icc_cache->ICCLink;
    curr_pos2 = curr_pos1;

    while (curr_pos1 != NULL ){

        if (curr_pos1->LinkHashCode == hashcode){

            return(curr_pos1);

        }

        curr_pos1 = curr_pos1->PrevLink;

    }

    while (curr_pos2 != NULL ){

        if (curr_pos2->LinkHashCode == hashcode){

            return(curr_pos2);

        }

        curr_pos2 = curr_pos2->NextLink;

    }

    return(NULL);


}


/* Find entry with zero ref count and remove it */
/* may need to lock cache during this time to avoid */
/* issue in multi-threaded case */

static gsicc_link_t*
gsicc_find_zeroref_cache(gsicc_link_cache_t *icc_cache){

    gsicc_link_t *curr_pos1,*curr_pos2;
    bool foundit = 0;

    /* Look through the cache for zero ref count */

    curr_pos1 = icc_cache->ICCLink;
    curr_pos2 = curr_pos1;

    while (curr_pos1 != NULL ){

        if (curr_pos1->ref_count == 0){

            return(curr_pos1);

        }

        curr_pos1 = curr_pos1->PrevLink;

    }

    while (curr_pos2 != NULL ){

        if (curr_pos2->ref_count == 0){

            return(curr_pos2);

        }

        curr_pos2 = curr_pos2->NextLink;

    }

    return(NULL);

}

/* Remove link from cache.  Notify CMS and free */

static void
gsicc_remove_link(gsicc_link_t *link,gsicc_link_cache_t *icc_cache, gs_memory_t *memory){


    gsicc_link_t *prevlink,*nextlink;

    prevlink = link->PrevLink;
    nextlink = link->NextLink;

    if (prevlink != NULL){
        prevlink->NextLink = nextlink;
    }

    if (nextlink != NULL){
        nextlink->PrevLink = prevlink;
    }

    gsicc_link_free(link, memory);

}


/* This is the main function called to obtain a linked transform from the ICC manager
   If the manager has the link ready, it will return it.  If not, it will request 
   one from the CMS and then return it.  We may need to do some cache locking during
   this process to avoid multi-threaded issues (e.g. someone deleting while someone
   is updating a reference count) */

gsicc_link_t* 
gsicc_get_link(gs_imager_state * pis, gsicc_colorspace_t  *input_colorspace, 
                    gsicc_colorspace_t *output_colorspace, 
                    gsicc_rendering_param_t *rendering_params, gs_memory_t *memory)
{

    int64_t hashcode;
    gsicc_link_t *link;
    void **contextptr = NULL;
    int ok;
    gsicc_manager_t *icc_manager = pis->icc_manager;
    gsicc_link_cache_t *icc_cache = pis->icc_cache;

    /* First compute the hash code for the incoming case */

    hashcode = gsicc_compute_hash(input_colorspace, output_colorspace, rendering_params);

    /* Check the cache for a hit */

    link = FindCacheLink(hashcode,icc_cache);
    
    /* Got a hit, update the reference count, return link */
    
    if (link != NULL) {

        link->ref_count++;
        return(link);  /* TO FIX: We are really not going to want to have the members
                          of this object visible outside gsiccmange */
    }

    /* If not, then lets create a new one if there is room or return NULL */
    /* Caller will need to try later */

    /* First see if we have space */

    if (icc_cache->num_links > ICC_CACHE_MAXLINKS){

        /* If not, see if there is anything we can remove from cache */

        link = gsicc_find_zeroref_cache(icc_cache);
        if ( link == NULL){

            return(NULL);

        } else {

            gsicc_remove_link(link,icc_cache, memory);

        }
      
    } 

    /* ToDo: If we are OK, then get the link.  But first convert all to ICC type */

    


    /* Now get the link */

    ok = gscms_get_link(link,input_colorspace,output_colorspace,rendering_params);
    
    if (ok){

       gsicc_add_link(icc_cache, link,contextptr, hashcode, memory);
       return(link);

    } else {

        return(NULL);

    }
        

}


/* Used by gs to notify the ICC manager that we are done with this link for now */

void 
gsicc_release_link(gsicc_link_t *icclink)
{
    /* Find link in cache */
    /* Decrement the reference count */

    icclink->ref_count--;

}


void 
setbuffer_desc(gsicc_bufferdesc_t *buffer_desc,unsigned char num_chan,
    unsigned char bytes_per_chan,
    bool has_alpha,
    bool alpha_first,
    bool little_endian,
    bool is_planar,
    gs_icc_colorbuffer_t buffercolor)
{
    buffer_desc->num_chan = num_chan;
    buffer_desc->bytes_per_chan = bytes_per_chan;
    buffer_desc->has_alpha = has_alpha;
    buffer_desc->alpha_first = alpha_first;
    buffer_desc->little_endian = little_endian;
    buffer_desc->is_planar = is_planar;
    buffer_desc->buffercolor = buffercolor;

}







