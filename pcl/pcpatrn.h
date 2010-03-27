/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* pcpatrn.h - PCL/GL client color and pattern code */

#ifndef pcpatrn_INCLUDED
#define pcpatrn_INCLUDED

#include "gx.h"
#include "gsstruct.h"
#include "gsrefct.h"
#include "pcindxed.h"
#include "pccsbase.h"

/*
 * PCL pattern types.
 *
 * There are two types of patterns used in PCL, colored and uncolored. In
 * order to support transparency, both types are implemented as colored 
 * patterns in the graphics library (which does not support opaque 
 * uncolored patterns).
 *
 * The values used are defined by HP.
 *
 * Note: The implementation of opaque uncolored patterns is not correct, and
 *       cannot be handled correctly with the current graphics library. The
 *       difficulty is that the "background" regions of opaque uncolored
 *       patterns should be rendered in "device" white. Due to the properties
 *       of the color space and the color lookup tables employed, it is not
 *       obvious which source color yields device white, or whether such a
 *       a source color even exists. Hence, there is no colored pattern that
 *       is known to give the same result as the opaque uncolored pattern.
 */
typedef enum {
    pcl_pattern_uncolored = 0,
    pcl_pattern_colored = 1
} pcl_pattern_type_t;

/*
 * The pattern data structure. This includes the pattern data, dimensions,
 * type, and implied resolution.
 *
 * It is not strictly necessary that this structure be reference counted, as
 * there is no chance that any unrendered objects will make use of the deleted
 * pattern. The use of reference counts makes for a more consistent
 * implementation, however, and leaves open the possibility of subsequent
 * implementations that may involve delayed rendering.
 */
typedef struct pcl_pattern_data_s {
    gs_depth_bitmap     pixinfo;    /* pixmap information; must be first */
    pcl_data_storage_t storage;     /* temporary/permanent/internal flag */
    rc_header           rc;
    pcl_pattern_type_t  type;       /* pattern type */
    int                 xres;       /* intended resolution for pattern */
    int                 yres;
} pcl_pattern_data_t;

#define private_st_pattern_data_t() /* in pcuptrn.c */  \
    gs_private_st_suffix_add0( st_pattern_data_t,       \
                               pcl_pattern_data_t,      \
                               "PCL/GL pattern data",   \
                               pattern_data_enum_ptrs,  \
                               pattern_data_reloc_ptrs, \
                               st_gs_depth_bitmap       \
                               )

/*
 * The usual copy, init, and release macros.
 */
#define pcl_pattern_data_init_from(pto, pfrom)  \
    BEGIN                                       \
    rc_increment(pfrom);                        \
    (pto) = (pfrom);                            \
    END

#define pcl_pattern_data_copy_from(pto, pfrom)          \
    BEGIN                                               \
    if ((pto) != (pfrom)) {                             \
        rc_increment(pfrom);                            \
        rc_decrement(pto, "pcl_pattern_data_copy_from");\
        (pto) = (pfrom);                                \
    }                                                   \
    END

#define pcl_pattern_data_release(ppat_data)             \
    rc_decrement(ppat_data, "pcl_pattern_data_release")



/* forward declaration */
#ifndef pcl_ccolor_DEFINED
#define pcl_ccolor_DEFINED
typedef struct pcl_ccolor_s     pcl_ccolor_t;
#endif

