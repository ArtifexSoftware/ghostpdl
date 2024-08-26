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

/*  GS ICC Manager.  Initial stubbing of functions.  */

#include "std.h"
#include "stdpre.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "scommon.h"
#include "strmio.h"
#include "gx.h"
#include "gp.h"
#include "gxgstate.h"
#include "gxcspace.h"
#include "gscms.h"
#include "gsicc_manage.h"
#include "gsicc_cache.h"
#include "gsicc_profilecache.h"
#include "gsicc_cms.h"
#include "gserrors.h"
#include "string_.h"
#include "gxclist.h"
#include "gxcldev.h"
#include "gzstate.h"
#include "gsicc_create.h"
#include "gpmisc.h"
#include "gxdevice.h"
#include "gxdevsop.h"
#include "assert_.h"

#define ICC_HEADER_SIZE 128
#define CREATE_V2_DATA 0

#if ICC_DUMP
unsigned int global_icc_index = 0;
#endif

/* Needed for gsicc_set_devicen_equiv_colors. */
extern const gs_color_space_type gs_color_space_type_ICC;

/* Static prototypes */

static void gsicc_set_default_cs_value(cmm_profile_t *picc_profile,
                                       gs_gstate *pgs);
static gsicc_namelist_t* gsicc_new_namelist(gs_memory_t *memory);
static gsicc_colorname_t* gsicc_new_colorname(gs_memory_t *memory);
static gsicc_namelist_t* gsicc_get_spotnames(gcmmhprofile_t profile,
                                             gs_memory_t *memory);
static void gsicc_manager_free_contents(gsicc_manager_t *icc_man,
                                        client_name_t cname);

static void rc_gsicc_manager_free(gs_memory_t * mem, void *ptr_in,
                                  client_name_t cname);
static void rc_free_icc_profile(gs_memory_t * mem, void *ptr_in,
                                client_name_t cname);
static int gsicc_load_profile_buffer(cmm_profile_t *profile, stream *s,
                                     gs_memory_t *memory);
static int64_t gsicc_search_icc_table(clist_icctable_t *icc_table,
                                      int64_t icc_hashcode, int *size);
static int gsicc_load_namedcolor_buffer(cmm_profile_t *profile, stream *s,
                          gs_memory_t *memory);
static cmm_srcgtag_profile_t* gsicc_new_srcgtag_profile(gs_memory_t *memory);
static void gsicc_free_spotnames(gsicc_namelist_t *spotnames, gs_memory_t * mem);

static void
gsicc_manager_finalize(const gs_memory_t *memory, void * vptr);

static void
gsicc_smask_finalize(const gs_memory_t *memory, void * vptr);

/* profile data structure */
/* profile_handle should NOT be garbage collected since it is allocated by the external CMS */
gs_private_st_ptrs2(st_gsicc_colorname, gsicc_colorname_t, "gsicc_colorname",
                    gsicc_colorname_enum_ptrs, gsicc_colorname_reloc_ptrs, name, next);

gs_private_st_ptrs2_final(st_gsicc_manager, gsicc_manager_t, "gsicc_manager",
                    gsicc_manager_enum_ptrs, gsicc_manager_profile_reloc_ptrs,
                    gsicc_manager_finalize, smask_profiles, device_n);

gs_private_st_simple_final(st_gsicc_smask, gsicc_smask_t, "gsicc_smask", gsicc_smask_finalize);

gs_private_st_ptrs2(st_gsicc_devicen, gsicc_devicen_t, "gsicc_devicen",
                gsicc_devicen_enum_ptrs, gsicc_devicen_reloc_ptrs, head, final);

gs_private_st_ptrs1(st_gsicc_devicen_entry, gsicc_devicen_entry_t,
                    "gsicc_devicen_entry", gsicc_devicen_entry_enum_ptrs,
                    gsicc_devicen_entry_reloc_ptrs, next);

typedef struct default_profile_def_s {
    const char *path;
    gsicc_profile_t default_type;
} default_profile_def_t;

static default_profile_def_t default_profile_params[] =
{
    {DEFAULT_GRAY_ICC, DEFAULT_GRAY},
    {DEFAULT_RGB_ICC, DEFAULT_RGB},
    {DEFAULT_CMYK_ICC, DEFAULT_CMYK},
    {LAB_ICC, LAB_TYPE}
};

void
gsicc_setcoloraccuracy(gs_memory_t *mem, uint level)
{
    gs_lib_ctx_t *ctx = gs_lib_ctx_get_interp_instance(mem);

    ctx->icc_color_accuracy = level;
}

uint
gsicc_currentcoloraccuracy(gs_memory_t *mem)
{
    gs_lib_ctx_t *ctx = gs_lib_ctx_get_interp_instance(mem);

    return ctx->icc_color_accuracy;
}

/* Get the size of the ICC profile that is in the buffer */
unsigned int
gsicc_getprofilesize(unsigned char *buffer)
{
    return ( (buffer[0] << 24) + (buffer[1] << 16) +
             (buffer[2] << 8)  +  buffer[3] );
}

/* Get major and minor ICC version number */
int
gsicc_getprofilevers(cmm_profile_t *icc_profile, unsigned char *major,
    unsigned char *minor)
{
    if (icc_profile == NULL || icc_profile->buffer == NULL)
        return -1;

    *major = icc_profile->buffer[8];
    *minor = icc_profile->buffer[9];

    return 0;
}

void
gsicc_set_icc_range(cmm_profile_t **icc_profile)
{
    int num_comp = (*icc_profile)->num_comps;
    int k;

    for ( k = 0; k < num_comp; k++) {
        (*icc_profile)->Range.ranges[k].rmin = 0.0;
        (*icc_profile)->Range.ranges[k].rmax = 1.0;
    }
}

cmm_profile_t*
gsicc_set_iccsmaskprofile(const char *pname,
                          int namelen, gsicc_manager_t *icc_manager,
                          gs_memory_t *mem)
{
    stream *str;
    int code;
    cmm_profile_t *icc_profile;

    if (icc_manager == NULL) {
        code = gsicc_open_search(pname, namelen, mem, NULL, 0, &str);
    } else {
        code = gsicc_open_search(pname, namelen, mem, mem->gs_lib_ctx->profiledir,
                                 mem->gs_lib_ctx->profiledir_len, &str);
    }
    if (code < 0 || str == NULL)
        return NULL;
    icc_profile = gsicc_profile_new(str, mem, pname, namelen);
    code = sfclose(str);
    if (icc_profile == NULL)
        return NULL;
    /* Get the profile handle */
    icc_profile->profile_handle =
            gsicc_get_profile_handle_buffer(icc_profile->buffer,
                                            icc_profile->buffer_size,
                                            mem);
    if (!icc_profile->profile_handle) {
        rc_free_icc_profile(mem, icc_profile, "gsicc_set_iccsmaskprofile");
        return NULL;
    }
    /* Compute the hash code of the profile. Everything in the
       ICC manager will have it's hash code precomputed */
    gsicc_get_icc_buff_hash(icc_profile->buffer, &(icc_profile->hashcode),
                            icc_profile->buffer_size);
    icc_profile->hash_is_valid = true;
    icc_profile->num_comps =
            gscms_get_input_channel_count(icc_profile->profile_handle, icc_profile->memory);
    icc_profile->num_comps_out =
            gscms_get_output_channel_count(icc_profile->profile_handle, icc_profile->memory);
    icc_profile->data_cs =
            gscms_get_profile_data_space(icc_profile->profile_handle, icc_profile->memory);
    gsicc_set_icc_range(&icc_profile);
    return icc_profile;
}

static void
gsicc_smask_finalize(const gs_memory_t *memory, void * vptr)
{
    gsicc_smask_t *iccsmask = (gsicc_smask_t *)vptr;

    gsicc_adjust_profile_rc(iccsmask->smask_gray, -1,
        "gsicc_smask_finalize");
    gsicc_adjust_profile_rc(iccsmask->smask_rgb, -1,
        "gsicc_smask_finalize");
    gsicc_adjust_profile_rc(iccsmask->smask_cmyk, -1,
        "gsicc_smask_finalize");
}

gsicc_smask_t*
gsicc_new_iccsmask(gs_memory_t *memory)
{
    gsicc_smask_t *result;

    result = (gsicc_smask_t *) gs_alloc_struct(memory, gsicc_smask_t, &st_gsicc_smask, "gsicc_new_iccsmask");
    if (result != NULL) {
        result->smask_gray = NULL;
        result->smask_rgb = NULL;
        result->smask_cmyk = NULL;
        result->memory = memory;
        result->swapped = false;
    }
    return result;
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
        return gs_throw(gs_error_VMerror, "insufficient memory to allocate smask profiles");
    /* Load the gray, rgb, and cmyk profiles */
    if ((icc_manager->smask_profiles->smask_gray =
        gsicc_set_iccsmaskprofile(SMASK_GRAY_ICC, strlen(SMASK_GRAY_ICC),
        icc_manager, stable_mem) ) == NULL)
        goto error;
    if ((icc_manager->smask_profiles->smask_rgb =
        gsicc_set_iccsmaskprofile(SMASK_RGB_ICC, strlen(SMASK_RGB_ICC),
        icc_manager, stable_mem)) == NULL)
        goto error;
    if ((icc_manager->smask_profiles->smask_cmyk =
        gsicc_set_iccsmaskprofile(SMASK_CMYK_ICC, strlen(SMASK_CMYK_ICC),
        icc_manager, stable_mem)) == NULL)
        goto error;

    /* Set these as "default" so that pdfwrite or other high level devices
       will know that these are manufactured profiles, and default spaces
       should be used */
    icc_manager->smask_profiles->smask_gray->default_match = DEFAULT_GRAY;
    icc_manager->smask_profiles->smask_rgb->default_match = DEFAULT_RGB;
    icc_manager->smask_profiles->smask_cmyk->default_match = DEFAULT_CMYK;
    return 0;

error:
    if (icc_manager->smask_profiles->smask_gray)
        rc_free_icc_profile(stable_mem, icc_manager->smask_profiles->smask_gray, "gsicc_initialize_iccsmask");
    icc_manager->smask_profiles->smask_gray = NULL;
    if (icc_manager->smask_profiles->smask_rgb)
        rc_free_icc_profile(stable_mem, icc_manager->smask_profiles->smask_rgb, "gsicc_initialize_iccsmask");
    icc_manager->smask_profiles->smask_rgb = NULL;
    if (icc_manager->smask_profiles->smask_cmyk)
        rc_free_icc_profile(stable_mem, icc_manager->smask_profiles->smask_cmyk, "gsicc_initialize_iccsmask");
    icc_manager->smask_profiles->smask_cmyk = NULL;
    gs_free_object(stable_mem, icc_manager->smask_profiles, "gsicc_initialize_iccsmask");
    icc_manager->smask_profiles = NULL;
    return gs_throw(-1, "failed to load an smask profile");
}

static int
gsicc_new_devicen(gsicc_manager_t *icc_manager)
{
/* Allocate a new deviceN ICC profile entry in the deviceN list */
    gsicc_devicen_entry_t *device_n_entry =
        gs_alloc_struct(icc_manager->memory, gsicc_devicen_entry_t,
                &st_gsicc_devicen_entry, "gsicc_new_devicen");
    if (device_n_entry == NULL)
        return gs_throw(gs_error_VMerror, "insufficient memory to allocate device n profile");
    device_n_entry->next = NULL;
    device_n_entry->iccprofile = NULL;
/* Check if we already have one in the manager */
    if ( icc_manager->device_n == NULL ) {
        /* First one.  Need to allocate the DeviceN main object */
        icc_manager->device_n = gs_alloc_struct(icc_manager->memory,
            gsicc_devicen_t, &st_gsicc_devicen, "gsicc_new_devicen");

        if (icc_manager->device_n == NULL)
            return gs_throw(gs_error_VMerror, "insufficient memory to allocate device n profile");

        icc_manager->device_n->head = device_n_entry;
        icc_manager->device_n->final = device_n_entry;
        icc_manager->device_n->count = 1;
        return 0;
    } else {
        /* We have one or more in the list. */
        icc_manager->device_n->final->next = device_n_entry;
        icc_manager->device_n->final = device_n_entry;
        icc_manager->device_n->count++;
        return 0;
    }
}

cmm_profile_t*
gsicc_finddevicen(const gs_color_space *pcs, gsicc_manager_t *icc_manager)
{
    int k,j,i;
    gsicc_devicen_entry_t *curr_entry;
    int num_comps;
    char **names = pcs->params.device_n.names;
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
                pname = (unsigned char *)names[j];
                name_size = strlen(names[j]);
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
    return NULL;
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

    num_colors = gscms_get_numberclrtnames(profile, memory);
    if (num_colors == 0)
        return(NULL);
    /* Allocate structure for managing this */
    list = gsicc_new_namelist(memory);
    if (list == NULL)
        return(NULL);
    curr_entry = &(list->head);
    list->count = num_colors;
    for (k = 0; k < num_colors; k++) {
        /* Allocate a new name object */
        clr_name = gscms_get_clrtname(profile, k, memory);
        if (clr_name == NULL)
            break;
        name = gsicc_new_colorname(memory);
        if (name == NULL) {
            /* FIXME: Free clr_name */
            gs_free_object(memory, clr_name, "gsicc_get_spotnames");
            break;
        }
        /* Get the name */
        name->name = clr_name;
        name->length = strlen(clr_name);
        *curr_entry = name;
        curr_entry = &(name->next);
    }
    if (k < num_colors) {
        /* Failed allocation */
        gsicc_free_spotnames(list, memory);
        return NULL;
    }
    return list;
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
                                                icc_profile->buffer_size,
                                                memory);
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
    if (result == NULL)
        return NULL;
    result->count = 0;
    result->head = NULL;
    result->name_str = NULL;
    result->color_map = NULL;
    return result;
}

/* Allocate new spot name.  */
static gsicc_colorname_t*
gsicc_new_colorname(gs_memory_t *memory)
{
    gsicc_colorname_t *result;

    result = gs_alloc_struct(memory,gsicc_colorname_t,
                &st_gsicc_colorname, "gsicc_new_colorname");
    if (result == NULL)
        return NULL;
    result->length = 0;
    result->name = NULL;
    result->next = NULL;
    return result;
}

/* If the profile is one of the default types that were set in the iccmanager,
   then the index for that type is returned.  Otherwise the ICC index is returned.
   This is currently used to keep us from writing out the default profiles for
   high level devices, if desired. */
