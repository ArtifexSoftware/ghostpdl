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
/*  GS ICC Manager.  Initial stubbing of functions.  */

#include "std.h"
#include "stdpre.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"  
#include "scommon.h"
#include "strmio.h"
#include "gx.h"
#include "gxistate.h"
#include "gxcspace.h"
#include "gscms.h"
#include "gsicc_manage.h"
#include "gsicc_cache.h"
#include "gsicc_profilecache.h"
#include "gserrors.h"
#include "string_.h"
#include "gxclist.h"
#include "gxcldev.h"
#include "gzstate.h"
#include "gsicc_create.h"
#include "gpmisc.h"

#define ICC_HEADER_SIZE 128

#if ICC_DUMP
unsigned int global_icc_index = 0;
#endif

/* Static prototypes */

static void gsicc_set_default_cs_value(cmm_profile_t *picc_profile, 
                                       gs_imager_state *pis);
static gsicc_namelist_t* gsicc_new_namelist(gs_memory_t *memory);
static gsicc_colorname_t* gsicc_new_colorname(gs_memory_t *memory);
static void gsicc_copy_colorname( const char *cmm_name, 
                                 gsicc_colorname_t *colorname, 
                                 gs_memory_t *memory );
static gsicc_namelist_t* gsicc_get_spotnames(gcmmhprofile_t profile, 
                                             gs_memory_t *memory);
static void rc_gsicc_manager_free(gs_memory_t * mem, void *ptr_in, 
                                  client_name_t cname);
static void rc_free_icc_profile(gs_memory_t * mem, void *ptr_in, 
                                client_name_t cname);
static int gsicc_load_profile_buffer(cmm_profile_t *profile, stream *s, 
                                     gs_memory_t *memory);
static stream* gsicc_open_search(const char* pname, int namelen, 
                                 gs_memory_t *mem_gc, 
                                 gsicc_manager_t *icc_manager);
static int gsicc_set_device_profile(gsicc_manager_t *icc_manager, 
                                    gx_device * pdev, gs_memory_t * mem);
static cmm_profile_t* gsicc_get_profile( gsicc_profile_t profile_type, 
                                        gsicc_manager_t *icc_manager );
static int64_t gsicc_search_icc_table(clist_icctable_t *icc_table, 
                                      int64_t icc_hashcode, int *size);
static int gsicc_load_namedcolor_buffer(cmm_profile_t *profile, stream *s, 
                          gs_memory_t *memory);


/* profile data structure */
/* profile_handle should NOT be garbage collected since it is allocated by the external CMS */
gs_private_st_ptrs2(st_gsicc_colorname, gsicc_colorname_t, "gsicc_colorname",
		    gsicc_colorname_enum_ptrs, gsicc_colorname_reloc_ptrs, name, next);

gs_private_st_ptrs1(st_gsicc_manager, gsicc_manager_t, "gsicc_manager",
		    gsicc_manager_enum_ptrs, gsicc_manager_profile_reloc_ptrs,
		    smask_profiles); 

gs_private_st_ptrs2(st_gsicc_devicen, gsicc_devicen_t, "gsicc_devicen",
		gsicc_devicen_enum_ptrs, gsicc_devicen_reloc_ptrs, head, final);

gs_private_st_ptrs2(st_gsicc_devicen_entry, gsicc_devicen_entry_t, 
                    "gsicc_devicen_entry", gsicc_devicen_entry_enum_ptrs, 
                    gsicc_devicen_entry_reloc_ptrs, iccprofile, next);

static const gs_color_space_type gs_color_space_type_icc = {
    gs_color_space_index_ICC,       /* index */
    true,                           /* can_be_base_space */
    true,                           /* can_be_alt_space */
    NULL                            /* This is going to be outside the norm. struct union*/
};

typedef struct default_profile_def_s {
    char *path;
    gsicc_profile_t default_type;
} default_profile_def_t;

static const default_profile_def_t default_profile_params[] =
{
    {DEFAULT_GRAY_ICC, DEFAULT_GRAY},
    {DEFAULT_RGB_ICC, DEFAULT_RGB},
    {DEFAULT_CMYK_ICC, DEFAULT_CMYK},
    {LAB_ICC, LAB_TYPE}
};

/* Get the size of the ICC profile that is in the buffer */
unsigned int gsicc_getprofilesize(unsigned char *buffer)
{
    return ( (buffer[0] << 24) + (buffer[1] << 16) +
             (buffer[2] << 8)  +  buffer[3] );
}

/*  This sets the directory to prepend to the ICC profile names specified for
    defaultgray, defaultrgb, defaultcmyk, proofing, linking, named color and device */
void
gsicc_set_icc_directory(const gs_imager_state *pis, const char* pname, 
                        int namelen)
{
    gsicc_manager_t *icc_manager = pis->icc_manager;
    char *result;
    gs_memory_t *mem_gc = pis->memory; 

    /* User param string.  Must allocate in non-gc memory */
    result = (char*) gs_alloc_bytes(mem_gc->non_gc_memory, namelen,
		   		     "gsicc_set_icc_directory");
    if (result != NULL) {
        strcpy(result, pname);
        icc_manager->profiledir = result;
        icc_manager->namelen = namelen;
    }
}

static void gscms_set_icc_range(cmm_profile_t **icc_profile)
{
    int num_comp = (*icc_profile)->num_comps;
    int k;

    for ( k = 0; k < num_comp; k++) {
        (*icc_profile)->Range.ranges[k].rmin = 0.0;
        (*icc_profile)->Range.ranges[k].rmax = 1.0;
    }
}

static cmm_profile_t*
gsicc_set_iccsmaskprofile(const char *pname, 
                          int namelen, gsicc_manager_t *icc_manager, 
                          gs_memory_t *mem)
{
    stream *str;
    int code;
    cmm_profile_t *icc_profile;

    str = gsicc_open_search(pname, namelen, mem, icc_manager);
    if (str != NULL) {
        icc_profile = gsicc_profile_new(str, mem, pname, namelen);
        code = sfclose(str);
        /* Get the profile handle */
        icc_profile->profile_handle = 
            gsicc_get_profile_handle_buffer(icc_profile->buffer,
                                            icc_profile->buffer_size);
        /* Compute the hash code of the profile. Everything in the 
           ICC manager will have it's hash code precomputed */
        gsicc_get_icc_buff_hash(icc_profile->buffer, &(icc_profile->hashcode),
                                        icc_profile->buffer_size);
        icc_profile->hash_is_valid = true;
        icc_profile->num_comps = 
            gscms_get_input_channel_count(icc_profile->profile_handle);
        icc_profile->num_comps_out = 
            gscms_get_output_channel_count(icc_profile->profile_handle);
        icc_profile->data_cs = 
            gscms_get_profile_data_space(icc_profile->profile_handle);
        gscms_set_icc_range(&icc_profile);
        return(icc_profile);
    } else {
        return(NULL);
    }
}

