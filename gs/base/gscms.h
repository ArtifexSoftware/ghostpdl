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
    gsicc_colorbuffer_t buffercolor;

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
    DEFAULT_CMYK
} gsicc_devicecolor_t;

/* A structure that is a member variable of the gs color space.
   See 

/* A structure for holding profile information.  A member variable
   of the ghostscript color structure. */
typedef struct cmm_profile_s {

    void *profile_handle;                          /* The profile handle to be used in linking */
    unsigned char *buffer;                         /* A buffer with ICC profile content */
    int64_t hashcode;                               /* A hash code for the icc profile */
    bool hash_is_valid;

} cmm_profile_t;

#ifndef cmm_profile_DEFINED
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

typedef struct gsiic_link_s gsicc_link_t;

typedef struct gsicc_hashlink_s {

    int64_t link_hashcode;  
    int64_t src_hash;
    int64_t des_hash;
    int64_t rend_hash;

} gsicc_hashlink_t;

typedef struct gsiic_link_s {

    void *link_handle;
    void *contextptr;
    gsicc_hashlink_t hashcode;
    int ref_count;
    gsicc_link_t *nextlink;
    gsicc_link_t *prevlink;
    bool includes_softproof;
    bool is_identity;  /* Used for noting that this is an identity profile */

} gsicc_link_t;


/* ICC Cache. The size of the cache is limited
   by max_memory_size.  Links are added if there
   is sufficient memory.  If not, then */

typedef struct gsicc_link_cache_s {

    gsicc_link_t *icc_link;
    int num_links;

} gsicc_link_cache_t;

/* Tha manager object */

typedef struct gsicc_manager_s {

    cmm_profile_t *device_profile;  /* The actual profile for the device */
    cmm_profile_t *device_named;    /* The named color profile for the device */
    cmm_profile_t *default_gray;    /* Default gray profile for device gray */
    cmm_profile_t *default_rgb;     /* Default RGB profile for device RGB */
    cmm_profile_t *default_cmyk;    /* Default CMYK profile for device CMKY */
    cmm_profile_t *proof_profile;   /* Profiling profile */
    cmm_profile_t *output_link;     /* Output device Link profile */

} gsicc_manager_t;

typedef struct gsicc_rendering_param_s {

    gsicc_rendering_intents_t rendering_intent;
    gs_object_tag_type_t    object_type;
    gsicc_black_point_comp_t black_point_comp;    

} gsicc_rendering_param_t;




#endif

