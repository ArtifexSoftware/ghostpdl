/*
 * Copyright (C) 1998 Aladdin Enterprises.
 * All rights reserved.
 *
 * This file is part of Aladdin Ghostscript.
 *
 * Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
 * or distributor accepts any responsibility for the consequences of using it,
 * or for whether it serves any particular purpose or works at all, unless he
 * or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
 * License (the "License") for full details.
 *
 * Every copy of Aladdin Ghostscript must include a copy of the License,
 * normally in a plain ASCII text file named PUBLIC.  The License grants you
 * the right to copy, modify and redistribute Aladdin Ghostscript, but only
 * under certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved on
 * all copies.
 */

/* pcpattyp.h - pattern type enumerations and related information */

#ifndef pcpattyp_INCLUDED
#define pcpattyp_INCLUDED

#include "pcident.h"

/*
 * Pattern source identifiers. There are three of these, one for PCL and two
 * for GL (one for FT, the other for SV). Though GL types are not usually
 * defined in PCL files, they are in this case so that the pattern code
 * can be kept together.
 */
typedef enum {
    pcl_pattern_solid_frgrnd = 0,   /* solid foreground / current pen */
    pcl_pattern_solid_white,
    pcl_pattern_shading,
    pcl_pattern_cross_hatch,
    pcl_pattern_user_defined,
    pcl_pattern_current_pattern    /* for rectangle fill only */
} pcl_pattern_source_t;

typedef enum {
    hpgl_FT_pattern_solid_pen1 = 1,
    hpgl_FT_pattern_solid_pen2 = 2,
    hpgl_FT_pattern_one_line = 3,
    hpgl_FT_pattern_two_lines = 4,
    hpgl_FT_pattern_shading = 10,
    hpgl_FT_pattern_RF = 11,
    hpgl_FT_pattern_cross_hatch = 21,
    hpgl_FT_pattern_user_defined = 22
} hpgl_FT_pattern_source_t;

typedef enum {
    hpgl_SV_pattern_solid_pen = 0,
    hpgl_SV_pattern_shade = 1,
    hpgl_SV_pattern_RF = 2,
    hpgl_SV_pattern_cross_hatch = 21,
    hpgl_SV_pattern_user_defined = 22
} hpgl_SV_pattern_source_t;
    

/*
 * Opaque definitions of palettes and foregrounds.
 */
#ifndef pcl_palette_DEFINED
#define pcl_palette_DEFINED
typedef struct pcl_palette_s    pcl_palette_t;
#endif

#ifndef pcl_frgrnd_DEFINED
#define pcl_frgrnd_DEFINED
typedef struct pcl_frgrnd_s     pcl_frgrnd_t;
#endif

/*
 * Structure to track what has been installed in the graphic state. This is
 * used to avoid unnecessary re-installation, which can be quite costly when
 * dealing with large halftones or device-independent color spaces.
 *
 *     pattern_id is the identifier of the currently installed pattern color
 *         (this is not the PCL id. for this pattern, which refers to the
 *         source form); if the current color space is not a pattern space
 *         this will be 0.
 *
 *     cspace_id is the identifier of the currently installed color space; if
 *         a pattern color space is currently isntalled, this will be 0.
 *
 *     ht_id is the identifier of the currently installed halftone; at present,
 *         all PCL halftones are threshold-array based.
 *
 *     crd_id is the identifier of the currently installed color rendering
 *         dictionary; the only language-modifiable feature of this dictionary
 *         space is the viewing illuminant.
 * 
 */
typedef struct pcl_gstate_ids_s {
    pcl_gsid_t   pattern_id;
    pcl_gsid_t   cspace_id;
    pcl_gsid_t   ht_id;
    pcl_gsid_t   crd_id;
} pcl_gstate_ids_t;

#endif			/* pcpattyp_INCLUDED */