gsicc_smask_t*
gsicc_new_iccsmask(gs_memory_t *memory)
{
    gsicc_smask_t *result;

    result = (gsicc_smask_t *) gs_alloc_bytes(memory, sizeof(gsicc_smask_t), "gsicc_new_iccsmask");
    if (result != NULL) {
        result->smask_gray = NULL;
        result->smask_rgb = NULL;
        result->smask_cmyk = NULL;
        result->memory = memory;
    }
    return(result);
}


/* Allocate a new structure to hold the profiles that contains the profiles
   used when we are in a softmask group */
int
gsicc_initialize_iccsmask(gsicc_manager_t *icc_manager)
{
    gs_memory_t *stable_mem = icc_manager->memory->stable_memory;

    /* Allocations need to be done in stable memory.  We want to maintain
       the smask_profiles object */
    icc_manager->smask_profiles = gsicc_new_iccsmask(stable_mem);
    if (icc_manager->smask_profiles == NULL)
        return gs_rethrow(-1, "insufficient memory to allocate smask profiles");   
    /* Load the gray, rgb, and cmyk profiles */
    if ((icc_manager->smask_profiles->smask_gray = 
        gsicc_set_iccsmaskprofile(SMASK_GRAY_ICC, strlen(SMASK_GRAY_ICC),
        icc_manager, stable_mem) ) == NULL) {
        return gs_rethrow(-1, "failed to load gray smask profile");  
    }
    if ((icc_manager->smask_profiles->smask_rgb = 
        gsicc_set_iccsmaskprofile(SMASK_RGB_ICC, strlen(SMASK_RGB_ICC),
        icc_manager, stable_mem)) == NULL) {
        return gs_rethrow(-1, "failed to load rgb smask profile");  
    }
    if ((icc_manager->smask_profiles->smask_cmyk = 
        gsicc_set_iccsmaskprofile(SMASK_CMYK_ICC, strlen(SMASK_CMYK_ICC),
        icc_manager, stable_mem)) == NULL) {
        return gs_rethrow(-1, "failed to load cmyk smask profile");  
    }
    /* Set these as "default" so that pdfwrite or other high level devices
       will know that these are manufactured profiles, and default spaces
       should be used */
    icc_manager->smask_profiles->smask_gray->default_match = DEFAULT_GRAY;
    icc_manager->smask_profiles->smask_rgb->default_match = DEFAULT_RGB;
    icc_manager->smask_profiles->smask_cmyk->default_match = DEFAULT_CMYK;
    return(0);
}

static int
gsicc_new_devicen(gsicc_manager_t *icc_manager)
{
/* Allocate a new deviceN ICC profile entry in the deviceN list */
    gsicc_devicen_entry_t *device_n_entry = 
        gs_alloc_struct(icc_manager->memory, gsicc_devicen_entry_t,
		&st_gsicc_devicen_entry, "gsicc_new_devicen");
    if (device_n_entry == NULL)
        return gs_rethrow(-1, "insufficient memory to allocate device n profile");
    device_n_entry->next = NULL;
    device_n_entry->iccprofile = NULL;
/* Check if we already have one in the manager */
    if ( icc_manager->device_n == NULL ) {
        /* First one.  Need to allocate the DeviceN main object */
        icc_manager->device_n = gs_alloc_struct(icc_manager->memory, 
            gsicc_devicen_t, &st_gsicc_devicen, "gsicc_new_devicen");

        if (icc_manager->device_n == NULL)
            return gs_rethrow(-1, "insufficient memory to allocate device n profile");

        icc_manager->device_n->head = device_n_entry;
        icc_manager->device_n->final = device_n_entry;
        icc_manager->device_n->count = 1;
        return(0);
    } else {        
        /* We have one or more in the list. */
        icc_manager->device_n->final->next = device_n_entry;
        icc_manager->device_n->final = device_n_entry;
        icc_manager->device_n->count++;
        return(0);
    }
}

cmm_profile_t*
gsicc_finddevicen(const gs_color_space *pcs, gsicc_manager_t *icc_manager)
{

    int k,j,i;
    gsicc_devicen_entry_t *curr_entry;
    int num_comps;
    const gs_separation_name *names = pcs->params.device_n.names;
    unsigned char *pname;
    unsigned int name_size;
    gsicc_devicen_t *devicen_profiles = icc_manager->device_n;
    gsicc_colorname_t *icc_spot_entry;
    int match_count = 0;
    bool permute_needed = false;

    num_comps = gs_color_space_num_components(pcs);

    /* Go through the list looking for a match */
    curr_entry = devicen_profiles->head;
    for ( k = 0; k < devicen_profiles->count; k++ ) {
        if (curr_entry->iccprofile->num_comps == num_comps ) {

            /* Now check the names.  The order is important
               since this is supposed to be the laydown order.
               If the order is off, the ICC profile will likely
               not be accurate.  The ICC profile drives the laydown
               order here.  A permutation vector is used to 
               reorganize the data prior to the transform application */
            for ( j = 0; j < num_comps; j++) {
	        /* Get the character string and length for the component name. */
	        pcs->params.device_n.get_colorname_string(icc_manager->memory, 
                                                    names[j], &pname, &name_size);

                /* Compare to the jth entry in the ICC profile */
                icc_spot_entry = curr_entry->iccprofile->spotnames->head;
                for ( i = 0; i < num_comps; i++) {
                    if( strncmp((const char *) pname, 
                        icc_spot_entry->name, name_size) == 0 ) {
                        /* Found a match */
                        match_count++;
                        curr_entry->iccprofile->devicen_permute[j] = i;
                        if ( j != i) {
                            /* Document ink order does not match ICC 
                               profile ink order */
                            permute_needed = true;
                        }
                        break;
                    } else
                        icc_spot_entry = icc_spot_entry->next;
                }
                if (match_count < j+1) 
                    return(NULL);
            }
            if ( match_count == num_comps) {
                /* We have a match.  Order of components does not match laydown
                   order specified by the ICC profile.  Set a flag.  This may
                   be an issue if we are using 2 DeviceN color spaces with the
                   same colorants but with different component orders.  The problem
                   comes about since we would be sharing the profile in the 
                   DeviceN entry of the icc manager. */
                curr_entry->iccprofile->devicen_permute_needed = permute_needed;
                return(curr_entry->iccprofile);
            }
            match_count = 0;
        }
    }
    return(NULL);
}

/* Populate the color names entries that should
   be contained in the DeviceN ICC profile */
static gsicc_namelist_t*
gsicc_get_spotnames(gcmmhprofile_t profile, gs_memory_t *memory)
{
    int k;
    gsicc_namelist_t *list;
    gsicc_colorname_t *name;
    gsicc_colorname_t **curr_entry;
    int num_colors;
    char *clr_name;

    num_colors = gscms_get_numberclrtnames(profile);
    if (num_colors == 0) 
        return(NULL);
    /* Allocate structure for managing this */
    list = gsicc_new_namelist(memory);
    if (list == NULL) 
        return(NULL);
    curr_entry = &(list->head);
    for (k = 0; k < num_colors; k++) {
       /* Allocate a new name object */
        name = gsicc_new_colorname(memory);
        *curr_entry = name;
        /* Get the name */
        clr_name = gscms_get_clrtname(profile, k);
        gsicc_copy_colorname(clr_name, *curr_entry, memory);
        curr_entry = &((*curr_entry)->next);
    }
    list->count = num_colors;
    return(list);
}