/*
 * The pattern structure. This is not reference counted, as the only place it
 * is referred to is the pattern dictionary.
 *
 * The primary purpose for this structure is to handle caching of pattern
 * instances. There are potentially two rendered instances of a pattern,
 * one as a mask (uncolored) pattern and one as a colored pattern.
 *
 * A "colored" pattern in the PCL sense will never have a mask rendering, but 
 * an "uncolored" PCL pattern may have both a mask and a colored rendering,
 * because mask patterns in the graphic library cannot be opaque, and cannot
 * use a halftone or color redering dictionary that differs from that being
 * used by a raster.
 *
 * The various "render key" field values are used to identify the environment
 * for which the client colors pointed to by pmask_ccolor  and pcol_ccolor
 * have been rendered for. Not all fields apply to both client colors:
 *
 *
 *    The transp bit indicates if the colored rendering was render with
 *        transparency set (mask patterns are always rendered with
 *        transparency set).
 *
 *    The orient field indicates the orientation of the rendering, in
 *        the range 0 to 3.
 *
 *    The pen field applies only to the colored pattern rendering and is used
 *        only for patterns that are uncolored in the PCL sense and rendered 
 *        from GL/2. The pen field indicates the palette entry used as the
 *        foreground for the pattern. For PCL colored patterns or uncolored
 *        patterns rendered from PCL, this field will be 0. The value 0 is
 *        also valid for GL/2, but this causes no difficulty as uncolored
 *        patterns rendered in GL/2 use the current palette, while those
 *        rendered in PCL use the current foreground. Hence, the cache_id
 *        (see below) will never be the same for both cases.
 *
 *    cache_id is the identifier of either the foreground or palette used
 *        to create the colored rendering of the pattern. This is the
 *        identifier of either a palette or a foreground. Only for opaque
 *        uncolored (in the PCL sense) patterns rendered from PCL is it
 *        the id of the foreground; in all other cases it is the id of the
 *        palette.
 *
 *    The ref_pt field identifies the reference point, in device space, for
 *        which both renderings of the pattern were created.
 */
typedef struct pcl_pattern_t {
    pcl_pattern_data_t *    ppat_data;

    /* the mask and colored rendered instances, if any */
    pcl_ccolor_t *          pcol_ccolor;
    pcl_ccolor_t *          pmask_ccolor;

    /* "rendered key" */
    uint                    transp:1;  /* transparency of rendering */
    uint                    orient:2;  /* orientation of rendering */
    uint                    pen:8;     /* 0 for PCL or colored patterns */
    pcl_gsid_t              cache_id;  /* foreground or palette */
    gs_point                ref_pt;    /* referenc point (device space) */
} pcl_pattern_t;

#define private_st_pattern_t()  /* in pcuptrn.c */  \
    gs_private_st_ptrs3( st_pattern_t,              \
                         pcl_pattern_t,             \
                         "PCL/GL pattern",          \
                         pattern_enum_ptrs,         \
                         pattern_reloc_ptrs,        \
                         ppat_data,                 \
                         pcol_ccolor,               \
                         pmask_ccolor               \
                         )


/*
 * The PCL structure corresponding to the graphic library's client color
 * structure. The latter potentially contains a client data structure pointer
 * (for pattern colors) which it does not (an cannot) take ownership of. To
 * release the associated memory at the correct time, it is necessary to build
 * a parallel structure which has ownership of the client data, and which
 * can be kept in a one-to-one relationship with the graphic library client
 * color structure.
 *
 * The interpretation of the various fields varies by color type:
 *
 *    For pcl_ccolor_unpatterned:
 *
 *      ppat_data == NULL,
 *
 *      one of pindexed or pbase points to the current color space (the other
 *          is NULL)
 *
 *      ccolor.paint.values[0] or ccolor.paint.values[0..2] holds the
 *          color component values
 *
 *      ccolor.pattern == NULL
 *
 *    For pcl_ccolor_mask_pattern:
 *
 *      ppat_data points to the pattern data
 *
 *      one of pindexed or pbase points to the base color space of the
 *          pattern color space (the other is NULL)
 * 
 *      ccolor.paint.values[0] or ccolor.paint.values[0..2] holds the
 *         color values to be use for the pattern foreground
 *
 *      ccolor.pattern points to the pattern instance
 *
 *    For pcl_ccolor_colored_pattern
 *
 *      ppat_data points to the pattern data
 *
 *      pindexed points to the base color space of the pattern
 *
 *      pbase == NULL,
 *
 *      ccolor.paint is ignored
 *
 *      ccolor.pattern points to the pattern instance
 *
 * Not that exactly one of pbase or pindexed will ever be non-NULL.
 *
 * The prast data is used only for colored patterns. When these are rendered
 * with transparency on, difficulties might arise because there is more than
 * one "white" value in the palette of the indexed color space pointed to by
 * pindexed. Since the ImageType 4 rendering mechanism used to implement
 * transaprent colored patterns cannot accommodate multiple non-contiguous
 * while values, the pattern data must be copied and all white values mapped
 * to a unique white value. The prast pointer points to this remapped array.
 */

