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


/*  Header for the ICC Manager */


#ifndef gsiccmanage_INCLUDED
#  define gsiccmanage_INCLUDED

#include "stdint_.h"
#include "scommon.h"
#include "gscdefs.h"
#include "gsstruct.h"
#include "gsutil.h"         /* Need for the object types */
#include "gsiccmanage.h"
#include "gscolor2.h"       /* Need for CRD support */
#include "gscspace.h"       /* Need for PS and PDF CIE color space support */
#include "icc.h"
  
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
} gs_icc_colorbuffer_t;

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
    gs_icc_colorbuffer_t buffercolor;

} gsicc_bufferdesc_t;


/* Enumerate the ICC rendering intents */
typedef enum {
    gsPERCEPTUAL,
    gsRELATIVECOLORIMETRIC,
    gsSATURATION,
    gsABSOLUTECOLORIMETRIC
} gs_icc_rendering_intents_t;

/*  Doing this an an enum type for now.
    There is alot going on with respect
    to this and V2 versus V4 profiles */

typedef enum {
    BP_ON,
    BP_OFF,
} gs_icc_black_point_comp_t;

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
    DEVICE_GRAY,
    DEVICE_RGB,
    DEVICE_CMYK,
    DEVICE_N,
    NONDEVICE
} gs_icc_devicecolor_t;

/* A structure for holding ICC profile info */
typedef struct gsicc_profile_s {

    void *ProfileRawBuf;      /* A raw buffer of ICC profile data */
    int ProfileHashCode;    /* A hash code for the icc profile */

    /* Pull out the header, since it has useful stuff for us */

    icHeader iccheader;

} gsicc_profile_t;



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

typedef struct gsiccmanage_colorspace_s {

    gs_colortype_t ColorType;
    gs_icc_devicecolor_t DeviceType;
    gsicc_profile_t *ProfileData;
    gs_cie_render *pcrd;
    gs_cie_a *pcie_a;
    gs_cie_abc *pcie_abc;
    gs_cie_def *pcie_def;
    gs_cie_defg *pcie_defg;   

} gsicc_colorspace_t;






/* The link object. */


typedef struct gsiic_link_s gsicc_link_t;

typedef struct gsiic_link_s {

    void *LinkHandle;
    void *ContextPtr;
    int64_t LinkHashCode;
    int ref_count;
    gsicc_link_t *NextLink;
    gsicc_link_t *PrevLink;
    bool includes_softproof;

} gsicc_link_t;


/* ICC Cache. The size of the cache is limited
   by max_memory_size.  Links are added if there
   is sufficient memory.  If not, then */

typedef struct gsicc_link_cache_s {

    gsicc_link_t *ICCLink;
    int num_links;

} gsicc_link_cache_t;

/* Tha manager object */

typedef struct gsicc_manager_s {

    gsicc_profile_t DeviceProfile;  /* The actual profile for the device */
    gsicc_profile_t DeviceNamed;    /* The named color profile for the device */
    gsicc_profile_t LABProfile;     /* CIELAB to CIELAB profile */
    gsicc_profile_t DefaultGray;    /* Default gray profile for device gray */
    gsicc_profile_t DefaultRGB;     /* Default RGB profile for device RGB */
    gsicc_profile_t DefaultCMYK;    /* Default CMYK profile for device CMKY */

    gsicc_profile_t ProofProfile;   /* Profiling profile */
    gsicc_profile_t OutputLink;     /* Output device Link profile */


} gsicc_manager_t;

typedef struct gsicc_rendering_param_s {

    gs_icc_rendering_intents_t rendering_intent;
    gs_object_tag_type_t    object_type;
    gs_icc_black_point_comp_t black_point_comp;
    

} gsicc_rendering_param_t;


#endif

