/* Copyright (C) 2001-2023 Artifex Software, Inc.
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

/*  Data type definitions when using the gscms  */

#ifndef gscms_INCLUDED
#  define gscms_INCLUDED

#include "std.h"
#include "gstypes.h"
#include "gscspace.h"      /* for gs_color_space */
#include "gsdevice.h"     /* Need to carry pointer to clist reader */
#include "gxsync.h"       /* for monitors */
#include "stdint_.h"

#define ICC_MAX_CHANNELS 15
#define NUM_DEVICE_PROFILES 4
#define NUM_SOURCE_PROFILES 3
#define GS_DEFAULT_DEVICE_PROFILE 0
#define GS_VECTOR_DEVICE_PROFILE 1
#define GS_IMAGE_DEVICE_PROFILE 2
#define GS_TEXT_DEVICE_PROFILE 3

#define AB_NEUTRAL_8 5
#define AB_NEUTRAL_16 5

#define DEV_NEUTRAL_8 5
#define DEV_NEUTRAL_16 5

#define ARTIFEX_sRGB_HASH 0xfbea006420fca6be

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

/* This object is used only for device post processing CM.  It and its objects
   must be allocated in non-gc memory. */

typedef struct gsicc_device_cm_s {
    cmm_profile_t *gray_profile;
    cmm_profile_t *rgb_profile;
    cmm_profile_t *cmyk_profile;
    cmm_profile_t *device_link_profile;
    gs_memory_t *memory;
} gsicc_device_cm_t;

/*  The buffer description.  We handle a variety of different types */
typedef enum {
    gsUNDEFINED = 0,
    gsGRAY,
    gsRGB,
    gsCMYK,
    gsNCHANNEL,
    gsCIEXYZ,
    gsCIELAB,
    gsNAMED
} gsicc_colorbuffer_t;

typedef struct gsicc_bufferdesc_s {
    unsigned char num_chan;
    unsigned char bytes_per_chan;
    bool has_alpha;
    bool alpha_first;
    bool endian_swap;
    bool is_planar;
    int plane_stride;
    int row_stride;
    int num_rows;
    int pixels_per_row;
} gsicc_bufferdesc_t;

/* Mapping procedures to allow easy vectoring depending upon if we are using
   the CMM or doing "dumb" color transforms */
typedef int (*gscms_trans_color_proc_t) (gx_device * dev, gsicc_link_t *icclink,
                                         void *inputcolor, void *outputcolor,
                                         int num_bytes);

typedef int (*gscms_trans_buffer_proc_t) (gx_device * dev, gsicc_link_t *icclink,
                                          gsicc_bufferdesc_t *input_buff_desc,
                                          gsicc_bufferdesc_t *output_buff_desc,
                                          void *inputbuffer, void *outputbuffer);

typedef void (*gscms_link_free_proc_t) (gsicc_link_t *icclink);

typedef bool (*gscms_monitor_proc_t) (void *inputcolor, int num_bytes);

typedef struct gscms_procs_s {
    gscms_trans_buffer_proc_t map_buffer;
    gscms_trans_color_proc_t  map_color;
    gscms_link_free_proc_t free_link;
    gscms_monitor_proc_t is_color;
} gscms_procs_t;

/* Allow different methods for releasing the opaque profile contents */
typedef void(*gscms_free_profile_proc_t) (void *profile_handle, gs_memory_t *memory);

/* Enumerate the ICC rendering intents and other parameters.  A note on
   these.  0-3 are for different values.   4-7 are for Override cases
   where we are trying to override some value specified in the document.
   8 is reserved for not specified.  This is used in the case were we
   can specify multiple rendering values for the sources and wish to just
   use default or document values for a particular value.  For example,
   we may want for RGB Graphics to specify a Rendering Intent but not
   a black point compensation. */