static void 
gsicc_get_devicen_names(cmm_profile_t *icc_profile, gs_memory_t *memory)
{
    /* The names are contained in the 
       named color tag.  We use the
       CMM to extract the data from the
       profile */
    if (icc_profile->profile_handle == NULL) {
        if (icc_profile->buffer != NULL) {
            icc_profile->profile_handle = 
                gsicc_get_profile_handle_buffer(icc_profile->buffer,
                                                icc_profile->buffer_size);
        } else
            return;
    }
    icc_profile->spotnames = 
        gsicc_get_spotnames(icc_profile->profile_handle, memory->non_gc_memory);
    return;
}

/* Allocate new spot name list object.  */
static gsicc_namelist_t* 
gsicc_new_namelist(gs_memory_t *memory)
{
    gsicc_namelist_t *result;

    result = (gsicc_namelist_t *) gs_alloc_bytes(memory->non_gc_memory, sizeof(gsicc_namelist_t),
                             "gsicc_new_namelist");
    result->count = 0;
    result->head = NULL;
    return(result);
}

/* Allocate new spot name.  */
static gsicc_colorname_t* 
gsicc_new_colorname(gs_memory_t *memory)
{
    gsicc_colorname_t *result;

    result = gs_alloc_struct(memory,gsicc_colorname_t,
		&st_gsicc_colorname, "gsicc_new_colorname");
    result->length = 0;
    result->name = NULL;
    result->next = NULL;
    return(result);
}

/* Copy the name from the CMM dependent stucture to ours */
void
gsicc_copy_colorname(const char *cmm_name, gsicc_colorname_t *colorname, 
                     gs_memory_t *memory )
{
    int length;

    length = strlen(cmm_name);
    colorname->name = (char*) gs_alloc_bytes(memory, length,
					"gsicc_copy_colorname");
    strcpy(colorname->name, cmm_name);
    colorname->length = length;
}

/* If the profile is one of the default types that were set in the iccmanager,
   then the index for that type is returned.  Otherwise the ICC index is returned.  
   This is currently used to keep us from writing out the default profiles for 
   high level devices, if desired. */
gs_color_space_index 
gsicc_get_default_type(cmm_profile_t *profile_data)
{
    switch ( profile_data->default_match ) {
        case DEFAULT_GRAY:
            return(gs_color_space_index_DeviceGray);
        case DEFAULT_RGB:
            return(gs_color_space_index_DeviceRGB);
        case DEFAULT_CMYK:
            return(gs_color_space_index_DeviceCMYK);
        case CIE_A:
            return(gs_color_space_index_CIEA);
        case CIE_ABC:
            return(gs_color_space_index_CIEABC);
        case CIE_DEF:
            return(gs_color_space_index_CIEDEF);
        case CIE_DEFG:
            return(gs_color_space_index_CIEDEFG);
        default:
            return(gs_color_space_index_ICC);
    }
}

/*  This computes the hash code for the ICC data and assigns the code and the 
    profile to the appropriate member variable in the ICC manager */
int 
gsicc_set_profile(gsicc_manager_t *icc_manager, const char* pname, int namelen, 
                  gsicc_profile_t defaulttype)
{

    cmm_profile_t *icc_profile;
    cmm_profile_t **manager_default_profile = NULL; /* quite compiler */
    stream *str;
    gs_memory_t *mem_gc = icc_manager->memory; 
    int code;
    int k;
    gsicc_colorbuffer_t default_space; /* Used to verify that we have the correct type */
        
    /* For now only let this be set once. We could have this changed dynamically
       in which case we need to do some deaallocations prior to replacing it */
    default_space = gsUNDEFINED;
    switch(defaulttype) {
        case DEFAULT_GRAY:
            manager_default_profile = &(icc_manager->default_gray);
            default_space = gsGRAY;
            break;
        case DEFAULT_RGB:
            manager_default_profile = &(icc_manager->default_rgb);
            default_space = gsRGB;
            break;
        case DEFAULT_CMYK:
             manager_default_profile = &(icc_manager->default_cmyk);
             default_space = gsCMYK;
             break;
        case PROOF_TYPE:
             manager_default_profile = &(icc_manager->proof_profile);
             break;
        case NAMED_TYPE:
             manager_default_profile = &(icc_manager->device_named);
             break;
        case LINKED_TYPE:
             manager_default_profile = &(icc_manager->output_link);
             break;
        case LAB_TYPE:
             manager_default_profile = &(icc_manager->lab_profile);
             break;
        case DEVICEN_TYPE:
            code = gsicc_new_devicen(icc_manager);
            if (code == 0) {
                manager_default_profile = 
                    &(icc_manager->device_n->final->iccprofile);
            } else {
                return(code);
            }
            break;
        case DEFAULT_NONE:
        default:
            return(0);
            break;
    }
    /* If it is not NULL then it has already been set. If it is different than 
       what we already have then we will want to free it.  Since other imager 
       states could have different default profiles, this is done via reference 
       counting.  If it is the same as what we already have then we DONT 
       increment, since that is done when the imager state is duplicated.  It 
       could be the same, due to a resetting of the user params. To avoid 
       recreating the profile data, we compare the string names. Also, we want 
       to avoid the default blowing away the -s settings for the gray, rgb and 
       cmyk profile following a vmreclaim so test if it is one of those default 
       types */
    if (defaulttype < DEVICEN_TYPE && (*manager_default_profile) != NULL) {
        return 0;
    }
    if ((*manager_default_profile) != NULL) {
        /* Check if this is what we already have */
        icc_profile = *manager_default_profile;
        if ( namelen == icc_profile->name_length )
            if( memcmp(pname, icc_profile->name, namelen) == 0) 
                return 0;
        rc_decrement(icc_profile,"gsicc_set_profile");
    }

    /* We need to do a special check for DeviceN since we have a linked list of 
       profiles and we can have multiple specifications */
    if ( defaulttype == DEVICEN_TYPE ) {
        gsicc_devicen_entry_t *current_entry = icc_manager->device_n->head;
        for ( k = 0; k < icc_manager->device_n->count; k++ ) {
            if ( current_entry->iccprofile != NULL ) {
                icc_profile = current_entry->iccprofile;
                if ( namelen == icc_profile->name_length )
                    if( memcmp(pname, icc_profile->name, namelen) == 0) 
                        return 0;
            }
            current_entry = current_entry->next;
        }
    }

    str = gsicc_open_search(pname, namelen, mem_gc, icc_manager);
    if (str != NULL) {
        icc_profile = gsicc_profile_new(str, mem_gc, pname, namelen);
        /* Add check so that we detect cases where we are loading a named
           color structure that is not a standard profile type */
        if (icc_profile == NULL && defaulttype == NAMED_TYPE) {
            /* Failed to load the named color profile.  Just load the file
               into the buffer as it is.  The profile_handle member
               variable can then be used to hold the named color 
               structure that is actually search. This is created later
               when needed. */
            icc_profile = gsicc_profile_new(NULL, mem_gc, NULL, 0);
            icc_profile->data_cs = gsNAMED;
            code = gsicc_load_namedcolor_buffer(icc_profile, str, mem_gc);
            if (code < 0) gs_rethrow1(-1, "problems with profile %s",pname);
            *manager_default_profile = icc_profile;
            return(0);  /* Done now, since this is not a standard ICC profile */
        } 
        code = sfclose(str);
        if (icc_profile == NULL) {
            return gs_rethrow1(-1, "problems with profile %s",pname);
        }
        *manager_default_profile = icc_profile;

        /* Get the profile handle */
        icc_profile->profile_handle = 
            gsicc_get_profile_handle_buffer(icc_profile->buffer,
                                            icc_profile->buffer_size);
        if (icc_profile->profile_handle == NULL) {
            return gs_rethrow1(-1, "allocation of profile %s handle failed", pname);
        }
        /* Compute the hash code of the profile. Everything in the ICC manager will have 
           it's hash code precomputed */
        gsicc_get_icc_buff_hash(icc_profile->buffer, &(icc_profile->hashcode),
                                icc_profile->buffer_size);
        icc_profile->hash_is_valid = true;
        icc_profile->default_match = defaulttype;
        icc_profile->num_comps = 
            gscms_get_input_channel_count(icc_profile->profile_handle);
        icc_profile->num_comps_out = 
            gscms_get_output_channel_count(icc_profile->profile_handle);
        icc_profile->data_cs = 
            gscms_get_profile_data_space(icc_profile->profile_handle);

        /* Check that we have the proper color space for the ICC 
           profiles that can be externally set */
        if (default_space != gsUNDEFINED) {
            if (icc_profile->data_cs != default_space) {
                return gs_rethrow(-1, "A default profile has an incorrect color space");
            }
        }
        /* Initialize the range to default values */
        for ( k = 0; k < icc_profile->num_comps; k++) {
            icc_profile->Range.ranges[k].rmin = 0.0;
            icc_profile->Range.ranges[k].rmax = 1.0;
        }
        if (defaulttype == LAB_TYPE) 
            icc_profile->islab = true;
        if ( defaulttype == DEVICEN_TYPE ) {
            /* Lets get the name information out of the profile.
               The names are contained in the icSigNamedColor2Tag
               item.  The table is in the A2B0Tag item.
               The names are in the order such that the fastest
               index in the table is the first name */
            gsicc_get_devicen_names(icc_profile, icc_manager->memory);
        }
        return(0);
    }
    return(-1);
}

