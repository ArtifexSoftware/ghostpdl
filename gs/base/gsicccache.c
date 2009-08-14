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
/*  GS ICC link cache.  Initial stubbing of functions.  */

#include "std.h"
#include "stdpre.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"  
#include "scommon.h"
#include "gx.h"
#include "gxistate.h"
#include "smd5.h" 
#include "gscms.h"
#include "gsicc_littlecms.h" 
#include "gsiccmanage.h"
#include "gsicccache.h"
#include "gserrors.h"

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



gs_private_st_ptrs3(st_icc_link, gsicc_link_t, "gsiccmanage_link",
		    icc_link_enum_ptrs, icc_link_reloc_ptrs,
		    contextptr, nextlink, prevlink);


gs_private_st_ptrs1(st_icc_linkcache, gsicc_link_cache_t, "gsiccmanage_linkcache",
		    icc_linkcache_enum_ptrs, icc_linkcache_reloc_ptrs,
		    icc_link);


/**
 * gsicc_cache_new: Allocate a new ICC cache manager
 * Return value: Pointer to allocated manager, or NULL on failure.
 **/

gsicc_link_cache_t *
gsicc_cache_new(gs_memory_t *memory)
{

    gsicc_link_cache_t *result;

    /* We want this to be maintained in stable_memory.  It should be be effected by the 
       save and restores */

    result = gs_alloc_struct(memory->stable_memory, gsicc_link_cache_t, &st_icc_linkcache,
			     "gsicc_cache_new");

    if ( result == NULL )
        return(NULL);

    rc_init_free(result, memory->stable_memory, 1, rc_gsicc_cache_free);

    result->icc_link = NULL;
    result->num_links = 0;
    result->memory = memory;

    return(result);

}

static void
rc_gsicc_cache_free(gs_memory_t * mem, void *ptr_in, client_name_t cname)
{
    /* Ending the entire cache.  The ref counts on all the links should be 0 */

    gsicc_link_cache_t *link_cache = (gsicc_link_cache_t * ) ptr_in;
    int k;
    gsicc_link_t *link;

    link = gsicc_find_zeroref_cache(link_cache);

    for( k = 0; k < link_cache->num_links; k++){

        if ( link_cache->icc_link != NULL){

            gsicc_remove_link(link_cache->icc_link,link_cache, mem);

        }

    }

    gs_free_object(mem->stable_memory, link_cache, "rc_gsicc_cache_free");

}

gsicc_link_t *
gsicc_add_link(gsicc_link_cache_t *link_cache, void *link_handle, void *contextptr, 
               gsicc_hashlink_t hashcode, gs_memory_t *memory)
{

    gsicc_link_t *result, *nextlink;

    /* The link has to be added in stable memory. We want them
       to be maintained across the gsave and grestore process */

    result = gs_alloc_struct(memory->stable_memory, gsicc_link_t, &st_icc_link,
			     "gsiccmanage_link_new");

    result->contextptr = contextptr;
    result->link_handle = link_handle;
    result->hashcode.link_hashcode = hashcode.link_hashcode;
    result->hashcode.des_hash = hashcode.des_hash;
    result->hashcode.src_hash = hashcode.src_hash;
    result->hashcode.rend_hash = hashcode.rend_hash;
    result->ref_count = 1;
    result->includes_softproof = 0;  /* Need to enable this at some point */

    if ( hashcode.src_hash == hashcode.des_hash ) {

        result->is_identity = true;

    } else {

        result->is_identity = false;

    }

    
    if (link_cache->icc_link != NULL){

        /* Add where ever we are right
           now.  Later we may want to
           do this differently.  */
        
        nextlink = link_cache->icc_link->nextlink;
        link_cache->icc_link->nextlink = result;
        result->prevlink = link_cache->icc_link;
        result->nextlink = nextlink;
        
        if (nextlink != NULL){

            nextlink->prevlink = result;

        }

    } else {

        result->nextlink = NULL;
        result->prevlink = NULL;
        link_cache->icc_link = result;
    
    }

    link_cache->num_links++;

    return(result);

}


void
gsicc_link_free(gsicc_link_t *icc_link, gs_memory_t *memory)
{

    gscms_release_link(icc_link);
    gs_free_object(memory->stable_memory, icc_link, "gsiccmanage_link_free");

}



static void 
gsicc_get_gscs_hash(gsicc_manager_t *icc_manager, gs_color_space *colorspace, int64_t *hash){

    /* There may be some work to do here with respect to pattern and indexed spaces */

    const gs_color_space_type *pcst = colorspace->type;

      switch(pcst->index){

	case gs_color_space_index_DeviceGray:

            *hash =  icc_manager->default_gray->hashcode;

            break;

	case gs_color_space_index_DeviceRGB:				

            *hash =  icc_manager->default_rgb->hashcode;

            break;

        case gs_color_space_index_DeviceCMYK:				

            *hash =  icc_manager->default_cmyk->hashcode;

            break;

	default:			
	    
	    break;

    }  



}

