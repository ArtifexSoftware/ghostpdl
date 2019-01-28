/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* pcfrgrnd.h - PCL foreground object */

#ifndef pcfrgrnd_INCLUDED
#  define pcfrgrnd_INCLUDED

#include "gx.h"
#include "gsstruct.h"
#include "gsrefct.h"
#include "pcstate.h"
#include "pcommand.h"
#include "pccsbase.h"
#include "pcht.h"
#include "pcpalet.h"

/*
 * Foreground data structure for PCL 5c; see Chapter 3 of the "PCL 5 Color
 * Technical Reference Manual". This structure is part of the PCL state.
 *
 * The handling of foreground color in PCL 5c is somewhat unintuitive, as
 * changing parameters that affect the foreground (rendering method, color
 * palette, etc.) does not affect the foreground until the latter is
 * explicitly modified. Clearly, this definition reflected HP's particular
 * implementation: a set of textures (one per component) is generated for
 * the current color index, current color palette, and current rendering
 * method when the foreground color command is issued, and this rendered
 * representation is stored in the PCL state.
 *
 * In a system such a Ghostscript graphic library, where rendered
 * representations are (properly) not visible to the client, a fair amount
 * of information must be kept to achieve the desired behavior. In essence,
 * the foreground color maintains the foreground color, base color space,
 * halfone/transfer function, and CRD with which it was created.
 *
 * When necessary, the foreground color also builds one additional color
 * space, to work around a limitation in the graphics library. All PCL's
 * built-in patterns (including shades) and format 0 user defined patterns
 * are, in the PostScript sense, uncolored: they adopt the foreground color
 * current when they are rendered. Unfortunately, these patterns also have
 * the transparency property of colored patterns: pixels outside of the mask
 * can still be opaque. The graphics library does not currently support
 * opaque rendering of uncolored patterns, so all PCL patterns are rendered
 * as colored. The foreground will build an indexed color space with two
 * entries for this purpose.
 *
 * The foreground structure is reference-counted. It is also assigned an
 * identifier, so that we can track when it is necessary to re-render
 * uncolored patterns.
 *
 * HP Bug is_cmy is set when the base colorspace of the foreground color
 * is CMY.
 * if foreground colorspace is the different than palette's force black
 *   CMY foreground + RGB raster --> black + raster
 *   RGB foreground + CMY raster --> black + raster
 * else same colorspaces:
 *   foreground + raster -> fg_color + raster
 * This is an HP bug in the 4550 4600.
 */
struct pcl_frgrnd_s
{
    rc_header rc;
    pcl_gsid_t id;
    bool is_cmy;                /* NB: see HP Bug above */
    byte color[3];
    pcl_cs_base_t *pbase;
    pcl_ht_t *pht;
};

#ifndef pcl_frgrnd_DEFINED
#define pcl_frgrnd_DEFINED
typedef struct pcl_frgrnd_s pcl_frgrnd_t;
#endif

/*
 * The usual init, copy,and release macros.
 */
#define pcl_frgrnd_init_from(pto, pfrom)    \
    BEGIN                                   \
    rc_increment(pfrom);                    \
    (pto) = (pfrom);                        \
    END

#define pcl_frgrnd_copy_from(pto, pfrom)            \
    BEGIN                                           \
    if ((pto) != (pfrom)) {                         \
        rc_increment(pfrom);                        \
        rc_decrement(pto, "pcl_frgrnd_copy_from");  \
        (pto) = (pfrom);                            \
    }                                               \
    END

#define pcl_frgrnd_release(pbase)           \
    rc_decrement(pbase, "pcl_frgrnd_release")

/*
 * Get the base color space type from a foreground object
 */
#define pcl_frgrnd_get_cspace(pfrgrnd)  ((pfrgrnd)->pbase->type)

/*
 * Build the default foreground. This should be called by the reset function
 * for palettes, and should only be called when the current palette is the
 * default 2-entry palette.
 */
int pcl_frgrnd_set_default_foreground(pcl_state_t * pcs);

/* checks for:
 * (white pattern or white foreground color) and transparent pattern
 * is a NOP
 */
bool is_invisible_pattern(pcl_state_t * pcs);

#endif /* pcfrgrnd_INCLUDED */