gs_color_space_index
gsicc_get_default_type(cmm_profile_t *profile_data)
{
    switch (profile_data->default_match) {
        case DEFAULT_GRAY:
            return gs_color_space_index_DeviceGray;
        case DEFAULT_RGB:
            return gs_color_space_index_DeviceRGB;
        case DEFAULT_CMYK:
            return gs_color_space_index_DeviceCMYK;
        case CIE_A:
            return gs_color_space_index_CIEA;
        case CIE_ABC:
            return gs_color_space_index_CIEABC;
        case CIE_DEF:
            return gs_color_space_index_CIEDEF;
        case CIE_DEFG:
            return gs_color_space_index_CIEDEFG;
        default:
            return gs_color_space_index_ICC;
    }
}

int
gsicc_use_fast_color(cmm_profile_t* profile_data)
{
    switch (profile_data->default_match) {
    case CIE_A:
    case CIE_ABC:
    case CIE_DEF:
    case CIE_DEFG:
    case LAB_TYPE:
    case NAMED_TYPE:
    case DEVICEN_TYPE:
        return 0;
    default:
        return profile_data->num_comps;
    }
}

bool
gsicc_is_default_profile(cmm_profile_t *profile_data)
{
    switch (profile_data->default_match) {
        case DEFAULT_GRAY:
        case DEFAULT_RGB:
        case DEFAULT_CMYK:
            return true;
        default:
            return false;
    }
}

bool
gsicc_profile_from_ps(cmm_profile_t *profile_data)
{
    switch ( profile_data->default_match ) {
        case CIE_A:
        case CIE_ABC:
        case CIE_DEF:
        case CIE_DEFG:
            return true;
        default:
            return false;
    }
}

/*
 * Adjust the reference count of the profile. This is intended to support
 * applications (such as XPS) which maintain ICC profiles outside of the
 * graphic state.
 */
/* for multi-threaded use, we need to adjust the ref_count safely */
void
gsicc_adjust_profile_rc(cmm_profile_t *profile_data, int delta, const char *name_str)
{
    if (profile_data != NULL) {
        gx_monitor_enter(profile_data->lock);
        if (profile_data->rc.ref_count == 1 && delta < 0) {
            profile_data->rc.ref_count = 0;		/* while locked */
            gx_monitor_leave(profile_data->lock);
            rc_free_struct(profile_data, name_str);
        } else {
            rc_adjust(profile_data, delta, name_str);
            gx_monitor_leave(profile_data->lock);
        }
    }
}

/* Fill in the actual source structure rending information */
static int
gsicc_fill_srcgtag_item(gsicc_rendering_param_t *r_params, char **pstrlast, bool cmyk)
{
    char *curr_ptr;
    int blackptcomp;
    int or_icc, preserve_k;
    int ri;

    /* Get the intent */
    curr_ptr = gs_strtok(NULL, "\t, \n\r", pstrlast);
    if (sscanf(curr_ptr, "%d", &ri) != 1)
        return_error(gs_error_unknownerror);
    r_params->rendering_intent = ri | gsRI_OVERRIDE;
    /* Get the black point compensation setting */
    curr_ptr = gs_strtok(NULL, "\t, \n\r", pstrlast);
    if (sscanf(curr_ptr, "%d", &blackptcomp) != 1)
        return_error(gs_error_unknownerror);
    r_params->black_point_comp = blackptcomp | gsBP_OVERRIDE;
    /* Get the over-ride embedded ICC boolean */
    curr_ptr = gs_strtok(NULL, "\t, \n\r", pstrlast);
    if (sscanf(curr_ptr, "%d", &or_icc) != 1)
        return_error(gs_error_unknownerror);
    r_params->override_icc = or_icc;
    if (cmyk) {
        /* Get the preserve K control */
        curr_ptr = gs_strtok(NULL, "\t, \n\r", pstrlast);
        if (sscanf(curr_ptr, "%d", &preserve_k) < 1)
            return_error(gs_error_unknownerror);
        r_params->preserve_black = preserve_k | gsKP_OVERRIDE;
    } else {
        r_params->preserve_black = gsBKPRESNOTSPECIFIED;
    }
    return 0;
}

static int
gsicc_check_device_link(cmm_profile_t *icc_profile, gs_memory_t *memory)
{
    bool value;

    value = gscms_is_device_link(icc_profile->profile_handle, memory);
    icc_profile->isdevlink = value;

    return value;
}

int
gsicc_get_device_class(cmm_profile_t *icc_profile)
{
    return gscms_get_device_class(icc_profile->profile_handle, icc_profile->memory);
}

/* This inititializes the srcgtag structure in the ICC manager */
static int
gsicc_set_srcgtag_struct(gsicc_manager_t *icc_manager, const char* pname,
                        int namelen)
{
    gs_memory_t *mem;
    stream *str;
    int code;
    int info_size;
    char *buffer_ptr, *curr_ptr, *last;
    int num_bytes;
    int k;
    static const char *const srcgtag_keys[] = {GSICC_SRCGTAG_KEYS};
    cmm_profile_t *icc_profile;
    cmm_srcgtag_profile_t *srcgtag;
    bool start = true;
    gsicc_cmm_t cmm = gsCMM_DEFAULT;

    /* If we don't have an icc manager or if this thing is already set
       then ignore the call.  For now, I am going to allow it to
       be set one time. */
    if (icc_manager == NULL || icc_manager->srcgtag_profile != NULL) {
        return 0;
    } else {
        mem = icc_manager->memory->non_gc_memory;
        code = gsicc_open_search(pname, namelen, mem, mem->gs_lib_ctx->profiledir,
                                 mem->gs_lib_ctx->profiledir_len, &str);
        if (code < 0)
            return code;
    }
    if (str != NULL) {
        /* Get the information in the file */
        code = sfseek(str,0,SEEK_END);
        if (code < 0)
            return code;
        info_size = sftell(str);
        code = srewind(str);
        if (code < 0)
            return code;
        if (info_size > (GSICC_NUM_SRCGTAG_KEYS + 1) * FILENAME_MAX) {
            return gs_throw1(-1, "setting of %s src obj color info failed",
                               pname);
        }
        /* Allocate the buffer, stuff with the data */
        buffer_ptr = (char*) gs_alloc_bytes(mem, info_size+1,
                                            "gsicc_set_srcgtag_struct");
        if (buffer_ptr == NULL) {
            return gs_throw1(gs_error_VMerror, "setting of %s src obj color info failed",
                               pname);
        }
        num_bytes = sfread(buffer_ptr,sizeof(unsigned char), info_size, str);
        code = sfclose(str);
        if (code < 0)
            return code;
        buffer_ptr[info_size] = 0;
        if (num_bytes != info_size) {
            gs_free_object(mem, buffer_ptr, "gsicc_set_srcgtag_struct");
            return gs_throw1(-1, "setting of %s src obj color info failed",
                               pname);
        }
        /* Create the structure in which we will store this data */
        srcgtag = gsicc_new_srcgtag_profile(mem);
        /* Now parse through the data opening the profiles that are needed */
        curr_ptr = buffer_ptr;
        /* Initialize that we want color management.  Then if profile is not
           present we know we did not want anything special done with that
           source type.  Else if we have no profile and don't want color
           management we will make sure to do that */
        for (k = 0; k < NUM_SOURCE_PROFILES; k++) {
            srcgtag->rgb_rend_cond[k].cmm = gsCMM_DEFAULT;
            srcgtag->cmyk_rend_cond[k].cmm = gsCMM_DEFAULT;
            srcgtag->gray_rend_cond[k].cmm = gsCMM_DEFAULT;
        }
        while (start || strlen(curr_ptr) > 0) {
            if (start) {
                curr_ptr = gs_strtok(buffer_ptr, "\t, \n\r", &last);
                start = false;
            } else {
                curr_ptr = gs_strtok(NULL, "\t, \n\r", &last);
            }
            if (curr_ptr == NULL) break;
            /* Now go ahead and see if we have a match */
            for (k = 0; k < GSICC_NUM_SRCGTAG_KEYS; k++) {
                if (strncmp(curr_ptr, srcgtag_keys[k], strlen(srcgtag_keys[k])) == 0 ) {
                    /* Check if the curr_ptr is None which indicates that this
                       object is not to be color managed.  Also, if the
                       curr_ptr is Replace which indicates we will be doing
                       direct replacement of the colors.  */
                    curr_ptr = gs_strtok(NULL, "\t, \n\r", &last);
                    if (strncmp(curr_ptr, GSICC_SRCTAG_NOCM, strlen(GSICC_SRCTAG_NOCM)) == 0 &&
                        strlen(curr_ptr) == strlen(GSICC_SRCTAG_NOCM)) {
                        cmm = gsCMM_NONE;
                        icc_profile = NULL;
                        break;
                    } else if ((strncmp(curr_ptr, GSICC_SRCTAG_REPLACE, strlen(GSICC_SRCTAG_REPLACE)) == 0 &&
                        strlen(curr_ptr) == strlen(GSICC_SRCTAG_REPLACE))) {
                        cmm = gsCMM_REPLACE;
                        icc_profile = NULL;
                        break;
                    } else {
                        /* Try to open the file and set the profile */
                        code = gsicc_open_search(curr_ptr, strlen(curr_ptr), mem,
                                                 mem->gs_lib_ctx->profiledir,
                                                 mem->gs_lib_ctx->profiledir_len, &str);
                        if (code < 0)
                            return code;
                        if (str != NULL) {
                            icc_profile =
                                gsicc_profile_new(str, mem, curr_ptr, strlen(curr_ptr));
                            code = sfclose(str);
                            if (code < 0)
                                return code;
                        }
                        if (str != NULL && icc_profile != NULL) {
                            code = gsicc_init_profile_info(icc_profile);
                            if (code < 0)
                                return code;
                            cmm = gsCMM_DEFAULT;
                            /* Check if this object is a devicelink profile.
                               If it is then the intent, blackpoint etc. are not
                               read nor used when dealing with these profiles */
                            gsicc_check_device_link(icc_profile, icc_profile->memory);
                            break;
                        } else {
                            /* Failed to open profile file. End this now. */
                            gs_free_object(mem, buffer_ptr, "gsicc_set_srcgtag_struct");
                            rc_decrement(srcgtag, "gsicc_set_srcgtag_struct");
                            return gs_throw1(-1,
                                    "setting of %s src obj color info failed", pname);
                        }
                    }
                }
            }
            /* Get the intent now and set the profile. If GSICC_SRCGTAG_KEYS
               order changes this switch needs to change also */
            switch (k) {
                case COLOR_TUNE:
                    /* Color tune profile. No intent */
                    srcgtag->color_warp_profile = icc_profile;
                    break;
                case VECTOR_CMYK:
                    srcgtag->cmyk_profiles[gsSRC_GRAPPRO] = icc_profile;
                    srcgtag->cmyk_rend_cond[gsSRC_GRAPPRO].cmm = cmm;
                    if (cmm == gsCMM_DEFAULT) {
                        code = gsicc_fill_srcgtag_item(&(srcgtag->cmyk_rend_cond[gsSRC_GRAPPRO]), &last, true);
                        if (code < 0)
                            return code;
                    }
                    break;
                case IMAGE_CMYK:
                    srcgtag->cmyk_profiles[gsSRC_IMAGPRO] = icc_profile;
                    srcgtag->cmyk_rend_cond[gsSRC_IMAGPRO].cmm = cmm;
                    if (cmm == gsCMM_DEFAULT) {
                        code = gsicc_fill_srcgtag_item(&(srcgtag->cmyk_rend_cond[gsSRC_IMAGPRO]), &last, true);
                        if (code < 0)
                            return code;
                    }
                    break;
                case TEXT_CMYK:
                    srcgtag->cmyk_profiles[gsSRC_TEXTPRO] = icc_profile;
                    srcgtag->cmyk_rend_cond[gsSRC_TEXTPRO].cmm = cmm;
                    if (cmm == gsCMM_DEFAULT) {
                        code = gsicc_fill_srcgtag_item(&(srcgtag->cmyk_rend_cond[gsSRC_TEXTPRO]), &last, true);
                        if (code < 0)
                            return code;
                    }
                    break;
                case VECTOR_RGB:
                    srcgtag->rgb_profiles[gsSRC_GRAPPRO] = icc_profile;
                    srcgtag->rgb_rend_cond[gsSRC_GRAPPRO].cmm = cmm;
                    if (cmm == gsCMM_DEFAULT) {
                        code = gsicc_fill_srcgtag_item(&(srcgtag->rgb_rend_cond[gsSRC_GRAPPRO]), &last, false);
                        if (code < 0)
                            return code;
                    }
                   break;
                case IMAGE_RGB:
                    srcgtag->rgb_profiles[gsSRC_IMAGPRO] = icc_profile;
                    srcgtag->rgb_rend_cond[gsSRC_IMAGPRO].cmm = cmm;
                    if (cmm == gsCMM_DEFAULT) {
                        code = gsicc_fill_srcgtag_item(&(srcgtag->rgb_rend_cond[gsSRC_IMAGPRO]), &last, false);
                        if (code < 0)
                            return code;
                    }
                    break;
                case TEXT_RGB:
                    srcgtag->rgb_profiles[gsSRC_TEXTPRO] = icc_profile;
                    srcgtag->rgb_rend_cond[gsSRC_TEXTPRO].cmm = cmm;
                    if (cmm == gsCMM_DEFAULT) {
                        code = gsicc_fill_srcgtag_item(&(srcgtag->rgb_rend_cond[gsSRC_TEXTPRO]), &last, false);
                        if (code < 0)
                            return code;
                    }
                    break;
                case VECTOR_GRAY:
                    srcgtag->gray_profiles[gsSRC_GRAPPRO] = icc_profile;
                    srcgtag->gray_rend_cond[gsSRC_GRAPPRO].cmm = cmm;
                    if (cmm == gsCMM_DEFAULT) {
                        code = gsicc_fill_srcgtag_item(&(srcgtag->gray_rend_cond[gsSRC_GRAPPRO]), &last, false);
                        if (code < 0)
                            return code;
                    }
                    break;
                case IMAGE_GRAY:
                    srcgtag->gray_profiles[gsSRC_IMAGPRO] = icc_profile;
                    srcgtag->gray_rend_cond[gsSRC_IMAGPRO].cmm = cmm;
                    if (cmm == gsCMM_DEFAULT) {
                        code = gsicc_fill_srcgtag_item(&(srcgtag->gray_rend_cond[gsSRC_IMAGPRO]), &last, false);
                        if (code < 0)
                            return code;
                    }
                    break;
                case TEXT_GRAY:
                    srcgtag->gray_profiles[gsSRC_TEXTPRO] = icc_profile;
                    srcgtag->gray_rend_cond[gsSRC_TEXTPRO].cmm = cmm;
                    if (cmm == gsCMM_DEFAULT) {
                        code = gsicc_fill_srcgtag_item(&(srcgtag->gray_rend_cond[gsSRC_TEXTPRO]), &last, false);
                        if (code < 0)
                            return code;
                    }
                    break;
                case GSICC_NUM_SRCGTAG_KEYS:
                    /* Failed to match the key */
                    gs_free_object(mem, buffer_ptr, "gsicc_set_srcgtag_struct");
                    rc_decrement(srcgtag, "gsicc_set_srcgtag_struct");
                    return gs_throw1(-1, "failed to find key in %s", pname);
                    break;
                default:
                    /* Some issue */
                    gs_free_object(mem, buffer_ptr, "gsicc_set_srcgtag_struct");
                    rc_decrement(srcgtag, "gsicc_set_srcgtag_struct");
                    return gs_throw1(-1, "Error in srcgtag data %s", pname);
                    break;
            }
        }
    } else {
        return gs_throw1(-1, "setting of %s src obj color info failed", pname);
    }
    gs_free_object(mem, buffer_ptr, "gsicc_set_srcgtag_struct");
    srcgtag->name_length = namelen;
    srcgtag->name = (char*) gs_alloc_bytes(mem, srcgtag->name_length + 1,
                                  "gsicc_set_srcgtag_struct");
    if (srcgtag->name == NULL)
        return gs_throw(gs_error_VMerror, "Insufficient memory for tag name");
    memcpy(srcgtag->name, pname, namelen);
    srcgtag->name[namelen] = 0x00;
    icc_manager->srcgtag_profile = srcgtag;
    return 0;
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
    int num_comps = 0;
    gsicc_colorbuffer_t default_space; /* Used to verify that we have the correct type */

    /* We need to check for the smask swapped profile condition.  If we are in
       that state, then any requests for setting the profile will be ignored.
       This is valid, since we are in the middle of drawing right now and this
       only would occur if we are doing a vmreclaim while in the middle of
       soft mask rendering */
    default_space = gsUNDEFINED;
    if (icc_manager->smask_profiles !=NULL &&
        icc_manager->smask_profiles->swapped == true) {
            return 0;
    } else {
        switch(defaulttype) {
            case DEFAULT_GRAY:
                manager_default_profile = &(icc_manager->default_gray);
                default_space = gsGRAY;
                num_comps = 1;
                break;
            case DEFAULT_RGB:
                manager_default_profile = &(icc_manager->default_rgb);
                default_space = gsRGB;
                num_comps = 3;
                break;
            case DEFAULT_CMYK:
                 manager_default_profile = &(icc_manager->default_cmyk);
                 default_space = gsCMYK;
                 num_comps = 4;
                 break;
            case NAMED_TYPE:
                 manager_default_profile = &(icc_manager->device_named);
                 default_space = gsNAMED;
                 break;
            case LAB_TYPE:
                 manager_default_profile = &(icc_manager->lab_profile);
                 num_comps = 3;
                 default_space = gsCIELAB;
                 break;
            case DEVICEN_TYPE:
                manager_default_profile = NULL;
                default_space = gsNCHANNEL;
                break;
            case DEFAULT_NONE:
            default:
                return 0;
                break;
        }
    }
    /* If it is not NULL then it has already been set. If it is different than
       what we already have then we will want to free it.  Since other
       gs_gstates could have different default profiles, this is done via reference
       counting.  If it is the same as what we already have then we DONT
       increment, since that is done when the gs_gstate is duplicated.  It
       could be the same, due to a resetting of the user params. To avoid
       recreating the profile data, we compare the string names. */
    if (defaulttype != DEVICEN_TYPE && (*manager_default_profile) != NULL) {
        /* Check if this is what we already have.  Also check if it is the
           output intent profile.  */
        icc_profile = *manager_default_profile;
        if ( namelen == icc_profile->name_length ) {
            if( memcmp(pname, icc_profile->name, namelen) == 0)
                return 0;
        }
        if (strncmp(icc_profile->name, OI_PROFILE,
                    icc_profile->name_length) == 0) {
                return 0;
        }
        gsicc_adjust_profile_rc(icc_profile, -1,"gsicc_set_profile");
        /* Icky - if the creation of the new profile fails, we end up with a dangling
           pointer, or a wrong reference count - so NULL the appropriate entry here
         */
        switch(defaulttype) {
            case DEFAULT_GRAY:
                icc_manager->default_gray = NULL;
                break;
            case DEFAULT_RGB:
                icc_manager->default_rgb = NULL;
                break;
            case DEFAULT_CMYK:
                 icc_manager->default_cmyk = NULL;
                 break;
            case NAMED_TYPE:
                 icc_manager->device_named = NULL;
                 break;
            case LAB_TYPE:
                 icc_manager->lab_profile = NULL;
                 break;
            default:
                break;
        }
    }
    /* We need to do a special check for DeviceN since we have a linked list of
       profiles and we can have multiple specifications */
    if (defaulttype == DEVICEN_TYPE) {
        if (icc_manager->device_n != NULL) {
            gsicc_devicen_entry_t *current_entry = icc_manager->device_n->head;
            for (k = 0; k < icc_manager->device_n->count; k++) {
                if (current_entry->iccprofile != NULL) {
                    icc_profile = current_entry->iccprofile;
                    if (namelen == icc_profile->name_length)
                    if (memcmp(pname, icc_profile->name, namelen) == 0)
                        return 0;
                }
                current_entry = current_entry->next;
            }
        }
        /* An entry was not found.  We need to create a new one to use */
        code = gsicc_new_devicen(icc_manager);
        if (code < 0)
            return code;
        manager_default_profile = &(icc_manager->device_n->final->iccprofile);
    }
    code = gsicc_open_search(pname, namelen, mem_gc, mem_gc->gs_lib_ctx->profiledir,
                             mem_gc->gs_lib_ctx->profiledir_len, &str);
    if (code < 0)
        return code;
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
            char *nameptr;

            icc_profile = gsicc_profile_new(NULL, mem_gc, NULL, 0);
            if (icc_profile == NULL)
                return gs_throw(gs_error_VMerror, "Creation of ICC profile failed");
            icc_profile->data_cs = gsNAMED;
            code = gsicc_load_namedcolor_buffer(icc_profile, str, mem_gc);
            if (code < 0) return gs_throw1(-1, "problems with profile %s", pname);
            *manager_default_profile = icc_profile;
            nameptr = (char*) gs_alloc_bytes(icc_profile->memory, namelen+1,
                                             "gsicc_set_profile");
            if (nameptr == NULL)
                return gs_throw(gs_error_VMerror, "Insufficient memory for profile name");
            memcpy(nameptr, pname, namelen);
            nameptr[namelen] = '\0';
            icc_profile->name = nameptr;
            icc_profile->name_length = namelen;
            return 0;  /* Done now, since this is not a standard ICC profile */
        }
        code = sfclose(str);
        if (icc_profile == NULL) {
            return gs_throw1(-1, "problems with profile %s",pname);
        }
         *manager_default_profile = icc_profile;
        icc_profile->default_match = defaulttype;
        if (defaulttype == LAB_TYPE)
            icc_profile->islab = true;
        if ( defaulttype == DEVICEN_TYPE ) {
            /* Lets get the name information out of the profile.
               The names are contained in the icSigNamedColor2Tag
               item.  The table is in the A2B0Tag item.
               The names are in the order such that the fastest
               index in the table is the first name */
            gsicc_get_devicen_names(icc_profile, icc_manager->memory);
            /* Init this profile now */
            code = gsicc_init_profile_info(icc_profile);
            if (code < 0) return gs_throw1(-1, "problems with profile %s", pname);
        } else {
            /* Delay the loading of the handle buffer until we need the profile.
               But set some basic stuff that we need. Take care of DeviceN
               profile now, since we don't know the number of components etc */
            icc_profile->num_comps = num_comps;
            icc_profile->num_comps_out = 3;
            gsicc_set_icc_range(&icc_profile);
            icc_profile->data_cs = default_space;
        }
        return 0;
    }
    return -1;
}