typedef enum {
    pcl_ccolor_unpatterned = 0,
    pcl_ccolor_mask_pattern,
    pcl_ccolor_colored_pattern
} pcl_ccolor_type_t;

struct  pcl_ccolor_s {
    rc_header               rc;
    pcl_ccolor_type_t       type;
    pcl_pattern_data_t *    ppat_data;  
    pcl_cs_indexed_t *      pindexed;
    pcl_cs_base_t *         pbase;
    const byte *            prast;
    gs_client_color         ccolor;
};

#define private_st_ccolor_t()               \
    gs_private_st_ptrs4( st_ccolor_t,       \
                         pcl_ccolor_t,      \
                         "PCL client color",\
                         ccolor_enum_ptrs,  \
                         ccolor_reloc_ptrs, \
                         ppat_data,         \
                         pindexed,          \
                         pbase,             \
                         ccolor.pattern     \
                         )

/*
 * The usual copy, init, and release macros.
 */
#define pcl_ccolor_init_from(pto, pfrom)    \
    BEGIN                                   \
    rc_increment(pfrom);                    \
    (pto) = (pfrom);                        \
    END

#define pcl_ccolor_copy_from(pto, pfrom)            \
    BEGIN                                           \
    if ((pto) != (pfrom)) {                         \
        rc_increment(pfrom);                        \
        rc_decrement(pto, "pcl_ccolor_copy_from");  \
        (pto) = (pfrom);                            \
    }                                               \
    END

#define pcl_ccolor_release(pccolor)             \
    rc_decrement(pccolor, "pcl_ccolor_release")


/*
 * Create a colored pcl_pattern_t object from a gs_depth_bitmap object. This
 * object will be considered "temporary" in the sense of a PCL resource, and
 * is stored in the GL/2 pattern dictionary (reserved for patterns created
 * via the GL/2 RF command.
 *
 * This procedure is exported for the convenience of the GL/2 RF command, as
 * it avoids having the GL/2 module deal with the pattern structure provided
 * above.
 *
 * Passing a null pointer clears the indicated entry; this can be used by
 * GL/2 to facilitate the reset functions of the IN and DF operators (and
 * the RF operator without operands).
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_pattern_RF(
    int                     ptrn_indx,    /* pattern index */
    const gs_depth_bitmap * ppixmap,      /* pixmap */
    pcl_state_t *           pcs
);