/* This is used to get the profile handle given a file name  */
cmm_profile_t*
gsicc_get_profile_handle_file(const char* pname, int namelen, gs_memory_t *mem)
{
    cmm_profile_t *result;
    stream* str;
    int code;

    /* First see if we can get the stream.  NOTE  icc directory not used! */
    str = gsicc_open_search(pname, namelen, mem, NULL);
    if (str != NULL) {
        result = gsicc_profile_new(str, mem, pname, namelen);
        code = sfclose(str);
        gsicc_init_profile_info(result);
        return(result);
    }
    return(NULL);
}

/* Given that we already have a profile in a buffer (e.g. generated from a PS object)
   this gets the handle and initializes the various member variables that we need */
void
gsicc_init_profile_info(cmm_profile_t *profile)
{
    int k;

    /* Get the profile handle */
    profile->profile_handle = 
        gsicc_get_profile_handle_buffer(profile->buffer,
                                        profile->buffer_size);

    /* Compute the hash code of the profile. */
    gsicc_get_icc_buff_hash(profile->buffer, &(profile->hashcode),
                            profile->buffer_size);
    profile->hash_is_valid = true;
    profile->default_match = DEFAULT_NONE;
    profile->num_comps = gscms_get_input_channel_count(profile->profile_handle);
    profile->num_comps_out = gscms_get_output_channel_count(profile->profile_handle);
    profile->data_cs = gscms_get_profile_data_space(profile->profile_handle);

    /* Initialize the range to default values */
    for ( k = 0; k < profile->num_comps; k++) {
        profile->Range.ranges[k].rmin = 0.0;
        profile->Range.ranges[k].rmax = 1.0;
    }
}

/* This is used to try to find the specified or default ICC profiles */
static stream* 
gsicc_open_search(const char* pname, int namelen, gs_memory_t *mem_gc, 
                  gsicc_manager_t *icc_manager)
{
    char *buffer;
    stream* str;

    /* Check if we need to prepend the file name  */
    if ( icc_manager != NULL) {
        if ( icc_manager->profiledir != NULL ) {            
            /* If this fails, we will still try the file by itself and with 
               %rom% since someone may have left a space some of the spaces 
               as our defaults, even if they defined the directory to use. 
               This will occur only after searching the defined directory.  
               A warning is noted.  */
            buffer = (char *) gs_alloc_bytes(mem_gc, namelen + icc_manager->namelen,
		   		         "gsicc_open_search");
            strcpy(buffer, icc_manager->profiledir);
            strcat(buffer, pname);
            str = sfopen(buffer, "rb", mem_gc);
            gs_free_object(mem_gc, buffer, "gsicc_open_search");
            if (str != NULL) return(str);
        } 
    }

    /* First just try it like it is */
    str = sfopen(pname, "rb", mem_gc);
    if (str != NULL) 
        return(str);

    /* If that fails, try %rom% */
    buffer = (char *) gs_alloc_bytes(mem_gc, 1 + namelen + 
                        strlen("%rom%iccprofiles/"),"gsicc_open_search");
    strcpy(buffer, "%rom%iccprofiles/");
    strcat(buffer, pname);
    str = sfopen(buffer, "rb", mem_gc);
    gs_free_object(mem_gc, buffer, "gsicc_open_search");
    if (str == NULL) {
        gs_warn1("Could not find %s ",pname);
    }
    return(str);
}

/* This sets the device profile. If the device does not have a defined 
   profile, then a default one is selected. */