/* This is used ONLY for delayed initialization of the "default" ICC profiles
   that are in the ICC manager.  This way we avoid getting these profile handles
   until we actually need them. Note that defaulttype is preset.  These are
   the *only* profiles that are delayed in this manner.  All embedded profiles
   and internally generated profiles have their handles found immediately */
int
gsicc_initialize_default_profile(cmm_profile_t *icc_profile)
{
    gsicc_profile_t defaulttype = icc_profile->default_match;
    gsicc_colorbuffer_t default_space = gsUNDEFINED;
    int num_comps, num_comps_out;
    gs_memory_t *mem = icc_profile->memory;

    /* Get the profile handle if it is not already set */
    if (icc_profile->profile_handle == NULL) {
        icc_profile->profile_handle =
                        gsicc_get_profile_handle_buffer(icc_profile->buffer,
                                                        icc_profile->buffer_size,
                                                        mem);
        if (icc_profile->profile_handle == NULL) {
            return gs_rethrow1(gs_error_VMerror, "allocation of profile %s handle failed",
                               icc_profile->name);
        }
    }
    if (icc_profile->buffer != NULL && icc_profile->hash_is_valid == false) {
        /* Compute the hash code of the profile. */
        gsicc_get_icc_buff_hash(icc_profile->buffer, &(icc_profile->hashcode),
                                icc_profile->buffer_size);
        icc_profile->hash_is_valid = true;
    }
    num_comps = icc_profile->num_comps;
    icc_profile->num_comps =
        gscms_get_input_channel_count(icc_profile->profile_handle,
            icc_profile->memory);
    num_comps_out = icc_profile->num_comps_out;
    icc_profile->num_comps_out =
        gscms_get_output_channel_count(icc_profile->profile_handle,
            icc_profile->memory);
    icc_profile->data_cs =
        gscms_get_profile_data_space(icc_profile->profile_handle,
            icc_profile->memory);
    if_debug0m(gs_debug_flag_icc,mem,"[icc] Setting ICC profile in Manager\n");
    switch(defaulttype) {
        case DEFAULT_GRAY:
            if_debug0m(gs_debug_flag_icc,mem,"[icc] Default Gray\n");
            default_space = gsGRAY;
            break;
        case DEFAULT_RGB:
            if_debug0m(gs_debug_flag_icc,mem,"[icc] Default RGB\n");
            default_space = gsRGB;
            break;
        case DEFAULT_CMYK:
            if_debug0m(gs_debug_flag_icc,mem,"[icc] Default CMYK\n");
            default_space = gsCMYK;
             break;
        case NAMED_TYPE:
            if_debug0m(gs_debug_flag_icc,mem,"[icc] Named Color\n");
            break;
        case LAB_TYPE:
            if_debug0m(gs_debug_flag_icc,mem,"[icc] CIELAB Profile\n");
            break;
        case DEVICEN_TYPE:
            if_debug0m(gs_debug_flag_icc,mem,"[icc] DeviceN Profile\n");
            break;
        case DEFAULT_NONE:
        default:
            return 0;
            break;
    }
    if_debug1m(gs_debug_flag_icc,mem,"[icc] name = %s\n", icc_profile->name);
    if_debug1m(gs_debug_flag_icc,mem,"[icc] num_comps = %d\n", icc_profile->num_comps);
    /* Check that we have the proper color space for the ICC
       profiles that can be externally set */
    if (default_space != gsUNDEFINED ||
        num_comps != icc_profile->num_comps ||
        num_comps_out != icc_profile->num_comps_out) {
        if (icc_profile->data_cs != default_space) {
            return gs_rethrow(-1, "A default profile has an incorrect color space");
        }
    }
    return 0;
}

/* This is used to get the profile handle given a file name  */
cmm_profile_t*
gsicc_get_profile_handle_file(const char* pname, int namelen, gs_memory_t *mem)
{
    cmm_profile_t *result;
    stream* str;
    int code;

    /* First see if we can get the stream. */
    code = gsicc_open_search(pname, namelen, mem, mem->gs_lib_ctx->profiledir,
        mem->gs_lib_ctx->profiledir_len, &str);
    if (code < 0 || str == NULL) {
        gs_throw(gs_error_VMerror, "Creation of ICC profile failed");
        return NULL;
    }
    result = gsicc_profile_new(str, mem, pname, namelen);
    code = sfclose(str);
    if (result == NULL) {
        gs_throw(gs_error_VMerror, "Creation of ICC profile failed");
        return NULL;
    }
    code = gsicc_init_profile_info(result);
    if (code < 0) {
        gs_throw(gs_error_VMerror, "Creation of ICC profile failed");
        return NULL;
    }
    return result;
}

/* Given that we already have a profile in a buffer (e.g. generated from a PS object)
   this gets the handle and initializes the various member variables that we need */
int
gsicc_init_profile_info(cmm_profile_t *profile)
{
    int k;

    /* Get the profile handle */
    profile->profile_handle =
        gsicc_get_profile_handle_buffer(profile->buffer,
                                        profile->buffer_size,
                                        profile->memory);
    if (profile->profile_handle == NULL)
        return -1;

    /* Compute the hash code of the profile. */
    gsicc_get_icc_buff_hash(profile->buffer, &(profile->hashcode),
                            profile->buffer_size);
    profile->hash_is_valid = true;
    profile->default_match = DEFAULT_NONE;
    profile->num_comps = gscms_get_input_channel_count(profile->profile_handle,
        profile->memory);
    profile->num_comps_out = gscms_get_output_channel_count(profile->profile_handle,
        profile->memory);
    profile->data_cs = gscms_get_profile_data_space(profile->profile_handle,
        profile->memory);

    /* Initialize the range to default values */
    for ( k = 0; k < profile->num_comps; k++) {
        profile->Range.ranges[k].rmin = 0.0;
        profile->Range.ranges[k].rmax = 1.0;
    }
    return 0;
}

