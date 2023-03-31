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


/* Public interface to halftone functionality */

#ifndef gsht_INCLUDED
#  define gsht_INCLUDED

#include "std.h"
#include "gsgstate.h"
#include "gstypes.h"

/* Define the ObjectType enum for Halftones */
typedef enum {
    HT_OBJTYPE_DEFAULT = 0,
    HT_OBJTYPE_VECTOR,
    HT_OBJTYPE_IMAGE,
    HT_OBJTYPE_TEXT,
    HT_OBJTYPE_COUNT		/* Must be last */
} gs_HT_objtype_t;

/* Client definition of (Type 1) halftones */
typedef struct gs_screen_halftone_s {
    float frequency;
    float angle;
    float (*spot_function) (double, double);
    /* setscreen or sethalftone sets these: */
    /* (a Level 2 feature, but we include them in Level 1) */
    float actual_frequency;
    float actual_angle;
} gs_screen_halftone;

#define st_screen_halftone_max_ptrs 0

/* Client definition of color (Type 2) halftones */
typedef struct gs_colorscreen_halftone_s {
    union _css {
        gs_screen_halftone indexed[4];
        struct _csc {
            gs_screen_halftone red, green, blue, gray;
        } colored;
    } screens;
} gs_colorscreen_halftone;

#define st_colorscreen_halftone_max_ptrs 0

/* Procedural interface */
int gs_setscreen(gs_gstate *, gs_screen_halftone *);
int gs_currentscreen(const gs_gstate *, gs_screen_halftone *);
int gs_currentscreenlevels(const gs_gstate *);

/*
 * Enumeration-style definition of a single screen.  The client must:
 *      - probably, call gs_screen_enum_alloc;
 *      - call gs_screen_init;
 *      - in a loop,
 *              - call gs_screen_currentpoint; if it returns 1, exit;
 *              - call gs_screen_next;
 *      - if desired, call gs_screen_install to install the screen.
 */
typedef struct gs_screen_enum_s gs_screen_enum;
gs_screen_enum *gs_screen_enum_alloc(gs_memory_t *, client_name_t);
int gs_screen_init(gs_screen_enum *, gs_gstate *, gs_screen_halftone *);
int gs_screen_currentpoint(gs_screen_enum *, gs_point *);
int gs_screen_next(gs_screen_enum *, double);
int gs_screen_install(gs_screen_enum *);

#endif /* gsht_INCLUDED */