typedef enum {
    gsPERCEPTUAL = 0,
    gsRELATIVECOLORIMETRIC,
    gsSATURATION,
    gsABSOLUTECOLORIMETRIC,
    gsPERCEPTUAL_OR,            /* These are needed for keeping track  */
    gsRELATIVECOLORIMETRIC_OR,  /* of when the source ri is going to */
    gsSATURATION_OR,            /* override the destination profile intent */
    gsABSOLUTECOLORIMETRIC_OR,  /* in particular through the clist */
    gsRINOTSPECIFIED = 8,       /* Used to ignore value when source based setting */

    /* Stop modern C shrinking this enum to a byte */
    gsicc_rendering_intent__FORCE_SIZE= 0x10000
} gsicc_rendering_intents_t;

/* We make an enumerated type in case someone wants to add different types
   of black point compensation.  Like lcms provides the option for. If
   any are added, be sure to add in the regular and the source overide
   option. Also not that we have at most 4 options due to gsBP_OVERRIDE  */
typedef enum {
    gsBLACKPTCOMP_OFF = 0,
    gsBLACKPTCOMP_ON,
    gsBLACKPTCOMP_OFF_OR = 4, /* These are needed for keeping track of the  */
    gsBLACKPTCOMP_ON_OR,      /* source blackpt is to overide dest. setting */
    gsBPNOTSPECIFIED = 8,    /* Used to ignore value when source based setting */

    /* Stop modern C shrinking this enum to a byte */
    gsicc_blackptcomp__FORCE_SIZE= 0x10000
} gsicc_blackptcomp_t;

/* This is used mainly for when the sourcegtag option specifies us to use no
   color management or a replacement color management CMM */
typedef enum {
    gsCMM_DEFAULT = 0,
    gsCMM_NONE,
    gsCMM_REPLACE
} gsicc_cmm_t;

/* Since this is not specified by the source document we don't need to worry
   about override values */
typedef enum {
    gsBLACKPRESERVE_OFF = 0,
    gsBLACKPRESERVE_KONLY,
    gsBLACKPRESERVE_KPLANE,
    gsBKPRESNOTSPECIFIED = 8,   /* Used to ignore value when source based setting */

    /* Stop modern C shrinking this enum to a byte */
    gsicc_blackpreserve__FORCE_SIZE= 0x10000
} gsicc_blackpreserve_t;

#define gsRI_OVERRIDE 0x4
#define gsBP_OVERRIDE 0x4
#define gsKP_OVERRIDE 0x4
#define gsRI_MASK 0x3;
#define gsBP_MASK 0x3;
#define gsKP_MASK 0x3;

/* Enumerate the types of profiles */
typedef enum {
    gsDEFAULTPROFILE = 0,
    gsGRAPHICPROFILE,
    gsIMAGEPROFILE,
    gsTEXTPROFILE,
    gsPROOFPROFILE,
    gsLINKPROFILE,
    gsOIPROFILE,
    gsPRPROFILE,
    gsBLENDPROFILE
} gsicc_profile_types_t;

typedef enum {
    gsSRC_GRAPPRO = 0,
    gsSRC_IMAGPRO,
    gsSRC_TEXTPRO,
} gsicc_profile_srctypes_t;

/* --------------- graphical object tags ------------ */

/* The default is "unknown" which has value 0 and by default devices don't encode tags */
typedef enum {
    GS_UNTOUCHED_TAG = 0x0,	/* UNTOUCHED *must* be 0 -- transparency code relies on this */
    GS_TEXT_TAG = 0x1,
    GS_IMAGE_TAG = 0x2,
    GS_VECTOR_TAG = 0x4,
    GS_UNKNOWN_TAG = 0x40,
    GS_DEVICE_ENCODES_TAGS = 0x80
} gs_graphics_type_tag_t;