/* This is used to try to find the specified or default ICC profiles */
/* This is where we would enhance the directory searching to use a   */
/* list of paths separated by ':' (unix) or ';' Windows              */
int
gsicc_open_search(const char* pname, int namelen, gs_memory_t *mem_gc,
                  const char* dirname, int dirlen, stream**strp)
{
    char *buffer;
    stream* str;

    /* Check if we need to prepend the file name  */
    if ( dirname != NULL) {
        /* If this fails, we will still try the file by itself and with
           %rom% since someone may have left a space some of the spaces
           as our defaults, even if they defined the directory to use.
           This will occur only after searching the defined directory.
           A warning is noted.  */
        buffer = (char *) gs_alloc_bytes(mem_gc, namelen + dirlen + 1,
                                     "gsicc_open_search");
        if (buffer == NULL)
            return_error(gs_error_VMerror);
        memcpy(buffer, dirname, dirlen);
        memcpy(buffer + dirlen, pname, namelen);
        /* Just to make sure we were null terminated */
        buffer[namelen + dirlen] = '\0';

        if (gs_check_file_permission(mem_gc, buffer, strlen(buffer), "r") >= 0) {
            str = sfopen(buffer, "r", mem_gc);
            gs_free_object(mem_gc, buffer, "gsicc_open_search");
            if (str != NULL) {
                *strp = str;
                return 0;
            }
        }
        else {
            gs_free_object(mem_gc, buffer, "gsicc_open_search");
        }
    }

    /* First just try it like it is */
    if (gs_check_file_permission(mem_gc, pname, namelen, "r") >= 0) {
        char CFileName[gp_file_name_sizeof];

        if (namelen + 1 > gp_file_name_sizeof)
            return_error(gs_error_ioerror);
        memcpy(CFileName, pname, namelen);
        CFileName[namelen] = 0x00;

        str = sfopen(CFileName, "r", mem_gc);
        if (str != NULL) {
            *strp = str;
            return 0;
        }
    }

    /* If that fails, try %rom% */ /* FIXME: Not sure this is needed or correct */
                                   /* A better approach might be to have built in defaults */
    buffer = (char *) gs_alloc_bytes(mem_gc, 1 + namelen +
                        strlen(DEFAULT_DIR_ICC),"gsicc_open_search");
    if (buffer == NULL)
        return_error(gs_error_VMerror);
    strcpy(buffer, DEFAULT_DIR_ICC);
    memcpy(buffer + strlen(DEFAULT_DIR_ICC), pname, namelen);
    /* Just to make sure we were null terminated */
    buffer[namelen + strlen(DEFAULT_DIR_ICC)] = '\0';
    str = sfopen(buffer, "r", mem_gc);
    gs_free_object(mem_gc, buffer, "gsicc_open_search");
    if (str == NULL) {
        gs_warn1("Could not find %s ",pname);
    }
    *strp = str;
    return 0;
}

/* Free source object icc array structure.  */
static void
rc_free_srcgtag_profile(gs_memory_t * mem, void *ptr_in, client_name_t cname)
{
    cmm_srcgtag_profile_t *srcgtag_profile = (cmm_srcgtag_profile_t *)ptr_in;
    int k;
    gs_memory_t *mem_nongc =  srcgtag_profile->memory;

    if (srcgtag_profile->rc.ref_count <= 1 ) {
        /* Decrement any profiles. */
        for (k = 0; k < NUM_SOURCE_PROFILES; k++) {
            if (srcgtag_profile->gray_profiles[k] != NULL) {
                gsicc_adjust_profile_rc(srcgtag_profile->gray_profiles[k], -1,
                    "rc_free_srcgtag_profile(gray)");
            }
            if (srcgtag_profile->rgb_profiles[k] != NULL) {
                gsicc_adjust_profile_rc(srcgtag_profile->rgb_profiles[k], -1,
                             "rc_free_srcgtag_profile(rgb)");
            }
            if (srcgtag_profile->cmyk_profiles[k] != NULL) {
                gsicc_adjust_profile_rc(srcgtag_profile->cmyk_profiles[k], -1,
                             "rc_free_srcgtag_profile(cmyk)");
            }
            if (srcgtag_profile->color_warp_profile != NULL) {
                gsicc_adjust_profile_rc(srcgtag_profile->color_warp_profile, -1,
                             "rc_free_srcgtag_profile(warp)");
            }
        }
        gs_free_object(mem_nongc, srcgtag_profile->name, "rc_free_srcgtag_profile");
        gs_free_object(mem_nongc, srcgtag_profile, "rc_free_srcgtag_profile");
    }
}

/* Allocate source object icc structure. */
static cmm_srcgtag_profile_t*
gsicc_new_srcgtag_profile(gs_memory_t *memory)
{
    cmm_srcgtag_profile_t *result;
    int k;

    result = (cmm_srcgtag_profile_t *) gs_alloc_bytes(memory->non_gc_memory,
                                            sizeof(cmm_srcgtag_profile_t),
                                            "gsicc_new_srcgtag_profile");
    if (result == NULL)
        return NULL;
    result->memory = memory->non_gc_memory;

    for (k = 0; k < NUM_SOURCE_PROFILES; k++) {
        result->rgb_profiles[k] = NULL;
        result->cmyk_profiles[k] = NULL;
        result->gray_profiles[k] = NULL;
        result->gray_rend_cond[k].black_point_comp = gsBPNOTSPECIFIED;
        result->gray_rend_cond[k].rendering_intent = gsRINOTSPECIFIED;
        result->gray_rend_cond[k].override_icc = false;
        result->gray_rend_cond[k].preserve_black = gsBKPRESNOTSPECIFIED;
        result->gray_rend_cond[k].cmm = gsCMM_DEFAULT;
        result->rgb_rend_cond[k].black_point_comp = gsBPNOTSPECIFIED;
        result->rgb_rend_cond[k].rendering_intent = gsRINOTSPECIFIED;
        result->rgb_rend_cond[k].override_icc = false;
        result->rgb_rend_cond[k].preserve_black = gsBKPRESNOTSPECIFIED;
        result->rgb_rend_cond[k].cmm = gsCMM_DEFAULT;
        result->cmyk_rend_cond[k].black_point_comp = gsBPNOTSPECIFIED;
        result->cmyk_rend_cond[k].rendering_intent = gsRINOTSPECIFIED;
        result->cmyk_rend_cond[k].override_icc = false;
        result->cmyk_rend_cond[k].preserve_black = gsBKPRESNOTSPECIFIED;
        result->cmyk_rend_cond[k].cmm = gsCMM_DEFAULT;
    }
    result->color_warp_profile = NULL;
    result->name = NULL;
    result->name_length = 0;
    rc_init_free(result, memory->non_gc_memory, 1, rc_free_srcgtag_profile);
    return result;
}

static void
gsicc_free_spotnames(gsicc_namelist_t *spotnames, gs_memory_t * mem)
{
    int k;
    gsicc_colorname_t *curr_name, *next_name;

    curr_name = spotnames->head;
    for (k = 0; k < spotnames->count; k++) {
        next_name = curr_name->next;
        /* Free the name */
        gs_free_object(mem, curr_name->name, "gsicc_free_spotnames");
        /* Free the name structure */
        gs_free_object(mem, curr_name, "gsicc_free_spotnames");
        curr_name = next_name;
    }
    if (spotnames->color_map != NULL) {
        gs_free_object(mem, spotnames->color_map, "gsicc_free_spotnames");
    }
    if (spotnames->name_str != NULL) {
        gs_free_object(mem, spotnames->name_str, "gsicc_free_spotnames");
    }
}

/* Free device icc array structure.  */
static void
rc_free_profile_array(gs_memory_t * mem, void *ptr_in, client_name_t cname)
{
    cmm_dev_profile_t *icc_struct = (cmm_dev_profile_t *)ptr_in;
    int k;
    gs_memory_t *mem_nongc =  icc_struct->memory;

    if (icc_struct->rc.ref_count <= 1 ) {
        /* Decrement any profiles. */
        for (k = 0; k < NUM_DEVICE_PROFILES; k++) {
            if (icc_struct->device_profile[k] != NULL) {
                if_debug1m(gs_debug_flag_icc, mem_nongc,
                           "[icc] Releasing device profile %d\n", k);
                gsicc_adjust_profile_rc(icc_struct->device_profile[k], -1,
                             "rc_free_profile_array");
            }
        }
        if (icc_struct->link_profile != NULL) {
            if_debug0m(gs_debug_flag_icc,mem_nongc,"[icc] Releasing link profile\n");
            gsicc_adjust_profile_rc(icc_struct->link_profile, -1, "rc_free_profile_array");
        }
        if (icc_struct->proof_profile != NULL) {
            if_debug0m(gs_debug_flag_icc,mem_nongc,"[icc] Releasing proof profile\n");
            gsicc_adjust_profile_rc(icc_struct->proof_profile, -1, "rc_free_profile_array");
        }
        if (icc_struct->oi_profile != NULL) {
            if_debug0m(gs_debug_flag_icc,mem_nongc,"[icc] Releasing oi profile\n");
            gsicc_adjust_profile_rc(icc_struct->oi_profile, -1, "rc_free_profile_array");
        }
        if (icc_struct->postren_profile != NULL) {
            if_debug0m(gs_debug_flag_icc, mem_nongc, "[icc] Releasing postren profile\n");
            gsicc_adjust_profile_rc(icc_struct->postren_profile, -1, "rc_free_profile_array");
        }
        if (icc_struct->blend_profile != NULL) {
            if_debug0m(gs_debug_flag_icc, mem_nongc, "[icc] Releasing blend profile\n");
            gsicc_adjust_profile_rc(icc_struct->blend_profile, -1, "rc_free_profile_array");
        }
        if (icc_struct->spotnames != NULL) {
            if_debug0m(gs_debug_flag_icc, mem_nongc, "[icc] Releasing spotnames\n");
            /* Free the linked list in this object */
            gsicc_free_spotnames(icc_struct->spotnames, mem_nongc);
            /* Free the main object */
            gs_free_object(mem_nongc, icc_struct->spotnames, "rc_free_profile_array");
        }
        if_debug0m(gs_debug_flag_icc,mem_nongc,"[icc] Releasing device profile struct\n");
        gs_free_object(mem_nongc, icc_struct, "rc_free_profile_array");
    }
}

/* Allocate device icc structure. The actual profiles are in this structure */
cmm_dev_profile_t*
gsicc_new_device_profile_array(gx_device *dev)
{
    cmm_dev_profile_t *result;
    int k;
    gs_memory_t *memory = dev->memory;

    if_debug0m(gs_debug_flag_icc,memory,"[icc] Allocating device profile struct\n");
    result = (cmm_dev_profile_t *) gs_alloc_bytes(memory->non_gc_memory,
                                            sizeof(cmm_dev_profile_t),
                                            "gsicc_new_device_profile_array");
    if (result == NULL)
        return NULL;
    result->memory = memory->non_gc_memory;

    for (k = 0; k < NUM_DEVICE_PROFILES; k++) {
        result->device_profile[k] = NULL;
        result->rendercond[k].rendering_intent = gsRINOTSPECIFIED;
        result->rendercond[k].black_point_comp = gsBPNOTSPECIFIED;
        result->rendercond[k].override_icc = false;
        result->rendercond[k].preserve_black = gsBKPRESNOTSPECIFIED;
        result->rendercond[k].graphics_type_tag = GS_UNKNOWN_TAG;
        result->rendercond[k].cmm = gsCMM_DEFAULT;
    }
    result->proof_profile = NULL;
    result->link_profile = NULL;
    result->postren_profile = NULL;
    result->blend_profile = NULL;
    result->oi_profile = NULL;
    result->spotnames = NULL;
    result->devicegraytok = true;  /* Default is to map gray to pure K */
    result->graydetection = false;
    result->pageneutralcolor = false;
    result->usefastcolor = false;  /* Default is to not use fast color */
    result->blacktext = false;
    result->blackvector = false;
    result->blackthresholdL = 90.0F;
    result->blackthresholdC = 0.0F;
    result->prebandthreshold = true;
    result->supports_devn = false;
    result->overprint_control = gs_overprint_control_enable;  /* Default overprint if the device can */
    rc_init_free(result, memory->non_gc_memory, 1, rc_free_profile_array);
    return result;
}

int
gsicc_set_device_blackpreserve(gx_device *dev, gsicc_blackpreserve_t blackpreserve,
                                gsicc_profile_types_t profile_type)
{
    int code;
    cmm_dev_profile_t *profile_struct;

    /* Although device methods should not be NULL, they are not completely filled in until
     * gx_device_fill_in_procs is called, and its possible for us to get here before this
     * happens, so we *must* make sure the method is not NULL before we use it.
     */
    if (dev_proc(dev, get_profile) == NULL) {
        profile_struct = dev->icc_struct;
    } else {
        code = dev_proc(dev, get_profile)(dev,  &profile_struct);
        if (code < 0)
            return code;
    }
    if (profile_struct ==  NULL)
        return 0;
    profile_struct->rendercond[profile_type].preserve_black = blackpreserve;
    return 0;
}

int
gsicc_set_device_profile_intent(gx_device *dev, gsicc_rendering_intents_t intent,
                                gsicc_profile_types_t profile_type)
{
    int code;
    cmm_dev_profile_t *profile_struct;

    /* Although device methods should not be NULL, they are not completely filled in until
     * gx_device_fill_in_procs is called, and its possible for us to get here before this
     * happens, so we *must* make sure the method is not NULL before we use it.
     */
    if (dev_proc(dev, get_profile) == NULL) {
        profile_struct = dev->icc_struct;
    } else {
        code = dev_proc(dev, get_profile)(dev,  &profile_struct);
        if (code < 0)
            return code;
    }
    if (profile_struct ==  NULL)
        return 0;
    profile_struct->rendercond[profile_type].rendering_intent = intent;
    return 0;
}

int
gsicc_set_device_blackptcomp(gx_device *dev, gsicc_blackptcomp_t blackptcomp,
                                gsicc_profile_types_t profile_type)
{
    int code = 0;
    cmm_dev_profile_t *profile_struct;

    /* Although device methods should not be NULL, they are not completely filled in until
     * gx_device_fill_in_procs is called, and its possible for us to get here before this
     * happens, so we *must* make sure the method is not NULL before we use it.
     */
    if (dev_proc(dev, get_profile) == NULL) {
        profile_struct = dev->icc_struct;
    } else {
        code = dev_proc(dev, get_profile)(dev,  &profile_struct);
    }
    if (profile_struct ==  NULL)
        return 0;
    profile_struct->rendercond[profile_type].black_point_comp = blackptcomp;
    return code;
}

/* This is used to set up the equivalent cmyk colors for the spots that may
   exist in an output DeviceN profile.  We do this by faking a new separation
   space for each one */
int
gsicc_set_devicen_equiv_colors(gx_device *dev, const gs_gstate * pgs,
                               cmm_profile_t *profile)
{
    gs_gstate temp_state = *((gs_gstate*)pgs);
    gs_color_space *pcspace = gs_cspace_alloc(pgs->memory->non_gc_memory,
                                              &gs_color_space_type_ICC);
    if (pcspace == NULL)
        return gs_throw(gs_error_VMerror, "Insufficient memory for devn equiv colors");
    pcspace->cmm_icc_profile_data = profile;
    temp_state.color[0].color_space = pcspace;
    return dev_proc(dev, update_spot_equivalent_colors)(dev, &temp_state, pcspace);
}

#define DEFAULT_ICC_PROCESS "Cyan, Magenta, Yellow, Black,"
#define DEFAULT_ICC_PROCESS_LENGTH 30
#define DEFAULT_ICC_COLORANT_NAME "ICC_COLOR_"
#define DEFAULT_ICC_COLORANT_LENGTH 12
/* allow at most 16 colorants */
/* This sets the colorants structure up in the device profile for when
   we are dealing with DeviceN type output profiles.  Note
   that this feature is only used with the tiffsep and psdcmyk devices.
   If name_str is null it will use default names for the colorants */
