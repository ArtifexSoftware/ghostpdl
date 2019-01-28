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

/* Defines and exports for library (e.g., Ghostscript) use of gen_ordered.c */

#ifndef gen_ordered_INCLUDED
#  define gen_ordered_INCLUDED

#include "stdpre.h"

#ifndef RAW_SCREEN_DUMP
#  define RAW_SCREEN_DUMP 0	/* Noisy output for .raw files for detailed debugging */
#endif
#if RAW_SCREEN_DUMP || !defined(LIB_BUILD)
#  define FULL_FILE_NAME_LENGTH 50
#endif

#define MAXVAL 65535.0
#define MIN(x,y) ((x)>(y)?(y):(x))
#define MAX(x,y) ((x)>(y)?(x):(y))
#define ROUND( a )  ( ( (a) < 0 ) ? (int) ( (a) - 0.5 ) : \
                                                  (int) ( (a) + 0.5 ) )
typedef enum {
    CIRCLE = 0,
    REDBOOK,
    INVERTED,
    RHOMBOID,
    LINE_X,
    LINE_Y,
    DIAMOND1,
    DIAMOND2,
    ROUNDSPOT,
    CUSTOM  /* Must remain last one */
} spottype_t;

typedef enum {
   OUTPUT_TOS = 0,
   OUTPUT_PS = 1,
   OUTPUT_PPM = 2,
   OUTPUT_RAW = 3,
   OUTPUT_RAW16 = 4
} output_format_type;

typedef struct htsc_param_s {
    double scr_ang;
    int targ_scr_ang;
    int targ_lpi;
    double vert_dpi;
    double horiz_dpi;
    bool targ_quant_spec;
    int targ_quant;
    int targ_size;
    bool targ_size_spec;
    spottype_t spot_type;
    bool holladay;
    double gamma;
    output_format_type output_format;
    int verbose;
} htsc_param_t;

typedef struct htsc_vector_s {
   double xy[2];
} htsc_vector_t;

typedef struct htsc_dig_grid_s {
    int width;
    int height;
    htsc_vector_t bin_center;
    void *memory;		/* needed if non-standard allocator */
    int *data;		/* may be int scaled to 65535 or pairs of turn-on-sequence */
} htsc_dig_grid_t;

/*
    For library usage, #define GS_LIB_BUILD needs to be specified when building
    gen_ordered.c. to leave out "main" and define ALLOC, FREE and PRINT/EPRINT
    macros as appropriate for Ghostscript use.

    Use as functions from other applications will need to #define the macros
    appropriately, adding an another ???_LIB_BUILD #define that also #defines
    LIB_BUILD
*/

/* Set reasonable defaults into the params */
void htsc_set_default_params(htsc_param_t *params);

/* Generate the threshold array given the params */
/*
    Note that final_mask.memory must be set as needed for the allocator and
    it is the callers responsibilty to free the final_mask.data that is returned.
*/
int htsc_gen_ordered(htsc_param_t params, int *S, htsc_dig_grid_t *final_mask);

#endif
