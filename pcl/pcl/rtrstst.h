/* Copyright (C) 2001-2020 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* rtrstst.h - structure for holding raster information in the PCL state */

#ifndef rtrstst_INCLUDED
#define rtrstst_INCLUDED

#include "gsimage.h"
#include "gx.h"
#include "pccoord.h"

/*
 * The current state of the raster machinery.
 *
 * There are several fields in the structure whose presence (or absence) may not
 * be totally intuitive.
 *
 * 1. The graphics margin (the "horizontal" distance of the origin of the image
 *    from the applicable edge of the logical page) is a state variable, even
 *    though this fact is almost never apparent in practice. The parameter is
 *    significant only if "graphics mode" (in the sense used in the PCL
 *    documentation) is entered implicitly; all of the explicit ways of entering
 *    graphics mode (<esc>*r#A, where  # is in the range 0-3) set the graphics
 *    margin. Furthermore, if the graphics mode is ended via <esc>*rC (but not
 *    <esc>*rB), the graphics margin is reset to 0.
 *
 *    For ease of implementation, the graphics margin is in centipoints, but in
 *    the orientation of raster space. The latter feature is significant: it is
 *    possible to implicitly enter graphics mode in an orientation different
 *    from that in which the graphics margin was set. In this case, the
 *    displacement remains the same, but in a different orientation.
 *
 * 2. Whether or not raster scaling is (potentially) used for a given image
 *    is a state variable in some HP PCL implementations, but not in others.
 *    Speicifically, the DeskJet 1600C/CM never uses raster scaling on implicit
 *    entry into graphics mode, while the Color LaserJet 5/5M retains the
 *    state of the last entry into graphics mode (in the current PCL state).
 *    The latter approach is used in this code.
 *
 *    Note that it is the scale/don't scale value of the last entry into
 *    graphics mode that is retained, not whether or not scaling actually
 *    took place for that raster. In particular, assume that graphics mode
 *    was entered with <esc>*r[23]A but the raster was not scaled due to some
 *    other condition (palette not writable, source raster dimensions not
 *    supplied). If the condition preventing raster scaling is altered, and
 *    subsequently graphics mode entered implicitly, raster scaling will
 *    occur.
 *
 * 3. The situation with the scale algorithm feature is not yet clear. The
 *    documentation states ("PCL 5 Color Technical Reference Manual", May
 *    1996 ed., p. 6-42) that "This command is valid only after a Start
 *    Raster command (<esc>*r#A) with scale mode ON." While this might indicate
 *    that the scale algorithm is not a state variable, we tend to think the
 *    statement merely reflects a poor choice of wording. We believe it
 *    would have been better stated as "this command is not effective except
 *    after ..." A reason for this belief is that all of the commands that
 *    are "valid" after a start raster mode are of the form <esc>*b#, while
 *    the scale algorithm command is of the form <esc>*t#, as are the source
 *    resolution and destination dimension commands (all of which are state
 *    variables).
 *
 * 4. Clipping or rasters in PCL is handled in a manner similar to that of
 *    text, which leads to some curious behavior. Normally, rasters are clipped
 *    to the printable page boundary. However, if the raster orgin is at
 *    the logical page "right" or "bottom" boundary relative to the print
 *    direction, the raster is clipped completely, even in cases in which the
 *    logical page boundary is inside the printable region.
 *
 *    Centi-point precision is used to determine whether or not the raster
 *    origin is at the logical page boundary. For logical orientation 0 and
 *    2, the the logical page is either 25 or 21 pixels (at 300 dpi) inside
 *    the printable region, depending on the page size. Hence, it is possible
 *    to relocate a raster by one centipoint, and have a 25 pixel extent of
 *    that raster either appear or disappear.
 *
 *    This clipping is inconsistently applied when presentation mode 3 is
 *    used and the print direction is not along with width of the "physical"
 *    page. In this case, the perpendicular of the raster scanline direction
 *    points in the opposite of the print direction, thus the raster could
 *    move off the "left" edge of the logical page. No clipping is performed
 *    in this case, most likely due to the implementation method chosen by HP.
 *
 *    The field clip_all is set to true if the raster output is to be suppressed
 *    because the raster origin is at the logical page boundary. The motion
 *    implied by rendered scanlines must still be accounted for, hence the
 *    need for this field.
 *
 *    Because of the dis-continuous nature of this effect, it is sensitive to
 *    to arithmetic precision. It is possible to set up situations in practice
 *    in which the last bit of precision of a floating-point calculation will
 *    determine whether a raster is clipped or not. Fortunately, these
 *    situations do not arise in practice.
 */

#ifndef pcl_raster_t_DEFINED
#define pcl_raster_t_DEFINED
typedef struct pcl_raster_t pcl_raster_type;
#endif /* pcl_raster_t_DEFINED */

/*
 * Types of entry into graphics mode. Note that implicit entry is distinct
 * from any of the explicit modes.
 */
typedef enum
{
    NO_SCALE_LEFT_MARG = 0,
    NO_SCALE_CUR_PT = 1,
    SCALE_LEFT_MARG = 2,
    SCALE_CUR_PTR = 3,
    IMPLICIT = 100
} pcl_gmode_entry_t;


typedef struct pcl_raster_state_s
{
    pcl_gmode_entry_t entry_mode;       /* how we entered gmode */
    uint resolution;            /* source resolution, dots per inch */

    /* various flags */
    uint pres_mode_3:1;         /* 1 ==> presentation mode 3 */
    uint scale_raster:1;        /* 1 ==> raster scaling enabled */
    uint src_width_set:1;       /* source raster width explicitly set */
    uint src_height_set:1;      /* source raster height explicitly set */
    uint dest_width_set:1;      /* destination width explicitly set */
    uint dest_height_set:1;     /* destination height explicitly set */
    uint scale_algorithm:1;     /* not currently supported */
    uint graphics_mode:1;       /* 1 ==> in graphics mode */
    uint compression_mode:8;    /* compression mode */

    /* for RTL support */
    int y_advance;              /* advance is +y (1) or -y (-1) */

    /* source and (if applicable) destination dimensions */
    uint src_width;             /* source raster width, samples */
    uint src_height;            /* source raster height, scanlines */
    uint dest_width_cp;         /* destination width, centi-points */
    uint dest_height_cp;        /* destination height, centi-points */

    coord gmargin_cp;           /* "horizontal" displacement of raster origin */

    int clip_all;               /* on last entry into raster mode, the raster
                                 * origin was at the logical page edge, hence
                                 * the raster needs to be clipped */
    pcl_raster_type *pcur_raster;       /* There is at most one image
                                         * actively under construction in PCL
                                         * at one time.  This pointer points
                                         * to that image, if it exists. The
                                         * pointer will be non-null while in
                                         * graphic mode.  */
} pcl_raster_state_t;

#endif /* rtrstst_INCLUDED */