int
gsicc_set_device_profile_colorants(gx_device *dev, char *name_str)
{
    int code;
    cmm_dev_profile_t *profile_struct;
    gsicc_colorname_t *name_entry;
    gsicc_colorname_t **curr_entry;
    gs_memory_t *mem;
    char *temp_ptr, *last = NULL;
    int done;
    gsicc_namelist_t *spot_names;
    char *pch;
    int str_len;
    int k;
    bool free_str = false;

    code = dev_proc(dev, get_profile)((gx_device *)dev, &profile_struct);
    if (profile_struct != NULL) {
        if (name_str == NULL) {
            /* Create a default name string that we can use */
            int total_len;
            int kk;
            int num_comps = profile_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE]->num_comps;
            char temp_str[DEFAULT_ICC_COLORANT_LENGTH+2];

            /* If names are already set then we do not want to set default ones */
            if (profile_struct->spotnames != NULL) {
                /* Check if we have at least as many spot names
                   as there are channels in the proFfile */
                if (num_comps > profile_struct->spotnames->count) {
                    gs_warn("ICC profile colorant names count insufficient");
                    return_error(gs_error_rangecheck);
                } else
                    return 0;
            }

            free_str = true;
            /* Assume first 4 are CMYK */
            total_len = ((DEFAULT_ICC_COLORANT_LENGTH + 1) * (num_comps-4)) +
                        DEFAULT_ICC_PROCESS_LENGTH - 1;  /* -1 due to no comma at end */
            name_str = (char*) gs_alloc_bytes(dev->memory, total_len+1,
                                               "gsicc_set_device_profile_colorants");
            if (name_str == NULL)
                return gs_throw(gs_error_VMerror, "Insufficient memory for colorant name");
            gs_snprintf(name_str, total_len+1, DEFAULT_ICC_PROCESS);
            for (kk = 0; kk < num_comps-5; kk++) {
                gs_snprintf(temp_str,sizeof(temp_str),"ICC_COLOR_%d,",kk);
                strcat(name_str,temp_str);
            }
            /* Last one no comma */
            gs_snprintf(temp_str,sizeof(temp_str),"ICC_COLOR_%d",kk);
            strcat(name_str,temp_str);
        }
        str_len = strlen(name_str);
        if (profile_struct->spotnames != NULL &&
            profile_struct->spotnames->name_str != NULL &&
            strlen(profile_struct->spotnames->name_str) == str_len) {
            /* Here we check if the names are the same */
            if (strncmp(name_str, profile_struct->spotnames->name_str, str_len) == 0) {
                if (free_str)
                    gs_free_object(dev->memory, name_str,
                                            "gsicc_set_device_profile_colorants");
                return 0;
            }
        }
        mem = dev->memory->non_gc_memory;
        /* We need to free the existing one if there was one */
        if (profile_struct->spotnames != NULL) {
            /* Free the linked list in this object */
            gsicc_free_spotnames(profile_struct->spotnames, mem);
            /* Free the main object */
            gs_free_object(mem, profile_struct->spotnames,
                           "gsicc_set_device_profile_colorants");
        }
        /* Allocate structure for managing names */
        spot_names = gsicc_new_namelist(mem);
        profile_struct->spotnames = spot_names;
        spot_names->name_str = (char*) gs_alloc_bytes(mem, str_len+1,
                                               "gsicc_set_device_profile_colorants");
        if (spot_names->name_str == NULL)
            return gs_throw(gs_error_VMerror, "Insufficient memory for spot name");
        memcpy(spot_names->name_str, name_str, strlen(name_str));
        spot_names->name_str[str_len] = 0;
        curr_entry = &(spot_names->head);
         /* Go ahead and tokenize now */
        pch = gs_strtok(name_str, ",", &last);

        while (pch != NULL) {
            if (spot_names->count == GS_CLIENT_COLOR_MAX_COMPONENTS)
                return gs_throw(gs_error_rangecheck, "Too many spot names");

            temp_ptr = pch;
            done = 0;
            /* Remove any leading spaces */
            while (!done) {
                if (*temp_ptr == 0x20) {
                    temp_ptr++;
                } else {
                    done = 1;
                }
            }
            /* Allocate a new name object */
            name_entry = gsicc_new_colorname(mem);
            if (name_entry == NULL)
                return gs_throw(gs_error_VMerror, "Insufficient memory for spot name");
            /* Set our current entry to this one */
            *curr_entry = name_entry;
            spot_names->count += 1;
            name_entry->length = strlen(temp_ptr);
            name_entry->name = (char *) gs_alloc_bytes(mem, name_entry->length,
                                        "gsicc_set_device_profile_colorants");
            if (name_entry->name == NULL)
                return gs_throw(gs_error_VMerror, "Insufficient memory for spot name");
            memcpy(name_entry->name, temp_ptr, name_entry->length);
            /* Get the next entry location */
            curr_entry = &((*curr_entry)->next);
            pch = gs_strtok(NULL, ",", &last);
        }
        /* Create the color map.  Query the device to find out where these
           colorants are located.   It is possible that the device may
           not be opened yet.  In which case, we need to make sure that
           when it is opened that it checks this entry and gets itself
           properly initialized if it is a separation device. */
        spot_names->color_map =
            (gs_devicen_color_map*) gs_alloc_bytes(mem,
                                                   sizeof(gs_devicen_color_map),
                                                   "gsicc_set_device_profile_colorants");
        if (spot_names->color_map == NULL)
            return gs_throw(gs_error_VMerror, "Insufficient memory for spot color map");
        spot_names->color_map->num_colorants = spot_names->count;
        spot_names->color_map->num_components = spot_names->count;

        name_entry = spot_names->head;
        for (k = 0; k < spot_names->count; k++) {
            int colorant_number = (*dev_proc(dev, get_color_comp_index))
                    (dev, (const char *)name_entry->name, name_entry->length,
                     SEPARATION_NAME);
            name_entry = name_entry->next;
            spot_names->color_map->color_map[k] = colorant_number;
        }
        /* We need to set the equivalent CMYK color for this colorant.  This is
           done by faking out the update spot equivalent call with a special
           gs_gstate and color space that makes it seem like the
           spot color is a separation color space.  Unfortunately, we need the
           graphic state to do this so we save it for later when we try to do
           our first mapping.  We then use this flag to know if we did it yet */
        spot_names->equiv_cmyk_set = false;
        if (free_str)
            gs_free_object(dev->memory, name_str,
                           "gsicc_set_device_profile_colorants");
    }
    return code;
}

/* This sets the device profiles. If the device does not have a defined
   profile, then a default one is selected. */
int
gsicc_init_device_profile_struct(gx_device * dev,
                                 char *profile_name,
                                 gsicc_profile_types_t profile_type)
{
    int code;
    cmm_profile_t *curr_profile;
    cmm_dev_profile_t *profile_struct;

    /* See if the device has a profile structure.  If it does, then do a
       check to see if the profile that we are trying to set is already
       set and the same.  If it is not, then we need to free it and then
       reset. */
    profile_struct = dev->icc_struct;
    if (profile_struct != NULL) {
        /* Get the profile of interest */
        if (profile_type < gsPROOFPROFILE) {
            curr_profile = profile_struct->device_profile[profile_type];
        } else {
            /* The proof, link profile or post render */
            if (profile_type == gsPROOFPROFILE) {
                curr_profile = profile_struct->proof_profile;
            } else if (profile_type == gsLINKPROFILE) {
                curr_profile = profile_struct->link_profile;
            } else if (profile_type == gsPRPROFILE) {
                curr_profile = profile_struct->postren_profile;
            } else
                curr_profile = profile_struct->blend_profile;

        }
        /* See if we have the same profile in this location */
        if (curr_profile != NULL) {
            /* There is something there now.  See if what we have coming in
               is different and it is not the output intent.  In this  */
            if (profile_name != NULL && curr_profile->name != NULL) {
                if (strncmp(curr_profile->name, profile_name,
                            strlen(profile_name)) != 0 &&
                    strncmp(curr_profile->name, OI_PROFILE,
                            strlen(curr_profile->name)) != 0) {
                    /* A change in the profile.  rc decrement this one as it
                       will be replaced */
                    gsicc_adjust_profile_rc(curr_profile, -1, "gsicc_init_device_profile_struct");
                    /* Icky - if the creation of the new profile fails, we end up with a dangling
                       pointer, or a wrong reference count - so NULL the appropriate entry here
                     */
                    if (profile_type < gsPROOFPROFILE) {
                        profile_struct->device_profile[profile_type] = NULL;
                    } else {
                        /* The proof, link profile or post render */
                        if (profile_type == gsPROOFPROFILE) {
                            profile_struct->proof_profile = NULL;
                        } else if (profile_type == gsLINKPROFILE) {
                            profile_struct->link_profile = NULL;
                        } else if (profile_type == gsPRPROFILE) {
                            profile_struct->postren_profile = NULL;
                        } else
                            profile_struct->blend_profile = NULL;

                    }

                } else {
                    /* Nothing to change.  It was either the same or is the
                       output intent */
                    return 0;
                }
            }
        }
    } else {
        /* We have no profile structure at all. Allocate the structure in
           non-GC memory.  */
        dev->icc_struct = gsicc_new_device_profile_array(dev);
        profile_struct = dev->icc_struct;
        if (profile_struct == NULL)
            return_error(gs_error_VMerror);
    }
    /* Either use the incoming or a default */
    if (profile_name == NULL) {
        int has_tags = device_encodes_tags(dev);
        profile_name =
            (char *) gs_alloc_bytes(dev->memory,
                                    MAX_DEFAULT_ICC_LENGTH,
                                    "gsicc_init_device_profile_struct");
        if (profile_name == NULL)
            return_error(gs_error_VMerror);
        switch(dev->color_info.num_components - has_tags) {
            case 1:
                strncpy(profile_name, DEFAULT_GRAY_ICC, strlen(DEFAULT_GRAY_ICC));
                profile_name[strlen(DEFAULT_GRAY_ICC)] = 0;
                break;
            case 3:
                strncpy(profile_name, DEFAULT_RGB_ICC, strlen(DEFAULT_RGB_ICC));
                profile_name[strlen(DEFAULT_RGB_ICC)] = 0;
                break;
            case 4:
                strncpy(profile_name, DEFAULT_CMYK_ICC, strlen(DEFAULT_CMYK_ICC));
                profile_name[strlen(DEFAULT_CMYK_ICC)] = 0;
                break;
            default:
                strncpy(profile_name, DEFAULT_CMYK_ICC, strlen(DEFAULT_CMYK_ICC));
                profile_name[strlen(DEFAULT_CMYK_ICC)] = 0;
                break;
        }
        /* Go ahead and set the profile */
        code = gsicc_set_device_profile(dev, dev->memory, profile_name,
                                        profile_type);
        gs_free_object(dev->memory, profile_name,
                       "gsicc_init_device_profile_struct");
        return code;
    } else {
        /* Go ahead and set the profile */
        code = gsicc_set_device_profile(dev, dev->memory, profile_name,
                                        profile_type);
        return code;
    }
}

/* This is used in getting a list of colorant names for the intepreters
   device parameter list. */
char* gsicc_get_dev_icccolorants(cmm_dev_profile_t *dev_profile)
{
    if (dev_profile == NULL || dev_profile->spotnames == NULL ||
        dev_profile->spotnames->name_str == NULL)
        return 0;
    else
        return dev_profile->spotnames->name_str;
}

/* Check that the current state of the profiles is fine. Proof profile is of no
   concern since it is doing a CIELAB to CIELAB mapping in the mashed up
   transform. */
static int
gsicc_verify_device_profiles(gx_device * pdev)
{
    int k;
    cmm_dev_profile_t *dev_icc = pdev->icc_struct;
    bool check_components = true;
    bool can_postrender = false;
    bool objects = false;
    int has_tags = device_encodes_tags(pdev);
    int num_components = pdev->color_info.num_components - has_tags;

    if (dev_proc(pdev, dev_spec_op) != NULL) {
        check_components = !(dev_proc(pdev, dev_spec_op)(pdev, gxdso_skip_icc_component_validation, NULL, 0));
        can_postrender = dev_proc(pdev, dev_spec_op)(pdev, gxdso_supports_iccpostrender, NULL, 0);
    }

    if (dev_icc->device_profile[GS_DEFAULT_DEVICE_PROFILE] == NULL)
        return 0;

    if (dev_icc->postren_profile != NULL && dev_icc->link_profile != NULL) {
        return gs_rethrow(-1, "Post render profile not allowed with device link profile");
    }

    if (dev_icc->blend_profile != NULL) {
        if (!(dev_icc->blend_profile->data_cs == gsGRAY ||
            dev_icc->blend_profile->data_cs == gsRGB ||
            dev_icc->blend_profile->data_cs == gsCMYK))
            return gs_rethrow(-1, "Blending color space must be Gray, RGB or CMYK");
    }

    if (dev_icc->postren_profile != NULL) {
        if (!can_postrender) {
            return gs_rethrow(-1, "Post render profile not supported by device");
        }
        if (check_components) {
            if (dev_icc->postren_profile->num_comps != num_components) {
                return gs_rethrow(-1, "Post render profile does not match the device color model");
            }
            return 0;
        }
        return 0; /* An interesting case with sep device.  Need to do a bit of testing here */
    }

    for (k = 1; k < NUM_DEVICE_PROFILES; k++) {
        if (dev_icc->device_profile[k] != NULL) {
            objects = true;
            break;
        }
    }

    if (dev_icc->link_profile == NULL) {
        if (!objects) {
            if (check_components && dev_icc->device_profile[GS_DEFAULT_DEVICE_PROFILE]->num_comps !=
                num_components)
                return gs_rethrow(-1, "Mismatch of ICC profiles and device color model");
            else
                return 0;  /* Currently sep devices have some leeway here */
        } else {
            if (check_components) {
                for (k = 1; k < NUM_DEVICE_PROFILES; k++)
                    if (dev_icc->device_profile[k] != NULL) {
                        if (dev_icc->device_profile[k]->num_comps != num_components)
                            return gs_rethrow(-1, "Mismatch of object dependent ICC profiles and device color model");
                    }
            }
            return 0;
        }
    } else {
        /* The input of the device link must match the output of the device
           profile and the device link output must match the device color
           model */
        if (check_components && dev_icc->link_profile->num_comps_out !=
            num_components) {
            return gs_rethrow(-1, "Mismatch of device link profile and device color model");
        }
        if (check_components) {
            for (k = 0; k < NUM_DEVICE_PROFILES; k++) {
                if (dev_icc->device_profile[k] != NULL) {
                    if (dev_icc->device_profile[k]->num_comps !=
                        dev_icc->link_profile->num_comps) {
                        return gs_rethrow(-1, "Mismatch of device link profile and device ICC profile");
                    }
                }
            }
        }
        return 0;
    }
}