/* Source profile graphic tag rendering conditions */
typedef struct gsicc_rendering_param_s {
    gsicc_rendering_intents_t rendering_intent;   /* Standard rendering intent */
    gsicc_blackptcomp_t black_point_comp;         /* Black point compensation */
    gsicc_blackpreserve_t preserve_black;         /* preserve K plane in CMYK2CMYK */
    gs_graphics_type_tag_t graphics_type_tag;     /* Some CMM may want this */
    gsicc_cmm_t cmm;                              /* which cmm? (used only with sourcetag) */
    bool override_icc;                            /* Override source ICC (used only with sourcetag) */
} gsicc_rendering_param_t;

/* Source profiles for different objects.  only CMYK and RGB */
typedef struct cmm_srcgtag_profile_s {
    cmm_profile_t  *rgb_profiles[NUM_SOURCE_PROFILES];
    gsicc_rendering_param_t rgb_rend_cond[NUM_SOURCE_PROFILES];
    cmm_profile_t  *cmyk_profiles[NUM_SOURCE_PROFILES];
    gsicc_rendering_param_t cmyk_rend_cond[NUM_SOURCE_PROFILES];
    cmm_profile_t  *gray_profiles[NUM_SOURCE_PROFILES];
    gsicc_rendering_param_t gray_rend_cond[NUM_SOURCE_PROFILES];
    cmm_profile_t  *color_warp_profile;
    gs_memory_t *memory;
    int name_length;            /* Length of file name */
    char *name;                 /* Name of file name where this is found */
    rc_header rc;
} cmm_srcgtag_profile_t;

typedef struct gsicc_colorname_s gsicc_colorname_t;

struct gsicc_colorname_s {
    char *name;
    int length;
    gsicc_colorname_t *next;
};

typedef struct gsicc_namelist_s gsicc_namelist_t;

typedef struct gs_devicen_color_map_s gs_devicen_color_map;

struct gsicc_namelist_s {
    int count;
    gsicc_colorname_t *head;
    char *name_str;
    gs_devicen_color_map *color_map;
    bool equiv_cmyk_set;   /* So that we make sure the equiv cmyk values are set */
                           /* This can't be done at the time this structure
                              is set up since we need the device and the graphic
                              state for this, but instead is done when we
                              do our first mapping */
};

/* Overprint (overprint_control)
 *    /enable	is the default
 *    /disable	turns off overprint
 *    /simulate	performs overprint simulation for all devices
 */
typedef enum {
    gs_overprint_control_enable = 0,	/* Overprint for CMYK devices (default) */
    gs_overprint_control_disable = 1,	/* No overprint for any devices */
    gs_overprint_control_simulate = 2	/* Simulate overprint for RGB and Gray devices */
} gs_overprint_control_t;
#define gs_overprint_control_names\
        "enable", "disable", "simulate"

/* Destination profiles for different objects */
struct cmm_dev_profile_s {
        cmm_profile_t  *device_profile[NUM_DEVICE_PROFILES];
        cmm_profile_t  *proof_profile;
        cmm_profile_t  *link_profile;
        cmm_profile_t  *oi_profile;  /* output intent profile */
        cmm_profile_t  *blend_profile; /* blending color space */
        cmm_profile_t  *postren_profile;  /* Profile for use by devices post render */
        gsicc_rendering_param_t rendercond[NUM_DEVICE_PROFILES];
        bool devicegraytok;        /* Used for forcing gray to pure black */
        bool graydetection;        /* Device param for monitoring for gray only page */
        bool pageneutralcolor;      /* Only valid if graydetection true */
        bool usefastcolor;         /* Used when we want to use no cm */
        bool blacktext;           /* Force text to be pure black */
        bool blackvector;         /* Force vectors to be pure black */
        float blackthresholdL;    /* Luminance threshold */
        float blackthresholdC;    /* Chrominance threshold */
        bool supports_devn;        /* If the target handles devn colors */
        gs_overprint_control_t overprint_control;	/* enable is the default */
        gsicc_namelist_t *spotnames;  /* If our device profiles are devn */
        bool prebandthreshold;     /* Used to indicate use of HT pre-clist */
        gs_memory_t *memory;
        rc_header rc;
};

