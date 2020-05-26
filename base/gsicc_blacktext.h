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

#ifndef gsicc_blacktext_INCLUDED
#  define gsicc_blacktext_INCLUDED

typedef struct gsicc_blacktext_state_s gsicc_blacktext_state_t;

struct gsicc_blacktext_state_s {
    gs_memory_t *memory;
    rc_header rc;
    bool is_fill;                 /* Needed for proper color restore */
    gs_color_space *pcs[2];       /* If doing black text, color spaces to restore */
    gs_client_color *pcc[2];      /* If doing black text, client colors to restore */
    float value[2];               /* DeviceGray setting blows away the client color
                                     zero value */
};

gsicc_blacktext_state_t* gsicc_blacktext_state_new(gs_memory_t *memory);
void gsicc_restore_black_text(gs_gstate *pgs);

#endif
