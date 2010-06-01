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
#include "gsdevice.h"     /* Need to carry pointer to clist reader */
#include "gxsync.h"       /* for semaphore and monitors */
#include "stdint_.h"

#define ICC_MAX_CHANNELS 15

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

typedef struct gs_range_icc_s {
    gs_range_t ranges[ICC_MAX_CHANNELS];
} gs_range_icc_t;  /* ICC profile input could be up to 15 bands */

/*  The buffer description.  We handle a variety of different types */
typedef enum {
    gsGRAY,
    gsRGB,
    gsCMYK,
    gsNCHANNEL,
    gsCIEXYZ,
    gsCIELAB,
    gsNAMED,
    gsUNDEFINED
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

/*  Doing this an an enum type for now.  There is alot going on with respect
 *  to this and V2 versus V4 profiles
 */

typedef enum {
    BP_ON,
    BP_OFF,
} gsicc_black_point_comp_t;

/*  Used so that we can specify if we want to link with Device input color spaces 
    during the link creation process. For the DeviceN case, the DeviceN profile
    must match the DeviceN profile in colorant order and number of colorants.
    Also, used to indicate if the profile matches one of the default profiles in
    the icc manager.  This is useful for reducing clist size since we will encode
    this value instead of the ICC profile.
*/

typedef enum {
    DEFAULT_NONE,
    DEFAULT_GRAY,
    DEFAULT_RGB,
    DEFAULT_CMYK,
    PROOF_TYPE,
    NAMED_TYPE,
    LINKED_TYPE,
    LAB_TYPE,
    DEVICEN_TYPE
} gsicc_profile_t;

#define gsicc_serial_data\
    unsigned char num_comps;		/* number of device dependent values */\
    unsigned char num_comps_out;	/* usually 3 but could be more if device link type */\
    bool islab;				/* Needed since we want to detect this to avoid */\
					/*  expensive decode on LAB images.  Is true */\
					/* if PDF color space is \Lab */\
    gsicc_profile_t default_match;	/* Used for detecting a match to a default space */\
    gsicc_colorbuffer_t data_cs;	/* The data color space of the profile (not the PCS) */\
    gs_range_icc_t Range;\
    int64_t hashcode;			/* A hash code for the icc profile */\
    bool hash_is_valid;			/* Is the code valid? */\
    int devicen_permute[ICC_MAX_CHANNELS];	/* Permutation vector for deviceN laydown order */\
    bool devicen_permute_needed;		/* Check if we need to permute the DeviceN values */\
    int buffer_size			/* size of ICC profile buffer */
    
/* A subset of the profile information which is used when writing and reading
 * out to the c-list
 */
typedef struct gsicc_serialized_profile_s {
    gsicc_serial_data;
} gsicc_serialized_profile_t;

typedef struct gsicc_colorname_s gsicc_colorname_t;

struct gsicc_colorname_s {
    char *name;
    int length;
    gsicc_colorname_t *next;
};

typedef struct gsicc_namelist_s gsicc_namelist_t;

struct gsicc_namelist_s {
    int count;
    gsicc_colorname_t *head;
};

/* A structure for holding profile information.  A member variable
 * of the ghostscript color structure.   The item is reference counted.
 */
struct cmm_profile_s {
    gsicc_serial_data;
    byte *buffer;               /* A buffer with ICC profile content */
    gx_device *dev;             /* A pointer to the clist device in which the ICC data may be contained */
    gsicc_namelist_t *spotnames;  /* Only used with NCLR ICC input profiles with named color tag */
    void *profile_handle;       /* The profile handle */
    rc_header rc;               /* Reference count.  So we know when to free */ 
    int name_length;            /* Length of file name */
    char *name;                 /* Name of file name (if there is one) where profile is found.
				 * If it was embedded in the stream, there will not be a file
				 * name.  This is primarily here for the system profiles, and
				 * so that we avoid resetting them everytime the user params
				 * are reloaded. */
};

#ifndef cmm_profile_DEFINED
typedef struct cmm_profile_s cmm_profile_t;
#define cmm_profile_DEFINED
#endif

/* A linked list structure for storing profiles in a table in which we
   can store and refer to from the clist and also when creating icc profiles
   from ps object.  Right now it is not clear to me if we really need a 
   cache in the traditional sense or a list since I believe the number of entries will
   in general be very small (i.e. there will not be at MOST more than 2 to 3 internal
   ICC profiles in a file).  The default GRAY, RGB, and CMYK profiles are not
   stored here but are maintained in the ICC manager.  This is for profiles
   that are in the content and for profiles we generate from PS and PDF CIE (NonICC)
   color spaces. 
 */
typedef struct gsicc_profile_entry_s gsicc_profile_entry_t;

struct gsicc_profile_entry_s {
    gs_color_space *color_space;     /* The color space with the profile */
    gsicc_profile_entry_t *next;    /* next CS */
    int64_t key;                    /* Key based off dictionary location */
};

/* ProfileList. The size of the list is limited by max_memory_size.
   Profiles are added if there is sufficient memory. */
typedef struct gsicc_profile_cache_s {
    gsicc_profile_entry_t *head;
    int num_entries;
    rc_header rc;
    gs_memory_t *memory;
} gsicc_profile_cache_t;

/*  These are the types that we can potentially have linked together by the CMS.
 *  If the CMS does not have understanding of PS color space types, then we 
 *  will need to convert them to an ICC type. */
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
    struct gsicc_link_cache_s *icc_link_cache;
    int ref_count;
    gsicc_link_t *next;
    gx_semaphore_t *wait;		/* semaphore used by waiting threads */
    int num_waiting;
    bool includes_softproof;
    bool is_identity;  /* Used for noting that this is an identity profile */
    bool valid;		/* true once link is completely built and usable */
};