/*  Used so that we can specify if we want to link with Device input color spaces
    during the link creation process. For the DeviceN case, the DeviceN profile
    must match the DeviceN profile in colorant order and number of colorants.
    Also, used to indicate if the profile matches one of the default profiles in
    the icc manager.  This is useful for reducing clist size since we will encode
    this value instead of the ICC profile.
*/
typedef enum {
    DEFAULT_NONE,   /* A profile that was actually embedded in a doc */
    DEFAULT_GRAY,   /* The default DeviceGray profile */
    DEFAULT_RGB,    /* The default DeviceRGB profile */
    DEFAULT_CMYK,   /* The default DeviceCMYK profile */
    NAMED_TYPE,     /* The named color profile */
    LAB_TYPE,       /* The CIELAB profile */
    DEVICEN_TYPE,   /* A special device N profile */
    DEFAULT_GRAY_s, /* Same as default but a source profile from document */
    DEFAULT_RGB_s,  /* Same as default but a source profile from document */
    DEFAULT_CMYK_s, /* Same as default but a source profile from document */
    LAB_TYPE_s,     /* Same as our default CIELAB but a source profile from doc */
    CAL_GRAY,       /* Generated from PDF cal gray object */
    CAL_RGB,        /* Generated from PDF cal rgb object */
    CIE_A,          /* Generated from PS CIEA definition */
    CIE_ABC,        /* Generated from PS CIEABC definition */
    CIE_DEF,        /* Generated from PS CIEDEF definition */
    CIE_DEFG,       /* Generated from PS CIEDEFG definition */
    CIE_CRD        /* Generated from PS CRD definition */
} gsicc_profile_t;

typedef enum {
    ICCVERS_UNKNOWN,
    ICCVERS_2,
    ICCVERS_NOT2
} gsicc_version_t;

#define gsicc_serial_data\
    unsigned char num_comps;		/* number of device dependent values */\
    unsigned char num_comps_out;	/* usually 3 but could be more if device link type */\
    bool islab;				/* Needed since we want to detect this to avoid */\
                                        /*  expensive decode on LAB images.  Is true */\
                                        /* if PDF color space is \Lab */\
    bool isdevlink;                  /* is this a device link profile */\
    gsicc_profile_t default_match;	/* Used for detecting a match to a default space */\
    gsicc_colorbuffer_t data_cs;	/* The data color space of the profile (not the PCS) */\
    gs_range_icc_t Range;\
    int64_t hashcode;			/* A hash code for the icc profile */\
    bool hash_is_valid;			/* Is the code valid? */\
    int devicen_permute[ICC_MAX_CHANNELS];	/* Permutation vector for deviceN laydown order */\
    bool devicen_permute_needed;		/* Check if we need to permute the DeviceN values */\
    int buffer_size;		/* size of ICC profile buffer */\
    bool rend_is_valid;                 /* Needed for cond/profile coupling during */\
    gsicc_rendering_param_t rend_cond   /* clist playback when rendering images */

/* A subset of the profile information which is used when writing and reading
 * out to the c-list
 */
typedef struct gsicc_serialized_profile_s {
    gsicc_serial_data;
} gsicc_serialized_profile_t;

/* A structure for holding profile information.  A member variable
 * of the ghostscript color structure.   The item is reference counted.
 */