int
gsicc_init_device_profile(const gs_state *pgs, gx_device * dev)
{
    int code;
    char *pname;

    /* See if the device has a profile */
    if (dev->color_info.icc_profile[0] == '\0') {
        /* Grab a default one and prepend the icc directory to 
           it so that we have a proper path during c-list reading
           if build was without compile inits.   */
        if (pgs->icc_manager->profiledir != NULL) {
            strcpy(dev->color_info.icc_profile, pgs->icc_manager->profiledir);
        }
        switch(dev->color_info.num_components) {
            case 1:
                strcat(dev->color_info.icc_profile, DEFAULT_GRAY_ICC);
                break;
            case 3:
                strcat(dev->color_info.icc_profile, DEFAULT_RGB_ICC);
                break;
            case 4:
                strcat(dev->color_info.icc_profile, DEFAULT_CMYK_ICC);
                break;
            default:
                strcat(dev->color_info.icc_profile, DEFAULT_CMYK_ICC);
                break;
        }
    } 
    /* Set the manager */
    if (dev->device_icc_profile == NULL) {
        /* No color model for the device */
        code = gsicc_set_device_profile(pgs->icc_manager, dev, pgs->memory);
        return(code);
    } else {
        /* It is possible that the profile name has not yet been prepended
           by the ICC directory path when we specify a directory and an output
           ICC profile.  This (the full path name) is needed later for c-list 
           processing.  Check and make sure that this is the case.  If not, then
           prepend it now if there is room. */
        if (!gp_file_name_is_absolute(dev->color_info.icc_profile,
            strlen(dev->color_info.icc_profile))) { 
            if (pgs->icc_manager->profiledir != NULL) {
                if (strncmp(pgs->icc_manager->profiledir,dev->color_info.icc_profile,
                    strlen(pgs->icc_manager->profiledir)) != 0) {
                    if ((strlen(pgs->icc_manager->profiledir) + 
                        strlen(dev->color_info.icc_profile)) < gp_file_name_sizeof) {
                        pname = (char *)gs_alloc_bytes(pgs->memory, 
                                                    strlen(dev->color_info.icc_profile)+1,
   		                                "gsicc_init_device_profile");
                        strcpy(pname,dev->color_info.icc_profile);
                        strcpy(dev->color_info.icc_profile,pgs->icc_manager->profiledir);
                        strcat(dev->color_info.icc_profile,pname);
                        gs_free_object(pgs->memory, pname, "gsicc_init_device_profile");
                    }
                }
            }
        }
        /* Check for cases where the color model has changed */
        if (dev->device_icc_profile->num_comps != 
            dev->color_info.num_components || 
            strncmp(dev->device_icc_profile->name,dev->color_info.icc_profile,
            dev->device_icc_profile->name_length) != 0 ) {
            /* First go ahead and try to use the profile path that is already 
               given in the dev */
            code = gsicc_set_device_profile(pgs->icc_manager, dev, pgs->memory);
            /* Check that we are OK in our setting of the profile.  If not,
               then we go ahead and grab the proper default rather than 
               render wrong or even worst crash */
            if (code < 0 || dev->device_icc_profile->num_comps != 
                dev->color_info.num_components) {
                if (pgs->icc_manager->profiledir != NULL) {
                    strcpy(dev->color_info.icc_profile, pgs->icc_manager->profiledir);
                } else {
                    /* profiledir not set.  go ahead and clear out the 
                       current profile information to avoid two profile 
                       names getting concatenated */
                    dev->color_info.icc_profile[0] = '\0';
                }
                switch(dev->color_info.num_components) {
                    case 1:
                        strcat(dev->color_info.icc_profile, DEFAULT_GRAY_ICC);
                        break;
                    case 3:
                        strcat(dev->color_info.icc_profile, DEFAULT_RGB_ICC);
                        break;
                    case 4:
                        strcat(dev->color_info.icc_profile, DEFAULT_CMYK_ICC);
                        break;
                    default:
                        strcat(dev->color_info.icc_profile, DEFAULT_CMYK_ICC);
                        break;
                }
            }
            return(code);
        }
        return(0);
    }
}

/*  This computes the hash code for the device profile and assigns the profile 
    to the device_icc_profile member variable of the device.  This should 
    really occur only one time, but may occur twice if a color model is 
    specified or a non default profile is specified on the command line */
static int 
gsicc_set_device_profile(gsicc_manager_t *icc_manager, gx_device * pdev, 
                         gs_memory_t * mem)
{
    cmm_profile_t *icc_profile;
    stream *str;
    const char *profile = &(pdev->color_info.icc_profile[0]);
    int code;

    /* It is possible that the device profile has already been 
       set.  We need to decrement if so and reset */

    if (pdev->device_icc_profile != NULL) {
        rc_decrement(pdev->device_icc_profile, "gsicc_set_device_profile");
    }
    /* Check if device has a profile.  This should always be true, since if 
       one was not set, we should have set it to the default. 
    */
    if (profile != '\0') {
        str = gsicc_open_search(profile, strlen(profile), mem, icc_manager);
        if (str != NULL) {
            icc_profile = 
                gsicc_profile_new(str, mem, profile, strlen(profile));
            code = sfclose(str);
            pdev->device_icc_profile = icc_profile;

            /* Get the profile handle */
            icc_profile->profile_handle = 
                gsicc_get_profile_handle_buffer(icc_profile->buffer,
                                                icc_profile->buffer_size);

            /* Compute the hash code of the profile. Everything in the 
               ICC manager will have it's hash code precomputed */
            gsicc_get_icc_buff_hash(icc_profile->buffer, 
                                    &(icc_profile->hashcode),
                                    icc_profile->buffer_size);
            icc_profile->hash_is_valid = true;

            /* Get the number of channels in the output profile */
            icc_profile->num_comps = 
                gscms_get_input_channel_count(icc_profile->profile_handle);
            icc_profile->num_comps_out = 
                gscms_get_output_channel_count(icc_profile->profile_handle);
            icc_profile->data_cs = 
                gscms_get_profile_data_space(icc_profile->profile_handle);
        } else
            return gs_rethrow(-1, "cannot find device profile");
    }
    return(0);
}

/* Set the icc profile in the gs_color_space object */
int
gsicc_set_gscs_profile(gs_color_space *pcs, cmm_profile_t *icc_profile, 
                       gs_memory_t * mem)
{
    if (pcs != NULL) {
#if ICC_DUMP
        if (icc_profile->buffer) {
            dump_icc_buffer(icc_profile->buffer_size, "set_gscs", 
                            icc_profile->buffer);
            global_icc_index++;
        }
#endif
        if (pcs->cmm_icc_profile_data != NULL) {
            /* There is already a profile set there */
            /* free it and then set to the new one.  */
            /* should we check the hash code and retain if the same
               or place this job on the caller?  */
            rc_free_icc_profile(mem, (void*) pcs->cmm_icc_profile_data, 
                                "gsicc_set_gscs_profile");
            pcs->cmm_icc_profile_data = icc_profile;
            return(0);
        } else {
            pcs->cmm_icc_profile_data = icc_profile;
            return(0);
        }
    }
    return(-1);
}