/*
 * Procedure to get/set up the current graphic state for the proper "pattern"
 * (which may be a solid color). The routine is called with the current PCL
 * state and one or two additional operands. The interpretation of the operands
 * is dependent on the particular routine used.
 *
 * These routines are intended to handle the many different combinations of
 * color space, color rendering dictionary, and halftone object required in
 * the graphic state in different situations. The chart below characterizes
 * what the operands are interpreted to mean, and what the source of the
 * (graphic state) color space, color, color rendering dictionary, and
 * halftone (including transfer function) are for each case. Note in 
 * particular that uncolored patterns (including the built-in shades and 
 * cross-hatch patterns) must be handled separately for PCL and GL.
 *
 *    solid white (PCL or GL)
 *        arg1              ignored
 *        arg2              ignored
 *
 *        pattern source    none
 *
 *        cspace            DeviceGray (irrespective of current space)
 *        color             1,0 (irrespective of current color)
 *        CRD               unchanged (irrelevant since DeviceGray is 
 *                          the color space)
 *        halftone          Fixed halftone with null transfer function
 *
 *        note: this routine does NOT override the current transparency
 *              values.
 *
 *    solid color (GL only)
 *        arg1              pen number
 *        arg2              ignored
 *
 *        pattern source    none
 *
 *        cspace            indexed color space from current palette
 *        color             pen number
 *        CRD               from current palette
 *        halftone          from current palette
 *
 *        note: this routine does NOT set the line width
 *
 *    solid foreground (PCL only)
 *        arg1              ignored
 *        arg2              ignored
 *
 *        pattern source    none
 *
 *        cspace            base color space from current foreground
 *        color             color from current foreground
 *        CRD               from current foreground
 *        halftone          from current foreground
 *
 *    shade pattern (PCL only)
 *        arg1              shade percentage
 *        arg2              ignored
 *
 *        pattern source    built-in shade patterns
 *
 *        cspace            Pattern color space without base color space
 *        color             Colored pattern generated from the pixmap data 
 *                          provided and a special indexed color space 
 *                          which utilizes the base color space in the 
 *                          current foreground and a 2-entry palette. The
 *                          palette contains the canonical white color and 
 *                          the foreground color. The color rendering 
 *                          dictionary and halftone also are taken from 
 *                          the current color.
 *        CRD               from current foreground (only relevant for 
 *                          pattern rendering)
 *        halftone          from current foreground (only relevant for 
 *                          pattern rendering).
 *
 *    shade pattern (GL only)
 *        arg1              shade percentage
 *        arg2              current pen
 *
 *        pattern source    built-in shade patterns
 *
 *        cspace            Pattern color space without base color space
 *        color             Colored pattern generated from the pixmap data 
 *                          provided and a special indexed color space 
 *                          which utilizes the base color space in the 
 *                          current palette and a 2-entry palette. The
 *                          palette contains the canonical white color and 
 *                          the color corresponding to the given pen. The 
 *                          color rendering dictionary and halftone are 
 *                          taken from the given palette.
 *        CRD               from current palette (only relevant for 
 *                          pattern rendering)
 *        halftone          from current palette (only relevant for 
 *                          pattern rendering).
 *
 *    cross-hatch pattern (PCL only)
 *        arg1              pattern index
 *        arg2              ignored
 *
 *        pattern-source    built-in cross-hatch patterns
 *
 *        cspace            Pattern color space without base color space
 *        color             Colored pattern generated from the pixmap data 
 *                          provided and a special indexed color space 
 *                          which utilizes the base color space in the 
 *                          current foreground and a 2-entry palette. The
 *                          palette contains the canonical white color and 
 *                          the foreground color. The color rendering 
 *                          dictionary and halftone also are taken from the 
 *                          current color.
 *        CRD               from current foreground (only relevant for 
 *                          pattern rendering)
 *        halftone          from current foreground (only relevant for 
 *                          pattern rendering).
 *
 *    cross-hatch pattern (GL only)
 *        arg1              pattern index
 *        arg2              current pen
 *
 *        pattern source    built-in cross-hatch patterns
 *
 *        cspace            Pattern color space without base color space
 *        color             Colored pattern generated from the pixmap data 
 *                          provided and a special indexed color space 
 *                          which utilizes the base color space in the 
 *                          current palette und and a 2-entry palette. The
 *                          palette contains the canonical white color and 
 *                          the color corresponding to the given pen. The 
 *                          color rendering dictionary and halftone are 
 *                          taken from the given palette.
 *        CRD               from current palette (only relevant for 
 *                          pattern rendering)
 *        halftone          from current palette (only relevant for 
 *                          pattern rendering).
 *
 *    PCL user-defined pattern (PCL only)
 *        arg1              pattern id
 *        arg2              ignored
 *
 *        pattern source    PCL user defined patterns
 *
 *        Handling depends on the pattern type. For uncolored patterns, the
 *        settings are:
 *    
 *          cspace          Pattern color space without base color space
 *          color           Colored pattern generated from the pixmap data 
 *                          provided and a special indexed color space 
 *                          which utilizes the base color space in the 
 *                          current foreground and a 2-entry palette. The
 *                          palette contains the canonical white color and 
 *                          the foreground color. The color rendering 
 *                          dictionary and halftone also are taken from the 
 *                          current color.
 *          CRD             from current foreground (only relevant for 
 *                          pattern rendering)
 *          halftone        from current foreground (only relevant for 
 *                          pattern rendering).
 *
 *        For colored patterns, the settings are:
 *
 *          cspace          Pattern color space without base color space
 *          color           Colored pattern generated from the pixmap data 
 *                          provided and the indexed color space in the 
 *                          current palette, along with the halftone and 
 *                          color rendering dictionary in the current palette
 *          CRD             from current palette (only relevant for 
 *                          pattern rendering)
 *          halftone        from current palette (only relevant for color 
 *                          generation)
 *
 *    PCL user-defined pattern (GL only)
 *        arg1              pattern id
 *        arg2              current pen
 *
 *        pattern source    PCL user defined patterns
 *
 *        Handling depends on the pattern type. For uncolored patterns, the
 *        settings are:
 *    
 *          cspace          Pattern color space without base color space
 *          color           Colored pattern generated from the pixmap data 
 *                          provided and a special indexed color space 
 *                          which utilizes the base color space in the 
 *                          current palette and a 2-entry palette. The
 *                          palette contains the canonical white color and 
 *                          the color corresponding to the given pen. The 
 *                          color rendering dictionary and halftone are 
 *                          taken from the given palette.
 *          CRD             from current palette (only relevant for 
 *                          pattern rendering)
 *          halftone        from current palette (only relevant for 
 *                          pattern rendering).
 * 
 *        For colored patterns, the settings are:
 *
 *          cspace          Pattern color space without base color space
 *          color           Colored pattern generated from the pixmap data 
 *                          provided and the indexed color space in the 
 *                          current palette, along with the halftone and 
 *                          color rendering dictionary in the current palette
 *          CRD             from current palette (only relevant for 
 *                          pattern rendering)
 *          halftone        from current palette (only relevant for color 
 *                          generation)
 *
 *    RF pattern (GL only)
 *        arg1              pattern index
 *        arg2              ignored
 *
 *        pattern source    GL user defined patterns
 *
 *        cspace            Pattern color space without base color space
 *        color             Colored pattern generated from the pixmap data 
 *                          provided and the indexed color space in the 
 *                          current palette, along with the halftone and 
 *                          color rendering dictionary in the current palette
 *        CRD               from current palette (only relevant for 
 *                          pattern rendering)
 *        halftone          from current palette (only relevant for color 
 *                          generation)
 *
 * These routines will also set the pattern reference point appropriatel for
 * either PCL or GL.
 *
 * The routines return 0 on success (with the graphic state properly set up),
 * < 0 in the event of an error.
 */

typedef int     (*pcl_pattern_set_proc_t)( pcl_state_t *, int arg1, int arg2 );

/*
 * Return the pattern set procedure appropriate for the specified pattern
 * source specification (the command that selected the pattern).
 *
 * A return of NULL indicates a range check error.
 */
pcl_pattern_set_proc_t pcl_pattern_get_proc_PCL(
    pcl_pattern_source_t   pattern_source
);

pcl_pattern_set_proc_t pcl_pattern_get_proc_FT(
    hpgl_FT_pattern_source_t    pattern_source
);

pcl_pattern_set_proc_t pcl_pattern_get_proc_SV(
    hpgl_SV_pattern_source_t    pattern_source
);

/*
 * Entry point to pattern-related functions.
 */
extern  const pcl_init_t    pcl_pattern_init;

#endif  	/* pcpatrn_INCLUDED */
