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


/*  Data type definitions when using the gscms  */


#ifndef gscms_INCLUDED
#  define gscms_INCLUDED

#include "std.h"
#include "stdpre.h"
#include "gstypes.h"
#include "gsutil.h"       /* Need for the object types */
#include "stdint_.h"

/* Define the preferred size of the output by the CMS */
/* This can be different than the size of gx_color_value 
   which can range between 8 and 16.  Here we can only
   have 8 or 16 bits */

typedef unsigned short icc_output_type;

#define icc_byte_count sizeof(icc_output_type)  
#define icc_bits_count (icc_byte_count * 8)
#define icc_max_color_value ((gx_color_value)((1L << icc_bits_count) - 1))
#define icc_value_to_byte(cv)\
  ((cv) >> (icc_bits_count - 8))
#define icc_value_from_byte(cb)\
  (((cb) << (icc_bits_count - 8)) + ((cb) >> (16 - icc_bits_count)))

typedef struct gs_range15_s {
    gs_range_t ranges[15];
} gs_range15_t;  /* ICC profile input could be up to 15 bands */


/*  The buffer description.  We
    may need to handle a variety
    of different types */

typedef enum {
    gsGRAY,
    gsRGB,
    gsCMYK,
    gsNCHANNEL,
    gsCIEXYZ,
    gsCIELAB
} gsicc_colorbuffer_t;

typedef struct gsicc_bufferdesc_s {

    unsigned char num_chan;
    unsigned char bytes_per_chan;
    bool has_alpha;
    bool alpha_first;
    bool little_endian;
    bool is_planar;
    int plane_stride;
    int row_stride;
    int num_rows;
    int pixels_per_row; 

} gsicc_bufferdesc_t;


/* Enumerate the ICC rendering intents */
typedef enum {
    gsPERCEPTUAL,
    gsRELATIVECOLORIMETRIC,
    gsSATURATION,
    gsABSOLUTECOLORIMETRIC
} gsicc_rendering_intents_t;

/*  Doing this an an enum type for now.
    There is alot going on with respect
    to this and V2 versus V4 profiles */

typedef enum {
    BP_ON,
    BP_OFF,
} gsicc_black_point_comp_t;

/* Used so that we can specify
    if we want to link with
    Device input color spaces 
    during the link creation
    process. For the DeviceN
    case, the DeviceN profile
    must match the DeviceN
    profile in colorant
    order and number of 
    colorants.  */

typedef enum {
    DEFAULT_GRAY,
    DEFAULT_RGB,
    DEFAULT_CMYK,
    PROOF_TYPE,
    NAMED_TYPE,
    LINKED_TYPE,
    LAB_TYPE

} gsicc_profile_t;


/* A structure for holding profile information.  A member variable
   of the ghostscript color structure.   The item is reference counted. */
struct cmm_profile_s {

    void *profile_handle;       /* The profile handle to be used in linking */
    unsigned char num_comps;    /* number of device dependent values */
    bool islab;                 /* Needed since we want to detect this to avoid 
                                   expensive decode on LAB images.  Is true
                                   if PDF color space is \Lab*/
    gs_range15_t Range;          
    byte *buffer;               /* A buffer with ICC profile content */
    int buffer_size;            /* size of ICC profile buffer */
    int64_t hashcode;           /* A hash code for the icc profile */
    bool hash_is_valid;         /* Is the code valid? */
    rc_header rc;               /* Reference count.  So we know when to free */ 
    int name_length;            /* Length of file name */
    char *name;                 /* Name of file name (if there is one) where profile is found.
                                   If it was embedded in the stream, there will not be a file
                                   name.  This is primarily here for the system profiles, and
                                   so that we avoid resetting them everytime the user params
                                   are reloaded. */
};

#ifndef cmm_profile_DEFINED
typedef struct cmm_profile_s cmm_profile_t;
#define cmm_profile_DEFINED
#endif


/*  These are the types that we can
    potentially have linked together
    by the CMS.  If the CMS does
    not have understanding of PS
    color space types, then we 
    will need to convert them to 
    an ICC type. */


typedef enum {
    DEVICETYPE,
    ICCTYPE,
    CRDTYPE,
    CIEATYPE,
    CIEABCTYPE,
    CIEDEFTYPE,
    CIEDEFGTYPE
} gs_colortype_t;

/* The link object. */

typedef struct gsicc_link_s gsicc_link_t;

typedef struct gsicc_hashlink_s {

    int64_t link_hashcode;  
    int64_t src_hash;
    int64_t des_hash;
    int64_t rend_hash;

} gsicc_hashlink_t;

struct gsicc_link_s {

    void *link_handle;
    void *contextptr;
    gsicc_hashlink_t hashcode;
    int ref_count;
    gsicc_link_t *nextlink;
    gsicc_link_t *prevlink;
    bool includes_softproof;
    bool is_identity;  /* Used for noting that this is an identity profile */

};


/* ICC Cache. The size of the cache is limited
   by max_memory_size.  Links are added if there
   is sufficient memory.  */

typedef struct gsicc_link_cache_s {

    gsicc_link_t *icc_link;
    int num_links;
    rc_header rc;
    gs_memory_t *memory;

} gsicc_link_cache_t;

/* Tha manager object */

typedef struct gsicc_manager_s {

    cmm_profile_t *device_named;    /* The named color profile for the device */
    cmm_profile_t *default_gray;    /* Default gray profile for device gray */
    cmm_profile_t *default_rgb;     /* Default RGB profile for device RGB */
    cmm_profile_t *default_cmyk;    /* Default CMYK profile for device CMKY */
    cmm_profile_t *proof_profile;   /* Profiling profile */
    cmm_profile_t *output_link;     /* Output device Link profile */
    cmm_profile_t *device_profile;  /* The actual profile for the device */
    cmm_profile_t *lab_profile;      /* Colorspace type ICC profile from LAB to LAB */

    char *profiledir;               /* Directory used in searching for ICC profiles */
    uint namelen;

    gs_memory_t *memory;            
    rc_header rc;

} gsicc_manager_t;

typedef struct gsicc_rendering_param_s {

    gsicc_rendering_intents_t rendering_intent;
    gs_object_tag_type_t    object_type;
    gsicc_black_point_comp_t black_point_comp;    

} gsicc_rendering_param_t;




#endif