/*  This computes the hash code for the device profile and assigns the profile
    in the icc_struct member variable of the device.  This should
    really occur only one time, but may occur twice if a color model is
    specified or a nondefault profile is specified on the command line */
int
gsicc_set_device_profile(gx_device * pdev, gs_memory_t * mem,
                         char *file_name, gsicc_profile_types_t pro_enum)
{
    cmm_profile_t *icc_profile;
    stream *str;
    int code;

    if (file_name == NULL)
        return 0;

    /* Check if device has a profile for this slot. Note that we already
       decremented for any profile that we might be replacing
       in gsicc_init_device_profile_struct */
    /* Silent on failure if this is an output intent profile that
     * could not be found.  Bug 695042.  Multi-threaded rendering
     * set up will try to find the file for the profile during the set
     * up via put/get params. but one does not exist.  The OI profile
     * will be cloned after the put/get params */
    if (strncmp(file_name, OI_PROFILE, strlen(OI_PROFILE)) == 0)
        return -1;

    code = gsicc_open_search(file_name, strlen(file_name), mem,
                             mem->gs_lib_ctx->profiledir,
                             mem->gs_lib_ctx->profiledir_len, &str);
    if (code < 0)
        return code;
    if (str == NULL)
        return gs_rethrow(-1, "cannot find device profile");

    icc_profile =
            gsicc_profile_new(str, mem, file_name, strlen(file_name));
    code = sfclose(str);
    if (icc_profile == NULL)
        return gs_throw(gs_error_VMerror, "Creation of ICC profile failed");

    /* Get the profile handle */
    icc_profile->profile_handle =
                gsicc_get_profile_handle_buffer(icc_profile->buffer,
                                                icc_profile->buffer_size,
                                                mem);
    if (icc_profile->profile_handle == NULL) {
        rc_decrement(icc_profile, "gsicc_set_device_profile");
        return_error(gs_error_unknownerror);
    }

    /* Compute the hash code of the profile. Everything in the
       ICC manager will have it's hash code precomputed */
    gsicc_get_icc_buff_hash(icc_profile->buffer,
                            &(icc_profile->hashcode),
                            icc_profile->buffer_size);
    icc_profile->hash_is_valid = true;

    /* Get the number of channels in the output profile */
    icc_profile->num_comps =
                gscms_get_input_channel_count(icc_profile->profile_handle,
                                              icc_profile->memory);
    if_debug1m(gs_debug_flag_icc, mem, "[icc] Profile has %d components\n",
               icc_profile->num_comps);
    icc_profile->num_comps_out =
                gscms_get_output_channel_count(icc_profile->profile_handle,
                                               icc_profile->memory);
    icc_profile->data_cs =
                gscms_get_profile_data_space(icc_profile->profile_handle,
                                             icc_profile->memory);

    /* We need to know if this is one of the "default" profiles or
       if someone has externally set it.  The reason is that if there
       is an output intent in the file, and someone wants to use the
       output intent our handling of the output intent profile is
       different depending upon if someone specified a particular
       output profile */
    switch (icc_profile->num_comps) {
        case 1:
            if (strncmp(icc_profile->name, DEFAULT_GRAY_ICC,
                        strlen(icc_profile->name)) == 0) {
                icc_profile->default_match = DEFAULT_GRAY;
            }
            break;
        case 3:
            if (strncmp(icc_profile->name, DEFAULT_RGB_ICC,
                        strlen(icc_profile->name)) == 0) {
                icc_profile->default_match = DEFAULT_RGB;
            }
            break;
        case 4:
            if (strncmp(icc_profile->name, DEFAULT_CMYK_ICC,
                        strlen(icc_profile->name)) == 0) {
                icc_profile->default_match = DEFAULT_CMYK;
            }
            break;
    }

    if_debug1m(gs_debug_flag_icc, mem, "[icc] Profile data CS is %d\n",
               icc_profile->data_cs);

    /* This is slightly silly, we have a device method for 'get_profile' we really ought to
     * have one for 'set_profile' as well. In its absence, make sure we are setting the profile
     * of the bottom level device.
     */
    while(pdev->child)
        pdev = pdev->child;

    switch (pro_enum)
    {
        case gsDEFAULTPROFILE:
        case gsGRAPHICPROFILE:
        case gsIMAGEPROFILE:
        case gsTEXTPROFILE:
            if_debug1m(gs_debug_flag_icc, mem,
                       "[icc] Setting device profile %d\n", pro_enum);
            pdev->icc_struct->device_profile[pro_enum] = icc_profile;
            break;
        case gsPROOFPROFILE:
            if_debug0m(gs_debug_flag_icc, mem, "[icc] Setting proof profile\n");
            pdev->icc_struct->proof_profile = icc_profile;
            break;
        case gsLINKPROFILE:
            if_debug0m(gs_debug_flag_icc, mem, "[icc] Setting link profile\n");
            pdev->icc_struct->link_profile = icc_profile;
            break;
        case gsPRPROFILE:
            if_debug0m(gs_debug_flag_icc, mem, "[icc] Setting postrender profile\n");
            pdev->icc_struct->postren_profile = icc_profile;
            break;
        case gsBLENDPROFILE:
            if_debug0m(gs_debug_flag_icc, mem, "[icc] Setting blend profile\n");
            pdev->icc_struct->blend_profile = icc_profile;
            break;
        default:
        case gsOIPROFILE:
            /* This never happens as output intent profile is set in zicc.c */
            rc_decrement(icc_profile, "gsicc_set_device_profile");
            return_error(gs_error_unknownerror);
    }

    /* Check that everything is OK with regard to the number of
       components. */
    if (gsicc_verify_device_profiles(pdev) < 0)
        return gs_rethrow(-1, "Error in device profiles");

    if (icc_profile->num_comps != 1 &&
        icc_profile->num_comps != 3 &&
        icc_profile->num_comps != 4) {
        /* NCLR Profile.  Set up default colorant names */
        code = gsicc_set_device_profile_colorants(pdev, NULL);
        if (code < 0)
            return code;
    }

    return 0;
}

/* Set the icc profile in the gs_color_space object */
int
gsicc_set_gscs_profile(gs_color_space *pcs, cmm_profile_t *icc_profile,
                       gs_memory_t * mem)
{
    if (pcs == NULL)
        return -1;
#if ICC_DUMP
    if (icc_profile->buffer) {
        dump_icc_buffer(mem,
                        icc_profile->buffer_size, "set_gscs",
                        icc_profile->buffer);
        global_icc_index++;
    }
#endif

    gsicc_adjust_profile_rc(icc_profile, 1, "gsicc_set_gscs_profile");
    if (pcs->cmm_icc_profile_data != NULL) {
        /* There is already a profile set there */
        /* free it and then set to the new one.  */
        /* should we check the hash code and retain if the same
           or place this job on the caller?  */
        gsicc_adjust_profile_rc(pcs->cmm_icc_profile_data, -1, "gsicc_set_gscs_profile");
    }
    pcs->cmm_icc_profile_data = icc_profile;
    return 0;
}

int
gsicc_clone_profile(cmm_profile_t *source, cmm_profile_t **destination,
                    gs_memory_t *memory)
{
    cmm_profile_t *des = gsicc_profile_new(NULL, memory, source->name,
        source->name_length);

    if (des == NULL)
        return gs_throw(gs_error_VMerror, "Profile clone failed");
    des->buffer = gs_alloc_bytes(memory, source->buffer_size, "gsicc_clone_profile");
    if (des->buffer == NULL) {
        gsicc_adjust_profile_rc(des, -1, "gsicc_clone_profile");
        return gs_throw(gs_error_VMerror, "Profile clone failed");
    }
    memcpy(des->buffer, source->buffer, source->buffer_size);
    des->buffer_size = source->buffer_size;
    gsicc_init_profile_info(des);
    *destination = des;
    return 0;
}

cmm_profile_t *
gsicc_profile_new(stream *s, gs_memory_t *memory, const char* pname,
                  int namelen)
{
    cmm_profile_t *result;
    int code;
    char *nameptr = NULL;
    gs_memory_t *mem_nongc = memory->non_gc_memory;

    result = (cmm_profile_t*) gs_alloc_bytes(mem_nongc, sizeof(cmm_profile_t),
                                    "gsicc_profile_new");
    if (result == NULL)
        return result;
    memset(result, 0, GSICC_SERIALIZED_SIZE);
    if (namelen > 0) {
        nameptr = (char*) gs_alloc_bytes(mem_nongc, namelen+1,
                             "gsicc_profile_new");
        if (nameptr == NULL) {
            gs_free_object(mem_nongc, result, "gsicc_profile_new");
            return NULL;
        }
        memcpy(nameptr, pname, namelen);
        nameptr[namelen] = '\0';
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
            gs_free_object(mem_nongc, nameptr, "gsicc_profile_new");
            return NULL;
        }
    } else {
        result->buffer = NULL;
        result->buffer_size = 0;
    }
    rc_init_free(result, mem_nongc, 1, rc_free_icc_profile);
    result->profile_handle = NULL;
    result->spotnames = NULL;
    result->rend_is_valid = false;
    result->isdevlink = false;  /* only used for srcgtag profiles */
    result->dev = NULL;
    result->memory = mem_nongc;
    result->vers = ICCVERS_UNKNOWN;
    result->v2_data = NULL;
    result->v2_size = 0;
    result->release = gscms_release_profile; /* Default case */

    result->lock = gx_monitor_label(gx_monitor_alloc(mem_nongc),
                                    "gsicc_manage");
    if (result->lock == NULL) {
        gs_free_object(mem_nongc, result->buffer, "gsicc_load_profile");
        gs_free_object(mem_nongc, result, "gsicc_profile_new");
        gs_free_object(mem_nongc, nameptr, "gsicc_profile_new");
        return NULL;
    }
    if_debug1m(gs_debug_flag_icc, mem_nongc,
               "[icc] allocating ICC profile = "PRI_INTPTR"\n", (intptr_t)result);
    return result;
}

static void
rc_free_icc_profile(gs_memory_t * mem, void *ptr_in, client_name_t cname)
{
    cmm_profile_t *profile = (cmm_profile_t *)ptr_in;
    gs_memory_t *mem_nongc =  profile->memory;

    if_debug2m(gs_debug_flag_icc, mem,
               "[icc] rc decrement profile = "PRI_INTPTR" rc = %ld\n",
               (intptr_t)ptr_in, profile->rc.ref_count);
    if (profile->rc.ref_count <= 1 ) {
        /* Clear out the buffer if it is full */
        if (profile->buffer != NULL) {
            gs_free_object(mem_nongc, profile->buffer, "rc_free_icc_profile(buffer)");
            profile->buffer = NULL;
        }
        if_debug0m(gs_debug_flag_icc, mem, "[icc] profile freed\n");
        /* Release this handle if it has been set */
        if (profile->profile_handle != NULL) {
            profile->release(profile->profile_handle, profile->memory);
            profile->profile_handle = NULL;
        }
        /* Release the name if it has been set */
        if (profile->name != NULL) {
            gs_free_object(mem_nongc, profile->name,"rc_free_icc_profile(name)");
            profile->name = NULL;
            profile->name_length = 0;
        }
        profile->hash_is_valid = 0;
        if (profile->lock != NULL) {
            gx_monitor_free(profile->lock);
            profile->lock = NULL;
        }
        /* If we had a DeviceN profile with names deallocate that now */
        if (profile->spotnames != NULL) {
            /* Free the linked list in this object */
            gsicc_free_spotnames(profile->spotnames, mem_nongc);
            /* Free the main object */
            gs_free_object(mem_nongc, profile->spotnames, "rc_free_icc_profile(spotnames)");
        }
        /* If we allocated a buffer to hold the v2 profile then free that */
        if (profile->v2_data != NULL) {
            gs_free_object(mem_nongc, profile->v2_data, "rc_free_icc_profile(v2_data)");
        }
        gs_free_object(mem_nongc, profile, "rc_free_icc_profile");
    }
}

/* We are just starting up.  We need to set the initial color space in the
   graphic state at this time */
int
gsicc_init_gs_colors(gs_gstate *pgs)
{
    int             code = 0;
    gs_color_space  *cs_old;
    gs_color_space  *cs_new;
    int k;

    if (pgs->in_cachedevice)
        return_error(gs_error_undefined);

    for (k = 0; k < 2; k++) {
        /* First do color space 0 */
        cs_old = pgs->color[k].color_space;
        cs_new = gs_cspace_new_DeviceGray(pgs->memory);
        if (cs_new == NULL)
            return_error(gs_error_VMerror);
        rc_increment_cs(cs_new);
        pgs->color[k].color_space = cs_new;
        if ( (code = cs_new->type->install_cspace(cs_new, pgs)) < 0 ) {
            pgs->color[k].color_space = cs_old;
            rc_decrement_only_cs(cs_new, "gsicc_init_gs_colors");
            return code;
        } else {
            rc_decrement_only_cs(cs_old, "gsicc_init_gs_colors");
        }
    }
    return code;
}

/* Only set those that have not already been set. */
int
gsicc_init_iccmanager(gs_gstate * pgs)
{
    int code = 0, k;
    const char *pname;
    int namelen;
    gsicc_manager_t *iccmanager = pgs->icc_manager;
    cmm_profile_t *profile;

    for (k = 0; k < 4; k++) {
        pname = default_profile_params[k].path;
        namelen = strlen(pname);

        switch(default_profile_params[k].default_type) {
            case DEFAULT_GRAY:
                profile = iccmanager->default_gray;
                break;
            case DEFAULT_RGB:
                profile = iccmanager->default_rgb;
                break;
            case DEFAULT_CMYK:
                 profile = iccmanager->default_cmyk;
                 break;
            default:
                profile = NULL;
        }
        if (profile == NULL)
            code = gsicc_set_profile(iccmanager, pname, namelen,
                                     default_profile_params[k].default_type);
        if (code < 0)
            return gs_rethrow(code, "cannot find default icc profile");
    }
#if CREATE_V2_DATA
    /* Test bed for V2 creation from V4 */
    for (int j = 2; j < 3; j++)
    {
        byte *data;
        int size;

        switch (default_profile_params[j].default_type) {
        case DEFAULT_GRAY:
            profile = iccmanager->default_gray;
            break;
        case DEFAULT_RGB:
            profile = iccmanager->default_rgb;
            break;
        case DEFAULT_CMYK:
            profile = iccmanager->default_cmyk;
            break;
        default:
            profile = NULL;
        }
        gsicc_initialize_default_profile(profile);
        data = gsicc_create_getv2buffer(pgs, profile, &size);
    }
#endif
    return 0;
}