static void
gsicc_mash_hash(gsicc_hashlink_t *hash){

    hash->link_hashcode = (hash->des_hash) ^ (hash->rend_hash) ^ (hash->src_hash);

}

void 
gsicc_get_icc_buff_hash(unsigned char *buffer, int64_t *hash)
{

    int profile_size = gsicc_getprofilesize(buffer);
    gsicc_get_buff_hash(buffer,profile_size,hash);

}

static void 
gsicc_get_buff_hash(unsigned char *data,unsigned int num_bytes,int64_t *hash)
{

    gs_md5_state_t md5;
    byte digest[16];
    int k;
    int64_t word1,word2,shift;

   /* We could probably do something faster than this. But use this for now. */
    gs_md5_init(&md5);
    gs_md5_append(&md5, data, num_bytes);
    gs_md5_finish(&md5, digest);

    /* For now, xor this into 64 bit word */

    word1 = 0;
    word2 = 0;
    shift = 0;

    /* need to do it this way because of 
       potential word boundary issues */

    for( k = 0; k<8; k++){
    
       word1 += ((int64_t) digest[k]) << shift;
       word2 += ((int64_t) digest[k+8]) << shift;
       shift += 8;

    }

    *hash = word1 ^ word2; 

}


/* Compute a hash code for the current transformation case.
    This just computes a 64bit xor of upper and lower portions of
    md5 for the input, output
    and rendering params structure.  We may change this later */

static void
gsicc_compute_linkhash(gsicc_manager_t *icc_manager, cmm_profile_t *input_profile, 
                   cmm_profile_t *output_profile, 
                   gsicc_rendering_param_t *rendering_params, gsicc_hashlink_t *hash)
{
   
   /* first get the hash codes for the color spaces */

    gsicc_get_cspace_hash(icc_manager, input_profile, &(hash->src_hash));
    gsicc_get_cspace_hash(icc_manager, output_profile, &(hash->des_hash));

    /* now for the rendering paramaters */

    gsicc_get_buff_hash((unsigned char *) rendering_params,
        sizeof(gsicc_rendering_param_t),&(hash->rend_hash));

   /* for now, mash all of these into a link hash */

   gsicc_mash_hash(hash);

}



static void
gsicc_get_cspace_hash(gsicc_manager_t *icc_manager, cmm_profile_t *cmm_icc_profile_data, int64_t *hash){


    if (cmm_icc_profile_data == NULL ){
        *hash = icc_manager->device_profile->hashcode;
        return;
    }

    if (cmm_icc_profile_data->hash_is_valid ){

        *hash = cmm_icc_profile_data->hashcode;
        return;

    } else {

        /* We need to compute for this color space */

        gsicc_get_icc_buff_hash(cmm_icc_profile_data->buffer, hash);
        cmm_icc_profile_data->hashcode = *hash;
        cmm_icc_profile_data->hash_is_valid = true;
        return;

    }


}