cmm_profile_t *
gsicc_profile_new(stream *s, gs_memory_t *memory, const char* pname, 
                  int namelen)
{
    cmm_profile_t *result;
    int code;
    char *nameptr;
    gs_memory_t *mem_nongc = memory->non_gc_memory;

    result = (cmm_profile_t*) gs_alloc_bytes(mem_nongc, sizeof(cmm_profile_t),  
                                    "gsicc_profile_new");
    if (result == NULL) 
        return result;
    memset(result,0,sizeof(gsicc_serialized_profile_t));
    if (namelen > 0) {
        nameptr = (char*) gs_alloc_bytes(mem_nongc, namelen,
	   		     "gsicc_profile_new");
        memcpy(nameptr, pname, namelen);
        result->name = nameptr;
    } else {
        result->name = NULL;
    }
    result->name_length = namelen;

    /* We may not have a stream if we are creating this
       object from our own constructed buffer.  For 
       example if we are converting CalRGB to an ICC type */
    if ( s != NULL) {
        code = gsicc_load_profile_buffer(result, s, mem_nongc);
        if (code < 0) {
            gs_free_object(mem_nongc, result, "gsicc_profile_new");
            return NULL;
        }
    } else {
        result->buffer = NULL;
        result->buffer_size = 0;
    }
    rc_init_free(result, mem_nongc, 1, rc_free_icc_profile);
    result->profile_handle = NULL;
    result->spotnames = NULL;
    result->dev = NULL;
    result->memory = mem_nongc;
    return(result);
}

static void
rc_free_icc_profile(gs_memory_t * mem, void *ptr_in, client_name_t cname)
{
    cmm_profile_t *profile = (cmm_profile_t *)ptr_in;
    int k;
    gsicc_colorname_t *curr_name, *next_name;
    gs_memory_t *mem_nongc =  profile->memory;

    if (profile->rc.ref_count <= 1 ) {
        /* Clear out the buffer if it is full */
        if(profile->buffer != NULL) {
            gs_free_object(mem_nongc, profile->buffer, "rc_free_icc_profile");
            profile->buffer = NULL;
        }

        /* Release this handle if it has been set */
        if(profile->profile_handle != NULL) {
            gscms_release_profile(profile->profile_handle);
            profile->profile_handle = NULL;
        }

        /* Release the name if it has been set */
        if(profile->name != NULL) {
            gs_free_object(mem_nongc ,profile->name,"rc_free_icc_profile");
            profile->name = NULL;
            profile->name_length = 0;
        }
        profile->hash_is_valid = 0;

        /* If we had a DeviceN profile with names 
           deallocate that now */
        if (profile->spotnames != NULL) {
            curr_name = profile->spotnames->head;
            for ( k = 0; k < profile->spotnames->count; k++) {
                next_name = curr_name->next;
                /* Free the name */
                gs_free_object(mem_nongc, curr_name->name, "rc_free_icc_profile");
                /* Free the name structure */
                gs_free_object(mem_nongc, curr_name, "rc_free_icc_profile");
                curr_name = next_name;
            }
            /* Free the main object */
            gs_free_object(mem_nongc, profile->spotnames, "rc_free_icc_profile");
        }
	gs_free_object(mem_nongc, profile, "rc_free_icc_profile");
    }
}

int
gsicc_init_iccmanager(gs_state * pgs)
{
    int code, k;
    const char *pname;
    int namelen;
    gsicc_manager_t *iccmanager = pgs->icc_manager;

    for (k = 0; k < 4; k++) {
        pname = default_profile_params[k].path;
        namelen = strlen(pname);
        code = gsicc_set_profile(iccmanager, pname, namelen+1,
            default_profile_params[k].default_type);
        if (code < 0)
            return gs_rethrow(code, "cannot find default icc profile");
    }
    return(0);
}

gsicc_manager_t *
gsicc_manager_new(gs_memory_t *memory)
{
    gsicc_manager_t *result;

    /* Allocated in stable gc memory.  This done since the profiles
       may be introduced late in the process. */
    result = gs_alloc_struct(memory->stable_memory, gsicc_manager_t, &st_gsicc_manager,
			     "gsicc_manager_new");
    if ( result == NULL )
        return(NULL);
   rc_init_free(result, memory->stable_memory, 1, rc_gsicc_manager_free);
   result->default_gray = NULL;
   result->default_rgb = NULL;
   result->default_cmyk = NULL;
   result->lab_profile = NULL;
   result->device_named = NULL;
   result->output_link = NULL;
   result->proof_profile = NULL;
   result->device_n = NULL;
   result->smask_profiles = NULL;
   result->memory = memory->stable_memory;
   result->profiledir = NULL;
   result->namelen = 0;
   return(result);
}


static void
rc_gsicc_manager_free(gs_memory_t * mem, void *ptr_in, client_name_t cname)
{
    /* Ending the manager.  Decrement the ref counts of the profiles
       and then free the structure */
    gsicc_manager_t *icc_manager = (gsicc_manager_t * ) ptr_in;
    int k;
    gsicc_devicen_entry_t *device_n, *device_n_next;

   rc_decrement(icc_manager->default_cmyk, "rc_gsicc_manager_free");
   rc_decrement(icc_manager->default_gray, "rc_gsicc_manager_free");
   rc_decrement(icc_manager->default_rgb, "rc_gsicc_manager_free");
   rc_decrement(icc_manager->device_named, "rc_gsicc_manager_free");
   rc_decrement(icc_manager->output_link, "rc_gsicc_manager_free");
   rc_decrement(icc_manager->proof_profile, "rc_gsicc_manager_free");
   rc_decrement(icc_manager->lab_profile, "rc_gsicc_manager_free");

   /* Loop through the DeviceN profiles */
   if ( icc_manager->device_n != NULL) {
       device_n = icc_manager->device_n->head;
       for ( k = 0; k < icc_manager->device_n->count; k++) {
           rc_decrement(device_n->iccprofile, "rc_gsicc_manager_free");
           device_n_next = device_n->next;
           gs_free_object(icc_manager->memory, device_n, "rc_gsicc_manager_free");            
           device_n = device_n_next;
       }
       gs_free_object(icc_manager->memory, icc_manager->device_n, 
                      "rc_gsicc_manager_free");
   }
   /* The soft mask profiles */
   if ( icc_manager->smask_profiles != NULL) {
       rc_decrement(icc_manager->smask_profiles->smask_gray, 
           "rc_gsicc_manager_free");
       rc_decrement(icc_manager->smask_profiles->smask_rgb, 
           "rc_gsicc_manager_free");
       rc_decrement(icc_manager->smask_profiles->smask_cmyk, 
           "rc_gsicc_manager_free");
   }
   gs_free_object(icc_manager->memory->non_gc_memory, icc_manager->profiledir, 
                  "rc_gsicc_manager_free");
   gs_free_object(icc_manager->memory, icc_manager, "rc_gsicc_manager_free");
}

/* Allocates and loads the icc buffer from the stream. */
static int
gsicc_load_profile_buffer(cmm_profile_t *profile, stream *s, 
                          gs_memory_t *memory)
{
    int                     num_bytes,profile_size;
    unsigned char           *buffer_ptr;
    int                     code;

    code = srewind(s);  /* Work around for issue with sfread return 0 bytes
                        and not doing a retry if there is an issue.  This
                        is a bug in the stream logic or strmio layer.  Occurs
                        with smask_withicc.pdf on linux 64 bit system */
    /* Get the size from doing a seek to the end and then a rewind instead
       of relying upon the profile size indicated in the header */
    code = sfseek(s,0,SEEK_END);
    profile_size = sftell(s);
    code = srewind(s);
    if (profile_size < ICC_HEADER_SIZE)
        return(-1);
    /* Allocate the buffer, stuff with the profile */
   buffer_ptr = gs_alloc_bytes(memory, profile_size,
					"gsicc_load_profile");
   if (buffer_ptr == NULL)
        return(-1);
   num_bytes = sfread(buffer_ptr,sizeof(unsigned char),profile_size,s);
   if( num_bytes != profile_size) {
       gs_free_object(memory, buffer_ptr, "gsicc_load_profile");
       return(-1);
   }
   profile->buffer = buffer_ptr;
   profile->buffer_size = num_bytes;
   return(0);
}