/* ICC Cache. The size of the cache is limited by max_memory_size.
 * Links are added if there is sufficient memory and if the number
 * of links does not exceed a (soft) limit.
 */

typedef struct gsicc_link_cache_s {
    gsicc_link_t *head;
    int num_links;
    rc_header rc;
    gs_memory_t *memory;
    gx_monitor_t *lock;		/* handle for the monitor */
    gx_semaphore_t *wait;	/* somebody needs a link cache slot */
    int num_waiting;		/* number of threads waiting */
} gsicc_link_cache_t;

/* A linked list structure to keep DeviceN ICC profiles
 * that the user wishes to use to achieve accurate rendering
 * with DeviceN (typically non CMYK or CMYK + spot) colors.
 * The ICC profiles used for this will require a special
 * private tag in the ICC profile that defines the colorant
 * names and they must match those in the DeviceN color
 * space.  Note this is not to say that DeviceN color
 * management can only be achieved with ICC profiles.  If
 * a customer has a proprietary mixing model for inks, they
 * will be able to hook in their method in the location
 * in the code where the DeviceN colors are processed.  If
 * there is no ICC color management of the DeviceN colors
 * and the DeviceN colors are NOT the native colors
 * for the device, then the colors will be transformed to 
 * the alternate CS using the alternate tint transform
 */

typedef struct gsicc_devicen_entry_s gsicc_devicen_entry_t;

struct gsicc_devicen_entry_s {
    cmm_profile_t *iccprofile;
    gsicc_devicen_entry_t *next;
};


typedef struct gsicc_devicen_s gsicc_devicen_t;

struct gsicc_devicen_s {
    gsicc_devicen_entry_t *head;
    gsicc_devicen_entry_t *final;
    int count;
};

typedef struct gsicc_smask_s {
    cmm_profile_t *smask_gray;
    cmm_profile_t *smask_rgb;
    cmm_profile_t *smask_cmyk;
} gsicc_smask_t;

/* The manager object */

typedef struct gsicc_manager_s {
    cmm_profile_t *device_named;    /* The named color profile for the device */
    cmm_profile_t *default_gray;    /* Default gray profile for device gray */
    cmm_profile_t *default_rgb;     /* Default RGB profile for device RGB */
    cmm_profile_t *default_cmyk;    /* Default CMYK profile for device CMKY */
    cmm_profile_t *proof_profile;   /* Proofing profile */
    cmm_profile_t *output_link;     /* Output device Link profile */
    cmm_profile_t *device_profile;  /* The actual profile for the device */
    cmm_profile_t *lab_profile;     /* Colorspace type ICC profile from LAB to LAB */
    gsicc_devicen_t *device_n;      /* A linked list of profiles used for DeviceN support */ 
    gsicc_smask_t *smask_profiles;  /* Profiles used when we are in a softmask group */
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

#endif /* ifndef gscms_INCLUDED */