static gsicc_link_t*
gsicc_findcachelink(gsicc_hashlink_t hash,gsicc_link_cache_t *icc_cache, bool includes_proof)
{

    gsicc_link_t *curr_pos1,*curr_pos2;
    bool foundit = 0;
    int64_t hashcode = hash.link_hashcode;

    /* Look through the cache for the hashcode */

    curr_pos1 = icc_cache->icc_link;
    curr_pos2 = curr_pos1;

    while (curr_pos1 != NULL ){

        if (curr_pos1->hashcode.link_hashcode == hashcode && includes_proof == curr_pos1->includes_softproof){

            return(curr_pos1);

        }

        curr_pos1 = curr_pos1->prevlink;

    }

    while (curr_pos2 != NULL ){

        if (curr_pos2->hashcode.link_hashcode == hashcode && includes_proof == curr_pos2->includes_softproof){

            return(curr_pos2);

        }

        curr_pos2 = curr_pos2->nextlink;

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

    curr_pos1 = icc_cache->icc_link;
    curr_pos2 = curr_pos1;

    while (curr_pos1 != NULL ){

        if (curr_pos1->ref_count == 0){

            return(curr_pos1);

        }

        curr_pos1 = curr_pos1->prevlink;

    }

    while (curr_pos2 != NULL ){

        if (curr_pos2->ref_count == 0){

            return(curr_pos2);

        }

        curr_pos2 = curr_pos2->nextlink;

    }

    return(NULL);

}

/* Remove link from cache.  Notify CMS and free */

static void
gsicc_remove_link(gsicc_link_t *link, gsicc_link_cache_t *icc_cache, gs_memory_t *memory){


    gsicc_link_t *prevlink,*nextlink;

    prevlink = link->prevlink;
    nextlink = link->nextlink;

    if (prevlink != NULL){
        prevlink->nextlink = nextlink;
    }

    if (nextlink != NULL){
        nextlink->prevlink = prevlink;
    }

    gsicc_link_free(link, memory);

}


/* This is the main function called to obtain a linked transform from the ICC cache
   If the cache has the link ready, it will return it.  If not, it will request 
   one from the CMS and then return it.  We may need to do some cache locking during
   this process to avoid multi-threaded issues (e.g. someone deleting while someone
   is updating a reference count) */

gsicc_link_t* 
gsicc_get_link(gs_imager_state *pis, gs_color_space  *input_colorspace, 
                    gs_color_space *output_colorspace, 
                    gsicc_rendering_param_t *rendering_params, gs_memory_t *memory, bool include_softproof)
{

    gsicc_hashlink_t hash;
    gsicc_link_t *link;
    gcmmhprofile_t link_handle = NULL;
    void **contextptr = NULL;
    gsicc_manager_t *icc_manager = pis->icc_manager; 
    gsicc_link_cache_t *icc_cache = pis->icc_cache;
    gcmmhprofile_t *cms_input_profile;
    gcmmhprofile_t *cms_output_profile;
    cmm_profile_t *gs_input_profile;
    cmm_profile_t *gs_output_profile;

    if ( input_colorspace->cmm_icc_profile_data == NULL ) {

        /* Use default type */

        gs_input_profile = gsicc_get_cs_profile(input_colorspace, pis->icc_manager); 

    } else {
        
        gs_input_profile = input_colorspace->cmm_icc_profile_data;

    }


    if ( output_colorspace != NULL ) {

        gs_output_profile = output_colorspace->cmm_icc_profile_data;

    } else {

        /* Use the device profile */
        gs_output_profile = pis->icc_manager->device_profile;

    }


    /* First compute the hash code for the incoming case */
    /* If the output color space is NULL we will use the device profile for the output color space */

    gsicc_compute_linkhash(icc_manager, gs_input_profile, gs_output_profile, rendering_params, &hash);

    /* Check the cache for a hit.  Need to check if softproofing was used */

    link = gsicc_findcachelink(hash,icc_cache,include_softproof);
    
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

    /* Now get the link */

    cms_input_profile = gs_input_profile->profile_handle;

    if (cms_input_profile == NULL){

        if (gs_input_profile->buffer != NULL){

            cms_input_profile = 
                gsicc_get_profile_handle_buffer(gs_input_profile->buffer);
            gs_input_profile->profile_handle = cms_input_profile;

        } else {

            /* Cant create the link.  No profile present, 
               nor any defaults to use for this */

            return(NULL);
        }

    }
  
    cms_output_profile = gs_output_profile->profile_handle;

    if (cms_output_profile == NULL){

        if (gs_output_profile->buffer != NULL){

            cms_output_profile = 
                gsicc_get_profile_handle_buffer(gs_output_profile->buffer);
            gs_output_profile->profile_handle = cms_output_profile;

        } else {

            /* Cant create the link.  No profile present.  Should not
               occur with defaults defined and device profile defined */

            return(NULL);
        }

    }

    link_handle = gscms_get_link(cms_input_profile, cms_output_profile, 
                                    rendering_params, pis->icc_manager);
    
    if (link_handle != NULL){

       link = gsicc_add_link(icc_cache, link_handle,contextptr, hash, memory);
       
    } else {
        
        return(NULL);

    }

    return(link);
        

}


/* Used by gs to notify the ICC manager that we are done with this link for now */

void 
gsicc_release_link(gsicc_link_t *icclink)
{
    /* Find link in cache */
    /* Decrement the reference count */

    icclink->ref_count--;

}

/* Used to initialize the buffer description prior to color conversion */

void
gsicc_init_buffer(gsicc_bufferdesc_t *buffer_desc, unsigned char num_chan, unsigned char bytes_per_chan,
                  bool has_alpha, bool alpha_first, bool is_planar, int plane_stride, int row_stride,
                  int num_rows, int pixels_per_row)
{


    buffer_desc->num_chan = num_chan;
    buffer_desc->bytes_per_chan = bytes_per_chan;
    buffer_desc->has_alpha = has_alpha;
    buffer_desc->alpha_first = alpha_first;
    buffer_desc->is_planar = is_planar;
    buffer_desc->plane_stride = plane_stride;
    buffer_desc->row_stride = row_stride;
    buffer_desc->num_rows = num_rows;
    buffer_desc->pixels_per_row = pixels_per_row; 

#if arch_is_big_endian
    buffer_desc->little_endian = false;
#else
    buffer_desc->little_endian = true;
#endif


}