static void
gsicc_manager_finalize(const gs_memory_t *memory, void * vptr)
{
    gsicc_manager_t *icc_man = (gsicc_manager_t *)vptr;

    gsicc_manager_free_contents(icc_man, "gsicc_manager_finalize");
}

gsicc_manager_t *
gsicc_manager_new(gs_memory_t *memory)
{
    gsicc_manager_t *result;

    /* Allocated in stable gc memory.  This done since the profiles
       may be introduced late in the process. */
    memory = memory->stable_memory;
    result = gs_alloc_struct(memory, gsicc_manager_t, &st_gsicc_manager,
                             "gsicc_manager_new");
    if ( result == NULL )
        return NULL;
    rc_init_free(result, memory, 1, rc_gsicc_manager_free);
    result->default_gray = NULL;
    result->default_rgb = NULL;
    result->default_cmyk = NULL;
    result->lab_profile = NULL;
    result->xyz_profile = NULL;
    result->graytok_profile = NULL;
    result->device_named = NULL;
    result->device_n = NULL;
    result->smask_profiles = NULL;
    result->memory = memory;
    result->srcgtag_profile = NULL;
    result->override_internal = false;
    return result;
}

static void gsicc_manager_free_contents(gsicc_manager_t *icc_manager,
                                        client_name_t cname)
{
    int k;
    gsicc_devicen_entry_t *device_n, *device_n_next;

    gsicc_adjust_profile_rc(icc_manager->default_cmyk, -1, "gsicc_manager_free_contents");
    gsicc_adjust_profile_rc(icc_manager->default_gray, -1, "gsicc_manager_free_contents");
    gsicc_adjust_profile_rc(icc_manager->default_rgb, -1, "gsicc_manager_free_contents");
    gsicc_adjust_profile_rc(icc_manager->device_named, -1, "gsicc_manager_free_contents");
    gsicc_adjust_profile_rc(icc_manager->lab_profile, -1, "gsicc_manager_free_contents");
    gsicc_adjust_profile_rc(icc_manager->graytok_profile, -1, "gsicc_manager_free_contents");
    rc_decrement(icc_manager->srcgtag_profile, "gsicc_manager_free_contents");

    /* Loop through the DeviceN profiles */
    if ( icc_manager->device_n != NULL) {
        device_n = icc_manager->device_n->head;
        for ( k = 0; k < icc_manager->device_n->count; k++) {
            gsicc_adjust_profile_rc(device_n->iccprofile, -1, "gsicc_manager_free_contents");
            device_n_next = device_n->next;
            gs_free_object(icc_manager->memory, device_n, "gsicc_manager_free_contents");
            device_n = device_n_next;
        }
        gs_free_object(icc_manager->memory, icc_manager->device_n,
                       "gsicc_manager_free_contents");
    }

    /* The soft mask profiles */
    if (icc_manager->smask_profiles != NULL) {
        gs_free_object(icc_manager->smask_profiles->memory, icc_manager->smask_profiles, "gsicc_manager_free_contents");
        icc_manager->smask_profiles = NULL;
    }
}

static void
rc_gsicc_manager_free(gs_memory_t * mem, void *ptr_in, client_name_t cname)
{
    /* Ending the manager.  Decrement the ref counts of the profiles
       and then free the structure */
    gsicc_manager_t *icc_manager = (gsicc_manager_t * ) ptr_in;

    assert(mem == icc_manager->memory);

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
    if (code < 0)
        return code;
    /* Get the size from doing a seek to the end and then a rewind instead
       of relying upon the profile size indicated in the header */
    code = sfseek(s,0,SEEK_END);
    if (code < 0)
        return code;
    profile_size = sftell(s);
    code = srewind(s);
    if (code < 0)
        return code;
    if (profile_size < ICC_HEADER_SIZE)
        return_error(gs_error_VMerror);
    /* Allocate the buffer, stuff with the profile */
   buffer_ptr = gs_alloc_bytes(memory, profile_size,
                                        "gsicc_load_profile");
   if (buffer_ptr == NULL)
        return gs_throw(gs_error_VMerror, "Insufficient memory for profile buffer");
   num_bytes = sfread(buffer_ptr,sizeof(unsigned char),profile_size,s);
   if( num_bytes != profile_size) {
       gs_free_object(memory, buffer_ptr, "gsicc_load_profile");
       return -1;
   }
   profile->buffer = buffer_ptr;
   profile->buffer_size = num_bytes;
   return 0;
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
    if (code < 0)
        return code;
    code = sfseek(s,0,SEEK_END);
    if (code < 0)
        return code;
    profile_size = sftell(s);
    code = srewind(s);
    if (code < 0)
        return code;
    /* Allocate the buffer, stuff with the profile */
    buffer_ptr = gs_alloc_bytes(memory->non_gc_memory, profile_size,
                                "gsicc_load_profile");
    if (buffer_ptr == NULL)
        return gs_throw(gs_error_VMerror, "Insufficient memory for profile buffer");
    num_bytes = sfread(buffer_ptr,sizeof(unsigned char),profile_size,s);
    if( num_bytes != profile_size) {
        gs_free_object(memory->non_gc_memory, buffer_ptr, "gsicc_load_profile");
        return -1;
    }
    profile->buffer = buffer_ptr;
    profile->buffer_size = num_bytes;
    return 0;
}

/* Check if the embedded profile is the same as any of the default profiles */
static void
gsicc_set_default_cs_value(cmm_profile_t *picc_profile, gs_gstate *pgs)
{
    gsicc_manager_t *icc_manager = pgs->icc_manager;
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
gsicc_init_hash_cs(cmm_profile_t *picc_profile, gs_gstate *pgs)
{
    if ( !(picc_profile->hash_is_valid) ) {
        gsicc_get_icc_buff_hash(picc_profile->buffer, &(picc_profile->hashcode),
                                picc_profile->buffer_size);
        picc_profile->hash_is_valid = true;
    }
    gsicc_set_default_cs_value(picc_profile, pgs);
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
    gsicc_serialized_profile_t profile_header;
    int k;

    if( pcrdev != NULL) {

        /* Check ICC table for hash code and get the whole size icc raw buffer
           plus serialized header information */
        position = gsicc_search_icc_table(pcrdev->icc_table,
                                          picc_profile->hashcode, &size);
        if ( position < 0 )
            return NULL;  /* Not found. */

        /* Get the ICC buffer.  We really want to avoid this transfer.
           I need to write  an interface to the CMM to do this through
           the clist ioprocs */
        /* Allocate the buffer */
        profile_size = size - GSICC_SERIALIZED_SIZE;
        /* Profile and its members are ALL in non-gc memory */
        buffer_ptr = gs_alloc_bytes(memory->non_gc_memory, profile_size,
                                            "gsicc_get_profile_handle_clist");
        if (buffer_ptr == NULL)
            return NULL;
        clist_read_chunk(pcrdev, position + GSICC_SERIALIZED_SIZE,
            profile_size, (unsigned char *) buffer_ptr);
        profile_handle = gscms_get_profile_handle_mem(buffer_ptr, profile_size, memory->non_gc_memory);
        if (profile_handle == NULL) {
            gs_free_object(memory->non_gc_memory, buffer_ptr, "gsicc_get_profile_handle_clist");
            return NULL;
        }
        /* We also need to get some of the serialized information */
        clist_read_chunk(pcrdev, position, GSICC_SERIALIZED_SIZE,
                        (unsigned char *) (&profile_header));
        picc_profile->buffer = NULL;
        picc_profile->buffer_size = 0;
        picc_profile->data_cs = profile_header.data_cs;
        picc_profile->default_match = profile_header.default_match;
        picc_profile->hash_is_valid = profile_header.hash_is_valid;
        picc_profile->hashcode = profile_header.hashcode;
        picc_profile->islab = profile_header.islab;
        picc_profile->num_comps = profile_header.num_comps;
        picc_profile->rend_is_valid = profile_header.rend_is_valid;
        picc_profile->rend_cond = profile_header.rend_cond;
        picc_profile->isdevlink = profile_header.isdevlink;
        for ( k = 0; k < profile_header.num_comps; k++ ) {
            picc_profile->Range.ranges[k].rmax =
                profile_header.Range.ranges[k].rmax;
            picc_profile->Range.ranges[k].rmin =
                profile_header.Range.ranges[k].rmin;
        }
        gs_free_object(memory->non_gc_memory, buffer_ptr, "gsicc_get_profile_handle_clist");
        return profile_handle;
     }
     return NULL;
}

gcmmhprofile_t
gsicc_get_profile_handle_buffer(unsigned char *buffer, int profile_size, gs_memory_t *memory)
{

    gcmmhprofile_t profile_handle = NULL;

     if( buffer != NULL) {
         if (profile_size < ICC_HEADER_SIZE) {
             return 0;
         }
         profile_handle = gscms_get_profile_handle_mem(buffer, profile_size, memory->non_gc_memory);
         return profile_handle;
     }
     return 0;
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
     int code = 0;
     bool islab;

     if (profile != NULL )
        return profile;
     /* else, return the default types */
     switch( color_space_index ) {
        case gs_color_space_index_DeviceGray:
            return icc_manager->default_gray;
            break;
        case gs_color_space_index_DeviceRGB:
            return icc_manager->default_rgb;
            break;
        case gs_color_space_index_DeviceCMYK:
            return icc_manager->default_cmyk;
            break;
            /* Only used in 3x types */
        case gs_color_space_index_DevicePixel:
            return 0;
            break;
       case gs_color_space_index_DeviceN:
            /* If we made it to here, then we will need to use the
               alternate colorspace */
            return 0;
            break;
       case gs_color_space_index_CIEDEFG:
           /* For now just use default CMYK to avoid segfault.  MJV to fix */
           gs_colorspace->cmm_icc_profile_data = icc_manager->default_cmyk;
           gsicc_adjust_profile_rc(icc_manager->default_cmyk, 1, "gsicc_get_gscs_profile");
           return gs_colorspace->cmm_icc_profile_data;
           /* Need to convert to an ICC form */
           break;
        case gs_color_space_index_CIEDEF:
           /* For now just use default RGB to avoid segfault.  MJV to fix */
           gs_colorspace->cmm_icc_profile_data = icc_manager->default_rgb;
           gsicc_adjust_profile_rc(icc_manager->default_rgb, 1, "gsicc_get_gscs_profile");
           return gs_colorspace->cmm_icc_profile_data;
           /* Need to convert to an ICC form */
           break;
        case gs_color_space_index_CIEABC:
            gs_colorspace->cmm_icc_profile_data =
                gsicc_profile_new(NULL, icc_manager->memory, NULL, 0);
            if (gs_colorspace->cmm_icc_profile_data == NULL) {
                gs_throw(gs_error_VMerror, "Creation of ICC profile for CIEABC failed");
                return NULL;
            }
            code =
                gsicc_create_fromabc(gs_colorspace,
                        &(gs_colorspace->cmm_icc_profile_data->buffer),
                        &(gs_colorspace->cmm_icc_profile_data->buffer_size),
                        icc_manager->memory,
                        &(gs_colorspace->params.abc->caches.DecodeABC.caches[0]),
                        &(gs_colorspace->params.abc->common.caches.DecodeLMN[0]),
                        &islab);
            if (code < 0) {
                gs_warn("Failed to create ICC profile from CIEABC");
                gsicc_adjust_profile_rc(gs_colorspace->cmm_icc_profile_data, -1,
                             "gsicc_get_gscs_profile");
                return NULL;
            }

            if (islab) {
                /* Destroy the profile */
                gsicc_adjust_profile_rc(gs_colorspace->cmm_icc_profile_data, -1,
                             "gsicc_get_gscs_profile");
                /* This may be an issue for pdfwrite */
                return icc_manager->lab_profile;
            }
            gs_colorspace->cmm_icc_profile_data->default_match = CIE_ABC;
            return gs_colorspace->cmm_icc_profile_data;
            break;
        case gs_color_space_index_CIEA:
            gs_colorspace->cmm_icc_profile_data =
                gsicc_profile_new(NULL, icc_manager->memory, NULL, 0);
            if (gs_colorspace->cmm_icc_profile_data == NULL) {
                gs_throw(gs_error_VMerror, "Creation of ICC profile for CIEA failed");
                return NULL;
            }
            code =
                gsicc_create_froma(gs_colorspace,
                            &(gs_colorspace->cmm_icc_profile_data->buffer),
                            &(gs_colorspace->cmm_icc_profile_data->buffer_size),
                            icc_manager->memory,
                            &(gs_colorspace->params.a->caches.DecodeA),
                            &(gs_colorspace->params.a->common.caches.DecodeLMN[0]));
            gs_colorspace->cmm_icc_profile_data->default_match = CIE_A;
            return gs_colorspace->cmm_icc_profile_data;
            break;
        case gs_color_space_index_Separation:
            /* Caller should use named color path */
            return 0;
            break;
        case gs_color_space_index_Pattern:
        case gs_color_space_index_Indexed:
            /* Caller should use the base space for these */
            return 0;
            break;
        case gs_color_space_index_ICC:
            /* This should not occur, as the space
               should have had a populated profile handle */
            return 0;
            break;
     }
    return 0;
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
            return curr_entry->serial_data.file_position;
        }
        curr_entry = curr_entry->next;
    }

    /* Did not find it! */
    *size = 0;
    return -1;
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
        return NULL;

    /* Check ICC table for hash code and get the whole size icc raw buffer
       plus serialized header information. Make sure the icc_table has
       been intialized */
    if (pcrdev->icc_table == NULL) {
        code = clist_read_icctable(pcrdev);
        if (code<0)
            return NULL;
    }
    position = gsicc_search_icc_table(pcrdev->icc_table, icc_hashcode, &size);
    if ( position < 0 )
        return NULL;

    /* Get the serialized portion of the ICC profile information */
    clist_read_chunk(pcrdev, position, GSICC_SERIALIZED_SIZE,
                    (unsigned char *) profile);
    return profile;
}

void
gsicc_profile_serialize(gsicc_serialized_profile_t *profile_data,
                        cmm_profile_t *icc_profile)
{
    if (icc_profile == NULL)
        return;
    memcpy(profile_data, icc_profile, GSICC_SERIALIZED_SIZE);
}

/* Utility functions */

int
gsicc_getsrc_channel_count(cmm_profile_t *icc_profile)
{
    return gscms_get_input_channel_count(icc_profile->profile_handle,
        icc_profile->memory);
}