struct cmm_profile_s {
    gsicc_serial_data;
    byte *buffer;                       /* A buffer with ICC profile content */
    gx_device *dev;                     /* A pointer to the clist device in which the ICC data may be contained */
    gsicc_namelist_t *spotnames;        /* Used for profiles that have non-standard colorants */
    void *profile_handle;               /* The profile handle */
    rc_header rc;                       /* Reference count.  So we know when to free */
    int name_length;                    /* Length of file name */
    char *name;                         /* Name of file name (if there is one) where profile is found.
                                         * If it was embedded in the stream, there will not be a file
                                         * name.  This is primarily here for the system profiles, and
                                         * so that we avoid resetting them everytime the user params
                                         * are reloaded. */
    gsicc_version_t vers;               /* Is this profile V2 */
    byte *v2_data;                      /* V2 data that is equivalent to this profile. Used for PDF-A1 support */
    int v2_size;                        /* Number of bytes in v2_data */
    gs_memory_t *memory;                /* In case we have some in non-gc and some in gc memory */
    gx_monitor_t *lock;                 /* handle for the monitor */
    gscms_free_profile_proc_t release;  /* Release the profile handle at CMM */
};

/* The above definition is plagued with an offset issue.  Probably should
   do away with gsicc_serialized_profile_t type */
#ifndef offsetof
#define offsetof(st, member) \
   ((char *)&(((st *)(NULL))->member) - (char *)(NULL))
#endif
#define GSICC_SERIALIZED_SIZE offsetof(cmm_profile_t, buffer)

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
    gsicc_profile_entry_t *next;     /* next CS */
    uint64_t key;                    /* Key based off dictionary location */
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

typedef struct gsicc_hashlink_s {
    int64_t link_hashcode;
    int64_t src_hash;
    int64_t des_hash;
    int64_t rend_hash;
} gsicc_hashlink_t;

struct gsicc_link_s {
    void *link_handle;		/* the CMS decides what this is */
    gs_memory_t *memory;
    gscms_procs_t procs;
    gsicc_hashlink_t hashcode;
    struct gsicc_link_cache_s *icc_link_cache;
    int ref_count;
    gsicc_link_t *next;
    gx_monitor_t *lock;		/* lock used while changing contents */
    bool includes_softproof;
    bool includes_devlink;
    bool is_identity;  /* Used for noting that this is an identity profile */
    bool valid;		/* true once link is completely built and usable */
    bool is_monitored;
    gscms_procs_t orig_procs;  /* procs to use after monitoring */
    gsicc_colorbuffer_t data_cs; /* needed for begin_monitor after end_monitor */
    int num_input;  /* Need so we can monitor properly */
    int num_output; /* Need so we can monitor properly */
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
    bool cache_full;		/* flag that some thread needs a cache slot */
    gx_semaphore_t *full_wait;	/* semaphore for waiting when the cache is full */
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

/* Had to add bool so that we know if things were swapped.
   The reason is that if we are in a swapped state and
   there is a vmreclaim we then end up sending the user
   params again and we will find that there is a mismatch */
typedef struct gsicc_smask_s {
    cmm_profile_t *smask_gray;
    cmm_profile_t *smask_rgb;
    cmm_profile_t *smask_cmyk;
    gs_memory_t *memory;
    bool swapped;
} gsicc_smask_t;

/* The manager object */

typedef struct gsicc_manager_s {
    cmm_profile_t *device_named;    /* The named color profile for the device */
    cmm_profile_t *default_gray;    /* Default gray profile for device gray */
    cmm_profile_t *default_rgb;     /* Default RGB profile for device RGB */
    cmm_profile_t *default_cmyk;    /* Default CMYK profile for device CMKY */
    cmm_profile_t *lab_profile;     /* Colorspace type ICC profile from LAB to LAB */
    cmm_profile_t *xyz_profile;     /* RGB based proflie that hands back CIEXYZ values */
    cmm_profile_t *graytok_profile; /* A specialized profile for mapping gray to K */
    gsicc_devicen_t *device_n;      /* A linked list of profiles used for DeviceN support */
    gsicc_smask_t *smask_profiles;  /* Profiles used when we are in a softmask group */
    bool override_internal;         /* Set via the user params */
    cmm_srcgtag_profile_t *srcgtag_profile;
    gs_memory_t *memory;
    rc_header rc;
} gsicc_manager_t;

#endif /* ifndef gscms_INCLUDED */
