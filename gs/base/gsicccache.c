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
#include "gsicccache.h"

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



gs_private_st_ptrs4(st_icc_link, gsicc_link_t, "gsiccmanage_link",
		    icc_link_enum_ptrs, icc_link_reloc_ptrs,
		    link_handle, contextptr, nextlink, prevlink);


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

    result = gs_alloc_struct(memory, gsicc_link_cache_t, &st_icc_linkcache,
			     "gsiccmanage_linkcache_new");
    result->icc_link = NULL;
    result->num_links = 0;

    return(result);

}


static void
gsicc_add_link(gsicc_link_cache_t *link_cache, void *link_handle,void *contextptr, int64_t link_hashcode, gs_memory_t *memory)
{

    gsicc_link_t *result, *nextlink;

    result = gs_alloc_struct(memory, gsicc_link_t, &st_icc_link,
			     "gsiccmanage_link_new");

    result->contextptr = contextptr;
    result->link_handle = link_handle;
    result->link_hashcode = link_hashcode;
    result->ref_count = 1;
    
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



int
gsicc_get_color_info(gs_color_space *colorspace,unsigned char *color, int *size_color){

  /*  switch(colorspace->ColorType){

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

	default:			
	    return_error(gs_error_rangecheck);
	    break;

    }  */

    return(0);


}


/* Compute a hash code for the current transformation case.
    This just computes a 64bit xor of upper and lower portions of
    md5 for the input, output
    and rendering params structure.  We may change this later */

static int64_t
gsicc_compute_hash(gs_color_space *input_colorspace, 
                   gs_color_space *output_colorspace, 
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
FindCacheLink(int64_t hashcode,gsicc_link_cache_t *icc_cache, bool includes_proof)
{

    gsicc_link_t *curr_pos1,*curr_pos2;
    bool foundit = 0;

    /* Look through the cache for the hashcode */

    curr_pos1 = icc_cache->icc_link;
    curr_pos2 = curr_pos1;

    while (curr_pos1 != NULL ){

        if (curr_pos1->link_hashcode == hashcode && includes_proof == curr_pos1->includes_softproof){

            return(curr_pos1);

        }

        curr_pos1 = curr_pos1->prevlink;

    }

    while (curr_pos2 != NULL ){

        if (curr_pos2->link_hashcode == hashcode && includes_proof == curr_pos2->includes_softproof){

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
gsicc_remove_link(gsicc_link_t *link,gsicc_link_cache_t *icc_cache, gs_memory_t *memory){


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


/* This is the main function called to obtain a linked transform from the ICC manager
   If the manager has the link ready, it will return it.  If not, it will request 
   one from the CMS and then return it.  We may need to do some cache locking during
   this process to avoid multi-threaded issues (e.g. someone deleting while someone
   is updating a reference count) */

gsicc_link_t* 
gsicc_get_link(gs_imager_state *pis, gs_color_space  *input_colorspace, 
                    gs_color_space *output_colorspace, 
                    gsicc_rendering_param_t *rendering_params, gs_memory_t *memory, bool include_softproof)
{

    int64_t hashcode;
    gsicc_link_t *link;
    void **contextptr = NULL;
    int ok;
    gsicc_manager_t *icc_manager = pis->icc_manager; 
    gsicc_link_cache_t *icc_cache = pis->icc_cache;

    /* First compute the hash code for the incoming case */

    hashcode = gsicc_compute_hash(input_colorspace, output_colorspace, rendering_params);

    /* Check the cache for a hit.  Need to check if softproofing was used */

    link = FindCacheLink(hashcode,icc_cache,include_softproof);
    
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
    
    ok = gscms_get_link(link,input_colorspace,output_colorspace,rendering_params, pis->icc_manager);
    
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