void
gsicc_extract_profile(gs_graphics_type_tag_t graphics_type_tag,
                       cmm_dev_profile_t *profile_struct,
                       cmm_profile_t **profile, gsicc_rendering_param_t *render_cond)
{
    switch (graphics_type_tag & ~GS_DEVICE_ENCODES_TAGS) {
        case GS_UNKNOWN_TAG:
        case GS_UNTOUCHED_TAG:
        default:
            (*profile) = profile_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE];
            *render_cond = profile_struct->rendercond[GS_DEFAULT_DEVICE_PROFILE];
            break;
        case GS_VECTOR_TAG:
            *render_cond = profile_struct->rendercond[GS_VECTOR_DEVICE_PROFILE];
            if (profile_struct->device_profile[GS_VECTOR_DEVICE_PROFILE] != NULL) {
                (*profile) = profile_struct->device_profile[GS_VECTOR_DEVICE_PROFILE];
            } else {
                (*profile) = profile_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE];
            }
            break;
        case GS_IMAGE_TAG:
            *render_cond = profile_struct->rendercond[GS_IMAGE_DEVICE_PROFILE];
            if (profile_struct->device_profile[GS_IMAGE_DEVICE_PROFILE] != NULL) {
                (*profile) = profile_struct->device_profile[GS_IMAGE_DEVICE_PROFILE];
            } else {
                (*profile) = profile_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE];
            }
            break;
        case GS_TEXT_TAG:
            *render_cond = profile_struct->rendercond[GS_TEXT_DEVICE_PROFILE];
            if (profile_struct->device_profile[GS_TEXT_DEVICE_PROFILE] != NULL) {
                (*profile) = profile_struct->device_profile[GS_TEXT_DEVICE_PROFILE];
            } else {
                (*profile) = profile_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE];
            }
            break;
        }
}

/* internal ICC and rendering intent override control */
void
gs_setoverrideicc(gs_gstate *pgs, bool value)
{
    if (pgs->icc_manager != NULL) {
        pgs->icc_manager->override_internal = value;
    }
}
bool
gs_currentoverrideicc(const gs_gstate *pgs)
{
    if (pgs->icc_manager != NULL) {
        return pgs->icc_manager->override_internal;
    } else {
        return false;
    }
}

void
gsicc_setrange_lab(cmm_profile_t *profile)
{
    profile->Range.ranges[0].rmin = 0.0;
    profile->Range.ranges[0].rmax = 100.0;
    profile->Range.ranges[1].rmin = -128.0;
    profile->Range.ranges[1].rmax = 127.0;
    profile->Range.ranges[2].rmin = -128.0;
    profile->Range.ranges[2].rmax = 127.0;
}

#if ICC_DUMP
/* Debug dump of ICC buffer data */
static void
dump_icc_buffer(const gs_memory_t *mem, int buffersize, char filename[],byte *Buffer)
{
    char full_file_name[50];
    gp_file *fid;

    gs_snprintf(full_file_name,sizeof(full_file_name),"%d)%s_debug.icc",global_icc_index,filename);
    fid = gp_fopen(mem, full_file_name,"wb");
    gp_fwrite(Buffer,sizeof(unsigned char),buffersize,fid);
    gp_fclose(fid);
}
#endif

/* The following are for setting the system/user params */
/* No default for the deviceN profile. */
void
gs_currentdevicenicc(const gs_gstate * pgs, gs_param_string * pval)
{
    static const char *const rfs = "";

    /*FIXME: This should return the entire list !!! */
    /*       Just return the first one for now      */
    if (pgs->icc_manager->device_n == NULL) {
        pval->data = (const byte *) rfs;
        pval->persistent = true;
    } else {
        pval->data =
            (const byte *) (pgs->icc_manager->device_n->head->iccprofile->name);
        pval->persistent = false;
    }
    pval->size = strlen((const char *)pval->data);
}

int
gs_setdevicenprofileicc(const gs_gstate * pgs, gs_param_string * pval)
{
    int code = 0;
    char *pname, *pstr, *pstrend, *last = NULL;
    int namelen = (pval->size)+1;
    gs_memory_t *mem = pgs->memory;

    /* Check if it was "NULL" */
    if (pval->size != 0) {
        /* The DeviceN name can have multiple files
           in it.  This way we can define all the
           DeviceN color spaces with ICC profiles.
           divide using , and ; delimeters as well as
           remove leading and ending spaces (file names
           can have internal spaces). */
        pname = (char *)gs_alloc_bytes(mem, namelen,
                                     "set_devicen_profile_icc");
        if (pname == NULL)
            return_error(gs_error_VMerror);
        memcpy(pname,pval->data,namelen-1);
        pname[namelen-1] = 0;
        pstr = gs_strtok(pname, ",;", &last);
        while (pstr != NULL) {
            namelen = strlen(pstr);
            /* Remove leading and trailing spaces from the name */
            while ( namelen > 0 && pstr[0] == 0x20) {
                pstr++;
                namelen--;
            }
            namelen = strlen(pstr);
            pstrend = &(pstr[namelen-1]);
            while ( namelen > 0 && pstrend[0] == 0x20) {
                pstrend--;
                namelen--;
            }
            code = gsicc_set_profile(pgs->icc_manager, (const char*) pstr, namelen, DEVICEN_TYPE);
            if (code < 0)
                return gs_throw(code, "cannot find devicen icc profile");
            pstr = gs_strtok(NULL, ",;", &last);
        }
        gs_free_object(mem, pname,
        "set_devicen_profile_icc");
        return code;
    }
    return 0;
}

void
gs_currentdefaultgrayicc(const gs_gstate * pgs, gs_param_string * pval)
{
    static const char *const rfs = DEFAULT_GRAY_ICC;

    if (pgs->icc_manager->default_gray == NULL) {
        pval->data = (const byte *) rfs;
        pval->persistent = true;
    } else {
        pval->data = (const byte *) (pgs->icc_manager->default_gray->name);
        pval->persistent = false;
    }
    pval->size = strlen((const char *)pval->data);
}

int
gs_setdefaultgrayicc(const gs_gstate * pgs, gs_param_string * pval)
{
    int code;
    char *pname;
    int namelen = (pval->size)+1;
    gs_memory_t *mem = pgs->memory;
    bool not_initialized;

    /* Detect if this is our first time in here.  If so, then we need to
       reset up the default gray color spaces that are in the graphic state
       to be ICC based.  It was not possible to do it until after we get
       the profile */
    not_initialized = (pgs->icc_manager->default_gray == NULL);

    pname = (char *)gs_alloc_bytes(mem, namelen,
                             "set_default_gray_icc");
    if (pname == NULL)
        return_error(gs_error_VMerror);
    memcpy(pname,pval->data,namelen-1);
    pname[namelen-1] = 0;
    code = gsicc_set_profile(pgs->icc_manager,
        (const char*) pname, namelen-1, DEFAULT_GRAY);
    gs_free_object(mem, pname,
        "set_default_gray_icc");
    if (code < 0)
        return gs_throw(code, "cannot find default gray icc profile");
    /* if this is our first time in here then we need to properly install the
       color spaces that were initialized in the graphic state at this time */
    if (not_initialized) {
        code = gsicc_init_gs_colors((gs_gstate*) pgs);
    }
    if (code < 0)
        return gs_throw(code, "error initializing gstate color spaces to icc");
    return code;
}

void
gs_currenticcdirectory(const gs_gstate * pgs, gs_param_string * pval)
{
    static const char *const rfs = DEFAULT_DIR_ICC;   /* as good as any other */
    const gs_lib_ctx_t *lib_ctx = pgs->memory->gs_lib_ctx;

    if (lib_ctx->profiledir == NULL) {
        pval->data = (const byte *)rfs;
        pval->size = strlen(rfs);
        pval->persistent = true;
    } else {
        pval->data = (const byte *)(lib_ctx->profiledir);
        pval->size = lib_ctx->profiledir_len;
        pval->persistent = false;
    }
}

int
gs_seticcdirectory(const gs_gstate * pgs, gs_param_string * pval)
{
    char *pname;
    int namelen = (pval->size)+1;
    gs_memory_t *mem = (gs_memory_t *)pgs->memory;

    /* Check if it was "NULL" */
    if (pval->size != 0 ) {
        pname = (char *)gs_alloc_bytes(mem, namelen,
                                       "gs_seticcdirectory");
        if (pname == NULL)
            return gs_rethrow(gs_error_VMerror, "cannot allocate directory name");
        memcpy(pname,pval->data,namelen-1);
        pname[namelen-1] = 0;
        if (gs_lib_ctx_set_icc_directory(mem, (const char*) pname, namelen-1) < 0) {
            gs_free_object(mem, pname, "gs_seticcdirectory");
            return -1;
        }
        gs_free_object(mem, pname, "gs_seticcdirectory");
    }
    return 0;
}

void
gs_currentsrcgtagicc(const gs_gstate * pgs, gs_param_string * pval)
{
    if (pgs->icc_manager->srcgtag_profile == NULL) {
        pval->data = NULL;
        pval->size = 0;
        pval->persistent = true;
    } else {
        pval->data = (byte *)pgs->icc_manager->srcgtag_profile->name;
        pval->size = pgs->icc_manager->srcgtag_profile->name_length;
        pval->persistent = false;
    }
}

int
gs_setsrcgtagicc(const gs_gstate * pgs, gs_param_string * pval)
{
    int code;
    char *pname;
    int namelen = (pval->size)+1;
    gs_memory_t *mem = pgs->memory;

    if (pval->size == 0) return 0;
    pname = (char *)gs_alloc_bytes(mem, namelen, "set_srcgtag_icc");
    if (pname == NULL)
        return_error(gs_error_VMerror);
    memcpy(pname,pval->data,namelen-1);
    pname[namelen-1] = 0;
    code = gsicc_set_srcgtag_struct(pgs->icc_manager, (const char*) pname,
                                   namelen);
    gs_free_object(mem, pname, "set_srcgtag_icc");
    if (code < 0)
        return gs_rethrow(code, "cannot find srctag file");
    return code;
}

void
gs_currentdefaultrgbicc(const gs_gstate * pgs, gs_param_string * pval)
{
    static const char *const rfs = DEFAULT_RGB_ICC;

    if (pgs->icc_manager->default_rgb == NULL) {
        pval->data = (const byte *) rfs;
        pval->persistent = true;
    } else {
        pval->data = (const byte *) (pgs->icc_manager->default_rgb->name);
        pval->persistent = false;
    }
    pval->size = strlen((const char *)pval->data);
}

int
gs_setdefaultrgbicc(const gs_gstate * pgs, gs_param_string * pval)
{
    int code;
    char *pname;
    int namelen = (pval->size)+1;
    gs_memory_t *mem = pgs->memory;

    pname = (char *)gs_alloc_bytes(mem, namelen,
                             "set_default_rgb_icc");
    if (pname == NULL)
        return_error(gs_error_VMerror);
    memcpy(pname,pval->data,namelen-1);
    pname[namelen-1] = 0;
    code = gsicc_set_profile(pgs->icc_manager,
        (const char*) pname, namelen-1, DEFAULT_RGB);
    gs_free_object(mem, pname,
        "set_default_rgb_icc");
    if (code < 0)
        return gs_rethrow(code, "cannot find default rgb icc profile");
    return code;
}

void
gs_currentnamedicc(const gs_gstate * pgs, gs_param_string * pval)
{
    static const char *const rfs = "";

    if (pgs->icc_manager->device_named == NULL) {
        pval->data = (const byte *) rfs;
        pval->persistent = true;
    } else {
        pval->data = (const byte *) (pgs->icc_manager->device_named->name);
        pval->persistent = false;
    }
    pval->size = strlen((const char *)pval->data);
}

int
gs_setnamedprofileicc(const gs_gstate * pgs, gs_param_string * pval)
{
    int code;
    char* pname;
    int namelen = (pval->size)+1;
    gs_memory_t *mem = pgs->memory;

    /* Check if it was "NULL" */
    if (pval->size != 0) {
        pname = (char *)gs_alloc_bytes(mem, namelen,
                                 "set_named_profile_icc");
        if (pname == NULL)
            return_error(gs_error_VMerror);
        memcpy(pname,pval->data,namelen-1);
        pname[namelen-1] = 0;
        code = gsicc_set_profile(pgs->icc_manager,
            (const char*) pname, namelen-1, NAMED_TYPE);
        gs_free_object(mem, pname,
                "set_named_profile_icc");
        if (code < 0)
            return gs_rethrow(code, "cannot find named color icc profile");
        return code;
    }
    return 0;
}

void
gs_currentdefaultcmykicc(const gs_gstate * pgs, gs_param_string * pval)
{
    static const char *const rfs = DEFAULT_CMYK_ICC;

    if (pgs->icc_manager->default_cmyk == NULL) {
        pval->data = (const byte *) rfs;
        pval->persistent = true;
    } else {
        pval->data = (const byte *) (pgs->icc_manager->default_cmyk->name);
        pval->persistent = false;
    }
    pval->size = strlen((const char *)pval->data);
}

int
gs_setdefaultcmykicc(const gs_gstate * pgs, gs_param_string * pval)
{
    int code;
    char* pname;
    int namelen = (pval->size)+1;
    gs_memory_t *mem = pgs->memory;

    pname = (char *)gs_alloc_bytes(mem, namelen,
                             "set_default_cmyk_icc");
    if (pname == NULL)
        return_error(gs_error_VMerror);
    memcpy(pname,pval->data,namelen-1);
    pname[namelen-1] = 0;
    code = gsicc_set_profile(pgs->icc_manager,
        (const char*) pname, namelen-1, DEFAULT_CMYK);
    gs_free_object(mem, pname,
                "set_default_cmyk_icc");
    if (code < 0)
        return gs_throw(code, "cannot find default cmyk icc profile");
    return code;
}

void
gs_currentlabicc(const gs_gstate * pgs, gs_param_string * pval)
{
    static const char *const rfs = LAB_ICC;

    pval->data = (const byte *)( (pgs->icc_manager->lab_profile == NULL) ?
                        rfs : pgs->icc_manager->lab_profile->name);
    pval->size = strlen((const char *)pval->data);
    pval->persistent = true;
}

int
gs_setlabicc(const gs_gstate * pgs, gs_param_string * pval)
{
    int code;
    char* pname;
    int namelen = (pval->size)+1;
    gs_memory_t *mem = pgs->memory;

    pname = (char *)gs_alloc_bytes(mem, namelen,
                             "set_lab_icc");
    if (pname == NULL)
        return_error(gs_error_VMerror);
    memcpy(pname,pval->data,namelen-1);
    pname[namelen-1] = 0;
    code = gsicc_set_profile(pgs->icc_manager,
        (const char*) pname, namelen-1, LAB_TYPE);
    gs_free_object(mem, pname,
                "set_lab_icc");
    if (code < 0)
        return gs_throw(code, "cannot find default lab icc profile");
    return code;
}
