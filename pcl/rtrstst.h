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

/* rtrstst.h - structure for holding raster information in the PCL state */

#ifndef rtrstst_INCLUDED
#define rtrstst_INCLUDED

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
 *    that the scale algorithm is not a state variable, we tend to thing the
 *    statement merely reflects a poor choice of wording. We believe it
 *    would have been better stated as "this command is not effective except
 *    after ..." A reason for this belief is that all of the commands that
 *    are "valid" after a start raster mode are of the form <esc>*b#, while
 *    the scale algorithm command is of the form <esc>*t#, as are the source
 *    resolution and destination dimension commands (all of which are state
 *    variables).
 */

typedef struct pcl_raster_state_s {
    uint    resolution;         /* source resolution, dots per inch */

    /* various flags */
    uint    pres_mode_3:1;      /* 1 ==> presentation mode 3 */
    uint    scale_raster:1;     /* 1 ==> raster scaling enabled */
    uint    src_width_set:1;    /* source raster width explicitly set */
    uint    src_height_set:1;   /* source raster height explicitly set */
    uint    dest_width_set:1;   /* destination width explicitly set */
    uint    dest_height_set:1;  /* destination height explicitly set */
    uint    scale_algorithm:1;  /* not currently supported */
    uint    graphics_mode:1;    /* 1 ==> in graphics mode */
    uint    compression_mode:8; /* compression mode */

    /* for RTL support */
    int     y_advance;          /* advance is +y (1) or -y (-1) */

    /* source and (if applicable) destination dimensions */
    uint    src_width;          /* source raster width, samples */
    uint    src_height;         /* source raster height, scanlines */
    uint    dest_width_cp;      /* destination width, centi-points */
    uint    dest_height_cp;     /* destination height, centi-points */

    coord   gmargin_cp;         /* "horizontal" displacement of raster origin */
} pcl_raster_state_t;

#endif			/* rtrstst_INCLUDED */