/* Allocates and loads the named color structure from the stream. */
static int
gsicc_load_namedcolor_buffer(cmm_profile_t *profile, stream *s, 
                          gs_memory_t *memory)
{
    int                     num_bytes,profile_size;
    unsigned char           *buffer_ptr;
    int                     code;

    code = srewind(s);  
    code = sfseek(s,0,SEEK_END);
    profile_size = sftell(s);
    code = srewind(s);
    /* Allocate the buffer, stuff with the profile */
   buffer_ptr = gs_alloc_bytes(memory, profile_size,
					"gsicc_load_profile");
   if (buffer_ptr == NULL)
        return(-1);
   num_bytes = sfread(buffer_ptr,sizeof(unsigned char),profile_size,s);
   if( num_bytes != profile_size) {
       gs_free_object(memory, buffer_ptr, "gsicc_load_profile");
       return(-1);
   }
   profile->buffer = buffer_ptr;
   profile->buffer_size = num_bytes;
   return(0);
}
/* Check if the embedded profile is the same as any of the default profiles */
static void
gsicc_set_default_cs_value(cmm_profile_t *picc_profile, gs_imager_state *pis)
{
    gsicc_manager_t *icc_manager = pis->icc_manager;
    int64_t hashcode = picc_profile->hashcode;

    if ( picc_profile->default_match == DEFAULT_NONE ) {
        switch ( picc_profile->data_cs ) {
            case gsGRAY:
                if ( hashcode == icc_manager->default_gray->hashcode )
                    picc_profile->default_match = DEFAULT_GRAY_s;
                break;
            case gsRGB:
                if ( hashcode == icc_manager->default_rgb->hashcode )
                    picc_profile->default_match = DEFAULT_RGB_s;
                break;
            case gsCMYK:
                if ( hashcode == icc_manager->default_cmyk->hashcode )
                    picc_profile->default_match = DEFAULT_CMYK_s;
                break;
            case gsCIELAB:
                if ( hashcode == icc_manager->lab_profile->hashcode )
                    picc_profile->default_match = LAB_TYPE_s;
                break;
            case gsCIEXYZ:
                return;
                break;
            case gsNCHANNEL:
                return;
                break;
            default:
                return;
        }
    }
}

/* Initialize the hash code value */
void
gsicc_init_hash_cs(cmm_profile_t *picc_profile, gs_imager_state *pis)
{
    if ( !(picc_profile->hash_is_valid) ) {
        gsicc_get_icc_buff_hash(picc_profile->buffer, &(picc_profile->hashcode),
                                picc_profile->buffer_size);
        picc_profile->hash_is_valid = true;
    }
    gsicc_set_default_cs_value(picc_profile, pis);
}

/* Interface code to get the profile handle for data
   stored in the clist device */
gcmmhprofile_t
gsicc_get_profile_handle_clist(cmm_profile_t *picc_profile, gs_memory_t *memory)
{
    gcmmhprofile_t profile_handle = NULL;
    unsigned int profile_size;
    int size;
    gx_device_clist_reader *pcrdev = (gx_device_clist_reader*) picc_profile->dev;
    unsigned char *buffer_ptr;
    int64_t position;

    if( pcrdev != NULL) {

        /* Check ICC table for hash code and get the whole size icc raw buffer
           plus serialized header information */ 
        position = gsicc_search_icc_table(pcrdev->icc_table, 
                                          picc_profile->hashcode, &size);
        if ( position < 0 ) 
            return(0);  /* Not found. */
    
        /* Get the ICC buffer.  We really want to avoid this transfer.  
           I need to write  an interface to the CMM to do this through 
           the clist ioprocs */
        /* Allocate the buffer */
        profile_size = size - sizeof(gsicc_serialized_profile_t);
        /* Profile and its members are ALL in non-gc memory */
        buffer_ptr = gs_alloc_bytes(memory->non_gc_memory, profile_size,
					    "gsicc_get_profile_handle_clist");
        if (buffer_ptr == NULL)
            return(0);
        picc_profile->buffer = buffer_ptr;
        clist_read_chunk(pcrdev, position+sizeof(gsicc_serialized_profile_t), 
            profile_size, (unsigned char *) buffer_ptr);
        profile_handle = gscms_get_profile_handle_mem(buffer_ptr, profile_size);
        return(profile_handle);
     }
     return(0);
}

