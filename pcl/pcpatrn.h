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

/* pcpatrn.h - code for PCL and GL/2 patterns, including color */

#ifndef pcpatrn_INCLUDED
#define pcpatrn_INCLUDED

#include "gx.h"
#include "gsstruct.h"
#include "gsbitmap.h"
#include "gscspace.h"
#include "pcstate.h"
#include "pcommand.h"

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
 * The PCL pattern structure as used by the drawing code. These objects 
 * are not reference counted as they are either statically allocated (the 
 * PCL built-in patterns) or held only by one of the pattern dictionaries 
 * (GL/2 or PCL).
 *
 * The pcl_entity_common field is a temporary/permanent indicator. Such an
 * indicator occurs in all "conventional" PCL resource types.
 *
 * The type field indicates the PCL pattern type: colored or uncolored (both
 * are implemented as colored patterns; see comment above);
 *
 * The pxinfo field is a graphic library client-bitmap structure (i.e.:
 * scanlines are only byte-aligned). This structure is passed to
 * gs_makepixmappattern to build the pattern representation.
 *
 * The orient flag indicates the orientation (0 to 3) for which the pattern was
 * rendered;* a value of -1 indicates that the pattern has never been rendered,
 * and hence the remaining rendering-specific parameters are irrelevant.
 *
 * The palette string holds the 2-entry palette that is required for rendering
 * uncolored patterns. This will always be a zero-length string for colored
 * patterns.
 *
 * The pcspace field is a pointer to the indexed color space used to render
 * this pattern. This is needed only for the 2-entry color space required for
 * uncolored patterns, as otherwise the color space might be deleted before
 * the pattern is rendered.
 *
 * The prast field is used only for colored patterns, and holds a copy of the
 * pixmap raster data if one was required for the rendered version of the
 * pattern. This handles the unusual case in which the raster data for a
 * colored pattern had to be copied because the palette it was being rendered
 * with had more than one white entry.
 *
 * The pen_num field indicates if the pattern was last rendered for GL/2 and,
 * if so, which pen (palette entry) is to be considered the foreground color.
 * If its value is -1, it indicates that the pattern was last rendered for
 * PCL. If it is >= 0, it is the pen number that was current the last time
 * the pattern was rendered. This is significant for uncolored patterns, which
 * adopt the foreground color when rendered in PCL, but the current pen when
 * rendered in GL/2. The variable is ignored for colored patterns
 *
 * The interpretation of cache_id varies based on the pattern type, and whether
 * penum is -1 or >= 0:
 *
 *     For uncolored patterns rendered in PCL, it is the identifier of the
 *         foreground whose attributes were used in the rendering
 *
 *     For colored patterns and uncolored patterns rendered in GL/2, it is
 *         the identifier of the palette for which the pattern was rendered;
 *
 * The ccolor_id field is the identifier assigned to the rendered pattern
 * itself. Once the accuracy of the rendered pattern has been established
 * via the other parameters, this parameter can be checked against the
 * pattern_id field of the PCL state to see if the pattern must be re-inserted
 * into the graphic state.
 */
typedef struct pcl_pattern_s {
    gs_depth_bitmap     pixinfo;    /* pixmap information; must be first */
    pcl_entity_common;              /* temporary/permanent flag */
    pcl_pattern_type_t  type;       /* pattern type */
    int                 xres;       /* intended resolution for pattern */
    int                 yres;

    /* the following paramters refer to renderings of the pattern */
    short               orient;     /* orientation for rendering [0, 3], or -1 */
    short               pen_num;    /* GL/2 pen for rendering, or -1 */
    gs_point            ref_pt;     /* reference point (device space) */
    pcl_gsid_t          cache_id;   /* palette or foreground id. */
    gs_string           palette;    /* 2-entry palette for uncolored patterns */
    gs_color_space *    pcspace;    /* 2-entry color space for pattern */
    const byte *        prast;      /* copy of raster (colored patterns) */
    pcl_gsid_t          ccolor_id;  /* id of the pattern, as a color */
    gs_client_color     ccolor;     /* the rendered pattern, as a color */
} pcl_pattern_t;
    
#define private_st_pattern_t()                          \
    gs_private_st_composite( st_pattern_t,              \
                             pcl_pattern_t,             \
                             "PCL-GL/2 pattern object", \
                             pattern_enum_ptrs,         \
                             pattern_reloc_ptrs         \
                             )

/*
 * The usual copy, init, and release macros. Note that since the object is
 * not reference counted, the release operation does nothing: a pattern may
 * only be released by undefining (or redefining) the entry with the
 * associated pattern id in the user defined pattern dictionary. Hence,
 * statically defined patterns are never released, which is the desired
 * result.
 */
#define pcl_pattern_init_from(pto, pfrom)   (pto) = (pfrom)
#define pcl_pattern_copy_from(pto, pfrom)   (pto) = (pfrom)
#define pcl_pattern_release(ppatrn)

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
extern int     pcl_pattern_RF(
    int                     ptrn_indx,    /* pattern index */
    const gs_depth_bitmap * ppixmap,      /* pixmap */
    gs_memory_t *           pcs
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
extern  pcl_pattern_set_proc_t  pcl_pattern_get_proc_PCL(
    pcl_pattern_source_t   pattern_source
);

extern  pcl_pattern_set_proc_t  pcl_pattern_get_proc_FT(
    hpgl_FT_pattern_source_t    pattern_source
);

extern  pcl_pattern_set_proc_t  pcl_pattern_get_proc_SV(
    hpgl_SV_pattern_source_t    pattern_source
);

/*
 * Entry point to pattern-related functions.
 */
extern  const pcl_init_t    pcl_pattern_init;

#endif  	/* pcpatrn_INCLUDED */
