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


/* Alpha-compositing interface */

#ifndef gsalphac_INCLUDED
#  define gsalphac_INCLUDED

#include "std.h"
#include "gscompt.h"

/*
 * Define the compositing operations.  These values must match the ones in
 * dpsNeXT.h.
 */
typedef enum {
    composite_Clear = 0,
    composite_Copy,
    composite_Sover,
    composite_Sin,
    composite_Sout,
    composite_Satop,
    composite_Dover,
    composite_Din,
    composite_Dout,
    composite_Datop,
    composite_Xor,
    composite_PlusD,
    composite_PlusL,
#define composite_last composite_PlusL
    composite_Highlight,	/* (only for compositerect) */
#define compositerect_last composite_Highlight
    composite_Dissolve		/* (not for PostScript composite operators) */
#define composite_op_last composite_Dissolve
} gs_composite_op_t;

/*
 * Define parameters for alpha-compositing.
 */
typedef struct gs_composite_alpha_params_s {
    gs_composite_op_t op;
    float delta;		/* only for Dissolve */
} gs_composite_alpha_params_t;

/* Create an alpha-compositing object. */
int gs_create_composite_alpha(gs_composite_t ** ppcte,
                              const gs_composite_alpha_params_t * params,
                              gs_memory_t * mem);

#endif /* gsalphac_INCLUDED */