gcmmhprofile_t
gsicc_get_profile_handle_buffer(unsigned char *buffer, int profile_size)
{

    gcmmhprofile_t profile_handle = NULL;

     if( buffer != NULL) {
         if (profile_size < ICC_HEADER_SIZE) {
             return(0);
         }
         profile_handle = gscms_get_profile_handle_mem(buffer, profile_size);
         return(profile_handle);
     }
     return(0);
}

 /*  If we have a profile for the color space already, then we use that.  
     If we do not have one then we will use data from 
     the ICC manager that is based upon the current color space. */
 cmm_profile_t*
 gsicc_get_gscs_profile(gs_color_space *gs_colorspace, 
                        gsicc_manager_t *icc_manager)
 {
     cmm_profile_t *profile = gs_colorspace->cmm_icc_profile_data;
     gs_color_space_index color_space_index = 
            gs_color_space_get_index(gs_colorspace);
     int code;
     bool islab;

     if (profile != NULL )
        return(profile);
     /* else, return the default types */
     switch( color_space_index ) {
	case gs_color_space_index_DeviceGray:
            return(icc_manager->default_gray);
            break;
	case gs_color_space_index_DeviceRGB:
            return(icc_manager->default_rgb);
            break;
	case gs_color_space_index_DeviceCMYK:
            return(icc_manager->default_cmyk);
            break;
            /* Not sure yet what our response to 
	case gs_color_space_index_DevicePixel:
               this should be */
            return(0);
            break;
       case gs_color_space_index_DeviceN:
            /* If we made it to here, then we will need to use the 
               alternate colorspace */
            return(0);
            break;
       case gs_color_space_index_CIEDEFG:
           /* For now just use default CMYK to avoid segfault.  MJV to fix */
           gs_colorspace->cmm_icc_profile_data = icc_manager->default_cmyk;
           rc_increment(icc_manager->default_cmyk);
           return(gs_colorspace->cmm_icc_profile_data);
           /* Need to convert to an ICC form */
            break;
        case gs_color_space_index_CIEDEF:
           /* For now just use default RGB to avoid segfault.  MJV to fix */
           gs_colorspace->cmm_icc_profile_data = icc_manager->default_rgb;
           rc_increment(icc_manager->default_rgb);
           return(gs_colorspace->cmm_icc_profile_data);
           /* Need to convert to an ICC form */
            break;
        case gs_color_space_index_CIEABC:
            gs_colorspace->cmm_icc_profile_data = 
                gsicc_profile_new(NULL, icc_manager->memory, NULL, 0);
            code = 
                gsicc_create_fromabc(gs_colorspace, 
                        &(gs_colorspace->cmm_icc_profile_data->buffer), 
                        &(gs_colorspace->cmm_icc_profile_data->buffer_size), 
                        icc_manager->memory,
                        &(gs_colorspace->params.abc->caches.DecodeABC.caches[0]),
                        &(gs_colorspace->params.abc->common.caches.DecodeLMN[0]),
                        &islab);
            if (islab) {
                /* Destroy the profile */
                rc_decrement(gs_colorspace->cmm_icc_profile_data,
                             "gsicc_get_gscs_profile");
                /* This may be an issue for pdfwrite */
                return(icc_manager->lab_profile);
            }
            gs_colorspace->cmm_icc_profile_data->default_match = CIE_ABC;
            return(gs_colorspace->cmm_icc_profile_data);
            break;
        case gs_color_space_index_CIEA:
            gs_colorspace->cmm_icc_profile_data = 
                gsicc_profile_new(NULL, icc_manager->memory, NULL, 0);
            code = 
                gsicc_create_froma(gs_colorspace, 
                            &(gs_colorspace->cmm_icc_profile_data->buffer), 
                            &(gs_colorspace->cmm_icc_profile_data->buffer_size), 
                            icc_manager->memory,  
                            &(gs_colorspace->params.a->caches.DecodeA),
                            &(gs_colorspace->params.a->common.caches.DecodeLMN[0]));
            gs_colorspace->cmm_icc_profile_data->default_match = CIE_A;
            return(gs_colorspace->cmm_icc_profile_data);
            break;
        case gs_color_space_index_Separation:
            /* Caller should use named color path */
            return(0);
            break;
        case gs_color_space_index_Pattern:
        case gs_color_space_index_Indexed:
            /* Caller should use the base space for these */
            return(0);
            break;
        case gs_color_space_index_ICC:
            /* This should not occur, as the space
               should have had a populated profile handle */
            return(0);
            break;
     } 
    return(0);
 }

static cmm_profile_t*
gsicc_get_profile( gsicc_profile_t profile_type, gsicc_manager_t *icc_manager )
{

    switch (profile_type) {
        case DEFAULT_GRAY:
            return(icc_manager->default_gray);
        case DEFAULT_RGB:
            return(icc_manager->default_rgb);
        case DEFAULT_CMYK:
             return(icc_manager->default_cmyk);
        case PROOF_TYPE:
             return(icc_manager->proof_profile);
        case NAMED_TYPE:
             return(icc_manager->device_named);
        case LINKED_TYPE:
             return(icc_manager->output_link);
        case LAB_TYPE:
             return(icc_manager->lab_profile);
        case DEVICEN_TYPE:
            /* TO DO */
            return(NULL);
        case DEFAULT_NONE:
        default:
            return(NULL);
    }
    return(NULL);
 }

static int64_t
gsicc_search_icc_table(clist_icctable_t *icc_table, int64_t icc_hashcode, int *size)
{
    int tablesize = icc_table->tablesize, k;
    clist_icctable_entry_t *curr_entry;

    curr_entry = icc_table->head;
    for (k = 0; k < tablesize; k++ ) {    
        if ( curr_entry->serial_data.hashcode == icc_hashcode ) {
            *size = curr_entry->serial_data.size;
            return(curr_entry->serial_data.file_position);
        }
        curr_entry = curr_entry->next;
    }

    /* Did not find it! */
    *size = 0;
    return(-1);
}

/* This is used to get only the serial data from the clist.  We don't bother
   with the whole profile until we actually need it.  It may be that the link
   that we need is already in the link cache */
cmm_profile_t*
gsicc_read_serial_icc(gx_device *dev, int64_t icc_hashcode)
{
    cmm_profile_t *profile;
    int64_t position;
    int size;
    int code;
    gx_device_clist_reader *pcrdev = (gx_device_clist_reader*) dev;

    /* Create a new ICC profile structure */
    profile = gsicc_profile_new(NULL, pcrdev->memory, NULL, 0);
    if (profile == NULL)
        return(NULL);  

    /* Check ICC table for hash code and get the whole size icc raw buffer
       plus serialized header information. Make sure the icc_table has 
       been intialized */
    if (pcrdev->icc_table == NULL) {
        code = clist_read_icctable(pcrdev);
        if (code<0)
            return(NULL);
    }
    position = gsicc_search_icc_table(pcrdev->icc_table, icc_hashcode, &size);
    if ( position < 0 ) 
        return(NULL);
    
    /* Get the serialized portion of the ICC profile information */
    clist_read_chunk(pcrdev, position, sizeof(gsicc_serialized_profile_t), 
                     (unsigned char *) profile);
    return(profile);
}

void 
gsicc_profile_serialize(gsicc_serialized_profile_t *profile_data, 
                        cmm_profile_t *icc_profile)
{
    int k;
  
    if (icc_profile == NULL)
        return;
    profile_data->buffer_size = icc_profile->buffer_size;
    profile_data->data_cs = icc_profile->data_cs;
    profile_data->default_match = icc_profile->default_match;
    profile_data->hash_is_valid = icc_profile->hash_is_valid;
    profile_data->hashcode = icc_profile->hashcode;
    profile_data->islab = icc_profile->islab;
    profile_data->num_comps = icc_profile->num_comps;
    for ( k = 0; k < profile_data->num_comps; k++ ) {
        profile_data->Range.ranges[k].rmax = 
            icc_profile->Range.ranges[k].rmax;
        profile_data->Range.ranges[k].rmin = 
            icc_profile->Range.ranges[k].rmin;
    }
}

/* Utility functions */

int 
gsicc_getsrc_channel_count(cmm_profile_t *icc_profile)
{
    return(gscms_get_input_channel_count(icc_profile->profile_handle));
}

/*
 * Adjust the reference count of the profile. This is intended to support
 * applications (such as XPS) which maintain ICC profiles outside of the
 * graphic state. 
 */
void
gsicc_profile_reference(cmm_profile_t *icc_profile, int delta)
{
    if (icc_profile != NULL)
        rc_adjust(icc_profile, delta, "gsicc_profile_reference");
}

#if ICC_DUMP
/* Debug dump of ICC buffer data */
static void
dump_icc_buffer(int buffersize, char filename[],byte *Buffer)
{
    char full_file_name[50];
    FILE *fid;

    sprintf(full_file_name,"%d)%s_debug.icc",global_icc_index,filename);
    fid = fopen(full_file_name,"wb");
    fwrite(Buffer,sizeof(unsigned char),buffersize,fid);
    fclose(fid);
}
#endif
